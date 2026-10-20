// The gradient contract (roadmap 17 #34, record 17/19): gradientAt is a unit
// outward direction, trueGradientAt the gradient with its magnitude, and the
// two callers that need |grad f| -- normalizedOf and closestSurfacePoint's
// Newton step -- take it from trueGradientAt.

#include "example_common.h"

#include "dualc/implicit.h"
#include "dualc/implicit2d.h"
#include "dualc/primitives.h"

#include "geometrycentral/surface/surface_mesh_factories.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace dualc;
using Catch::Approx;

namespace {

using MeshPair =
    std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
               std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>;

MeshPair makeMesh(const std::vector<Vector3>& positions,
                  const std::vector<std::vector<std::size_t>>& polygons) {
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              positions);
}

// A closed axis-aligned unit cube centred at the origin, outward-wound.
MeshPair makeUnitCube() {
  return makeMesh(
      {Vector3{-0.5, -0.5, -0.5}, Vector3{0.5, -0.5, -0.5},
       Vector3{0.5, 0.5, -0.5},   Vector3{-0.5, 0.5, -0.5},
       Vector3{-0.5, -0.5, 0.5},  Vector3{0.5, -0.5, 0.5},
       Vector3{0.5, 0.5, 0.5},    Vector3{-0.5, 0.5, 0.5}},
      {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
       {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}});
}

// A tetrahedron of edge `s` with its slanted face {1,2,3} missing: the
// winding field seals the hole with a smooth cap, the one place w is smooth.
// w is scale-invariant, so |grad w| on the cap goes as 1/s: ~1.01 at s = 1
// (where the field happens to look metric), ~0.25 at s = 4, ~4 at s = 0.25.
MeshPair makeOpenTetrahedron(double s = 1.0) {
  return makeMesh({Vector3{0, 0, 0}, Vector3{s, 0, 0}, Vector3{0, s, 0},
                   Vector3{0, 0, s}},
                  {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}});
}

// Central difference of f's own valueAt, magnitude kept, at step e.
Vector3 fd(const ImplicitField& f, const Vector3& p, double e) {
  return Vector3{
      f.valueAt(Vector3{p.x + e, p.y, p.z}) -
          f.valueAt(Vector3{p.x - e, p.y, p.z}),
      f.valueAt(Vector3{p.x, p.y + e, p.z}) -
          f.valueAt(Vector3{p.x, p.y - e, p.z}),
      f.valueAt(Vector3{p.x, p.y, p.z + e}) -
          f.valueAt(Vector3{p.x, p.y, p.z - e})} /
         (2.0 * e);
}

// Sample points: an n^3 lattice over [lo, hi]^3, shifted off the obvious
// symmetry planes so no point sits exactly on a seam, centre or face.
std::vector<Vector3> lattice(double lo, double hi, int n) {
  std::vector<Vector3> pts;
  const double h = (hi - lo) / (n - 1);
  for (int k = 0; k < n; ++k)
    for (int j = 0; j < n; ++j)
      for (int i = 0; i < n; ++i)
        pts.push_back(Vector3{lo + h * i + 0.0123, lo + h * j + 0.0371,
                              lo + h * k + 0.0217});
  return pts;
}

// trueGradientAt agrees with the field's own central difference wherever
// valueAt is smooth. "Smooth here" is decided by the FD itself: two steps a
// decade apart must agree, else the stencil straddles a kink (a hard seam, a
// fold, an onion's zero) and the point is skipped. Returns the number of
// points that were checked.
int checkTrueGradientMatchesFd(const ImplicitField& f,
                               const std::vector<Vector3>& pts) {
  int checked = 0;
  for (const Vector3& p : pts) {
    const Vector3 a = fd(f, p, 1e-4);
    const Vector3 b = fd(f, p, 1e-5);
    const double scale = std::max(a.norm(), 1e-3);
    if ((a - b).norm() > 1e-4 * scale) continue;
    const Vector3 g = f.trueGradientAt(p);
    INFO("p = (" << p.x << ", " << p.y << ", " << p.z << ")");
    REQUIRE((g - a).norm() <= 1e-4 * scale);
    ++checked;
  }
  return checked;
}

// A field whose trueGradientAt is deliberately NOT its FD: proves a caller
// reads the virtual rather than differencing valueAt itself.
class RampWithDeclaredSlope : public ImplicitField {
public:
  double  valueAt(const Vector3& p) const override { return 2.0 * p.x; }
  Vector3 gradientAt(const Vector3&) const override { return {1.0, 0.0, 0.0}; }
  Vector3 trueGradientAt(const Vector3&) const override {
    return {8.0, 0.0, 0.0};
  }
  BBox bounds() const override { return BBox::infinite(); }
};

} // namespace

