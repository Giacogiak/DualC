# Significant bugs and their fixes (§ 4), incl. the code-screening batch (§ 4.9)

Part of [01 — Core dual contouring](README.md). Every entry is preserved in the relevant
source file's comments and tests; this is the record of what went wrong and why the fix
is what it is. Findings the 2026-08-21 batch did **not** close are tracked in
[17](../17-code-audit-and-hardening/README.md).

## 4. Significant bugs and their fixes

Every entry below is preserved in the relevant source file's comments and tests. The
point of cataloguing them here is so the next person doesn't re-trip them.

### 4.1 Sampler bailed at the root on L-shaped meshes

- **Symptom:** `dualc_demo molde.obj` produced a 3-vert / 1-face placeholder.
- **Cause:** Refinement criterion was "all 8 corners + center sign uniform → don't refine". For a calf+foot, the root bbox's center sits in empty space, so all 9 sample points returned "outside" and the sampler bailed.
- **First fix:** Enforce `SamplerParams::minDepth` as a floor — always recurse below that depth, regardless of sign uniformity.
- **Real fix:** Replace the entire sign-based heuristic with `MeshBVH::cellOverlapsAABB(BBox)` — an iterative DFS over the nanort BVH node tree that returns `true` as soon as any leaf-AABB overlaps the cell. Geometric, not sign-based. Catches thin features that fit inside a cell with all-outside corner signs.
- **Files:** [src/internal/mesh_bvh.cpp](../../../src/internal/mesh_bvh.cpp) (`cellOverlapsAABB`), [src/sampler.cpp](../../../src/sampler.cpp) (`buildNode`).

### 4.2 Sign oracle was O(triangles) per query

