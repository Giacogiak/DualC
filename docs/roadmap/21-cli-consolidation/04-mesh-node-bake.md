# CLI consolidation — per-node `bake`, measured

Part of [CLI consolidation](README.md) (block 21): the evidence behind
[#57](01-record.md#57-per-node-bake-on-the-mesh-nodes), landed 2026-10-15 by the plan-run unit
P02.2 on branch `feat/cli-consolidation`; the numbers
[D-60](01-record.md#the-dualc_boolean-decision) was decided on (`dualc_boolean` kept), and the
margin rule the owner picked after reading them (D-62).

## What changed

`mesh(…, bake=N[, band=W])` and `winding(…, bake=N)` replace the source with a `GridField` of
N³ samples. The rule lives once, in `examples/field_graph.{h,cpp}` (`checkNodeBake`,
`nodeBakeRegion`, `bakeMeshNode`, `bakeWindingNode`, `childBakeMargin`), and both the
builder and the GLSL codegen call it:

- **Region:** the node's own `bounds()` padded by 10 % of their largest extent **plus
  max(`band`, margin)** — the margin its ancestors hand down (§ The margin). A `winding`
  node's bounds are already padded (10 % of the diagonal), so it is padded twice; harmless.
- **Band** (mesh only): `MeshSource::bakeToGrid`'s exact-distance shell, `band` world units,
  default 3 bake cells (the library floors it at 1.5 cells), raised to margin + 3 cells under a
  margin. A `winding` node bakes the full lattice; `band` on it is an unknown parameter.
- **Validation:** `bake` an integer in [2, 512] (512³ floats = 512 MiB), `band` positive and
  only with `bake`; every error located on the node, before the mesh is loaded.
- **GPU:** `emitMesh` / `emitWinding` upload the node's own grid, so the texture is the CPU
  grid bit for bit and `--grid-res` does not apply to the node. Parity case `mesh-bake` (75):
  GPU vs the *built* CPU field, maxErr 2.7e-4; with the codegen made to re-bake over the trace
  box it fails at 5.85e-1 (3,168/4,096 over tolerance).

**Where the plan was wrong.** The plan put the region at the node's AABB + 10 % "without `k`".
Measured, that breaks every smooth op whose `k` exceeds the margin: outside its region a grid
clamps to the face value, which never grows, and the blend turns the clamp into material
(row *before D-62* of the smooth-union table: 3× the faces, max deviation 1.56 on a 2.7-wide
model). `dualc_boolean` avoids it by padding its region by `k`. The unit's first fix let an
explicit `band` widen the region and documented *band ≥ the parent's `k`*; at the P02 gate the
owner picked the margin below instead.

## The margin (decision (b))

Giacomo, 2026-10-15, of four options — (a) the documented `band ≥ k` only, (b) propagate `k`,
(c) a located error on a direct baked child, (d) a `GridField` that extrapolates past its
region: *"(b) Propagate k"*. Every node hands each child a margin (`childBakeMargin`; 0 at the
root):

| Parent | Margin handed to the child |
| --- | --- |
| `smooth-union` / `-intersection` / `-difference` | max(margin, `k`) — so the largest `k` above, not only the nearest: an outer blend reads the inner one's clamped values too |
| `offset`, `round` | margin + max(`r`, 0) |
| `onion` | margin + \|`thickness`\| |
| `displace` | margin + \|`amplitude`\| |
| `graded-onion`, `graded-offset` | base: margin + max(\|`t1`\|, \|`t2`\|); control: unchanged |
| `scale` | margin ÷ \|`s`\| (the child's own units) |
| everything else (hard booleans, `translate`, `rotate`, `transform`, `normalize`, …) | unchanged |

`offset` / `onion` are beyond the owner's words but are the same clamp: `offset(mesh(cube,
bake=32), r=0.3)` contoured to **1 face** and `onion(…, thickness=0.3)` to 588, against the
exact 9,804 and 10,392 (`--depth 5`), because 0.3 exceeds the 10 % pad. A baked node pads its
region by max(`band`, margin) and keeps its exact band ≥ margin + 3 cells — `dualc_boolean
--bake`'s rule (pad 10 % + `k`, band `k` + 3 cells) per node; an explicit larger `band` wins.
The GLSL walk tracks the same margins on its emit path, and a subtree that holds a baked node
joins its margin to its dedup key (one mesh under two margins is two textures).

Proofs: unit tests *childBakeMargin hands each child the distance its ancestors read* (the
table, op by op), *a baked child inherits its ancestors' margin* (sign agrees with the exact
field off the surface through a direct smooth child, `translate`, `scale(0.6)`, a nested
smooth op with the larger `k` outside, `offset` and `onion` of 0.3; with propagation switched
off the first case fails; the bare library composition still shows the trap), *an explicit
band larger than the margin wins*, and *GLSL codegen uploads a baked node's own grid* (six
nested graphs: texture == `bakeMeshNode(…, expected margin)` bit for bit). Parity case
`mesh-bake-smooth` (76): a baked cube under `translate` inside a `smooth-union`, GPU vs the
built CPU field, maxErr 5.7e-4; with the GLSL side forced to margin 0 it fails at 8.66e-1
(3,752/4,096 over tolerance).

## The measurements

Linux, GCC 15, Ninja, Release, `build/examples/`, run from `data/`; Intel i7-920 (4 cores /
8 threads), 22 GB. Wall: median of 3, peak RSS: max of 3, both `/usr/bin/time`. Faces: the
written OBJ. Deviation: `|valueAt|` of the exact graph (no bake) at every output vertex, max
and mean, by a scratch evaluator linked against `libdualc_examples_fieldgraph`. *Root box:*
`dualc_field` and `dualc_boolean` both auto-fit the octree to the field's bounds + 5 %, and a
baked field's bounds are its region, so a bake also coarsens the cells at a given depth; the
"… root box" rows pin `--bounds` to the exact run's or the tool's root to separate the two.
The smooth-union table was re-run after D-62 (the *before* row is the first run's); the two
hard-op tables are unchanged by it (hard ops hand down no margin; the graph's difference
output was `cmp`-identical before and after).
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
| `dualc_boolean --bake 64` | 0.65 | 29 | 3,468 | 0.00819 | 0.00195 |
| graph `mesh(bake=64)` ×2, **before D-62** (no margin) | 1.42 | 370 | 28,160 | **1.56** | **0.67** |
| graph `mesh(bake=64)` ×2, margin `k` handed down | 0.66 | 29 | 3,564 | 0.00851 | 0.00186 |
| … exact root box | 0.80 | 78 | 9,116 | 0.00363 | 0.000647 |
| … tool root box | 0.66 | 29 | 3,468 | 0.00673 | 0.00188 |
| exact graph | 1.35 | 58 | 9,116 | 0.00456 | 0.000882 |

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
time and memory. Under a smooth op it matches the tool once the margin is handed down — with
no margin it was wrong. `dualc_boolean`'s combined region is not reproduced: a node never sees
its sibling's box, only the margin its ancestors hand it. The owner kept the tool (D-60).

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
