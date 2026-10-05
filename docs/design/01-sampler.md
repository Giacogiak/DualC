# 3. The sampler — field → Hermite octree

Defined in [`src/sampler.cpp`](../../src/sampler.cpp). There is exactly **one** sampling
algorithm; the mesh entry point is a short adapter over it:

```cpp
HermiteOctree sampleMeshToHermiteOctree(SurfaceMesh& mesh,
                                        VertexPositionGeometry& geometry,
                                        const SamplerParams& params,
                                        Diagnostics* diag,
                                        const CancelToken* cancel,
                                        ProgressSink* progress) {
  MeshSource source(mesh, geometry, params.interpolateNormals,
                    params.signMethod);
  if (diag) { /* inputEmpty, inputBoundaryEdges, inputNonManifoldEdges, inputWatertight */ }
  return sampleFieldToHermiteOctree(source, params, diag, cancel, progress);
}
```

The `MeshSource` is a stack temporary that owns its BVH for the duration of the call; the
four input diagnostics are read off its topology book before the sampler runs
([17/05](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md)). The
mesh path is therefore a strict subset of the field path — one refinement algorithm to reason
about, one to test. The sign oracle behind `MeshSource::isInside` is its own page,
[§ 3.4](02-sign-oracles.md).

## 3.1 Root bounds

`sampleFieldToHermiteOctree` validates the depths first — a negative `minDepth` or `maxDepth`,
or `minDepth > maxDepth`, throws `std::invalid_argument` before anything is allocated (the
judgement calls behind that are in
[17/08](../roadmap/17-code-audit-and-hardening/08-argument-validation.md)). It then resolves
the root box, with three distinct outcomes:

- **`SamplerParams::rootBounds` set** — used verbatim, with **no padding applied**.
- **`field.bounds()` is the `BBox::infinite()` sentinel** — throws `std::invalid_argument`.
  `BBox::isInfinite()` is an exact `±inf` comparison on all six components, so it recognises
  only the sentinel.
