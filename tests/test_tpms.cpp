#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace dualc;
using Catch::Approx;

namespace {

// Helper: shorthand for the per-TPMS periodicity check at one point.
//
// Every TPMS in our library is periodic in each axis with period `wavelength`
// (the 2x-frequency terms in Fischer-Koch / Lidinoid are sub-periods, but the
// full surface is still periodic at +wavelength). Step by wavelength along
// each axis and require values match to FP tolerance.
template <typename Field>
void checkPeriodic(const Field& f, double wavelength) {
  const Vector3 p{0.13, 0.27, 0.41};
  const double v0 = f.valueAt(p);
  for (int axis = 0; axis < 3; ++axis) {
    Vector3 q = p;
    if (axis == 0) q.x += wavelength;
    if (axis == 1) q.y += wavelength;
    if (axis == 2) q.z += wavelength;
    REQUIRE(f.valueAt(q) == Approx(v0).margin(1e-10));
  }
}

} // namespace

TEST_CASE("GyroidField value, bounds, periodicity", "[tpms]") {
  GyroidField g(Vector3{0, 0, 0}, 1.0);

  // Origin lies on the gyroid 0-isosurface: all sin(0) terms vanish.
  REQUIRE(g.valueAt(Vector3{0, 0, 0}) == Approx(0.0).margin(1e-12));

  // Unbounded.
  REQUIRE(g.bounds().isInfinite());

  checkPeriodic(g, 1.0);
}

TEST_CASE("SchwarzPField value, bounds, periodicity", "[tpms]") {
  SchwarzPField p(Vector3{0, 0, 0}, 1.0);

  // Origin is the global maximum of cos(kx)+cos(ky)+cos(kz): F(0)=3.
  REQUIRE(p.valueAt(Vector3{0, 0, 0}) == Approx(3.0));
  // Quarter-period along each axis: every cosine becomes zero -> F=0.
  REQUIRE(p.valueAt(Vector3{0.25, 0.25, 0.25}) == Approx(0.0).margin(1e-12));

  REQUIRE(p.bounds().isInfinite());
  checkPeriodic(p, 1.0);
}

TEST_CASE("DiamondField value, bounds, periodicity", "[tpms]") {
  DiamondField d(Vector3{0, 0, 0}, 1.0);

  // Every term in Schwarz D has at least one sin(0) -> F(0)=0.
  REQUIRE(d.valueAt(Vector3{0, 0, 0}) == Approx(0.0).margin(1e-12));

  REQUIRE(d.bounds().isInfinite());
  checkPeriodic(d, 1.0);
}

TEST_CASE("FischerKochSField value, bounds, periodicity", "[tpms]") {
  FischerKochSField f(Vector3{0, 0, 0}, 1.0);

  // Each Fischer-Koch S term contains a sin(0) factor at the origin -> F(0)=0.
  REQUIRE(f.valueAt(Vector3{0, 0, 0}) == Approx(0.0).margin(1e-12));

  REQUIRE(f.bounds().isInfinite());
  checkPeriodic(f, 1.0);
}

TEST_CASE("LidinoidField value, bounds, periodicity", "[tpms]") {
  LidinoidField l(Vector3{0, 0, 0}, 1.0);

  // At the origin: the sin(2k.)cos(k.)sin(k.) terms vanish (sin(0)=0); the
  // cos(2k.)cos(2k.) terms are all 1, giving t2=1.5; the constant 0.15
  // shifts -> F(0) = 0 - 1.5 + 0.15 = -1.35.
  REQUIRE(l.valueAt(Vector3{0, 0, 0}) == Approx(-1.35));

  REQUIRE(l.bounds().isInfinite());
  checkPeriodic(l, 1.0);
}

TEST_CASE("NeoviusField value, bounds, periodicity", "[tpms]") {
  NeoviusField n(Vector3{0, 0, 0}, 1.0);

  // F(0) = 3*(1+1+1) + 4*1*1*1 = 13.
  REQUIRE(n.valueAt(Vector3{0, 0, 0}) == Approx(13.0));

  REQUIRE(n.bounds().isInfinite());
  checkPeriodic(n, 1.0);
}

TEST_CASE("Wavelength scales the period", "[tpms]") {
  // Same primitive at two different wavelengths. At a generic off-axis
  // point, halving the wavelength MUST change the value (the trig argument
  // doubles), while stepping by the period must recover the original value.
  GyroidField a(Vector3{0, 0, 0}, 1.0);
  GyroidField b(Vector3{0, 0, 0}, 2.0);
  const Vector3 p{0.2, 0.3, 0.1};

  // Periodicity in each direction at its own wavelength.
  REQUIRE(a.valueAt(p + Vector3{1.0, 0, 0}) ==
          Approx(a.valueAt(p)).margin(1e-10));
  REQUIRE(b.valueAt(p + Vector3{2.0, 0, 0}) ==
          Approx(b.valueAt(p)).margin(1e-10));

  // Different wavelengths produce different fields at the same generic point.
  REQUIRE(a.valueAt(p) != Approx(b.valueAt(p)).margin(1e-3));
}

namespace {

// Build a unit cube [-0.5, 0.5]^3 as a triangulated SurfaceMesh + geometry.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeUnitCube() {
  std::vector<Vector3> positions = {
      Vector3{-0.5, -0.5, -0.5}, Vector3{ 0.5, -0.5, -0.5},
      Vector3{ 0.5,  0.5, -0.5}, Vector3{-0.5,  0.5, -0.5},
      Vector3{-0.5, -0.5,  0.5}, Vector3{ 0.5, -0.5,  0.5},
      Vector3{ 0.5,  0.5,  0.5}, Vector3{-0.5,  0.5,  0.5},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {4, 5, 6, 7},   // -z, +z
      {0, 1, 5, 4}, {3, 7, 6, 2},   // -y, +y
      {0, 4, 7, 3}, {1, 2, 6, 5},   // -x, +x
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              positions);
}

} // namespace

