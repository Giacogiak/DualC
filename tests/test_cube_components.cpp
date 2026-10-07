#include "internal/cube_components.h"
#include "internal/dc_tables.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <numeric>
#include <set>
#include <vector>

namespace {

// Derive hasCrossing[12] from corner signs: each cube edge crosses iff its
// two endpoints disagree.
std::array<bool, 12> crossingsFromCorners(const std::array<bool, 8>& signs) {
  std::array<bool, 12> out{};
  for (std::size_t e = 0; e < 12; ++e) {
    const auto& ep = dualc::tables::kEdgeEndpoints[e];
    out[e] = (signs[ep[0]] != signs[ep[1]]);
  }
  return out;
}

// Set of edge indices in a given component (utility for membership checks).
std::set<int> edgesInComponent(const dualc::internal::EdgeComponents& parts,
                               int componentId) {
  std::set<int> out;
  for (int e = 0; e < 12; ++e) {
    if (parts.componentOfEdge[static_cast<std::size_t>(e)] == componentId) {
      out.insert(e);
    }
  }
  return out;
}

} // namespace

using namespace dualc;
using namespace dualc::internal;

TEST_CASE("partitionCubeEdges: all-outside has no components", "[cube_components]") {
  std::array<bool, 8> signs{};
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 0);
  for (int e = 0; e < 12; ++e) {
    REQUIRE(parts.componentOfEdge[static_cast<std::size_t>(e)] == -1);
  }
}

TEST_CASE("partitionCubeEdges: all-inside has no components", "[cube_components]") {
  std::array<bool, 8> signs;
  signs.fill(true);
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 0);
}

TEST_CASE("partitionCubeEdges: one inside corner gives one component",
          "[cube_components]") {
  // Corner 0 inside, all others outside. Edges 0 (0-1), 4 (0-2), 8 (0-4)
  // cross; everything else is uniform.
  std::array<bool, 8> signs{};
  signs[0] = true;
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 1);
  REQUIRE(edgesInComponent(parts, 0) == std::set<int>{0, 4, 8});
}

TEST_CASE("partitionCubeEdges: two adjacent inside corners merge",
          "[cube_components]") {
  // Corners 0 and 1 share edge 0 (X-aligned). The surface is one connected
  // patch -- 1 component covering 4 crossings: edges 4, 5, 8, 9.
  std::array<bool, 8> signs{};
  signs[0] = true;
  signs[1] = true;
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 1);
  REQUIRE(edgesInComponent(parts, 0) == std::set<int>{4, 5, 8, 9});
}

TEST_CASE("partitionCubeEdges: body-diagonal corners give 2 components",
          "[cube_components]") {
  // Corners 0 and 7 (opposite body-diagonal). Surface is 2 small patches,
  // one bump per inside corner.
  //   corner 0 -> edges {0, 4, 8}
  //   corner 7 -> edges {3, 7, 11}
  std::array<bool, 8> signs{};
  signs[0] = true;
  signs[7] = true;
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 2);

  // Which component got id 0 vs 1 is order-dependent; verify the two
  // expected sets are present, irrespective of label.
  const auto a = edgesInComponent(parts, 0);
  const auto b = edgesInComponent(parts, 1);
  const std::set<int> patchA{0, 4, 8};
  const std::set<int> patchB{3, 7, 11};
  REQUIRE(((a == patchA && b == patchB) || (a == patchB && b == patchA)));
}

TEST_CASE("partitionCubeEdges: face-diagonal corners (saddle face) give 2 components",
          "[cube_components]") {
  // Corners 0 and 3 sit on the z=0 face but are not edge-adjacent: face z=0
  // has signs (in, out, in, out), the classic MC saddle. Under the inside-
  // corner pairing rule, the 4 face crossings split into 2 face-local
  // components, which glue with the other faces' 2-change links to give
  // exactly 2 cube-level components, one centred on corner 0 and one on
  // corner 3.
  std::array<bool, 8> signs{};
  signs[0] = true;
  signs[3] = true;
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 2);

  const auto a = edgesInComponent(parts, 0);
  const auto b = edgesInComponent(parts, 1);
  // edges from corner 0 = {0, 4, 8}; edges from corner 3 = {1, 5, 11}
  const std::set<int> patchA{0, 4, 8};
  const std::set<int> patchB{1, 5, 11};
  REQUIRE(((a == patchA && b == patchB) || (a == patchB && b == patchA)));
}

