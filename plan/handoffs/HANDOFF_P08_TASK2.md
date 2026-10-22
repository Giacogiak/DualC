# Handoff — P08.2: harvest, statuses, the owner's review list

Unit P08.2 of [`plan/17_P08_gates-and-close.md`](../17_P08_gates-and-close.md) § T2, the last
unit of the run. Session of 2026-10-22 on branch `feat/porous-shell` in
`~/Documents/DualC-porous`. Nothing was pushed, no remote touched; `main`,
`feat/hygiene-and-editor`, `~/Documents/DualC` and Boletus were left alone. **`plan/` is not
deleted** (D-51: one commit after the driver marks this unit done).

**Commits:**

- `aecc4b4` — the harvest: 23/10 as a folder, the roadmap index's close, the re-pointed links.
- The commit that adds this handoff.

## What changed, and why

| File | What |
| --- | --- |
| `docs/roadmap/23-porous-shell/10-plan-run-record.md` → `10-plan-run-record/README.md` | **The record page became a same-numbered folder** (principle 5): the harvest would have taken it to about 28 KB. P08.1's text is kept as written, with its relative links re-pointed one level up; added: a pages table, four more open items (§ Open after the gates, items 3–6), a pointer under § For the owner's review, and § The run — P01 to P08, harvested (the per-unit table, how the run went) |
| `10-plan-run-record/01-measurements-and-lessons.md` (new) | the counts the run moved (porous ctests 0 → 39, suite 408, parity 82 → 87, ABI 0.5.2, IDs and the next free ones), the measurements only the handoffs held, the plan files' corrections, the lessons |
| `10-plan-run-record/02-in-run-choices.md` (new) | every *Chosen in-run (pending owner review)*, P01.1 … P08.2, with its record page |
| `10-plan-run-record/03-owners-review.md` (new) | the owner's review list (copied below) with the PR text |
| `docs/roadmap/23-porous-shell/README.md` | the page-10 sentence names the folder and its pages |
| `docs/roadmap/README.md` | § Current focus: the run closed 2026-10-22, awaiting review and PR, linking 23/10; § Next up item 1: the review list's link; milestones: a 2026-10-22 row for the run, and the 2026-10-15 #58 row leaves the five-row window (D-43; its record is 21/05). 15,354 / 15,360 B |
| `docs/roadmap/19-docs-layers/08-semantic-lint/10-2026-10-22-after-porous-run.md` | its two links to 23/10's anchor re-pointed to the folder's README |
| `plan/handoffs/HANDOFF_P08_TASK1.md` | the same link re-pointed (the `links` check reads `plan/`) |

**Statuses.** #63–#68 were already DONE with their evidence on 23/02 and the child pages, and
#69 DEFERRED with its trigger in the decisions index; D-67 … D-75 are written, D-74 and D-75
marked pending owner review. No row needed a change, and no new `#N` or `D-NN` was opened: the
open items (design/10's split, D-53, the crisp rim, `relax` and threads, an `--egl` mode, the
CI-only checks) are written in 23/10 § Open after the gates as the owner's calls, because their
homes (design/10, the decisions index) are at or near the byte cap.

**Chosen in-run (pending owner review):** 23/10 as a folder rather than a new child 11 beside
it; the milestone row for the run drops #58's row. Both are on 23/10/02.

## The harvest, one fact class at a time

Every handoff was read in full. Most of each unit's facts were already routed by the unit
itself (23/01–09, design/12–13, 11/16, 14/04, `capi/README.md`, the decisions pages). What
only the handoffs held now lives on 23/10:

- the per-unit table with commits, and how the run went (the OOM, the NV92 hang and the reboot,
  the gates green on first run, the byte cap): 23/10 README;
- the cross-unit counts and the handoff-only measurements (`build-gl`'s 81 / 244 s; the bake
  speed-up that is not one; the ABI test's export size; Boletus's header delta and the 2⁵³
  dump; the audit's noise): 23/10/01;
- the plan files' corrections (four italic notes, the facts recorded instead, the owner's
  three manifest edits) and fourteen lessons: 23/10/01;
- every in-run choice (14 rows, 60-odd choices): 23/10/02;
- every *For the owner's review* and *Open* item: 23/10 README § Open after the gates and
  23/10/03.

**A correction found while harvesting:** P06.1's handoff says the driver's smoke filter does not
include `cli_c_abi_porous`. It does (`porous` matches it), and P08.1's 39/39 counted it.
Written on 23/10/01.

## Measured, and under what

The owner's host (Ubuntu, GCC 15, CMake 4, Ninja, 8 cores, 22 GB), one heavy process at a time.

