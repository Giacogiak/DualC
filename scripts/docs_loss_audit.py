#!/usr/bin/env python3
"""DualC docs loss audit -- did a docs restructuring lose a fact?

The gate (check.py) proves shape: links resolve, pages fit the cap, indexes
are complete. It never proves that a sentence, a number, a heading or a flag
that existed on one commit still exists on another. This script does, as pure
set arithmetic over two git trees, so that the same (base, head, triage file)
always gives the same bytes and the same exit code.

Four unit types are extracted from every file in --scope on the BASE tree and
looked for on the HEAD tree, per file, never against a concatenation:

  sentence     prose split into sentences (a table row is one sentence);
               word-8-gram shingles; kept when >= KEPT_SCK of them are in one
               head file, else bucketed rewritten / partial / gone by 4-grams
  evidence     distinctive numbers (ISO dates, hashes, versions, N,NNN, N/N,
               number+unit, 4+ digit integers) that must co-occur with their
               neighbours in one block of one head file
  heading      anchor slugs (check.py's slugify, GitHub's rule)
  identifier   backticked symbols, --flags, #N / D-NN / letter IDs, paths

The head tree is tiered: (a) live docs, (b) verbatim homes (the study pack
that git mv'd into raw/), (c) quotation only -- the raw/ screenings, the plan
and the roadmap 19 phase records, which QUOTE what they removed and so can
never make a residual "kept" -- and code, for identifiers. The audit's own
dump and record page are excluded from the corpus for the same reason.

Every residual is keyed by sha1 of its normalised text and triaged in
scripts/docs_loss_audit.json: `intentional` / `superseded` must cite a record
anchor AND a quote the script verifies on the head tree; `restored` must name
a commit; `noise` must give the pattern it matches. Bulk rules carry a
category from the record's removal catalogue and a max_matches cap, and every
row a rule absorbs is printed. Exit 0 means: every residual has a verdict,
every citation resolves, and the untriaged count is within the ratchet.

Usage:
    python scripts/docs_loss_audit.py                       # merge-base main..HEAD vs HEAD
    python scripts/docs_loss_audit.py --base 3d73220 --head structure-docs
    python scripts/docs_loss_audit.py --untriaged --source docs/roadmap/
    python scripts/docs_loss_audit.py --write-dump docs/raw/<date>-loss-audit-<base>-<head>.txt
    python scripts/docs_loss_audit.py --selftest

Exit codes: 0 audit clean, 1 residuals untriaged / citation unresolved /
rule overflow / stale restoration, 2 the tool or its configuration is broken.
Python 3, standard library only; imports the markdown helpers of check.py so
anchors are computed exactly as the gate computes them.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check import (HEADING_RE, INLINE_CODE_RE, heading_slugs, run, repo_root,  # noqa: E402
                   slugify, strip_fences, table_cells)

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# --------------------------------------------------------------------------
# Constants -- printed in the report header, pinned in the triage file
# --------------------------------------------------------------------------

UNITS = ("sentence", "evidence", "heading", "identifier")
THRESHOLDS = {
    "MIN_WORDS": 6,       # shorter prose units are not sentences
    "K_LONG": 8,          # shingle length for units of >= 8 words
    "K_SHORT": 4,         # shingle length for the rewritten/partial buckets
    "KEPT_SCK": 0.6,      # fraction of K_LONG-shingles found in one file => kept
    "REWRITTEN_SC4": 0.6, # fraction of 4-shingles found => rewritten (else partial/gone)
    "PARTIAL_SC4": 0.3,
    "CTX_WORDS": 3,       # neighbour words carried by an evidence token in prose
    "CTX_MIN_HIT": 2,     # of which this many must co-occur in one head block
    "TOP_K_FILES": 5,     # candidate files scored exactly per unit
}
T = THRESHOLDS

TIER_A = ("docs/design/", "docs/roadmap/", "docs/command_reference/", "docs/decisions/",
          "docs/README.md", "AGENTS.md", "README.md", "STRUCTURE.md", "THIRD_PARTY.md",
          ".claude/")
TIER_B = ("docs/raw/study/", "docs/raw/2026-06-24-parity-run.txt")
TIER_C_OVERRIDE = ("docs/roadmap/19-docs-layers/",)   # records quote what they removed
TIER_C = ("docs/raw/",)
TIER_CODE = ("src/", "include/", "examples/", "capi/", "tests/", "scripts/")
DOC_EXT = (".md", ".txt")
CODE_EXT = (".h", ".hpp", ".cpp", ".c", ".py", ".cmake", ".txt", ".md")
CORPUS_EXCLUDE = (r"^docs/raw/\d{4}-\d\d-\d\d-loss-audit-",
                  r"^docs/roadmap/19-docs-layers/09-pre-merge-loss-audit",
                  r"^scripts/docs_loss_audit\.(py|json)$")   # the triage excerpts and selftest strings
DEFAULT_SCOPE = ("docs/roadmap/", "docs/command_reference/", "docs/ARCHITECTURE.md",
                 "docs/report-quality-inspection-contouring.md", "docs/study/README.md")
DEFAULT_ANNEX = ("docs/ARCHITECTURE.md",)

BUCKETS = ("kept", "rewritten", "partial", "gone", "raw-only", "token-only",
           "unverifiable-context", "code-only")
BUCKET_SHORT = {"unverifiable-context": "unverif-ctx"}
for _b in BUCKETS:
    BUCKET_SHORT.setdefault(_b, _b)
VERDICTS = ("intentional", "superseded", "restored", "noise")
CITING = ("intentional", "superseded")

STOPWORDS = frozenset("""
the a an and or but of to in on at by for with from as is are was were be been being
it its this that these those there here than then so if not no nor into onto over
under per via any all each one two per such same also only both either neither which
who whom whose what when where why how can could may might will would shall should
must do does did done has have had having about above below between after before
""".split())

# --------------------------------------------------------------------------
# Normalisation -- ONE fold for both trees
# --------------------------------------------------------------------------

FOLD_MAP = {
    "\u2019": "'", "\u2018": "'", "\u201c": '"', "\u201d": '"', "\u2013": "-", "\u2014": "-",
    "\u2026": "...", "\u2248": "~", "\xd7": "x", "\u2044": "/", "\u2264": "<=",
    "\u2265": ">=", "\xa0": " ", "\xb7": "-", "\u2022": "-", "\u2192": "->",
    "\u2190": "<-", "\u2011": "-", "\u2212": "-",
}
LINK_TEXT_RE = re.compile(r"\[([^\]]*)\]\(([^)]*)\)")
LINK_TARGET_RE = re.compile(r"\]\([^)]*\)")
WORD_RE = re.compile(r"[a-z0-9]+(?:[.\-_/:][a-z0-9]+)*")
SENT_SPLIT_RE = re.compile(r"(?<=[.!?])\s+(?=[A-Z0-9`(\[\"'*])")
LIST_RE = re.compile(r"^\s*(?:[-*+]|\d+[.)])\s+")
SEP_CELL_RE = re.compile(r":?-{2,}:?")


def fold(text):
    """NFKC + typographic punctuation to ASCII + LF. Applied to every blob of
    both trees on read, so a reflow or a smart-quote pass is not a loss."""
    text = unicodedata.normalize("NFKC", text).replace("\r\n", "\n").replace("\r", "\n")
    for k, v in FOLD_MAP.items():
        text = text.replace(k, v)
    return text


def md_strip(text):
    """Markdown syntax off, content kept: link text without its target,
    emphasis markers, backticks, blockquote and list markers."""
    text = LINK_TEXT_RE.sub(r"\1", text)
    text = text.replace("**", "").replace("`", "")
    text = re.sub(r"(?<!\w)\*(?=\S)|(?<=\S)\*(?!\w)", "", text)
    text = re.sub(r"^\s*>\s?", "", text, flags=re.M)
    return text


def words(text):
    return WORD_RE.findall(md_strip(text).lower())


FENCE_LINE_RE = re.compile(r"^\s*(```|~~~)")


def blocks(text):
    """(line, kind, text): kind heading | row | para | code. A paragraph is its
    consecutive non-blank lines joined by one space, so the 88-column reflow of
    the base commit is invisible; a list item starts a new paragraph; a table
    separator row is dropped; every non-blank line inside a fence is a `code`
    block of its own (recipes and build lines carry numbers and flags too)."""
    out, para, para_line, fence = [], [], 0, None

    def flush():
        if para:
            out.append((para_line, "para", " ".join(para)))
            del para[:]

    for i, line in enumerate(text.split("\n"), 1):
        s = line.strip()
        m = FENCE_LINE_RE.match(line)
        if m:
            flush()
            if fence is None:
                fence = m.group(1)
            elif s.startswith(fence):
                fence = None
            continue
        if fence is not None:
            if s:
                out.append((i, "code", s))
            continue
        if not s:
            flush()
            continue
        m = HEADING_RE.match(line)
        if m:
            flush()
            out.append((i, "heading", m.group(2).strip()))
            continue
        if s.startswith("|"):
            flush()
            cells = table_cells(line)
            if all(not c or SEP_CELL_RE.fullmatch(c) for c in cells):
                continue
            out.append((i, "row", " ; ".join(c for c in cells if c)))
            continue
        if LIST_RE.match(line):
            flush()
        if not para:
            para_line = i
        para.append(s)
    flush()
    return out


def shingles(ws, k):
    k = min(k, len(ws))
    if k <= 0:
        return set()
    return set(" ".join(ws[i:i + k]) for i in range(len(ws) - k + 1))


def sentence_units(text):
    """(line, kind, sentence_text, words) -- prose sentences, table rows and
    fenced code lines of at least MIN_WORDS words. Headings are their own unit."""
    out = []
    for line, kind, btxt in blocks(text):
        if kind == "heading":
            continue
        parts = [btxt] if kind in ("row", "code") else SENT_SPLIT_RE.split(btxt)
        for p in parts:
            p = p.strip()
            ws = words(p)
            if len(ws) >= T["MIN_WORDS"]:
                out.append((line, kind, p, ws))
    return out


def sentence_bucket(sck, sc4):
    if sck >= T["KEPT_SCK"]:
        return "kept"
    if sc4 >= T["REWRITTEN_SC4"]:
        return "rewritten"
    if sc4 >= T["PARTIAL_SC4"]:
        return "partial"
    return "gone"


# --------------------------------------------------------------------------
# Evidence tokens
# --------------------------------------------------------------------------

EVIDENCE_RE = re.compile(r"""
 (?P<date>\b\d{4}-\d\d-\d\d\b)
