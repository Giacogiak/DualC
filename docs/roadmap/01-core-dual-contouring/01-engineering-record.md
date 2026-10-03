# Engineering record (end-of-v2 handoff): decisions, pipeline, quality bars, conventions

Part of [01 — Core dual contouring](README.md). The design-level hand-off as it stood at
the end of v2: the locked architectural decisions (§ 2), the pipeline (§ 3), the quality
bars (§ 5) and the conventions (§ 7). The bug catalogue (§ 4) is
[02](02-bug-catalogue.md); the current design walkthrough is
[`docs/design/`](../../design/README.md).

## 2. Architectural decisions (locked in early, still hold)

| Decision | Rationale |
| --- | --- |
| **Two-stage pipeline** (sampler + contourer, communicating only via `HermiteOctree`) | Each side reusable in isolation; the Hermite octree is a clean abstraction that other contourers / samplers can plug into. |
| **Octree only** (no uniform-grid path) | Simpler v0 surface area; adaptive refinement happens via the octree's own shape. Uniform grid is a special case we can always add later. |
| **Geometry-central as a sibling checkout** (`add_subdirectory("../geometry-central" ...)`) | No FetchContent, no submodules, no `find_package`. Repo stays self-contained on the user's box. Geometry-central transitively pulls Eigen, nanort, nanoflann, happly — DualC needs no extra `target_link_libraries` for those. *Superseded 2026-09-21 by D-42 ([20 #47](../20-public-delivery.md#47-pin-geometry-central-to-upstream-own-nanort-self-bootstrapping-clone)): pinned upstream commit fetched at configure, nanort vendored.* |
| **Permissive-only vendoring** (Unlicense / PD code OK; LGPL / GPL never copied) | User preference for keeping DualC permissive. Where the cleanest reference was LGPL (Tao Ju's octree DC, Nick Gildea's octree.cpp), we re-derived from cube combinatorics rather than copy. Only vendored files: `src/internal/{qef,svd}.{h,cpp}` from [DualContouringSample](https://github.com/nickgildea/DualContouringSample), byte-identical, Unlicense headers preserved. Documented in [THIRD_PARTY.md](../../../THIRD_PARTY.md). |
| **MIT for DualC's own code** | Pairs with the vendoring rule. |
| **Catch2 v3 for tests, fetched via FetchContent inside `tests/CMakeLists.txt`** | Users build out-of-the-box with no extra installs. Test target only builds when `DUALC_BUILD_TESTS=ON`. |
| **C++17, `target_compile_features(... cxx_std_17)`** | Sufficient for `std::optional`, structured bindings, `std::clamp` — no need to chase newer standards. |
| **Public API in `include/dualc/`, internals in `src/internal/`** | Strict separation; public-API headers don't reference internal types like `svd::QefSolver` or `nanort::*`. |

## 3. Pipeline as it stands today

### 3.1 Sampler ([src/sampler.cpp](../../../src/sampler.cpp))

1. Auto-fit the root AABB around the input (or use `params.rootBounds` if given), padded by `params.padFraction`.
2. Build a `MeshBVH` (nanort `BVHAccel<float, TriangleMesh, TriangleSAHPred, TriangleIntersector>`) over the input's packed positions + indices. Also store per-vertex area-weighted normals (via geometry-central's `requireVertexNormals()`).
3. Build a `SignOracle` over the BVH (3-ray majority-vote stabbing parity).
4. Recursive `buildNode`:
   - Below `minDepth`: always recurse.
   - At/below `maxDepth`: leaf. If the cell's 8 corners straddle the surface, populate `HermiteLeafData` (corner signs + 12 outer-edge Hermite samples via `segmentFirstHit`).
   - Otherwise: refine iff `MeshBVH::cellOverlapsAABB(node.bounds)` returns true. Cells with no BVH overlap become Hermite-less leaves at whatever depth refinement stopped.

### 3.2 Optional simplify ([src/contourer.cpp](../../../src/contourer.cpp) `simplifyHermiteOctree`)

Bottom-up depth-first traversal. For each internal node whose 8 children are all surface
leaves with `HermiteLeafData`:
1. Topology-safe tests (all three must pass):
   - **No internal-edge sign change** — none of the 6 edges shared by 4 sibling children crosses the surface.
   - **No outer-edge double-crossing** — for any of the 12 outer parent edges, if both endpoints have the same sign, the mid-edge sign must match.
   - **No saddle face** — none of the 6 outer parent faces has 4 sign changes around its boundary cycle.
2. Build the merged QEF by accumulating all 8 children's `HermiteLeafData` edge crossings.
3. If the merged QEF's **geometric energy at the solved vertex** ≤ `simplificationError`, replace the node with a single pseudo-leaf carrying the merged `HermiteLeafData`; free the children. Otherwise leave intact. A fourth gate then re-runs `partitionCubeEdges` on the merged data and refuses if the result is more than one surface component.
4. The public entry takes `const ContourerParams&`, so the collapse solve is regularised with the caller's `qefRegularization` exactly as leaf solves are. The legacy `simplifyHermiteOctree(octree, double)` overload forwards to it.

