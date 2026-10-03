#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <stdexcept>

using namespace dualc;
using Catch::Approx;

namespace {

// Central finite-difference gradient of any field, for cross-checking the
// closed-form / base-class gradients.
Vector3 fdGradient(const ImplicitField& f, const Vector3& p, double e = 1e-5) {
  const double dx = f.valueAt(Vector3{p.x + e, p.y, p.z}) -
                    f.valueAt(Vector3{p.x - e, p.y, p.z});
  const double dy = f.valueAt(Vector3{p.x, p.y + e, p.z}) -
                    f.valueAt(Vector3{p.x, p.y - e, p.z});
  const double dz = f.valueAt(Vector3{p.x, p.y, p.z + e}) -
                    f.valueAt(Vector3{p.x, p.y, p.z - e});
  return (Vector3{dx, dy, dz} / (2.0 * e)).normalize();
}

} // namespace

// ===================================================================
// Tier A
// ===================================================================

TEST_CASE("SphereField is a signed distance to a sphere", "[primitives]") {
  SphereField s(Vector3{1.0, 2.0, 3.0}, 2.0);

  REQUIRE(s.valueAt(Vector3{1.0, 2.0, 3.0}) == Approx(-2.0));   // centre
  REQUIRE(s.valueAt(Vector3{4.0, 2.0, 3.0}) == Approx(1.0));    // outside
  REQUIRE(s.valueAt(Vector3{3.0, 2.0, 3.0}) == Approx(0.0));    // on surface

  const Vector3 g = s.gradientAt(Vector3{5.0, 2.0, 3.0});
  REQUIRE(g.norm() == Approx(1.0));
  REQUIRE(g.x == Approx(1.0));
  REQUIRE(g.y == Approx(0.0).margin(1e-12));

  const BBox b = s.bounds();
  REQUIRE(b.min.x == Approx(-1.0));
  REQUIRE(b.max.z == Approx(5.0));
}

TEST_CASE("BoxField is a signed distance to a box", "[primitives]") {
  BoxField box(Vector3{-1.0, -1.0, -1.0}, Vector3{1.0, 1.0, 1.0});

  REQUIRE(box.valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-1.0));   // centre
  REQUIRE(box.valueAt(Vector3{1.0, 0.0, 0.0}) == Approx(0.0));    // on a face
  REQUIRE(box.valueAt(Vector3{3.0, 0.0, 0.0}) == Approx(2.0));    // off a face
  REQUIRE(box.valueAt(Vector3{2.0, 2.0, 2.0}) ==
          Approx(std::sqrt(3.0)));                                // off a corner

  // The base-class central-difference gradient agrees with an independent FD.
  const Vector3 p{1.0, 0.3, -0.2};  // on the +x face
  const Vector3 g = box.gradientAt(p);
  const Vector3 gf = fdGradient(box, p);
  REQUIRE(g.x == Approx(gf.x).margin(1e-3));
  REQUIRE(g.y == Approx(gf.y).margin(1e-3));
  REQUIRE(g.z == Approx(gf.z).margin(1e-3));
}

TEST_CASE("RoundBoxField rounds the corners", "[primitives]") {
  // Outer extent [-1,1]^3, corners rounded by 0.5.
  RoundBoxField rb(Vector3{-1.0, -1.0, -1.0}, Vector3{1.0, 1.0, 1.0}, 0.5);

  // Face centres are unaffected by rounding.
  REQUIRE(rb.valueAt(Vector3{1.0, 0.0, 0.0}) == Approx(0.0));
  // The rounded corner pulls the surface inward: the geometric corner
  // (1,1,1) is now strictly outside.
  REQUIRE(rb.valueAt(Vector3{1.0, 1.0, 1.0}) > 0.0);
}

TEST_CASE("PlaneField is a half-space with infinite bounds", "[primitives]") {
  PlaneField plane(Vector3{0.0, 1.0, 0.0}, 0.0);  // surface y = 0

  REQUIRE(plane.valueAt(Vector3{5.0, -3.0, 2.0}) == Approx(-3.0));
  REQUIRE(plane.valueAt(Vector3{5.0, 4.0, 2.0}) == Approx(4.0));

  const Vector3 g = plane.gradientAt(Vector3{1.0, 1.0, 1.0});
  REQUIRE(g.y == Approx(1.0));

  REQUIRE(plane.bounds().isInfinite());
}

