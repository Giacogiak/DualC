# DualC — Design

The compiled description of the engine as it is: *what the code does and why*, rewritten
freely whenever the code changes and dated nowhere. The record of how it got this way — what
changed when, and which experiment motivated it — is [`docs/roadmap/`](../roadmap/README.md);
how to drive the tools is [`docs/command_reference/`](../command_reference/README.md). The
section labels (§ 3.2, § 4.6 …) are stable names, kept so that citations in the study pack
and the roadmap keep resolving; they are not a reading order.

DualC is a small C++17 library that converts triangle meshes **and arbitrary real-valued
implicit fields** into clean, dual-contoured triangle meshes. It is built on top of
[geometry-central](https://geometry-central.net) and exposes two composable modules — a
**sampler** and a **contourer** — plus an optional octree-simplification pass between them and
the `dualContourField` / `dualContourMesh` conveniences that run the whole chain. For build
instructions see the [root README](../../README.md).

## 1. Pipeline at a glance

```
                    ImplicitField  (a mesh is one kind of field:
                          │         MeshSource wraps SurfaceMesh + BVH)
                          │
                  dualContourField  ─────────────────────────┐
                  dualContourMesh   (mesh sampler + same tail)
                          │                                   │
       ┌──────────────────┴───────────┬───────────────────────┴──────────┐
       ▼                              ▼                                  ▼
 ┌────────────┐  HermiteOctree  ┌──────────────────┐  HermiteOctree  ┌─────────────┐
 │  sampler   │ ──────────────▶ │ simplifyHermite  │ ──────────────▶ │  contourer  │
 │            │                 │ Octree (optional)│                 │             │
 │ field →    │                 │                  │                 │ octree →    │
 │ adaptive   │                 │ bottom-up cell   │                 │ mesh + geo  │
 │ octree +   │                 │ collapse driven  │                 │ + per-vertex│
 │ Hermite    │                 │ by simplifica-   │                 │ normals     │
 │ data       │                 │ tionError        │                 │             │
 └─────┬──────┘                 └────────┬─────────┘                 └──────┬──────┘
       │                                 │                                  │
   reads:                            mutates:                           writes:
   - ImplicitField::cellOverlaps     - octree in place, merging       - SurfaceMesh
   - ImplicitField::isInside           8 surface leaves into a        - VertexPositionGeometry
   - ImplicitField::edgeHit            coarser pseudo-leaf when       - std::vector<Vector3>
   - ImplicitField::closest-           topology + QEF energy            (per-vertex normals)
     SurfacePoint (miss path)          allow it                      via Ju/Schaefer/Warren
                                     - skipped entirely when           cellProc/faceProc/
                                       simplificationError <= 0        edgeProc recursion
```

The stages are decoupled: the sampler is field-aware but contour-agnostic; the contourer is
contour-aware and knows nothing about where the Hermite data came from. A `HermiteOctree` is
the only thing that crosses the boundary.

The collapse pass is what makes the tree genuinely mixed-depth. The sampler itself produces
a tree that is uniform-depth *wherever it carries data* — every surface-touching cell is
refined to `maxDepth` — and adaptivity is added afterwards, in `simplifyHermiteOctree`, where
the QEF error needed to judge a merge is already available. The contourer handles mixed depth
natively ([§ 4.2](03-contourer-recursion.md#42-the-two-mixed-depth-devices)), so the pass is
genuinely optional.

`dualContourField` composes all three ([`src/pipeline.cpp`](../../src/pipeline.cpp)):

```cpp
HermiteOctree octree =
    sampleFieldToHermiteOctree(field, samplerParams, diag, cancel, progress);
return simplifyAndContour(octree, contourerParams, diag, cancel, progress);
                                        // collapse (if enabled), then contour
```

`dualContourMesh` shares that same tail but builds its octree through
`sampleMeshToHermiteOctree`, which is the library's **single** `MeshSource` construction site.
That is what guarantees `SamplerParams::signMethod` reaches the mesh path;
`tests/test_pipeline.cpp` asserts that `dualContourMesh` and the explicit
`sampleMeshToHermiteOctree` + `contourHermiteOctree` chain agree, by element counts, for
every `SignMethod`. The
defect that made the single site a rule is
[01 § 4.9](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch).

## 2. The data contract: `HermiteOctree`

The sampler produces and the contourer consumes a single data structure: an octree whose
leaves carry **Hermite data** (sign + edge crossings + edge-crossing normals).

Defined in [`include/dualc/hermite_octree.h`](../../include/dualc/hermite_octree.h):

```cpp
struct HermiteEdge {
  Vector3 position{0.0, 0.0, 0.0};   // crossing point in world space
  Vector3 normal{0.0, 0.0, 0.0};     // unit surface normal at the crossing
  bool    hasCrossing = false;
};

struct HermiteLeafData {
  std::array<bool, 8>         cornerInside{};   // sign per cube corner
  std::array<HermiteEdge, 12> edges{};          // 12 cube edges
};

struct HermiteNode {
  BBox bounds{};
  int  depth  = 0;
  bool isLeaf = true;
  std::unique_ptr<HermiteLeafData> leaf{};             // only on surface leaves
  std::array<std::unique_ptr<HermiteNode>, 8> children{};
};
```

A leaf cell that has a surface crossing carries a fully-populated `HermiteLeafData`. Leaves
entirely inside or entirely outside the surface have `leaf == nullptr` (they are
"Hermite-less" leaves at whatever depth refinement stopped). Internal nodes have eight
children and no leaf payload.

Two things about this contract are load-bearing:

- **The leaf stores signs, not values.** `cornerInside` is `std::array<bool, 8>`. Corner
  SDF values are deliberately not part of the contract, which keeps the sampler/contourer
  interface sign-based and lets non-metric sources (a generalized-winding-number field)
  participate. The cost is that Schaefer's asymptotic decider for saddle faces is
  unavailable to the contourer ([§ 4.5](04-qef-manifold-collapse.md#45-manifold-dual-contouring)).
- **`hasCrossing[e]` is exactly "the two endpoint signs of edge `e` differ".**
  `populateLeaf` sets it on *every* branch of its fallback ladder
  ([§ 3.3](01-sampler.md#33-refinement-and-edge-crossing-capture)), and `tryCollapse`
  preserves it when it builds a merged leaf
  ([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)). Both
  `partitionCubeEdges` and the emission code depend on this equivalence.

After the collapse pass, "leaf" does not imply "at `maxDepth`": an internal node can be
turned into a **pseudo-leaf** carrying merged Hermite data at a shallower depth.
`HermiteOctree` is move-only, and exposes `root()`, `leafCount()` and `nodeCount()`.

## 10.2 Data flow on the field path

```
   field tree (FieldPtr)
        │
        ▼
  sampleFieldToHermiteOctree   ── resolves the root box (§ 3.1: SamplerParams::rootBounds,
        │                         else a padded field.bounds()), then runs the buildNode
        │                         recursion of § 3, driven by field.cellOverlaps / isInside /
        ▼                         edgeHit / closestSurfacePoint.
   HermiteOctree
        │
        ├─ simplifyHermiteOctree (optional, § 4.6)
        ▼
   contourHermiteOctree         ── field-agnostic (§ 4)
        │
        ▼
   mesh + geometry + normals
```

`dualContourField(field, samplerParams, contourerParams)` runs the whole chain;
`dualContourMesh` builds the `MeshSource` through `sampleMeshToHermiteOctree` and then shares
the same collapse-and-contour tail. The mesh path is the field path with a `MeshSource` built
for you, in one place (§ 1).

## The pages of this layer

| Page | Labels | Holds |
| --- | --- | --- |
| [01-sampler.md](01-sampler.md) | § 3, § 3.1–3.3 | root bounds, the parallel build, refinement, `edgeHit` per field type, the miss ladder |
| [02-sign-oracles.md](02-sign-oracles.md) | § 3.4 | the three `SignMethod` oracles, their cost and failure modes |
| [03-contourer-recursion.md](03-contourer-recursion.md) | § 4, § 4.1–4.3 | `cellProc`/`faceProc`/`edgeProc`, the two mixed-depth devices, emission and winding |
| [04-qef-manifold-collapse.md](04-qef-manifold-collapse.md) | § 4.4–4.6 | the per-cell QEF, manifold DC, the collapse pass and its gates |
| [05-conventions-and-tables.md](05-conventions-and-tables.md) | § 5, § 6 | geometry-central integration and the build rules that hold the library boundary; corner/edge/child indexing, the descent tables, the four-cell order |
| [06-parameters-and-vendoring.md](06-parameters-and-vendoring.md) | § 7, § 8 | `SamplerParams` / `ContourerParams` field by field; the vendored QEF/SVD and its one local change |
| [07-limitations.md](07-limitations.md) | § 9 | the standing limitations, each with its fix and its roadmap item |
| [08-implicit-field-layer.md](08-implicit-field-layer.md) | § 10, § 10.1, 10.3–10.5 | `ImplicitField`, sharp features, the shape of the field library, unbounded fields |
| [09-conventions.md](09-conventions.md) | — | project-wide conventions: units and frames, export-format dispatch and the `.part` temp-then-rename, PowerShell quoting, keyboard-layout independence, primitive parameter forms, testing conventions |
| [10-invariants-and-tolerances.md](10-invariants-and-tolerances.md) | — | the output invariants, the ≥ 2–3 cells rule, `mix`, the `BBox` sentinels, `--tile-depth`'s range, the cancellation and progress hooks, every numeric constant |
| [11-glossary.md](11-glossary.md) | — | one line per term, and the ID vocabulary (`#N`, Tier 1 item, § A–G, build / pillar) |

## Things we do not do

Standing decisions that shape the code and the docs; each is a one-liner here, a row in the
[settled decisions](../decisions/01-settled.md) and a record in the roadmap.

- **geometry-central is consumed pinned, never vendored**: one upstream commit, fetched at
  configure, or a local tree via `-DDUALC_GC_DIR`
  ([§ 5](05-conventions-and-tables.md#5-geometry-central-integration)); the only vendored
  sources are the public-domain QEF/SVD pair and the MIT nanort header the BVH is written
  against ([§ 8](06-parameters-and-vendoring.md#8-vendored-third-party-code)), and LGPL/GPL
  code is read, never copied ([`THIRD_PARTY.md`](../../THIRD_PARTY.md)).
- **No mesh I/O and no mesh transforms in the library** — the host's job
  ([09](09-conventions.md#units-and-frames)).
- **No line numbers in this layer.** A design page names a file and a symbol; a line pin
  drifts with the next edit.
- **No renumbering, no restated counts, no rewritten headings**: IDs are assigned at birth
  and a count lives only with its owner
  ([`docs/README.md` § Conventions](../README.md#conventions)).
- **No search index to keep in sync** — the folder READMEs plus `rg` are the search layer
  ([`docs/README.md` § How to find things](../README.md#how-to-find-things)).
- **No reflow of `docs/raw/`** and no edit to a study file; a correction is a new dated file
  or an errata row beside it ([`docs/raw/README.md`](../raw/README.md)).
