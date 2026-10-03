#include "dualc/contourer.h"
#include "dualc/hermite_octree.h"

#include "internal/contourer_internals.h"
#include "internal/dc_tables.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

using namespace dualc;

TEST_CASE("Contourer stub returns non-null mesh + geometry pointers",
          "[contourer]") {
  HermiteOctree octree(BBox::unit());
  ContourerParams params;
  auto [mesh, geom, normals] = contourHermiteOctree(octree, params);
  REQUIRE(mesh != nullptr);
  REQUIRE(geom != nullptr);
  // v0 stub: emits a single placeholder triangle so the mesh is well-formed.
  REQUIRE(mesh->nVertices() == 3);
  REQUIRE(mesh->nFaces() == 1);
  REQUIRE(normals.size() == 3);
}

namespace {

// Build a BBox::unit (= [-0.5, 0.5]^3) HermiteLeafData with corners 0 and 7
// marked inside (opposite body-diagonal). The 6 outgoing edges from those
// two corners are fabricated with axis-aligned crossings 0.2 units in from
// the inside corner: the QEF for the corner-0 component intersects at
// (-0.3, -0.3, -0.3); for the corner-7 component, at (0.3, 0.3, 0.3).
std::unique_ptr<HermiteLeafData> makeDoubleCornerLeaf() {
  auto leaf = std::make_unique<HermiteLeafData>();
  leaf->cornerInside.fill(false);
  leaf->cornerInside[0] = true;
  leaf->cornerInside[7] = true;

  auto setEdge = [&](int e, Vector3 pos, Vector3 nrm) {
    leaf->edges[static_cast<std::size_t>(e)].position    = pos;
    leaf->edges[static_cast<std::size_t>(e)].normal      = nrm;
    leaf->edges[static_cast<std::size_t>(e)].hasCrossing = true;
  };

  // Edges from corner 0 = (-0.5, -0.5, -0.5): outward normal = +axis,
  // crossing 0.2 along the axis from the corner.
  setEdge(0, Vector3{-0.3, -0.5, -0.5}, Vector3{ 1.0,  0.0,  0.0}); // 0-1, X
  setEdge(4, Vector3{-0.5, -0.3, -0.5}, Vector3{ 0.0,  1.0,  0.0}); // 0-2, Y
  setEdge(8, Vector3{-0.5, -0.5, -0.3}, Vector3{ 0.0,  0.0,  1.0}); // 0-4, Z

  // Edges from corner 7 = (0.5, 0.5, 0.5): outward normal = -axis,
  // crossing 0.2 along the axis from the corner.
  setEdge(3,  Vector3{ 0.3,  0.5,  0.5}, Vector3{-1.0,  0.0,  0.0}); // 6-7, X
  setEdge(7,  Vector3{ 0.5,  0.3,  0.5}, Vector3{ 0.0, -1.0,  0.0}); // 5-7, Y
  setEdge(11, Vector3{ 0.5,  0.5,  0.3}, Vector3{ 0.0,  0.0, -1.0}); // 3-7, Z

  return leaf;
}

HermiteNode makeUnitLeafNode(std::unique_ptr<HermiteLeafData> leaf) {
  HermiteNode node;
  node.bounds = BBox::unit();
  node.depth  = 0;
  node.isLeaf = true;
  node.leaf   = std::move(leaf);
  return node;
}

bool near(double a, double b, double eps = 1e-5) {
  return std::abs(a - b) < eps;
}

} // namespace

TEST_CASE("MDC: opposite-corner leaf yields two well-separated vertices",
          "[contourer][mdc]") {
  HermiteNode node = makeUnitLeafNode(makeDoubleCornerLeaf());
  ContourerParams params;
  params.manifoldDC = true;

  const auto mls = internal::solveLeaf(node, params);
  REQUIRE(mls.parts.numComponents == 2);

  // Both components have a vertex; both vertices land at the QEF minimum of
  // the three axis-aligned planes intersecting at (0.2,0.2,0.2) and
  // (0.8,0.8,0.8) respectively. Component IDs are order-dependent, so check
  // both orderings.
  REQUIRE(mls.perComponent[0].hasVertex);
  REQUIRE(mls.perComponent[1].hasVertex);

  const Vector3 va = mls.perComponent[0].vertex;
  const Vector3 vb = mls.perComponent[1].vertex;

  CAPTURE(va.x, va.y, va.z, vb.x, vb.y, vb.z);

  // The two components should land at the 3-orthogonal-planes intersection
  // near each inside corner: (-0.3, -0.3, -0.3) and (0.3, 0.3, 0.3).
  // Component IDs are order-dependent, so check both orderings.
  const double eps = 1e-4;
  const bool ab = near(va.x, -0.3, eps) && near(va.y, -0.3, eps) && near(va.z, -0.3, eps) &&
                  near(vb.x,  0.3, eps) && near(vb.y,  0.3, eps) && near(vb.z,  0.3, eps);
  const bool ba = near(va.x,  0.3, eps) && near(va.y,  0.3, eps) && near(va.z,  0.3, eps) &&
                  near(vb.x, -0.3, eps) && near(vb.y, -0.3, eps) && near(vb.z, -0.3, eps);
  REQUIRE((ab || ba));
}

