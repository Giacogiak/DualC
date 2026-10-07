#!/usr/bin/env python3
"""DualC local checks gate -- the "CI" for a repo with no remote.

DualC has no GitHub remote and is not going to get one, so there is nothing to
trigger a hosted build. What CI actually buys is two separable things:

  (a) ONE reproducible gate -- a single command that runs every check and says
      pass/fail. That needs no server at all, and it is what this file is.
  (b) Unattended triggering by something that is not you. That is genuinely
      absent locally; scripts/hooks/pre-commit is an honest substitute for the
      fast tier, not an equivalent. Without a remote `pre-push` never fires, so
      the build tier stays "run the one command before you commit".

Usage:
    python scripts/check.py            # fast + build
    python scripts/check.py --fast     # docs/hygiene only, ~1s, no build
    python scripts/check.py --gpu      # opt-in: dualc_glsl_parity, needs GL
    python scripts/check.py --list     # what each check does

Exit codes: 0 all selected checks passed, 1 a check failed, 2 the gate itself
could not run (missing tool, bad usage, internal error) -- kept distinct so the
hook can tell "your commit is bad" from "the gate is broken".

Python 3, standard library only.
"""

import argparse
import ast
import json
import os
import re
import shutil
import subprocess
import sys
import time

# The console here is cp1252 and the docs are full of em-dashes, "pi" and ">=".
# Without this the gate crashes on the first file it tries to report about.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

OK, FAIL, SKIP, INFO = "OK", "FAIL", "SKIP", "INFO"


# --------------------------------------------------------------------------
# Repo access
# --------------------------------------------------------------------------

def run(cmd, cwd=None, capture=True):
    """Run a command, returning (returncode, output). Never raises on failure."""
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=capture, text=True,
                           encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return 127, ""
    out = ((p.stdout or "") + (p.stderr or "")) if capture else ""
    return p.returncode, out


def repo_root():
    rc, out = run(["git", "rev-parse", "--show-toplevel"])
    if rc != 0:
        die("not inside a git work tree")
    return os.path.normpath(out.strip())


def die(msg):
    print("check.py: %s" % msg, file=sys.stderr)
    sys.exit(2)


def tracked(root, *patterns):
    """Files git knows about.

    Deliberately the INDEX, not a filesystem walk: run from the pre-commit
    hook, that is what makes a staged `git add` of a new file visible to the
    STRUCTURE.md check. The trade-off (documented, accepted for a
    single-developer local repo) is that file CONTENT is read from the work
    tree, so a partial `git add` can commit content the gate never saw.
    """
    rc, out = run(["git", "-C", root, "ls-files"] + list(patterns))
    if rc != 0:
        die("git ls-files failed")
    return [p.strip() for p in out.splitlines() if p.strip()]


def read_text(root, rel):
    """Universal-newline UTF-8 read. Returns None if the path is not on disk.

    A staged deletion leaves a path in `git ls-files` with no file behind it;
    the gate must report that, not die with a traceback mid-commit.
    """
    try:
        with open(os.path.join(root, rel), encoding="utf-8", newline=None) as f:
            return f.read()
    except (OSError, UnicodeDecodeError):
        return None


def measure(text):
    """(lines, words, bytes) with bytes computed from CONTENT, not from disk.

    os.path.getsize() counts CRLF as two bytes, so a file re-saved with
    different line endings would shift its byte count with no content change
    and trip the size ratchet for nothing.
    """
    return len(text.splitlines()), len(text.split()), len(text.encode("utf-8"))


# --------------------------------------------------------------------------
# Markdown parsing
# --------------------------------------------------------------------------