TEST_CASE("End-to-end: gyroid + onion clipped to a unit cube is manifold",
          "[tpms][pipeline]") {
  auto [mesh, geom] = makeUnitCube();
  auto src = std::make_shared<MeshSource>(*mesh, *geom);

  // Three cells across the cube. Onion thickness 0.08 is well above the
  // depth-6 cell size (~0.015), so the thick-wall shell resolves cleanly.
  auto tpms = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 0.35);
  FieldPtr lattice = onionOf(tpms, 0.08);
  FieldPtr clipped = intersectionOf(lattice, src);

  SamplerParams sp;
  sp.maxDepth = 6;
  BBox box;
  box.min = Vector3{-0.6, -0.6, -0.6};
  box.max = Vector3{ 0.6,  0.6,  0.6};
  sp.rootBounds = box;
  ContourerParams cp;

  auto [outMesh, outGeom, outNormals] = dualContourField(*clipped, sp, cp);
  REQUIRE(outMesh != nullptr);
  REQUIRE(outMesh->nVertices() > 0);

  // Manifold dual contouring + closed-volume composition (a thick-walled
  // gyroid shell clipped to a closed cube) must produce a watertight
  // output: no boundary edges anywhere.
  int boundaryEdges = 0;
  for (auto e : outMesh->edges()) {
    if (e.isBoundary()) ++boundaryEdges;
  }
  REQUIRE(boundaryEdges == 0);
}

TEST_CASE("normalizedOf preserves the 0-isosurface", "[tpms][normalize]") {
  auto raw = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 1.0);
  FieldPtr norm = normalizedOf(raw);

  // The origin is on the gyroid surface; normalising should leave the value
  // at zero (0 / |grad| == 0).
  REQUIRE(norm->valueAt(Vector3{0, 0, 0}) == Approx(0.0).margin(1e-10));

  // At a generic point the normalised value should have the same sign as the
  // raw one (we just divided by a positive quantity).
  const Vector3 p{0.1, 0.2, 0.3};
  const double v_raw = raw->valueAt(p);
  const double v_norm = norm->valueAt(p);
  REQUIRE((v_raw > 0) == (v_norm > 0));
}

TEST_CASE("normalizedOf divides by the true gradient magnitude, not a unit one",
          "[tpms][normalize]") {
  // Regression: PrimitiveField::gradientAt returns a *unit* direction, so a
  // normalisation that divided by child->gradientAt().norm() was a no-op.
  // At a high-frequency wavelength |grad f| >> 1, so the normalised value must
  // be markedly smaller in magnitude than the raw one and equal raw / |grad f|.
  auto raw = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 0.5);
  FieldPtr norm = normalizedOf(raw);

  const Vector3 p{0.07, 0.13, 0.19};  // generic off-surface point
  constexpr double e = 1e-4;
  auto v = [&](double x, double y, double z) {
    return raw->valueAt(Vector3{x, y, z});
  };
  const double dx = v(p.x + e, p.y, p.z) - v(p.x - e, p.y, p.z);
  const double dy = v(p.x, p.y + e, p.z) - v(p.x, p.y - e, p.z);
  const double dz = v(p.x, p.y, p.z + e) - v(p.x, p.y, p.z - e);
  const double gmag = std::sqrt(dx * dx + dy * dy + dz * dz) / (2.0 * e);

  REQUIRE(gmag > 5.0);  // k = 2*pi/0.5 ~ 12.6: the surface is steep
  REQUIRE(norm->valueAt(p) == Approx(raw->valueAt(p) / gmag).epsilon(1e-6));
  // The no-op bug returned raw/1.0; guard that the magnitude actually shrinks.
  REQUIRE(std::abs(norm->valueAt(p)) < 0.5 * std::abs(raw->valueAt(p)));
}

TEST_CASE("normalizedOf rescues a sub-cell thin-wall offset to a watertight shell",
          "[tpms][normalize][pipeline]") {
  // A raw offset of 0.05 at wavelength 0.5 is only ~0.008 thick metrically --
  // below the depth-6 cell (~0.016), so the raw shell fragments (open edges).
  // normalizedOf rescales the band to a metric ~0.1 wall that resolves cleanly
  // into a closed manifold. Pre-fix this test fails (boundary edges present).
  auto [mesh, geom] = makeUnitCube();
  auto src = std::make_shared<MeshSource>(*mesh, *geom);

  auto tpms = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 0.5);
  FieldPtr lattice = onionOf(normalizedOf(tpms), 0.05);
  FieldPtr clipped = intersectionOf(lattice, src);

  SamplerParams sp;
  sp.maxDepth = 6;
  BBox box;
  box.min = Vector3{-0.6, -0.6, -0.6};
  box.max = Vector3{ 0.6,  0.6,  0.6};
  sp.rootBounds = box;
  ContourerParams cp;

  auto [outMesh, outGeom, outNormals] = dualContourField(*clipped, sp, cp);
  REQUIRE(outMesh != nullptr);
  REQUIRE(outMesh->nVertices() > 0);

  int boundaryEdges = 0;
  for (auto e : outMesh->edges()) {
    if (e.isBoundary()) ++boundaryEdges;
  }
  REQUIRE(boundaryEdges == 0);
}