// ---------------------------------------------------------------------------
// The virtual itself
// ---------------------------------------------------------------------------

TEST_CASE("trueGradientAt's default is the central difference of valueAt",
          "[gradient]") {
  // A gyroid of wavelength 0.5: k = 2*pi/0.5, so |grad f| is ~12, far from
  // the 1 gradientAt reports.
  const GyroidField g(Vector3{0, 0, 0}, 0.5);
  const Vector3 p{0.07, 0.13, 0.19};
  const Vector3 t = g.trueGradientAt(p);
  const Vector3 ref = fd(g, p, 1e-4);
  REQUIRE(t.x == Approx(ref.x).epsilon(1e-12));
  REQUIRE(t.y == Approx(ref.y).epsilon(1e-12));
  REQUIRE(t.z == Approx(ref.z).epsilon(1e-12));
  REQUIRE(t.norm() > 5.0);
  REQUIRE(g.gradientAt(p).norm() == Approx(1.0).margin(1e-12));
  // Same direction.
  REQUIRE(dot(t / t.norm(), g.gradientAt(p)) == Approx(1.0).margin(1e-12));
}

TEST_CASE("Sphere and plane trueGradientAt are analytic", "[gradient]") {
  const SphereField s(Vector3{1, 2, 3}, 0.5);
  const Vector3 p{1.3, 1.6, 3.9};
  const Vector3 d = (p - Vector3{1, 2, 3}) / (p - Vector3{1, 2, 3}).norm();
  REQUIRE((s.trueGradientAt(p) - d).norm() < 1e-15);
  // Undefined at the centre: zero, no fallback direction.
  REQUIRE(s.trueGradientAt(Vector3{1, 2, 3}).norm() == 0.0);

  const PlaneField pl(Vector3{0, 3, 4}, -1.0);
  REQUIRE((pl.trueGradientAt(Vector3{5, -7, 2}) - Vector3{0, 0.6, 0.8})
              .norm() < 1e-15);
}

TEST_CASE("GridField trueGradientAt is the trilinear gradient, unnormalised",
          "[gradient][grid]") {
  // Bake a gyroid (|grad| ~ 2*pi) so the magnitude is visibly not 1.
  const GyroidField gy(Vector3{0, 0, 0}, 1.0);
  BBox region;
  region.min = Vector3{-1, -1, -1};
  region.max = Vector3{1, 1, 1};
  const FieldPtr grid = bakeToGrid(gy, region, 33, 1);  // cell = 1/16

  // A cell centre: the in-cell interpolant is a polynomial, so a tiny FD
  // that stays inside the cell is exact to rounding.
  const double h = 2.0 / 32.0;
  const Vector3 p{-1 + 13.5 * h, -1 + 20.5 * h, -1 + 7.5 * h};
  const Vector3 t = grid->trueGradientAt(p);
  const Vector3 ref = fd(*grid, p, 1e-6);
  REQUIRE((t - ref).norm() < 1e-6 * t.norm());
  REQUIRE(t.norm() > 2.0);
  // gradientAt is its direction.
  REQUIRE((grid->gradientAt(p) - t / t.norm()).norm() < 1e-15);
}

TEST_CASE("Pass-through nodes forward trueGradientAt with the chain rule",
          "[gradient]") {
  // Children with |grad| != 1 so a dropped factor shows: a gyroid
  // (|grad| ~ 2*pi/1.5) and a unit sphere.
  const FieldPtr gy = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 1.5);
  const FieldPtr sp = std::make_shared<SphereField>(Vector3{0.2, 0, 0}, 0.8);
  const FieldPtr tilted = transformed(
      gy, Mat4::translation(Vector3{0.1, -0.2, 0.3}) *
              Mat4::rotation(Vector3{1, 2, 3}, 0.7));

  const std::vector<std::pair<std::string, FieldPtr>> nodes = {
      {"offset", offsetOf(gy, 0.15)},
      {"onion", onionOf(gy, 0.1)},
      {"transform", tilted},
      {"scale", scaled(gy, 1.7)},
      {"scale<0", scaled(gy, -0.6)},
      {"mirror", mirrored(tilted, Vector3{1, 1, 0})},
      {"repeat", repeated(sp, Vector3{2.5, 0, 2.5})},
      {"repeat-overhang", repeated(sp, Vector3{1.4, 1.4, 0})},
      {"repeatLimited", repeatedLimited(sp, Vector3{1.5, 0, 0},
                                        Vector3i{3, 1, 1})},
      {"union", unionOf(gy, sp)},
      {"intersection", intersectionOf(gy, sp)},
      {"difference", differenceOf(sp, gy)},
      {"xor", xorOf(gy, sp)},
  };
  const auto pts = lattice(-1.4, 1.4, 7);
  for (const auto& [name, f] : nodes) {
    INFO("node: " << name);
    const int checked = checkTrueGradientMatchesFd(*f, pts);
    REQUIRE(checked > 150);  // of 343: the kink filter must not eat the test
  }
}

