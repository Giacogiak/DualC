# Record — Phase 8 (the semantic lint and the shrunken skill)

The dated close-of-session entry of [roadmap 19](README.md) Phase 8, the last phase, done
as one session: the plan is
[§ 4 Phase 8](../../raw/2026-09-17-docs-restructuring-plan.md#phase-8--semantic-lint-and-the-shrunken-skill-a9-a10-1-session)
(items 1–3, advice A9 and A10), the hand-downs are the "leaves" lists of
[19/02](02-record-phase-3.md), [19/04](04-record-phase-5/02-session-5b.md),
[19/05](05-record-phase-6.md) and [19/06](06-record-phase-7.md).

**DONE (2026-09-20, commits `677c047`, `8fcafe7` and this record).**

- **Item 1 — the procedure**, `.claude/commands/docs-semantic-lint.md` (4.4 KB, listed in
  `STRUCTURE.md`; the harness offers it as `/docs-semantic-lint`). The five classes as the
  plan names them, each with the command that seeds it and what is *not* a finding; the
  two inputs a run records (last code commit, last design commit); the run-file shape —
  `19/08/NN-<date>-<slug>.md`, date in the filename and first body line, a findings table
  with `file:line` on both sides, a count per class, zero a valid result; and what the
  lint does not do (fix prose, read `raw/`, repeat the gate). **Where the runs live:**
  the same-numbered folder [08-semantic-lint/](08-semantic-lint/README.md), one child per
  run, because monthly runs accumulate and the first one alone is 7.5 KB against a 15 KB
  cap; `index_roots` gained it. The cadence rule was already principle 1 of `AGENTS.md`
  (Phase 1); it now names the command — `root-entry` green, 73 of 80 lines.
- **Item 2 — the first run**, [08/01](08-semantic-lint/01-2026-09-20-first-run.md). All
  twelve design pages read against the code they name. **7 findings: (a) 4, (c) 3, (b)/(d)/(e)
  0**, each zero with the measurement behind it (the last code change is `d217d3a`,
  2026-09-11, so no design page predates it; 5 orphan candidates are 19's own record
  children; 28 DEFERRED triggers read, none met). Six were fixed in the run's commit —
  the two entry-point quotes that predated the `Diagnostics` channel, `design/03` arguing
  with a comment that had been rewritten, `design/07`'s account of which decorator
  forwards `cellOverlaps` how, roadmap 02's v2 section reduced to one sentence + link per
  heading, and its two homeless gotchas given homes (`design/08` § 10.4, `design/10`'s
  resolution rule, the `4^maxDepth` scaling measured on a sphere) — the seventh (01/01
  § 3, the end-of-v2 hand-off) is kept, dated by its own intro. **One code observation
  came out of (a):** `OffsetField` (`offsetOf` / `roundedOf`) does not override
  `cellOverlaps`, so `offset(gyroid(wavelength=0.15),r=0)` at depth 6 loses a third of
  the surface against the bare gyroid (331,918 vs 499,018 faces, 57,072 vs 15,822
  boundary edges); it is a dated note on [17/03 #25](../17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#25-lipschitzbound--the-silent-correctness-opt-out-behind-celloverlaps),
  whose trigger it is the in-tree instance of, and a code session's decision — nothing
  under `src/` changed in this phase.
- **Item 3 — the skill**, proposed with the old and new bodies side by side and written on
  the owner's yes. `~/.claude/skills/repo-docs-lifecycle/` went from **four files,
  28,415 B** (`SKILL.md` 4,712, `doc_conventions.md` 12,059, `session_start.md` 4,106,
  `session_end.md` 7,538) to **two files, 8,516 B** — `SKILL.md` 4,106 (the trigger
  description verbatim; the two moments, four steps each; the fact-class → home table)
  and `references/doc_conventions.md` 4,410 (layout, the anatomies, the status
  vocabulary and the style kept; every rule the gate enforces reduced to a *gate* tag;
  § 1b Legacy containment deleted, the folder-promotion rule in its place). That is 30 %
  of the budget, under #44's ⅓; the two session files are gone, their content being the
  eight steps. The old body is the dated import
  [`raw/2026-09-20-repo-docs-lifecycle-skill-before-phase-8.txt`](../../raw/2026-09-20-repo-docs-lifecycle-skill-before-phase-8.txt)
  (`.txt`, so the markdown checks do not read its skill-relative links), because no
  gate check can see that folder. **The harness loads two copies:** the local source
  (listed as `repo-docs-lifecycle`, the one edited) and the claude.ai-synced bucket
  (`anthropic-skills:repo-docs-lifecycle`, a `custom` upload of 2026-07-10, byte-identical
  before the edit); the second keeps the old body until the owner re-uploads it — the
  owner's action, recorded here so the divergence is not a surprise.
- **The script pass** (`8fcafe7`), handed down since 19/02–19/04: `table_cells()` splits a
  row on unescaped pipes only and undoes the `\|` escape (row D-28), replacing the six
  bare `split("|")` sites; `decisions-index` keeps every row per `(file, anchor)` as a
  list and checks each DEFERRED row for a trigger, and its fail fixture is now the case
  that collapsed before (two rows on one heading, the first without a trigger — the
  previous check passed it, proven by running the old script against the new fixture);
  `status-vocab` reads the two decisions tables through `status_vocab_only` (83 cells,
  0 off-legend) — not through `status_tables`, because `status-sync` reads that list and
  the decisions rows land on record anchors, 35 false disagreements when tried. A
  `status-vocab` fixture pair is new. `--selftest` 24 → 26 runs, 0 wrong; `check_selftest`
  passes under CTest; 17/09 carries the two-line dated note (15,159 B, under the cap by
  201 — the next append splits it).

## What the whole of roadmap 19 leaves

- **The plan's end state holds.** The tree matches § 3 (`AGENTS.md` + `CLAUDE.md` =
  `@AGENTS.md`, `STRUCTURE.md` without status clauses, `design/` 01–11 + README,
  `decisions/` two tables, `roadmap/` with 16 and 18 as tombstones, `command_reference/`
  cleaned, `raw/` with the study pack, the screenings, the plan, the parity run and now
  the skill import); the § 4 size end-state holds — `size_baseline` empty, `docs/raw/` the
  only exempt tree, every maintained page under the cap; the three baselines (`size`,
  `heading_hierarchy`, `stale_paths`) are empty and have stayed empty since Phase 6.
  Items #36–#45 are all DONE; **block 19's status cell becomes DONE.**
- **To ordinary sessions, not to a phase:** the monthly `/docs-semantic-lint` (next due by
  2026-10-20, or after the first session that edits both `src/` and `docs/design/`), with
  its run as `19/08/02-…`; the `OffsetField` observation on 17/03 #25 (the usage side is
  already said: the `offset` row of cmdref 11/01 and the 06 pointer carry the caveat); the merge of
  `structure-docs` into `main` (every commit since 2026-09-17 sits on that branch
  — the owner's decision, after this phase); the re-upload of the revised skill to claude.ai; the `tile-depth >= depth`
  console line, a MINOR-OPEN on 11/03 § 5 for a code session. No (a) finding is left
  unfixed.
- **`index-staleness` INFO entries, all benign:** 11/03, 01/02, 17/09 and ERRATA as after
  Phase 7, plus 17/03 (the #25 note; its 17/README row is unchanged and correct) and this
  record itself, which every correcting commit makes newer than `19/README` (its row
  there is correct), and the three pages the usage-side caveat and the run's exponent
  note touched after their indexes (cmdref 06, cmdref 11/01, 19/08/01 — rows unchanged)
  — 02 and 19/README left the list because the close commit rewrote roadmap/README with
  them.

*Verified at close* (measured after this record, the README rows and the roadmap/README row
landed; corrected in a follow-up if the numbers moved with the commit):
`python scripts/check.py --fast` and `--docs --strict` — 26 checks, 25 OK, 1 INFO
(`index-staleness`, the entries above), **0 failed, `--strict` fully green since Phase 6**;
1,558 links, 0 broken; 549 cross-file + 22 same-file anchors, 0 unresolved; `indexes` 19 roots, 124
siblings, 0 unlinked; 110 files under the cap, 20 exempt; `check.py --selftest` 26 fixture
runs, 0 wrong; `ctest -N` 256. Nothing under `src/`, `include/`, `tests/`, `examples/` or
`capi/` changed.

*(2026-09-20, later the same day: the owner opened
[17 #46](../17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#46-offsetfield-does-not-forward-celloverlaps)
for the `OffsetField` gap; the #25 note became a pointer to it.)*

*(2026-09-21 — the skill's home, settled; three additions.)* Item 3's skill lived only in
`~/.claude/skills/`, and the first reading of principle 1 ("nothing outside the repo")
moved it into `.claude/skills/` as a project skill — verified loadable, then reversed the
same session on the owner's rule, now [D-41](../../decisions/01-settled.md): **the
documentation and every fact about the project live in the repo; the instructions on how
to handle that documentation are the owner's personal workflow and are not published with
it.** So the skill stays in `~/.claude/skills/` (two files, one copy — the claude.ai
re-upload owed above is moot, the synced bucket no longer carries it), `STRUCTURE.md` does
not list it, and what a public reader needs is inline: `AGENTS.md` principle 1 names the
skill as the owner's and spells its four steps each way (75 of 80 lines, `root-entry`
green), so a fresh session finds the procedure from the entry file rather than from a
trigger phrase. Three things the routine relied on but the skill did not say were added,
8,516 → 9,290 B (32.7 % of the old budget, under #44's ⅓): a *Between the two ends*
section — the full `check.py` before a commit in a code session and `--gpu` when codegen or
a field node is touched, since the hooks run only the docs tier; the rule that a `design/`
page rewritten *because the code moved* makes the session a src + design one, so
`/docs-semantic-lint` runs before the next — and a routing row for a new file
(`STRUCTURE.md` + its folder README). The `/docs-semantic-lint` procedure follows the same
rule: Item 1's `.claude/commands/docs-semantic-lint.md` left the repo for the owner's
`~/.claude/commands/`, its `STRUCTURE.md` row with it; what a public reader needs of it —
the five finding classes and that its runs are dated files under `08/` — is on
[08/README](08-semantic-lint/README.md), which now names where the procedure lives.

*(2026-09-21 — merged.)* `structure-docs` fast-forwarded into `main` at `d8bc3aa`, 43
commits over base `3d73220`, `--fast` green on `main` after the merge; the last item the
block left to the owner is closed. The branch is kept.

*(2026-09-21 — D-41 refined: the run folder is the repo's fact.)* Boletus ported the gate
and settled the same boundary as its own D-41
([`D:\Boletus\docs\decisions\01-settled.md` D-41](file:///D:/Boletus/docs/decisions/01-settled.md)),
which left the owner's `~/.claude/commands/docs-semantic-lint.md` naming DualC's run folder
inside a command two repos share. So the command is **one file** that names no path; the
one per-repo fact it needs — where a run is written — is `semantic_lint_runs.folder` of
`scripts/check_data.json` (here `19/08`, also an `index_roots` entry); and the fast check
`semantic-lint-runs`, ported verbatim from Boletus with its fixture pair, keeps that fact
true: the folder and its README exist, the folder is an index root, every run is
`NN-<date>-<slug>.md` with its date on the first body line. The three runs already had the
anatomy (3 runs, 0 problems on the first pass); 26 → 27 checks, `--selftest` 26 → 28 runs,
`check_selftest` green under CTest. [08/README](08-semantic-lint/README.md) says where the
folder is named; [17/09](../17-code-audit-and-hardening/09-local-checks-gate/README.md), the
gate's record, sits 8 bytes under the cap and takes no append — its check tables are dated
snapshots, and the next line it needs splits it into a folder — which is when its check
table gains the `semantic-lint-runs` row it owes. The same holds one notch
worse for [`roadmap/README.md`](../README.md), the status snapshot: 15,359 of 15,360 B at
this session's start, so it took no row and its direct children (15, this session's link
sweep) show under `index-staleness` until it is relieved — that page is an `index_roots`
entry and the one status surface, so its split is a session of its own, not a side effect
of this one.

*(2026-09-21, later: both pages relieved — [10](10-cap-relief.md), D-43; the two "takes no append" clauses above are closed.)*

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
