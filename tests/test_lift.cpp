#include "dualc/implicit2d.h"
#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace dualc;
using Catch::Approx;

// ===================================================================
// 2D primitives
// ===================================================================

TEST_CASE("Circle2D is a signed distance to a circle", "[lift]") {
  Circle2D circle(Vector2{1.0, 2.0}, 2.0);

  REQUIRE(circle.valueAt(Vector2{1.0, 2.0}) == Approx(-2.0));  // centre
  REQUIRE(circle.valueAt(Vector2{4.0, 2.0}) == Approx(1.0));   // outside
  REQUIRE(circle.valueAt(Vector2{3.0, 2.0}) == Approx(0.0));   // on rim

  const Vector2 g = circle.gradientAt(Vector2{5.0, 2.0});
  REQUIRE(g.norm() == Approx(1.0));
  REQUIRE(g.x == Approx(1.0));
}

TEST_CASE("Box2D is a signed distance to a rectangle", "[lift]") {
  Box2D box(Vector2{0.0, 0.0}, Vector2{2.0, 1.0});

  REQUIRE(box.valueAt(Vector2{0.0, 0.0}) == Approx(-1.0));   // centre
  REQUIRE(box.valueAt(Vector2{2.0, 0.0}) == Approx(0.0));    // on an edge
  REQUIRE(box.valueAt(Vector2{4.0, 0.0}) == Approx(2.0));    // off an edge
  REQUIRE(box.valueAt(Vector2{5.0, 4.0}) ==
          Approx(std::hypot(3.0, 3.0)));                     // off a corner
}

TEST_CASE("Segment2D is a thick 2D segment", "[lift]") {
  Segment2D seg(Vector2{-2.0, 0.0}, Vector2{2.0, 0.0}, 0.5);

  REQUIRE(seg.valueAt(Vector2{0.0, 0.0}) == Approx(-0.5));   // on the spine
  REQUIRE(seg.valueAt(Vector2{0.0, 0.5}) == Approx(0.0));    // on the edge
  REQUIRE(seg.valueAt(Vector2{3.0, 0.0}) == Approx(0.5));    // past the end
}

TEST_CASE("Polygon2D is a signed distance to a polygon", "[lift]") {
  Polygon2D tri({Vector2{0.0, 0.0}, Vector2{2.0, 0.0}, Vector2{0.0, 2.0}});

  REQUIRE(tri.valueAt(Vector2{0.4, 0.4}) < 0.0);   // inside
  REQUIRE(tri.valueAt(Vector2{5.0, 5.0}) > 0.0);   // outside
  REQUIRE(tri.valueAt(Vector2{1.0, 0.0}) == Approx(0.0).margin(1e-9));  // edge
}

// ===================================================================
// 2D -> 3D lifts
// ===================================================================

TEST_CASE("RevolveField of a circle reproduces a torus", "[lift]") {
  auto circle = std::make_shared<Circle2D>(Vector2{0.0, 0.0}, 0.5);
  RevolveField revolved(circle, 2.0);          // major 2, minor 0.5
  TorusField torus(Vector3{0.0, 0.0, 0.0}, 2.0, 0.5);

  for (const Vector3 p : {Vector3{2.0, 0.0, 0.0}, Vector3{2.5, 0.0, 0.0},
                          Vector3{0.0, 0.0, 0.0}, Vector3{0.0, 0.5, 2.0},
                          Vector3{1.3, 0.4, -1.1}}) {
    REQUIRE(revolved.valueAt(p) == Approx(torus.valueAt(p)).margin(1e-9));
  }

  const BBox b = revolved.bounds();
  REQUIRE(b.max.x == Approx(2.5));
  REQUIRE(b.max.y == Approx(0.5));
}

TEST_CASE("ExtrudeField of a box reproduces a box", "[lift]") {
  auto profile = std::make_shared<Box2D>(Vector2{0.0, 0.0}, Vector2{1.0, 0.5});
  ExtrudeField extruded(profile, 0.75);
  BoxField box(Vector3{-1.0, -0.5, -0.75}, Vector3{1.0, 0.5, 0.75});

  for (const Vector3 p : {Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
                          Vector3{2.0, 0.0, 0.0}, Vector3{2.0, 1.5, 1.75},
                          Vector3{0.3, -0.2, 0.5}}) {
    REQUIRE(extruded.valueAt(p) == Approx(box.valueAt(p)).margin(1e-9));
  }

  const BBox b = extruded.bounds();
  REQUIRE(b.min.z == Approx(-0.75));
  REQUIRE(b.max.x == Approx(1.0));
}

TEST_CASE("dualContourField meshes 2D lifts end-to-end", "[lift]") {
  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;

  SECTION("revolved profile") {
    auto circle = std::make_shared<Circle2D>(Vector2{0.0, 0.0}, 0.5);
    auto revolved = std::make_shared<RevolveField>(circle, 2.0);
    auto [mesh, geom, normals] = dualContourField(*revolved, sp, cp);
    REQUIRE(mesh != nullptr);
    REQUIRE(mesh->nFaces() > 0);
  }

  SECTION("extruded profile") {
    auto profile =
        std::make_shared<Box2D>(Vector2{0.0, 0.0}, Vector2{1.0, 0.5});
    auto extruded = std::make_shared<ExtrudeField>(profile, 0.75);
    auto [mesh, geom, normals] = dualContourField(*extruded, sp, cp);
    REQUIRE(mesh != nullptr);
    REQUIRE(mesh->nFaces() > 0);
  }
}