|(?P<hash>\b(?=[0-9a-f]*\d)(?=[0-9a-f]*[a-f])[0-9a-f]{7,40}\b)
|(?P<ver>\bv?\d+\.\d+\.\d+\b)
|(?P<thou>\b\d{1,3}(?:,\d{3})+\b)
|(?P<ratio>\b\d+/\d+\b)
|(?P<unit>\b\d+(?:\.\d+)?\s?(?:ms|s|mm|cm|um|MB|GB|KB|B|M|k|fps|Hz|kHz|MHz|deg|%)(?![A-Za-z0-9]))
|(?P<sci>\b\d+(?:\.\d+)?e-?\d+\b)
|(?P<dec>\b\d+\.\d{2,}\b)
|(?P<factor>\b\d+(?:\.\d+)?x\b)
|(?P<big>\b\d{4,}\b)
""", re.X)


def norm_token(t):
    return t.replace(" ", "")


def evidence_tokens(text):
    return [norm_token(m.group(0)) for m in EVIDENCE_RE.finditer(text)]


def context_words(before, after):
    """The CTX_WORDS nearest usable words around a prose token: not a
    stopword, at least 3 letters, carrying no digit (a digit would be a token)."""
    def usable(ws):
        return [w for w in ws if w not in STOPWORDS and len(w) >= 3 and not any(c.isdigit() for c in w)]
    b = usable(words(before))[::-1]
    a = usable(words(after))
    out = []
    for i in range(max(len(a), len(b))):
        if i < len(b):
            out.append(b[i])
        if i < len(a):
            out.append(a[i])
        if len(out) >= T["CTX_WORDS"]:
            break
    return out[:T["CTX_WORDS"]]


def evidence_units(text):
    """(line, kind, token, ctx) for every evidence token, fences included. In a
    table row the context is the row's other tokens plus the first cell's
    words plus its nearest prose words -- numeric rows have few of the latter."""
    out = []
    for line, kind, btxt in blocks(text):
        toks = [(m.start(), m.end(), norm_token(m.group(0))) for m in EVIDENCE_RE.finditer(btxt)]
        if not toks:
            continue
        if kind == "row":
            for s, e, tok in toks:
                ctx = [t2 for _, _, t2 in toks if t2 != tok]
                ctx.extend(context_words(btxt[:s], btxt[e:]))
                out.append((line, kind, tok, ctx))
        else:
            for s, e, tok in toks:
                out.append((line, kind, tok, context_words(btxt[:s], btxt[e:])))
    return out


def block_vocab(btxt):
    return set(words(btxt)) | set(evidence_tokens(btxt))


# --------------------------------------------------------------------------
# Headings and identifiers
# --------------------------------------------------------------------------

def heading_units(text):
    """(line, slug, heading_text) with GitHub's -1/-2 suffixes -- the same
    rule as check.heading_slugs, kept in step by using its slugify."""
    seen, out = {}, []
    for i, line in enumerate(strip_fences(text).split("\n"), 1):
        m = HEADING_RE.match(line)
        if not m:
            continue
        base = slugify(m.group(2))
        if not base:
            continue
        n = seen.get(base, 0)
        seen[base] = n + 1
        out.append((i, base if n == 0 else "%s-%d" % (base, n), m.group(2).strip()))
    return out


CALL_RE = re.compile(r"^([A-Za-z_][\w:.>-]*)\s*\(.*\)$")
PATH_RE = re.compile(r"^(\S+\.(?:md|h|hpp|cpp|c|py|txt|cmake|html|svg|json|obj|stl|3mf|fld|glsl|cs))"
                     r"(?::\d+(?:-\d+)?(?:,\d+(?:-\d+)?)*)?$")
SECTION_RE = re.compile(r"^\d{1,2}\s?\xa7\s?[\w.]+$")
PROG_RE = re.compile(r"^(?:\./)?(dualc_[a-z_]+)(?:\.exe)?(\s.*)?$")
FLAG_SPAN_RE = re.compile(r"^(--?[A-Za-z][\w-]*)(\s+\S.*)?$")
FLAG_RE = re.compile(r"(?<![\w-])(--[a-z][a-z0-9-]+)(?![\w-])")
BARE_ID_RE = re.compile(r"(?<![\w-])(#\d{1,3}|D-\d{2}|[A-Z]\d{1,2})(?![\w-])")


def apply_renames(s, renames):
    """Rewrite every renamed path (longest first) inside a string."""
    for old, new in renames:
        if old in s:
            s = s.replace(old, new)
    return s


def ident_normalize(span, renames=()):
    """The identifiers a backticked span stands for, or [] if it is prose."""
    s = fold(span).strip().strip("'\"").rstrip(",;.:")
    if len(s) < 2 or len(s) > 80 or s.isdigit() or re.fullmatch(r"[a-z]{1,3}", s):
        return []
    if not re.search(r"\w", s):
        return []
    m = CALL_RE.match(s)
    if m:
        return [m.group(1)]
    m = PATH_RE.match(s)
    if m:
        return [apply_renames(m.group(1), renames)]
    m = SECTION_RE.match(s)
    if m:
        return [s.replace(" ", "")]
    m = PROG_RE.match(s)
    if m:
        out = [m.group(1)]
        rest = m.group(2) or ""
        out.extend(FLAG_RE.findall(rest))
        out.append(re.sub(r"\s+", " ", s))
        return out
    m = FLAG_SPAN_RE.match(s)
    if m:
        out = [m.group(1)]
        if m.group(2):
            out.append(re.sub(r"\s+", " ", s))
        return out
    if re.search(r"\s", s):
        return []
    return [apply_renames(s, renames)]


def identifier_units(text, renames=()):
    """(line, ident) for every identifier outside fences. On a table row the
    cells are split first so an unpaired backtick stays inside its cell; a
    cell with an odd backtick count is skipped."""
    out = []
    for i, line in enumerate(strip_fences(text).split("\n"), 1):
        segs = table_cells(line) if line.strip().startswith("|") else [line]
        for seg in segs:
            if seg.count("`") % 2:
                continue
            for m in INLINE_CODE_RE.finditer(seg):
                for ident in ident_normalize(m.group(0)[1:-1], renames):
                    out.append((i, ident))
            rest = LINK_TARGET_RE.sub("]", INLINE_CODE_RE.sub(" ", seg))
            for m in FLAG_RE.finditer(rest):
                out.append((i, m.group(1)))
            for m in BARE_ID_RE.finditer(rest):
                out.append((i, m.group(1)))
    return out


def boundary_re(needle):
    pre = r"(?<![\w-])" if re.match(r"\w", needle) else ""
    post = r"(?![\w-])" if re.search(r"\w$", needle) else ""
    return re.compile(pre + re.escape(needle) + post)


def residual_for_code_only(ident):
    """A flag, a program name or a docs path alive in code but gone from the
    docs is a documented-fact loss, not a kept identifier."""
    return ident.startswith("-") or ident.startswith("dualc_") or ident.startswith("docs/")


def unit_key(unit, text):
    return hashlib.sha1((unit + "\n" + text).encode("utf-8")).hexdigest()[:12]


# --------------------------------------------------------------------------
# Git access -- blobs only, never the working tree
# --------------------------------------------------------------------------

def die(msg):
    sys.stderr.write("docs_loss_audit: %s\n" % msg)
    sys.exit(2)


def git_out(root, args):
    p = subprocess.run(["git", "-C", root] + args, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if p.returncode != 0:
        die("git %s failed: %s" % (" ".join(args), p.stderr.strip()))
    return p.stdout


def git_rev(root, rev):
    return git_out(root, ["rev-parse", "--verify", rev + "^{commit}"]).strip()


def git_ls(root, rev):
    return [l for l in git_out(root, ["ls-tree", "-r", "--name-only", rev]).split("\n") if l]


def git_blobs(root, rev, paths):
    """{path: text as committed} through one `git cat-file --batch` process."""
    if not paths:
        return {}
    inp = "".join("%s:%s\n" % (rev, p) for p in paths).encode("utf-8")
    p = subprocess.run(["git", "-C", root, "cat-file", "--batch"], input=inp, capture_output=True)
    if p.returncode != 0:
        die("git cat-file --batch failed: %s" % p.stderr.decode("utf-8", "replace").strip())
    out, pos, res = p.stdout, 0, {}
    for path in paths:
        nl = out.index(b"\n", pos)
        header = out[pos:nl].decode("utf-8", "replace").split()
        pos = nl + 1
        if header[-1] == "missing":
            die("%s:%s is missing" % (rev, path))
        size = int(header[2])
        res[path] = out[pos:pos + size].decode("utf-8", "replace")
        pos += size + 1
    return res


def rename_map(root, base, head):
    """[(old, new)] for every R row of `git diff -M`, plus the directory pairs
    they imply, longest old path first; and the set of R100 old paths."""
    pairs, exact = set(), set()
    for line in git_out(root, ["diff", "-M", "--name-status", base, head]).split("\n"):
        parts = line.split("\t")
        if len(parts) == 3 and parts[0].startswith("R"):
            old, new = parts[1], parts[2]
            pairs.add((old, new))
            if parts[0] == "R100":
                exact.add(old)
            od, nd = os.path.dirname(old), os.path.dirname(new)
            if od != nd and os.path.basename(old) == os.path.basename(new):
                pairs.add((od + "/", nd + "/"))
    return sorted(pairs, key=lambda p: (-len(p[0]), p[0])), exact


def commit_exists(root, h):
    rc, _ = run(["git", "-C", root, "cat-file", "-e", h + "^{commit}"])
    return rc == 0


# --------------------------------------------------------------------------
# The head corpus
# --------------------------------------------------------------------------

def tier_of(path):
    for pat in CORPUS_EXCLUDE:
        if re.match(pat, path):
            return "excluded"
    if path.startswith(TIER_C_OVERRIDE):
        return "c"
    if path.startswith(TIER_B):
        return "b"
    if path.startswith(TIER_A):
        return "a"
    if path.startswith(TIER_C):
        return "c"
    if path.startswith(TIER_CODE):
        return "code"
    return None


class HeadFile(object):
    def __init__(self, path, tier, text, renames):
        self.path, self.tier, self.text = path, tier, text
        if tier == "code":
            self.idents = set()
            return
        self.words = words(text)
        self.g8 = shingles(self.words, T["K_LONG"])
        self.g4 = shingles(self.words, T["K_SHORT"])
        self.slugs = heading_slugs(text)
        self.blocks = [block_vocab(b) for _, k, b in blocks(text)]
        self.idents = set(i for _, i in identifier_units(text, renames))


class Corpus(object):
    def __init__(self, files):
        self.files = files                      # sorted by path
        self.by_tier = {}
        for i, f in enumerate(files):
            self.by_tier.setdefault(f.tier, []).append(i)
        self.inv8, self.inv4 = {}, {}
        for i, f in enumerate(files):
            if f.tier == "code":
                continue
            for g in f.g8:
                self.inv8.setdefault(g, []).append(i)
            for g in f.g4:
                self.inv4.setdefault(g, []).append(i)
        self.tok_index = {}
        for i, f in enumerate(files):
            if f.tier == "code":
                continue
            for bi, vocab in enumerate(f.blocks):
                for tok in vocab:
                    if any(c.isdigit() for c in tok):
                        self.tok_index.setdefault(tok, []).append((i, bi))
        self.slugs = {}
        for i, f in enumerate(files):
            if f.tier != "code":
                for s in f.slugs:
                    self.slugs.setdefault(s, []).append(i)
        self.ident_files = {}
        for i, f in enumerate(files):
            for ident in f.idents:
                self.ident_files.setdefault(ident, []).append(i)
        self.tier_text = {}
        for tier, ids in self.by_tier.items():
            self.tier_text[tier] = [(i, files[i].text) for i in ids]

    def best_sentence(self, ws, tiers):
        """(sck, sc4, file_index) of the best-scoring file among `tiers`."""
        s8, s4 = shingles(ws, T["K_LONG"] if len(ws) >= T["K_LONG"] else len(ws)), shingles(ws, T["K_SHORT"])
        hits = {}
        for g in s8:
            for i in self.inv8.get(g, ()):
                hits[i] = hits.get(i, 0) + 1
        if not hits:
            for g in s4:
                for i in self.inv4.get(g, ()):
                    hits[i] = hits.get(i, 0) + 1
        cands = [i for i in hits if self.files[i].tier in tiers]
        cands.sort(key=lambda i: (-hits[i], self.files[i].path))
        best = (0.0, 0.0, -1)
        for i in cands[:T["TOP_K_FILES"]]:
            f = self.files[i]
            sck = len(s8 & f.g8) / float(len(s8)) if s8 else 0.0
            sc4 = len(s4 & f.g4) / float(len(s4)) if s4 else 0.0
            if (sck, sc4) > best[:2]:
                best = (sck, sc4, i)
        return best

    def evidence_hit(self, tok, ctx, tiers):
        """The first file (by path) with a block holding tok and enough of ctx;
        -1 if none; -2 if the token exists in `tiers` but no block passes."""
        seen = False
        for i, bi in self.tok_index.get(tok, ()):
            f = self.files[i]
            if f.tier not in tiers:
                continue
            seen = True
            if sum(1 for c in ctx if c in f.blocks[bi]) >= T["CTX_MIN_HIT"]:
                return i
        return -2 if seen else -1

    def ident_hit(self, ident, tiers):
        for i in self.ident_files.get(ident, ()):
            if self.files[i].tier in tiers:
                return i
        rx = boundary_re(ident)
        for tier in sorted(tiers):
            for i, text in self.tier_text.get(tier, ()):
                if rx.search(text):
                    return i
        return -1


# --------------------------------------------------------------------------
# Matching -- one Row per base unit
# --------------------------------------------------------------------------

class Row(object):
    __slots__ = ("unit", "source", "line", "key", "text", "bucket", "best", "verdict",
                 "cite", "rule", "annex")

    def __init__(self, unit, source, line, key, text):
        self.unit, self.source, self.line, self.key, self.text = unit, source, line, key, text
        self.bucket, self.best, self.verdict, self.cite, self.rule, self.annex = "", "", "", "", "", False

    def residual(self):
        return self.bucket != "kept"


def fmt_best(corpus, i, sck=None, sc4=None):
    if i < 0:
        return "-"
    f = corpus.files[i]
    if sck is None:
        return "%s %s" % (f.tier, f.path)
    return "%s %s %.3f/%.3f" % (f.tier, f.path, sck, sc4)


def match_file(corpus, source, text, renames, only):
    """Every unit of one base file, scored against the corpus."""
    rows = []
    live = ("a", "b")
    text = apply_renames(text, renames)
    if "sentence" in only:
        for line, kind, s, ws in sentence_units(text):
            r = Row("sentence", source, line, unit_key("sentence", " ".join(ws)), s)
            sck, sc4, i = corpus.best_sentence(ws, live)
            r.bucket = sentence_bucket(sck, sc4)
            r.best = fmt_best(corpus, i, sck, sc4)
            if r.bucket != "kept":
                csck, csc4, ci = corpus.best_sentence(ws, ("c",))
                if sentence_bucket(csck, csc4) == "kept":
                    r.bucket, r.best = "raw-only", fmt_best(corpus, ci, csck, csc4)
            rows.append(r)
    if "evidence" in only:
        for line, kind, tok, ctx in evidence_units(text):
            r = Row("evidence", source, line, unit_key("evidence", tok + " " + " ".join(ctx)),
                    "%s  [%s]" % (tok, ", ".join(ctx)))
            if len(ctx) < T["CTX_MIN_HIT"]:
                i = corpus.evidence_hit(tok, ctx, live)
                r.bucket = "unverifiable-context"
                r.best = fmt_best(corpus, i) if i >= 0 else ("token present" if i == -2 else "-")
            else:
                i = corpus.evidence_hit(tok, ctx, live)
                if i >= 0:
                    r.bucket, r.best = "kept", fmt_best(corpus, i)
                else:
                    ci = corpus.evidence_hit(tok, ctx, ("c",))
                    if ci >= 0:
                        r.bucket, r.best = "raw-only", fmt_best(corpus, ci)
                    else:
                        r.bucket, r.best = ("token-only" if i == -2 else "gone"), "-"
            rows.append(r)
    if "heading" in only:
        for line, slug, htext in heading_units(text):
            r = Row("heading", source, line, unit_key("heading", slug), "%s  (%s)" % (slug, htext))
            ids = [i for i in corpus.slugs.get(slug, ()) if corpus.files[i].tier in live]
            if ids:
                r.bucket, r.best = "kept", fmt_best(corpus, ids[0])
            else:
                cids = [i for i in corpus.slugs.get(slug, ()) if corpus.files[i].tier == "c"]
                if cids:
                    r.bucket, r.best = "raw-only", fmt_best(corpus, cids[0])
                else:
                    ws = words(htext)
                    sck, sc4, i = corpus.best_sentence(ws, live) if ws else (0.0, 0.0, -1)
                    r.bucket, r.best = "gone", fmt_best(corpus, i, sck, sc4)
            rows.append(r)
    if "identifier" in only:
        seen = set()
        for line, ident in identifier_units(text, renames):
            if ident in seen:
                continue
            seen.add(ident)
            r = Row("identifier", source, line, unit_key("identifier", ident), ident)
            i = corpus.ident_hit(ident, live)
            if i >= 0:
                r.bucket, r.best = "kept", fmt_best(corpus, i)
            else:
                i = corpus.ident_hit(ident, ("code",))
                if i >= 0:
                    r.bucket = "code-only" if residual_for_code_only(ident) else "kept"
                    r.best = fmt_best(corpus, i)
                else:
                    i = corpus.ident_hit(ident, ("c",))
                    r.bucket = "raw-only" if i >= 0 else "gone"
                    r.best = fmt_best(corpus, i)
            rows.append(r)
    return rows


# --------------------------------------------------------------------------
# Triage
# --------------------------------------------------------------------------

def parse_record(s):
    if "#" not in s:
        return None, None
    path, anchor = s.split("#", 1)
    return path, anchor


def resolve_citation(head_text, anchor, quote):
    """'' if the anchor is a heading of the file (slugs computed on the text as
    committed, as the gate does) and the folded quote is in its folded words; else the reason."""
    if head_text is None:
        return "record file not on head"
    if anchor not in heading_slugs(head_text):
        return "anchor #%s not in record" % anchor
    q = words(fold(quote))
    if len(q) < 4:
        return "quote shorter than 4 words"
    if (" " + " ".join(q) + " ") not in (" " + " ".join(words(fold(head_text))) + " "):
        return "quote not found in record"
    return ""


def validate_audit(audit, base_full, scope, annex):
    errs = []
    if audit.get("base") not in (base_full, base_full[:7]):
        errs.append("triage `base` %r is not %s" % (audit.get("base"), base_full[:7]))
    if audit.get("thresholds") != THRESHOLDS:
        errs.append("triage `thresholds` differ from the script's constants; the triage is void")
    for k, item in audit.get("items", {}).items():
        if k.startswith("_"):
            continue
        for f in ("unit", "source", "verdict"):
            if f not in item:
                errs.append("item %s lacks `%s`" % (k, f))
        v = item.get("verdict")
        if v not in VERDICTS:
            errs.append("item %s: unknown verdict %r" % (k, v))
        if v in CITING and ("record" not in item or "quote" not in item):
            errs.append("item %s: %s needs `record` and `quote`" % (k, v))
        if v == "restored" and not re.fullmatch(r"[0-9a-f]{7,40}", item.get("commit", "")):
            errs.append("item %s: restored needs a 7-40 hex `commit`" % k)
        if v == "noise" and "note" not in item:
            errs.append("item %s: noise needs a `note`" % k)
    for rule in audit.get("rules", []):
        rid = rule.get("id", "?")
        for f in ("id", "unit", "source", "max_matches", "verdict"):
            if f not in rule:
                errs.append("rule %s lacks `%s`" % (rid, f))
        v = rule.get("verdict")
        if v == "restored":
            errs.append("rule %s: restorations are per item" % rid)
        elif v not in VERDICTS:
            errs.append("rule %s: unknown verdict %r" % (rid, v))
        if v in CITING and ("record" not in rule or "quote" not in rule or "category" not in rule):
            errs.append("rule %s: %s needs `record`, `quote` and `category`" % (rid, v))
        if v == "noise" and "pattern" not in rule:
            errs.append("rule %s: noise needs a `pattern`" % rid)
        if rule.get("unit") == "sentence":
            b = rule.get("bucket")
            if b not in ("rewritten", "partial", "gone", "raw-only"):
                errs.append("rule %s: a sentence rule needs `bucket` rewritten|partial|gone|raw-only" % rid)
            elif b != "rewritten" and "category" not in rule:
                errs.append("rule %s: a %s rule needs a `category` from the removal catalogue" % (rid, b))
    return errs


def rule_matches(rule, row):
    if rule["unit"] != row.unit or rule["source"] != row.source:
        return False
    if "bucket" in rule and rule["bucket"] != row.bucket:
        return False
    if "pattern" in rule and not re.search(rule["pattern"], row.text):
        return False
    return True


def apply_triage(rows, audit, head_blobs, root):
    """Fill verdict/cite/rule on every residual row. Returns (problems, notes):
    problems make the audit fail, notes are informational."""
    problems, notes = [], []
    items = {k: v for k, v in audit.get("items", {}).items() if not k.startswith("_")}
    rules = audit.get("rules", [])
    by_key = {}
    for r in rows:
        by_key.setdefault(r.key, []).append(r)
    cite_cache = {}

    def cite(entry, label):
        path, anchor = parse_record(entry.get("record", ""))
        if path is None:
            return "record is not path#anchor"
        ck = (path, anchor, entry.get("quote", ""))
        if ck not in cite_cache:
            cite_cache[ck] = resolve_citation(head_blobs.get(path), anchor, entry.get("quote", ""))
        return cite_cache[ck]

    # An item names one unit; the same text may occur in several base files
    # (one key, several rows) and the verdict applies to every one of them.
    for k, item in sorted(items.items()):
        rs = by_key.get(k, [])
        if not rs:
            notes.append("item %s (%s) matches no unit on the base tree: settled" % (k, item.get("text", "")[:40]))
            continue
        if item["unit"] != rs[0].unit or item["source"] not in [r.source for r in rs]:
            problems.append("item %s: unit/source do not match the row (%s %s)" % (k, rs[0].unit, rs[0].source))
            continue
        if not any(r.residual() for r in rs):
            notes.append("item %s is kept on head: settled (%s)" % (k, item["verdict"]))
            continue
        v = item["verdict"]
        if v == "restored":
            if not commit_exists(root, item["commit"]):
                problems.append("item %s: commit %s does not exist" % (k, item["commit"]))
            problems.append("item %s: restored in %s but still %s on head (stale)" % (k, item["commit"], rs[0].bucket))
            for r in rs:
                r.verdict, r.cite = "restored?", item["commit"]
            continue
        if v in CITING:
            why = cite(item, k)
            if why:
                problems.append("item %s: %s" % (k, why))
        for r in rs:
            if r.residual():
                r.verdict = v
                if v in CITING:
                    r.cite = item["record"]
    counts = {}
    for rule in rules:
        rid = rule["id"]
        counts[rid] = 0
        if rule["verdict"] in CITING:
            why = cite(rule, rid)
            if why:
                problems.append("rule %s: %s" % (rid, why))
        for r in rows:
            if r.residual() and not r.verdict and rule_matches(rule, r):
                r.verdict, r.rule = rule["verdict"], rid
                r.cite = rule.get("record", rule.get("pattern", ""))
                counts[rid] += 1
        if counts[rid] > rule["max_matches"]:
            problems.append("rule %s absorbed %d rows, max_matches is %d" % (rid, counts[rid], rule["max_matches"]))
        if counts[rid] == 0:
            notes.append("rule %s matches nothing: settled" % rid)
    return problems, notes, counts


# --------------------------------------------------------------------------
# Report
# --------------------------------------------------------------------------

def fmt_row(r):
    v = r.verdict or ("untriaged" if r.residual() else "kept")
    if r.rule:
        v = "%s (%s)" % (v, r.rule)
    return "%s | %s:%d | %s | %s | %s%s\n    text: %s" % (
        r.key, r.source, r.line, r.bucket, r.best, v,
        (" " + r.cite) if r.cite and not r.rule else "", r.text[:160])


def report(args, base_full, head_full, base_paths, corpus, rows, rule_counts, problems, notes,
           excluded, renames, audit):
    out = []
    w = out.append
    w("docs_loss_audit  base=%s  head=%s  audit=%s" % (base_full, head_full, args.audit))
    w("scope: %s  (%d base files)" % (" ".join(args.scope), len(base_paths)))
    w("annex: %s" % " ".join(args.annex))
    tiers = " ".join("%s=%d" % (t, len(corpus.by_tier.get(t, ()))) for t in ("a", "b", "c", "code"))
    w("tiers: %s  excluded=%d (CORPUS_EXCLUDE)" % (tiers, excluded))
    w("renames applied to base text: %d" % len(renames))
    w("thresholds: %s" % " ".join("%s=%s" % (k, T[k]) for k in sorted(T)))
    if args.only != list(UNITS):
        w("only: %s" % " ".join(args.only))
    if args.source:
        w("source filter: %s" % args.source)
    shown = [r for r in rows if (args.all or r.residual())
             and (not args.source or r.source.startswith(args.source))
             and (not args.untriaged or (r.residual() and not r.verdict))]
    for annex in (False, True):
        sub = [r for r in shown if r.annex == annex]
        if annex:
            if not any(r.annex for r in rows):
                continue
            w("")
            w("== annex %s ==" % " ".join(args.annex))
        for unit in args.only:
            ur = [r for r in sub if r.unit == unit]
            allr = [r for r in rows if r.unit == unit and r.annex == annex]
            res = [r for r in allr if r.residual()]
            untr = [r for r in res if not r.verdict]
            w("")
            w("== %s%s  units=%d residual=%d untriaged=%d ==" % (
                unit, " (annex)" if annex else "", len(allr), len(res), len(untr)))
            for r in sorted(ur, key=lambda r: (r.source, r.line, r.key)):
                w(fmt_row(r))
    w("")
    w("== summary ==")
    hdr = ("%-11s" % "unit" + "".join("%11s" % BUCKET_SHORT[b] for b in BUCKETS) + " |"
           + "".join("%12s" % v for v in VERDICTS) + "%12s" % "untriaged")
    w(hdr)
    for annex in (False, True):
        for unit in args.only:
            ur = [r for r in rows if r.unit == unit and r.annex == annex]
            if not ur:
                continue
            bc = dict((b, sum(1 for r in ur if r.bucket == b)) for b in BUCKETS)
            vc = dict((v, sum(1 for r in ur if r.verdict == v)) for v in VERDICTS)
            untr = sum(1 for r in ur if r.residual() and not r.verdict)
            w("%-11s" % (unit + ("*" if annex else "")) + "".join("%11d" % bc[b] for b in BUCKETS)
              + " |" + "".join("%12d" % vc[v] for v in VERDICTS) + "%12d" % untr)
    if any(r.annex for r in rows):
        w("(* = annex)")
    if rule_counts:
        w("rules: " + "  ".join("%s=%d" % (k, v) for k, v in sorted(rule_counts.items())))
    for n in notes:
        w("note: %s" % n)
    for p in problems:
        w("PROBLEM: %s" % p)
    untriaged = sum(1 for r in rows if r.residual() and not r.verdict)
    limit = audit.get("untriaged_max", 0) if audit else 0
    ok = untriaged <= limit and not problems
    w("untriaged: %d (max %d)  exit: %d" % (untriaged, limit, 0 if ok else 1))
    return "\n".join(out) + "\n", (0 if ok else 1)


# --------------------------------------------------------------------------
# Selftest -- the pure functions, no git
# --------------------------------------------------------------------------

def selftest():
    fails = []

    def check(name, cond):
        if not cond:
            fails.append(name)

    # fold symmetry: the typographic and the ASCII spelling are one string
    check("fold", fold("\u2248 \u2153 \u2013 \u201cx\u201d 80\xb3") == "~ 1/3 - \"x\" 803")
    # paragraphs reflow, list items split, table separator dropped
    b = blocks("# H\n\nOne line\nwraps here.\n\n- item one\n- item two\n\n| a | b |\n| --- | --- |\n| c | d |\n")
    check("blocks", [k for _, k, _ in b] == ["heading", "para", "para", "para", "row", "row"])
    check("blocks-join", b[1][2] == "One line wraps here.")
    check("blocks-row", b[5][2] == "c ; d")
    s = sentence_units("The quick brown fox jumps over the lazy dog. It sleeps. Then the quick brown fox eats a big meal today.\n")
    check("sentences", len(s) == 2 and s[0][3][0] == "the")
    # bucket edges
    check("bucket", (sentence_bucket(0.6, 0), sentence_bucket(0.59, 0.6), sentence_bucket(0.1, 0.3),
                     sentence_bucket(0, 0.29)) == ("kept", "rewritten", "partial", "gone"))
    # short units: one shingle, kept iff verbatim
    check("kshort", shingles(["a", "b", "c", "d", "e", "f"], 8) == {"a b c d e f"})
    # identifiers
    ren = [("docs/study/", "docs/raw/study/")]
    check("id-call", ident_normalize("dualc_mesh_free(m, 0)") == ["dualc_mesh_free"])
    check("id-path", ident_normalize("docs/study/A3-the-qef.md:239-251", ren) == ["docs/raw/study/A3-the-qef.md"])
    check("id-prog", ident_normalize("dualc_field --mem 4G") == ["dualc_field", "--mem", "dualc_field --mem 4G"])
    check("id-flag", ident_normalize("--depth 7") == ["--depth", "--depth 7"])
    check("id-sec", ident_normalize("07 \xa76") == ["07\xa76"])
    check("id-T4", ident_normalize("T4") == ["T4"])
    check("id-prose", ident_normalize("some prose words") == [])
    check("id-num", ident_normalize("300") == [])
    iu = identifier_units("| `foo_bar` and `--bake` | odd ` here | see #34 and D-12 and [x](#--thin-features) |\n")
    ids = [i for _, i in iu]
    check("id-cells", "foo_bar" in ids and "--bake" in ids and "#34" in ids and "D-12" in ids
          and "--thin-features" not in ids and not any(" " in i for i in ids))
    # evidence
    toks = evidence_tokens("on 2026-06-24 commit 3af866c v0.3.1 1,024 faces 0.25 mm 205/205 3x 12345 and 300 and 16")
    check("ev-tokens", toks == ["2026-06-24", "3af866c", "v0.3.1", "1,024", "0.25mm", "205/205", "3x", "12345"])
    ev = evidence_units("| molde | default | 38,116 | 76,228 |\n\nThe gyroid shell took 4.6 GB of memory at depth seven.\n")
    check("ev-row", ev[1][2] == "76,228" and ev[1][3] == ["38,116", "default", "molde"])
    check("ev-para", ev[2][2] == "4.6GB" and set(ev[2][3]) <= {"memory", "took", "shell", "gyroid", "depth"} and len(ev[2][3]) == 3)
    # headings
    hu = heading_units("# A\n## B c\n## B c\n")
    check("headings", [s for _, s, _ in hu] == ["a", "b-c", "b-c-1"])
    # citation
    check("cite-ok", resolve_citation("# Rec\n\nthe types table is re-lettered Y1-Y6 today.\n", "rec",
                                      "types table is re-lettered") == "")
    check("cite-anchor", resolve_citation("# Rec\n", "nope", "a b c d") != "")
    check("cite-quote", resolve_citation("# Rec\nx\n", "rec", "not in there at all") != "")
    # rules
    audit = {"base": "x", "thresholds": dict(THRESHOLDS), "items": {}, "rules": [
        {"id": "r1", "unit": "sentence", "source": "a.md", "bucket": "gone", "max_matches": 1,
         "verdict": "intentional", "record": "r.md#r", "quote": "a b c d"}]}
    check("rule-needs-category", any("category" in e for e in validate_audit(audit, "x", (), ())))
    audit["rules"][0]["category"] = "A1"
    check("rule-ok", validate_audit(audit, "x", (), ()) == [])
    audit["thresholds"]["K_LONG"] = 9
    check("thresholds-pinned", any("thresholds" in e for e in validate_audit(audit, "x", (), ())))
    check("key-stable", unit_key("sentence", "a b") == unit_key("sentence", "a b") and len(unit_key("x", "y")) == 12)
    print("selftest: %d checks, %d failed%s" % (30, len(fails), (": " + ", ".join(fails)) if fails else ""))
    return 1 if fails else 0


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

def main(argv):
    ap = argparse.ArgumentParser(description="deterministic docs loss audit between two commits")
    ap.add_argument("--base", default=None, help="base commit (default: merge-base main HEAD)")
    ap.add_argument("--head", default="HEAD")
    ap.add_argument("--scope", nargs="*", default=list(DEFAULT_SCOPE), help="base-tree path prefixes")
    ap.add_argument("--annex", nargs="*", default=list(DEFAULT_ANNEX), help="scope prefixes reported apart")
    ap.add_argument("--triage", default=None, help="triage JSON (default scripts/docs_loss_audit.json)")
    ap.add_argument("--audit", default=None, help="key under `audits` (default: base[:7])")
    ap.add_argument("--only", nargs="*", default=list(UNITS), choices=UNITS)
    ap.add_argument("--source", default="", help="print only rows from this base path prefix")
    ap.add_argument("--untriaged", action="store_true", help="print only residual rows without a verdict")
    ap.add_argument("--all", action="store_true", help="print kept rows too")
    ap.add_argument("--write-dump", default=None, help="write the report (identical bytes) here")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()

    root = repo_root()
    base_rev = args.base or git_out(root, ["merge-base", "main", "HEAD"]).strip()
    base_full, head_full = git_rev(root, base_rev), git_rev(root, args.head)
    args.audit = args.audit or base_full[:7]
    triage_path = args.triage or os.path.join(root, "scripts", "docs_loss_audit.json")

    renames, r100 = rename_map(root, base_full, head_full)
    base_all = git_ls(root, base_full)
    base_paths = sorted(p for p in base_all if p.startswith(tuple(args.scope)) and p.endswith(DOC_EXT))
    uncovered = [p for p in base_all if p.startswith("docs/") and p.endswith(DOC_EXT)
                 and p not in base_paths and p not in r100]
    if uncovered:
        die("base docs files neither in --scope nor an R100 rename:\n  " + "\n  ".join(uncovered))

    head_all = git_ls(root, head_full)
    head_sel, excluded = [], 0
    for p in head_all:
        t = tier_of(p)
        if t == "excluded":
            excluded += 1
        elif t == "code" and p.endswith(CODE_EXT) or t in ("a", "b", "c") and p.endswith(DOC_EXT):
            head_sel.append((p, t))
    head_blobs = git_blobs(root, head_full, [p for p, _ in head_sel])
    corpus = Corpus([HeadFile(p, t, fold(head_blobs[p]), renames) for p, t in head_sel])
    base_blobs = git_blobs(root, base_full, base_paths)

    rows = []
    for p in base_paths:
        rs = match_file(corpus, p, fold(base_blobs[p]), renames, args.only)
        annex = p.startswith(tuple(args.annex))
        for r in rs:
            r.annex = annex
        rows.extend(rs)

    audit, problems, notes, rule_counts = None, [], [], {}
    if os.path.exists(triage_path):
        try:
            with open(triage_path, encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError) as e:
            die("cannot read %s: %s" % (triage_path, e))
        audit = data.get("audits", {}).get(args.audit)
        if audit is None:
            die("%s has no audit %r" % (triage_path, args.audit))
        errs = validate_audit(audit, base_full, args.scope, args.annex)
        if errs:
            die("triage file invalid:\n  " + "\n  ".join(errs))
        problems, notes, rule_counts = apply_triage(rows, audit, head_blobs, root)
    else:
        notes.append("no triage file at %s: every residual is untriaged" % triage_path)

    text, rc = report(args, base_full, head_full, base_paths, corpus, rows, rule_counts,
                      problems, notes, excluded, renames, audit)
    sys.stdout.write(text)
    if args.write_dump:
        with open(args.write_dump, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