TEST_CASE("partitionCubeEdges: a face of inside corners gives one flat component",
          "[cube_components]") {
  // Corners 0,1,2,3 inside (the z=0 face). The surface is a single plane
  // across the cube; the 4 z-aligned edges cross.
  std::array<bool, 8> signs{};
  for (int c = 0; c < 4; ++c) signs[c] = true;
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 1);
  REQUIRE(edgesInComponent(parts, 0) == std::set<int>{8, 9, 10, 11});
}

TEST_CASE("partitionCubeEdges: four independent inside corners give 4 components",
          "[cube_components]") {
  // Corners {0, 3, 5, 6} -- a maximum independent set on the cube graph
  // (no two share an edge). Each gives one isolated 3-edge component, so
  // partition has 4 components, all 12 edges crossing.
  std::array<bool, 8> signs{};
  signs[0] = true;
  signs[3] = true;
  signs[5] = true;
  signs[6] = true;
  auto parts = partitionCubeEdges(signs, crossingsFromCorners(signs));
  REQUIRE(parts.numComponents == 4);

  // Every edge crosses.
  for (int e = 0; e < 12; ++e) {
    REQUIRE(parts.componentOfEdge[static_cast<std::size_t>(e)] >= 0);
  }
  // Each component has exactly 3 edges (the 3 outgoing edges of one inside
  // corner). Verify by counting.
  std::array<int, 4> sizes{};
  for (int e = 0; e < 12; ++e) {
    const int c = parts.componentOfEdge[static_cast<std::size_t>(e)];
    REQUIRE(c >= 0);
    REQUIRE(c < 4);
    sizes[static_cast<std::size_t>(c)] += 1;
  }
  for (int s : sizes) REQUIRE(s == 3);
}

TEST_CASE("partitionCubeEdges: symmetry -- inverting all signs preserves partition",
          "[cube_components]") {
  // The component structure depends on which corners are inside, but in
  // the saddle disambiguation it favours inside corners. Flipping all
  // signs should flip which corners are 'inside', which can change saddle
  // pairing. Test on a non-saddle config (corner 0 only) where flipping
  // gives the dual (7 corners inside) but the same single connected
  // surface -- still one component, same edges.
  std::array<bool, 8> a{};
  a[0] = true;
  auto pa = partitionCubeEdges(a, crossingsFromCorners(a));

  std::array<bool, 8> b;
  for (int c = 0; c < 8; ++c) b[static_cast<std::size_t>(c)] = !a[static_cast<std::size_t>(c)];
  auto pb = partitionCubeEdges(b, crossingsFromCorners(b));

  REQUIRE(pa.numComponents == pb.numComponents);
  REQUIRE(edgesInComponent(pa, 0) == edgesInComponent(pb, 0));
}

// ===========================================================================
// All 256 sign configurations (roadmap 17 #32, audit C45)
// ===========================================================================
//
// The function is pure, O(1) and allocation-free, so every input can be
// checked. The hand-picked cases above stay: they name the expected edge sets.
// This sweep checks, for every configuration, the invariants that make the
// per-component vertices a manifold surface:
//   1. an edge carries a component iff it crosses, and the ids are dense;
//   2. on every face, each crossing has exactly one partner in its own
//      component -- the face's other crossing, or on a saddle face the one
//      sharing its inside corner -- so each component closes into one loop
//      around the cube;
//   3. the number of components agrees with an oracle that never pairs
//      crossings at all: k disjoint loops on the cube's surface (a sphere)
//      cut it into k + 1 regions, so k = inside regions + outside regions - 1.
//      Inside corners join along cube edges; outside corners also join across
//      a saddle face's diagonal, because the inside-pair rule cuts the inside
//      corners off and leaves the outside ones connected.

namespace {

// The cube edge joining corners a and b, or -1 when they are not adjacent.
int edgeBetween(int a, int b) {
  for (int e = 0; e < 12; ++e) {
    const auto& ep = dualc::tables::kEdgeEndpoints[static_cast<std::size_t>(e)];
    if ((ep[0] == a && ep[1] == b) || (ep[0] == b && ep[1] == a)) return e;
  }
  return -1;
}

bool isSaddleFace(const std::array<bool, 8>& signs, int f) {
  const auto& fc = dualc::tables::kFaceCorners[static_cast<std::size_t>(f)];
  int changes = 0;
  for (int i = 0; i < 4; ++i)
    if (signs[fc[static_cast<std::size_t>(i)]] !=
        signs[fc[static_cast<std::size_t>((i + 1) % 4)]])
      ++changes;
  return changes == 4;
}

// Connected components among the corners with `signs[c] == side`.
int regionsOnSide(const std::array<bool, 8>& signs, bool side) {
  std::array<int, 8> parent;
  std::iota(parent.begin(), parent.end(), 0);
  auto find = [&](int c) {
    while (parent[static_cast<std::size_t>(c)] != c)
      c = parent[static_cast<std::size_t>(c)];
    return c;
  };
  auto join = [&](int a, int b) {
    if (signs[static_cast<std::size_t>(a)] != side ||
        signs[static_cast<std::size_t>(b)] != side)
      return;
    parent[static_cast<std::size_t>(find(a))] = find(b);
  };
  for (const auto& ep : dualc::tables::kEdgeEndpoints) join(ep[0], ep[1]);
  if (!side) {  // outside corners also meet across a saddle face's diagonal
    for (int f = 0; f < 6; ++f) {
      if (!isSaddleFace(signs, f)) continue;
      const auto& fc = dualc::tables::kFaceCorners[static_cast<std::size_t>(f)];
      join(fc[0], fc[2]);
      join(fc[1], fc[3]);
    }
  }
  std::set<int> roots;
  for (int c = 0; c < 8; ++c)
    if (signs[static_cast<std::size_t>(c)] == side) roots.insert(find(c));
  return static_cast<int>(roots.size());
}

} // namespace