TEST_CASE("MDC opt-out: same leaf collapses to a single mid-cube vertex",
          "[contourer][mdc]") {
  HermiteNode node = makeUnitLeafNode(makeDoubleCornerLeaf());
  ContourerParams params;
  params.manifoldDC = false;

  const auto mls = internal::solveLeaf(node, params);
  REQUIRE(mls.parts.numComponents == 1);
  REQUIRE(mls.perComponent[0].hasVertex);

  // The 6-plane QEF (3 planes at -0.3 + 3 planes at +0.3 per axis)
  // minimises at the centroid (0, 0, 0).
  const Vector3 v = mls.perComponent[0].vertex;
  REQUIRE(near(v.x, 0.0));
  REQUIRE(near(v.y, 0.0));
  REQUIRE(near(v.z, 0.0));
}

// ===========================================================================
// Adaptive cell collapse -- the three topology gates and the error gate
// ===========================================================================
//
// `simplifyHermiteOctree` had no test at all before this. The cases below
// hand-build a parent whose 8 children are leaves and check each refusal
// reason in isolation, plus two configurations that must actually collapse.
//
// Signs are authored on the 3x3x3 lattice of corner positions spanning the
// parent [-1,1]^3: lattice (i,j,k) sits at world (i-1, j-1, k-1). Child c
// occupies octant c, and child c's local corner q is lattice
// (cx+qx, cy+qy, cz+qz) -- so lattice (2cx,2cy,2cz) is exactly the parent
// corner that the collapse reads out of child c, and the six internal edges
// are the segments from lattice (1,1,1) to the six face centres. Authoring at
// this level therefore controls every gate exactly.

namespace {

using LatticeSign = std::function<bool(int, int, int)>;

// One override of a fabricated edge crossing, used to shape the merged QEF.
struct EdgeOverride {
  int     child;
  int     edge;
  Vector3 position;
  Vector3 normal;
};

Vector3 latticeWorld(int i, int j, int k) {
  return Vector3{static_cast<double>(i) - 1.0, static_cast<double>(j) - 1.0,
                 static_cast<double>(k) - 1.0};
}

// Build the 8-child parent. Every child leaf satisfies the sampler's
// invariant (a crossing on an edge iff its two endpoint signs differ);
// crossings default to the edge midpoint with an arbitrary unit normal, which
// is enough for the gates that refuse before the QEF is ever solved.
HermiteOctree makeCollapseCandidate(
    const LatticeSign& inside,
    const std::vector<EdgeOverride>& overrides = {}) {
  BBox rootBounds;
  rootBounds.min = Vector3{-1.0, -1.0, -1.0};
  rootBounds.max = Vector3{ 1.0,  1.0,  1.0};

  auto root    = std::make_unique<HermiteNode>();
  root->bounds = rootBounds;
  root->depth  = 0;
  root->isLeaf = false;

  for (int c = 0; c < 8; ++c) {
    const int cx = c & 1, cy = (c >> 1) & 1, cz = (c >> 2) & 1;
    auto child    = std::make_unique<HermiteNode>();
    child->depth  = 1;
    child->isLeaf = true;
    child->bounds.min = latticeWorld(cx, cy, cz);
    child->bounds.max = latticeWorld(cx + 1, cy + 1, cz + 1);

    auto leaf = std::make_unique<HermiteLeafData>();
    auto latticeOf = [&](std::size_t q) {
      const int qi = static_cast<int>(q);
      return std::array<int, 3>{cx + (qi & 1), cy + ((qi >> 1) & 1),
                                cz + ((qi >> 2) & 1)};
    };
    for (std::size_t q = 0; q < 8; ++q) {
      const auto l = latticeOf(q);
      leaf->cornerInside[q] = inside(l[0], l[1], l[2]);
    }
    for (std::size_t e = 0; e < 12; ++e) {
      const auto ep = tables::kEdgeEndpoints[e];
      if (leaf->cornerInside[ep[0]] == leaf->cornerInside[ep[1]]) continue;
      const auto la = latticeOf(ep[0]);
      const auto lb = latticeOf(ep[1]);
      auto& he       = leaf->edges[e];
      he.hasCrossing = true;
      he.position    = (latticeWorld(la[0], la[1], la[2]) +
                     latticeWorld(lb[0], lb[1], lb[2])) * 0.5;
      he.normal      = Vector3{0.0, 0.0, 1.0};
    }
    child->leaf = std::move(leaf);
    root->children[static_cast<std::size_t>(c)] = std::move(child);
  }

  for (const auto& o : overrides) {
    auto& he = root->children[static_cast<std::size_t>(o.child)]
                   ->leaf->edges[static_cast<std::size_t>(o.edge)];
    REQUIRE(he.hasCrossing);  // an override must land on a real crossing
    he.position = o.position;
    he.normal   = o.normal;
  }

  HermiteOctree octree(rootBounds);
  octree.setRoot(std::move(root));
  return octree;
}

// Everything inside except the single lattice point (2,2,2) = parent corner 7.
// Clips one corner: no internal edge crosses, no outer edge double-crosses,
// no face saddles, and the three crossings form one surface component.
bool cornerClip(int i, int j, int k) { return !(i == 2 && j == 2 && k == 2); }

} // namespace

