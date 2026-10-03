#include "dualc/implicit.h"

#include "geometrycentral/surface/surface_mesh_factories.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace dualc;
using Catch::Approx;

namespace {

// Axis-aligned unit cube centred at the origin: [-0.5, 0.5]^3.
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

// A unit-radius sphere SDF that does NOT override edgeHit -- exercises the
// base ImplicitField::edgeHit root-finder directly.
class SphereSdf : public ImplicitField {
public:
  double valueAt(const Vector3& p) const override { return p.norm() - 1.0; }
  Vector3 gradientAt(const Vector3& p) const override {
    const double n = p.norm();
    return (n > 1e-12) ? (p / n) : Vector3{0.0, 0.0, 1.0};
  }
  BBox bounds() const override {
    return BBox{Vector3{-1.0, -1.0, -1.0}, Vector3{1.0, 1.0, 1.0}};
  }
};

} // namespace

TEST_CASE("ImplicitField base edgeHit finds a crossing on a radial segment",
          "[implicit]") {
  SphereSdf s;
  Vector3 p, n;
  // Straight through the centre: the crossing is the unit-radius point.
  REQUIRE(s.edgeHit(Vector3{2.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}, p, n));
  REQUIRE(p.x == Approx(1.0).margin(1e-6));
  REQUIRE(p.y == Approx(0.0).margin(1e-6));
  REQUIRE(p.z == Approx(0.0).margin(1e-6));
  REQUIRE(n.norm() == Approx(1.0).margin(1e-6));
}

TEST_CASE("ImplicitField base edgeHit converges on a curved chord",
          "[implicit]") {
  SphereSdf s;
  Vector3 p, n;
  // Off-centre segment at y = 0.5, from outside (x=2) to inside (x=0): it
  // crosses the sphere once, where x^2 = 0.75. The field is curved in the
  // segment parameter, so this exercises the Illinois false-position
  // refinement, not just the linear seed.
  REQUIRE(s.edgeHit(Vector3{2.0, 0.5, 0.0}, Vector3{0.0, 0.5, 0.0}, p, n));
  REQUIRE(s.valueAt(p) == Approx(0.0).margin(1e-4));
  REQUIRE(p.x == Approx(std::sqrt(0.75)).margin(1e-3));
  REQUIRE(n.norm() == Approx(1.0).margin(1e-6));
}

TEST_CASE("ImplicitField base edgeHit rejects a non-straddling segment",
          "[implicit]") {
  SphereSdf s;
  Vector3 p, n;
  // Both endpoints outside -> no bracketed root.
  REQUIRE_FALSE(s.edgeHit(Vector3{2.0, 0.0, 0.0}, Vector3{3.0, 0.0, 0.0},
                          p, n));
}

TEST_CASE("MeshSource::bounds is the unpadded mesh AABB", "[implicit]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource src(*mesh, *geom);

  const BBox b = src.bounds();
  REQUIRE(b.isValid());
  REQUIRE(b.min.x == Approx(-0.5));
  REQUIRE(b.min.y == Approx(-0.5));
  REQUIRE(b.min.z == Approx(-0.5));
  REQUIRE(b.max.x == Approx(0.5));
  REQUIRE(b.max.y == Approx(0.5));
  REQUIRE(b.max.z == Approx(0.5));
}

TEST_CASE("MeshSource::isInside classifies points", "[implicit]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource src(*mesh, *geom);

  REQUIRE(src.isInside(Vector3{0.0, 0.0, 0.0}));
  REQUIRE_FALSE(src.isInside(Vector3{2.0, 0.0, 0.0}));
  REQUIRE_FALSE(src.isInside(Vector3{0.0, 0.0, -3.0}));
}

TEST_CASE("MeshSource::valueAt is a signed distance", "[implicit]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource src(*mesh, *geom);

  // Centre: 0.5 to the nearest face, inside -> negative.
  REQUIRE(src.valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-0.5).margin(1e-5));

  // Outside, axis-aligned: distance to the x = 0.5 face.
  REQUIRE(src.valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(1.5).margin(1e-5));

  // Outside, beyond a corner: distance to (0.5, 0.5, 0.5).
  REQUIRE(src.valueAt(Vector3{2.0, 2.0, 2.0}) ==
          Approx(1.5 * std::sqrt(3.0)).margin(1e-5));
}

TEST_CASE("MeshSource::gradientAt points outward and is unit length",
          "[implicit]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource src(*mesh, *geom);

  const Vector3 g = src.gradientAt(Vector3{2.0, 0.0, 0.0});
  REQUIRE(g.norm() == Approx(1.0).margin(1e-6));
  REQUIRE(g.x == Approx(1.0).margin(1e-5));
  REQUIRE(g.y == Approx(0.0).margin(1e-5));
  REQUIRE(g.z == Approx(0.0).margin(1e-5));
}

TEST_CASE("MeshSource::edgeHit finds the surface crossing", "[implicit]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource src(*mesh, *geom);

  Vector3 p, n;
  const bool hit = src.edgeHit(Vector3{2.0, 0.0, 0.0},
                               Vector3{0.0, 0.0, 0.0}, p, n);
  REQUIRE(hit);
  REQUIRE(p.x == Approx(0.5).margin(1e-5));
  REQUIRE(p.y == Approx(0.0).margin(1e-5));
  REQUIRE(p.z == Approx(0.0).margin(1e-5));
}

