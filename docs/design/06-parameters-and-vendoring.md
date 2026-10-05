# 7–8. Configuration parameters and the vendored solver

The two parameter structs, field by field, with the defaults the headers declare (§ 7), and
the one piece of third-party code in the tree (§ 8). The CLI flags that set these fields, and
their own defaults, are the [command reference](../command_reference/README.md); which value
is rejected at construction is [17/08](../roadmap/17-code-audit-and-hardening/08-argument-validation.md).

## 7. Configuration parameters

### `SamplerParams` — [`include/dualc/sampler.h`](../../include/dualc/sampler.h)

| Field | Default | What it does |
| --- | --- | --- |
| `maxDepth` | 7 | Octree depth at which surface-touching cells become leaves and get Hermite data. Checked before `minDepth`, so it always wins. Cell side length = `rootExtent / 2^maxDepth`. |
| `minDepth` | 3 | Refine unconditionally to at least this depth. Defends against early termination on L-shaped / non-convex inputs, and guarantees a wide parallel frontier. A forced split may still have all 8 children pruned. Must not exceed `maxDepth`; neither may be negative ([§ 3.1](01-sampler.md#31-root-bounds)). |
| `rootBounds` | unset | If set, used **verbatim — no padding**. If unset, auto-fit a padded AABB from `field.bounds()`. Required for fields with infinite bounds. |
| `padFraction` | 0.05 | When auto-fitting, expand **every** axis by this fraction of the *longest* axis (isotropic in world units). |
| `signMethod` | `WINDING_NUMBER` | Selects the inside/outside oracle. All three values are implemented: 3-ray majority parity, Bærentzen–Aanæs pseudonormal, hierarchical generalized winding number ([§ 3.4](02-sign-oracles.md)). Honoured by both entry points — `dualContourMesh` routes through `sampleMeshToHermiteOctree` precisely so it cannot drift ([§ 1](README.md#1-pipeline-at-a-glance)). |
| `interpolateNormals` | `true` | When `true`, edge crossings record a barycentric blend of the input's area-weighted per-vertex normals — preserves smooth shading on organic meshes (molde). When `false`, the hit triangle's geometric face normal is recorded — preserves sharp 90° corners on CAD-style inputs (cube). Only affects the mesh path. |
| `numThreads` | 0 | Worker threads for the octree build; 0 means all hardware threads. The build is split into independent subtrees, so **the octree — and the mesh it contours to — is bit-identical regardless of this value** ([§ 3.2](01-sampler.md#32-parallel-octree-build)). |

### `ContourerParams` — [`include/dualc/contourer.h`](../../include/dualc/contourer.h)

| Field | Default | What it does |
| --- | --- | --- |
| `qefRegularization` | 0.1 | Pseudo-inverse truncation threshold passed to `QefSolver::solve`. Matches Nick Gildea's reference. Higher → more aggressive truncation of small eigenvalues of `AᵀA`; smoother on near-flat configurations. Applied to **both** leaf and collapse solves ([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)). No CLI exposes it. |
| `simplificationError` | 0.0 | Threshold for adaptive cell collapse; `<= 0` disables the pass entirely. Compared against the merged QEF's **geometric energy** at the solved vertex — a summed squared distance in length², so it scales with the number of merged samples and a value tuned at one depth is not portable to another ([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)). Applied by both pipeline entry points, or by calling `simplifyHermiteOctree` yourself between the sampler and the contourer. The CLIs expose it as `--collapse E`. |
| `clampVertexToCell` | `true` | Whether to constrain QEF minimisers to stay inside their cell. |
| `clampToleranceCells` | 1.0 | When clamping, how many cell-widths of drift to tolerate before falling back to the QEF mass point instead of componentwise-clamping ([§ 4.4](04-qef-manifold-collapse.md#44-the-per-cell-qef)). |
| `manifoldDC` | `true` | Manifold Dual Contouring: one QEF vertex per surface component in a cube instead of always one per cell. Fixes the "pinch across one cell" class of non-manifold edges. When `false`, the contourer falls back to byte-identical single-vertex behaviour ([§ 4.5](04-qef-manifold-collapse.md#45-manifold-dual-contouring)). |

`ContourerParams` has **no thread knob**; the contourer's QEF pre-solve hard-codes
`resolveThreadCount(0)`. Neither struct carries a field that is read nowhere: a public field
that silently does nothing is a promise the library does not keep
([17 #28](../roadmap/17-code-audit-and-hardening/04-engineering-quality.md#28-delete-the-two-dead-public-parameters)).

## 8. Vendored third-party code

DualC is MIT-licensed. Two third-party pieces are vendored in-tree. The first is the public-domain
(Unlicense) QEF/SVD solver from Nick Gildea's
[DualContouringSample](https://github.com/nickgildea/DualContouringSample), vendored as
`src/internal/qef.{h,cpp}` and `src/internal/svd.{h,cpp}` with their original headers
preserved. Three of the four are byte-identical to upstream. `svd.cpp` carries **one**
deliberate local modification: `Svd::pinv` zeroes only the eigenvalues *below* `tol`;
upstream also zeroes those *above* `1/tol`, which with DualC's default tolerance discards
everything above 10 and cripples the collapse solve
([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)). It is marked at its site
and in a note at the top of the file, and re-syncing with upstream means re-applying it. The
full attribution, the measurement behind the change and the test that pins it are
[`THIRD_PARTY.md`](../../THIRD_PARTY.md).

The second is nanort (MIT, Light Transport Entertainment), the 2016 single-header BVH at
`src/internal/third_party/nanort/nanort.h`, byte-identical to the copy geometry-central bundled
before v1.1.0. `mesh_bvh.cpp` instantiates its four-parameter `BVHAccel` and walks its nodes
directly; geometry-central v1.1.0 made its own nanort private and updated it to an incompatible
API, so DualC owns the header it was written against. Bumping nanort is a port of `mesh_bvh.cpp`,
not a header swap. Everything else under `src/internal/` is DualC-original.

`svd.cpp` is not a general SVD: it is a cyclic Jacobi eigensolver for a symmetric 3×3 matrix
(two-sided symmetric rotations on `AᵀA`, accumulating the eigenvectors). Because the input is `AᵀA` (symmetric PSD), the "singular values" it produces are
the *eigenvalues* of `AᵀA`, i.e. the squared singular values of `A`. That matters when
reasoning about `qefRegularization`: a tolerance of 0.1 on eigenvalues corresponds to ≈0.316
on singular values of `A`.

The LGPL-licensed octree code from the same project (and Tao Ju's reference implementation)
is deliberately **not** vendored — those are read-only references. The DC topology tables,
octree representation and triangle emission logic are re-derived in this codebase to keep
DualC permissively licensed.

---

← Back to the [design index](README.md) · the [docs index](../README.md)
