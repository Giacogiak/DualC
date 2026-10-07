// Roadmap 17 #34: argument validation. The library used to accept every one of
// these and return NaNs, an inverted solid, or an empty field -- each of which
// reads downstream as "your model is wrong" rather than "your call was wrong".
// Precedent for throwing at construction: GridField and MeshSource::bakeToGrid
// already do it.
#include "dualc/implicit.h"

#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <stdexcept>

using namespace dualc;
using Catch::Approx;

namespace {

FieldPtr unitSphere() {
  return std::make_shared<SphereField>(Vector3{0.0, 0.0, 0.0}, 1.0);
}

// A field with no geometry that reports whatever bounds it is handed -- used to
// contrast the BBox::empty() sentinel with the default-constructed BBox{} that
// used to stand in for it.
class NoExtentField : public ImplicitField {
public:
  explicit NoExtentField(const BBox& b) : b_(b) {}
  double valueAt(const Vector3&) const override { return 1e30; }
  Vector3 gradientAt(const Vector3&) const override {
    return Vector3{0.0, 0.0, 1.0};
  }
  BBox bounds() const override { return b_; }

private:
  BBox b_;
};

} // namespace

TEST_CASE("a sphere rejects a negative radius but keeps radius 0",
          "[validation]") {
  // radius 0 is load-bearing: `sphere(radius=0)` is the distance-to-centre
  // control field in four documented graded-lattice recipes
  // (docs/command_reference/11-dualc_field/03-graded-and-morph.md).
  REQUIRE_NOTHROW(SphereField(Vector3{0.0, 0.0, 0.0}, 0.0));
  SphereField point(Vector3{0.0, 0.0, 0.0}, 0.0);
  CHECK(point.valueAt(Vector3{3.0, 4.0, 0.0}) == Approx(5.0));

  // A negative radius has no zero crossing anywhere: |p - c| + |r| > 0.
  CHECK_THROWS_AS(SphereField(Vector3{0.0, 0.0, 0.0}, -1.0),
                  std::invalid_argument);
}