Multi-level cascade is automatic because the recursion is post-order: by the time a
parent is evaluated, its children may already be pseudo-leaves from earlier collapses.

### 3.3 Contourer ([src/contourer.cpp](../../../src/contourer.cpp))

Standard Ju/Schaefer/Warren `cellProc / faceProc / edgeProc` recursion driven by the
descent tables in [src/internal/dc_tables.cpp](../../../src/internal/dc_tables.cpp)
(`kCellProcFaceMask`, `kCellProcEdgeMask`, `kFaceProcFaceMask`, `kFaceProcEdgeMask`,
`kEdgeProcEdgeMask`, `kProcessEdgeMask`). Per-leaf QEF vertices are solved lazily inside
`leafVertexIndex` and stored in a `std::unordered_map<const HermiteNode*, int>`. Output
also carries one normal per vertex — the unit-normalised average of that cell's QEF
input normals.

Key invariant in the terminal `edgeProc` case: when all four cells around an absolute
edge are leaves at potentially varying depths, **the sign data comes from the FINEST
cell** (not the coarsest). Its local edge per `kProcessEdgeMask` IS the absolute edge —
a coarser cell's local edge would sit on the cell's outer surface, not on the interior
fine edge. Getting this backwards was the root of one of the major bugs (see §4).

When two of the four `edgeProc` slots resolve to the same pseudo-leaf, the quad
collapses to a triangle. `emitQuadAtEdge` deduplicates the polygon vertices in CCW order
and emits 1 triangle for 3 unique verts or 2 for 4 — skipping the degenerate case
entirely is what was producing holes early on.

## 5. Current quality bars (depth 7 unless stated)

| Mesh | Mode | Verts | Tris | Boundary edges | Non-manifold | Euler χ |
| --- | --- | --- | --- | --- | --- | --- |
| cube | default | 82,136 | 164,268 | 0 | 0 | 2 |
| cube | `--sharp` | 82,136 | 164,268 | 0 | 0 | 2 |
| cube | `--collapse 100` | 81,440 | 162,876 | 0 | 0 | 2 |
| molde | default (MDC on) | 38,116 | 76,228 | 0 | 0 | 2 |
| molde | `--collapse 100` (MDC on) | 34,328 | 68,652 | 0 | 0 | 2 |
| molde | `--sharp` (MDC on) | 38,116 | 76,228 | 0 | 0 | 2 |
| molde | `--no-manifold` (pre-MDC) | 38,113 | 76,228 | 0 | 3 | 2 |

Manifold Dual Contouring (default-on, `--no-manifold` opt-out) eliminates the 3
intrinsic non-manifold pinch edges that single-vertex DC left on molde. Each pinch cell
now emits 2 vertices (one per surface component) instead of one shared QEF vertex; same
face count, +3 vertices, fully manifold closed surface.

Runtimes on a modern desktop: cube/molde @ depth 7 ≈ 4-9 s; depth 5 ≈ 1-2 s; depth 9
projected ≈ 2 min. Multi-threading is the next perf chapter ([Tier-1 item 3](README.md#3-multi-threading-the-sampler--contourer)).

## 7. Conventions worth knowing before you touch any DC code

- **Cube corner indexing:** `c = (z << 2) | (y << 1) | x`. Corner 0 = (0,0,0), corner 7 = (1,1,1).
- **Cube edge indexing:** 12 edges, axis-major. Edges 0-3 are X-aligned, 4-7 Y-aligned, 8-11 Z-aligned. Endpoint convention in `kEdgeEndpoints[12][2]` is `(low, high)` along the edge's axis.
- **Octree child indexing:** matches the corner bit decomposition — `HermiteNode::children[c]` is the child whose own corner 0 sits at the parent's corner `c`.
- **edgeProc 4-cell order:** `(P1=hi, P2=hi)`, `(P1=lo, P2=hi)`, `(P1=lo, P2=lo)`, `(P1=hi, P2=lo)` where `P1, P2` are the perpendicular axes to the edge axis. Used consistently by `kCellProcEdgeMask`, `kEdgeProcEdgeMask`, `kProcessEdgeMask`, and the `kQuadCCWPlus` winding tables.
- **Outward-normal direction in quad emission:** when corner 0 of the absolute edge (low endpoint along the edge axis) is INSIDE, the outward normal of the emitted quad points in the **+axis** direction. The contourer's `emitQuadAtEdge` flips winding when corner 0 is outside.

If you change any of these conventions, every table in
[src/internal/dc_tables.cpp](../../../src/internal/dc_tables.cpp) needs to be re-derived
in lockstep. Easier to keep them.

---

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