TEST_CASE("Every other node's trueGradientAt (the FD default) matches its FD",
          "[gradient]") {
  const FieldPtr gy = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 1.5);
  const FieldPtr sp = std::make_shared<SphereField>(Vector3{0.2, 0, 0}, 0.8);
  const FieldPtr ctl = std::make_shared<PlaneField>(Vector3{1, 0, 0}, 0.0);
  const std::vector<std::pair<std::string, FieldPtr>> nodes = {
      {"smoothUnion", smoothUnionOf(gy, sp, 0.3)},
      {"smoothIntersection", smoothIntersectionOf(gy, sp, 0.3)},
      {"smoothDifference", smoothDifferenceOf(sp, gy, 0.3)},
      {"mix", mixOf(gy, sp, ctl, -0.5, 0.5)},
      {"gradedOnion", gradedOnionOf(gy, ctl, 0.05, 0.2, -1, 1)},
      {"gradedOffset", gradedOffsetOf(sp, ctl, 0.0, 0.3, -1, 1)},
      {"elongate", elongated(sp, Vector3{0.3, 0, 0.2})},
      {"normalize", normalizedOf(gy)},
      {"twist", twisted(gy, 0.8, 1)},
      {"bend", bent(sp, 0.4, 0)},
      {"displace", displaced(sp, [](const Vector3& p) {
                     return 0.05 * std::sin(5 * p.x) * std::sin(5 * p.y);
                   })},
  };
  const auto pts = lattice(-1.4, 1.4, 7);
  for (const auto& [name, f] : nodes) {
    INFO("node: " << name);
    REQUIRE(checkTrueGradientMatchesFd(*f, pts) > 150);
  }
}

// ---------------------------------------------------------------------------
// NormalizedField
// ---------------------------------------------------------------------------

TEST_CASE("normalizedOf divides by the child's trueGradientAt", "[gradient]") {
  // valueAt = 2x, declared slope 8: the normalised value is 2x/8, not 2x/2
  // (its FD) and not 2x/1 (its gradientAt).
  const FieldPtr n = normalizedOf(std::make_shared<RampWithDeclaredSlope>());
  REQUIRE(n->valueAt(Vector3{0.4, 1, 1}) == Approx(0.1).epsilon(1e-15));

  // Through a scale: d/dp [s * child(p/s)] keeps the slope, so a scaled
  // gyroid normalises to the same metric value as the gyroid at p/s, times s.
  const FieldPtr gy = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 0.5);
  const Vector3 p{0.07, 0.13, 0.19};
  REQUIRE(normalizedOf(scaled(gy, 2.0))->valueAt(p * 2.0) ==
          Approx(2.0 * normalizedOf(gy)->valueAt(p)).epsilon(1e-9));
}

// ---------------------------------------------------------------------------
// closestSurfacePoint's Newton step
// ---------------------------------------------------------------------------

namespace {

// The step closestSurfacePoint took before the true-gradient path: by f
// along the unit direction gradientAt gives. Exact only where |grad f| = 1.
Vector3 oldStep(const ImplicitField& f, const Vector3& q) {
  const Vector3 g = f.gradientAt(q);
  return q - g * (f.valueAt(q) / g.norm());
}

} // namespace

TEST_CASE("closestSurfacePoint's Newton step beats the unit step on a gyroid",
          "[gradient][newton]") {
  // Wavelength 1: |grad f| ~ 2*pi on the surface, so the old unit step
  // overshoots by ~6x the offset.
  const GyroidField g(Vector3{0, 0, 0}, 1.0);
  int n = 0;
  for (const Vector3& q : lattice(-0.45, 0.45, 7)) {
    if (std::abs(g.valueAt(q)) > 0.3) continue;  // near the surface only
    Vector3 cp, cn;
    REQUIRE(g.closestSurfacePoint(q, cp, cn));
    const double fNew = std::abs(g.valueAt(cp));
    const double fOld = std::abs(g.valueAt(oldStep(g, q)));
    INFO("q = (" << q.x << ", " << q.y << ", " << q.z << ") |f(q)| = "
                 << std::abs(g.valueAt(q)));
    REQUIRE(fNew < fOld);
    REQUIRE(cn.norm() == Approx(1.0).margin(1e-12));
    ++n;
  }
  REQUIRE(n >= 20);
}

