#include "dualc/implicit2d.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace dualc {

namespace {

// Central-difference gradient of a 3D field's own valueAt.
Vector3 fdGradient(const ImplicitField& f, const Vector3& p) {
  constexpr double e = 1e-4;
  const double dx = f.valueAt(Vector3{p.x + e, p.y, p.z}) -
                    f.valueAt(Vector3{p.x - e, p.y, p.z});
  const double dy = f.valueAt(Vector3{p.x, p.y + e, p.z}) -
                    f.valueAt(Vector3{p.x, p.y - e, p.z});
  const double dz = f.valueAt(Vector3{p.x, p.y, p.z + e}) -
                    f.valueAt(Vector3{p.x, p.y, p.z - e});
  const Vector3 g{dx, dy, dz};
  const double n = g.norm();
  return (n > 1e-12) ? g * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
}

// length(max(d, 0)) for a 2D vector (dx, dy).
double len2max0(double dx, double dy) {
  const double x = std::max(dx, 0.0);
  const double y = std::max(dy, 0.0);
  return std::sqrt(x * x + y * y);
}

} // namespace

// ---- Revolution ---------------------------------------------------------

RevolveField::RevolveField(Field2DPtr profile, double axisOffset)
    : profile_(std::move(profile)), offset_(axisOffset) {}

double RevolveField::valueAt(const Vector3& p) const {
  const double r = std::hypot(p.x, p.z);
  return profile_->valueAt(Vector2{r - offset_, p.y});
}

Vector3 RevolveField::gradientAt(const Vector3& p) const {
  return fdGradient(*this, p);
}

BBox RevolveField::bounds() const {
  const BBox2D b2 = profile_->bounds();
  if (!b2.isValid()) return BBox::empty();
  const double maxR = std::max(std::abs(b2.min.x + offset_),
                               std::abs(b2.max.x + offset_));
  BBox b;
  b.min = Vector3{-maxR, b2.min.y, -maxR};
  b.max = Vector3{ maxR, b2.max.y,  maxR};
  return b;
}

// ---- Extrusion ----------------------------------------------------------

ExtrudeField::ExtrudeField(Field2DPtr profile, double halfHeight)
    : profile_(std::move(profile)), h_(halfHeight) {}

double ExtrudeField::valueAt(const Vector3& p) const {
  const double d = profile_->valueAt(Vector2{p.x, p.y});
  const double wy = std::abs(p.z) - h_;
  return std::min(std::max(d, wy), 0.0) + len2max0(d, wy);
}

Vector3 ExtrudeField::gradientAt(const Vector3& p) const {
  return fdGradient(*this, p);
}

BBox ExtrudeField::bounds() const {
  const BBox2D b2 = profile_->bounds();
  if (!b2.isValid()) return BBox::empty();
  BBox b;
  b.min = Vector3{b2.min.x, b2.min.y, -h_};
  b.max = Vector3{b2.max.x, b2.max.y,  h_};
  return b;
}

} // namespace dualc
