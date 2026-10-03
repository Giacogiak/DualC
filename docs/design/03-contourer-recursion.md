# 4. The contourer — Hermite octree → mesh

Defined in [`src/contourer.cpp`](../../src/contourer.cpp). This is the real
Ju/Schaefer/Warren octree dual contouring: a `cellProc` / `faceProc` / `edgeProc` recursion
over the tree, with mixed depth handled natively. This page is the traversal and the
emission; the per-cell solve, manifold DC and the collapse pass are
[§ 4.4–4.6](04-qef-manifold-collapse.md).

`contourHermiteOctree` does three things.

**1. Leaf collection and parallel QEF pre-solve.** `collectLeaves` walks the tree depth-first
and pushes every node with a non-null `leaf` pointer — real leaves *and* pseudo-leaves from a
prior collapse. `parallelFor` then fills a `std::vector<MultiLeafSolve>`, which is moved into
`state.leafSolveCache`:

```cpp
const auto solveOne = [&](std::size_t i) {
  if (cancel) cancel->throwIfRequested("contouring");
  solved[i] = internal::solveLeaf(*leaves[i], params);
};
if (progress) {   // polled, so the sink reports from the calling thread only
  progress->report(Stage::Contour, 0, 2 * leafTotal);
  internal::parallelForPolled(leafTotal, internal::resolveThreadCount(0), solveOne,
                              [&](std::size_t done, std::size_t) {
                                progress->report(Stage::Contour, done, 2 * leafTotal);
                              });
} else {
  internal::parallelFor(leafTotal, internal::resolveThreadCount(0), solveOne);
}
```

