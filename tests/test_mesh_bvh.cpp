#include "internal/mesh_bvh.h"
#include "internal/sign_oracle.h"

#include "geometrycentral/surface/surface_mesh_factories.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <tuple>
#include <vector>

using namespace dualc;
using dualc::internal::MeshBVH;
using Catch::Approx;

namespace {

// Axis-aligned unit cube centred at the origin: [-0.5, 0.5]^3.
// Same layout as test_implicit.cpp::makeUnitCube (face polygons are CCW from
// outside so input normals are outward-pointing).
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeUnitCube() {
  std::vector<Vector3> positions = {
      Vector3{-0.5, -0.5, -0.5}, Vector3{0.5, -0.5, -0.5},
      Vector3{0.5, 0.5, -0.5},   Vector3{-0.5, 0.5, -0.5},
      Vector3{-0.5, -0.5, 0.5},  Vector3{0.5, -0.5, 0.5},
      Vector3{0.5, 0.5, 0.5},    Vector3{-0.5, 0.5, 0.5},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
      {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

// Unit cube with the +z (top) face removed -- an open shell. Triangle
// orientation of the remaining 5 faces stays outward.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeOpenCube() {
  std::vector<Vector3> positions = {
      Vector3{-0.5, -0.5, -0.5}, Vector3{0.5, -0.5, -0.5},
      Vector3{0.5, 0.5, -0.5},   Vector3{-0.5, 0.5, -0.5},
      Vector3{-0.5, -0.5, 0.5},  Vector3{0.5, -0.5, 0.5},
      Vector3{0.5, 0.5, 0.5},    Vector3{-0.5, 0.5, 0.5},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, /* {4,5,6,7} +z face dropped */ {0, 1, 5, 4},
      {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

// A single triangle (1,0,0),(0,1,0),(0,0,1). Seen from the origin it covers
// exactly one octant of the sphere: solid angle pi/2, winding number
// (pi/2) / (4 pi) = 0.125. Its normal (1,1,1) points away from the origin.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeOctantTriangle() {
  std::vector<Vector3> positions = {
      Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0},
  };
  std::vector<std::vector<std::size_t>> polygons = {{0, 1, 2}};
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

} // namespace

TEST_CASE("MeshBVH::closestPoint lands on the +x face for (0.7, 0, 0)",
          "[mesh_bvh][closest_point]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);

  Vector3 outP, outN;
  int     outTri;
  REQUIRE(bvh.closestPoint(Vector3{0.7, 0.0, 0.0}, outP, outN, outTri));
  REQUIRE(outP.x == Approx(0.5).margin(1e-6));
  REQUIRE(outP.y == Approx(0.0).margin(1e-6));
  REQUIRE(outP.z == Approx(0.0).margin(1e-6));
  // Face normal of one of the +x face triangles.
  REQUIRE(outN.x == Approx(1.0).margin(1e-6));
  REQUIRE(outN.y == Approx(0.0).margin(1e-6));
  REQUIRE(outN.z == Approx(0.0).margin(1e-6));
}

TEST_CASE("MeshBVH::closestPointWithPseudoNormal returns face normal in face region",
          "[mesh_bvh][pseudonormal]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);

  Vector3 outP, outN;
  int     outTri;
  REQUIRE(bvh.closestPointWithPseudoNormal(Vector3{0.7, 0.0, 0.0}, outP, outN,
                                           outTri));
  REQUIRE(outP.x == Approx(0.5).margin(1e-6));
  REQUIRE(outN.x == Approx(1.0).margin(1e-6));
  REQUIRE(outN.y == Approx(0.0).margin(1e-6));
  REQUIRE(outN.z == Approx(0.0).margin(1e-6));
}

TEST_CASE("MeshBVH::closestPointWithPseudoNormal averages two faces on an edge",
          "[mesh_bvh][pseudonormal]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);

  // (0.7, 0.7, 0) is in the Voronoi region of the cube edge running along
  // z at x = y = 0.5 -- shared by the +x face (normal (1,0,0)) and the +y
  // face (normal (0,1,0)). Expected pseudonormal: (1,1,0) / sqrt(2).
  Vector3 outP, outN;
  int     outTri;
  REQUIRE(bvh.closestPointWithPseudoNormal(Vector3{0.7, 0.7, 0.0}, outP, outN,
                                           outTri));
  REQUIRE(outP.x == Approx(0.5).margin(1e-6));
  REQUIRE(outP.y == Approx(0.5).margin(1e-6));
  REQUIRE(outP.z == Approx(0.0).margin(1e-6));
  const double inv = 1.0 / std::sqrt(2.0);
  REQUIRE(outN.x == Approx(inv).margin(1e-6));
  REQUIRE(outN.y == Approx(inv).margin(1e-6));
  REQUIRE(outN.z == Approx(0.0).margin(1e-6));
}

TEST_CASE("MeshBVH::closestPointWithPseudoNormal angle-weights at a cube corner",
          "[mesh_bvh][pseudonormal]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);

  // (0.7, 0.7, 0.7) is in the Voronoi region of cube corner (0.5, 0.5, 0.5),
  // shared by the +x, +y, +z faces. Each quad face contributes the full
  // 90deg corner angle at this vertex (the fan-triangulation splits the
  // square's corner into two adjacent sub-angles that sum to 90deg). So
  // the angle-weighted pseudonormal is (1,1,1) / sqrt(3).
  Vector3 outP, outN;
  int     outTri;
  REQUIRE(bvh.closestPointWithPseudoNormal(Vector3{0.7, 0.7, 0.7}, outP, outN,
                                           outTri));
  REQUIRE(outP.x == Approx(0.5).margin(1e-6));
  REQUIRE(outP.y == Approx(0.5).margin(1e-6));
  REQUIRE(outP.z == Approx(0.5).margin(1e-6));
  const double inv = 1.0 / std::sqrt(3.0);
  REQUIRE(outN.x == Approx(inv).margin(1e-6));
  REQUIRE(outN.y == Approx(inv).margin(1e-6));
  REQUIRE(outN.z == Approx(inv).margin(1e-6));
}

TEST_CASE("MeshBVH::closestPoint and pseudoNormal agree on outP / outTri",
          "[mesh_bvh][pseudonormal]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);

  // The two queries share the same BVH walk; the only difference is the
  // returned normal. Spot-check that outP / outTri match across a set of
  // probe points covering all 3 region types.
  for (const Vector3& q : {Vector3{0.7, 0.0, 0.0},     // face region
                           Vector3{0.7, 0.7, 0.0},     // edge region
                           Vector3{0.7, 0.7, 0.7},     // corner region
                           Vector3{-2.0, -0.1, 0.3},   // far-field face
                           Vector3{-0.4, 0.4, 0.4}}) { // inside, near corner
    Vector3 p1, n1, p2, n2;
    int     t1, t2;
    REQUIRE(bvh.closestPoint(q, p1, n1, t1));
    REQUIRE(bvh.closestPointWithPseudoNormal(q, p2, n2, t2));
    REQUIRE(t1 == t2);
    REQUIRE(p1.x == Approx(p2.x).margin(1e-12));
    REQUIRE(p1.y == Approx(p2.y).margin(1e-12));
    REQUIRE(p1.z == Approx(p2.z).margin(1e-12));
  }
}

TEST_CASE("MeshBVH::windingNumber is exact on known configurations",
          "[mesh_bvh][gwn]") {
  SECTION("single triangle covers one octant from the origin") {
    auto [mesh, geom] = makeOctantTriangle();
    MeshBVH bvh(*mesh, *geom);
    REQUIRE(bvh.windingNumber(Vector3{0.0, 0.0, 0.0}) ==
            Approx(0.125).margin(1e-9));
  }
  SECTION("watertight cube reads ~1 inside and ~0 outside") {
    auto [mesh, geom] = makeUnitCube();
    MeshBVH bvh(*mesh, *geom);
    REQUIRE(bvh.windingNumber(Vector3{0.0, 0.0, 0.0}) ==
            Approx(1.0).margin(1e-9));
    REQUIRE(bvh.windingNumber(Vector3{5.0, 3.0, 4.0}) ==
            Approx(0.0).margin(1e-9));
  }
}

TEST_CASE("MeshBVH::windingNumber reads inside on an open shell",
          "[mesh_bvh][gwn]") {
  auto [mesh, geom] = makeOpenCube();
  MeshBVH bvh(*mesh, *geom);
  // 5 of 6 faces present; each subtends 4*pi/6 at the centre, so w = 5/6.
  const double w = bvh.windingNumber(Vector3{0.0, 0.0, 0.0});
  REQUIRE(w == Approx(5.0 / 6.0).margin(1e-6));
  REQUIRE(w > 0.5);   // still classified inside despite the open top
  REQUIRE(w < 1.0);
}

TEST_CASE("MeshBVH::windingNumberFast matches exact windingNumber",
          "[mesh_bvh][gwn]") {
  auto [mesh, geom] = makeOpenCube();
  MeshBVH bvh(*mesh, *geom, /*interpolateNormals=*/true,
              /*buildWindingTree=*/true);

  // Grid offset off the +-0.5 cube planes so no probe lands on the surface,
  // where w is discontinuous. Near probes descend to exact triangle sums;
  // far probes exercise the multipole expansion.
  double maxErr = 0.0;
  for (double x = -1.85; x <= 1.9; x += 0.37)
    for (double y = -1.85; y <= 1.9; y += 0.37)
      for (double z = -1.85; z <= 1.9; z += 0.37) {
        const Vector3 q{x, y, z};
        const double exact = bvh.windingNumber(q);
        const double fast  = bvh.windingNumberFast(q);
        maxErr = std::max(maxErr, std::abs(fast - exact));
        // Sign relative to the 0.5 threshold must always agree -- this is
        // the contract the GWN sign oracle depends on.
        REQUIRE((fast > 0.5) == (exact > 0.5));
      }
  REQUIRE(maxErr < 1e-2);
}

TEST_CASE("MeshBVH::windingNumberFast is zero without the winding tree",
          "[mesh_bvh][gwn]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);  // buildWindingTree defaults to false
  REQUIRE(bvh.windingNumberFast(Vector3{0.0, 0.0, 0.0}) == Approx(0.0).margin(1e-12));
}

// ---------------------------------------------------------------------------
// countRayHits -- parity, grazing degeneracies, and the thin-feature/advance
// interaction (code-audit item #23).
// ---------------------------------------------------------------------------

namespace {

// Closed axis-aligned box [lo, hi], same face/winding layout as makeUnitCube
// (each quad CCW seen from outside, so triangle normals point outward).
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeBox(const Vector3& lo, const Vector3& hi) {
  std::vector<Vector3> positions = {
      Vector3{lo.x, lo.y, lo.z}, Vector3{hi.x, lo.y, lo.z},
      Vector3{hi.x, hi.y, lo.z}, Vector3{lo.x, hi.y, lo.z},
      Vector3{lo.x, lo.y, hi.z}, Vector3{hi.x, lo.y, hi.z},
      Vector3{hi.x, hi.y, hi.z}, Vector3{lo.x, hi.y, hi.z},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
      {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

} // namespace

TEST_CASE("countRayHits gives even parity outside and odd parity inside a cube",
          "[mesh_bvh][parity]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);

  // Directions with no axis-aligned component, so no probe lies in a face
  // plane; each must cross the closed cube exactly twice from outside.
  const std::vector<Vector3> dirs = {
      Vector3{1.0, 0.37, 0.21}, Vector3{-1.0, 0.53, -0.29},
      Vector3{0.17, 1.0, -0.43}, Vector3{-0.31, -0.19, 1.0},
  };
  for (const Vector3& d : dirs) {
    INFO("direction " << d.x << ", " << d.y << ", " << d.z);
    REQUIRE(bvh.countRayHits(Vector3{0.0, 0.0, 0.0}, d) == 1);   // inside
    REQUIRE(bvh.countRayHits(Vector3{0.0, 0.0, -3.0}, d) % 2 == 0);
    REQUIRE(bvh.countRayHits(Vector3{3.0, 3.0, 3.0}, d) % 2 == 0);
  }
}

TEST_CASE("countRayHits does not double-count an exact edge or vertex hit",
          "[mesh_bvh][parity][grazing]") {
  auto [mesh, geom] = makeUnitCube();
  MeshBVH bvh(*mesh, *geom);

  // Straight down the diagonal of the z = 0 plane: enters exactly on the
  // (x=0.5, y=0.5) edge and leaves exactly on the (x=-0.5, y=-0.5) edge.
  // Each of those edges is shared by two triangles, so a non-watertight
  // intersector -- or one whose duplicate suppression has been removed --
  // reports 4 rather than 2.
  REQUIRE(bvh.countRayHits(Vector3{2.0, 2.0, 0.0}, Vector3{-1.0, -1.0, 0.0}) == 2);

  // Body diagonal: enters exactly on the corner (0.5, 0.5, 0.5) and leaves
  // exactly on (-0.5, -0.5, -0.5). Each corner is shared by six triangles.
  REQUIRE(bvh.countRayHits(Vector3{2.0, 2.0, 2.0},
                           Vector3{-1.0, -1.0, -1.0}) == 2);

  // The same two rays must still classify the origin as inside / the far
  // point as outside once the 3-ray vote runs over them.
  internal::SignOracle oracle(bvh, SignMethod::WINDING_NUMBER);
  REQUIRE(oracle.isInside(Vector3{0.0, 0.0, 0.0}));
  REQUIRE_FALSE(oracle.isInside(Vector3{2.0, 2.0, 2.0}));
}

// Audit item #23. countRayHits advances past each hit by
// max(tHit * 1e-5, 1e-6), an epsilon relative to *hit distance* rather than to
// feature size. For a slab of thickness w whose near face sits at axial
// distance X from the origin, the crossing separation in ray-t is w/dx and the
// advance is (X/dx)*1e-5 -- the 1/dx cancels, so the far face is swallowed
// exactly when w < X*1e-5, for every direction at once. Float ULP is ~1.2e-7
// relative, so the band of features that are geometrically resolvable but
// silently dropped spans roughly 84x.
//
// 6 um wall at x = 2000 puts w/X at 3e-6: 3.3x inside the epsilon, and still
// ~34 ULP of separation at the hit distance, so this measures the epsilon and
// not float noise.
TEST_CASE("countRayHits sees both faces of a thin wall far from the origin",
          "[mesh_bvh][parity][audit23]") {
  const double X = 2000.0;
  const double w = 0.006;
  auto [mesh, geom] = makeBox(Vector3{X, -4000.0, -4000.0},
                              Vector3{X + w, 4000.0, 4000.0});
  MeshBVH bvh(*mesh, *geom);

  const double tFront  = X;                 // dx = 1 for the +x probe
  const double advance = tFront * 1e-5;
  INFO("front face at t = " << tFront << ", back face at t = " << (tFront + w)
       << "; HEAD advances min_t by " << advance << " (" << (advance / w)
       << "x the wall thickness), so the back face is skipped");
  REQUIRE(bvh.countRayHits(Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0}) == 2);
}

TEST_CASE("WINDING_NUMBER oracle keeps a point outside a distant thin wall",
          "[mesh_bvh][parity][sign_oracle][audit23]") {
  // The three probe directions of SignOracle all have a positive x component
  // (dx ~ 0.731 / 0.422 / 0.535), so all three cross the slab -- at t ~ 2734,
  // 4736 and 3735, with y and z well inside the +-4000 extent. They share the
  // origin and the same relative epsilon, so the 3-ray majority vote does not
  // rescue the parity: all three lose the back face together.
  const double X = 2000.0;
  const double w = 0.006;
  auto [mesh, geom] = makeBox(Vector3{X, -4000.0, -4000.0},
                              Vector3{X + w, 4000.0, 4000.0});
  MeshBVH bvh(*mesh, *geom, /*interpolateNormals=*/true,
              /*buildWindingTree=*/true);

  const Vector3 q{0.0, 0.0, 0.0};
  // Independent oracle: the generalized winding number does not go through
  // countRayHits at all, and says the origin is outside.
  REQUIRE(bvh.windingNumber(q) < 0.5);
  REQUIRE_FALSE(internal::SignOracle(bvh, SignMethod::GENERALIZED_WINDING_NUMBER)
                    .isInside(q));

  INFO("origin is 2 m away from a 6 um plate");
  REQUIRE_FALSE(internal::SignOracle(bvh, SignMethod::WINDING_NUMBER).isInside(q));
}
