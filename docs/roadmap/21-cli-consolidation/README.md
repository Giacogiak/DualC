# CLI consolidation — one framework, `dualc_field` + `dualc_field_view`

The record of folding DualC's side CLIs into the field-graph framework: every capability a
side tool alone holds moves into the graph — and so within reach of the C ABI, which is the
graph and nothing else — and the tool retires. Born 2026-10-14 from the owner's answers to the
[CLI tools screening](../../raw/2026-10-14-cli-tools-screening.md) (the evidence: a verdict per
tool, eight byte-identical tool-vs-graph runs, three side findings); worked through the
[consolidation plan](../../raw/2026-10-14-cli-consolidation-plan.md), run by `plan-run` on
branch `feat/cli-consolidation`. Headings are frozen at ID + title; status, date and trigger
live on the body lines. The decisions are D-55 … D-62
([settled](../../decisions/01-settled.md); D-60 was open until the P02 gate, 2026-10-15) and
D-11's partly fired trigger.

## The pages of this block

A folder from the start: the plan lands six items over seven phases. [01](01-record.md) is the
record — every item, its acceptance line and its dated appends; an item whose measurements
outgrow it keeps its summary there and its numbers in a numbered child:
[02](02-demo-mesh-winding.md), the demo meshes' winding before and after (#55);
[03](03-mesh-sign-gwn.md), `mesh(sign="gwn")`'s equivalence, tests and ABI call (#56);
[04](04-mesh-node-bake.md), per-node `bake` against `dualc_boolean --bake` and the exact
graph — D-60's evidence — and the margin a baked node inherits (D-62) (#57);
[05](05-2d-profiles-and-lifts.md), the 2D profiles and lifts, the `rect` token, the
equivalence with `dualc_lift` and the GLSL half's parity (#58);
[06](06-slice-any-graph.md), `dualc_field --slice`, its refusals, its region and the
equivalence with `dualc_slice` (#59);
[07](07-docs-migration.md), the eight retiring pages' content re-homed under 11 and 12, every
migrated recipe run against its old tool (#60, P05).

| Item | Page | Status |
| --- | --- | --- |
| The owner's decisions of 2026-10-14 (D-55 … D-59) | [01](01-record.md#the-owners-decisions) | DONE 2026-10-14 (decided; the phases execute) |
| D-60 — `dualc_boolean`'s retirement, decided after P02 | [01](01-record.md#the-dualc_boolean-decision) | DONE 2026-10-15 — kept, on [04](04-mesh-node-bake.md)'s measurements |
| #55 Generated demo meshes wound outward (F1) | [01](01-record.md#55-generated-demo-meshes-wound-outward) | DONE 2026-10-14 — P01 T2; measured in [02](02-demo-mesh-winding.md) |
| #56 The mesh node takes `sign="gwn"` | [01](01-record.md#56-the-mesh-node-takes-signgwn) | DONE 2026-10-15 — P02 T1; measured in [03](03-mesh-sign-gwn.md) |
| #57 Per-node bake on the mesh nodes | [01](01-record.md#57-per-node-bake-on-the-mesh-nodes) | DONE 2026-10-15 — P02 T2; measured in [04](04-mesh-node-bake.md) |
| #58 2D profiles and `revolve` / `extrude` in the graph | [01](01-record.md#58-2d-profiles-and-revolve--extrude-in-the-graph) | DONE 2026-10-15 — P03 T1 (CPU) + T2 (GPU); measured in [05](05-2d-profiles-and-lifts.md) |
| #59 Slice any graph | [01](01-record.md#59-slice-any-graph) | DONE 2026-10-16 — P04 T1; measured in [06](06-slice-any-graph.md) |
| #60 Retire the eight tools | [01](01-record.md#60-retire-the-eight-tools) | PLANNED — P05 landed 2026-10-16 ([07](07-docs-migration.md)); P06 next |

---

← Back to the [Roadmap index](../README.md).
