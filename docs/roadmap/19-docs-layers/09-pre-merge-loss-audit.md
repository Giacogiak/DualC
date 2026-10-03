# Pre-merge loss audit — the base tree against `structure-docs`

Part of the [Docs layers record](README.md) (roadmap 19). A validation of the whole
block, run before the merge to `main`; not a Phase 9 — the phase table is closed.

**DONE (2026-09-20).** Base `3d73220` (`main`, the merge base) against `3c0070f` (the last
content commit of `structure-docs`); the script is
[`scripts/docs_loss_audit.py`](../../../scripts/docs_loss_audit.py), its triage
[`scripts/docs_loss_audit.json`](../../../scripts/docs_loss_audit.json), the full report
[`docs/raw/2026-09-20-loss-audit-3d73220-3c0070f.txt`](../../raw/2026-09-20-loss-audit-3d73220-3c0070f.txt).
Result: **0 untriaged, 0 restorations owed** — every residual resolves to a removal a phase
record names, a correction the record names, or an extractor artefact, each with a citation
the script verifies. Two things the audit changed on the way: the phase 6 record gained a
dated line for three recipe-heading renames it had not stated ([05](05-record-phase-6.md)),
and the audit's own files are excluded from the corpus they would otherwise certify.

## Why a separate audit

The gate proves shape — links, anchors, sizes, index rows, status words — and no phase
record in 19 claims more: every *Verified* block counts links, bytes and `ctest` entries.
None proves that a sentence, a number, a heading or a flag that existed on `main` still
exists on the branch. This audit does, as set arithmetic over two git trees, so that the
same `(base, head, triage)` gives byte-identical output and the same exit code.

## Method

Four unit types are extracted from every file in scope on the base tree and looked for on
the head tree **per file**, never against a concatenation:

| Unit | Extraction | Kept when |
| --- | --- | --- |
| sentence | prose split into sentences; a table row and a fenced line are one unit each; ≥ `MIN_WORDS`=6 words; word-`K_LONG`=8 shingles (a 6–7-word unit has one shingle: kept ⇔ verbatim) | ≥ `KEPT_SCK`=0.6 of its shingles in one head file; else `rewritten` (≥ `REWRITTEN_SC4`=0.6 of its 4-shingles), `partial` (≥ `PARTIAL_SC4`=0.3) or `gone` |
| evidence | ISO dates, 7+-hex hashes, versions, `N,NNN`, `N/N`, number + unit, 4+-digit integers; context = the `CTX_WORDS`=3 nearest content words (a table row: its sibling tokens plus those words) | token in a block of one head file with ≥ `CTX_MIN_HIT`=2 of its context; else `token-only`, `unverifiable-context` (fewer than 2 usable context items — never kept silently), `raw-only`, `gone` |
| heading | anchor slugs by `check.py`'s `slugify`, with GitHub's `-1` suffixes | slug in any live head file |
| identifier | backticked symbols (`f(a, b)` → `f`, `path:N-M` → `path`, renames applied), `--flags` outside link targets, `#N`, `D-NN`, letter IDs | in a live file's identifiers or text; `code-only` counts as kept except for a flag, a `dualc_*` name or a `docs/` path |

The head tree is tiered and the tier is part of the verdict: **a** live docs
(`design/`, `roadmap/`, `command_reference/`, `decisions/`, the root files, `.claude/`);
**b** verbatim homes (`raw/study/`, the parity run); **c** quotation only — the rest of
`raw/` (the screenings *quote* the base tree) and `roadmap/19-docs-layers/` (the records
quote what they removed): a match found only there is `raw-only`, never kept; **code** for
identifiers. `CORPUS_EXCLUDE` removes the audit's own dump, this page and the two script
files — the triage excerpts and the selftest strings had made `T4` "kept" through the code
tier before that guard existed. One normaliser folds both trees (NFKC, typographic
punctuation, the 88-column reflow); the one asymmetric step is the rename map
(`git diff -M`: the study pack, `17/03` → folder, 40 pairs) substituted into base text.

Every residual is keyed by `sha1(unit + normalised text)[:12]` and triaged in the JSON.
Verdicts: `intentional` and `superseded` need a `record` anchor **and** a `quote` of ≥ 4
words, both resolved on the head tree; `restored` needs a commit and the row must be kept
again; `noise` needs a note. A bulk rule needs one source page, a bucket, a `max_matches`
cap and a `category` naming the phase record's removal class; every row a rule absorbs is
printed in the report under that rule. Thresholds are pinned in the JSON; a mismatch voids
the triage (exit 2). Exit 0 ⇔ no untriaged row above the ratchet (`untriaged_max`, now 0),
every citation resolves, no rule over its cap, no stale restoration.

## The run

| Unit | Base units | Kept | Rewritten | Partial | Gone | Other residual | Triaged as |
| --- | --- | --- | --- | --- | --- | --- | --- |
| sentence | 3,935 | 3,212 | 326 | 127 | 270 | — | 720 intentional, 3 superseded |
| evidence | 1,481 | 1,382 | — | — | 4 | 12 raw-only, 53 token-only, 30 unverifiable | 43 intentional, 56 noise |
| heading | 400 | 390 | — | — | 10 | — | 10 intentional |
| identifier | 4,298 | 4,267 | — | — | 23 | 6 raw-only, 2 code-only | 30 intentional, 1 superseded |
| annex `ARCHITECTURE.md` | 755 | 611 | 62 | 27 | 42 | 13 token-only | 143 intentional, 1 superseded |