TEST_CASE("PSEUDONORMAL sign matches WINDING_NUMBER on a watertight cube",
          "[implicit][sign]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource srcGwn(*mesh, *geom, /*interpolateNormals=*/true,
                    SignMethod::WINDING_NUMBER);
  MeshSource srcPn (*mesh, *geom, /*interpolateNormals=*/true,
                    SignMethod::PSEUDONORMAL);

  // Probe points well clear of the surface. Both methods are allowed to
  // disagree exactly on it; we don't probe within ~1e-3 of any face.
  const std::vector<Vector3> probes = {
      Vector3{0.0, 0.0, 0.0},      // centre, inside
      Vector3{0.3, 0.2, 0.1},      // off-centre, inside
      Vector3{-0.4, 0.4, -0.4},    // inside, near a corner
      Vector3{2.0, 0.0, 0.0},      // far outside +x face
      Vector3{0.6, 0.6, 0.6},      // outside, in +x+y+z corner region
      Vector3{-2.0, 1.5, 0.0},     // outside, in -x edge region
      Vector3{0.0, 0.0, -1.2},     // outside the -z face
  };
  for (const Vector3& p : probes) {
    REQUIRE(srcPn.isInside(p) == srcGwn.isInside(p));
  }
}

TEST_CASE("MeshSource::closestSurfacePoint returns face point + face normal",
          "[implicit][closest_surface]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource src(*mesh, *geom);

  Vector3 p, n;
  REQUIRE(src.closestSurfacePoint(Vector3{2.0, 0.0, 0.0}, p, n));
  REQUIRE(p.x == Approx(0.5).margin(1e-6));
  REQUIRE(p.y == Approx(0.0).margin(1e-6));
  REQUIRE(p.z == Approx(0.0).margin(1e-6));
  REQUIRE(n.x == Approx(1.0).margin(1e-6));
  REQUIRE(n.y == Approx(0.0).margin(1e-6));
  REQUIRE(n.z == Approx(0.0).margin(1e-6));
}

TEST_CASE("ImplicitField::closestSurfacePoint default Newton step on a sphere",
          "[implicit][closest_surface]") {
  SphereSdf s;
  Vector3 p, n;
  // From a point on the +x axis outside the unit sphere: one Newton step
  // lands exactly on the surface because f / |grad| = (r - 1) along a
  // radial line, and the step removes that offset.
  REQUIRE(s.closestSurfacePoint(Vector3{2.0, 0.0, 0.0}, p, n));
  REQUIRE(p.x == Approx(1.0).margin(1e-6));
  REQUIRE(p.y == Approx(0.0).margin(1e-6));
  REQUIRE(p.z == Approx(0.0).margin(1e-6));
  REQUIRE(n.x == Approx(1.0).margin(1e-6));
  REQUIRE(n.norm() == Approx(1.0).margin(1e-6));
}

TEST_CASE("GENERALIZED_WINDING_NUMBER sign matches WINDING_NUMBER on a "
          "watertight cube", "[implicit][sign]") {
  auto [mesh, geom] = makeUnitCube();
  MeshSource srcRay(*mesh, *geom, /*interpolateNormals=*/true,
                    SignMethod::WINDING_NUMBER);
  MeshSource srcGwn(*mesh, *geom, /*interpolateNormals=*/true,
                    SignMethod::GENERALIZED_WINDING_NUMBER);

  // Probe points well clear of the surface -- both methods may disagree
  // exactly on it. On a watertight cube they must classify identically.
  const std::vector<Vector3> probes = {
      Vector3{0.0, 0.0, 0.0},      // centre, inside
      Vector3{0.3, 0.2, 0.1},      // off-centre, inside
      Vector3{-0.4, 0.4, -0.4},    // inside, near a corner
      Vector3{2.0, 0.0, 0.0},      // far outside +x face
      Vector3{0.6, 0.6, 0.6},      // outside, in +x+y+z corner region
      Vector3{-2.0, 1.5, 0.0},     // outside, in -x edge region
      Vector3{0.0, 0.0, -1.2},     // outside the -z face
  };
  for (const Vector3& p : probes) {
    REQUIRE(srcGwn.isInside(p) == srcRay.isInside(p));
  }
}

TEST_CASE("WindingNumberField classifies inside / outside",
          "[implicit][gwn]") {
  auto [mesh, geom] = makeUnitCube();
  WindingNumberField wf(*mesh, *geom);

  REQUIRE(wf.isInside(Vector3{0.0, 0.0, 0.0}));
  REQUIRE(wf.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);

  REQUIRE_FALSE(wf.isInside(Vector3{2.0, 0.0, 0.0}));
  REQUIRE(wf.valueAt(Vector3{2.0, 0.0, 0.0}) > 0.0);
}

TEST_CASE("WindingNumberField edgeHit finds the cube face crossing",
          "[implicit][gwn]") {
  auto [mesh, geom] = makeUnitCube();
  WindingNumberField wf(*mesh, *geom);

  Vector3 p, n;
  // Outside (+x) to inside (centre): the GWN 0.5-isosurface crossing sits on
  // the +x face at x = 0.5.
  REQUIRE(wf.edgeHit(Vector3{2.0, 0.0, 0.0}, Vector3{0.0, 0.0, 0.0}, p, n));
  REQUIRE(p.x == Approx(0.5).margin(3e-2));
  REQUIRE(n.norm() == Approx(1.0).margin(1e-6));
  REQUIRE(n.x > 0.5);  // outward-pointing
}

TEST_CASE("WindingNumberField bounds pad the mesh AABB", "[implicit][gwn]") {
  auto [mesh, geom] = makeUnitCube();
  WindingNumberField wf(*mesh, *geom);

  const BBox b = wf.bounds();
  REQUIRE(b.isValid());
  // Padded outward from the [-0.5, 0.5]^3 cube AABB.
  REQUIRE(b.min.x < -0.5);
  REQUIRE(b.max.x >  0.5);
}