TEST_CASE("CapsuleField is a swept sphere along a segment", "[primitives]") {
  CapsuleField cap(Vector3{-2.0, 0.0, 0.0}, Vector3{2.0, 0.0, 0.0}, 1.0);

  REQUIRE(cap.valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(-1.0));   // on axis
  REQUIRE(cap.valueAt(Vector3{0.0, 1.0, 0.0}) == Approx(0.0));    // tube surface
  REQUIRE(cap.valueAt(Vector3{4.0, 0.0, 0.0}) == Approx(1.0));    // past the cap
}

TEST_CASE("CappedCylinderField is a flat-capped cylinder", "[primitives]") {
  CappedCylinderField cyl(Vector3{0.0, -2.0, 0.0}, Vector3{0.0, 2.0, 0.0},
                          1.0);

  REQUIRE(cyl.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);            // inside
  REQUIRE(cyl.valueAt(Vector3{1.0, 0.0, 0.0}) == Approx(0.0));   // side surface
  REQUIRE(cyl.valueAt(Vector3{0.0, 2.0, 0.0}) == Approx(0.0));   // cap surface
  REQUIRE(cyl.valueAt(Vector3{0.0, 4.0, 0.0}) == Approx(2.0));   // past the cap
}

TEST_CASE("TorusField is a signed distance to a torus", "[primitives]") {
  TorusField torus(Vector3{0.0, 0.0, 0.0}, 3.0, 1.0);

  REQUIRE(torus.valueAt(Vector3{3.0, 0.0, 0.0}) == Approx(-1.0));  // tube core
  REQUIRE(torus.valueAt(Vector3{4.0, 0.0, 0.0}) == Approx(0.0));   // outer rim
  REQUIRE(torus.valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(0.0));   // inner rim
  REQUIRE(torus.valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(2.0));   // hole centre
}

TEST_CASE("EllipsoidField vanishes on the axis-aligned surface points",
          "[primitives]") {
  EllipsoidField ell(Vector3{0.0, 0.0, 0.0}, Vector3{2.0, 1.0, 1.0});

  REQUIRE(ell.valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(0.0).margin(1e-9));
  REQUIRE(ell.valueAt(Vector3{0.0, 1.0, 0.0}) == Approx(0.0).margin(1e-9));
  REQUIRE(ell.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);   // centre is inside
  REQUIRE(ell.valueAt(Vector3{3.0, 0.0, 0.0}) > 0.0);   // outside
}

TEST_CASE("dualContourField meshes a SphereField end-to-end", "[primitives]") {
  auto sphere = std::make_shared<SphereField>(Vector3{0.0, 0.0, 0.0}, 1.0);

  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;
  auto [mesh, geom, normals] = dualContourField(*sphere, sp, cp);

  REQUIRE(mesh != nullptr);
  REQUIRE(mesh->nVertices() > 0);
  REQUIRE(mesh->nFaces() > 0);
}

TEST_CASE("BoxField reproduces the unit-cube regression", "[primitives]") {
  auto box = std::make_shared<BoxField>(Vector3{-0.5, -0.5, -0.5},
                                        Vector3{0.5, 0.5, 0.5});

  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;
  auto [mesh, geom, normals] = dualContourField(*box, sp, cp);

  REQUIRE(mesh != nullptr);
  REQUIRE(mesh->nVertices() > 0);
  REQUIRE(mesh->nFaces() > 0);
}

TEST_CASE("Sampling an infinite field without rootBounds throws",
          "[primitives]") {
  auto plane = std::make_shared<PlaneField>(Vector3{0.0, 1.0, 0.0}, 0.0);

  SamplerParams sp;
  ContourerParams cp;
  REQUIRE_THROWS_AS(dualContourField(*plane, sp, cp), std::invalid_argument);

  // With an explicit finite root box it samples fine.
  SamplerParams sp2;
  sp2.maxDepth = 4;
  BBox root;
  root.min = Vector3{-2.0, -2.0, -2.0};
  root.max = Vector3{2.0, 2.0, 2.0};
  sp2.rootBounds = root;
  auto [mesh, geom, normals] = dualContourField(*plane, sp2, cp);
  REQUIRE(mesh != nullptr);
  REQUIRE(mesh->nFaces() > 0);
}

// ===================================================================
// Tier B
// ===================================================================

