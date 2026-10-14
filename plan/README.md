# `plan/` — the runner's split of the CLI consolidation plan

The plan itself is the immutable input
[`docs/raw/2026-10-14-cli-consolidation-plan.md`](../docs/raw/2026-10-14-cli-consolidation-plan.md):
the owner's goal and decisions, the repo facts verified on the drafting day, and the seven
phases; its evidence is the [screening](../docs/raw/2026-10-14-cli-tools-screening.md). This folder
is the **working split** the `plan-run` driver executes — one file per phase, one fresh session
per task, a handoff per unit under `handoffs/`. A session may correct these files when the code
proves a step wrong; the raw plan never is. Status lives in roadmap block 21 (opened by P01 T1)
and in the handoffs, never here. Transient: deleted at the plan's close (D-51).

| File | Phase | Advances |
| --- | --- | --- |
| [10_P01_record-and-demo-meshes.md](10_P01_record-and-demo-meshes.md) | P01 — open the record, fix the inside-out demo meshes (F1) | block 21, its items and decisions |
| [11_P02_mesh-node-params.md](11_P02_mesh-node-params.md) | P02 — `mesh(sign="gwn")` and per-node `bake` | `dualc_demo`, `dualc_boolean` retirement preconditions |
| [12_P03_2d-lifts.md](12_P03_2d-lifts.md) | P03 — 2D profiles + `revolve` / `extrude` nodes, CPU then GLSL | `dualc_lift`; D-11's 2D trigger |
| [13_P04_graph-slice.md](13_P04_graph-slice.md) | P04 — `dualc_field --slice` on any graph | `dualc_slice` |
| [14_P05_docs-migration.md](14_P05_docs-migration.md) | P05 — every retiring page's content moved to the framework pages | all eight retirements |
| [15_P06_retire-tools.md](15_P06_retire-tools.md) | P06 — delete the eight tools: code + tests, then docs | all eight retirements |
| [16_P07_close.md](16_P07_close.md) | P07 — harvest, close block 21, hand over the PR (`plan/` is deleted after the run) | — |

`remote_run_check.py` is the `remote-run` check: the pushed branch's latest GitHub Actions run
must be green on its required jobs. The manifest `run.manifest.json` is written by
`/plan-run-setup`; the run state is `.plan-run/` at the repo root, gitignored.

**Branch for every unit: `feat/cli-consolidation`.** Push it as often as needed; never push
`main`, never force-push. The owner opens and merges the PR after P07.

Every session follows `AGENTS.md`: the reading order at the start, the full gate
(`python3 scripts/check.py`) before a code commit, `--gpu --build-dir build-gl` when a field node
or the GLSL codegen moved (rebuild `build-gl` first: `cmake --build build-gl -j`), and at the
close `python3 scripts/check.py --docs --strict` plus the routing of every fact by the table in
`docs/README.md`. The handoff is in addition to the roadmap record, never a substitute.
