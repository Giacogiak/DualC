# `plan/` — the runner's split of the porous shell plan

The plan itself is the immutable input
[`docs/raw/2026-10-19-porous-shell-plan.md`](../docs/raw/2026-10-19-porous-shell-plan.md):
the owner's goal and decisions, the repo facts verified on the drafting day, the node as
designed, and the eight phases. This folder is the **working split** the `plan-run` driver
executes: one file per phase, one fresh session per task, and a handoff per unit under
`handoffs/`. A session may correct these files when the code proves a step wrong, in italics
(`*Corrected by P0N.M:* …`); the raw plan is never corrected. Status lives in the roadmap block
P01 opens, never here. The folder is transient: it is deleted at the plan's close (D-51).

| File | Phase | Advances |
| --- | --- | --- |
| [10_P01_explore-and-record.md](10_P01_explore-and-record.md) | P01 — study the Delaunay engine and the distributions, open the block, configure the worktree's build trees | the block, decisions |
| [11_P02_seeds.md](11_P02_seeds.md) | P02 — deterministic seeds: poisson / jitter / random, graded density, Lloyd relax | seeds item |
| [12_P03_delaunay-and-complex.md](12_P03_delaunay-and-complex.md) | P03 — the vendored (or own) Delaunay, then the cell complex and its bin grid | Delaunay + complex item |
| [13_P04_field-and-node.md](13_P04_field-and-node.md) | P04 — `PorousField` + the `porous` graph node, then contouring end to end | CPU node item |
| [14_P05_glsl-and-viewer.md](14_P05_glsl-and-viewer.md) | P05 — GLSL codegen, parity, `dualc_field_view` | GPU node item |
| [15_P06_abi-and-docs.md](15_P06_abi-and-docs.md) | P06 — C ABI test + entry, command reference, design | ABI + docs item |
| [16_P07_boletus-plan.md](16_P07_boletus-plan.md) | P07 — the Boletus implementation plan (Boletus repo, own branch, commit only) | Boletus plan item |
| [17_P08_gates-and-close.md](17_P08_gates-and-close.md) | P08 — every gate, repair, harvest, owner's review list (`plan/` deleted after the run) | — |

`/plan-run-setup` writes the manifest, `run.manifest.json`. The run state goes in `.plan-run/`
at the repo root, which is gitignored. `init` splits these files into 12 units, every gate
`auto`. **The settings proposed for setup** (the owner confirms each) mirror the hygiene run's:

- **Sessions:** `model opus`, `effort high`, `permissionMode auto`, `maxBudgetUsd 25`,
  `maxUnitsPerRun 12`, `maxRunListUsd 250`, `timeoutMinutes 180`.
- **Scope:** `addDirs ["/home/giak/Documents/Boletus"]`, `gitPolicy.noPush true`,
  `stopUnits []`, `initialHandoff` = the raw plan, `handoffDir plan/handoffs`.
- **Checks:**
  - `ciCommand "python3 scripts/check.py"`.
  - Default `verify` = `tree-clean, new-commit, handoff, smoke`, where `smoke` is
    `cmake --build build -j && ctest --test-dir build -R "porous|field_graph|field_glsl" --output-on-failure`.
  - P08.1 and P08.2 also get `ci`.
  - *Corrected by P01.1:* the `smoke` filter `porous|field_graph|field_glsl` matches no ctest
    name today: the Catch2 cases read `field-graph …` and `GLSL codegen …`, so it runs only the
    `porous` cases (none before P02, `ctest` then exits 0 on "No tests were found"). The
    manifest is the owner's and is not changed; the defaults table names new cases `porous: …`
    so the filter bites, and each unit also runs `-R 'porous|field-graph|GLSL codegen'` itself.
- **`promptNotes`:** the hygiene run's rules, adapted:
  - never block on a question;
  - branch `feat/porous-shell`, never push;
  - Boletus is writable on `feat/porous-shell-plan` in P07 only, read-only elsewhere;
  - **no full gate per unit: the session runs the targeted build + its tests, and P08 runs every
    gate**;
  - IDs read from the files;
  - nothing stays in the chat.

**Where and when.** The worktree is `~/Documents/DualC-porous`, branch **`feat/porous-shell`**.
It runs **after** the hygiene + editor run: before the first unit, the owner rebases the branch
onto that run's tip once its `plan/` has been deleted (`git rebase feat/hygiene-and-editor`).
**Never `git push`, never add or change a remote.** Every push is the owner's, by hand.

**Unattended run, checks at the end.** Every gate is `auto`, and the run goes through the night
with no human stop. A session **never blocks on a question**. Where a choice is open, it takes
the default in the raw plan's § 3, writes `**Chosen in-run (pending owner review):** …` in its
handoff and the record page, and continues.

- **During the run:** the driver verifies P01–P07 only for a clean tree, a new commit, a handoff
  and the `smoke` check (incremental build + the porous / field-graph / GLSL ctests). Each session
  builds and runs the tests it writes.
- **At the end:** the full gate, the GPU tier, `--docs --strict` and the semantic lint run in
  P08.1, which repairs what they find (raw plan § 1, *Checks*).

Every session follows `AGENTS.md`: the reading order at the start, every fact routed by the table
in `docs/README.md`, numbers as IDs. The handoff is in addition to the roadmap record, never a
substitute.

← Back to the [raw plan](../docs/raw/2026-10-19-porous-shell-plan.md).
