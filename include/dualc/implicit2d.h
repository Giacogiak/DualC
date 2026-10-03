#pragma once

#include "dualc/implicit.h"

#include "geometrycentral/utilities/vector2.h"

#include <cmath>
#include <memory>
#include <vector>

// 2D implicit fields and the operators that lift them into 3D.
//
// A 2D field is the cross-section authoring primitive: draw a profile in the
// plane, then `RevolveField` spins it around an axis or `ExtrudeField` sweeps
// it along one. Revolving a circle yields a torus; extruding a box yields a
// box -- the same shapes the 3D primitives give, reachable from a 2D sketch.

namespace dualc {

using geometrycentral::Vector2;

// Axis-aligned 2D bounding box.
struct BBox2D {
  Vector2 min{0.0, 0.0};
  Vector2 max{0.0, 0.0};

  bool isValid() const {
    return std::isfinite(min.x) && std::isfinite(min.y) &&
           std::isfinite(max.x) && std::isfinite(max.y) &&
           max.x >= min.x && max.y >= min.y;
  }
};

// A real-valued 2D implicit field. SDF convention: valueAt < 0 inside.
class ImplicitField2D {
public:
  virtual ~ImplicitField2D() = default;
  virtual double  valueAt(const Vector2& p)    const = 0;
  virtual Vector2 gradientAt(const Vector2& p) const = 0;
  virtual BBox2D  bounds() const = 0;
};

using Field2DPtr = std::shared_ptr<ImplicitField2D>;

// Base for 2D primitives: supplies a central-difference gradient so a
// subclass only has to provide valueAt() and bounds().
class PrimitiveField2D : public ImplicitField2D {
public:
  Vector2 gradientAt(const Vector2& p) const override {
    constexpr double e = 1e-4;
    const double dx = valueAt(Vector2{p.x + e, p.y}) -
                      valueAt(Vector2{p.x - e, p.y});
    const double dy = valueAt(Vector2{p.x, p.y + e}) -
                      valueAt(Vector2{p.x, p.y - e});
    const Vector2 g{dx, dy};
    const double n = g.norm();
    return (n > 1e-12) ? g * (1.0 / n) : Vector2{0.0, 1.0};
  }
};

// --- 2D primitives -------------------------------------------------------

// Circle of `radius` centred at `center`.
class Circle2D : public PrimitiveField2D {
public:
  Circle2D(const Vector2& center, double radius);
  double  valueAt(const Vector2& p) const override;
  Vector2 gradientAt(const Vector2& p) const override;
  BBox2D  bounds() const override;

private:
  Vector2 c_;
  double  r_;
};

// Axis-aligned rectangle: `halfExtents` from `center`.
class Box2D : public PrimitiveField2D {
public:
  Box2D(const Vector2& center, const Vector2& halfExtents);
  double valueAt(const Vector2& p) const override;
  BBox2D bounds() const override;

private:
  Vector2 c_;
  Vector2 h_;
};

// Thick segment (2D capsule): the `radius`-wide swept disc along [a, b].
class Segment2D : public PrimitiveField2D {
public:
  Segment2D(const Vector2& a, const Vector2& b, double radius);
  double valueAt(const Vector2& p) const override;
  BBox2D bounds() const override;

private:
  Vector2 a_, b_;
  double  r_;
};

// Signed distance to a simple polygon (vertices in order; the winding need
// not be consistent -- the sign is recovered from a crossing count).
class Polygon2D : public PrimitiveField2D {
public:
  explicit Polygon2D(std::vector<Vector2> vertices);
  double valueAt(const Vector2& p) const override;
  BBox2D bounds() const override;

private:
  std::vector<Vector2> v_;
};

// --- 2D -> 3D lifts ------------------------------------------------------

// Revolve a 2D field around the y-axis. The field's local x maps to
// (distance from the y-axis) - axisOffset, its local y maps to world y.
// Revolving a Circle2D yields a torus.
class RevolveField : public ImplicitField {
public:
  RevolveField(Field2DPtr profile, double axisOffset);
  double  valueAt(const Vector3& p)    const override;
  Vector3 gradientAt(const Vector3& p) const override;
  BBox    bounds() const override;

private:
  Field2DPtr profile_;
  double     offset_;
};

// Extrude a 2D field (in the xy-plane) along z by +/- halfHeight.
// Extruding a Box2D yields a box.
class ExtrudeField : public ImplicitField {
public:
  ExtrudeField(Field2DPtr profile, double halfHeight);
  double  valueAt(const Vector3& p)    const override;
  Vector3 gradientAt(const Vector3& p) const override;
  BBox    bounds() const override;

private:
  Field2DPtr profile_;
  double     h_;
};

} // namespace dualc
