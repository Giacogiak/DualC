#include "dualc/contourer.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "internal/contourer_internals.h"
#include "internal/cube_components.h"
#include "internal/dc_tables.h"
#include "internal/parallel.h"
#include "internal/qef.h"
#include "internal/svd.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dualc {

namespace {

// Tally undirected edges of an emitted triangle list by incidence count:
// exactly one triangle -> an open boundary, three or more -> a non-manifold
// junction. Both are zero on the closed manifold mesh dual contouring is
// meant to produce. O(3T) hash inserts; only ever called when a caller asks
// for Diagnostics.
//
// Vertex indices are packed two-to-a-uint64, which needs them to fit in 32
// bits. Beyond that the tally is skipped rather than reported wrong -- the
// counts stay 0 and `outputWatertight` stays true, so a caller must not read
// "watertight" as proof on a mesh with more than 4 billion vertices (nothing
// in this library can build one).
void tallyEdges(const std::vector<std::vector<std::size_t>>& tris,
                std::size_t vertexCount,
                std::size_t& outBoundary, std::size_t& outNonManifold) {
  outBoundary    = 0;
  outNonManifold = 0;
  if (vertexCount > 0xFFFFFFFFull) return;

  std::unordered_map<std::uint64_t, std::uint32_t> counts;
  counts.reserve(tris.size() * 2);
  for (const auto& t : tris) {
    for (std::size_t i = 0; i < t.size(); ++i) {
      const std::uint64_t a = t[i];
      const std::uint64_t b = t[(i + 1) % t.size()];
      const std::uint64_t key =
          (a < b) ? ((a << 32) | b) : ((b << 32) | a);
      ++counts[key];
    }
  }
  for (const auto& kv : counts) {
    if (kv.second == 1)     ++outBoundary;
    else if (kv.second > 2) ++outNonManifold;
  }
}

// ===========================================================================
// QEF tuning -- per Nick Gildea's reference values; see
// `D:\DualContouringSample\octree.cpp` for the original constants.
// ===========================================================================

constexpr int   kQefSweepCount  = 50;
constexpr float kQefSvdTol      = 1e-6f;
constexpr float kQefPinvDefault = 0.1f;

// ===========================================================================
// Helpers
// ===========================================================================

Vector3 clampToBox(const Vector3& v, const BBox& b) {
  return Vector3{std::clamp(v.x, b.min.x, b.max.x),
                 std::clamp(v.y, b.min.y, b.max.y),
                 std::clamp(v.z, b.min.z, b.max.z)};
}

bool isFarOutsideBox(const Vector3& v, const BBox& b, const Vector3& tolerance) {
  return v.x < b.min.x - tolerance.x || v.x > b.max.x + tolerance.x ||
         v.y < b.min.y - tolerance.y || v.y > b.max.y + tolerance.y ||
         v.z < b.min.z - tolerance.z || v.z > b.max.z + tolerance.z;
}

// Bring the shared internal::* MDC types into the anonymous namespace as
// short aliases. solveLeaf itself lives in `dualc::internal` (declared in
// internal/contourer_internals.h) so the test suite can exercise it.
using internal::EdgeComponents;
using internal::LeafSolve;
using internal::MultiLeafSolve;

} // namespace
} // namespace dualc

// ===========================================================================
// Per-leaf solve (defined in dualc::internal so tests can call it directly).
// ===========================================================================

