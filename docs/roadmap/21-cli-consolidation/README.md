# CLI consolidation — one framework, `dualc_field` + `dualc_field_view`

The record of folding DualC's side CLIs into the field-graph framework: every capability a
side tool alone holds moves into the graph — and so within reach of the C ABI, which is the
graph and nothing else — and the tool retires. Born 2026-10-14 from the owner's answers to the
[CLI tools screening](../../raw/2026-10-14-cli-tools-screening.md) (the evidence: a verdict per
tool, eight byte-identical tool-vs-graph runs, three side findings); worked through the
[consolidation plan](../../raw/2026-10-14-cli-consolidation-plan.md), run by `plan-run` on
branch `feat/cli-consolidation`. Headings are frozen at ID + title; status, date and trigger
live on the body lines. The decisions are D-55 … D-59
([settled](../../decisions/01-settled.md)), D-60 (open, [index](../../decisions/README.md)) and
D-11's partly fired trigger.

## The pages of this block

A folder from the start: the plan lands six items over seven phases. One record page for now;
an item that outgrows it moves to a numbered child of its own.

| Item | Page | Status |
| --- | --- | --- |
| The owner's decisions of 2026-10-14 (D-55 … D-59) | [01](01-record.md#the-owners-decisions) | DONE 2026-10-14 (decided; the phases execute) |
| D-60 — `dualc_boolean`'s retirement, decided after P02 | [01](01-record.md#the-dualc_boolean-decision) | DEFERRED — trigger: P02 T2's measurements, at the approve gate after P02 |
| #55 Generated demo meshes wound outward (F1) | [01](01-record.md#55-generated-demo-meshes-wound-outward) | NEXT — P01 T2 |
| #56 The mesh node takes `sign="gwn"` | [01](01-record.md#56-the-mesh-node-takes-signgwn) | PLANNED — P02 T1 |
| #57 Per-node bake on the mesh nodes | [01](01-record.md#57-per-node-bake-on-the-mesh-nodes) | PLANNED — P02 T2 |
| #58 2D profiles and `revolve` / `extrude` in the graph | [01](01-record.md#58-2d-profiles-and-revolve--extrude-in-the-graph) | PLANNED — P03 |
| #59 Slice any graph | [01](01-record.md#59-slice-any-graph) | PLANNED — P04 |
| #60 Retire the eight tools | [01](01-record.md#60-retire-the-eight-tools) | PLANNED — P05, P06 |

---

← Back to the [Roadmap index](../README.md).
