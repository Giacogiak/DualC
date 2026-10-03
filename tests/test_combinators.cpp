#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace dualc;
using Catch::Approx;

namespace {

// Axis-aligned cube of side 2*half centred at `c`.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeBox(const Vector3& c, double half) {
  const double h = half;
  std::vector<Vector3> positions = {
      c + Vector3{-h, -h, -h}, c + Vector3{h, -h, -h},
      c + Vector3{h, h, -h},   c + Vector3{-h, h, -h},
      c + Vector3{-h, -h, h},  c + Vector3{h, -h, h},
      c + Vector3{h, h, h},    c + Vector3{-h, h, h},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
      {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

double smoothMinRef(double a, double b, double k) {
  double h = std::clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
  return (b * (1.0 - h) + a * h) - k * h * (1.0 - h);
}

} // namespace

TEST_CASE("Hard union is min and OR of operands", "[combinators]") {
  // A = [-0.5,0.5]^3, B = [0,1]^3 (overlap along x in [0,0.5]).
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto [mB, gB] = makeBox(Vector3{0.5, 0.5, 0.5}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto b = std::make_shared<MeshSource>(*mB, *gB);
  auto u = unionOf(a, b);

  const Vector3 p{-0.3, 0.0, 0.0};  // inside A only
  REQUIRE(u->valueAt(p) ==
          Approx(std::min(a->valueAt(p), b->valueAt(p))).margin(1e-9));
  REQUIRE(u->isInside(p));
  REQUIRE(u->isInside(Vector3{0.9, 0.5, 0.5}));   // inside B only
  REQUIRE_FALSE(u->isInside(Vector3{3.0, 0.0, 0.0}));
}

TEST_CASE("Hard intersection is AND of operands", "[combinators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto [mB, gB] = makeBox(Vector3{0.5, 0.5, 0.5}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto b = std::make_shared<MeshSource>(*mB, *gB);
  auto x = intersectionOf(a, b);

  REQUIRE(x->isInside(Vector3{0.25, 0.25, 0.25}));   // in both
  REQUIRE_FALSE(x->isInside(Vector3{-0.3, 0.0, 0.0})); // in A only
  REQUIRE_FALSE(x->isInside(Vector3{0.9, 0.9, 0.9}));  // in B only
}

TEST_CASE("Hard difference removes the second operand", "[combinators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto [mB, gB] = makeBox(Vector3{0.5, 0.5, 0.5}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto b = std::make_shared<MeshSource>(*mB, *gB);
  auto d = differenceOf(a, b);

  REQUIRE(d->isInside(Vector3{-0.3, 0.0, 0.0}));      // in A, not B
  REQUIRE_FALSE(d->isInside(Vector3{0.25, 0.25, 0.25})); // in both -> carved
  const Vector3 p{-0.3, 0.0, 0.0};
  REQUIRE(d->valueAt(p) ==
          Approx(std::max(a->valueAt(p), -b->valueAt(p))).margin(1e-9));
}

TEST_CASE("A \\ A is empty at the field level", "[combinators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto d = differenceOf(a, a);

  // max(v, -v) = |v| >= 0 everywhere: nothing is inside.
  REQUIRE_FALSE(d->isInside(Vector3{0.0, 0.0, 0.0}));
  REQUIRE_FALSE(d->isInside(Vector3{0.2, -0.1, 0.3}));
  REQUIRE_FALSE(d->isInside(Vector3{2.0, 2.0, 2.0}));
}

TEST_CASE("Hard union gradient is the active operand's (sharp seam)",
          "[combinators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto [mB, gB] = makeBox(Vector3{0.5, 0.5, 0.5}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto b = std::make_shared<MeshSource>(*mB, *gB);
  auto u = unionOf(a, b);

  // At (-0.3,0,0) A has the smaller value, so the union gradient must be A's
  // gradient verbatim -- no blending across operands.
  const Vector3 p{-0.3, 0.0, 0.0};
  const Vector3 gu = u->gradientAt(p);
  const Vector3 ga = a->gradientAt(p);
  REQUIRE(gu.x == Approx(ga.x));
  REQUIRE(gu.y == Approx(ga.y));
  REQUIRE(gu.z == Approx(ga.z));
}

TEST_CASE("Smooth union matches the Quilez polynomial and never exceeds min",
          "[combinators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto [mB, gB] = makeBox(Vector3{0.5, 0.5, 0.5}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto b = std::make_shared<MeshSource>(*mB, *gB);
  const double k = 2.0;
  auto su = smoothUnionOf(a, b, k);

  const Vector3 p{0.0, 0.0, 0.0};
  const double da = a->valueAt(p);
  const double db = b->valueAt(p);
  REQUIRE(su->valueAt(p) == Approx(smoothMinRef(da, db, k)).margin(1e-9));
  // The blend fillet only adds material: smin <= min.
  REQUIRE(su->valueAt(p) <= std::min(da, db) + 1e-9);
}

TEST_CASE("offsetOf shifts the field by a constant", "[decorators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto o = offsetOf(a, 0.2);

  for (const Vector3 p : {Vector3{0, 0, 0}, Vector3{0.7, 0, 0},
                          Vector3{0.3, 0.1, -0.2}}) {
    REQUIRE(o->valueAt(p) == Approx(a->valueAt(p) - 0.2).margin(1e-9));
  }
}

TEST_CASE("onionOf is abs(field) minus thickness", "[decorators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto shell = onionOf(a, 0.1);

  for (const Vector3 p : {Vector3{0, 0, 0}, Vector3{0.45, 0, 0},
                          Vector3{0.7, 0, 0}}) {
    REQUIRE(shell->valueAt(p) ==
            Approx(std::abs(a->valueAt(p)) - 0.1).margin(1e-9));
  }
}

TEST_CASE("gradedOffsetOf inflates a solid by a control-driven ramp",
          "[decorators]") {
  // base = unit sphere at the origin; control = distance from the origin
  // (sphere of radius 0). t ramps t1 -> t2 as control goes d0 -> d1.
  auto base = std::make_shared<SphereField>(Vector3{0, 0, 0}, 1.0);
  auto control = std::make_shared<SphereField>(Vector3{0, 0, 0}, 0.0);
  const double t1 = 0.05, t2 = 0.25, d0 = 0.5, d1 = 1.5;
  auto g = gradedOffsetOf(base, control, t1, t2, d0, d1);

  auto rampT = [&](const Vector3& p) {
    double u = (control->valueAt(p) - d0) / (d1 - d0);
    u = std::min(1.0, std::max(0.0, u));
    return t1 + (t2 - t1) * u;
  };

  // value == base - t(control) at points spanning the ramp (inside plateau,
  // on the ramp, and past d1's plateau).
  for (const Vector3 p : {Vector3{0.2, 0, 0}, Vector3{0.8, 0, 0},
                          Vector3{1.2, 0, 0}, Vector3{0.3, 0.4, -0.5}}) {
    REQUIRE(g->valueAt(p) ==
            Approx(base->valueAt(p) - rampT(p)).margin(1e-9));
  }

  // Uniform ramp (t1 == t2 == T) collapses to a plain outward offset by +T:
  // gradedOffset = base - T, and offsetOf(base, r) = base - r, so r = T.
  const double T = 0.15;
  auto flat = gradedOffsetOf(base, control, T, T, d0, d1);
  auto plain = offsetOf(base, T);
  for (const Vector3 p : {Vector3{0.2, 0, 0}, Vector3{1.2, 0, 0},
                          Vector3{0.3, 0.4, -0.5}}) {
    REQUIRE(flat->valueAt(p) == Approx(plain->valueAt(p)).margin(1e-9));
  }

  // Outward growth widens the bounds by max(t1,t2).
  const BBox b = g->bounds();
  REQUIRE(b.max.x == Approx(1.0 + t2));
  REQUIRE(b.min.x == Approx(-(1.0 + t2)));
}

TEST_CASE("mixOf morphs A into B along a control ramp", "[combinators]") {
  // A/B = two offset spheres; control = distance from the origin (sphere r=0).
  // value = lerp(A, B, w), w = clamp((control - lo)/(hi - lo), 0, 1).
  auto a = std::make_shared<SphereField>(Vector3{-0.6, 0, 0}, 0.8);
  auto b = std::make_shared<SphereField>(Vector3{0.6, 0, 0}, 0.8);
  auto control = std::make_shared<SphereField>(Vector3{0, 0, 0}, 0.0);
  const double lo = 0.3, hi = 1.2;
  auto m = mixOf(a, b, control, lo, hi);

  auto rampW = [&](const Vector3& p) {
    double u = (control->valueAt(p) - lo) / (hi - lo);
    return std::min(1.0, std::max(0.0, u));
  };

  // Below lo (control <= lo) -> pure A; above hi -> pure B; on the ramp -> lerp.
  const Vector3 belowLo{0.1, 0, 0};   // control = 0.1 <= lo
  const Vector3 onRamp{0.75, 0, 0};   // control = 0.75, u = 0.5
  const Vector3 aboveHi{3.0, 0, 0};   // control = 3.0 >= hi
  REQUIRE(m->valueAt(belowLo) == Approx(a->valueAt(belowLo)).margin(1e-9));
  REQUIRE(m->valueAt(aboveHi) == Approx(b->valueAt(aboveHi)).margin(1e-9));
  for (const Vector3 p : {onRamp, Vector3{0.5, 0.4, -0.2}}) {
    const double w = rampW(p);
    const double expected = a->valueAt(p) * (1.0 - w) + b->valueAt(p) * w;
    REQUIRE(m->valueAt(p) == Approx(expected).margin(1e-9));
  }

  // bounds() is the union of BOTH operands' bounds (the surface can be either).
  const BBox bb = m->bounds();
  REQUIRE(bb.min.x == Approx(-1.4));  // A: -0.6 - 0.8
  REQUIRE(bb.max.x == Approx(1.4));   // B:  0.6 + 0.8

  // Degenerate band (lo == hi): the ramp collapses to a hard step at that
  // control value -- A on one side, B on the other, no divide-by-zero.
  auto step = mixOf(a, b, control, 0.5, 0.5);
  const Vector3 inside{0.2, 0, 0};    // control = 0.2 < 0.5 -> A
  const Vector3 outside{1.0, 0, 0};   // control = 1.0 > 0.5 -> B
  REQUIRE(step->valueAt(inside) == Approx(a->valueAt(inside)).margin(1e-9));
  REQUIRE(step->valueAt(outside) == Approx(b->valueAt(outside)).margin(1e-9));
}

TEST_CASE("mixOf refines cells holding a stub cap on neither operand surface",
          "[combinators]") {
  // The morph surface floats free of BOTH operand zero-sets: lerp(A,B,w)=0 where
  // A/B = -w/(1-w). If cellOverlaps only forwarded `a || b` (the union pattern),
  // such a cell -- deep inside A, in B's empty space -- would be pruned and the
  // cap silently dropped. This pins the conservative refine that prevents it.
  auto big = std::make_shared<SphereField>(Vector3{0, 0, 0}, 2.0);   // A
  auto small = std::make_shared<SphereField>(Vector3{0, 0, 0}, 0.5);  // B
  auto control = std::make_shared<SphereField>(Vector3{0, 0, 0}, 0.0);  // = |p|
  // At p=(1,0,0): control=1, w=1/1.5=2/3; A=-1 (deep inside), B=+0.5 (outside).
  // blend = -1*(1-2/3) + 0.5*(2/3) = 0 -> the surface passes through here.
  auto m = mixOf(big, small, control, 0.0, 1.5);
  const BBox cell{{0.9, -0.1, -0.1}, {1.1, 0.1, 0.1}};

  // Neither operand surface is in the cell (A=0 at |p|=2, B=0 at |p|=0.5)...
  REQUIRE_FALSE(big->cellOverlaps(cell));
  REQUIRE_FALSE(small->cellOverlaps(cell));
  // ...yet the mixed surface is, so the mix node MUST refine it.
  REQUIRE(m->cellOverlaps(cell));
  // Sanity: the blend really does straddle zero across the cell.
  REQUIRE(m->valueAt(Vector3{0.95, 0, 0}) < 0.0);
  REQUIRE(m->valueAt(Vector3{1.05, 0, 0}) > 0.0);
}

TEST_CASE("transformed places the field rigidly", "[decorators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto moved = transformed(a, Mat4::translation(Vector3{5.0, 0.0, 0.0}));

  REQUIRE(moved->isInside(Vector3{5.0, 0.0, 0.0}));
  REQUIRE_FALSE(moved->isInside(Vector3{0.0, 0.0, 0.0}));

  const BBox b = moved->bounds();
  REQUIRE(b.isValid());
  REQUIRE(b.min.x == Approx(4.5));
  REQUIRE(b.max.x == Approx(5.5));
}

TEST_CASE("scaled scales both domain and field value", "[decorators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);  // [-0.5,0.5]^3
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto big = scaled(a, 2.0);                        // -> [-1,1]^3

  REQUIRE(big->isInside(Vector3{0.9, 0.0, 0.0}));
  REQUIRE_FALSE(big->isInside(Vector3{1.5, 0.0, 0.0}));
  // value scales by s: 2 * A.valueAt(origin) = 2 * (-0.5).
  REQUIRE(big->valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-1.0).margin(1e-5));

  const BBox b = big->bounds();
  REQUIRE(b.min.x == Approx(-1.0));
  REQUIRE(b.max.x == Approx(1.0));
}

TEST_CASE("elongated stretches the field along an axis", "[decorators]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto stretched = elongated(a, Vector3{1.0, 0.0, 0.0});

  REQUIRE(stretched->isInside(Vector3{1.0, 0.0, 0.0}));   // inside the slab
  REQUIRE_FALSE(stretched->isInside(Vector3{2.0, 0.0, 0.0}));

  const BBox b = stretched->bounds();
  REQUIRE(b.min.x == Approx(-1.5));
  REQUIRE(b.max.x == Approx(1.5));
}

TEST_CASE("dualContourField meshes a boolean union end-to-end",
          "[combinators][pipeline]") {
  auto [mA, gA] = makeBox(Vector3{0, 0, 0}, 0.5);
  auto [mB, gB] = makeBox(Vector3{0.5, 0.5, 0.5}, 0.5);
  auto a = std::make_shared<MeshSource>(*mA, *gA);
  auto b = std::make_shared<MeshSource>(*mB, *gB);
  auto u = unionOf(a, b);

  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;
  auto [outMesh, outGeom, outNormals] = dualContourField(*u, sp, cp);

  REQUIRE(outMesh != nullptr);
  REQUIRE(outMesh->nVertices() > 0);
  REQUIRE(outMesh->nFaces() > 0);
}
