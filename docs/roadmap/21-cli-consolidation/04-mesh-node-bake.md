# CLI consolidation — per-node `bake`, measured

Part of [CLI consolidation](README.md) (block 21): the evidence behind
[#57](01-record.md#57-per-node-bake-on-the-mesh-nodes), landed 2026-10-15 by the plan-run unit
P02.2 on branch `feat/cli-consolidation`, and the numbers
[D-60](01-record.md#the-dualc_boolean-decision) — `dualc_boolean`'s retirement — is decided on.

## What changed

`mesh(…, bake=N[, band=W])` and `winding(…, bake=N)` replace the source with a `GridField` of
N³ samples. The rule lives once, in `examples/field_graph.{h,cpp}` (`checkNodeBake`,
`nodeBakeRegion`, `bakeMeshNode`, `bakeWindingNode`), and both the builder and the GLSL codegen
call it:

- **Region:** the node's own `bounds()` padded by 10 % of their largest extent **plus `band`**
  when one is given. A `winding` node's bounds are already padded (10 % of the diagonal), so it
  is padded twice; harmless.
- **Band** (mesh only): `MeshSource::bakeToGrid`'s exact-distance shell, `band` world units,
  default 3 bake cells (the library floors it at 1.5 cells). A `winding` node bakes the full
  lattice; `band` on it is an unknown parameter.
- **Validation:** `bake` an integer in [2, 512] (512³ floats = 512 MiB), `band` positive and
  only with `bake`; every error located on the node, before the mesh is loaded.
- **GPU:** `emitMesh` / `emitWinding` upload the node's own grid, so the texture is the CPU
  grid bit for bit and `--grid-res` does not apply to the node. Parity case `mesh-bake` (75):
  GPU vs the *built* CPU field, maxErr 2.7e-4; with the codegen made to re-bake over the trace
  box it fails at 5.85e-1 (3,168/4,096 over tolerance).

**Where the plan was wrong.** The plan put the region at the node's AABB + 10 % "without `k`".
Measured, that breaks every smooth op whose `k` exceeds the margin: outside its region a grid
clamps to the face value, which never grows, and the blend turns the clamp into material
(row *graph `mesh(bake)`* of the smooth-union table: 3× the faces, max deviation 1.56 on a
2.7-wide model). `dualc_boolean` avoids it by padding its region by `k`. The fix keeps the
plan's knob: `band` also widens the region, so the docs' rule, *band ≥ the parent's `k`*,
covers both the exact shell and the clamp margin. Test: *a baked child under a smooth op
needs band >= k*.

## The measurements

Linux, GCC 15, Ninja, Release, `build/examples/`, run from `data/`; Intel i7-920 (4 cores /
8 threads), 22 GB. Wall: median of 3, peak RSS: max of 3, both `/usr/bin/time`. Faces: the
written OBJ. Deviation: `|valueAt|` of the exact graph (no bake) at every output vertex, max
and mean, by a scratch evaluator linked against `libdualc_examples_fieldgraph`. *Root box:*
`dualc_field` and `dualc_boolean` both auto-fit the octree to the field's bounds + 5 %, and a
baked field's bounds are its region, so a bake also coarsens the cells at a given depth; the
"… root box" rows pin `--bounds` to the exact run's or the tool's root to separate the two.
In every case `dualc_boolean` without `--bake` and the exact graph wrote byte-identical OBJ.

`difference sphere.obj cube.obj --depth 6`, bake 64 (model extent 2):

| Run | Wall s | RSS MB | Faces | max dev | mean dev |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dualc_boolean --bake 64` | 0.60 | 54 | 20,280 | 0.00655 | 0.000377 |
| graph `mesh(bake=64)` ×2 | 0.81 | 81 | 29,496 | 0.0052 | 0.000258 |
| … exact root box | 1.03 | 105 | 41,640 | 0.00123 | 0.000141 |
| … tool root box | 0.66 | 57 | 20,280 | 0.00713 | 0.000374 |
| exact graph | 2.19 | 136 | 41,736 | 0.00139 | 0.000167 |

`smooth-union torus.obj cube.obj --k 1 --depth 6`, bake 64 (model extent 2.7):

| Run | Wall s | RSS MB | Faces | max dev | mean dev |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dualc_boolean --bake 64` | 0.73 | 29 | 3,468 | 0.00819 | 0.00195 |
| graph `mesh(bake=64)` ×2, default band | 1.42 | 370 | 28,160 | **1.56** | **0.67** |
| graph `mesh(bake=64,band=1)` ×2 | 0.59 | 29 | 3,564 | 0.00851 | 0.00186 |
| … exact root box | 0.76 | 78 | 9,116 | 0.00363 | 0.000647 |
| … tool root box | 0.61 | 29 | 3,468 | 0.00673 | 0.00188 |
| exact graph | 1.69 | 58 | 9,116 | 0.00456 | 0.000882 |

`difference opA.obj opB.obj --depth 7`, bake 128 (local only, not in CI; 8,387 + 50 faces,
model ≈ 550 mm). Page 03 names no depth for this pair: depth 7 is the tool's default, 128 its
recipe B8's bake.

| Run | Wall s | RSS MB | Faces | max dev | mean dev |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dualc_boolean --bake 128` | 3.36 | 250 | 45,688 | 3.11 | 0.0331 |
| graph `mesh(bake=128)` ×2 | 6.16 | 496 | 108,660 | 1.71 | 0.0129 |
| … exact root box | 7.19 | 630 | 162,620 | 1.47 | 0.0138 |
| … tool root box | 4.87 | 248 | 45,688 | 3.11 | 0.0333 |
| exact graph | 25.79 | 1,988 | 162,612 | 1.32 | 0.00806 |

## The reading

Per-node `bake` keeps the speed advantage: on the large pair 6.2 s and 0.5 GB against the
exact 25.8 s and 2.0 GB, and at the same octree as the exact run it is still 3.6× faster
(7.2 s) at a mean deviation 1.7× the exact one. Against the tool's combined-region bake it
loses no accuracy: under the tool's own root box it writes the tool's face count to the face
and its deviation (3.11 / 0.0333 vs 3.11 / 0.0331), at 1.5× the tool's wall (4.9 s vs 3.4 s —
each node's grid is denser than a shared one). By default it is finer than the tool, because
its root box is the operand's own: 2.4× the faces, 2.6× lower mean deviation, about twice the
time and memory. The one trap is smooth ops: with the default band the result is wrong, with
`band=k` it matches the tool. `dualc_boolean`'s combined region is not reproduced — a node sees
neither its sibling's box nor its parent's `k`; `band` is the parent's `k` passed by hand.

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
