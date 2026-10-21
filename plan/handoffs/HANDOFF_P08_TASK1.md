# Handoff — P08.1: harvest, lint, loss audit, final gates, PR text for the owner

Unit P08.1 of [`plan/17_P08_close.md`](../17_P08_close.md) § T1, the only unit of P08 and the
last of the run. Session of 2026-10-21, branch `feat/hygiene-and-editor`, start commit
`5bad79d`. Nothing pushed, no remote touched, `main` untouched, `~/Documents/DualCEditor`
only read (its `docs/roadmap/06-dualc-questions.md` and `tests/data/meshes/cube.obj`, for
Q7). The record is the new
[17/22](../../docs/roadmap/17-code-audit-and-hardening/22-hygiene-plan-run.md); this handoff
adds the commands, the audit triage and the text the owner needs for the push and the PR.

## Outcome in one paragraph

The run is closed on the local branch, ready for the owner's push and PR. Every handoff
P01.1 … P07.2 was read and its cross-unit facts routed: 17/22 holds the units, the counts
the run moved, the handoff-only measurements, how the run went, the plan-file corrections,
**every in-run choice in one table for the owner's review**, what the run taught and what
it left open. The roadmap README's Current focus, Track 1 and milestones window are current
(D-43 kept at five rows). `/docs-semantic-lint` ran as delta run 09 (2 findings: one fixed,
one the owner's). The editor's Q7 was reproduced and its cause found (upstream
geometry-central, negative OBJ face indices). The loss audit of `plan/` and of the touched
pages is triaged below; the final gates are green (§ Gates).

## What changed, and why

1. **`docs/roadmap/17-code-audit-and-hardening/22-hygiene-plan-run.md`** (new): the harvest,
   modelled on 21/09. Indexed in 17's README; to fit the 15,360-byte cap the 15 index entry
   was condensed (its batch contents are 15's own headings).
2. **`docs/roadmap/README.md`**: Current focus rewritten around the closed run (block 21's
   paragraph shortened to its outcome, "the PR is the owner's" → merged 2026-10-19);
   Track 1 item 1 "merged 2026-10-19", item 2's #53 clause names the gate and links 17/22;
   a 2026-10-21 milestone row in, the 2026-10-15 #57 row out (its record is 21/04); the
   block-21 milestone row's "the branch awaits its PR" → "its PR merged 2026-10-19
   (`5ce5b1c`)".
3. **Block 21**: a dated append at the end of 21/09 (*2026-10-19, merged*, sourced to the
   raw plan's § 2) and 21's README row 09 "merged 2026-10-19 (`5ce5b1c`)" — the stale "PR is
   the owner's" P07.1 and P07.2 flagged.
4. **Semantic lint run 09**
   ([19/08/09](../../docs/roadmap/19-docs-layers/08-semantic-lint/09-2026-10-21-after-hygiene-run.md),
   row in 19/08's README): (a) design/10's central-difference row claimed an absolute 1e-4
   step for *every* FD gradient; `WindingNumberField::trueGradientAt` steps 1e-4 × its mesh
   diagonal (`winding_field.cpp:55`) — **fixed** in design/10 (one resolution-rule sentence
   shortened to stay at the cap, 15,360 / 15,360); (e) #11's trigger ("work needing
   before/after numbers") met by #34 and #35's scratch benchmarks — handed to the owner via
   17/22 § Open after the run.
5. **No status flip needed for #34 / #35 / #53**: P04b.1, P06.2 and P05.1 had already set
   17's table, 17/04, 14/06, 14's README and the roadmap README with their evidence; checked
   by reading and by the docs tier's `status-sync`. #53 stays PARTIAL.

**Chosen in-run (pending owner review)** — also in 17/22's in-run table:
- the record as 17's next child (22), with P07's choices linked to block 22 rather than
  restated;
- one milestone row for the whole run, rather than one per item; it pushes #57's row out of
  the window;
- Q7 recorded on 17/22's open table, no `#N` opened: the bug is in the pinned upstream
  reader, outside DualC's code, and the fix (a pre-pass in the host's mesh loader, an
  upstream report, or a re-pin) is the owner's choice.

## The editor's Q7 — reproduced

`dualc_field g.json --depth 4 -o out.obj` with `g.json` = `{"op":"mesh","path":"edcube.obj"}`
and `edcube.obj` a copy of DualCEditor's `tests/data/meshes/cube.obj`:
`field-graph error: cannot read mesh 'edcube.obj': basic_string::substr: __pos (which is
18446744073709551615) > this->size() (which is 0)`, exit 2. Bisected by dropping lines: the
file loads without its last face `f -4 -3 -2 -1`, and a file holding only that face fails;
the comment, the `o` line, `vn` and the `v//vn`, `v/vt`, `v/vt/vn` faces all load. Cause:
`build/_deps/geometry-central-src/src/surface/simple_polygon_mesh.cpp`, the `f` branch: a
negative parsed index is taken for a line continuation (`getline` of the next line, then
`line.substr(line.find_first_not_of(…))`), which throws at end of file. Relative indices are
unsupported by the pinned reader. Recorded on 17/22.

## Loss audit

One run, background with a 2-hour limit (8 min): `python3 scripts/docs_loss_audit.py --base
5bad79d --head <scratch> --scope docs/ plan/ --annex plan/ --triage /tmp/p08/triage.json
--untriaged --write-dump /tmp/p08/audit-untriaged.txt`. The head (`8ad7fff`) is a
`git commit-tree` of a separate index: the staged close with `plan/` removed (`git rm -r
--cached plan` under `GIT_INDEX_FILE=/tmp/p08/index`), so `plan/`'s whole text is audited
against the final docs. The triage is keyed `5bad79d` with the `3d73220` thresholds and no
rules; `scripts/docs_loss_audit.json` is unchanged. Exit 1, as always with no triage rules:
11,911 untriaged in all.

- **Docs side** (sentence 3,211, evidence 5,682, heading 178, identifier 977 — the known
  ~10,050 baseline). Rows on the pages this unit changed, all intentional:
  - roadmap README :21 ×3 *gone* — the old Current-focus sentences on block 21 (its opening,
    the branch, #55–#60): their homes are 21's README, D-55 and 21/09. The fourth, "through
    the hosted-CI plan, run closed 2026-10-07 (20/04)", was **restored** after the audit.
  - roadmap README :40 *rewritten* (the block-21 milestone, "awaits its PR" → merged), :44
    *gone* (the #57 row, out of the window by D-43; its record is 21/04), :60 *partial*
    (Track 1 item 1, "the PR is the owner's" → merged).
  - 17 README :36 *gone* — the condensed 15 entry; its contents are 15's own headings.
  - design/10 :91 *gone* (the resolution-rule sentence, re-worded for the cap: "Each recipe's
    command-reference page gives its depth"), :176 *partial* (the lint's fix).
  - 21 README :43 *partial* — row 09's status, merged.
  - Every other row on these pages sits on an untouched line (17 README :46–:147, 19/08
    README's index table, 21/09's run table and footer): the noise class.
- **The annex, `plan/`**: sentence 1,101, evidence 515 (158 gone, 294 token-only, 16
  unverifiable), heading 129, identifier 118. Read by class:
  - *Sentences, headings*: the handoffs were paraphrased into the records, not copied.
  - *Evidence gone*: page byte counts on a given day, audit row hashes, untriaged counts,
    commit hashes, P06.1's single runs (their medians are on 21) and P04.1's wall times (on
    20). Carried to 17/22 because no page had them: the red run's step 111/111, the
    sanitized configure's 85 s. Checked present: 41.7 / 45.1 s, 3.39 GB, 9.6e-4, 1.7e-3,
    TEXTREL, 1,038 s, 1.94 copies, 200.9 M, 293,640, 0.01316, 125.2 / 59.4 s, 3,452 lines,
    48 MB, 4.9e-n, the ±1/20 section step (22, in words).
  - *Evidence token-only*: every token (0.0129, 1.30×, 82/82, 2676c6a, …) is on a page in
    another context.
  - *Identifiers*: `/tmp` paths and scratch binaries, line ranges already in 22, the audit
    recipe, the compare link (this handoff and the PR). Carried to 17/22: the variants'
    `P04B_V` / `P04B_R` selectors, the evaluator's link line, `dualc_tests "[e12]"`.
- By hand, `git diff -U0 5bad79d -- docs/ | grep '^-'`: the same lines as the docs side above,
  nothing else.

**0 untriaged rows are a loss.** The dump is not kept (as 21/09 and 20/04).

## Gates

All on Linux (Ubuntu), GCC 15, Ninja, Release, i7-920 (4 c / 8 t).

- **Clean gate**: `cmake --build build --target clean` (282 files), then
  `python3 scripts/check.py --clean`: 32 checks, 31 passed, 0 failed, 0 skipped;
  configure 1 s, build 248 s, **254 TUs compiled, 0 DualC-origin warnings**, ctest
  **369 / 369** in 152 s. Log `/tmp/p08/gate-clean.log`. (It ran while the docs were still
  being edited; the final gate below re-ran the docs tier on the committed content.)
- **GPU tier**: `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict
  --build-dir build-gl`: parity **82 / 82**, PASS. Log `/tmp/p08/gpu.log`. (Rebuilt first;
  no field code moved in this unit, so it is the run's closing proof, not this unit's.)
- **Final gate on the final tree**: `python3 scripts/check.py` with every edit staged, just
  before the commit: 32 checks, 29 passed, 0 failed, 2 skipped (configure already configured;
  warnings incremental, 0 TUs — the clean run above is the warnings proof), ctest
  **369 / 369** in 120 s, exit 0. Log `/tmp/p08/gate-final.log`. Only this handoff's
  final-gate line changed after it, re-checked by `--docs --strict`.
- **`python3 scripts/check.py --docs --strict`**: 28 checks, 27 passed, `index-staleness`
  INFO (report-only, the usual list), PASS — re-run after the last edit.

## Exit criteria — evidence, bullet by bullet

- **"The run is closed on the local branch, ready for the owner's push and PR."**
  - *Closed*: every handoff harvested into 17/22 (units, counts, handoff-only measurements,
    plan corrections, every in-run choice in one table, lessons, open items); status
    snapshot current (roadmap README Current focus, Track 1, milestones window five rows,
    D-43); #34 PARTIAL / #35 DONE / #53 PARTIAL in 17's table, 17/04, 14/06, 14 README and
    the roadmap README, `status-sync` 0 disagree.
  - *Lint*: run 09 recorded in 19/08 (2 findings, 1 fixed, 1 handed to the owner).
  - *Loss audit*: § Loss audit, 0 untriaged losses.
  - *Gates green*: § Gates — clean gate 369/369 with 0 warnings on 254 TUs, parity 82/82,
    `--docs --strict` PASS, and the final `check.py`.
  - *Local, committed, not pushed*: `git status` clean after the commit; no `git push` was
    run, no remote added or changed; the branch has no upstream.
  - *Ready for the push and PR*: § For the owner — the push command, the compare link, the
    CI jobs to read for #53, the PR description with the ABI version, the counts, the
    decisions and the in-run choices.
- **T1 acceptance**: "`plan/`'s facts are harvested" — 17/22 + § Loss audit; "every local
  gate is green" — § Gates; "the handoff carries the push command, the compare link and the
  PR text" — § For the owner.

## What I got wrong on the way

- My first 17/22 wrote "17's README sat within 7 bytes of 15,360 from P03.2 on"; the
  handoffs give 4 → 24 → 19 → 7 → 1 bytes free across P03.2 … P06.2, so it now says 25.
- The first 17/22 wrote "(above)" beside a link; the `directional` check refused it.
- A first Q7 attempt passed the graph to `--expr` with a quoted path and hit the expression
  parser, not the reader; the JSON graph reproduced it.

## For the owner — the push and the PR

1. **When to push.** After the driver marks P08 done, `plan/` is deleted in one commit
   (D-51, with its loss audit). Push after that commit, so the PR does not carry `plan/`:

   ```bash
   git push -u origin feat/hygiene-and-editor
   ```

2. **Open the PR** (no `gh` here): <https://github.com/Giacogiak/DualC/compare/main...feat/hygiene-and-editor>

3. **CI jobs to read for #53** (`.github/workflows/gate.yml`, run on the push and the PR):
   - `build (ubuntu-24.04)` — **the #53 proof**. Its ctest output (job log or the
     annotations) must list `cli_c_abi`, `cli_c_abi_mesh`, `cli_c_abi_mesh_inmem`,
     `cli_c_abi_cancel` and `cli_c_abi_threads` passed. Green → #53 DONE: flip 14/06's
     status line, 14's README row, the 17 README / roadmap README clauses, and replace 17's
     Latest-verification text with the CI counts (the README is at the cap: replace, do not
     add).
   - `sanitize` — the same five under ASan + UBSan (local run: 367/367 in 30 min, P05).
   - `build (windows-2022)` and `build (macos-14)` — the first hosted builds of the DLL and
     the `.dylib` (P05 in-run choice (a)). If macOS goes red on the `.dylib`: fix it, or add
     a per-OS rule to `GATE_OPTIONS` in `scripts/check.py`.
   - `gpu` — parity; expected 82/82.
   - `docs` — the strict docs tier.
   Expected ctest counts (an expectation from 2026-10-07's offsets, not a measurement):
   369 on Linux and macOS, 371 on Windows.

4. **The PR description**, ready to paste (its links point at the branch, so they resolve
   while it exists):

```markdown
**Hygiene + editor acknowledgement — #34's rest, #35, the gate builds the C ABI (#53), the
standalone UI handed to DualCEditor**

Runs the [hygiene + editor plan](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/raw/2026-10-19-hygiene-and-editor-plan.md) as twelve
unattended `plan-run` units (2026-10-19 … 2026-10-21). The harvest, with every in-run choice
awaiting review in one table, is
[roadmap 17/22](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/roadmap/17-code-audit-and-hardening/22-hygiene-plan-run.md).

**What changed**

- **#34, the API hygiene batch — PARTIAL.**
  - `ContourerParams::numThreads` bounds the contourer's leaf pre-solve; `bakeToGrid`,
    `MeshSource::bakeToGrid`, `FieldGraph::build` and `compileToGlsl` take a thread count;
    `dualc_field --threads N` and `dualc_field_view --threads N` (0 = all, cap 1024). Output
    bit-identical at every count ([17/18](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/roadmap/17-code-audit-and-hardening/18-contourer-threads.md)).
  - The `gradientAt` contract written down — a unit outward direction (D-65) — with the
    30-override census; a new virtual `trueGradientAt` (∇f, magnitude kept) used by
    `normalizedOf` and by a Newton step `q − G·f/|G|²` in `closestSurfacePoint` (gyroid:
    mean |f| after the step 9.6e-4 vs 0.953); `WindingNumberField::gradientAt` is unit;
    one FD helper, `src/internal/fd_gradient.h` ([17/19](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/roadmap/17-code-audit-and-hardening/19-gradient-contract.md)).
  - The `GridField` normal: a continuous node-gradient normal (P04) and limited stencils —
    minmod, ratio switch, crease masks (P04b) — were measured; none kept the sharp CAD
    pair's mean deviation at the cell gradient's, so **the cell gradient stays** and E12 is
    reopened. A crease test was added; the continuity test is kept hidden (`[e12]`)
    ([17/20](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/roadmap/17-code-audit-and-hardening/20-gridfield-normal.md)).
- **#35, `RepeatField` neighbour set — DONE.** A new opt-in virtual `boundsBoundValue()`
  (sphere, box, capsule, round cone, hard union) lets both repeats skip copies that cannot
  win; repeats of one already read are skipped too; `cellOverlaps` asks the home tile first.
  Octet at depth 7: 125.2 s → 60.0 s, 1.30× the single fold (was 2.73×); six reference
  meshes byte-identical, and a field-level test against the old 8-copy loop
  ([17/21](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/roadmap/17-code-audit-and-hardening/21-repeat-pruning.md)).
- **#53 — PARTIAL, the remote run owed.** `scripts/check.py` configures with
  `DUALC_BUILD_C_ABI=ON`, so every gate run and every CI build / sanitize job links
  `libdualc_capi` and runs the five `cli_c_abi*` cases; an existing build dir missing a gate
  option is re-configured in place. The red run without PIC fails the `.so` link as
  predicted ([14/06](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/roadmap/14-c-abi/06-linux-shared-library.md)).
- **Track 2 → DualCEditor (D-66).** Every interactive UI item (section-plane UI, parameter
  edits, the clip mask, the Web build) is indexed with its original intent verbatim in the
  new [block 22](https://github.com/Giacogiak/DualC/blob/feat/hygiene-and-editor/docs/roadmap/22-dualceditor-handoff/README.md); D-16 / D-17 / D-25 name
  the editor. DualC keeps the codegen side (D-29 / D-30 / D-31).
- Docs: semantic lint runs 08 and 09; the roadmap snapshot is current.

**The ABI**: `dualc 0.5.2` (from 0.5.1) — a patch: `DualcContourParams.numThreads` now also
bounds the contourer; no signature or struct changed. Create-time bakes still use every
hardware thread (#62, DEFERRED).

**Counts** (local: Linux, GCC 15, Ninja, Release): ctest 344 → **369 / 369** (C ABI cases
in), also 367 / 367 under ASan + UBSan at P05; GPU parity **82 / 82**; 0 DualC-origin
warnings on a full rebuild.

**Decisions**: D-65 (the `gradientAt` contract) and D-66 (all UI is DualCEditor's), both
settled; #62 DEFERRED (create-time bakes ignore the host's thread count).

**In-run choices awaiting review** (the full table is 17/22 § The in-run choices):

- #34: ship the cell gradient (E12 open) — or minmod (continuous, case A +18 %) or the 40°
  angle mask (A +6 %); the continuity test hidden, not deleted.
- #35: lever (c) not taken at 1.30× (the fold's mesh is not byte-identical);
  `boundsBoundValue` as a public opt-in virtual; home-tile-first `cellOverlaps`.
- #53: the C ABI on every CI job, macOS and Windows included; re-configure in place.
- `--threads` capped at 1024; `MeshSource` keeps the FD `trueGradientAt`; hard booleans,
  mirror and repeats forward `trueGradientAt`.
- The hand-off: block 22 a folder; two old homes keep their record with a pointer; D-16
  stays DEFERRED.

**Open, not blocking**: #53's flip after this PR's CI; #11's trigger met by the run's
scratch benchmarks; the editor's Q7 reproduced — geometry-central's OBJ reader fails on
negative face indices (17/22).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

## Left open

- The in-run choices: 17/22 § The in-run choices, one table.
- 17/22 § Open after the run: #53's remote proof, 17's Latest verification after CI, E12,
  the stencil variants' scratch source (`/tmp/p04b/src/grid_field_variants.cpp`, gone at the
  next reboot — copy it out if a stencil is to be taken), #11's trigger, Q7, the GLSL
  mirror of `repeat`, `MeshSource` under `normalizedOf`, Boletus's re-vendor.
- **`plan/` is not deleted here** (the driver reads the manifest and this handoff). The
  deletion (D-51) is one commit after the driver marks P08 done; its own loss audit only
  needs to cover this handoff, whose facts are on 17/22, 19/08/09 and in the PR text.
- Next free IDs unchanged: **D-67, #63**, roadmap block **23**; block 17's next page **23**;
  the semantic-lint folder's next run **10**.
- Cap room now: 17 README 15,345 / 15,360, design/10 15,360 (none), roadmap README 15,027,
  17/22 15,326 (a further append there means a folder split).
- Scratch, safe to delete: `/tmp/p08/` (triage, scratch index, audit dump, gate logs), and
  the earlier units' `/tmp/p04b/`, `/tmp/p05/`, `/tmp/p06/`, `/tmp/p07/`.