- **Symptom:** On molde (76k tris) at depth 7, full pipeline projected to take hours.
- **Cause:** Generalised winding number summed per triangle for every sign query; ~100k cells × 9 sample points × 76k tris ≈ 7 × 10⁹ ops.
- **Fix:** BVH-accelerated ray-stabbing parity via `MeshBVH::countRayHits` — iteratively call `nanort::BVHAccel::Traverse`, advancing `ray.min_t` past each hit. O(hits · log N) per query, ~milliseconds × millions, sub-second overall.
- **Robustness fix:** Single-ray parity is wrong when the probe ray grazes coplanar near-edges. Upgraded to 3-ray majority vote with axis-permuted directions (`(1, √3-1, 1/√3)` and two cyclic permutations) in [src/internal/sign_oracle.cpp](../../../src/internal/sign_oracle.cpp).
- **Caveat:** Correct only for closed/oriented meshes — for non-watertight inputs we need GWN with BVH-pruned summation (deferred, see [Tier-2 item 6](README.md#6-generalized-winding-number-with-hierarchical-acceleration)).

### 4.3 Output was noisy / smooth tangency broken

Four root causes compounded; resolved one at a time:

| Cause | Where | Fix |
| --- | --- | --- |
| Output OBJ had no `vn` lines, so viewers flat-shaded | The demo called `writeSurfaceMesh(mesh, geom, path)` which doesn't emit `vn`. | Contourer now returns a per-vertex normals vector (`std::vector<Vector3>`); demo writes via `WavefrontOBJ::write(filename, geometry, CornerData<Vector3>& normals)`. |
| Edge-crossing normals were geometric face normals, not interpolated | `MeshBVH::segmentFirstHit` recorded `triFaceNormal(tri)`. | Use `nanort::TriangleIntersection`'s `u, v` to barycentrically interpolate the input mesh's per-vertex area-weighted normals. Falls back to face normal if the interpolated normal is degenerate. |
| QEF clamp pinned flat-region vertices to cell walls | `clampVertexToCell = true` did a hard componentwise clamp. | Soft clamp: only force the vertex back inside the cell when it has drifted more than `clampToleranceCells` (default 1.0) cell widths outside. If it has, fall back to the QEF mass point; otherwise componentwise-clamp as before. |
| `qefRegularization` default `1e-6` was 100× tighter than Nick Gildea's reference value | Default kept tiny singular values that should have been truncated. | Default bumped to `0.1f` to match the reference. |

Per-vertex output normals are computed as the unit-normalised average of the cell's QEF
input normals (since those are themselves smooth after the interpolation fix, the
average is smooth too).

### 4.4 First adaptive-collapse attempt produced 4-7k boundary edges + 13-14k non-manifold edges on molde

The simplification logic was correct, but the **contourer's handling of pseudo-leaves**
had two distinct bugs that the recursion only triggered in the presence of mixed-depth
leaves:

1. **Degenerate-quad skipping** — when a pseudo-leaf P spans two perpendicular slots around a fine edge, `edgeProc`'s 4-cell array contained P twice. The old code detected the duplicate and skipped emission entirely → holes. Replaced with a vertex-dedupe pass that emits 1 triangle when the polygon collapses to 3 unique vertices, 2 triangles for 4.
2. **Wrong cell's sign data** — for varying-depth terminal cases, the code picked the **coarsest** cell's local edge (per `kProcessEdgeMask`) to test for a sign change. But that local edge sits on the coarse cell's outer surface, which is NOT the absolute (fine) edge being processed. Switched to picking the **finest** cell — its local edge per `kProcessEdgeMask` is exactly the absolute edge.

After fixing both, molde with `--collapse 100` produces a clean closed genus-0 manifold
(Euler χ = 2, 0 boundary, 3 intrinsic non-manifold edges — unchanged by the collapse).

### 4.5 Sharp 90° corners rounded by smooth-normal interpolation

- **Symptom:** Cube output bounds drifted from exactly `[-0.5, 0.5]³` to `[-0.50007, 0.50007]³`.
- **Cause:** Barycentric-interpolated vertex normals near cube corners average the 3 incident face normals; QEF placement minimises against this smoothed direction set, rounding the corner.
- **Fix:** `SamplerParams::interpolateNormals` (default `true`). When `false`, `MeshBVH::segmentFirstHit` returns the hit triangle's geometric face normal instead. CLI: `--sharp` flag.
- **Effect on cube with `--sharp`:** 8 vertices pinned exactly at the cube corners, 1,392 on the cube edges, 80,736 on the cube faces — every output vertex lies on the cube boundary.

### 4.9 Code-screening batch
**2026-08-21.**


A full read-through of the engine (audit kept under `docs/raw/study/`) surfaced a
list of defects; this batch fixed five of the top-ranked ones and closed the
test gaps they exposed. All five had been invisible to the suite.

- **`dualContourMesh` silently dropped `SamplerParams::signMethod`.** It built
  its own `MeshSource` from just `interpolateNormals`, so asking for
  `GENERALIZED_WINDING_NUMBER` (or `PSEUDONORMAL`) through the mesh entry point
  got 3-ray parity instead — the one oracle that cannot handle the soup / open
  shells GWN exists for. Fixed structurally: `dualContourMesh` now delegates to
  `sampleMeshToHermiteOctree`, leaving **one** `MeshSource` construction site in
  the library. Both entry points share a `simplifyAndContour` tail.
  Files: [src/pipeline.cpp](../../../src/pipeline.cpp),
  [tests/test_pipeline.cpp](../../../tests/test_pipeline.cpp).

- **The vendored `Svd::pinv` truncated *large* eigenvalues.** Upstream reads
  `(fabs(x) < tol || fabs(1/x) < tol) ? 0 : (1/x)`; the rows of `A` are unit
  normals, so `λmax(AᵀA) ≤ n` and with DualC's default `tol = 0.1` the second
  clause zeroed everything above 10. A per-leaf QEF holds at most 12 samples and
  was nearly always fine; **collapse merges up to 96** and routinely lost every
  direction, returning the mass point. Dropped that clause — the first clause is
  the real regularisation. This is DualC's only local modification to a vendored
  file; it is marked at its site and in [THIRD_PARTY.md](../../../THIRD_PARTY.md).
  Files: [src/internal/svd.cpp](../../../src/internal/svd.cpp),
  [tests/test_qef.cpp](../../../tests/test_qef.cpp).

- **Collapse thresholded the wrong quantity.** `tryCollapse` compared
  `simplificationError` against `solve()`'s return — the normal-equation
  residual of the mass-point-centred system, i.e. "how badly the linear solve
  converged" — while the geometric QEF energy `QefSolver::getError()` went
  uncalled. Now it calls `getError(solved)` (safe straight after `solve()`,
  which restores the un-centred `Aᵀb`), clamped at zero because `bᵀb` is
  accumulated in `float` and not re-centred.
  **Behaviour change, measured** (`molde.obj`, `--depth 7`): `--collapse 0` is
  unchanged (38,116 V / 76,228 F both before and after — the pinv fix is a no-op
  on the leaf path here). `--collapse 1` goes from 70,170 to 68,698 faces, i.e.
  2% more merged; `--collapse 100` is ~68,6xx either way. Smaller than expected,
  and for a good reason: the binding constraint on collapse is the three
  topology gates, not the error gate. What did change is the *meaning* of `E` —
  a summed squared distance, so it scales with the merged sample count and is
  not portable across depths. Doc pages updated accordingly.

- **Collapse ignored `qefRegularization`**, because `simplifyHermiteOctree` took
  a bare `double`. Added a `const ContourerParams&` overload that threads the
  pinv tolerance through; the `double` overload forwards to it.
  Files: [include/dualc/contourer.h](../../../include/dualc/contourer.h),
  [src/contourer.cpp](../../../src/contourer.cpp).

- **No domain wrapper overrode `cellOverlaps`.** That predicate is the sampler's
  only pruning gate, so it is a correctness opt-out, not just a fast path: a
  wrapper that leaves the Lipschitz-1 default in place discards a non-Lipschitz
  child's always-overlap signal, and `mirrored(gyroid)` / `twisted(gyroid)` and
  friends silently dropped surface cells. All **nine** wrappers now forward to
  their child on a box containing the child's image of the cell — the six in
  `domain_ops.cpp` plus `TransformField`, `ScaleField` and `ElongateField` in
  `decorators.cpp` (the last three matter because the strut-lattice builder is
  `transformed(repeated(capsule union))`). The obligation is now stated on
  `ImplicitField::cellOverlaps` itself and pinned by a property test.
  **Measured** (`--depth 7`, identical bounds either side): the centred octet
  strut lattice is byte-identical and no slower (40.5 s → 40.8 s) — its capsules
  are Lipschitz-1, so old and new predicates agree. A twisted gyroid shell costs
  20% more (19.7 s → 23.6 s) and gains 2.2% more faces (529,822 → 541,484), and
  the interesting part is what those faces were: counting edge incidence in the
  two OBJs, the **old output had 4,196 boundary edges — actual holes — and the
  new one has none**. So this was not a refinement-quality nicety; a domain op
  over a TPMS was producing non-watertight meshes. (Both outputs carry the same
  5 four-incident edges, a pre-existing and unrelated pinch.) No blow-up on
  either path.

  Both new tests were checked against a build with the six overrides disabled:
  the conservativeness property test fails (nothing is forwarded at all) and the
  watertightness test fails with 574 boundary edges.

**Test gaps closed in the same batch** (the audit ranked these #1 and #2 among
all coverage gaps):

- Adaptive collapse had **zero** test references. Added the three topology-gate
  refusals and the error gate as hand-built octrees
  ([tests/test_contourer.cpp](../../../tests/test_contourer.cpp)), plus end-to-end
  sweeps asserting χ is preserved and no boundary edge opens while the face
  count drops ([tests/test_demo_meshes.cpp](../../../tests/test_demo_meshes.cpp)).
- The six DC descent tables had no direct test, and `dc_tables.h` falsely
  claimed they did. All six are now checked by **re-deriving** each entry from
  the four basic shape tables and the shared (P1, P2) node-ordering convention,
  so a transcription slip names its own index
  ([tests/test_dc_tables.cpp](../../../tests/test_dc_tables.cpp)).
- Wrapper `cellOverlaps` conservativeness is pinned black-box: a recording child
  notes every point it is read at over a cell and every box it is asked about,
  and every read must land inside some asked box
  ([tests/test_domain_ops.cpp](../../../tests/test_domain_ops.cpp)). A newly added
  wrapper has to be added to that list by hand — the underlying design problem
  (a virtual whose omission is silent) is unchanged and is still best solved by
  a declared per-field Lipschitz constant.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