TEST_CASE("closestSurfacePoint's Newton step beats the unit step on a winding "
          "field's cap", "[gradient][newton][gwn]") {
  // Two sizes, one each side of |grad w| = 1: the unit step undershoots on
  // the big mesh and overshoots on the small one. Newton's residual is the
  // same at both (w is scale-invariant). At edge 1 the cap has |grad w| ~
  // 1.01, the two steps nearly coincide, and w's curvature lets the unit step
  // land closer at 10 of these 24 points -- not a case this test can claim.
  const double s = GENERATE(4.0, 0.25);
  auto [mesh, geom] = makeOpenTetrahedron(s);
  const WindingNumberField wf(*mesh, *geom);
  // The cap seals the missing face x + y + z = s; walk its normal through
  // the centroid and a few off-centre feet.
  const Vector3 nrm = Vector3{1, 1, 1} / std::sqrt(3.0);
  int n = 0;
  for (const Vector3& foot :
       {Vector3{1, 1, 1} / 3.0, Vector3{0.4, 0.3, 0.3},
        Vector3{0.25, 0.45, 0.3}, Vector3{0.3, 0.3, 0.4}}) {
    for (double t : {-0.12, -0.06, -0.03, 0.03, 0.06, 0.12}) {
      const Vector3 q = (foot + nrm * t) * s;
      Vector3 cp, cn;
      REQUIRE(wf.closestSurfacePoint(q, cp, cn));
      const double fNew = std::abs(wf.valueAt(cp));
      const double fOld = std::abs(wf.valueAt(oldStep(wf, q)));
      INFO("q = (" << q.x << ", " << q.y << ", " << q.z << ") |f(q)| = "
                   << std::abs(wf.valueAt(q)));
      REQUIRE(fNew < fOld);
      ++n;
    }
  }
  REQUIRE(n == 24);
}

// ---------------------------------------------------------------------------
// WindingNumberField: unit gradientAt, raw FD as trueGradientAt
// ---------------------------------------------------------------------------

TEST_CASE("WindingNumberField gradientAt is the unit direction of its "
          "trueGradientAt", "[gradient][gwn]") {
  auto [mesh, geom] = makeOpenTetrahedron(4.0);
  const WindingNumberField wf(*mesh, *geom);
  for (const Vector3& p : lattice(-1.2, 5.2, 6)) {
    const Vector3 t = wf.trueGradientAt(p);
    const Vector3 g = wf.gradientAt(p);
    REQUIRE(g.norm() == Approx(1.0).margin(1e-12));
    if (t.norm() > 1e-12) REQUIRE((g - t / t.norm()).norm() < 1e-12);
  }
  // Near the cap |grad w| is far from 1: the magnitude lives only in
  // trueGradientAt.
  const Vector3 c = Vector3{4, 4, 4} / 3.0;
  REQUIRE(wf.trueGradientAt(c).norm() < 0.5);
}

// ---------------------------------------------------------------------------
// The property: every concrete field type honours the contract
// ---------------------------------------------------------------------------

