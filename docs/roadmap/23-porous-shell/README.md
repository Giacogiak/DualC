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
Delaunay engine and the packed complex as built and measured.

| Item | Page | Status |
| --- | --- | --- |
| The owner's decisions of 2026-10-19 (D-67 … D-73) and the engine choice (D-74) | [02](02-record.md#the-owners-decisions) | DONE 2026-10-21 (decided; D-74 and the defaults marked † pending owner review) |
| #63 Deterministic seeds in any volume | [02](02-record.md#63-deterministic-seeds-in-any-volume) | DONE 2026-10-21 (P02; the in-run choices pending owner review) |
| #64 Delaunay tetrahedralization and the packed complex | [02](02-record.md#64-delaunay-tetrahedralization-and-the-packed-complex) · [03](03-delaunay-engine.md) · [04](04-complex-and-bins.md) | DONE 2026-10-21 (P03; the in-run choices pending owner review) |
| #65 The porous node on the CPU | [02](02-record.md#65-the-porous-node-on-the-cpu) | PLANNED — P04 |
| #66 The porous node in GLSL and dualc_field_view | [02](02-record.md#66-the-porous-node-in-glsl-and-dualc_field_view) | PLANNED — P05 |
| #67 The C ABI test and the documentation | [02](02-record.md#67-the-c-abi-test-and-the-documentation) | PLANNED — P06 |
| #68 The Boletus implementation plan | [02](02-record.md#68-the-boletus-implementation-plan) | PLANNED — P07 |
| #69 Graded nodes miss a negative thickness in cellOverlaps | [02](02-record.md#69-graded-nodes-miss-a-negative-thickness-in-celloverlaps) | DEFERRED — trigger in the decisions index |

---

← Back to the [Roadmap index](../README.md).
