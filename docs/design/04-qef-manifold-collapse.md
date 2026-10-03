# 4.4–4.6 The QEF, manifold DC and adaptive collapse

The per-cell solve behind every vertex the contourer emits
([§ 4.3](03-contourer-recursion.md#43-emission)), the one-vertex-per-component rule that
keeps the output manifold, and the optional bottom-up collapse that makes the octree
mixed-depth ([§ 1](README.md#1-pipeline-at-a-glance)). All three live in
[`src/contourer.cpp`](../../src/contourer.cpp) and share the vendored `svd::QefSolver`
([§ 8](06-parameters-and-vendoring.md#8-vendored-third-party-code)).

## 4.4 The per-cell QEF

`solveOneComponent` accumulates one QEF input per usable crossing edge of the component —
skipping `hasCrossing == false` and zero-normal edges — into the vendored `svd::QefSolver`,
and solves with `kQefSvdTol = 1e-6f`, `kQefSweepCount = 50` and a pseudo-inverse tolerance
taken from `ContourerParams::qefRegularization` (falling back to `kQefPinvDefault = 0.1f`
when it is not positive).

The solver accumulates the upper triangle of `AᵀA`, `Aᵀb`, `bᵀb`, the raw mass-point *sum*
and `numPoints`, where `A`'s rows are unit normals and `b`'s entries are `nᵢ·pᵢ` — the
classic plane-fit energy `E(x) = Σ (nᵢ · (x − pᵢ))²`. At solve time the mass point is used as
an **origin shift**: the system is centred, solved, and the centroid added back. That
improves conditioning and, more importantly, makes the truncated pseudo-inverse's null space
default to the mass point rather than to the world origin — the standard DC regularisation.

If a component has no usable normals it produces no vertex at all. Vertex clamping is then a
**soft** clamp:

```cpp
if (params.clampVertexToCell) {
  const Vector3 tol{ext.x * params.clampToleranceCells, /* … */};
  if (isFarOutsideBox(v, node.bounds, tol)) {
    const auto& mp = q.getMassPoint();
    v = clampToBox(Vector3{mp.x, mp.y, mp.z}, node.bounds);
  } else {
    v = clampToBox(v, node.bounds);
  }
}
```

If the minimiser drifted more than `clampToleranceCells` cell widths outside the cell, the
solve is deemed degenerate and the vertex is replaced by the mass point (then clamped, a
no-op because the centroid of points on the cell's edges is always inside). Otherwise it is
componentwise-clamped. A hard componentwise clamp pins flat-region vertices to cell walls and
produces visible faceting ([01 § 4.3](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#43-output-was-noisy--smooth-tangency-broken));
letting a vertex sit slightly outside its own cell is legal for DC as long as connectivity is
unchanged.

Each component also produces an output normal: the unit-normalised sum of that component's
QEF input normals. Those are the per-vertex normals returned to the caller, index-aligned
with `mesh.vertices()`.

## 4.5 Manifold dual contouring

`ContourerParams::manifoldDC` is **`true` by default**. Instead of one QEF vertex per cell,
DualC places **one vertex per surface component inside the cell** (Schaefer/Ju/Warren 2007),
which is the standard fix for the "pinch across one cell" class of non-manifold edges.

`internal::solveLeaf` is the driver:

```cpp
if (params.manifoldDC) {
  out.parts = partitionCubeEdges(data.cornerInside, hasCrossing);
} else {
  out.parts.componentOfEdge.fill(-1);
  bool any = false;
  for (std::size_t e = 0; e < 12; ++e)
    if (hasCrossing[e]) { out.parts.componentOfEdge[e] = 0; any = true; }
  out.parts.numComponents = any ? 1 : 0;
}
for (int c = 0; c < out.parts.numComponents; ++c)
  out.perComponent[c] = solveOneComponent(node, params, out.parts, c);
```

With MDC off, every crossing collapses into a single component and the result is
byte-identical to single-vertex DC. `solveLeaf` deliberately lives in `dualc::internal`
(declared in `src/internal/contourer_internals.h`) rather than an anonymous namespace, so the
test suite can exercise the multi-vertex path on hand-built `HermiteLeafData` without going
through the traversal.

**`partitionCubeEdges`**
([`src/internal/cube_components.cpp`](../../src/internal/cube_components.cpp)) is a tiny
union-find **over the 12 cube edges** (not the corners). It is O(1) and allocation-free:
`std::array<std::int8_t, 12> parent`, path-halving `find`, no union-by-rank (the tree is at
most 12 deep). It walks the 6 faces (`kFaceCorners`), maps each face's 4 CCW boundary edges
to cube-edge indices, and counts crossings on them:

- **0 crossings** — nothing to connect on this face.
- **2 crossings** — `uf.unite(eA, eB)`: the surface segment crossing this face joins them.
- **4 crossings — the saddle case.** Four crossings on a face cycle can only arise from
  strictly alternating corner signs, so the face is genuinely ambiguous: the surface can pair
  the crossings two ways. DualC uses the **inside-pair rule** — pair the two crossings that
  share an *inside* corner:

  ```cpp
  if (cornerInside[fc[0]]) { uf.unite(faceEdge[3], faceEdge[0]);
                             uf.unite(faceEdge[1], faceEdge[2]); }
  else                     { uf.unite(faceEdge[0], faceEdge[1]);
                             uf.unite(faceEdge[2], faceEdge[3]); }
  ```

  This is one of the two topologically valid choices, and it is a *fixed* choice rather
  than a decided one. **Schaefer's asymptotic decider is unavailable** because it needs the
  corner SDF *values* to evaluate the bilinear interpolant on the face, and
  `HermiteLeafData` stores only `std::array<bool, 8> cornerInside`
  ([§ 2](README.md#2-the-data-contract-hermiteoctree)). Storing 8 doubles per leaf would
  enable the exact decider at 64 bytes per leaf.

Union-find roots are then compacted into 0-indexed component IDs in edge order, so IDs are
deterministic but arbitrary. A cube has **at most 4 surface components**, which is asserted
and is why `MultiLeafSolve::perComponent` is a fixed `std::array<LeafSolve, 4>`.

Emission needs no special case: `emitQuadAtEdge` calls
`leafVertexIndex(nodes[k], kProcessEdgeMask[axis][k])`, so each of the four cells around an
edge independently resolves *its own* local edge to *its own* component. If two slots are
the same pseudo-leaf but different components they yield distinct indices and the quad stays
a quad; if the same component, the dedupe collapses it to a triangle.

**Measured effect.** On molde, MDC removes the intrinsic non-manifold pinch edges that
single-vertex DC leaves, at the same face count and one extra vertex per pinch cell — the
numbers are the quality-bar table in
[01 § 5](../roadmap/01-core-dual-contouring/01-engineering-record.md#5-current-quality-bars-depth-7-unless-stated),
and they hold with `--collapse` enabled.

## 4.6 Adaptive cell collapse

`simplificationError` is **live**, wired into both pipeline entry points. There are two
public entries — the params overload is the real one, and the `double` overload forwards to
it with otherwise-default parameters:

```cpp
void simplifyHermiteOctree(HermiteOctree& octree, const ContourerParams& params,
                           const CancelToken* cancel) {
  if (params.simplificationError <= 0.0) return;
  const float pinvTol = (params.qefRegularization > 0.0)
                            ? static_cast<float>(params.qefRegularization)
                            : kQefPinvDefault;
  simplifyRecursive(octree.root(), params.simplificationError, pinvTol, cancel);
}
```

Taking the struct is what lets the collapse solve use the **same** `qefRegularization` as the
leaf solves; a bare `double` entry would leave it hard-coded at `kQefPinvDefault`, so a
caller who changed the regularisation would change only half the pipeline. (No CLI exposes
`qefRegularization` — it is a library-level knob only.)

`simplifyRecursive` is a **post-order** DFS — children first, then `tryCollapse` on the node
— which is what makes multi-level cascades free: by the time a parent is tested, its
children may already be pseudo-leaves.

`tryCollapse`, in order:

1. Refuse if the node is already a leaf.
2. Require all 8 children to exist, be leaves, and carry `HermiteLeafData`.
3. **Three topology gates**, any of which refuses the collapse:
   - `hasInternalSignChange` — walks the 6 internal edges (`kCellProcEdgeMask`) and returns
     true if the corresponding local edge of the first child sharing each has disagreeing
     endpoint signs. An internal sign change is a surface feature strictly inside the parent
     that a single vertex cannot represent, and it would simply vanish on collapse.
   - `hasOuterDoubleCrossing` — for each of the 12 outer edges, reads `signA` (child A's
     corner A), `signMid` (child A's corner B, the shared midpoint) and `signB` (child B's
     corner B), and refuses when `signA == signB && signMid != signA` — a hidden double
     crossing that collapse would erase.
   - `hasSaddleFace` — reconstructs the parent's 8 corner signs as
     `ps[c] = children[c]->leaf->cornerInside[c]`, counts sign changes around each of the 6
     face 4-cycles, and refuses on `>= 4`. A single QEF vertex cannot represent a saddle.
4. **Merged QEF.** `accumulateLeafIntoQef` adds every usable crossing of all 8 children — up
   to 96 samples — into one `svd::QefSolver`. This is a re-accumulation of the raw Hermite
   samples, not the O(1) additive `QefData::add(QefData)` merge that `qef.cpp` also provides.
5. **Acceptance test:**

   ```cpp
   q.solve(solved, kQefSvdTol, kQefSweepCount, pinvTol);
   const double qefEnergy = static_cast<double>(std::max(0.0f, q.getError(solved)));
   if (qefEnergy > errorThreshold) return false;
   ```

   The threshold is compared against the **geometric QEF energy** at the solved point,
   `xᵀAᵀAx − 2x·Aᵀb + bᵀb` = the summed squared distance from the merged vertex to the planes
   the merged Hermite samples define. That is `QefSolver::getError(pos)`, which is safe to
   call straight after `solve()` because `solve()` restores the un-centred `Aᵀb` on its way
   out.

   This is deliberately **not** `solve()`'s return value. That is `Svd::solveSymmetric`'s
   `calcError` — the residual of the normal equations `‖Aᵀb − AᵀA·x‖²` on the
   mass-point-centred system, i.e. "how badly the linear solve converged", not "how far the
   samples are from the fit". The two are correlated, but only the energy is a geometric
   quantity; thresholding the residual refuses collapses outside near-planar regions once
   the pseudo-inverse truncates (the defect, its measurement on molde and the fix are
   [01 § 4.9](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch)).
   In practice the three topology gates usually bind first.

   Two caveats. The energy is a **summed** squared distance, so it grows with the number of
   merged samples: a threshold tuned at one octree depth is not directly transferable to
   another. And `bᵀb` is the one accumulator that is *not* re-centred on the mass point, so
   in `float` it cancels badly for geometry far from the world origin — the
   `std::max(0.0f, …)` clamp keeps that from going negative but cannot restore the precision.
   Near the origin the number is trustworthy; in site coordinates it is noisy
   ([§ 9](07-limitations.md), [17 #24](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#24-far-from-origin-precision--re-centre-before-the-float-seam)).

   `pinvTol` comes from `ContourerParams::qefRegularization`, so leaf and collapse solves
   are regularised identically.
6. **Merged leaf construction.** Parent corner `c` takes child `c`'s corner `c` sign (the
   same world point, because child `c` occupies the parent's `c`-th octant). Parent edge `e`
   takes whichever of its two child sub-edges — both at local index `e` — has `hasCrossing`.
   Gate 3's outer-double-crossing test is what guarantees this preserves the
   `hasCrossing ⟺ sign change` invariant.
7. **Multi-component refusal.** `partitionCubeEdges` is re-run on the merged data and the
   collapse is refused if `numComponents > 1`. The three topology gates catch most such
   cases; this catches the residual non-saddle ones, such as opposite-inside-corners.
   Without it, a pseudo-leaf would either bake two separate sheets into one summed QEF or
   lose track of which component each outer edge belongs to.
8. **Install.** `node.leaf = std::move(merged); node.isLeaf = true;` and
   `children[c].reset()` — the `unique_ptr` reset frees the entire subtree.

**Measured effect.** The vertex/face counts of molde and the cube under `--collapse 100`, with
0 boundary edges, 0 non-manifold edges and Euler χ = 2, are the same quality-bar table in
[01 § 5](../roadmap/01-core-dual-contouring/01-engineering-record.md#5-current-quality-bars-depth-7-unless-stated).

---

← Back to the [design index](README.md) · the [docs index](../README.md)