- **otherwise** — `padBBox(raw, padFraction)` if `raw.isValid()`, else `BBox::unit()`
  (`[-0.5, 0.5]³`). The invalid case (empty mesh, disjoint intersection) is the *correct*
  answer for those inputs and is reported rather than refused: when the caller passed a
  `Diagnostics*` it sets `Diagnostics::boundsFallback`, and every CLI prints that as a
  `[dualc] warning:` line. With no `Diagnostics*` the fallback is unannounced. The channel is
  [17/05](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md); the asymmetry
  with the infinite case is stated in [§ 10.5](08-implicit-field-layer.md#105-unbounded-fields).

`padBBox` is **isotropic in world units**, derived from the *largest* extent:

```cpp
const double maxDim = std::max({ext.x, ext.y, ext.z, 1e-12});
const Vector3 pad{maxDim * padFraction, maxDim * padFraction, maxDim * padFraction};
```

This guarantees the surface never touches the root boundary regardless of aspect ratio, at
the cost of a near-cubic root box for a thin part. `internal::childBounds` splits at the
per-axis midpoint, so **cells are anisotropic boxes, not cubes**; everything downstream (the
DC tables, the QEF) is metric-free, so this is sound.

## 3.2 Parallel octree build

The build is split at a shallow fork depth:

```cpp
const int stopDepth = std::min(params.maxDepth, 3);
std::vector<HermiteNode*> frontier;
collectFrontier(*octree.root(), /*depth=*/0, params.minDepth,
                params.maxDepth, stopDepth, field, frontier);
const auto body = [&](std::size_t i) {
  HermiteNode* n = frontier[i];
  buildNode(*n, n->depth, params.minDepth, params.maxDepth, field, cancel);
};
if (progress) {            // polled: the caller reports, the workers build
  const std::size_t n = frontier.size();
  progress->report(Stage::Sample, 0, n);
  internal::parallelForPolled(n, params.numThreads, body,
                              [&](std::size_t done, std::size_t total) {
                                progress->report(Stage::Sample, done, total);
                              });
  progress->report(Stage::Sample, n, n);
} else {
  internal::parallelFor(frontier.size(), params.numThreads, body);
}
```

`collectFrontier` refines *serially* to `stopDepth`, mirroring `buildNode`'s prune/refine
decision exactly, and collects the surviving nodes. Each frontier node then roots an
independent subtree that a worker builds with no shared writes.

**The output is bit-identical regardless of `numThreads`**, and the guarantee is structural,
not incidental:

1. each frontier node roots a disjoint subtree, so no worker touches another's memory;
2. the frontier is built serially, so its order is fixed;
3. every node's Hermite data is a pure function of its own bounds and the field — there is
   no reduction, hence no floating-point associativity dependence;
4. the contourer walks the tree structurally, not in completion order.

`internal::parallelFor` ([`src/internal/parallel.h`](../../src/internal/parallel.h)) is a
short header-only helper: dynamic scheduling via one relaxed atomic counter (octree subtrees
differ wildly in surface complexity, so static chunking would idle threads), the calling
thread participates, `join()` supplies the synchronisation edge. `numThreads == 0` means
"all hardware threads". A throwing body is safe under it: the first `std::exception_ptr` is
captured, an atomic flag stops the other workers claiming indices, every thread is joined,
and the exception is rethrown on the calling thread — so the threaded path behaves exactly
like the serial one (`tests/test_parallel.cpp`;
[17 #22](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#22-parallelfor-exception-propagation)).

Because the field is queried concurrently through a `const ImplicitField&`, every field
implementation must be safe to call concurrently on the same object. All in-tree fields
satisfy this by construction (state is written once in the constructor); user-derived fields
must too ([17 #29](../roadmap/17-code-audit-and-hardening/04-engineering-quality.md#29-thread-safety-contract-for-user-derived-implicitfield)).

The duplication between `collectFrontier` and `buildNode` is a maintenance hazard — any
change to the refinement rule has to be made in both — and the frontier is capped at
`8³ = 512` tasks by the hard-coded `stopDepth`.

Two optional hooks ride this build ([10 § Hooks](10-invariants-and-tolerances.md#hooks-cancellation-and-progress)):
`buildNode` polls a `CancelToken` once per internal node — above the child loop, so at
most the 8 leaves under one `maxDepth − 1` node are built between polls — and throws
`Cancelled`, which `parallelFor`'s exception path carries out after joining; and when a
`ProgressSink` is passed the frontier loop runs through `internal::parallelForPolled`
instead: with two or more resolved threads its calling thread claims no index and reports
`(done, n)` at most every 100 ms; with one thread (or a one-node frontier) the caller runs
the loop itself and polls every `max(1, n/64)` items. Either way the sink is never invoked
from a worker. Neither changes the octree.

## 3.3 Refinement and edge-crossing capture

`buildNode` has exactly three outcomes:

- **`depth >= maxDepth`** — sample the 8 corner signs with `field.isInside`, mark leaf, call
  `populateLeaf`. Only cells at `maxDepth` get Hermite data.
- **`depth < minDepth`** — force-recurse regardless of geometry. (Defends against L-shaped
  inputs whose root-bbox centre sits in empty space, and guarantees a wide parallel
  frontier.)
- **otherwise** — refine iff `field.cellOverlaps(node.bounds)`, a purely geometric test. If
  not, the cell becomes a Hermite-less leaf.

`maxDepth` is checked first and unconditionally; the pair is validated at entry (§ 3.1), so
`minDepth > maxDepth` cannot reach this point.

`populateLeaf` walks the 12 cube edges, skipping those whose endpoint signs agree. For the
rest, the query direction is normalised:

```cpp
// Always query from the outside endpoint towards the inside one, so the
// reported crossing is the surface entry point.
const bool hit = sa ? field.edgeHit(b, a, outP, outN)
                    : field.edgeHit(a, b, outP, outN);
```

This matters because both `MeshSource::edgeHit` and `WindingNumberField::edgeHit` resolve to
a **first-hit** ray query: on a thin feature an edge may cross the surface three times, and
first-hit-from-outside deterministically yields the entry point instead of something
dependent on the arbitrary edge orientation in `kEdgeEndpoints`.

**`edgeHit` is not one algorithm.** It is the field's own:

- **`MeshSource::edgeHit`** forwards straight to `MeshBVH::segmentFirstHit` — an **exact
  analytic ray/triangle intersection** from nanort. There is no iteration on the mesh path,
  and the crossing normal comes from the hit triangle (barycentric-interpolated vertex
  normals, or the face normal when `interpolateNormals == false`).
- **`ImplicitField::edgeHit`** (the generic default,
  [`src/implicit/implicit_field.cpp`](../../src/implicit/implicit_field.cpp)) is
  **Illinois-modified regula falsi with a 6-step budget — not bisection**. It returns `false`
  immediately if the endpoints do not straddle, seeds with the linear false-position estimate
  `tStar = va / (va - vb)`, then runs at most `kEdgeHitRefineSteps = 6` corrective steps,
  halving the value stored at a bracket end that has been retained twice in a row (the
  Illinois trick, which breaks false position's one-sided stall). It exits early on
  `|f| <= 1e-9` or a bracket no wider than `1e-6`, and reports the **bracket midpoint**, not
  the last false-position estimate. The normal comes from `gradientAt(outP)`. Cost: 2 + up to
  6 `valueAt` calls plus one `gradientAt`. The design point is that along a short cube edge a
  near-SDF is near-linear, so a linear seed plus six corrections beats 50 blind bisections;
  on a C0 field (a hard boolean seam) convergence degrades to bisection quality, i.e.
  worst-case `h/64` on an edge of length `h`.
- **`WindingNumberField::edgeHit`** tries the BVH first and only falls back to the generic
  root-finder where no triangle lies on the segment — because the generalized winding number
  jumps by exactly 1 across a triangle, so the 0.5-isosurface *coincides with the geometry*
  wherever geometry exists.

### The miss fallback ladder

The sign comes from `field.isInside` and the crossing from `edgeHit`. These are two
independent oracles over the same geometry, so they can disagree — at grazing incidence, at
a T-junction, or when the single-precision BVH loses a sliver triangle. The signs are
authoritative (they drive the DC tables), so the sampler must produce *something* for the
edge. Three branches, in order:

```cpp
if (hit) {
  he.position = outP; he.normal = outN; he.hasCrossing = true;
} else {
  const Vector3 mid = (a + b) * 0.5;
  Vector3 cpP, cpN;
  if (field.closestSurfacePoint(mid, cpP, cpN)) {
    he.position = cpP; he.normal = cpN; he.hasCrossing = true;
  } else {
    he.position = mid; he.normal = Vector3{0.0, 0.0, 0.0};
    he.hasCrossing = true;
  }
}
```

**`hasCrossing = true` on all three branches is deliberate.** The contourer keys its topology
off `hasCrossing`; dropping the edge would tear the mesh. The degenerate third case is
instead filtered out of the *geometry*: both `solveOneComponent` and `accumulateLeafIntoQef`
skip zero-normal edges explicitly —

```cpp
if (!he.hasCrossing) continue;
if (he.normal.x == 0.0 && he.normal.y == 0.0 && he.normal.z == 0.0) continue;
```

— so such an edge contributes **connectivity but no QEF plane constraint**. This two-tier
split, *topology from signs, geometry from planes*, is the sampler's central design decision.

The middle rung is the weak one: `closestSurfacePoint(mid)` returns the closest surface point
to the edge *midpoint*, which is excellent when the true crossing merely grazed the edge but
can be a full cell away if the sign oracle was simply wrong — and it is fed to the QEF as a
full-weight plane constraint with no distance sanity check.

### Why purely-geometric refinement?

Refining only where the 8 corner signs (plus the centre sign) disagree is rejected as the
criterion: any feature smaller than the cell can fit inside with all 9 sample points outside,
and gets pruned (the L-shaped-input failure this produced is
[01 § 4.1](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#41-sampler-bailed-at-the-root-on-l-shaped-meshes)).
For a mesh, `MeshSource::cellOverlaps` is an exact BVH-AABB overlap test: a cell containing
any input triangle is refined, with no field evaluation at all. For a generic field the
default `cellOverlaps` is corner-sign disagreement plus a conservative Lipschitz-1 centre test
— see [§ 10.1](08-implicit-field-layer.md#101-implicitfield) for the caveat that carries.

Note also what the criterion *is not*: it never looks at curvature or feature size, and there
is no error-driven refinement. That adaptivity lives downstream, in the collapse pass
([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)).

---

← Back to the [design index](README.md) · the [docs index](../README.md)