FENCE_RE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE_RE = re.compile(r"`[^`\n]*`")
LINK_RE = re.compile(r"\[[^\]\n]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
MD_LINK_TEXT_RE = re.compile(r"\[([^\]]*)\]\([^)]*\)")


def strip_fences(text):
    """Blank out fenced code blocks, preserving line numbering."""
    out, in_fence, marker = [], False, None
    for line in text.split("\n"):
        m = FENCE_RE.match(line)
        if m:
            if not in_fence:
                in_fence, marker = True, m.group(1)
                out.append("")
                continue
            if line.strip().startswith(marker):
                in_fence, marker = False, None
                out.append("")
                continue
        out.append("" if in_fence else line)
    return "\n".join(out)


def strip_code(text):
    """Fences AND inline code removed -- what link extraction should see.

    Required, not cosmetic: docs/design/ pages and docs/raw/study/B4-concurrency.md
    contain `](std::size_t i)` inside fences, which the link regex happily reads
    as a link to a file named `std::size_t`. Inline code goes too, so a link
    written inside backticks as an EXAMPLE is not chased as a real one.

    Headings must NOT be run through this -- see heading_slugs().
    """
    return "\n".join(INLINE_CODE_RE.sub("", l) for l in strip_fences(text).split("\n"))


def links_in(text):
    """Yield (line_number, target) for every markdown link outside code."""
    for i, line in enumerate(strip_code(text).split("\n"), 1):
        for m in LINK_RE.finditer(line):
            yield i, m.group(1)


def table_cells(line, strip_chars=""):
    """Split one markdown table row into its cells. A cell may carry a literal
    pipe as `\\|` (row D-28 of the decisions index does), so the split is on
    unescaped pipes only and the escape is undone in the cell text; splitting
    on a bare `|` would shift every column after it. `strip_chars` are
    stripped from each cell after whitespace."""
    body = line.strip()
    if body.startswith("|"):
        body = body[1:]
    if body.endswith("|") and not body.endswith("\\|"):
        body = body[:-1]
    return [c.strip().strip(strip_chars).replace("\\|", "|")
            for c in re.split(r"(?<!\\)\|", body)]


def slugify(heading):
    """GitHub's heading -> anchor rule.

    Lowercase, drop everything but [a-z0-9 _-], spaces to hyphens. Markdown
    emphasis and link syntax are removed first, so `## **#34** [x](y)` and the
    docs' heavy use of section signs, em-dashes and backticks all land on the
    same slug the rendered page would use.
    """
    h = MD_LINK_TEXT_RE.sub(r"\1", heading)
    h = h.replace("`", "").replace("*", "").replace("_", "_")
    h = h.lower()
    h = re.sub(r"[^a-z0-9 _\-]", "", h)
    return h.strip().replace(" ", "-")


def heading_slugs(text):
    """Every anchor a file offers, with GitHub's -1/-2 duplicate suffixes."""
    # strip_fences, NOT strip_code: GitHub keeps the text inside backticks, so
    # `### 10.1 \`ImplicitField\`` anchors as #101-implicitfield. Dropping the
    # code span would slug it as #101 and report 40 false broken anchors.
    seen, slugs = {}, set()
    for line in strip_fences(text).split("\n"):
        m = HEADING_RE.match(line)
        if not m:
            continue
        base = slugify(m.group(2))
        if not base:
            continue
        n = seen.get(base, 0)
        seen[base] = n + 1
        slugs.add(base if n == 0 else "%s-%d" % (base, n))
    return slugs


def expand_braces(text):
    """Expand STRUCTURE.md's `mesh_bvh.{h,cpp}` shorthand into real names.

    Without this a naive basename scan reports ~15 false omissions.
    """
    names = []
    for m in re.finditer(r"([A-Za-z0-9_./+-]+)\.\{([^}\n]*)\}", text):
        for ext in m.group(2).split(","):
            ext = ext.strip()
            if ext:
                names.append("%s.%s" % (m.group(1), ext))
    return names


# --------------------------------------------------------------------------
# Check registry
# --------------------------------------------------------------------------

CHECKS = []


class Result:
    def __init__(self, status, summary, details=None):
        self.status = status
        self.summary = summary
        self.details = details or []


def check(cid, tier, purpose, report_only=False):
    """`report_only` checks answer INFO, never FAIL, unless `--strict` (the
    session-end mode) promotes their INFO to FAIL."""
    def deco(fn):
        fn.cid, fn.tier, fn.purpose, fn.report_only = cid, tier, purpose, report_only
        CHECKS.append(fn)
        return fn
    return deco


def walk_files(root):
    """Every file under `root`, repo-relative with forward slashes -- the
    fixture trees of `--selftest` are not git repositories."""
    out = []
    for d, _, names in os.walk(root):
        for n in names:
            rel = os.path.relpath(os.path.join(d, n), root).replace(os.sep, "/")
            out.append(rel)
    return sorted(out)


class Ctx:
    """Everything a check needs, resolved once."""

    def __init__(self, root, data, args, walk=False):
        self.root = root
        self.data = data
        self.args = args
        if walk:
            self.all_files = walk_files(root)
            self.md = [r for r in self.all_files if r.endswith(".md")]
        else:
            self.md = tracked(root, "*.md")
            self.all_files = tracked(root)
        self._text = {}

    def text(self, rel):
        if rel not in self._text:
            self._text[rel] = read_text(self.root, rel)
        return self._text[rel]

    def exists(self, rel):
        return os.path.exists(os.path.join(self.root, rel))


# --------------------------------------------------------------------------
# Fast tier -- docs contract and repo hygiene
# --------------------------------------------------------------------------

@check("links", "fast", "every relative markdown link target exists")
def check_links(ctx):
    ext = tuple(ctx.data["external_link_prefixes"])
    out_of_repo = tuple(ctx.data["out_of_repo_links"]["prefixes"])
    total, external, foreign, bad = 0, 0, 0, []
    for rel in ctx.md:
        text = ctx.text(rel)
        if text is None:
            bad.append("%s:0: tracked but not on disk" % rel)
            continue
        base = os.path.dirname(rel)
        for line, target in links_in(text):
            if target.startswith(ext):
                external += 1
                continue
            if target.startswith("#"):
                continue          # same-file anchor: the anchors check owns it
            total += 1
            if target.startswith(out_of_repo):
                # Deliberate cross-project links (D:\Boletus). They resolve only
                # where that sibling is checked out, so they can never be a
                # portable invariant -- counted, never failed.
                foreign += 1
                continue
            path = target.split("#", 1)[0]
            if not path:
                continue
            if not ctx.exists(os.path.normpath(os.path.join(base, path))):
                bad.append("%s:%d: broken link -> %s" % (rel, line, target))
    summary = "%d links, %d broken (%d external, %d cross-project)" % (
        total, len(bad), external, foreign)
    return Result(FAIL if bad else OK, summary, bad)


@check("anchors", "fast", "every #anchor resolves against the target's headings")
def check_anchors(ctx):
    ext = tuple(ctx.data["external_link_prefixes"])
    out_of_repo = tuple(ctx.data["out_of_repo_links"]["prefixes"])
    cache, cross, same, bad = {}, 0, 0, []

    def slugs_of(rel):
        if rel not in cache:
            text = ctx.text(rel)
            cache[rel] = heading_slugs(text) if text is not None else set()
        return cache[rel]

    for rel in ctx.md:
        text = ctx.text(rel)
        if text is None:
            continue
        base = os.path.dirname(rel)
        for line, target in links_in(text):
            if target.startswith(ext) or target.startswith(out_of_repo):
                continue
            if target.startswith("#"):
                same += 1
                if target[1:] not in slugs_of(rel):
                    bad.append("%s:%d: no such heading in this file -> %s"
                               % (rel, line, target))
                continue
            if "#" not in target:
                continue
            path, anchor = target.split("#", 1)
            tgt = os.path.normpath(os.path.join(base, path))
            if not ctx.exists(tgt):
                continue          # already reported by the links check
            cross += 1
            if anchor not in slugs_of(tgt.replace(os.sep, "/")):
                bad.append("%s:%d: no such heading in %s -> #%s"
                           % (rel, line, path, anchor))
    return Result(FAIL if bad else OK,
                  "%d cross-file + %d same-file anchors, %d unresolved"
                  % (cross, same, len(bad)), bad)


@check("indexes", "fast", "every numbered sibling is linked from its README")
def check_indexes(ctx):
    bad, roots, siblings = [], 0, 0
    for root in ctx.data["index_roots"]:
        readme = "%s/README.md" % root
        text = ctx.text(readme)
        if text is None:
            bad.append("%s:0: index README missing" % readme)
            continue
        roots += 1
        d = os.path.join(ctx.root, root)
        for name in sorted(os.listdir(d)):
            full = os.path.join(d, name)
            if name == "README.md":
                continue
            if os.path.isdir(full):
                # A numbered DIRECTORY is a topic too (17-code-audit-...), and
                # the index links its README rather than the directory.
                if not os.path.exists(os.path.join(full, "README.md")):
                    continue
                needle, label = "%s/README.md" % name, name + "/"
            elif name.endswith(".md"):
                needle, label = name, name
            else:
                continue
            siblings += 1
            if needle not in text:
                bad.append("%s:0: %s is not linked from the index" % (readme, label))
    return Result(FAIL if bad else OK,
                  "%d roots, %d siblings, %d unlinked" % (roots, siblings, len(bad)),
                  bad)


RUN_FILE_RE = re.compile(r"^(\d\d)-(\d{4}-\d\d-\d\d)-[a-z0-9-]+\.md$")


@check("semantic-lint-runs", "fast",
       "the /docs-semantic-lint run folder check_data.json names exists and its runs are dated")
def check_semantic_lint_runs(ctx):
    """The owner's `/docs-semantic-lint` command is one file for every repo
    that carries this gate; the only per-repo fact it needs -- where a run is
    written -- lives here, in `semantic_lint_runs.folder`, and this check keeps
    that fact true: the folder exists with a README, it is an index root (so
    `indexes` sees every run), and every run is `NN-<date>-<slug>.md` with its
    date on the first body line, the anatomy the command prescribes."""
    folder = ctx.data["semantic_lint_runs"]["folder"].rstrip("/")
    bad = []
    if not ctx.exists(folder + "/README.md"):
        bad.append("%s/README.md:0: run folder or its README missing" % folder)
        return Result(FAIL, "run folder missing", bad)
    if folder not in ctx.data["index_roots"]:
        bad.append("scripts/check_data.json:0: %s is not in index_roots" % folder)
    runs = 0
    for name in sorted(os.listdir(os.path.join(ctx.root, folder))):
        if name == "README.md" or not name.endswith(".md"):
            continue
        rel = "%s/%s" % (folder, name)
        m = RUN_FILE_RE.match(name)
        if not m:
            bad.append("%s:0: a run is named NN-<date>-<slug>.md" % rel)
            continue
        runs += 1
        text = ctx.text(rel) or ""
        heads = headings(text)
        first = first_body_line(text, heads[0][0]) if heads else ""
        if m.group(2) not in first:
            bad.append("%s:0: the date %s is not on the first body line" % (rel, m.group(2)))
    return Result(FAIL if bad else OK,
                  "%s: %d runs, %d problems" % (folder, runs, len(bad)), bad)


@check("sizes", "fast", "doc size contract, ratcheted against a recorded baseline")
def check_sizes(ctx):
    caps = ctx.data["size_caps"]
    scope = tuple(ctx.data["size_scope"])
    frozen = {e["path"] for e in ctx.data["frozen"]}
    frozen_pref = tuple(ctx.data["frozen_prefixes"])
    baseline = {k: v for k, v in ctx.data["size_baseline"].items()
                if not k.startswith("_")}
    bad, scanned, based, exempt = [], 0, 0, 0
    for rel in ctx.md:
        if not rel.startswith(scope):
            continue
        if rel in frozen or rel.startswith(frozen_pref):
            exempt += 1
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        scanned += 1
        lines, words, byts = measure(text)
        limit = dict(caps)
        if rel in baseline:
            based += 1
            for axis in ("lines", "words", "bytes"):
                limit[axis] = max(caps[axis], baseline[rel][axis])
        for axis, got in (("lines", lines), ("words", words), ("bytes", byts)):
            if got > limit[axis]:
                how = "grew past its recorded baseline" if rel in baseline \
                      else "over the size contract"
                bad.append("%s:0: %s -- %s %d > %d (raise it in "
                           "scripts/check_data.json only on purpose)"
                           % (rel, how, axis, got, limit[axis]))
    return Result(FAIL if bad else OK,
                  "%d files, %d exempt, %d baselined, %d over"
                  % (scanned, exempt, based, len(bad)), bad)


@check("frozen-decl", "fast", "each frozen file is still named in its declaring README")
def check_frozen_decl(ctx):
    """Ties the machine-readable frozen list back to the prose that declares it.

    The declarations are flowing sentences with inline links; parsing them as
    grammar would be fragile enough to turn this gate into a liability. Instead
    the paragraph is located by its marker and each entry's token looked for
    INSIDE it -- reword the paragraph and drop a file, and this goes red.

    Scoping to the paragraph is load-bearing, not tidiness. A first version
    searched the whole README for the filename and passed unconditionally,
    because docs/roadmap/README.md links every topic from its index table
    anyway; it certified nothing. (The roadmap declares its frozen files as
    bare numbers -- `13` -- which is why the token is configured per entry
    rather than derived from the filename.)
    """
    sections = ctx.data["frozen_sections"]
    bad = []
    for entry in ctx.data["frozen"]:
        path, decl, token = entry["path"], entry["declared_in"], entry["token"]
        if not ctx.exists(path):
            bad.append("%s:0: declared frozen but does not exist" % path)
            continue
        text = ctx.text(decl)
        if text is None:
            bad.append("%s:0: declaring README missing" % decl)
            continue
        marker = sections.get(decl)
        start = text.find(marker) if marker else -1
        if start < 0:
            bad.append("%s:0: the frozen declaration paragraph (%s) is gone"
                       % (decl, marker))
            continue
        end = text.find("\n\n", start)
        para = text[start:end if end > 0 else len(text)]
        if token not in para:
            bad.append("%s:0: frozen paragraph no longer names %s (looked for %s)"
                       % (decl, path, token))
    return Result(FAIL if bad else OK,
                  "%d frozen files, %d undeclared" % (len(ctx.data["frozen"]), len(bad)),
                  bad)


@check("footer", "fast", "every non-index doc page ends with its back-link footer")
def check_footer(ctx):
    """The contract's navigation fabric has a floor: every topic page and tool
    page ends with `← Back to the [... index](README.md)`. The footer may span
    lines (page 11 and roadmap 14 carry `· related:` trailers), so the rule is
    on the LAST PARAGRAPH, not the last line: nothing may follow it -- roadmap
    12 once had its "Continued in [16]" pointer placed after the footer, which
    reads as a footer but is not one.
    """
    roots = tuple(ctx.data["footer_roots"])
    marker = ctx.data["footer_marker"]
    bad, scanned = [], 0
    for rel in ctx.md:
        if not rel.startswith(roots) or os.path.basename(rel) == "README.md":
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        scanned += 1
        paras = [p for p in re.split(r"\n\s*\n", text.rstrip()) if p.strip()]
        last = paras[-1].lstrip() if paras else ""
        if not last.startswith(marker):
            bad.append("%s:0: last paragraph is not the footer (starts %r)"
                       % (rel, last[:40]))
    return Result(FAIL if bad else OK,
                  "%d pages, %d without footer" % (scanned, len(bad)), bad)


@check("frozen-growth", "fast", "frozen files do not grow past their recorded line count")
def check_frozen_growth(ctx):
    """The freeze is the one rule `sizes` deliberately does not watch (frozen
    files are skipped there), and the 2026-09-11 docs screening found two
    multi-line record appends that slipped through. A frozen file may still
    take the contract's ONE-LINE status/pointer edits, which change its line
    count -- so this is the same "bump the number in the same commit" pattern
    as `size_baseline`, not a hard no-growth rule that would be disabled on
    its first legitimate use. The diff on `frozen[].lines` is the review.
    """
    bad, n = [], 0
    for entry in ctx.data["frozen"]:
        path, limit = entry["path"], entry.get("lines")
        text = ctx.text(path)
        if text is None or limit is None:
            continue
        n += 1
        got = len(text.splitlines())
        if got > limit:
            bad.append("%s:0: frozen file grew -- %d lines > recorded %d (a one-line "
                       "edit bumps `lines` in scripts/check_data.json in the same "
                       "commit; anything larger goes to a -continued file)"
                       % (path, got, limit))
    return Result(FAIL if bad else OK,
                  "%d frozen files, %d grew" % (n, len(bad)), bad)


@check("heading-status", "fast", "no heading carries a status word or a date")
def check_heading_status(ctx):
    """Anchors derive from heading text, so a heading like
    `## §D standalone raymarch app — DONE (2026-06-14)` breaks every inbound
    link the day its status changes. The contract puts status on the first
    body line for exactly this reason. Existing offenders are grandfathered
    per file in `heading_status_baseline`; the count may only go down.
    """
    words = ctx.data["status_words"]
    pat = re.compile(r"\b(%s)\b|20\d\d-\d\d-\d\d" % "|".join(words))
    baseline = {k: v for k, v in ctx.data["heading_status_baseline"].items()
                if not k.startswith("_")}
    scope = tuple(ctx.data["size_scope"])
    bad, scanned, offending = [], 0, 0
    for rel in ctx.md:
        if not rel.startswith(scope):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        scanned += 1
        hits = []
        for i, line in enumerate(strip_fences(text).split("\n"), 1):
            m = HEADING_RE.match(line)
            if m and pat.search(m.group(2)):
                hits.append(i)
        offending += len(hits)
        allowed = baseline.get(rel, 0)
        if len(hits) > allowed:
            bad.append("%s:%d: %d heading(s) carry status/date, baseline allows %d"
                       % (rel, hits[-1], len(hits), allowed))
    return Result(FAIL if bad else OK,
                  "%d files, %d grandfathered headings, %d files over baseline"
                  % (scanned, offending, len(bad)), bad)


@check("status-vocab", "fast", "Status cells in index tables use the legend's words")
def check_status_vocab(ctx):
    """The roadmap legend is DONE / NEXT / PLANNED / DEFERRED / DROPPED (+ the
    PARTIAL qualifier). The screening found "OPEN", "open", "IN PROGRESS" and
    empty cells in the two index tables that are supposed to be the snapshot.
    Applies to every markdown table with a `Status` column in the listed files:
    `status_tables` (which `status-sync` reads too) plus the
    `status_vocab_only` tables -- the decisions index, whose rows land on
    record anchors rather than status-bearing headings.
    """
    words = ctx.data["status_words"]
    pat = re.compile(r"\b(%s)\b" % "|".join(words))
    bad, cells = [], 0
    extra = ctx.data.get("status_vocab_only", {}).get("tables", [])
    for rel in list(ctx.data["status_tables"]) + list(extra):
        text = ctx.text(rel)
        if text is None:
            bad.append("%s:0: missing" % rel)
            continue
        col = None
        for i, line in enumerate(strip_fences(text).split("\n"), 1):
            if not line.lstrip().startswith("|"):
                col = None
                continue
            parts = table_cells(line)
            if col is None:
                if "Status" in parts:
                    col = parts.index("Status")
                continue
            if all(re.fullmatch(r":?-+:?", c) for c in parts if c):
                continue
            if col >= len(parts):
                continue
            cells += 1
            if not pat.search(parts[col]):
                bad.append("%s:%d: Status cell has no legend word -> %r"
                           % (rel, i, parts[col][:60]))
    return Result(FAIL if bad else OK,
                  "%d status cells, %d off-legend" % (cells, len(bad)), bad)


@check("index-staleness", "fast", "topic files committed after their index README (report only)")
def check_index_staleness(ctx):
    """`indexes` proves a row EXISTS; nothing can prove its status cell is true.
    Staleness is the automatable proxy: a topic file whose last commit is newer
    than its index README's is where "index says one thing, file moved on"
    lives (roadmap 03/11 carried an export bug as "still open" three months
    after 09 recorded the fix). Report-only: a count sync or typo fix in a
    topic file is legitimate without an index touch. The session-end sweep
    reads this list.
    """
    # One log walk, newest first; the first time a path appears is its last
    # commit. ~50 per-file `git log -1` calls cost 1.8 s on the ~1 s tier.
    last = {}
    rc, out = run(["git", "-C", ctx.root, "log", "--format=%ct", "--name-only",
                   "--", "docs/"])
    if rc != 0:
        return Result(FAIL, "git log failed", [])
    stamp = 0
    for line in out.splitlines():
        line = line.strip()
        if line.isdigit():
            stamp = int(line)
        elif line and line not in last:
            last[line] = stamp

    def last_commit(rel):
        return last.get(rel, 0)

    notes = []
    for root in ctx.data["index_roots"]:
        readme_t = last_commit("%s/README.md" % root)
        d = os.path.join(ctx.root, root)
        for name in sorted(os.listdir(d)):
            full = os.path.join(d, name)
            if os.path.isdir(full):
                rel = "%s/%s/README.md" % (root, name)
                if not os.path.exists(os.path.join(full, "README.md")):
                    continue
            elif name.endswith(".md") and name != "README.md":
                rel = "%s/%s" % (root, name)
            else:
                continue
            if last_commit(rel) > readme_t:
                notes.append("%s: committed after %s/README.md -- re-read its index row"
                             % (rel, root))
    return Result(INFO if notes else OK,
                  "%d topic files newer than their index" % len(notes), notes)


@check("claude-md", "fast", "the root entry files point at the status snapshot instead of copying it")
def check_claude_md(ctx):
    """CLAUDE.md is the first file every session reads and lives outside
    `size_scope`, so it was the repo's highest-drift surface: its "Current
    focus" paragraph was three months behind docs/roadmap/README.md and quoted
    a primitive count no other file agreed with. The fix is structural -- it
    links, it does not restate -- and this keeps it that way: no test/parity
    counts, no version numbers, no item ranges, no primitive counts.

    Since roadmap 19 Phase 1 the entry file is AGENTS.md and CLAUDE.md is the
    one-line `@AGENTS.md` import, so the scan covers every file listed in
    `root_entry_files` -- moving the prose would otherwise move it out of the
    check's reach. This check is about restated FACTS; the entry file's shape
    (line cap, dates, `NN/NN` pointers) is `root-entry`'s.
    """
    patterns, bad = ctx.data["claude_md_forbidden"], []
    for rel in ctx.data["root_entry_files"]:
        if rel.startswith("_"):
            continue
        text = ctx.text(rel)
        if text is None:
            bad.append("%s:0: missing" % rel)
            continue
        # Match on whitespace-normalised text, not line by line: the original
        # offender was "~40" at the end of one line and "analytic primitives"
        # at the start of the next, which a per-line scan can never see. The
        # line number is recovered by counting words up to the match.
        body = strip_code(text)
        flat = re.sub(r"\s+", " ", body)
        starts = [m.start() for m in re.finditer(r"\S+", body)]
        for label, rx in patterns.items():
            for m in re.finditer(rx, flat):
                n = len(flat[:m.start()].split())
                line = body[:starts[min(n, len(starts) - 1)]].count("\n") + 1
                bad.append("%s:%d: restates a %s (link to docs/roadmap/README.md instead) -> %s"
                           % (rel, line, label, m.group(0)[:70]))
    files = [k for k in ctx.data["root_entry_files"] if not k.startswith("_")]
    return Result(FAIL if bad else OK,
                  "%d files, %d patterns, %d problems"
                  % (len(files), len(patterns), len(bad)), bad)


RECIPE_CMD_RE = re.compile(r"^\s*(?:\.[\\/])?(dualc_[a-z_]+)(?:\.exe)?\b(.*)$")


def recipe_flags(text):
    """Every (tool, flag, line) a page's recipes use: a command line is a line
    that starts with `dualc_<tool>` (an optional `./`, `.\\` or `.exe` around it)
    inside a fence or in a table cell, with `\\`-continuations joined and `#`
    comments dropped. Flags are `--word` tokens on that line only.
    """
    out, lines = [], text.split("\n")
    i = 0
    while i < len(lines):
        line, n = lines[i], i + 1
        while line.rstrip().endswith("\\") and i + 1 < len(lines):
            i += 1
            line = line.rstrip()[:-1] + " " + lines[i]
        i += 1
        if line.lstrip().startswith("#"):
            continue
        cells = table_cells(line, "`") if line.lstrip().startswith("|") else [line]
        for cell in cells:
            m = RECIPE_CMD_RE.match(cell)
            if not m:
                continue
            args = m.group(2).split(" #")[0]
            for flag in re.findall(r"(?<![\w-])(--[a-z][a-z0-9-]*)", args):
                out.append((m.group(1), flag, n))
    return out


def table_flags(text):
    """The flags a page's tables document: a table cell that BEGINS with a
    backticked `--flag` (a flag-table row, or a decorator/operator row whose
    second cell is the flag). A flag merely mentioned inside a cell's prose,
    or inside a recipe cell, documents nothing."""
    found = set()
    for line in strip_fences(text).split("\n"):
        if not line.lstrip().startswith("|"):
            continue
        for cell in table_cells(line):
            m = re.match(r"\s*`(--[a-z][a-z0-9-]*)", cell)
            if m:
                found.add(m.group(1))
    return found


@check("flag-table", "fast",
       "every parser flag of a tool is on its page, and every recipe flag is in a table")
def check_flag_table(ctx):
    """The contract says "every flag is in the table with its real default", and
    the 2026-09-11 screening found page 05 with no flag table at all and three
    others missing `--collapse`. The parser is the truth: every `"--flag"`
    literal in `examples/<tool>.cpp` must be mentioned on
    `docs/command_reference/NN-<tool>.md` (the page slug IS the binary name).
    Cheap and one-directional -- it cannot tell a wrong default from a right
    one, only a flag that is not there at all.

    The second direction (roadmap 19 Phase 6): every `--flag` a recipe uses is
    documented in a table of the page that owns the binary the recipe names --
    a recipe on page 09 that runs `dualc_demo --sharp` is held to page 01's
    tables, and a post-op flag of `dualc_primitive` to page 02's decorator
    tables. A recipe flag the tool's parser does not know is reported too.
    """
    exempt = set(ctx.data["flag_table_exempt"])
    bad, pages, flags = [], 0, 0
    d = os.path.join(ctx.root, "docs", "command_reference")
    page_of, texts, srcs = {}, {}, {}
    for name in sorted(os.listdir(d)):
        # A tool page is `NN-tool.md`, or -- after a subfolder promotion -- the
        # folder `NN-tool/`, whose pages together must mention every flag.
        m = re.match(r"^\d\d-([a-z_]+)(\.md)?$", name)
        if name.endswith(".md"):
            texts["docs/command_reference/%s" % name] = ctx.text("docs/command_reference/%s" % name)
        elif os.path.isdir(os.path.join(d, name)):
            for f in sorted(os.listdir(os.path.join(d, name))):
                if f.endswith(".md"):
                    rel = "docs/command_reference/%s/%s" % (name, f)
                    texts[rel] = ctx.text(rel)
        if not m:
            continue
        tool = m.group(1)
        src = ctx.text("examples/%s.cpp" % tool)
        if m.group(2):
            page = texts.get("docs/command_reference/%s" % name)
        else:
            page = "\n".join(t or "" for rel, t in texts.items()
                             if rel.startswith("docs/command_reference/%s/" % name))
        if src is None or page is None:
            continue
        pages += 1
        page_of[tool], srcs[tool] = name, src
        for flag in sorted(set(re.findall(r'"(--[a-z][a-z0-9-]*)', src))):
            if flag in exempt:
                continue
            flags += 1
            if flag not in page:
                bad.append("docs/command_reference/%s:0: examples/%s.cpp parses `%s` "
                           "but the page never mentions it" % (name, tool, flag))
    # Post-op flags are parsed by the shared registry, not by dualc_primitive.cpp.
    common = ctx.text("examples/example_common.cpp") or ""
    known = {t: set(re.findall(r'"(--[a-z][a-z0-9-]*)', s)) for t, s in srcs.items()}
    if "dualc_primitive" in known:
        known["dualc_primitive"] |= set(re.findall(r'"(--[a-z][a-z0-9-]*)', common))
    documented = {}
    for tool, name in page_of.items():
        documented[tool] = set()
        for rel, t in texts.items():
            if t and (rel == "docs/command_reference/%s" % name
                      or rel.startswith("docs/command_reference/%s/" % name)):
                documented[tool] |= table_flags(t)
    recipes = 0
    for rel, t in sorted(texts.items()):
        if t is None:
            continue
        for tool, flag, line in recipe_flags(t):
            if tool not in page_of or flag in exempt:
                continue
            recipes += 1
            if flag not in known.get(tool, set()):
                bad.append("%s:%d: recipe passes `%s` to %s, which its parser does not know"
                           % (rel, line, flag, tool))
            elif flag not in documented[tool]:
                bad.append("%s:%d: recipe passes `%s` to %s, but no table on %s documents it"
                           % (rel, line, flag, tool, page_of[tool]))
    return Result(FAIL if bad else OK,
                  "%d pages, %d flags, %d recipe flags, %d problems"
                  % (pages, flags, recipes, len(bad)), bad)


@check("structure", "fast", "every tracked file is named in STRUCTURE.md")
def check_structure(ctx):
    text = ctx.text("STRUCTURE.md")
    if text is None:
        return Result(FAIL, "STRUCTURE.md missing", [])
    names = set(re.findall(r"[A-Za-z0-9_.+-]+", text)) | set(expand_braces(text))
    summarized = tuple(ctx.data["structure_summarized_prefixes"].keys())
    ignore = set(ctx.data["structure_ignore"])
    bad, scanned = [], 0
    for rel in ctx.all_files:
        if rel in ignore or rel.startswith(summarized):
            continue
        scanned += 1
        if os.path.basename(rel) not in names:
            bad.append("STRUCTURE.md:0: %s is tracked but not documented" % rel)
    return Result(FAIL if bad else OK,
                  "%d tracked files, %d undocumented" % (scanned, len(bad)), bad)


@check("vendoring", "fast", "vendored files are attributed and their licenses exist")
def check_vendoring(ctx):
    """Makes the permissive-only vendoring rule mechanical rather than a comment.

    Two directions: nothing appears under a third-party root without an entry in
    THIRD_PARTY.md, and no path THIRD_PARTY.md cites has gone stale.
    """
    text = ctx.text("THIRD_PARTY.md")
    if text is None:
        return Result(FAIL, "THIRD_PARTY.md missing", [])
    roots = tuple(ctx.data["third_party_roots"])
    # THIRD_PARTY.md uses the same `miniz.{h,c}` brace shorthand STRUCTURE.md does.
    named = set(expand_braces(text))
    bad, vendored = [], 0
    for rel in ctx.all_files:
        if not rel.startswith(roots):
            continue
        vendored += 1
        if os.path.basename(rel) not in text and os.path.basename(rel) not in named:
            bad.append("THIRD_PARTY.md:0: %s is vendored but not attributed" % rel)
    cited = 0
    for m in re.finditer(r"`([A-Za-z0-9_./+-]+/[A-Za-z0-9_./+-]+\.[A-Za-z0-9]+)`", text):
        path = m.group(1)
        if path.startswith(("http", "<")):
            continue
        cited += 1
        if not ctx.exists(path):
            line = text[:m.start()].count("\n") + 1
            bad.append("THIRD_PARTY.md:%d: cites a path that does not exist -> %s"
                       % (line, path))
    return Result(FAIL if bad else OK,
                  "%d vendored files, %d cited paths, %d problems"
                  % (vendored, cited, len(bad)), bad)


@check("ordering", "fast", "ledger item headings stay in ascending order")
def check_ordering(ctx):
    """#35 once landed above #34 and had to be moved by hand. This is that check.

    `<=`, not `<`: a duplicated `## #NN` heading is the other way the ledger
    can lie, and until 2026-09-11 uniqueness was not checked at all.

    Only the `## #NN` item headings inside the ledger's item files are ordered.
    The topic README deliberately lists its files by theme, not numerically, and
    is not checked.
    """
    bad, items = [], 0
    for rel in ctx.data["ordered_item_files"]:
        text = ctx.text(rel)
        if text is None:
            bad.append("%s:0: missing" % rel)
            continue
        prev, prev_line = None, 0
        for i, line in enumerate(strip_code(text).split("\n"), 1):
            m = re.match(r"^##\s+#(\d+)\b", line)
            if not m:
                continue
            n = int(m.group(1))
            items += 1
            if prev is not None and n <= prev:
                bad.append("%s:%d: #%d appears after #%d (line %d)"
                           % (rel, i, n, prev, prev_line))
            prev, prev_line = n, i
    return Result(FAIL if bad else OK,
                  "%d ledger items, %d out of order" % (items, len(bad)), bad)


@check("scripts", "fast", "every script in scripts/ parses")
def check_scripts(ctx):
    bad, n = [], 0
    for rel in ctx.all_files:
        if not (rel.startswith("scripts/") and rel.endswith(".py")):
            continue
        n += 1
        text = ctx.text(rel)
        if text is None:
            bad.append("%s:0: tracked but not on disk" % rel)
            continue
        try:
            ast.parse(text, filename=rel)
        except SyntaxError as e:
            bad.append("%s:%s: %s" % (rel, e.lineno, e.msg))
    return Result(FAIL if bad else OK,
                  "%d scripts, %d broken" % (n, len(bad)), bad)


# A zero literal compared through Catch's `Approx`, with no `.margin(` after
# it. Approx's default tolerance is relative (epsilon * |target|), so against
# 0.0 it is exact equality (roadmap 17 #32, finding C42). `\s` spans lines, so
# a comparison split across two lines is still seen.
UNGUARDED_APPROX_ZERO = re.compile(
    r"\bApprox\(\s*[-+]?(?:0+(?:\.0*)?|\.0+)(?:[eE][-+]?\d+)?[fFlL]?\s*\)(?!\s*\.\s*margin\s*\()")


def unguarded_approx_zero(ctx):
    """(rel, line) of every unguarded `Approx(0.0)` in tests/."""
    hits = []
    for rel in ctx.all_files:
        if rel.startswith("tests/") and rel.endswith((".cpp", ".h", ".hpp")):
            text = ctx.text(rel)
            for m in UNGUARDED_APPROX_ZERO.finditer(text or ""):
                hits.append((rel, text[:m.start()].count("\n") + 1))
    return hits


@check("approx-zero", "fast", "no `Approx(0.0)` in tests/ without a `.margin(`")
def check_approx_zero(ctx):
    """The ratchet #32 brought to 0 on 2026-10-07 (38 sites fixed).

    Was a report-only metric that would not gate, so that a legitimate new
    Approx(0.0) is not blocked. There is no legitimate one: against zero the
    relative epsilon is exact equality, which `== 0.0` says more honestly, and
    a tolerance is `.margin(...)`. Gating it is what keeps the 0.
    """
    hits = unguarded_approx_zero(ctx)
    bad = ["%s:%d: `Approx(0)` without `.margin(...)` is exact equality" % h
           for h in hits]
    return Result(FAIL if bad else OK,
                  "%d unguarded `Approx(0.0)` in tests/" % len(bad), bad)


# --------------------------------------------------------------------------
# Fast tier, second batch -- the checks of roadmap 19 Phase 2 (plan § 4).
# Each turns one clause the 2026-09-17 screenings found violated into a line
# of output; the report-only ones exist so the session-end sweep reads a list
# instead of remembering a checklist.
# --------------------------------------------------------------------------

def in_scope(ctx, rel, scope=None):
    """Docs-contract scope: `size_scope` minus the immutable `frozen_prefixes`."""
    scope = tuple(scope or ctx.data["size_scope"])
    return rel.startswith(scope) and not rel.startswith(tuple(ctx.data["frozen_prefixes"]))


def headings(text):
    """(line, level, text) for every heading outside fences."""
    out = []
    for i, line in enumerate(strip_fences(text).split("\n"), 1):
        m = HEADING_RE.match(line)
        if m:
            out.append((i, len(m.group(1)), m.group(2).strip()))
    return out


def first_body_line(text, heading_line):
    """The first non-blank line after a heading (1-based line numbers), or ""."""
    lines = strip_fences(text).split("\n")
    for line in lines[heading_line:]:
        if line.strip():
            return line.strip()
    return ""


@check("drift-pending", "fast", "every DRIFT-PENDING line is listed (fails under --strict)",
       report_only=True)
def check_drift_pending(ctx):
    """`DRIFT-PENDING: <what is owed>` is the contract's marker for
    documentation debt the record admits to (a fact that could not be
    reconstructed from git). It is grep-able by design; this prints the grep so
    a session cannot end without seeing it. `--strict` -- the session-end mode
    -- turns a non-empty list into a failure.
    """
    notes = []
    for rel in ctx.md:
        if not in_scope(ctx, rel):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        for i, line in enumerate(strip_code(text).split("\n"), 1):
            if "DRIFT-PENDING:" in line:
                notes.append("%s:%d: %s" % (rel, i, line.strip()[:90]))
    return Result(INFO if notes else OK, "%d DRIFT-PENDING lines" % len(notes), notes)


@check("status-sync", "fast", "an index Status cell agrees with the entry it links to")
def check_status_sync(ctx):
    """`status-vocab` proves the cell uses a legend word; this proves the word
    is the one the linked entry itself carries. For every row of a
    `status_tables` table whose link carries an anchor, the first body line
    under that heading must contain the cell's first legend word. Rows that
    link a whole file cannot be checked (a topic's status is compound) and are
    counted as unverifiable -- Phase 5 of roadmap 19 adds anchors to the
    folder-README page tables so more rows land on a heading.
    """
    words = ctx.data["status_words"]
    pat = re.compile(r"\b(%s)\b" % "|".join(words))
    bad, checked, unverifiable = [], 0, 0
    for rel in ctx.data["status_tables"]:
        text = ctx.text(rel)
        if text is None:
            bad.append("%s:0: missing" % rel)
            continue
        base = os.path.dirname(rel)
        col = None
        for i, line in enumerate(strip_fences(text).split("\n"), 1):
            if not line.lstrip().startswith("|"):
                col = None
                continue
            parts = table_cells(line)
            if col is None:
                if "Status" in parts:
                    col = parts.index("Status")
                continue
            if all(re.fullmatch(r":?-+:?", c) for c in parts if c) or col >= len(parts):
                continue
            want = pat.search(parts[col])
            links = [m.group(1) for m in LINK_RE.finditer(line) if "#" in m.group(1)]
            if not want or not links:
                unverifiable += 1
                continue
            path, anchor = links[0].split("#", 1)
            tgt = os.path.normpath(os.path.join(base, path)).replace(os.sep, "/")
            ttext = ctx.text(tgt)
            if ttext is None:
                continue          # the links check reports it
            hit = None
            for hline, _, htext in headings(ttext):
                if slugify(htext) == anchor:
                    hit = hline
                    break
            if hit is None:
                continue          # the anchors check reports it
            checked += 1
            body = first_body_line(ttext, hit)
            if not re.search(r"\b%s\b" % want.group(1), body):
                bad.append("%s:%d: cell says %s, but %s#%s opens with %r"
                           % (rel, i, want.group(1), path, anchor, body[:50]))
    return Result(FAIL if bad else OK,
                  "%d anchored rows checked, %d unverifiable, %d disagree"
                  % (checked, unverifiable, len(bad)), bad)


def doc_lag_verdict(code, docs, ahead, cfg):
    """(is_lagging, message) from the newest code commit time, the newest docs
    commit time, and the number of code commits since the newest docs commit.
    Pure so `--selftest` can exercise it without a git history."""
    if not code or not docs:
        return False, "no history to compare"
    days = (code - docs) / 86400.0
    if ahead > cfg["max_commits"] or days > cfg["max_days"]:
        return True, ("code is %d commits / %.1f days ahead of docs/ (limits %d / %d)"
                      % (ahead, days, cfg["max_commits"], cfg["max_days"]))
    return False, "code %d commits / %.1f days ahead of docs/" % (ahead, max(days, 0.0))


@check("doc-lag", "fast", "code commits are not running ahead of docs/ (fails under --strict)",
       report_only=True)
def check_doc_lag(ctx):
    """The last-documented-commit-vs-HEAD proxy. Newest commit touching the
    code trees vs newest commit touching `docs/`, and how many code commits
    have landed since the docs last moved. Report-only, because a code-only
    burst is legitimate mid-feature; `--strict` at session end makes it a
    failure, which is when the record is owed.
    """
    cfg = ctx.data["doc_lag"]
    code_paths = cfg["code_paths"]
    rc1, code_out = run(["git", "-C", ctx.root, "log", "-1", "--format=%ct", "--"] + code_paths)
    rc2, docs_out = run(["git", "-C", ctx.root, "log", "-1", "--format=%ct", "--", "docs/"])
    if rc1 != 0 or rc2 != 0:
        return Result(FAIL, "git log failed", [])
    code_t = int(code_out.strip() or 0)
    docs_t = int(docs_out.strip() or 0)
    rc3, ahead_out = run(["git", "-C", ctx.root, "log", "--format=%H",
                          "--since=%d" % docs_t, "--"] + code_paths)
    ahead = len([l for l in ahead_out.splitlines() if l.strip()]) if rc3 == 0 else 0
    lagging, msg = doc_lag_verdict(code_t, docs_t, ahead, cfg)
    return Result(INFO if lagging else OK, msg,
                  ["docs/:0: the newest code commit has no docs commit after it -- "
                   "record it, or the roadmap is fiction"] if lagging else [])


@check("root-entry", "fast", "the entry file stays short and pointer-shaped")
def check_root_entry(ctx):
    """AGENTS.md is read in full at the start of every session, so every line
    costs attention; and it is the one file that must never restate what an
    index owns. Structural rules only -- the line cap from `root_entry_files`
    and the `root_entry_forbidden` patterns (digits before a slash, i.e. an
    `NN/NN` count or a `17/09` pointer; a version; an item range; an ISO
    date). What counts as a restated FACT is `claude-md`'s business.
    """
    patterns, bad = ctx.data["root_entry_forbidden"], []
    for rel, max_lines in ctx.data["root_entry_files"].items():
        if rel.startswith("_"):
            continue
        text = ctx.text(rel)
        if text is None:
            bad.append("%s:0: missing" % rel)
            continue
        n_lines = len(text.splitlines())
        if max_lines and n_lines > max_lines:
            bad.append("%s:%d: %d lines, the entry file is capped at %d"
                       % (rel, n_lines, n_lines, max_lines))
        for i, line in enumerate(strip_code(text).split("\n"), 1):
            for label, rx in patterns.items():
                m = re.search(rx, line)
                if m:
                    bad.append("%s:%d: %s -> %s" % (rel, i, label, m.group(0)[:40]))
    return Result(FAIL if bad else OK,
                  "%d files, %d patterns, %d problems"
                  % (len([k for k in ctx.data["root_entry_files"] if not k.startswith("_")]),
                     len(patterns), len(bad)), bad)


@check("decisions-index", "fast", "every DROPPED/DEFERRED entry has its decisions row")
def check_decisions_index(ctx):
    """A decision buried in a topic file's chronology is invisible from
    anywhere else -- the next agent proposes it again. Every entry in
    `docs/roadmap/` whose first body line opens with **DROPPED** or
    **DEFERRED** (or whose grandfathered heading carries the word) must have a
    row in `docs/decisions/README.md` linking `file#that-heading`, and every
    DEFERRED row must carry a trigger (non-empty text after the word). Several
    rows may cite one heading (D-14/D-23 both cite 11/03 § 5a), so the rows
    are kept per (file, anchor) as a list and each DEFERRED row is checked on
    its own -- keyed as a plain dict, the last row hid the others. SKIPs until
    Phase 4 of roadmap 19 writes the index.
    """
    cfg = ctx.data["decisions_index"]
    index = ctx.text(cfg["index"])
    if index is None:
        return Result(SKIP, "%s not written yet (roadmap 19 Phase 4)" % cfg["index"])
    idx_dir = os.path.dirname(cfg["index"])
    rows, nrows, bad = {}, 0, []
    for i, line in enumerate(strip_fences(index).split("\n"), 1):
        if not line.lstrip().startswith("|"):
            continue
        cells = table_cells(line)
        status = next((c for c in cells if "DEFERRED" in c), "")
        if status and not re.sub(r"\W*DEFERRED\W*", "", status).strip():
            bad.append("%s:%d: DEFERRED row names no trigger" % (cfg["index"], i))
        keyed = False
        for m in LINK_RE.finditer(line):
            if "#" not in m.group(1):
                continue
            path, anchor = m.group(1).split("#", 1)
            tgt = os.path.normpath(os.path.join(idx_dir, path)).replace(os.sep, "/")
            rows.setdefault((tgt, anchor), []).append(i)
            keyed = True
        nrows += keyed
    entries = 0
    for rel in ctx.md:
        if not rel.startswith(cfg["scope"]):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        for hline, _, htext in headings(text):
            body = first_body_line(text, hline)
            m = re.match(r"\*\*(DROPPED|DEFERRED)\b", body) \
                or re.search(r"\b(DROPPED|DEFERRED)\b", htext)
            if not m:
                continue
            entries += 1
            key = (rel, slugify(htext))
            if key not in rows:
                bad.append("%s:%d: %s entry has no row in %s"
                           % (rel, hline, m.group(1), cfg["index"]))
    return Result(FAIL if bad else OK,
                  "%d decision entries, %d rows, %d problems" % (entries, nrows, len(bad)),
                  bad)


@check("design-no-history", "fast", "the compiled design layer carries no history")
def check_design_no_history(ctx):
    """`docs/design/` is the layer that is rewritten freely to describe the
    present; the moment it says "since", "used to" or a date it has started
    accreting a changelog, which is the roadmap's job. SKIPs until Phase 3 of
    roadmap 19 creates the folder.
    """
    cfg = ctx.data["design_no_history"]
    scope = cfg["scope"]
    if not os.path.isdir(os.path.join(ctx.root, scope)):
        return Result(SKIP, "%s not created yet (roadmap 19 Phase 3)" % scope)
    pats = [(label, re.compile(rx, re.I)) for label, rx in cfg["patterns"].items()]
    bad, scanned = [], 0
    for rel in ctx.md:
        if not rel.startswith(scope):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        scanned += 1
        for i, line in enumerate(strip_code(text).split("\n"), 1):
            for label, rx in pats:
                m = rx.search(line)
                if m:
                    bad.append("%s:%d: %s -> %r" % (rel, i, label, m.group(0)))
    return Result(FAIL if bad else OK,
                  "%d design pages, %d history markers" % (scanned, len(bad)), bad)


@check("heading-hierarchy", "fast", "no ### directly under #, no heading repeating the H1")
def check_heading_hierarchy(ctx):
    """Split residue: a page cut out of a bigger file keeps its `###` levels
    under a new `#`, and often repeats the parent's title as its second
    heading. Existing offenders are grandfathered per file in
    `heading_hierarchy_baseline`; the count may only go down (Phases 5 and 6).
    """
    baseline = {k: v for k, v in ctx.data["heading_hierarchy_baseline"].items()
                if not k.startswith("_")}
    bad, scanned, grandfathered = [], 0, 0
    for rel in ctx.md:
        if not in_scope(ctx, rel):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        scanned += 1
        hits, prev, h1 = [], None, None
        for line, level, htext in headings(text):
            if level == 1 and h1 is None:
                h1 = htext
            elif h1 is not None and htext == h1:
                hits.append("%s:%d: heading repeats the H1" % (rel, line))
            if prev == 1 and level == 3:
                hits.append("%s:%d: ### directly under #" % (rel, line))
            prev = level
        allowed = baseline.get(rel, 0)
        grandfathered += min(len(hits), allowed)
        if len(hits) > allowed:
            bad.extend(hits[allowed:] if allowed else hits)
    return Result(FAIL if bad else OK,
                  "%d files, %d grandfathered, %d new" % (scanned, grandfathered, len(bad)),
                  bad)


@check("stale-paths", "fast", "no prose names NN-slug.md where NN-slug/ is now a folder")
def check_stale_paths(ctx):
    """When a page is promoted to a same-numbered folder, every link is
    retargeted (the `links` check sees to that) but the link TEXT and the prose
    around it still say `11-dualc_field.md`. Inline code is ignored: a
    backticked old name is a historical literal (17/10's split table), a bare
    one is a stale pointer. Grandfathered per file in `stale_paths_baseline`;
    counts only go down.
    """
    folders = set()
    for rel in ctx.md:
        parts = rel.split("/")
        if len(parts) >= 2 and parts[-1] == "README.md" \
                and re.match(r"^\d\d-[A-Za-z0-9_-]+$", parts[-2]):
            folders.add(parts[-2])
    baseline = {k: v for k, v in ctx.data["stale_paths_baseline"].items()
                if not k.startswith("_")}
    needles = ["%s.md" % f for f in sorted(folders)]
    bad, scanned, grandfathered = [], 0, 0
    for rel in ctx.md:
        if not in_scope(ctx, rel):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        scanned += 1
        hits = []
        for i, line in enumerate(strip_code(text).split("\n"), 1):
            for n in needles:
                if n in line:
                    hits.append("%s:%d: names %s, which is a folder now" % (rel, i, n))
        allowed = baseline.get(rel, 0)
        grandfathered += min(len(hits), allowed)
        if len(hits) > allowed:
            bad.extend(hits[allowed:] if allowed else hits)
    return Result(FAIL if bad else OK,
                  "%d folders, %d files, %d grandfathered, %d new"
                  % (len(folders), scanned, grandfathered, len(bad)), bad)


@check("directional", "fast", "\"above\"/\"below\" next to a link into another file (report only)",
       report_only=True)
def check_directional(ctx):
    """"See the table above" survives a split with the table now in another
    file. Report-only: the words are sometimes right (the link is a courtesy),
    so the list is read, not enforced.
    """
    words = re.compile(r"\b(%s)\b" % "|".join(ctx.data["directional_words"]), re.I)
    notes = []
    for rel in ctx.md:
        if not in_scope(ctx, rel):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        for i, line in enumerate(strip_code(text).split("\n"), 1):
            for m in LINK_RE.finditer(line):
                if m.group(1).startswith("#") or ".md" not in m.group(1):
                    continue
                near = " ".join(line[:m.start()].split()[-3:] + line[m.end():].split()[:3])
                if words.search(near):
                    notes.append("%s:%d: %r beside a link to %s"
                                 % (rel, i, words.search(near).group(0), m.group(1)))
    return Result(INFO if notes else OK, "%d directional links" % len(notes), notes)


@check("usage-in-record", "fast", "no flag/default table inside docs/roadmap/ (report only)",
       report_only=True)
def check_usage_in_record(ctx):
    """A flag table in the record is a second copy of the usage contract that
    the `flag-table` check does not read and that drifts the day a default
    changes. Report-only: the roadmap may legitimately quote a table as
    evidence of what a change was.
    """
    scope = ctx.data["usage_in_record"]["scope"]
    heads = set(ctx.data["usage_in_record"]["headers"])
    notes = []
    for rel in ctx.md:
        if not rel.startswith(scope) or not in_scope(ctx, rel):
            continue
        text = ctx.text(rel)
        if text is None:
            continue
        lines = strip_fences(text).split("\n")
        for i, line in enumerate(lines):
            if not line.lstrip().startswith("|") or (i and lines[i - 1].lstrip().startswith("|")):
                continue
            if i + 1 >= len(lines) or not re.match(r"^\s*\|?\s*:?-+", lines[i + 1]):
                continue
            cells = set(table_cells(line, "*`"))
            if cells & heads:
                notes.append("%s:%d: table with columns %s -- link the reference page instead"
                             % (rel, i + 1, sorted(cells & heads)))
    return Result(INFO if notes else OK, "%d usage tables in the record" % len(notes), notes)


# --------------------------------------------------------------------------
# Build tier
# --------------------------------------------------------------------------

def build_dir(ctx):
    return ctx.args.build_dir


def generator_args(config):
    """The one per-platform choice of the build tier (roadmap 20 #49).

    Windows keeps the multi-config Visual Studio generator the gate was born
    on. Elsewhere it is Ninja when on PATH, else CMake's default (Unix
    Makefiles); both are single-config, so the build type is fixed here at
    configure and `--config` / `-C` are inert afterwards.
    """
    if os.name == "nt":
        return ["-G", "Visual Studio 17 2022", "-A", "x64"]
    gen = ["-G", "Ninja"] if shutil.which("ninja") else []
    return gen + ["-DCMAKE_BUILD_TYPE=%s" % config]


@check("configure", "build", "cmake configure (reuses the build dir unless --clean)")
def check_configure(ctx):
    bd = build_dir(ctx)
    # geometry-central needs no probe: CMakeLists.txt fetches the pinned upstream
    # commit unless -DDUALC_GC_DIR names a local tree (roadmap 20 #47).
    # -D KEY=VALUE (roadmap 17 #33) is how CI turns on DUALC_WERROR and
    # DUALC_SANITIZE: an existing build dir is re-configured in place with them
    # rather than skipped, so a define is never silently dropped.
    defines = ["-D" + d for d in ctx.args.define]
    cache = os.path.join(ctx.root, bd, "CMakeCache.txt")
    if os.path.exists(cache) and not ctx.args.clean and not defines:
        return Result(SKIP, "%s already configured (use --clean to redo)" % bd)
    gen = ([] if os.path.exists(cache) and not ctx.args.clean
           else generator_args(ctx.args.config))
    cmd = (["cmake", "-S", ".", "-B", bd] + gen +
           ["-DDUALC_BUILD_TESTS=ON", "-DDUALC_BUILD_EXAMPLES=ON"] + defines)
    rc, out = run(cmd, cwd=ctx.root)
    if rc == 127:
        return Result(SKIP, "cmake not on PATH")
    if rc != 0:
        lines = [l for l in out.splitlines() if l.strip()]
        # The `CMake Error` blocks first: stderr follows stdout here, and the
        # warnings of the fetched projects would otherwise push the error out
        # of the first ten lines CI's annotations keep (roadmap 17 #33).
        errs, inside = [], False
        for l in lines:
            if l.startswith("CMake Error"):
                inside = True
            elif not l[:1].isspace():
                inside = False       # a block's body is indented
            if inside:
                errs.append(l)
        tail = errs[:10] + [l for l in lines[-15:] if l not in errs]
        note = ("a clean configure fetches geometry-central and Catch2 from GitHub "
                "by pinned SHA (and Eigen via geometry-central) -- pass "
                "-DDUALC_GC_DIR and set FETCHCONTENT_SOURCE_DIR_CATCH2 to work offline")
        return Result(FAIL, "cmake configure failed", tail + [note])
    return Result(OK, "configured %s%s" % (bd, " " + " ".join(defines) if defines else ""))


@check("build", "build", "cmake --build --config Release")
def check_build(ctx):
    bd = build_dir(ctx)
    if not os.path.exists(os.path.join(ctx.root, bd, "CMakeCache.txt")):
        return Result(SKIP, "%s is not configured" % bd)
    # --parallel: Unix Makefiles would otherwise build serially (Ninja and
    # MSBuild-with-VS already pick their own job count).
    cmd = ["cmake", "--build", bd, "--config", ctx.args.config]
    if os.name != "nt":
        cmd.append("--parallel")
    rc, out = run(cmd, cwd=ctx.root)
    if rc == 127:
        return Result(SKIP, "cmake not on PATH")
    logdir = os.path.join(ctx.root, bd, "_check")
    try:
        os.makedirs(logdir, exist_ok=True)
        with open(os.path.join(logdir, "build.log"), "w",
                  encoding="utf-8", errors="replace") as f:
            f.write(out)
    except OSError:
        pass
    ctx.build_log = out
    if rc != 0:
        # MSVC prints `: error C1234:`, GCC/Clang `: error:`.
        errs = [l.strip() for l in out.splitlines()
                if ": error " in l or ": error:" in l][:20]
        return Result(FAIL, "build failed", errs or ["see %s/_check/build.log" % bd])
    return Result(OK, "built (%s), log in %s/_check/build.log" % (ctx.args.config, bd))


WARN_RE = re.compile(r"^\s*(.+?)\((\d+)[,)].*?:\s*warning\s+([A-Za-z]+\d+)\s*:\s*(.*)$")
GNU_WARN_RE = re.compile(r"^\s*(.+?):(\d+):\d+:\s*warning:\s*(.*)$")
# One compiled TU per line: MSBuild echoes the bare source name indented by two
# spaces; Ninja and Makefiles print a `Building CXX/C object ...` progress line.
TU_RE = re.compile(r"^\s{2}\S+\.(?:cpp|cc|cxx|c)\s*$"
                   r"|^\[\s*\d+(?:%|/\d+)\]\s+Building C(?:XX)? object ")


@check("warnings", "build", "no DualC-origin compiler warnings in the build log")
def check_warnings(ctx):
    """Mechanises the "no warnings from our own code" claim that was checked by eye.

    Deliberately a log filter rather than /WX locally: a hard gate would break
    on a compiler bump or a geometry-central header change, and this reproduces
    exactly the check that was being done manually. CI's build matrix adds the
    hard gate on top with `-D DUALC_WERROR=ON` (roadmap 17 #33).

    Scope: `/W4 /permissive-` (MSVC) or `-Wall -Wextra -Wpedantic` (GCC/Clang)
    is applied by dualc_target_options() (cmake/DualCWarnings.cmake) to every
    DualC target -- the library, tests, examples and the C ABI -- and to no
    vendored or fetched code (roadmap 17 #33, 2026-10-06).
    """
    log = getattr(ctx, "build_log", None)
    if log is None:
        return Result(SKIP, "no build log (run without --only/--skip on build)")
    tus = sum(1 for l in log.split("\n") if TU_RE.match(l))
    if tus == 0:
        # An incremental build recompiles nothing and prints nothing, so a
        # scan here would be a green light over an empty input -- worse than no
        # check at all.
        return Result(SKIP, "build compiled 0 translation units (incremental) "
                            "-- use --clean for a meaningful scan")
    exclude = ("/_deps/", "\\_deps\\", "examples/third_party/",
               "examples\\third_party\\", "src/internal/third_party/",
               "src\\internal\\third_party\\", "geometry-central")
    found, seen = [], set()
    for line in log.split("\n"):
        m = WARN_RE.match(line) or GNU_WARN_RE.match(line)
        if not m:
            continue
        path = m.group(1)
        # Match the whole line, not just the header path: MSBuild appends the
        # owning [project.vcxproj], which is what places a warning raised
        # inside an STL header (xmemory, ...) with the third-party TU that
        # instantiated it rather than with DualC.
        if any(x in line for x in exclude):
            continue
        key = (path, m.group(2))
        if key in seen:
            continue
        seen.add(key)
        found.append(line.strip())
    return Result(FAIL if found else OK,
                  "%d TUs compiled, %d DualC-origin warnings" % (tus, len(found)),
                  found[:20])


@check("ctest", "build", "ctest, serial (never -j)")
def check_ctest(ctx):
    """Serial on purpose.

    Until 2026-10-07 the reason was correctness: every Catch case shared one
    WORKING_DIRECTORY and tests/test_field_graph.cpp wrote a fixed temp
    filename (finding C43, roadmap 17 #32). Both are gone -- each case runs in
    a directory of its own (tests/test_main.cpp) and `ctest -j 8` passes. The
    reason now is cost against benefit: `-j 8` saves about a third (86 s ->
    57 s on 8 cores), because the engine already parallelises inside each
    case, and oversubscribing the cores is what exposes the timing-dependent
    cancel/progress tests (17/14 findings 8 and 9). `ctest -j` is safe to run
    by hand.

    The total is reported, never asserted: it legitimately grows as tests are
    added (231 on 2026-09-01, 253 today). Contrast the parity stage, where the
    count IS asserted because its failure mode is silent skipping.
    """
    bd = build_dir(ctx)
    if not os.path.exists(os.path.join(ctx.root, bd, "CMakeCache.txt")):
        return Result(SKIP, "%s is not configured" % bd)
    rc, out = run(["ctest", "--test-dir", bd, "-C", ctx.args.config,
                   "--output-on-failure"], cwd=ctx.root)
    if rc == 127:
        return Result(SKIP, "ctest not on PATH")
    # CTest 3 prints "100% tests passed, 0 tests failed out of N"; CTest 4
    # drops the failed clause when nothing failed.
    m = re.search(r"(\d+)% tests passed(?:, (\d+) tests failed)? out of (\d+)", out)
    if not m:
        return Result(FAIL, "could not parse ctest output",
                      out.splitlines()[-15:])
    failed, total = int(m.group(2) or 0), int(m.group(3))
    if failed:
        names = [l.strip() for l in out.splitlines()
                 if re.search(r"\*\*\*Failed|\(Failed\)", l)][:20]
        return Result(FAIL, "%d of %d tests failed" % (failed, total), names)
    return Result(OK, "%d tests, 0 failed" % total)


# --------------------------------------------------------------------------
# GPU tier -- opt-in locally, required in CI (the `gpu` job)
# --------------------------------------------------------------------------

@check("parity", "gpu", "dualc_glsl_parity: GPU GLSL vs C++ field values")
def check_parity(ctx):
    """The CPU/GPU parity harness (roadmap 17 #31), wrapped so it cannot lie.

    The trap this exists for: the mesh/winding cases resolve `cube.obj` by bare
    filename and SKIP THEMSELVES if it is not found, and the binary still exits
    0. Trusting the exit code would silently accept a 69-case run as a pass. So
    the announced case count is asserted against the expected number.

    It needs a GL context with an R32F-renderable FBO; CI's required `gpu` job
    provides one under Xvfb + Mesa llvmpipe, which is what made #31 binding.
    """
    bd = build_dir(ctx)
    # Multi-config (VS) puts it under examples/<config>/, single-config
    # (Ninja, Makefiles) directly under examples/.
    name = "dualc_glsl_parity" + (".exe" if os.name == "nt" else "")
    exe = next((p for p in (os.path.join(ctx.root, bd, "examples", ctx.args.config, name),
                            os.path.join(ctx.root, bd, "examples", name))
                if os.path.exists(p)), None)
    if exe is None:
        return Result(SKIP, "dualc_glsl_parity not built "
                            "(configure with -DDUALC_BUILD_GLSL_PARITY=ON)")
    # cwd = repo root: findDemoMesh() accepts either the binary's own directory
    # or the repo root (it probes "cube.obj" then "data/cube.obj").
    rc, out = run([exe], cwd=ctx.root)
    expected = ctx.data["parity_expected_cases"]
    head = re.search(r"dualc_glsl_parity:\s*(\d+)\s+cases", out)
    foot = re.search(r":\s*(\d+)/(\d+)\s+passed", out)
    if not head or not foot:
        return Result(FAIL, "could not parse parity output", out.splitlines()[-15:])
    announced, passed, total = int(head.group(1)), int(foot.group(1)), int(foot.group(2))
    if announced != expected:
        return Result(FAIL,
                      "expected %d cases, harness announced %d" % (expected, announced),
                      ["cases silently skip when their demo mesh is not found, and "
                       "the binary still exits 0 -- either the run is incomplete, or "
                       "nodes were added and parity_expected_cases in "
                       "scripts/check_data.json needs updating"])
    if rc != 0 or passed != total:
        fails = [l.strip() for l in out.splitlines() if "FAIL" in l][:20]
        return Result(FAIL, "%d/%d passed" % (passed, total), fails)
    return Result(OK, "%d/%d passed" % (passed, total))


# --------------------------------------------------------------------------
# Report-only metrics (never affect the exit code)
# --------------------------------------------------------------------------

def print_metrics(ctx):
    """Counters the #32 coverage ledger cites, made countable.

    The `Approx(0.0)` count is also the `approx-zero` check, which gates it at
    0 since 2026-10-07. Until then the needle matched `== Approx(0.0)` whether
    or not a `.margin(` followed, so it read 70 while the unguarded count was
    38: it also counted the guarded uses, and missed a comparison split across
    two lines. It now counts what the check counts.
    """
    approx = len(unguarded_approx_zero(ctx))
    # C43's fixed temp filename was `temp_directory_path() / "dualc_fg_box.obj"`,
    # shared by every build tree on the machine. The first version of this
    # needle looked for "dualc_fieldgraph_test", a string that never existed
    # in tests/, and reported "no" while the finding stayed true -- the docs
    # screening of 2026-09-11 caught it. Match the construct, not a guessed
    # name, in every test source.
    fixed_tmp = [rel for rel in ctx.all_files
                 if rel.startswith("tests/") and rel.endswith(".cpp")
                 and re.search(r'temp_directory_path\(\)\s*/\s*"[^"]+"',
                               ctx.text(rel) or "")]
    print("")
    print("metrics (roadmap 17 #32, report only):")
    print("  unguarded `Approx(0.0)` in tests/ : %d" % approx)
    print("  fixed temp filename under temp_directory_path() : %s"
          % (", ".join(fixed_tmp) if fixed_tmp else "none"))


# --------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------

def hook_notice(root):
    """core.hooksPath is local config and does not survive a clone."""
    rc, out = run(["git", "-C", root, "config", "--get", "core.hooksPath"])
    if rc != 0 or out.strip().replace("\\", "/") != "scripts/hooks":
        print("note: hook not installed -- run: "
              "git config core.hooksPath scripts/hooks")


def selftest(root, args):
    """The gate's own test: every check with a fixture tree under
    scripts/check_fixtures/<check-id>/{pass,fail}/ is run against both, and
    must answer OK on `pass` and FAIL (INFO for a report-only check) on
    `fail`. A fixture directory is a miniature repo (walked, not `git
    ls-files`); a `data.json` inside it overrides keys of check_data.json.
    Registered with CTest as `check_selftest` so a broken check is caught by
    the build tier, not discovered when it lets something through.
    """
    fx = os.path.join(root, "scripts", "check_fixtures")
    try:
        with open(os.path.join(root, "scripts", "check_data.json"), encoding="utf-8") as f:
            base = json.load(f)
    except (OSError, ValueError) as e:
        die("cannot read scripts/check_data.json: %s" % e)
    by_id = {c.cid: c for c in CHECKS}
    failures, n = [], 0
    for cid in sorted(os.listdir(fx)):
        c = by_id.get(cid)
        if c is None:
            failures.append("%s: fixture for a check that does not exist" % cid)
            continue
        for kind in ("pass", "fail"):
            d = os.path.join(fx, cid, kind)
            if not os.path.isdir(d):
                failures.append("%s/%s: fixture missing" % (cid, kind))
                continue
            data = json.loads(json.dumps(base))
            override = read_text(d, "data.json")
            if override is not None:
                data.update(json.loads(override))
            n += 1
            try:
                r = c(Ctx(d, data, args, walk=True))
            except Exception as e:
                failures.append("%s/%s: raised %s: %s" % (cid, kind, type(e).__name__, e))
                continue
            expect = OK if kind == "pass" else (INFO if c.report_only else FAIL)
            mark = "ok  " if r.status == expect else "BAD "
            print("  %s %-18s %-4s -> %-4s %s" % (mark, cid, kind, r.status, r.summary))
            if r.status != expect:
                failures.append("%s/%s: expected %s, got %s -- %s"
                                % (cid, kind, expect, r.status, "; ".join(r.details[:3])))
    # doc-lag reads git, which a fixture tree does not have: its verdict is a
    # pure function, tested here directly.
    cfg = {"max_commits": 5, "max_days": 7}
    cases = [((1000, 900, 2, cfg), False), ((10 * 86400 + 100, 100, 1, cfg), True),
             ((1000, 900, 6, cfg), True), ((0, 0, 0, cfg), False)]
    for argv_, want in cases:
        n += 1
        got = doc_lag_verdict(*argv_)[0]
        print("  %s %-18s %-4s -> %s" % ("ok  " if got == want else "BAD ", "doc-lag",
                                          "fail" if want else "pass", got))
        if got != want:
            failures.append("doc-lag: verdict%r expected %s" % (argv_, want))
    print("")
    for f in failures:
        print("  " + f)
    print("%d fixture runs, %d wrong" % (n, len(failures)))
    print("FAIL" if failures else "PASS")
    return 1 if failures else 0


def hook_main(argv, args):
    """Claude Code `Stop` hook. Runs the docs tier in a child so its report is
    captured whole; a PASS prints nothing (the hook's stdout is only visible in
    transcript mode), a FAIL puts the report on stderr and exits 2 so the
    report reaches the agent as the thing to fix before it stops. If the hook
    is already active (`stop_hook_active` on stdin) exit 1 instead -- a
    non-blocking error shown to the user -- so an unfixable failure cannot
    loop the agent forever.
    """
    active = False
    try:
        payload = json.loads(sys.stdin.read() or "{}")
        active = bool(payload.get("stop_hook_active"))
    except (ValueError, OSError):
        pass
    passthrough = [a for a in argv if a != "--hook"] or ["--docs"]
    rc, out = run([sys.executable, os.path.abspath(__file__)] + passthrough)
    if rc == 0:
        return 0
    sys.stderr.write("scripts/check.py %s failed -- fix the report before stopping:\n"
                     % " ".join(passthrough))
    sys.stderr.write(out)
    return 1 if active else 2


def main(argv):
    ap = argparse.ArgumentParser(
        description="DualC checks gate: the one command, locally and in hosted CI "
                    "(.github/workflows/gate.yml runs exactly this).")
    ap.add_argument("--fast", action="store_true",
                    help="docs + repo hygiene only, no build (~1s)")
    ap.add_argument("--docs", action="store_true", help="alias for --fast")
    ap.add_argument("--build", action="store_true",
                    help="configure + build + warning scan + ctest")
    ap.add_argument("--gpu", action="store_true",
                    help="opt-in: dualc_glsl_parity (needs a GL context)")
    ap.add_argument("--all", action="store_true", help="--fast --build --gpu")
    ap.add_argument("--clean", action="store_true",
                    help="re-configure from scratch (needs network for Catch2)")
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--config", default="Release")
    ap.add_argument("-D", "--define", action="append", default=[], metavar="KEY=VALUE",
                    help="extra CMake cache entry for the configure step, repeatable "
                         "(CI: -D DUALC_WERROR=ON, -D DUALC_SANITIZE=address,undefined)")
    ap.add_argument("--only", default="", help="comma-separated check ids")
    ap.add_argument("--skip", default="", help="comma-separated check ids")
    ap.add_argument("--metrics", action="store_true",
                    help="print the #32 report-only counters")
    ap.add_argument("--strict", action="store_true",
                    help="session-end mode: SKIP and report-only INFO count as failure")
    ap.add_argument("--hook", action="store_true",
                    help="Claude Code Stop-hook mode: silent on PASS, report on stderr "
                         "and exit 2 on FAIL (exit 1 if the hook is already active)")
    ap.add_argument("--selftest", action="store_true",
                    help="run every check against scripts/check_fixtures/ and exit")
    ap.add_argument("--list", action="store_true",
                    help="list every check and exit")
    args = ap.parse_args(argv)

    if args.list:
        for c in CHECKS:
            print("  %-18s %-6s %s%s" % (c.cid, c.tier, c.purpose,
                                         " [report-only]" if c.report_only else ""))
        return 0
    if args.selftest:
        return selftest(repo_root(), args)
    if args.hook:
        return hook_main(argv, args)

    tiers = set()
    if args.all:
        tiers = {"fast", "build", "gpu"}
    else:
        if args.fast or args.docs:
            tiers.add("fast")
        if args.build:
            tiers.add("build")
        if args.gpu:
            tiers.add("gpu")
        if not tiers:
            tiers = {"fast", "build"}      # the default gate

    root = repo_root()
    try:
        with open(os.path.join(root, "scripts", "check_data.json"),
                  encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError) as e:
        die("cannot read scripts/check_data.json: %s" % e)

    only = {s for s in args.only.split(",") if s}
    skip = {s for s in args.skip.split(",") if s}
    selected = [c for c in CHECKS
                if c.tier in tiers and (not only or c.cid in only)
                and c.cid not in skip]
    if not selected:
        die("no checks selected")

    ctx = Ctx(root, data, args)
    counts = {OK: 0, FAIL: 0, SKIP: 0}
    details = []
    for c in selected:
        t0 = time.time()
        try:
            r = c(ctx)
        except Exception as e:                       # a broken gate is not a red commit
            print("check.py: %s raised %s: %s" % (c.cid, type(e).__name__, e),
                  file=sys.stderr)
            return 2
        dt = time.time() - t0
        if r.status == INFO and c.report_only and args.strict:
            r.status = FAIL           # session-end mode: the list must be empty
        counts[r.status] = counts.get(r.status, 0) + 1
        print("[%-4s] %-18s %-55s %5.2fs" % (r.status, c.cid, r.summary, dt))
        if r.details:
            details.append((c.cid, r.status, r.details))

    for cid, status, lines in details:
        if status not in (FAIL, INFO):
            continue
        # INFO details are printed too: the report-only checks exist so the
        # session-end sweep reads a list, and a list nobody prints is a checklist.
        print("")
        print("--- %s ---" % cid)
        for line in lines[:20]:
            print("  %s" % line)
        if len(lines) > 20:
            print("  ... and %d more" % (len(lines) - 20))

    if args.metrics:
        print_metrics(ctx)

    print("")
    print("%d checks: %d passed, %d failed, %d skipped"
          % (len(selected), counts[OK], counts[FAIL], counts[SKIP]))
    failed = counts[FAIL] or (args.strict and counts[SKIP])
    if "fast" in tiers and not failed:
        hook_notice(root)
    print("FAIL" if failed else "PASS")
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.exit(2)