Scope: `docs/roadmap/`, `docs/command_reference/`, the quality report (now `12/08/`) and
`docs/study/README.md` (an R087 rename) — 77 base files; `ARCHITECTURE.md` as an annex,
its § → page map already being [02](02-record-phase-3.md)'s. **Not audited, by the owner's
choice:** the root `README.md`, `CLAUDE.md` and `STRUCTURE.md` (their rewrite is
[01](01-record-phases-0-2.md)'s record) — the start-up completeness assertion ranges over
`docs/**` only, so "0 untriaged" is a statement about `docs/`. The run takes 8 s.

**What was read.** Every `partial` and `gone` row was listed page by page and reviewed,
and every heading, identifier and evidence residual, before its rule or item was written;
`rewritten` rows (≥ 60 % of their 4-shingles on one page) were absorbed by per-page rules
after the page's other rows had been reviewed — the 21 `rm-rw-*` rules cite the phase's
method sentence, not a per-row category, and their per-row evidence is the 4-gram score
printed under each rule in the report. Where a row looked like a loss, the head tree was
grepped for the fact and the note on the rule says where it is: the oracle algorithms on `design/02`, the
`5.19 M → 519 k` acceptance as `5,189,100 → 518,856` on `12/06`, the enclosed-cavity fix on
`12/07 § H2`, the `x' = 0` collapse defect on `01/02 § 4.9`, the `Visual Studio 17 2022`
build line in `AGENTS.md`'s fence, `Tao Ju 2006` in a heading of `01/README`.

**What was found.**

- **No restoration owed.** No fact on the base tree lacks a home on the branch.
- **Three recipe headings renamed without a record line** — `06`'s "Output mode
  recipes", `11/05`'s "Recipes — decimation", `11/08`'s "Recipes — tiled & welded 3MF",
  all `## Recipes` since Phase 6 item 2. No link targeted the old anchors; the dated
  append on [05](05-record-phase-6.md) states the exception and the headings stand.
- **Five rows are four corrections, not losses** (`superseded`): `dualc_lift --offset`'s
  `0` / `R` row and its prose twin (default `1`, named `O`, 19/05 item 1), the `--tile-depth` "clamped to
  `[2, depth − 1]`" claim (the only hard bound is `D ≥ 2`, `design/10`), and
  `ARCHITECTURE.md`'s "`minDepth > maxDepth` is not an error" (it throws, `design/01`).
- **56 evidence rows (49 distinct tokens) are `noise`**, cleared by a same-file presence check, not read one
  by one: the token still occurs on the head version of its own page (a status-line date,
  a flag value) while its row moved or its neighbours were reworded — `0.25` of
  `11/05:87`, whose row is `25%` on `12/08/01`, is found on `11/05` as a different
  occurrence; the fact survived, the check does not say where.
- **The rest is the phase records' own catalogue**: the ARCHITECTURE.md compile and
  dedupe (Phase 3), the roadmap snapshot, tombstones, restated numbers, dated residue and
  duplicate-to-link moves (5a, 5b, 5c), the command-reference flag tables, recipe IDs,
  rationale-to-record links and one-home facts (Phase 6), the study pack move (Phase 7),
  roadmap 02's v2 reduction (Phase 8).

**Three extractor lessons, each a guard now.** The audit's outputs re-supply every
residual on a rerun (excluded from the corpus; the header prints `excluded=N`); numbers
inside fenced code and headings were invisible on the head side (fenced lines and headings
are blocks on both trees); a numeric table row has no prose neighbours (row context =
sibling tokens + nearest words, and an empty context is its own bucket).

## Rerun

```bash
python scripts/docs_loss_audit.py --base 3d73220 --head <rev>    # exit 0 = clean
python scripts/docs_loss_audit.py --base 3d73220 --untriaged     # what is left, if anything
python scripts/docs_loss_audit.py --selftest                     # the pure functions, no git
```

Against the merge commit the counts must be those of the table (the audit's own files are
excluded, so the commits after `3c0070f` change nothing in scope); a new residual means a
fact moved after the audit and is triaged by id, with the ratchet at 0. The same script
audits any future split: a new key under `audits`, keyed by the base commit.

**Limits.** The audit proves *presence*, not *truth* — that is the semantic lint
([08](08-semantic-lint/README.md)); and a `rewritten` sentence's nuance is scored, not
read — the `gone` and `partial` rows are the ones a human reads, and they were. Reachability
was not re-checked: `links`, `anchors` and `indexes` ran green through every phase.

**Gate at close.** `check.py --fast` 26 checks, 25 OK, `index-staleness` INFO (the entries
[07](07-record-phase-8.md) names); `--docs --strict` PASS; `docs_loss_audit.py --selftest` 30 checks, 0 failed;
`ctest -N` unchanged — nothing under `src/`, `include/`, `tests/`, `examples/` or `capi/`
changed. The gate itself is untouched: this is a script beside `check.py`, not a check in it.

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