TEST_CASE("BoxFrameField is a hollow frame", "[primitives]") {
  BoxFrameField frame(Vector3{-1.0, -1.0, -1.0}, Vector3{1.0, 1.0, 1.0}, 0.2);

  REQUIRE(frame.valueAt(Vector3{0.0, 0.0, 0.0}) > 0.0);   // hollow centre
  REQUIRE(frame.valueAt(Vector3{0.9, 0.9, 0.0}) < 0.0);   // inside an edge strut
  REQUIRE(frame.valueAt(Vector3{0.0, 0.9, 0.9}) < 0.0);   // inside another strut
}

TEST_CASE("ConeField is an exact finite cone", "[primitives]") {
  // Apex at origin, opening downward, height 2, base radius 1 (tan = 0.5).
  ConeField cone(Vector3{0.0, 0.0, 0.0}, std::atan(0.5), 2.0);

  REQUIRE(cone.valueAt(Vector3{0.0, -1.0, 0.0}) < 0.0);            // inside
  REQUIRE(cone.valueAt(Vector3{0.0, -2.0, 0.0}) == Approx(0.0));   // base centre
  REQUIRE(cone.valueAt(Vector3{0.0, 1.0, 0.0}) == Approx(1.0));    // above apex
}

TEST_CASE("CappedConeField is a truncated cone", "[primitives]") {
  // y in [-2,2], bottom radius 2, top radius 1.
  CappedConeField cc(Vector3{0.0, 0.0, 0.0}, 2.0, 2.0, 1.0);

  REQUIRE(cc.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);            // inside
  REQUIRE(cc.valueAt(Vector3{0.0, 2.0, 0.0}) == Approx(0.0));   // top cap centre
  REQUIRE(cc.valueAt(Vector3{0.0, -2.0, 0.0}) == Approx(0.0));  // bottom cap
  REQUIRE(cc.valueAt(Vector3{2.0, -2.0, 0.0}) == Approx(0.0));  // bottom rim
}

TEST_CASE("RoundConeField is a sphere-capped taper", "[primitives]") {
  RoundConeField rc(Vector3{0.0, -2.0, 0.0}, Vector3{0.0, 2.0, 0.0}, 2.0, 1.0);

  REQUIRE(rc.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);            // inside
  REQUIRE(rc.valueAt(Vector3{0.0, -4.0, 0.0}) == Approx(0.0));  // bottom sphere
  REQUIRE(rc.valueAt(Vector3{0.0, 3.0, 0.0}) == Approx(0.0));   // top sphere
}

