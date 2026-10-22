# The porous run — every in-run choice, for the owner's review

Part of [the run's record](README.md) (23/10), gathered by P08.2 on 2026-10-22. The run was
unattended ([D-73](../../../decisions/03-settled-continued.md)): where the plan left a choice
open, the unit took the study's defaults table or made the call, wrote it as **Chosen in-run
(pending owner review)** on its record page, and went on. This page lists every one, in one
place; the record page holds the reason and the measurement. Accepting one costs nothing;
where a unit named the alternative, it is in the reason.

| Unit | Choice | Record |
| --- | --- | --- |
| P01.1 | **D-74:** the engine is our own sequential Bowyer–Watson on Shewchuk's predicates; geogram's PSM rejected for its unlocked process-wide RNG and its call-to-call tet order | [02 § The engine choice](../02-record.md#the-engine-choice), [01 § (a)](../01-study.md#a-the-delaunay-engine) |
| P01.1 | **D-75:** settled rows from D-67 on go to `03-settled-continued.md`, not a folder split of `01-settled.md` (about 40 inbound links, some from `docs/raw/`) | [03-settled-continued](../../../decisions/03-settled-continued.md) |
| P01.1 | the defaults table's † rows: `spacing` per distribution, the variable-radius Bridson, the pad, `relax`, the Delaunay-walls walk, `smooth` (each later refined in italics on the table) | [01 § The defaults table](../01-study.md#the-defaults-table) |
| P02.1 | (1) Lloyd's weight `(spacing/spacing(x))⁵`, not `³` · (2) fresh samples per iteration, 32 per seed · (3) Bridson on a linked-list grid at `max(rmin, rmax/2)` with a restart scan · (4) graded `jitter` / `random` thin a finer layout · (5) the ceilings · (6) the graded-parameter errors · (7) DualC's own `exp` for the gaussian ramp | [09](../09-seeds-as-built.md) |
| P03.1 | (1) an infinite vertex, not the finite super-tet · (2) symbols renamed through a wrapper TU, `predicates.c` byte-identical · (3) C becomes a project language · (4) canonical output, orientation not stored | [03 § The choices made in the run](../03-delaunay-engine.md#the-choices-made-in-the-run) |
| P03.2 | (1) the smooth fold in ascending distance, ties by packed index · (2) struts clamped at 0 · (3) Delaunay walls 0 outside the seeds' hull · (4) the struts' `B` = max(larger spacing, largest cell radius) · (5) the drop rule · (6) start bins by two sweeps, bins 1.25× coarser past `kPorousMaxBins` | [04 § The rules](../04-complex-and-bins.md#the-rules) |
| P04.1 | (1) the field caps struts at `B − smooth` · (2) `smooth ≥ B` with struts is an error · (3) an analytic gradient · (4) `childBakeMargin(porous, 0)` = the pad · (5) the domain-wrapper test does not apply · (6) `PorousField` public, the complex internal · (7) no cancel token reaches the graph build | [05 § The in-run choices](../05-porous-field.md#the-in-run-choices) |
| P04.2 | (1) a `smooth-intersection` rim on the test and the sphere sample · (2) the test pins `seed=3` at depth 6 · (3) no tracked item and no design/10 line for the crisp rim · (4) the samples' nodes, sizes and parameters | [06 § The in-run choices](../06-sponge-end-to-end.md#the-in-run-choices) |
| P05.1 | (1) integer textures R32I / RGBA32I (signed keeps −1) · (2) a public `PorousField::packed()` · (3) `GlslLimits` defaults to the guarantees (2048 rows, 16 units) · (4) a baked fallback lowers the step scale to 0.5 · (5) the fold as a cursor rescan, exact to 64 primitives within `k`; no arrays, `continue` or returns in loops · (6) the upload wired into the harness and viewer | [07 § The in-run choices](../07-glsl-node.md#the-in-run-choices) |
| P05.2 | (1) five parity cases, the sponge added · (2) the GPU tier on llvmpipe through a scratch EGL context; no committed `--egl` mode · (3) the snapshots under `build/`, not in `docs/` · (4) the viewer recompiles only when the queried limits change the scene · (5) the NV92 not used | [08 § The in-run choices](../08-parity-and-viewer.md#the-in-run-choices) |
| P06.1 | (1) #63's detail moved to 09, not 02 split · (2) the ABI stays 0.5.2 · (3) "the expression form" read as both shorthand-vs-JSON and the plain create call · (4) the cancel case cancels contour and export; the create call cannot be cancelled · (5) `relax` ignores `--threads`, documented, not changed · (6) the skinned recipe V4 listed with its 8 non-manifold edges · (7) recipe letter `V` · (8) wording trims to fit 11/README and command_reference/README | [02 #67](../02-record.md#67-the-c-abi-test-and-the-documentation) |
| P07.1 | (1) one *Porous Shell* component in Sources · (2) all twelve keys as inputs, `Jitter` appended · (3) a key emitted only off DualC's default · (4) the ranges left to DualC's located errors · (5) `Seed` a 32-bit integer · (6) no uniform push for `smooth` (Boletus D-27) · (7) Boletus D-49 … D-52 named, not written · (8) the pin is the future merge commit · (9) Boletus's checkout left on `main` | [02 #68](../02-record.md#68-the-boletus-implementation-plan); the plan's § Decided by this plan |
| P08.1 | design/13 written for the porous library's contract, as design/10 is at the cap; design/10's split, and D-53, handed to the owner | [README § Open after the gates](README.md#open-after-the-gates) |
| P08.2 | 23/10 became a folder for the harvest; the milestone row for the run drops the window's 2026-10-15 row (#58), whose record is [21/05](../../21-cli-consolidation/05-2d-profiles-and-lifts.md) | this page |

---

← Back to [23/10 — the run's record](README.md) · [block 23](../README.md) · the [Roadmap index](../../README.md).