The two hooks — the per-leaf cancellation poll and the polled loop — are
[10 § Hooks](10-invariants-and-tolerances.md#hooks-cancellation-and-progress); neither
touches a result. Only the pre-solve is parallel. The traversal, vertex-index allocation and triangle emission
are strictly serial, which is exactly what makes the output bit-identical regardless of
thread count: `solveLeaf` is a pure function of the leaf, and index assignment happens
later, in deterministic traversal order. (`ContourerParams` has no thread knob; the call
site hard-codes `resolveThreadCount(0)`.) A cache miss in `leafVertexIndex` falls back to an
inline solve, so correctness never depends on the pre-pass having run.

**2. The recursion** (`cellProc(octree.root(), state)`).

**3. Compaction, empty-output fallback, the output `Diagnostics` counts, and the
geometry-central handoff** (end of § 4.3).

## 4.1 `cellProc` / `faceProc` / `edgeProc`

```cpp
void cellProc(const HermiteNode* node, ContourState& s) {
  if (node == nullptr || node->isLeaf) return;
  if (s.cancel) s.cancel->throwIfRequested("contouring");   // 10 § Hooks
  for (int c = 0; c < 8; ++c) cellProc(node->children[c].get(), s);
  for (const auto& m : tables::kCellProcFaceMask)
    faceProc(node->children[m[0]].get(), node->children[m[1]].get(), m[2], s);
  for (const auto& m : tables::kCellProcEdgeMask)
    edgeProc(node->children[m[0]].get(), node->children[m[1]].get(),
             node->children[m[2]].get(), node->children[m[3]].get(), m[4], s);
}
```

Textbook JSW: eight recursive `cellProc` calls, then the parent's **12 internal faces**
(`kCellProcFaceMask`), then its **6 internal edges** (`kCellProcEdgeMask`). The tables are
listed in [§ 6](05-conventions-and-tables.md#the-dc-descent-tables).

`faceProc` **returns immediately when both sides are leaves and never emits geometry**:

```cpp
if (aLeaf && bLeaf) return;     // face emits no triangles directly
```

Otherwise it descends into the 4 sub-faces of the shared face (`kFaceProcFaceMask`) and the
4 internal edges of that face (`kFaceProcEdgeMask`, a struct-of-arrays
`FaceProcEdgeCall{childOf, childIdx, edgeAxis}`).

`edgeProc` is **the only emitter**. Non-terminal, it splits into 2 sub-edges along the edge
axis via `kEdgeProcEdgeMask`.

## 4.2 The two mixed-depth devices

**`childOrSelf` — a leaf descends to itself.**

```cpp
inline const HermiteNode* childOrSelf(const HermiteNode* n, std::uint8_t childIdx) {
  if (n == nullptr) return nullptr;
  if (n->isLeaf) return n;
  return n->children[childIdx].get();
}
```

A coarse cell therefore reappears at every deeper recursion level, so the recursion never
has to know that its four cells are at different depths.

**The finest-cell rule for reading signs.** In the terminal case (all four cells are
leaves), the sign change that decides whether to emit is read from the cell with the
*largest* depth:

```cpp
int finestK = 0;
for (int k = 1; k < 4; ++k) if (arr[k]->depth > arr[finestK]->depth) finestK = k;
const HermiteNode* finest = arr[finestK];
if (!finest->leaf) return;
const std::uint8_t e = tables::kProcessEdgeMask[axis][finestK];
const auto& endpoints = tables::kEdgeEndpoints[e];
const bool sa = finest->leaf->cornerInside[endpoints[0]];
const bool sb = finest->leaf->cornerInside[endpoints[1]];
if (sa == sb) return;
emitQuadAtEdge(arr, axis, /*corner0Inside=*/sa, s);
```

This is the load-bearing invariant of the whole mixed-depth story: only the finest cell's
local edge (per `kProcessEdgeMask`) **is** the absolute minimal edge. A coarser pseudo-leaf's
same-indexed edge sits on that cell's outer cube and is a different segment entirely.
`kProcessEdgeMask` is also arranged so that endpoint 0 of the local edge is always at the
**low** end of the absolute edge along its axis, for every `finestK` — which is what lets
`sa` be passed straight through as `corner0Inside`. Reading the coarsest cell instead was the
root of one of the major early bugs
([01 § 4.4](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#44-first-adaptive-collapse-attempt-produced-4-7k-boundary-edges--13-14k-non-manifold-edges-on-molde)).

## 4.3 Emission

`emitQuadAtEdge` resolves one vertex per surrounding cell, then triangulates:

```cpp
const auto& localEdge = tables::kProcessEdgeMask[axis];
int v[4];
for (int k = 0; k < 4; ++k) {
  v[k] = leafVertexIndex(nodes[k], localEdge[k], s);
  if (v[k] < 0) return;     // missing neighbour -> nothing to connect
}
const int (&order)[4] = kQuadCCWPlus[axis];
int polygon[4] = {v[order[0]], v[order[1]], v[order[2]], v[order[3]]};
```

**`kQuadCCWPlus`** puts the four cells in CCW order as seen from `+axis`:

```cpp
constexpr int kQuadCCWPlus[3][4] = {
    {0, 1, 2, 3}, // X-axis edge
    {0, 3, 2, 1}, // Y-axis edge   <-- reversed
    {0, 1, 2, 3}, // Z-axis edge
};
```

The Y row is reversed because `edgeProc`'s four-cell convention is
`(P1=hi, P2=hi), (lo, hi), (lo, lo), (hi, lo)` where `(P1, P2)` are the two axes
perpendicular to the edge: `(Y, Z)` for an X edge, **`(X, Z)` for a Y edge**, `(X, Y)` for
a Z edge. `(X, Z)` is a left-handed pair with respect to `+Y`, so the traversal order flips.
This is the single asymmetry in the tables; the four-cell order itself is stated once, in
[§ 6](05-conventions-and-tables.md#the-four-cell-ordering-around-an-edge).

**Degenerate quads emit a triangle, they are not skipped.** When pseudo-leaves of varying
depth surround a fine edge, two of the four slots can resolve to the same vertex; the polygon
collapses from a quad to a triangle. The code dedupes the four indices and emits 1 or 2
triangles accordingly:

```cpp
if (n < 3) return;          // genuine degenerate: the whole edge is inside one cell
if (corner0Inside) {
  s.tris.push_back({u0, u1, u2});
  if (n == 4) s.tris.push_back({u0, u2, u3});
} else {
  s.tris.push_back({u0, u2, u1});
  if (n == 4) s.tris.push_back({u0, u3, u2});
}
```

Skipping the degenerate case entirely leaves boundary edges, i.e. holes, in the output mesh
after a collapse (the same [01 § 4.4](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#44-first-adaptive-collapse-attempt-produced-4-7k-boundary-edges--13-14k-non-manifold-edges-on-molde)).
Winding is decided purely by `corner0Inside`: inside ⇒ outward normal along `+axis` ⇒ the
tabulated CCW order; outside ⇒ reversed.

**Lazy vertex allocation.** `leafVertexIndex` resolves the leaf's `MultiLeafSolve`, picks
the component owning the queried local edge, then — on the leaf's *first touch* — allocates
vertex indices for **all** of that leaf's valid components at once into a
`std::array<int, 4>`:

```cpp
std::int8_t comp = mls.parts.componentOfEdge[localEdgeIdx];
if (comp < 0) {
  if (mls.parts.numComponents != 1) return -1;  // multi-component: bail
  comp = 0;                                     // single-component: the lone vertex
}
```

Subsequent edge lookups in the same leaf are then a hash probe plus an array index rather
than a re-walk of the QEF data. The `comp < 0` single-component fallback is what makes
collapse and MDC compose: a pseudo-leaf's outer edge may carry no crossing, yet its single
QEF vertex is still the correct connection target.

The **compaction pass** at the end drops unreferenced positions and remaps indices. It is
load-bearing, not defensive: allocation is lazy *per leaf* and eager *per component*. `leafVertexIndex` fills in
indices for **all** of a leaf's valid components on first touch, not just the one being
queried (above), and `emitQuadAtEdge` allocates for `nodes[0..k-1]` before discovering that
slot `k` has no vertex and returning without emitting. So a vertex can be created for a
component that no emitted triangle ends up referencing, and the pass is load-bearing for
that case rather than purely hypothetical. (How often it actually fires has not been
measured.)

Finally, an **empty-output placeholder** guards geometry-central's `SurfaceMesh` constructor:
if there were no positions or no triangles, a single `(0,0,0) / (1,0,0) / (0,1,0)` triangle
is synthesised so the constructor does not reject the result. "No surface" and "one tiny
triangle" are therefore indistinguishable in the returned mesh; `Diagnostics::emptyContour`
is the only thing that tells them apart, and the CLIs print it as a `[dualc] warning:` line
([17/05](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md)).

---

← Back to the [design index](README.md) · the [docs index](../README.md)