namespace dualc {
namespace internal {
namespace {

// Solve a single component's QEF: accumulate only the leaf's edges that
// belong to the named component, then fill out.vertex/normal/hasVertex.
LeafSolve solveOneComponent(const HermiteNode& node,
                            const ContourerParams& params,
                            const EdgeComponents& parts,
                            std::int8_t componentId) {
  LeafSolve out;
  if (!node.leaf) return out;
  const HermiteLeafData& data = *node.leaf;

  svd::QefSolver q;
  int n = 0;
  Vector3 normalSum{0.0, 0.0, 0.0};
  for (std::size_t e = 0; e < 12; ++e) {
    if (parts.componentOfEdge[e] != componentId) continue;
    const auto& he = data.edges[e];
    if (!he.hasCrossing) continue;
    if (he.normal.x == 0.0 && he.normal.y == 0.0 && he.normal.z == 0.0) continue;
    q.add(static_cast<float>(he.position.x),
          static_cast<float>(he.position.y),
          static_cast<float>(he.position.z),
          static_cast<float>(he.normal.x),
          static_cast<float>(he.normal.y),
          static_cast<float>(he.normal.z));
    normalSum = normalSum + he.normal;
    ++n;
  }
  if (n == 0) return out;

  // Output normal: unit-normalized average of this component's QEF input
  // normals.
  const double m = normalSum.norm();
  out.normal = (m > 1e-12) ? (normalSum / m) : Vector3{0.0, 0.0, 0.0};

  svd::Vec3 solved;
  const float pinvTol = (params.qefRegularization > 0)
                          ? static_cast<float>(params.qefRegularization)
                          : kQefPinvDefault;
  q.solve(solved, kQefSvdTol, kQefSweepCount, pinvTol);
  Vector3 v{static_cast<double>(solved.x),
            static_cast<double>(solved.y),
            static_cast<double>(solved.z)};

  if (params.clampVertexToCell) {
    const Vector3 ext = node.bounds.extent();
    const Vector3 tol{ext.x * params.clampToleranceCells,
                      ext.y * params.clampToleranceCells,
                      ext.z * params.clampToleranceCells};
    if (isFarOutsideBox(v, node.bounds, tol)) {
      const auto& mp = q.getMassPoint();
      v = Vector3{static_cast<double>(mp.x),
                  static_cast<double>(mp.y),
                  static_cast<double>(mp.z)};
      v = clampToBox(v, node.bounds);
    } else {
      v = clampToBox(v, node.bounds);
    }
  }

  out.vertex = v;
  out.hasVertex = true;
  return out;
}

} // namespace

// Partition the leaf's crossing edges by surface component and solve one QEF
// per component. When manifoldDC is off, all crossings collapse to a single
// component -- byte-identical to the pre-MDC single-vertex behaviour.
MultiLeafSolve solveLeaf(const HermiteNode& node, const ContourerParams& params) {
  MultiLeafSolve out;
  if (!node.leaf) return out;
  const HermiteLeafData& data = *node.leaf;

  std::array<bool, 12> hasCrossing{};
  for (std::size_t e = 0; e < 12; ++e) {
    hasCrossing[e] = data.edges[e].hasCrossing;
  }

  if (params.manifoldDC) {
    out.parts = partitionCubeEdges(data.cornerInside, hasCrossing);
  } else {
    out.parts.componentOfEdge.fill(-1);
    bool any = false;
    for (std::size_t e = 0; e < 12; ++e) {
      if (hasCrossing[e]) {
        out.parts.componentOfEdge[e] = 0;
        any = true;
      }
    }
    out.parts.numComponents = any ? 1 : 0;
  }

  for (int c = 0; c < out.parts.numComponents; ++c) {
    out.perComponent[static_cast<std::size_t>(c)] =
        solveOneComponent(node, params, out.parts,
                          static_cast<std::int8_t>(c));
  }
  return out;
}

} // namespace internal
} // namespace dualc