TEST_CASE("TPMS primitives reject a non-positive wavelength", "[validation]") {
  const Vector3 c{0.0, 0.0, 0.0};
  // k = 2*pi/wavelength, so 0 makes every sin/cos argument infinite -> NaN.
  CHECK_THROWS_AS(GyroidField(c, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(SchwarzPField(c, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(DiamondField(c, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(FischerKochSField(c, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(LidinoidField(c, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(NeoviusField(c, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(GyroidField(c, -0.4), std::invalid_argument);
  REQUIRE_NOTHROW(GyroidField(c, 0.4));
}

TEST_CASE("scaled rejects a zero factor and no longer inverts a negative one",
          "[validation]") {
  CHECK_THROWS_AS(scaled(unitSphere(), 0.0), std::invalid_argument);

  // A negative factor is a scale composed with a point reflection through the
  // origin. It used to multiply the child's value by a negative number, which
  // negated it -- returning the COMPLEMENT of the solid asked for -- and to
  // report a min-above-max bounds box, which isValid() rejects, silently
  // sending the sampler to its BBox::unit() fallback.
  FieldPtr pos = scaled(unitSphere(), 2.0);
  FieldPtr neg = scaled(unitSphere(), -2.0);

  // The unit sphere is symmetric under point reflection, so the two must agree
  // exactly, everywhere.
  const Vector3 probes[] = {Vector3{0.0, 0.0, 0.0},  Vector3{1.0, 0.0, 0.0},
                            Vector3{0.0, 3.0, 0.0},  Vector3{2.0, 2.0, 2.0},
                            Vector3{-1.5, 0.4, 0.9}};
  for (const Vector3& p : probes) {
    CHECK(neg->valueAt(p) == Approx(pos->valueAt(p)));
    CHECK(neg->isInside(p) == pos->isInside(p));
  }
  // Inside stays inside: the sphere of radius 2 contains the origin.
  CHECK(neg->valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);
  CHECK(neg->valueAt(Vector3{5.0, 0.0, 0.0}) > 0.0);

  // ...and the bounds are a real box, not an inverted one.
  const BBox b = neg->bounds();
  CHECK(b.isValid());
  CHECK(b.min.x == Approx(-2.0));
  CHECK(b.max.x == Approx(2.0));

  // An off-centre child is reflected through the origin, as a negative scale
  // should be -- not merely scaled.
  auto off = std::make_shared<SphereField>(Vector3{1.0, 0.0, 0.0}, 0.5);
  FieldPtr r = scaled(off, -2.0);
  CHECK(r->valueAt(Vector3{-2.0, 0.0, 0.0}) < 0.0);   // reflected copy
  CHECK(r->valueAt(Vector3{2.0, 0.0, 0.0}) > 0.0);    // not where it was
}

TEST_CASE("the repeat factories reject nonsense periods and counts",
          "[validation]") {
  // 0 means "do not repeat this axis" and stays legal; a negative component
  // was silently treated the same way by every `s > 0` test.
  REQUIRE_NOTHROW(repeated(unitSphere(), Vector3{4.0, 0.0, 0.0}));
  CHECK_THROWS_AS(repeated(unitSphere(), Vector3{4.0, -1.0, 0.0}),
                  std::invalid_argument);

  // clampId folds a count of 0 to the single id 0, so asking for zero copies
  // silently produced exactly one.
  CHECK_THROWS_AS(
      repeatedLimited(unitSphere(), Vector3{4.0, 0.0, 0.0}, Vector3i{0, 1, 1}),
      std::invalid_argument);
  CHECK_THROWS_AS(
      repeatedLimited(unitSphere(), Vector3{4.0, 0.0, 0.0}, Vector3i{3, 1, -2}),
      std::invalid_argument);
  REQUIRE_NOTHROW(
      repeatedLimited(unitSphere(), Vector3{4.0, 0.0, 0.0}, Vector3i{3, 1, 1}));
}

TEST_CASE("transformed rejects a matrix its inverse cannot handle",
          "[validation]") {
  // TransformField inverts with Mat4::inverseRigid() = [R^T | -R^T t], which
  // is exact for a rotation + translation and wrong for anything else.
  REQUIRE_NOTHROW(transformed(unitSphere(), Mat4::identity()));
  REQUIRE_NOTHROW(transformed(
      unitSphere(), Mat4::translation(Vector3{1.0, -2.0, 0.5}) *
                        Mat4::rotation(Vector3{0.2, 1.0, -0.3}, 0.7)));

  Mat4 sheared = Mat4::identity();
  sheared.m[0][1] = 0.4;                       // x += 0.4*y
  CHECK_THROWS_AS(transformed(unitSphere(), sheared), std::invalid_argument);

  Mat4 scaledM = Mat4::identity();
  scaledM.m[0][0] = 2.0;                       // non-uniform scale
  CHECK_THROWS_AS(transformed(unitSphere(), scaledM), std::invalid_argument);

  Mat4 reflected = Mat4::identity();
  reflected.m[2][2] = -1.0;                    // det = -1
  CHECK_THROWS_AS(transformed(unitSphere(), reflected), std::invalid_argument);
}

TEST_CASE("the sampler rejects a nonsense depth pair", "[validation]") {
  SphereField f(Vector3{0.0, 0.0, 0.0}, 0.4);

  SamplerParams bad;
  bad.minDepth = 6;
  bad.maxDepth = 4;                            // inverted
  CHECK_THROWS_AS(sampleFieldToHermiteOctree(f, bad), std::invalid_argument);

  SamplerParams negative;
  negative.minDepth = -1;
  negative.maxDepth = 5;
  CHECK_THROWS_AS(sampleFieldToHermiteOctree(f, negative),
                  std::invalid_argument);

  SamplerParams ok;
  ok.minDepth = 2;
  ok.maxDepth = 4;
  REQUIRE_NOTHROW(sampleFieldToHermiteOctree(f, ok));
}

TEST_CASE("BBox::empty is the sentinel BBox{} could never be", "[validation]") {
  // The trap: a default-constructed BBox is the degenerate box at the origin,
  // and it is VALID -- so an emptiness guard written as `!b.isValid()` never
  // fires for it, and bboxUnion drags the origin into whatever it unions.
  CHECK(BBox{}.isValid());

  const BBox e = BBox::empty();
  CHECK_FALSE(e.isValid());
  CHECK_FALSE(e.isInfinite());   // not the unbounded sentinel either

  // Through the public surface: a union's bounds must ignore an operand that
  // has no extent. A sphere well away from the origin makes the difference
  // visible -- the old sentinel drags the origin in, the new one does not.
  auto farSphere = std::make_shared<SphereField>(Vector3{5.5, 5.5, 5.5}, 0.5);

  const BBox withEmpty =
      unionOf(farSphere, std::make_shared<NoExtentField>(BBox::empty()))
          ->bounds();
  CHECK(withEmpty.min.x == Approx(5.0));   // the empty operand is ignored
  CHECK(withEmpty.max.x == Approx(6.0));

  const BBox withLegacy =
      unionOf(farSphere, std::make_shared<NoExtentField>(BBox{}))->bounds();
  CHECK(withLegacy.min.x == Approx(0.0).margin(1e-12));  // the trap: the origin is dragged in
}
