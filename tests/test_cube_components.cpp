#include "internal/cube_components.h"
#include "internal/dc_tables.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>

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