namespace dualc {
namespace {

// Re-establish the anonymous-namespace short aliases for the simplify and
// recursive-contouring code below.
using internal::EdgeComponents;
using internal::LeafSolve;
using internal::MultiLeafSolve;

// ===========================================================================
// Adaptive cell collapse (Ju/Schaefer/Warren simplifyOctree)
// ===========================================================================

// Returns true iff the parent of these 8 children has at least one INTERNAL
// edge (one of the 6 edges shared by 4 sibling children) with a surface
// crossing. Internal sign changes get lost on collapse; we refuse to do so.
bool hasInternalSignChange(const HermiteNode& node) {
  for (const auto& m : tables::kCellProcEdgeMask) {
    const std::uint8_t axis = m[4];
    // Look at the first of the 4 children sharing this internal edge and
    // check if its corresponding local edge has a sign change.
    const HermiteNode* child = node.children[m[0]].get();
    if (child == nullptr || !child->leaf) continue;
    const std::uint8_t e = tables::kProcessEdgeMask[axis][0];
    const auto endpoints = tables::kEdgeEndpoints[e];
    if (child->leaf->cornerInside[endpoints[0]] != child->leaf->cornerInside[endpoints[1]]) {
      return true;
    }
  }
  return false;
}

// Returns true iff any of the parent's 6 outer faces has a saddle sign
// configuration -- 4 face corners alternating in sign such that the surface
// must cross the face on 4 boundary edges (not 0 or 2). A single QEF vertex
// at the parent can't represent a saddle, so collapsing would change topology.
bool hasSaddleFace(const HermiteNode& node) {
  std::array<bool, 8> ps;
  for (int c = 0; c < 8; ++c) {
    ps[static_cast<std::size_t>(c)] =
        node.children[static_cast<std::size_t>(c)]->leaf->cornerInside[static_cast<std::size_t>(c)];
  }
  for (std::size_t f = 0; f < 6; ++f) {
    const auto& fc = tables::kFaceCorners[f];
    int signChanges = 0;
    for (int i = 0; i < 4; ++i) {
      const int j = (i + 1) % 4;
      if (ps[fc[i]] != ps[fc[j]]) ++signChanges;
    }
    if (signChanges >= 4) return true;
  }
  return false;
}

// Returns true iff any of the 12 outer edges of the parent has a hidden
// double-crossing (both endpoints same sign but mid-edge sign disagrees).
// Such a feature would be lost on collapse.
bool hasOuterDoubleCrossing(const HermiteNode& node) {
  for (std::uint8_t e = 0; e < 12; ++e) {
    const auto endpoints = tables::kEdgeEndpoints[e];
    const std::uint8_t cA = endpoints[0];
    const std::uint8_t cB = endpoints[1];
    const HermiteNode* childA = node.children[cA].get();
    if (childA == nullptr || !childA->leaf) continue;
    // Mid-edge corner = child[A]'s corner B (= child[B]'s corner A, same world point).
    const bool signA   = childA->leaf->cornerInside[cA];
    const bool signMid = childA->leaf->cornerInside[cB];
    const bool signB   = node.children[cB]->leaf->cornerInside[cB];
    if (signA == signB && signMid != signA) return true;
  }
  return false;
}

// Walk a leaf's edges, accumulating crossings into a QEF.
void accumulateLeafIntoQef(const HermiteNode& leaf, svd::QefSolver& q) {
  if (!leaf.leaf) return;
  for (const auto& he : leaf.leaf->edges) {
    if (!he.hasCrossing) continue;
    if (he.normal.x == 0.0 && he.normal.y == 0.0 && he.normal.z == 0.0) continue;
    q.add(static_cast<float>(he.position.x),
          static_cast<float>(he.position.y),
          static_cast<float>(he.position.z),
          static_cast<float>(he.normal.x),
          static_cast<float>(he.normal.y),
          static_cast<float>(he.normal.z));
  }
}

// Try collapsing an internal node into a pseudo-leaf. Returns true on success.
// `pinvTol` is the QEF pseudo-inverse truncation threshold, threaded through
// from ContourerParams::qefRegularization so that leaf solves and collapse
// solves are regularised identically.
bool tryCollapse(HermiteNode& node, double errorThreshold, float pinvTol) {
  if (node.isLeaf) return false;

  // Require all 8 children to be surface leaves.
  for (int c = 0; c < 8; ++c) {
    const auto* ch = node.children[static_cast<std::size_t>(c)].get();
    if (ch == nullptr || !ch->isLeaf || !ch->leaf) return false;
  }

  // Topology-safe tests.
  if (hasInternalSignChange(node))   return false;
  if (hasOuterDoubleCrossing(node))  return false;
  if (hasSaddleFace(node))           return false;

  // Accumulate merged QEF.
  svd::QefSolver q;
  for (int c = 0; c < 8; ++c) {
    accumulateLeafIntoQef(*node.children[static_cast<std::size_t>(c)], q);
  }
  if (q.getData().numPoints == 0) return false;

  svd::Vec3 solved;
  q.solve(solved, kQefSvdTol, kQefSweepCount, pinvTol);

  // Acceptance test: the GEOMETRIC QEF energy at the solved point,
  //   E(x) = x^T (A^T A) x  -  2 x . (A^T b)  +  b^T b
  //        = sum over merged samples of (n_i . (x - p_i))^2,
  // i.e. the summed squared distance from the merged vertex to the planes the
  // merged Hermite samples define. Units are length^2, which is what
  // `errorThreshold` is documented to be.
  //
  // Note this is NOT what `solve()` returns. Its return value is
  // Svd::solveSymmetric's `calcError`, the residual of the NORMAL EQUATIONS
  // (||A^T b - A^T A x||^2 on the mass-point-centred system) -- a measure of
  // how badly the linear solve converged, not of how far the samples are from
  // the fit. The two are correlated but not interchangeable, and only the
  // former is portable across model scales.
  //
  // Clamp at zero: btb is accumulated in float and is not re-centred on the
  // mass point, so for geometry far from the world origin the subtraction can
  // round slightly negative on an otherwise perfect fit.
  const double qefEnergy = static_cast<double>(std::max(0.0f, q.getError(solved)));
  if (qefEnergy > errorThreshold) return false;

  // Build merged HermiteLeafData. Parent corner c sign = child[c]'s corner c sign
  // (same world position, since child[c] occupies parent's c-th octant).
  auto merged = std::make_unique<HermiteLeafData>();
  for (int c = 0; c < 8; ++c) {
    merged->cornerInside[static_cast<std::size_t>(c)] =
        node.children[static_cast<std::size_t>(c)]->leaf->cornerInside[static_cast<std::size_t>(c)];
  }
  // Each parent edge has 2 child sub-edges (both with the same local edge
  // index in their respective children). Take whichever sub-edge crosses.
  for (std::uint8_t e = 0; e < 12; ++e) {
    const auto endpoints = tables::kEdgeEndpoints[e];
    const auto& edgeA = node.children[endpoints[0]]->leaf->edges[e];
    const auto& edgeB = node.children[endpoints[1]]->leaf->edges[e];
    if (edgeA.hasCrossing) {
      merged->edges[e] = edgeA;
    } else if (edgeB.hasCrossing) {
      merged->edges[e] = edgeB;
    }
  }

  // Multi-component refusal: a pseudo-leaf carries one HermiteLeafData; the
  // contourer solves one QEF per surface component in it. If the merged
  // sign config splits into more than one component the collapse would
  // either bake two separate sheets into one summed QEF (wrong) or force
  // MDC to lose track of which component each outer edge belongs to. The
  // saddle-face / outer-double-crossing / internal-sign-change tests above
  // catch most of these; this catches the residual non-saddle cases such
  // as opposite-inside-corners.
  {
    std::array<bool, 12> mergedHasCrossing{};
    for (std::size_t e = 0; e < 12; ++e) {
      mergedHasCrossing[e] = merged->edges[e].hasCrossing;
    }
    const internal::EdgeComponents mergedParts =
        internal::partitionCubeEdges(merged->cornerInside, mergedHasCrossing);
    if (mergedParts.numComponents > 1) return false;
  }

  // Install on the parent and free children.
  node.leaf = std::move(merged);
  node.isLeaf = true;
  for (int c = 0; c < 8; ++c) node.children[static_cast<std::size_t>(c)].reset();
  return true;
}

// Bottom-up depth-first traversal: simplify children first, then attempt to
// collapse this node.
void simplifyRecursive(HermiteNode* node, double errorThreshold, float pinvTol,
                       const CancelToken* cancel) {
  if (node == nullptr || node->isLeaf) return;
  if (cancel) cancel->throwIfRequested("collapsing");
  for (int c = 0; c < 8; ++c) {
    simplifyRecursive(node->children[static_cast<std::size_t>(c)].get(),
                      errorThreshold, pinvTol, cancel);
  }
  tryCollapse(*node, errorThreshold, pinvTol);
}

// ===========================================================================
// Recursive contouring (cellProc / faceProc / edgeProc)
// ===========================================================================

struct ContourState {
  const ContourerParams& params;
  // Per-leaf vertex index, one slot per surface component (MDC). -1 = the
  // component is empty (no crossings) or has not been allocated yet. Slots
  // are populated lazily, all at once on the first reference into a leaf,
  // so subsequent edge lookups in the same leaf are a single array index.
  std::unordered_map<const HermiteNode*, std::array<int, 4>> vertIdxByLeafComp;
  std::vector<Vector3> positions;
  std::vector<Vector3> normals;
  std::vector<std::vector<std::size_t>> tris;
  // QEF solutions pre-computed in parallel before traversal (see
  // contourHermiteOctree). leafVertexIndex consults this instead of solving
  // lazily; a miss falls back to an inline solve so correctness never depends
  // on the pre-pass having covered the node.
  std::unordered_map<const HermiteNode*, MultiLeafSolve> leafSolveCache;
  // Cancellation checkpoint polled at every internal node of the traversal;
  // null when the caller passed none.
  const CancelToken* cancel = nullptr;
  // Progress: the traversal is serial and on the calling thread, so it may
  // report directly. `leafTotal` is the QEF pre-pass count (the first half of
  // the stage's total), `leavesTouched` counts first-touched leaves.
  ProgressSink* progress = nullptr;
  std::size_t leafTotal = 0;
  std::size_t leavesTouched = 0;
};

// Depth-first collection of every leaf (real or pseudo-leaf from a prior
// collapse) -- any node carrying HermiteLeafData.
void collectLeaves(const HermiteNode* node,
                   std::vector<const HermiteNode*>& out) {
  if (node == nullptr) return;
  if (node->leaf) out.push_back(node);
  for (int c = 0; c < 8; ++c) {
    collectLeaves(node->children[static_cast<std::size_t>(c)].get(), out);
  }
}

// Returns -1 if `localEdgeIdx` (0..11, the cube edge in `node`'s frame) has
// no crossing in this leaf -- i.e. the absolute edge it represents does not
// belong to any surface component in this cell. Otherwise returns the
// vertex index of the component containing that edge. Allocates the leaf's
// per-component vertices lazily on first touch.
int leafVertexIndex(const HermiteNode* node, int localEdgeIdx, ContourState& s) {
  if (node == nullptr || !node->leaf) return -1;

  // Resolve the leaf's MultiLeafSolve (cache hit, or inline solve fallback).
  auto cit = s.leafSolveCache.find(node);
  if (cit == s.leafSolveCache.end()) {
    cit = s.leafSolveCache
              .emplace(node, internal::solveLeaf(*node, s.params)).first;
  }
  const MultiLeafSolve& mls = cit->second;

  // Pick the component this edge belongs to. Two fallbacks for the case
  // where `localEdgeIdx` has no crossing in this leaf's stored data:
  //  * single-component leaf: use the lone component (legacy single-vertex
  //    DC semantics -- the leaf's only surface piece). Required for
  //    pseudo-leaves from `--collapse` whose outer edges don't all carry
  //    a crossing but whose single QEF vertex is still the right one to
  //    connect against.
  //  * multi-component leaf: bail (no triangle for this absolute edge).
  std::int8_t comp =
      mls.parts.componentOfEdge[static_cast<std::size_t>(localEdgeIdx)];
  if (comp < 0) {
    if (mls.parts.numComponents != 1) return -1;
    comp = 0;
  }

  // First touch of this leaf: allocate vertex indices for all valid
  // components up front, so later edge lookups never re-walk the QEF data.
  auto ait = s.vertIdxByLeafComp.find(node);
  if (ait == s.vertIdxByLeafComp.end()) {
    std::array<int, 4> slots{-1, -1, -1, -1};
    for (int c = 0; c < mls.parts.numComponents; ++c) {
      const LeafSolve& sol = mls.perComponent[static_cast<std::size_t>(c)];
      if (sol.hasVertex) {
        slots[static_cast<std::size_t>(c)] =
            static_cast<int>(s.positions.size());
        s.positions.push_back(sol.vertex);
        s.normals.push_back(sol.normal);
      }
    }
    ait = s.vertIdxByLeafComp.emplace(node, slots).first;
    if (s.progress && (++s.leavesTouched % 4096) == 0) {
      s.progress->report(Stage::Contour, s.leafTotal + s.leavesTouched,
                         2 * s.leafTotal);
    }
  }

  return ait->second[static_cast<std::size_t>(comp)];
}

// CCW-from-+axis quad orderings (in edgeProc's node[0..3] indices).
// node[0] = (P1=hi, P2=hi), node[1] = (P1=lo, P2=hi),
// node[2] = (P1=lo, P2=lo), node[3] = (P1=hi, P2=lo)
// where (P1, P2) are the perpendicular axes to the edge axis.
//
// Triangulation (n0, ni, nj, nk) emits triangles (n0, ni, nj) and (n0, nj, nk).
constexpr int kQuadCCWPlus[3][4] = {
    {0, 1, 2, 3}, // X-axis edge
    {0, 3, 2, 1}, // Y-axis edge
    {0, 1, 2, 3}, // Z-axis edge
};

void emitQuadAtEdge(const HermiteNode* const nodes[4], int axis,
                    bool corner0Inside, ContourState& s) {
  // For each of the 4 cells around the absolute edge, look up the vertex of
  // the component that contains this cell's local edge per kProcessEdgeMask.
  // Under MDC, two cells around the edge may resolve to different
  // components of the same pseudo-leaf -- the dedupe below handles either
  // way (same vertex collapses to a triangle, distinct vertices stay a
  // quad).
  const auto& localEdge =
      tables::kProcessEdgeMask[static_cast<std::size_t>(axis)];
  int v[4];
  for (int k = 0; k < 4; ++k) {
    v[k] = leafVertexIndex(nodes[k],
                           localEdge[static_cast<std::size_t>(k)], s);
    if (v[k] < 0) return;     // missing neighbor -> nothing to connect
  }

  // Walk the 4 cells in CCW order (per axis). When pseudo-leaves of varying
  // depth surround a fine edge, two of the slots can resolve to the same
  // cell -- the polygon collapses from a quad to a triangle. Drop adjacent
  // duplicates (and the wrap-around duplicate) before triangulating, then
  // emit either 1 or 2 triangles depending on how many unique vertices we
  // have. Skipping the degenerate case entirely (as the previous version
  // did) leaves boundary edges == holes in the output mesh.
  const int (&order)[4] = kQuadCCWPlus[static_cast<std::size_t>(axis)];
  int polygon[4] = {v[order[0]], v[order[1]], v[order[2]], v[order[3]]};

  int unique[4];
  int n = 0;
  for (int k = 0; k < 4; ++k) {
    bool dup = false;
    for (int j = 0; j < n; ++j) if (unique[j] == polygon[k]) { dup = true; break; }
    if (!dup) unique[n++] = polygon[k];
  }
  if (n < 3) return;          // genuine degenerate (the entire absolute edge sits inside a single cell)

  const std::size_t u0 = static_cast<std::size_t>(unique[0]);
  const std::size_t u1 = static_cast<std::size_t>(unique[1]);
  const std::size_t u2 = static_cast<std::size_t>(unique[2]);

  if (corner0Inside) {
    // Outward normal in +axis direction; CCW from +axis camera.
    s.tris.push_back({u0, u1, u2});
    if (n == 4) {
      const std::size_t u3 = static_cast<std::size_t>(unique[3]);
      s.tris.push_back({u0, u2, u3});
    }
  } else {
    s.tris.push_back({u0, u2, u1});
    if (n == 4) {
      const std::size_t u3 = static_cast<std::size_t>(unique[3]);
      s.tris.push_back({u0, u3, u2});
    }
  }
}

void cellProc(const HermiteNode* node, ContourState& s);
void faceProc(const HermiteNode* a, const HermiteNode* b, int axis, ContourState& s);
void edgeProc(const HermiteNode* n0, const HermiteNode* n1,
              const HermiteNode* n2, const HermiteNode* n3,
              int axis, ContourState& s);

void cellProc(const HermiteNode* node, ContourState& s) {
  if (node == nullptr || node->isLeaf) return;
  if (s.cancel) s.cancel->throwIfRequested("contouring");
  for (int c = 0; c < 8; ++c) {
    cellProc(node->children[static_cast<std::size_t>(c)].get(), s);
  }
  for (const auto& m : tables::kCellProcFaceMask) {
    faceProc(node->children[m[0]].get(), node->children[m[1]].get(), m[2], s);
  }
  for (const auto& m : tables::kCellProcEdgeMask) {
    edgeProc(node->children[m[0]].get(),
             node->children[m[1]].get(),
             node->children[m[2]].get(),
             node->children[m[3]].get(),
             m[4], s);
  }
}

inline const HermiteNode* childOrSelf(const HermiteNode* n, std::uint8_t childIdx) {
  if (n == nullptr) return nullptr;
  if (n->isLeaf) return n;
  return n->children[childIdx].get();
}

void faceProc(const HermiteNode* a, const HermiteNode* b, int axis, ContourState& s) {
  if (a == nullptr || b == nullptr) return;
  const bool aLeaf = a->isLeaf;
  const bool bLeaf = b->isLeaf;
  if (aLeaf && bLeaf) return;     // face emits no triangles directly

  // Recurse into 4 sub-faces and 4 internal edges.
  const auto& subFaces = tables::kFaceProcFaceMask[static_cast<std::size_t>(axis)];
  for (const auto& sf : subFaces) {
    faceProc(childOrSelf(a, sf[0]), childOrSelf(b, sf[1]), axis, s);
  }

  const auto& subEdges = tables::kFaceProcEdgeMask[static_cast<std::size_t>(axis)];
  for (const auto& se : subEdges) {
    const HermiteNode* nodes[4];
    for (int k = 0; k < 4; ++k) {
      const HermiteNode* src = (se.childOf[k] == 0) ? a : b;
      nodes[k] = childOrSelf(src, se.childIdx[k]);
    }
    edgeProc(nodes[0], nodes[1], nodes[2], nodes[3], se.edgeAxis, s);
  }
}

void edgeProc(const HermiteNode* n0, const HermiteNode* n1,
              const HermiteNode* n2, const HermiteNode* n3,
              int axis, ContourState& s) {
  if (n0 == nullptr || n1 == nullptr || n2 == nullptr || n3 == nullptr) return;
  const HermiteNode* arr[4] = {n0, n1, n2, n3};

  // Terminal: all 4 are leaves. Pick the FINEST cell (largest depth) -- its
  // local edge per kProcessEdgeMask is the absolute edge whose sign change
  // we need to test. A coarser pseudo-leaf's local-edge endpoints sit on
  // its OUTER 8-corner cube, which is NOT the absolute fine edge if that
  // edge lies interior to the coarse cell's outer surface.
  const bool allLeaves = arr[0]->isLeaf && arr[1]->isLeaf
                       && arr[2]->isLeaf && arr[3]->isLeaf;
  if (allLeaves) {
    int finestK = 0;
    for (int k = 1; k < 4; ++k) {
      if (arr[k]->depth > arr[finestK]->depth) finestK = k;
    }
    const HermiteNode* finest = arr[finestK];
    if (!finest->leaf) return;       // no sign data

    const std::uint8_t e = tables::kProcessEdgeMask[static_cast<std::size_t>(axis)]
                                                   [static_cast<std::size_t>(finestK)];
    const auto& endpoints = tables::kEdgeEndpoints[e];
    const bool sa = finest->leaf->cornerInside[endpoints[0]];
    const bool sb = finest->leaf->cornerInside[endpoints[1]];
    if (sa == sb) return;

    // For all finestK values, kProcessEdgeMask is set so that endpoint 0 of
    // the local edge sits at the LOW end of the absolute edge along its
    // axis (e.g. at lo-x for an X-axis edge). So `sa` is always the sign
    // at the low end of the axis -- consistent with the
    // kQuadCCWPlus winding convention.
    emitQuadAtEdge(arr, axis, /*corner0Inside=*/sa, s);
    return;
  }

  // Recurse into 2 sub-edges along the edge axis.
  const auto& sub = tables::kEdgeProcEdgeMask[static_cast<std::size_t>(axis)];
  for (const auto& halfChildren : sub) {
    const HermiteNode* h[4];
    for (int k = 0; k < 4; ++k) {
      h[k] = childOrSelf(arr[k], halfChildren[k]);
    }
    edgeProc(h[0], h[1], h[2], h[3], axis, s);
  }
}

} // namespace

// ===========================================================================
// Public entries
// ===========================================================================

void simplifyHermiteOctree(HermiteOctree& octree, const ContourerParams& params,
                           const CancelToken* cancel) {
  if (params.simplificationError <= 0.0) return;
  const float pinvTol = (params.qefRegularization > 0.0)
                            ? static_cast<float>(params.qefRegularization)
                            : kQefPinvDefault;
  simplifyRecursive(octree.root(), params.simplificationError, pinvTol, cancel);
}

void simplifyHermiteOctree(HermiteOctree& octree, double errorThreshold) {
  ContourerParams params;
  params.simplificationError = errorThreshold;
  simplifyHermiteOctree(octree, params);
}


std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
           std::vector<Vector3>>
contourHermiteOctree(const HermiteOctree& octree, const ContourerParams& params,
                     Diagnostics* diag, const CancelToken* cancel,
                     ProgressSink* progress) {
  ContourState state{params, {}, {}, {}, {}, {}};
  state.cancel   = cancel;
  state.progress = progress;
  if (cancel) cancel->throwIfRequested("contouring");

  // Pre-solve every leaf's QEF (one per surface component under MDC) across
  // all hardware threads. solveLeaf is a pure deterministic function of the
  // leaf, so the cached results are bit-identical to solving lazily -- the
  // traversal below still allocates vertex indices in its own serial order.
  // (The thread count stays resolveThreadCount(0): ContourerParams has no
  // thread knob -- that absence is open item #34, not this pass's business.)
  std::vector<const HermiteNode*> leaves;
  collectLeaves(octree.root(), leaves);
  std::vector<internal::MultiLeafSolve> solved(leaves.size());
  const std::size_t leafTotal = leaves.size();
  state.leafTotal = leafTotal;
  const auto solveOne = [&](std::size_t i) {
    if (cancel) cancel->throwIfRequested("contouring");
    solved[i] = internal::solveLeaf(*leaves[i], params);
  };
  if (progress) {
    // Stage total is 2N: the first half is this pre-pass, the second half is
    // leaves first touched by the serial traversal below.
    progress->report(Stage::Contour, 0, 2 * leafTotal);
    internal::parallelForPolled(
        leafTotal, internal::resolveThreadCount(0), solveOne,
        [&](std::size_t done, std::size_t) {
          progress->report(Stage::Contour, done, 2 * leafTotal);
        });
  } else {
    internal::parallelFor(leafTotal, internal::resolveThreadCount(0), solveOne);
  }
  state.leafSolveCache.reserve(leaves.size());
  for (std::size_t i = 0; i < leaves.size(); ++i) {
    state.leafSolveCache.emplace(leaves[i], std::move(solved[i]));
  }

  // Recursive contouring. Each leaf's QEF vertex is allocated lazily on
  // first reference inside emitQuadAtEdge -> leafVertexIndex.
  cellProc(octree.root(), state);
  if (progress) progress->report(Stage::Contour, 2 * leafTotal, 2 * leafTotal);

  std::vector<Vector3>                  positions = std::move(state.positions);
  std::vector<Vector3>                  normals   = std::move(state.normals);
  std::vector<std::vector<std::size_t>> tris      = std::move(state.tris);

  // Allocation is lazy per LEAF but eager per COMPONENT: leafVertexIndex
  // fills in every valid component of a leaf on first touch, and
  // emitQuadAtEdge allocates for nodes[0..k-1] before discovering that slot k
  // has no vertex and bailing. So a position can end up referenced by no
  // triangle, and this pass is load-bearing for that case -- not, as an
  // earlier comment here claimed, merely defence against some future change.
  if (!positions.empty() && !tris.empty()) {
    std::vector<bool> used(positions.size(), false);
    for (const auto& t : tris) for (auto i : t) used[i] = true;
    bool anyUnused = false;
    for (bool u : used) if (!u) { anyUnused = true; break; }
    if (anyUnused) {
      std::vector<std::size_t> remap(positions.size(),
                                     static_cast<std::size_t>(-1));
      std::vector<Vector3> compactPos;
      std::vector<Vector3> compactNor;
      compactPos.reserve(positions.size());
      compactNor.reserve(positions.size());
      for (std::size_t i = 0; i < positions.size(); ++i) {
        if (used[i]) {
          remap[i] = compactPos.size();
          compactPos.push_back(positions[i]);
          compactNor.push_back(normals[i]);
        }
      }
      for (auto& t : tris) for (auto& i : t) i = remap[i];
      positions = std::move(compactPos);
      normals   = std::move(compactNor);
    }
  }

  // Empty-output fallback: a single placeholder triangle keeps geometry-
  // central's SurfaceMesh ctor happy when the input had no surface. Without
  // Diagnostics::emptyContour the caller cannot tell it apart from a real
  // one-triangle result -- there is no other error channel out of here.
  if (positions.empty() || tris.empty()) {
    if (diag) diag->emptyContour = true;
    positions = {Vector3{0.0, 0.0, 0.0},
                 Vector3{1.0, 0.0, 0.0},
                 Vector3{0.0, 1.0, 0.0}};
    normals   = {Vector3{0.0, 0.0, 1.0},
                 Vector3{0.0, 0.0, 1.0},
                 Vector3{0.0, 0.0, 1.0}};
    tris = {{0, 1, 2}};
  }

  // Report on what is actually being returned -- the placeholder included, so
  // the counts always describe the mesh the caller receives. `emptyContour`
  // is what separates the placeholder from a genuine one-triangle surface.
  if (diag) {
    diag->outputVertices  = positions.size();
    diag->outputTriangles = tris.size();
    tallyEdges(tris, positions.size(), diag->outputBoundaryEdges,
               diag->outputNonManifoldEdges);
    diag->outputWatertight = diag->outputBoundaryEdges == 0 &&
                             diag->outputNonManifoldEdges == 0;
  }

  auto built = geometrycentral::surface::makeSurfaceMeshAndGeometry(tris, positions);
  return std::make_tuple(std::move(std::get<0>(built)),
                         std::move(std::get<1>(built)),
                         std::move(normals));
}

} // namespace dualc
