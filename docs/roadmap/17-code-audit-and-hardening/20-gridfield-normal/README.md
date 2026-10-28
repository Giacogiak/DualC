# #34 — a continuous `GridField` normal

The record of the last open bullet of
[04 #34](../04-engineering-quality.md#34-api-hygiene-batch) (audit finding E12): the
Hermite normal a baked grid hands the QEF. It builds on P03's `trueGradientAt`
([19](../19-gradient-contract.md#the-true-gradient-path)). P04 and P04b were units of the
[hygiene + editor run](../../../raw/2026-10-19-hygiene-and-editor-plan.md), on branch
`feat/hygiene-and-editor`, 2026-10-20 and 10-21. A page until 2026-10-28, a folder
since: those two units stand verbatim on [01](01-stencils-measured.md), the
stencil that shipped is on [02](02-minmod-shipped.md).

**Status: #34 DONE 2026-10-28** — `gradientAt` is the normalised trilinear blend of per-node
*minmod* gradients, continuous everywhere, the sharp CAD pair's mean deviation accepted at
+18 % by the owner ([D-76](../../../decisions/01-settled.md)); the hidden continuity test is
live again.

## The pages of this folder

- [`01-stencils-measured.md`](01-stencils-measured.md) — P04 (2026-10-20): the central node
  blend, continuous, measured on four cases, the CAD pair +35 %; P04b (2026-10-21): minmod,
  ratio switches and crease masks measured, none kept the pair's mean, the cell gradient
  restored, E12 open.
- [`02-minmod-shipped.md`](02-minmod-shipped.md) — 2026-10-28: the owner's pick, minmod,
  re-implemented and re-measured on the four cases plus an oversampled one with the new
  `dualc_mesh_deviation` tool; the tests moved; #34 DONE.

## Entries

- **2026-10-20** — P04.1, the central blend: [01 § What changed](01-stencils-measured.md#what-changed).
- **2026-10-21** — P04b.1, the limited stencils, the cell gradient restored:
  [01 § P04b](01-stencils-measured.md#p04b--a-limited-node-stencil-measured).
- **2026-10-28** — the measurement tool, then minmod shipped: [02](02-minmod-shipped.md).

---

← Back to the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