TEST_CASE("InfiniteCylinderField is unbounded", "[primitives]") {
  InfiniteCylinderField cyl(Vector3{0.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                            1.0);

  REQUIRE(cyl.valueAt(Vector3{0.0, 100.0, 0.0}) == Approx(-1.0));  // on axis
  REQUIRE(cyl.valueAt(Vector3{1.0, 5.0, 0.0}) == Approx(0.0));     // surface
  REQUIRE(cyl.valueAt(Vector3{3.0, -7.0, 0.0}) == Approx(2.0));    // outside
  REQUIRE(cyl.bounds().isInfinite());
}

TEST_CASE("HexPrismField is a hexagonal prism", "[primitives]") {
  // Apothem 1, half-length 2 along z.
  HexPrismField hex(Vector3{0.0, 0.0, 0.0}, 1.0, 2.0);

  REQUIRE(hex.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);            // inside
  REQUIRE(hex.valueAt(Vector3{0.0, 1.0, 0.0}) == Approx(0.0));   // flat side
  REQUIRE(hex.valueAt(Vector3{0.0, 0.0, 3.0}) == Approx(1.0));   // past the cap
}

TEST_CASE("TriPrismField encloses a triangular prism", "[primitives]") {
  TriPrismField tri(Vector3{0.0, 0.0, 0.0}, 1.0, 2.0);

  REQUIRE(tri.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);   // inside
  REQUIRE(tri.valueAt(Vector3{5.0, 5.0, 5.0}) > 0.0);   // outside
}

TEST_CASE("OctahedronField is a signed distance to an octahedron",
          "[primitives]") {
  OctahedronField oct(Vector3{0.0, 0.0, 0.0}, 1.0);            // exact
  REQUIRE(oct.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);          // inside
  REQUIRE(oct.valueAt(Vector3{1.0, 0.0, 0.0}) == Approx(0.0)); // a vertex
  REQUIRE(oct.valueAt(Vector3{2.0, 0.0, 0.0}) > 0.0);          // outside

  OctahedronField bnd(Vector3{0.0, 0.0, 0.0}, 1.0, false);     // bounded
  REQUIRE(bnd.valueAt(Vector3{1.0, 0.0, 0.0}) == Approx(0.0));
  REQUIRE(bnd.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);
}

TEST_CASE("PyramidField is a square pyramid", "[primitives]") {
  PyramidField pyr(Vector3{0.0, 0.0, 0.0}, 2.0);  // base side 1, apex y = 2

  REQUIRE(pyr.valueAt(Vector3{0.0, 1.0, 0.0}) < 0.0);            // inside
  REQUIRE(pyr.valueAt(Vector3{0.0, 0.0, 0.0}) == Approx(0.0));   // base centre
  REQUIRE(pyr.valueAt(Vector3{0.0, 3.0, 0.0}) == Approx(1.0));   // above apex
}

TEST_CASE("SolidAngleField is a clipped spherical wedge", "[primitives]") {
  SolidAngleField sa(Vector3{0.0, 0.0, 0.0}, 3.14159265 / 4.0, 2.0);

  REQUIRE(sa.valueAt(Vector3{0.0, 1.0, 0.0}) < 0.0);   // inside the wedge
  REQUIRE(sa.valueAt(Vector3{0.0, 3.0, 0.0}) > 0.0);   // past the sphere
  REQUIRE(sa.valueAt(Vector3{2.0, 0.1, 0.0}) > 0.0);   // outside the cone
}

// ===================================================================
// Tier C
// ===================================================================

TEST_CASE("CappedTorusField is a torus arc", "[primitives]") {
  CappedTorusField ct(Vector3{0.0, 0.0, 0.0}, 3.14159265 / 2.0, 3.0, 1.0);

  REQUIRE(ct.valueAt(Vector3{0.0, 3.0, 0.0}) == Approx(-1.0));  // tube core
  REQUIRE(ct.valueAt(Vector3{0.0, 4.0, 0.0}) == Approx(0.0));   // outer rim
  REQUIRE(ct.valueAt(Vector3{0.0, 0.0, 0.0}) > 0.0);            // hole centre
}

TEST_CASE("LinkField is a torus stretched along y", "[primitives]") {
  LinkField link(Vector3{0.0, 0.0, 0.0}, 1.0, 2.0, 0.5);

  REQUIRE(link.valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(-0.5));  // tube core
  REQUIRE(link.valueAt(Vector3{2.5, 0.0, 0.0}) == Approx(0.0));   // tube surface
  REQUIRE(link.valueAt(Vector3{0.0, 0.0, 0.0}) > 0.0);            // hole centre
}

TEST_CASE("CutSphereField keeps the cap above the cut plane", "[primitives]") {
  CutSphereField cs(Vector3{0.0, 0.0, 0.0}, 2.0, 1.0);  // r = 2, cut at y = 1

  REQUIRE(cs.valueAt(Vector3{0.0, 1.5, 0.0}) < 0.0);            // inside the cap
  REQUIRE(cs.valueAt(Vector3{0.0, 2.0, 0.0}) == Approx(0.0));   // top of sphere
  REQUIRE(cs.valueAt(Vector3{0.0, 0.0, 0.0}) > 0.0);            // below the cut
}

TEST_CASE("CutHollowSphereField is a thin bowl", "[primitives]") {
  CutHollowSphereField ch(Vector3{0.0, 0.0, 0.0}, 2.0, 1.0, 0.1);

  REQUIRE(ch.valueAt(Vector3{2.0, 0.0, 0.0}) < 0.0);   // within the shell wall
  REQUIRE(ch.valueAt(Vector3{0.0, 0.0, 0.0}) > 0.0);   // hollow interior
}

TEST_CASE("DeathStarField is a sphere with a spherical bite", "[primitives]") {
  // Main sphere r = 2 at origin; bite sphere r = 1 centred at (2,0,0).
  DeathStarField ds(Vector3{0.0, 0.0, 0.0}, 2.0, 1.0, 2.0);

  REQUIRE(ds.valueAt(Vector3{-2.0, 0.0, 0.0}) == Approx(0.0));  // far surface
  REQUIRE(ds.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);            // inside
  REQUIRE(ds.valueAt(Vector3{2.0, 0.0, 0.0}) > 0.0);            // bitten away
}

TEST_CASE("VesicaSegmentField is a lens swept along a segment",
          "[primitives]") {
  VesicaSegmentField ves(Vector3{-2.0, 0.0, 0.0}, Vector3{2.0, 0.0, 0.0}, 1.5);

  REQUIRE(ves.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);           // inside
  REQUIRE(ves.valueAt(Vector3{0.0, 1.5, 0.0}) == Approx(0.0));  // mid bulge
  REQUIRE(ves.valueAt(Vector3{0.0, 3.0, 0.0}) > 0.0);           // outside
}

TEST_CASE("RhombusField is a rounded diamond prism", "[primitives]") {
  RhombusField rh(Vector3{0.0, 0.0, 0.0}, 2.0, 1.0, 0.5, 0.0);

  REQUIRE(rh.valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(0.0));  // x diagonal tip
  REQUIRE(rh.valueAt(Vector3{0.0, 0.0, 1.0}) == Approx(0.0));  // z diagonal tip
  REQUIRE(rh.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);           // inside
  REQUIRE(rh.valueAt(Vector3{3.0, 0.0, 0.0}) > 0.0);           // outside
}

TEST_CASE("VerticalCapsuleField is a capsule from y=0 to y=h", "[primitives]") {
  VerticalCapsuleField vc(Vector3{0.0, 0.0, 0.0}, 2.0, 0.5);

  REQUIRE(vc.valueAt(Vector3{0.0, 1.0, 0.0}) == Approx(-0.5));  // on the axis
  REQUIRE(vc.valueAt(Vector3{0.5, 1.0, 0.0}) == Approx(0.0));   // tube surface
  REQUIRE(vc.valueAt(Vector3{0.0, 3.0, 0.0}) > 0.0);            // past the top
}

TEST_CASE("RoundedCylinderField is a cylinder with rounded rims",
          "[primitives]") {
  RoundedCylinderField rc(Vector3{0.0, 0.0, 0.0}, 2.0, 0.3, 1.0);

  REQUIRE(rc.valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);            // inside
  REQUIRE(rc.valueAt(Vector3{2.0, 0.0, 0.0}) == Approx(0.0));   // side surface
  REQUIRE(rc.valueAt(Vector3{0.0, 1.0, 0.0}) == Approx(0.0));   // cap surface
  REQUIRE(rc.valueAt(Vector3{0.0, 2.0, 0.0}) == Approx(1.0));   // past the cap
}

TEST_CASE("TriangleField is an unsigned distance to a triangle",
          "[primitives]") {
  TriangleField tri(Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
                    Vector3{0.0, 1.0, 0.0});

  REQUIRE(tri.valueAt(Vector3{0.25, 0.25, 0.0}) == Approx(0.0).margin(1e-9));
  REQUIRE(tri.valueAt(Vector3{0.25, 0.25, 1.0}) == Approx(1.0));
  REQUIRE(tri.valueAt(Vector3{0.25, 0.25, -1.0}) == Approx(1.0));  // unsigned
  REQUIRE(tri.valueAt(Vector3{5.0, 5.0, 0.0}) > 0.0);
}

TEST_CASE("QuadField is an unsigned distance to a quad", "[primitives]") {
  QuadField quad(Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
                 Vector3{1.0, 1.0, 0.0}, Vector3{0.0, 1.0, 0.0});

  REQUIRE(quad.valueAt(Vector3{0.5, 0.5, 0.0}) == Approx(0.0).margin(1e-9));
  REQUIRE(quad.valueAt(Vector3{0.5, 0.5, 2.0}) == Approx(2.0));
}

TEST_CASE("InfiniteConeField is unbounded", "[primitives]") {
  InfiniteConeField cone(Vector3{0.0, 0.0, 0.0}, 3.14159265 / 4.0);

  REQUIRE(cone.valueAt(Vector3{0.0, -5.0, 0.0}) < 0.0);  // inside (opens -y)
  REQUIRE(cone.valueAt(Vector3{0.0, 5.0, 0.0}) > 0.0);   // wrong side
  REQUIRE(cone.bounds().isInfinite());
}

TEST_CASE("dualContourField meshes a Tier-C primitive end-to-end",
          "[primitives]") {
  auto rc = std::make_shared<RoundedCylinderField>(Vector3{0.0, 0.0, 0.0},
                                                   2.0, 0.3, 1.0);
  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;
  auto [mesh, geom, normals] = dualContourField(*rc, sp, cp);

  REQUIRE(mesh != nullptr);
  REQUIRE(mesh->nVertices() > 0);
  REQUIRE(mesh->nFaces() > 0);
}