TEST_CASE("Every concrete field type's gradientAt is unit, or shorter in a "
          "blend", "[gradient][contract]") {
  struct Case {
    std::string name;
    FieldPtr    f;
    bool        blend;  // smooth booleans, mix, elongate: |g| <= 1
  };
  std::vector<Case> cases;

  // Every catalogue primitive (sphere and plane included), every TPMS, every
  // strut lattice -- the leaves the field graph can name.
  for (const auto& e : dce::primitiveCatalogue())
    cases.push_back({e.name, dce::buildPrimitive(e.name, {}), false});
  for (const auto& k : dce::tpmsKinds())
    cases.push_back({k.name, dce::makeTpmsField(k.name, Vector3{0, 0, 0}, 1.3),
                     false});
  for (const auto& k : dce::strutKinds())
    cases.push_back({"strut-" + k.name,
                     dce::makeStrutLattice(k.name, Vector3{0, 0, 0}, 1.2, 0.15),
                     false});

  // The mesh-backed and baked sources.
  auto [cubeMesh, cubeGeom] = makeUnitCube();
  auto [tetMesh, tetGeom] = makeOpenTetrahedron();
  const FieldPtr mesh = std::make_shared<MeshSource>(*cubeMesh, *cubeGeom);
  const FieldPtr wind = windingNumberField(*tetMesh, *tetGeom);
  cases.push_back({"MeshSource", mesh, false});
  cases.push_back({"WindingNumberField", wind, false});
  const FieldPtr gy = std::make_shared<GyroidField>(Vector3{0, 0, 0}, 1.5);
  BBox region;
  region.min = Vector3{-1.5, -1.5, -1.5};
  region.max = Vector3{1.5, 1.5, 1.5};
  cases.push_back({"GridField", bakeToGrid(*gy, region, 17, 1), false});

  // The 2D lifts.
  cases.push_back(
      {"revolve", std::make_shared<RevolveField>(
                      std::make_shared<Circle2D>(Vector2{0.8, 0}, 0.3), 0.0),
       false});
  cases.push_back(
      {"extrude", std::make_shared<ExtrudeField>(
                      std::make_shared<Box2D>(Vector2{0, 0}, Vector2{0.5, 0.3}),
                      0.6),
       false});

  // Every combinator, decorator and domain operator, over unit children --
  // and the winding field under the pass-through nodes, the one tree that was
  // off the contract before its gradientAt became unit.
  const FieldPtr sp = std::make_shared<SphereField>(Vector3{0.2, 0, 0}, 0.8);
  const FieldPtr ctl = std::make_shared<PlaneField>(Vector3{1, 0, 0}, 0.0);
  const Mat4 rigid = Mat4::translation(Vector3{0.1, -0.2, 0.3}) *
                     Mat4::rotation(Vector3{1, 2, 3}, 0.7);
  for (const auto& [childName, child] :
       std::vector<std::pair<std::string, FieldPtr>>{{"gyroid", gy},
                                                     {"winding", wind}}) {
    const std::vector<std::pair<std::string, FieldPtr>> passThrough = {
        {"union", unionOf(child, sp)},
        {"intersection", intersectionOf(child, sp)},
        {"difference", differenceOf(sp, child)},
        {"xor", xorOf(child, sp)},
        {"offset", offsetOf(child, 0.1)},
        {"rounded", roundedOf(child, 0.1)},
        {"onion", onionOf(child, 0.1)},
        {"gradedOnion", gradedOnionOf(child, ctl, 0.05, 0.2, -1, 1)},
        {"gradedOffset", gradedOffsetOf(child, ctl, 0.0, 0.2, -1, 1)},
        {"transform", transformed(child, rigid)},
        {"scale", scaled(child, 1.3)},
        {"scale<0", scaled(child, -0.7)},
        {"normalize", normalizedOf(child)},
        {"mirror", mirrored(child, Vector3{1, 1, 0})},
        {"repeat", repeated(child, Vector3{1.1, 0, 0})},
        {"repeatLimited",
         repeatedLimited(child, Vector3{1.1, 0, 0}, Vector3i{2, 1, 1})},
        {"twist", twisted(child, 0.8, 1)},
        {"bend", bent(child, 0.4, 0)},
        {"displace", displaced(child, [](const Vector3& p) {
                       return 0.05 * std::sin(5 * p.x);
                     })},
    };
    for (const auto& [name, f] : passThrough)
      cases.push_back({name + "(" + childName + ")", f, false});
    cases.push_back({"smoothUnion(" + childName + ")",
                     smoothUnionOf(child, sp, 0.3), true});
    cases.push_back({"smoothIntersection(" + childName + ")",
                     smoothIntersectionOf(child, sp, 0.3), true});
    cases.push_back({"smoothDifference(" + childName + ")",
                     smoothDifferenceOf(sp, child, 0.3), true});
    cases.push_back({"mix(" + childName + ")",
                     mixOf(child, sp, ctl, -0.5, 0.5), true});
    cases.push_back({"elongate(" + childName + ")",
                     elongated(child, Vector3{0.3, 0, 0.2}), true});
  }

  const auto pts = lattice(-1.3, 1.3, 7);
  for (const auto& c : cases) {
    REQUIRE(c.f != nullptr);
    INFO("field: " << c.name);
    for (const Vector3& p : pts) {
      const double len = c.f->gradientAt(p).norm();
      INFO("p = (" << p.x << ", " << p.y << ", " << p.z << ") |g| = " << len);
      REQUIRE(len <= 1.0 + 1e-9);
      if (!c.blend) REQUIRE(std::abs(len - 1.0) <= 1e-9);
    }
  }
}