- `ctest --test-dir build -N`: **408** tests, **39** matching `porous` (408 − 39 = 369, the
  hygiene run's close).
- `wc -c`: 23/10's README 9.2 KB, 01 6.2 KB, 02 5.6 KB, 03 7.0 KB; roadmap/README 15,354 B.
- The gates and the loss audit: below.
- `/tmp/p082/` holds the gate log, the audit dump and the scratch triage; gone at a reboot.

## Checks run

All on the owner's host (free RAM 15 GB), one heavy process at a time, with no code change
since P08.1 (this unit changed docs only):

| Gate | Command | Final line |
| --- | --- | --- |
| Full gate | `python3 scripts/check.py` (on the staged harvest) | `32 checks: 29 passed, 0 failed, 2 skipped` / `PASS` — `ctest 408 tests, 0 failed` (146.9 s, serial); `configure` SKIP (configured), `warnings` SKIP (incremental, 0 TUs: P08.1's clean scan, 0 DualC-origin warnings over 260 TUs, still holds); 2 min 28 s |
| GPU tier | `cmake --build build-gl -j && timeout 900 prlimit --as=8000000000 -- python3 scripts/check.py --gpu --strict --build-dir build-gl` | `ninja: no work to do.`; `parity 87/87 passed` / `1 checks: 1 passed, 0 failed, 0 skipped` / `PASS`. Renderer checked first: `glxinfo -B` → llvmpipe (LLVM 21.1.8), "Accelerated: no" — the NV92 was not touched |
| Docs, strict | `python3 scripts/check.py --docs --strict` (staged, then after `aecc4b4`) | `28 checks: 27 passed, 0 failed, 0 skipped` / `PASS`; INFO `index-staleness` 17 pages |

The semantic lint was not re-run: P08.1's run 10 covers the code and design/, and this unit
changed neither.

## Loss audit

Run **before** the commit, as the rule asks:
`python3 scripts/docs_loss_audit.py --base 0d2a8e8 --head ed5e5fe --scope docs/ plan/ --annex plan/ --triage /tmp/p082-triage.json`.

- The head `ed5e5fe` is a scratch `git commit-tree` of the staged harvest with `plan/`
  removed (a temporary index); the triage is a scratch file with the `3d73220` thresholds and
  no rules, so every row is untriaged by construction. Exit 0, 12,580 untriaged rows; dump in
  `/tmp/p082/stdout.txt` (not committed).
- **The docs side:** the changed pages give 7 rows, all intentional: the Current-focus rewrite
  (roadmap/README:21; "the study and defaults in 23/01" is linked from the block README), the
  #58 milestone row leaving the window (its record 21/05), the block README's page-10 sentence,
  one 23/10 sentence the tool reads as rewritten though it is verbatim in the folder's README,
  and three token-only rows. Lint run 10's 84 rows are the noise every unchanged lint run
  shows (07 shows 142); its diff is the two link targets. The rest is the whole-tree noise
  (`docs/raw/` files and older records).
- **The annex (`plan/`):** 1,255 sentence, 346 evidence, 132 heading and 111 identifier rows
  residual: the handoffs, paraphrased into 03–09 and 23/10. A script listed the annex rows
  whose every number is absent from `docs/` (79): page byte counts of a given day, single
  ctest timings, P01's numbers (in the raw dump in another format), anchors and GUID
  fragments. The real ones were carried in `aecc4b4`, on 23/10/01 § Measurements only the
  handoffs held: the ABI test's sizes and timing, the rim variants at depth 6 (P04.1's
  1,776 / 155,640 included), the scale notes (2M seeds ≈ 30 s and 430 MB, the drop rule's
  1,288 / 732 / 2,784, the 8 s brute-force case, the 3000³ `bad_alloc`), and MSVC's D9025.
- No triage entry is committed (as P08.1 and the hygiene close did).

This handoff and the three P08.2 additions above were written after the audit's head; their
facts are on 23/10 (the README's close paragraph, 01, 03).

## What I got wrong on the way

- **My first list in § Open after the gates numbered 3, 2, 3, 4**; fixed to 3–6 before staging.
- **The roadmap index came out 37 B over the cap** after the milestone row and the
  current-focus rewrite; two parentheticals trimmed it to 15,354 B.
- **The first `--docs --strict` failed on a broken link** in P08.1's handoff (the `links` check
  reads `plan/` too); re-pointed.
- **My background wait loop never ended:** `until … ! pgrep -f "docs_loss_audit.py --base 0d2a8e8"`
  matched its own command line — P08.1's `pkill -f` lesson again — and was stopped at its time
  limit; the gate and the audit had finished long before. The session was then cut off before
  committing, and a resumed session finished it.
- **My first count of porous ctests was 40:** `ctest -N | grep -ci porous` also counts the
  "Test project …/DualC-porous/build" header line. It is 39.

## The owner's review list

The same list is on [23/10/03](../../docs/roadmap/23-porous-shell/10-plan-run-record/03-owners-review.md),
with the commands in full; the in-run choices are on
[23/10/02](../../docs/roadmap/23-porous-shell/10-plan-run-record/02-in-run-choices.md).

**Every `Chosen in-run (pending owner review)` line, with its page:**

| Unit | Choices | Page |
| --- | --- | --- |
| P01.1 | D-74 (own Bowyer–Watson on Shewchuk), D-75 (`03-settled-continued.md`), the defaults table's † rows | 23/02 § The engine choice, decisions/03, 23/01 § The defaults table |
| P02.1 | seven: Lloyd weight ⁵, fresh samples, Bridson grid, graded thinning, ceilings, graded errors, own `exp` | 23/09 |
| P03.1 | four: infinite vertex, renaming wrapper TU, C as a project language, canonical output | 23/03 § The choices made in the run |
| P03.2 | six: ascending fold, struts ≥ 0, Delaunay walls 0 outside the hull, `B`, drop rule, start-bin sweeps | 23/04 § The rules |
| P04.1 | seven: struts capped at `B − smooth`, `smooth ≥ B` an error, analytic gradient, `childBakeMargin`, no domain test, `PorousField` public, no cancel in the build | 23/05 § The in-run choices |
| P04.2 | four: smoothed rim, `seed=3` at depth 6, no item for the crisp rim, the samples' parameters | 23/06 § The in-run choices |
| P05.1 | six: R32I/RGBA32I, `PorousField::packed()`, guaranteed limits, step 0.5 on fallback, cursor-rescan fold, upload wired | 23/07 § The in-run choices |
| P05.2 | five: five parity cases, llvmpipe via scratch EGL, snapshots under `build/`, recompile only on change, NV92 not used | 23/08 § The in-run choices |
| P06.1 | eight: #63 → 09, ABI 0.5.2, both expression forms, cancel contour + export, `relax` ignores threads, V4 kept, letter `V`, trims | 23/02 #67 |
| P07.1 | nine: one component, twelve inputs, keys only off default, DualC's ranges, 32-bit `Seed`, no `smooth` push, D-49 … D-52 named, the merge-commit pin, checkout on `main` | 23/02 #68; the Boletus plan's § Decided by this plan |
| P08.1 | design/13 for the library contract; design/10's split and D-53 handed over | 23/10 § Open after the gates |
| P08.2 | 23/10 a folder; #58's milestone row out of the window | 23/10/02 |

**What needs a person** (`needsHumanCheck` is raised for these):

1. **The NV92.** After P08.1's reboot `glxinfo -B` named llvmpipe; when the NV92 is the active
   renderer, run one porous snapshot under `journalctl -kf | grep -i -E "trap|pgraph"`, then
   `python3 scripts/check.py --gpu --strict --build-dir build-gl` (87/87 expected).
   *Headless:* 87/87 on llvmpipe.
2. **The viewer live:** `build-gl/examples/dualc_field_view examples/samples/porous-sponge.json`
   — nudge `t2`, add `"smooth": 0.02` and nudge it, change `spacing`; then recipe P17.
3. **The samples as a sponge:** from `build/examples`, `dualc_field porous-sponge.json --depth 7
   -o sponge.stl` (and the two bracket samples); cut a section.
4. **Read design/13** against `include/dualc/porous.h`, and **11/16** as a user (V4's 8
   non-manifold edges).
5. **The Boletus plan's** § Decided by this plan.

**The decisions owed:** design/10's split or design/13 as the home of the porous hooks and
constants; D-53 reopened or not; #70 or a design/10 line for the crisp rim; `relax` and the
thread count beside #62; an `--egl` harness mode; D-74, D-75 and the in-run choices.

**The snapshots and the cavity counts:** `build/porous-snapshots/porous-sponge.png` (86 KB) and
`porous-sponge-struts.png` (143 KB, recipe `porous-sponge-struts.json` beside it), llvmpipe
renders. Surfaces (the outer shell plus each closed cavity), all watertight: `porous-sponge`
**14**, `porous-foam-bracket` **23**, `porous-bone-bracket` **7** at depth 7; the ctest's
sponge **14** at depth 6.

**The Boletus plan:** Boletus `docs/raw/2026-10-22-porous-shell-plan.md`, branch
**`feat/porous-shell-plan`**, commit `1ccbe23` (from Boletus `main` `2423275`; not pushed; the
checkout is on `main`).

**The counts:** ctest **408 / 408** (39 porous), GPU parity **87 / 87** (llvmpipe), 0
DualC-origin warnings, `--docs --strict` green.

**The push and the compare link.** The branch carries the hygiene run's 26 commits
(`adc465b` and below) under this run's 34 and this unit's two. Push and merge
`feat/hygiene-and-editor` first. Then, after the D-51 commit that deletes `plan/`:

```bash
git push -u origin feat/porous-shell
# only if the hygiene PR was squashed or rebased instead of merged:
git fetch origin && git rebase --onto origin/main adc465b feat/porous-shell
```

Compare link: <https://github.com/Giacogiak/DualC/compare/main...feat/porous-shell>

**The PR text:**

> **Porous shell — a `porous` field-graph source (#63–#68)**
>
> A natural-sponge source for the field graph: seeds in any volume (`poisson` / `jitter` /
> `random`, a graded density over a control, Lloyd `relax`), one Delaunay tetrahedralization
> (own Bowyer–Watson on Shewchuk's vendored public-domain predicates), and the unsigned
> distance to its Voronoi or Delaunay complex, `walls` or `struts`, smoothed in the node and
> thickened by the existing `onion` / `graded-onion` / `graded-offset`.
>
> - **Library:** `include/dualc/porous.h` — `generatePorousSeeds`, `PorousField`; the complex
>   internal; deterministic, the seeds, tets and packed arrays pinned by digest.
> - **CLI:** `porous(…)` in `dualc_field` (12 keys), three samples, `docs/command_reference/11-dualc_field/16-porous-shell.md`.
> - **GPU:** the node in GLSL from data textures, a baked fallback past the limits; parity 82 → 87.
> - **C ABI:** no version move (0.5.2); `cli_c_abi_porous` covers an in-memory mesh volume,
>   JSON == shorthand, cancel.
> - **Boletus:** an implementation plan on Boletus `feat/porous-shell-plan` (not pushed).
> - **Design:** `docs/design/13-porous-shell.md`; the record is `docs/roadmap/23-porous-shell/`.
>
> Local gates on Linux (GCC 15): ctest 408/408, parity 87/87 (llvmpipe), 0 warnings,
> `--docs --strict` green. Not verified locally: macOS / Windows digests and MSVC's build of
> `predicates.c` (this CI run), the NV92. Known limit, older than this PR: a crisp
> `intersection` rim on a curved volume leaves a few non-manifold edges; the samples use a
> `smooth-intersection`. The in-run choices awaiting review: `23-porous-shell/10-plan-run-record/02-in-run-choices.md`.
>
> 🤖 Generated with [Claude Code](https://claude.com/claude-code)

## Acceptance and exit criteria, one by one

**T2 acceptance — "`plan/`'s facts are harvested, and every local gate is green":** holds.
Every handoff was read; what only they held is on 23/10 and its pages (§ The harvest above),
and the loss audit's annex was screened for leftovers. The full gate, the GPU tier and
`--docs --strict` are green (§ Checks run).

**T2 acceptance — "the handoff carries the review list, the push command, the compare link and
the PR text":** holds, § The owner's review list above (also on 23/10/03).

**T2 steps:** 1 harvest ✓ (23/10 README, 01, 02); 2 statuses ✓ (items and decision rows
already final, checked; roadmap § Current focus, § Next up, the milestone row, five rows
kept); 3 review list ✓; 4 loss audit with `--annex plan/` ✓, `plan/` not deleted; 5 final
`--docs --strict` ✓.

**T1's criteria (P08.1), re-checked:** all four gates green; each final line is in P08.1's
handoff and re-run above where code-dependent.

**Exit criterion — "The run is closed on the local branch, ready for the owner's review, push
and PR":** holds. Every unit's work is committed on `feat/porous-shell`, the tree is clean
after this handoff's commit, nothing is pushed, and the review list, the push order, the
compare link and the PR text are on 23/10/03. What stays for the owner is listed there and
above; none of it blocks the PR. The D-51 deletion of `plan/` is the next commit, after the
driver marks this unit done.

## What the next step needs to know

- **The D-51 commit** deletes `plan/` (the manifest and the handoffs included) once the driver
  marks this unit done. The loss audit above covers it: run it again only if `plan/` changes
  after this commit. `.plan-run/` is gitignored and stays.
- `docs/roadmap/README.md` is 6 B under its cap; the next status change there needs a trim.
- `index-staleness` (INFO, report-only) lists 17 pages; 23/README stays on it even though it
  was committed with roadmap/README in `aecc4b4` (the check reads commit order, and the two
  share a commit), and `design/13-porous-shell.md` joined it from P08.1's last commit. Both
  index rows were re-read and are right.
- Next free IDs: block 24, #70, D-76, lint run 11, design page 14.