TEST_CASE("partitionCubeEdges: invariants hold on all 256 sign configurations",
          "[cube_components]") {
  std::map<int, int> byCount;  // numComponents -> configurations
  for (int config = 0; config < 256; ++config) {
    std::array<bool, 8> signs{};
    for (int c = 0; c < 8; ++c)
      signs[static_cast<std::size_t>(c)] = ((config >> c) & 1) != 0;
    const auto crossing = crossingsFromCorners(signs);
    const auto parts    = partitionCubeEdges(signs, crossing);
    INFO("config = " << config);
    ++byCount[parts.numComponents];

    // 1. Crossing <=> labelled; labels dense in [0, numComponents).
    REQUIRE(parts.numComponents >= 0);
    REQUIRE(parts.numComponents <= 4);
    std::vector<int> size(static_cast<std::size_t>(parts.numComponents), 0);
    for (std::size_t e = 0; e < 12; ++e) {
      const int id = parts.componentOfEdge[e];
      REQUIRE((id >= 0) == crossing[e]);
      REQUIRE(id < parts.numComponents);
      if (id >= 0) ++size[static_cast<std::size_t>(id)];
    }
    for (int s : size) REQUIRE(s >= 3);  // a loop round a cube corner at least

    // 2. Every face pairs its crossings within one component.
    for (int f = 0; f < 6; ++f) {
      const auto& fc = dualc::tables::kFaceCorners[static_cast<std::size_t>(f)];
      std::vector<int> faceEdges;  // crossing edges, in cycle order
      for (int i = 0; i < 4; ++i) {
        const int e = edgeBetween(fc[static_cast<std::size_t>(i)],
                                  fc[static_cast<std::size_t>((i + 1) % 4)]);
        REQUIRE(e >= 0);
        if (crossing[static_cast<std::size_t>(e)]) faceEdges.push_back(e);
      }
      REQUIRE(faceEdges.size() % 2 == 0);
      auto id = [&](int e) {
        return parts.componentOfEdge[static_cast<std::size_t>(e)];
      };
      if (faceEdges.size() == 2) {
        CHECK(id(faceEdges[0]) == id(faceEdges[1]));
      } else if (faceEdges.size() == 4) {
        // Saddle: the two edges meeting at each inside corner pair up.
        for (int i = 0; i < 4; ++i) {
          const int corner = fc[static_cast<std::size_t>(i)];
          if (!signs[static_cast<std::size_t>(corner)]) continue;
          const int prev = fc[static_cast<std::size_t>((i + 3) % 4)];
          const int next = fc[static_cast<std::size_t>((i + 1) % 4)];
          CHECK(id(edgeBetween(prev, corner)) == id(edgeBetween(corner, next)));
        }
      }
    }

    // 3. The region-count oracle.
    const bool anyCrossing =
        std::find(crossing.begin(), crossing.end(), true) != crossing.end();
    const int expected =
        anyCrossing ? regionsOnSide(signs, true) + regionsOnSide(signs, false) - 1
                    : 0;
    CHECK(parts.numComponents == expected);
  }
  // The census, pinned as measured: 2 configurations with no surface (all
  // in, all out), 162 with one component, 82 with two, 8 with three (three
  // mutually non-adjacent inside corners; not their complements, which the
  // inside-pair rule reads as two) and 2 with four (the two
  // maximum independent corner sets, {0,3,5,6} and {1,2,4,7}).
  CHECK(byCount[0] == 2);
  CHECK(byCount[1] == 162);
  CHECK(byCount[2] == 82);
  CHECK(byCount[3] == 8);
  CHECK(byCount[4] == 2);
}
