# 5–6. Geometry-central integration and the DC tables

The seam between DualC and geometry-central (§ 5), and the indexing conventions the DC tables
in [`src/internal/dc_tables.cpp`](../../src/internal/dc_tables.cpp) encode (§ 6). Change any
convention below and every table in that file has to be re-derived in lockstep — which is the
argument for keeping them exactly as they are.

## 5. Geometry-central integration

DualC builds on geometry-central without re-exporting its types:

- `Vector3` is aliased: `using dualc::Vector3 = geometrycentral::Vector3` (in
  [`types.h`](../../include/dualc/types.h)). All DualC geometry — `BBox`, `Mat4`, Hermite
  data, output positions — is `double`.
- Input meshes come in as `geometrycentral::surface::SurfaceMesh&` +
  `VertexPositionGeometry&`, and are consumed by `MeshSource` → `internal::MeshBVH`.
  `packMesh` iterates `mesh.vertices()` / `mesh.faces()`, reads
  `geometry.inputVertexPositions[v]`, and computes per-vertex normals via
  `requireVertexNormals()` / `unrequireVertexNormals()` — geometry-central's corner-angle
  weighting of the unit face normals, not area weighting — packing all three into aligned
  float buffers for nanort. Note this means "interpolated normals" always means *recomputed*
  normals — an authored crease in an input file cannot round-trip.
- Output meshes are constructed via `makeSurfaceMeshAndGeometry(polygons, positions)`; the
  per-vertex normals are returned alongside as a plain `std::vector<Vector3>`.

Geometry-central is **pinned to one upstream commit** (`DUALC_GC_GIT_TAG` in `CMakeLists.txt`)
and resolved there in three rungs — a `geometry-central` target an enclosing project already defines, a local tree named by
`-DDUALC_GC_DIR`, else `FetchContent` of the pinned commit into the build tree — and linked by
the DualC target's `target_link_libraries(... PUBLIC geometry-central)`, which brings Eigen and
happly into scope transitively. nanort and nanoflann are private to geometry-central at the
pinned version; the BVH uses DualC's own vendored nanort header ([§ 8](06-parameters-and-vendoring.md#8-vendored-third-party-code)).
Geometry-central is never vendored (`README.md`, `AGENTS.md`).

## 6. Conventions

### Cube corner indexing

Corner `c ∈ {0, 1, …, 7}` maps to `(x, y, z) ∈ {0, 1}³` via the bit decomposition
`c = (z << 2) | (y << 1) | x`.

| `c` | x | y | z |
| --- | - | - | - |
| 0   | 0 | 0 | 0 |
| 1   | 1 | 0 | 0 |
| 2   | 0 | 1 | 0 |
| 3   | 1 | 1 | 0 |
| 4   | 0 | 0 | 1 |
| 5   | 1 | 0 | 1 |
| 6   | 0 | 1 | 1 |
| 7   | 1 | 1 | 1 |

This is `dualc::tables::kCornerOffset`.

### Cube edge indexing

12 edges, axis-major (4 along each axis):

| edge | corners | axis |
| ---- | ------- | ---- |
| 0    | 0 → 1   | X    |
| 1    | 2 → 3   | X    |
| 2    | 4 → 5   | X    |
| 3    | 6 → 7   | X    |
| 4    | 0 → 2   | Y    |
| 5    | 1 → 3   | Y    |
| 6    | 4 → 6   | Y    |
| 7    | 5 → 7   | Y    |
| 8    | 0 → 4   | Z    |
| 9    | 1 → 5   | Z    |
| 10   | 2 → 6   | Z    |
| 11   | 3 → 7   | Z    |

`kEdgeEndpoints[12][2]` and `kEdgeAxis[12]`. Endpoint convention: low coord first.

### Octree child indexing

`HermiteNode::children[c]` for `c ∈ {0, …, 7}` is the child whose corner-0 sits at the
parent's corner `c`. Equivalently: the bit decomposition above also identifies child
positions within their parent. `internal::childBounds` splits each axis at the parent's
midpoint.

### The DC descent tables

Six tables drive the recursion of [§ 4.1](03-contourer-recursion.md#41-cellproc--faceproc--edgeproc):

| Table | Shape | Used by |
| --- | --- | --- |
| `kCellProcFaceMask` | 12 × `(childA, childB, axis)` | `cellProc` — the 12 internal faces |
| `kCellProcEdgeMask` | 6 × `(c0, c1, c2, c3, axis)` | `cellProc` — the 6 internal edges |
| `kFaceProcFaceMask` | 3 axes × 4 × `(childOfA, childOfB)` | `faceProc` — the 4 sub-faces |
| `kFaceProcEdgeMask` | 3 axes × 4 × `FaceProcEdgeCall` | `faceProc` — the 4 internal edges of the face |
| `kEdgeProcEdgeMask` | 3 axes × 2 × 4 children | `edgeProc` — the 2 sub-edges |
| `kProcessEdgeMask` | 3 axes × 4 local edge indices | `edgeProc` terminal + `emitQuadAtEdge` |

### The four-cell ordering around an edge

`edgeProc`'s four cells are always listed as `(P1=hi, P2=hi)`, `(P1=lo, P2=hi)`,
`(P1=lo, P2=lo)`, `(P1=hi, P2=lo)`, where `(P1, P2)` are the two axes perpendicular to the
edge axis: `(Y, Z)` for X, `(X, Z)` for Y, `(X, Y)` for Z.

`kProcessEdgeMask` gives each of those four cells the local edge index that corresponds to
the shared absolute edge:

```
axis=X: {0, 1, 3, 2}
axis=Y: {4, 5, 7, 6}
axis=Z: {8, 9, 11, 10}
```

and it is arranged so that endpoint 0 of the local edge always sits at the low end of the
absolute edge along its axis. `kQuadCCWPlus` then reorders those four cells into
CCW-from-`+axis` winding — its Y row is reversed, and the reason is stated once, with the
table, in [§ 4.3](03-contourer-recursion.md#43-emission). The contourer reverses the
triangles when the low endpoint is outside, so the outward normal always tracks the surface
gradient.

---

← Back to the [design index](README.md) · the [docs index](../README.md)
