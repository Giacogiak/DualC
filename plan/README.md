# `plan/` — the runner's split of the hygiene + editor plan

The plan itself is the immutable input
[`docs/raw/2026-10-19-hygiene-and-editor-plan.md`](../docs/raw/2026-10-19-hygiene-and-editor-plan.md):
the owner's goal and decisions, the repo facts verified on the drafting day, and the eight
phases. This folder is the **working split** the `plan-run` driver executes: one file per phase,
one fresh session per task, and a handoff per unit under `handoffs/`. A session may correct
these files when the code proves a step wrong; the raw plan is never corrected. Status lives
in roadmap blocks 17 (#34, #35), 14 (#53) and the editor hand-off block P07 opens, never here.
The folder is transient: it is deleted at the plan's close (D-51).

| File | Phase | Advances |
| --- | --- | --- |
| [10_P01_open-record.md](10_P01_open-record.md) | P01 — open the record, decision rows, the stale #53 cell | 17, 14, decisions |
| [11_P02_contourer-threads.md](11_P02_contourer-threads.md) | P02 — `ContourerParams::numThreads`, the C ABI, `dualc_field --threads` | #34 |
| [12_P03_gradient-contract.md](12_P03_gradient-contract.md) | P03 — the unit-direction contract, the true-gradient path | #34 |
| [13_P04_gridfield-normal.md](13_P04_gridfield-normal.md) | P04 — a continuous `GridField` normal; #34 closed | #34 |
| [14_P05_linux-so-ci.md](14_P05_linux-so-ci.md) | P05 — the gate builds the C ABI | #53 |
| [15_P06_repeat-pruning.md](15_P06_repeat-pruning.md) | P06 — `RepeatField` neighbour pruning, byte-identical | #35 |
| [16_P07_editor-acknowledgement.md](16_P07_editor-acknowledgement.md) | P07 — DualCEditor owns the standalone UI (hand-off block) | Track 2 |
| [17_P08_close.md](17_P08_close.md) | P08 — harvest, lint, loss audit, final gates, PR text (`plan/` is deleted after the run) | — |

`/plan-run-setup` writes the manifest, `run.manifest.json`. The run state goes in
`.plan-run/` at the repo root, which is gitignored.

**Branch for every unit: `feat/hygiene-and-editor`** (cut from `main` at `5ce5b1c`).
**Never `git push`, never add a remote.** Every push is the owner's, by hand. There is no
remote-run check: the owner reads CI after their push.

**Unattended run.** Every gate is `auto`, and the run is meant to go through the night with no
human stop. A session **never blocks on a question**. Where a choice is open, it takes the
default in the raw plan's § 3, writes `**Chosen in-run (pending owner review):** …` in its
handoff and the record page, and continues.

Every session follows `AGENTS.md`:

- the reading order at the start;
- the full gate (`python3 scripts/check.py`) before a code commit;
- `--gpu --build-dir build-gl` as well when a field node or the GLSL codegen moved (rebuild `build-gl` first: `cmake --build build-gl -j`);
- `python3 scripts/check.py --docs --strict` at the close, and every fact routed by the table in `docs/README.md`.

The handoff is in addition to the roadmap record, never a substitute.
