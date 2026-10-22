# Handoff — P07.1: the Boletus implementation plan

Unit P07.1 of [`plan/16_P07_boletus-plan.md`](../16_P07_boletus-plan.md) § T1, the last (and
only) unit of P07. Session of 2026-10-22. DualC: branch `feat/porous-shell` in
`~/Documents/DualC-porous`. Boletus: branch `feat/porous-shell-plan` in `~/Documents/Boletus`.
Nothing was pushed, no remote was touched, `main` / `feat/hygiene-and-editor` /
`~/Documents/DualC` were left alone, Boletus's `main` is untouched.

**Commits:**

- Boletus `1ccbe23` on `feat/porous-shell-plan` (parent: Boletus `main` at `2423275`):
  `docs/raw/2026-10-22-porous-shell-plan.md` (new) and its row in `docs/raw/README.md`.
- DualC `22a0a9f`: the pointers and the record (below).
- The DualC commit that adds this handoff.

**The record:** [23/02 #68](../../docs/roadmap/23-porous-shell/02-record.md#68-the-boletus-implementation-plan)
is **DONE** 2026-10-22 with a T1 append; the block README's row and the roadmap index's three
places read `#63–#68 DONE`.

## What changed, and why

| Where | What |
| --- | --- |
| Boletus `docs/raw/2026-10-22-porous-shell-plan.md` | The plan, in the shape of Boletus's `2026-10-06-public-delivery-plan.md` (Context, what was measured, decided, order of work, numbered steps with files and code, verification, not done). Content per the raw plan § 3 P07: the DualC commit to pin (the merge of `feat/porous-shell` into DualC `main`; until then `--dualc ~/Documents/DualC-porous`); the `Ops.cs` row with all 12 keys, kinds, DualC defaults and choices (from `readPorousParams` and `--list`); the *Porous Shell* component (14 inputs: the raw plan's list + `Jitter`; GUID `…0110`; Sources); the previews (Proxy, Write to File, Live with the file-watch rebuild); `CliParityTests`, vocabulary/serializer, validator, example, in-memory and cancel tests; the D-NN rows it would open (D-49 … D-52, Boletus's next free being D-49); the docs splits the size cap forces; the Rhino smoke test |
| Boletus `docs/raw/README.md` | one row |
| DualC `docs/roadmap/23-porous-shell/02-record.md` | #68 → DONE, T1 append (findings, in-run choices) |
| DualC `docs/roadmap/23-porous-shell/README.md` | the #68 row; a pointer paragraph naming the Boletus branch and file |
| DualC `docs/roadmap/15-boletus-handoff.md` | a row in § DualC retains: the `porous` source, PLANNED on the Boletus side, the branch and the file |
| DualC `docs/roadmap/README.md` | `#63–#68 DONE` in § Current focus, § Next up and the block table |

Boletus's `git status` was clean on `main` at the start. After the commit I switched Boletus's
checkout **back to `main`**, so the owner finds it as it was; the plan is on the branch only.

## Measured, and under what

- **Boletus's docs gate**, `python3 scripts/check.py --docs --strict` on the branch with the
  plan staged and again at commit: 27 checks, 26 OK + 1 INFO (`index-staleness` on
  `09-docs-layers/06-semantic-lint/README.md`, pre-existing on Boletus `main`, report-only),
  **PASS**. `sizes` 64 files, 0 over (`raw/` is exempt).
- **Boletus against this tree:** `DUALC_ROOT=~/Documents/DualC-porous python3
  scripts/check.py --docs` in Boletus: `dualc-links` 136 files, 118 DualC paths cited, **0
  missing** — the re-pin costs no citation chase.
- **The header between Boletus's pin `350cb3a` and `4bed5e3`:** `git diff 350cb3a HEAD --
  capi/dualc_c.h`: two comment lines (the `numThreads` comment, 0.5.2; the version string).
  No signature moves; `porous` needs nothing new in Boletus's `NativeMethods.cs`.
- **The serializer edge:** `dualc_field --expr "porous(…,seed=9007199254740992,relax=2,…)"
  --dump-json` emits `"relax": 2.0`, `"seed": 9.007199254740992e+15`; .NET's
  `double.ToString` writes `E+15`. Harmless for the dump-vs-dump tests; written into the plan
  so a golden string avoids it.
- **The DualC docs gate**, `python3 scripts/check.py --docs` before the DualC commit (and the
  pre-commit hook at it): 28 checks, 27 OK + 1 INFO, **PASS**. Sizes after: roadmap/README
  15,249; 23/02 13,775; 23/README 4,046; 15 13,673 (cap 15,360).

No build and no ctest: this unit changed docs only, in both repositories.

## What I got wrong on the way

- **I first wrote the cancel test as "returns `DUALC_CANCELLED`".** Boletus's wrapper throws
  `OperationCanceledException` (`ProgressAndCancelTests`); corrected in the plan before the
  commit.
- Nothing else surfaced; the plan's claims about Boletus were each read from its code
  (`OpSchema.ChildCount` exact, used only by the validator; `Proxy preview` creates on the solve
  thread; `Write to File` on its task; `MeshMaterializingResolver` writes `"R"` floats; the
  next GUID; `01-sources.md` 14,258 B and `design/04` 15,358 B against the 15,360 cap).

## The in-run choices

Each is **Chosen in-run (pending owner review)**, also in 23/02 #68 and in the plan's
§ Decided by this plan:

1. **One `Porous Shell` component in Sources**, thickness and clip left to the existing
   components (DualC D-69's composition).
2. **All twelve keys as inputs**: `Jitter` appended to the raw plan's list, so no DualC key is
   unreachable from the canvas.
3. **A key is emitted only off DualC's default**, so the canvas JSON equals DualC's samples.
4. **The ranges stay DualC's** (located errors surfaced by the terminals); the component checks
   only what it needs for a well-formed node.
5. **`Seed` is a 32-bit GH Integer**, a subset of DualC's 0–2⁵³.
6. **No uniform push for `smooth`** (Boletus D-27 stands).
7. **The plan's D-NN rows are named, not written**: D-49 (the component; DONE when it lands),
   D-50 (a cancellable build, DEFERRED), D-51 (create-time threads, DEFERRED, answers DualC
   #62), D-52 (opened only if the Live-preview mesh test finds other cells). Boletus block
   **11** and items from **#35** are named the same way.
8. **The pin is the future merge commit**, not a branch commit: a submodule must pin a commit
   reachable on GitHub, and this branch is not pushed.
9. **Boletus's checkout left on `main`** after the commit on the branch.

## Exit criteria of P07, one by one

- **T1 acceptance — "the Boletus commit exists on `feat/porous-shell-plan`, and Boletus's other
  branches are untouched":** `git -C ~/Documents/Boletus branch -v` → `feat/porous-shell-plan
  1ccbe23`, `main 2423275` (its tip before the session, `origin/main` the same); no other
  branch exists. Nothing pushed.
- **T1 acceptance — "the DualC pointer is committed, and `python3 scripts/check.py --docs` is
  green":** `22a0a9f` (23/README paragraph + row, 15 § DualC retains row, 23/02 #68, the
  index); `--docs` PASS, 27 OK + 1 INFO.
- **Exit — "Boletus has a ready-to-run implementation plan for the porous node":** the plan
  names every file to touch with the change in each (`OpSchema.cs`, `Ops.cs` with the code,
  `FieldGraphValidator.cs` rule by rule, `Field.cs` signature, `PorousShellComponent.cs` with
  the full inputs table and GUID, the five test files with their graphs and expected numbers
  from DualC's measurements, `check_data.json`'s floor, the two docs splits the cap forces,
  the decisions, the roadmap block), the order, the verification and the Rhino steps; Boletus's
  docs gate passes on it. The one thing it cannot do today is the pin itself: it waits for
  this branch's merge into DualC `main` on GitHub (the owner's PR), and says how to develop
  against the local tree until then. Judged by reading it against the Boletus code, not by
  running it — Boletus implements nothing in this run (D-72).

## For the owner's review

**needsHumanCheck is raised** — not for a window, but because "ready to run" and the in-run
defaults are the owner's judgement:

1. Read `~/Documents/Boletus/docs/raw/2026-10-22-porous-shell-plan.md` on its branch:
   `git -C ~/Documents/Boletus show feat/porous-shell-plan:docs/raw/2026-10-22-porous-shell-plan.md | less`.
   Confirm or change § Decided by this plan (the nine choices above).
2. Decide when to push Boletus's `feat/porous-shell-plan` (after DualC's PR merges, its Step 1
   can name the real pin).

**Verified headless:** both docs gates; the facts the plan states about Boletus's code and
about DualC's header, parser and dump.

## What the next unit needs to know

- **P08:** the Boletus plan's path and branch for the owner's review list: Boletus
  `docs/raw/2026-10-22-porous-shell-plan.md`, branch `feat/porous-shell-plan`, commit
  `1ccbe23`. Boletus is not part of P08's gates.
- The 23/02 page is at 13,775 B (room for P08's appends, about 1.5 KB); `roadmap/README.md`
  at 15,249 B — 111 B under the cap.
- No code changed in this unit; the smoke filter's results are P06's.
