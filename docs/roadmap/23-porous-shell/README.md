# Porous shell — a natural sponge from a cell complex

The record of the `porous` field-graph source: seeds in any volume (blue noise, jitter,
white noise, a graded density, Lloyd relaxation), one Delaunay tetrahedralization of them,
and the unsigned distance to its Voronoi or Delaunay complex (`walls` or `struts`), smoothed
in the node, thickened by the existing `onion` / `graded-onion` / `graded-offset`, in
`dualc_field`, `dualc_field_view` and the C ABI, plus an implementation plan for Boletus.
Born 2026-10-21 from the [porous shell plan](../../raw/2026-10-19-porous-shell-plan.md),
drafted from the owner's request of 2026-10-19 and run unattended by `plan-run` on branch
`feat/porous-shell`. Headings are frozen at ID + title; status, date and trigger live on the
body lines. The decisions are D-67 … D-75
([settled, continued](../../decisions/03-settled-continued.md)); the open one is #69
([decisions index](../../decisions/README.md)).

## The pages of this block

A folder from the start. [01](01-study.md) is the P01 study — the Delaunay engine, the
distributions, the distance formulas, the GPU layout — ending in **the defaults table**
that binds the later phases. [02](02-record.md) is the record: every item, its acceptance
line and its dated appends; an item whose measurements outgrow it keeps its summary there
and its numbers in a numbered child: [03](03-delaunay-engine.md) and [04](04-complex-and-bins.md) are #64's, the
Delaunay engine and the packed complex as built and measured; [05](05-porous-field.md) and
[06](06-sponge-end-to-end.md) are #65's, the field and the graph token, and the sponge end to end;
[07](07-glsl-node.md) and [08](08-parity-and-viewer.md) are #66's, the GLSL emitter and its data textures, and the parity and the viewer;
[09](09-seeds-as-built.md) is #63's, the seed generator as built, moved out of 02 when 02 reached its cap.
[10](10-plan-run-record/README.md) is the run's close, a folder since the harvest outgrew one page: the
gates P08 ran, what stays open after them, the per-unit table, the measurements and lessons
([01](10-plan-run-record/01-measurements-and-lessons.md)), every in-run choice
([02](10-plan-run-record/02-in-run-choices.md)) and the owner's review list
([03](10-plan-run-record/03-owners-review.md)). [11](11-viewer-trace-cost.md) is #70's, the
viewer's trace cost on a sponge, measured and fixed after the merge.

| Item | Page | Status |
| --- | --- | --- |
| The owner's decisions of 2026-10-19 (D-67 … D-73) and the engine choice (D-74) | [02](02-record.md#the-owners-decisions) | DONE 2026-10-21 (decided; D-74 and the defaults marked † pending owner review) |
| #63 Deterministic seeds in any volume | [02](02-record.md#63-deterministic-seeds-in-any-volume) · [09](09-seeds-as-built.md) | DONE 2026-10-21 (P02; the in-run choices pending owner review) |
| #64 Delaunay tetrahedralization and the packed complex | [02](02-record.md#64-delaunay-tetrahedralization-and-the-packed-complex) · [03](03-delaunay-engine.md) · [04](04-complex-and-bins.md) | DONE 2026-10-21 (P03; the in-run choices pending owner review) |
| #65 The porous node on the CPU | [02](02-record.md#65-the-porous-node-on-the-cpu) · [05](05-porous-field.md) · [06](06-sponge-end-to-end.md) | DONE 2026-10-22 (P04; the in-run choices pending owner review) |
| #66 The porous node in GLSL and dualc_field_view | [02](02-record.md#66-the-porous-node-in-glsl-and-dualc_field_view) · [07](07-glsl-node.md) · [08](08-parity-and-viewer.md) | DONE 2026-10-22 (P05; parity on llvmpipe, the NV92 and the in-run choices pending owner review) |
| #67 The C ABI test and the documentation | [02](02-record.md#67-the-c-abi-test-and-the-documentation) | DONE 2026-10-22 (P06; the in-run choices and the viewer recipes pending owner review) |
| #68 The Boletus implementation plan | [02](02-record.md#68-the-boletus-implementation-plan) | DONE 2026-10-22 (P07; Boletus branch `feat/porous-shell-plan`, the in-run defaults pending owner review) |
| #69 Graded nodes miss a negative thickness in cellOverlaps | [02](02-record.md#69-graded-nodes-miss-a-negative-thickness-in-celloverlaps) | DEFERRED — trigger in the decisions index |
| #70 Smooth booleans keep the viewer's metric step | [02](02-record.md#70-smooth-booleans-keep-the-viewers-metric-step) · [11](11-viewer-trace-cost.md) | DONE 2026-10-27 (the GTX 1050 orbit pending the owner) |

**The Boletus plan** is `docs/raw/2026-10-22-porous-shell-plan.md` in the Boletus repository,
on its branch `feat/porous-shell-plan` (commit only, not pushed); the pointer from the
Boletus boundary is [15 § DualC retains](../15-boletus-handoff.md#dualc-retains--engine--abi-boletus-only-requests-these-upstream).

---

← Back to the [Roadmap index](../README.md).