TEST_CASE("Collapse merges a corner-clip configuration", "[contourer][collapse]") {
  // Put the three crossings on one exact plane, so the merged QEF energy is
  // zero and even a minuscule threshold accepts.
  const double s = 1.0 / std::sqrt(3.0);
  const Vector3 n{s, s, s};
  HermiteOctree octree = makeCollapseCandidate(
      cornerClip, {{7, 3,  Vector3{0.4, 1.0, 1.0}, n},
                   {7, 7,  Vector3{1.0, 0.4, 1.0}, n},
                   {7, 11, Vector3{1.0, 1.0, 0.4}, n}});
  REQUIRE(octree.leafCount() == 8);

  simplifyHermiteOctree(octree, 1e-9);

  REQUIRE(octree.root()->isLeaf);
  REQUIRE(octree.leafCount() == 1);
  REQUIRE(octree.root()->leaf != nullptr);
  // Merged corner signs: only parent corner 7 outside.
  for (std::size_t c = 0; c < 8; ++c) {
    REQUIRE(octree.root()->leaf->cornerInside[c] == (c != 7));
  }
}

TEST_CASE("Collapse thresholds the geometric QEF energy", "[contourer][collapse]") {
  // Three parallel planes at slightly different offsets: rank 1, so no vertex
  // fits all three and the residual energy is sum (d_i - dbar)^2 over the unit
  // normal, about 6.7e-3. A threshold below that must refuse and one above it
  // must accept -- which pins the acceptance test to the QEF energy rather
  // than to some other correlated quantity.
  const double s = 1.0 / std::sqrt(3.0);
  const Vector3 n{s, s, s};
  const std::vector<EdgeOverride> tilted = {
      {7, 3,  Vector3{0.4, 1.0, 1.0}, n},
      {7, 7,  Vector3{1.0, 0.5, 1.0}, n},
      {7, 11, Vector3{1.0, 1.0, 0.6}, n}};

  HermiteOctree tight = makeCollapseCandidate(cornerClip, tilted);
  simplifyHermiteOctree(tight, 1e-4);
  CHECK_FALSE(tight.root()->isLeaf);

  HermiteOctree loose = makeCollapseCandidate(cornerClip, tilted);
  simplifyHermiteOctree(loose, 1e-1);
  CHECK(loose.root()->isLeaf);
}

TEST_CASE("Collapse refuses on an internal sign change", "[contourer][collapse]") {
  // A horizontal surface through the parent's upper half crosses the hi-z
  // internal edge (lattice (1,1,1)-(1,1,2)). Nothing else trips: the outer Z
  // edges change sign once each and no face saddles.
  HermiteOctree octree =
      makeCollapseCandidate([](int, int, int k) { return k <= 1; });
  simplifyHermiteOctree(octree, 1e6);
  CHECK_FALSE(octree.root()->isLeaf);
  CHECK(octree.leafCount() == 8);
}

TEST_CASE("Collapse refuses on a hidden outer double crossing",
          "[contourer][collapse]") {
  // Only the midpoint of parent edge 0 is outside: its two endpoints agree,
  // so the feature would vanish on collapse. No internal edge touches lattice
  // (1,0,0), so the internal gate passes first and this one is what refuses.
  HermiteOctree octree = makeCollapseCandidate(
      [](int i, int j, int k) { return !(i == 1 && j == 0 && k == 0); });
  simplifyHermiteOctree(octree, 1e6);
  CHECK_FALSE(octree.root()->isLeaf);
  CHECK(octree.leafCount() == 8);
}

TEST_CASE("Collapse refuses on a saddle face", "[contourer][collapse]") {
  // Parent corners 1 and 2 outside, 0 and 3 inside: the bottom face
  // alternates, so the surface must cross it on four boundary edges and a
  // single merged vertex cannot represent that. Every internal-edge endpoint
  // and every outer edge midpoint stays inside, so the two earlier gates pass.
  HermiteOctree octree = makeCollapseCandidate([](int i, int j, int k) {
    if (k != 0) return true;
    if (i == 2 && j == 0) return false;  // parent corner 1
    if (i == 0 && j == 2) return false;  // parent corner 2
    return true;
  });
  simplifyHermiteOctree(octree, 1e6);
  CHECK_FALSE(octree.root()->isLeaf);
  CHECK(octree.leafCount() == 8);
}
