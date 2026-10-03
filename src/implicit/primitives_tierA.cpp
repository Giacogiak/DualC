#include "dualc/primitives.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace dualc {

namespace {

double clampd(double x, double lo, double hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

double sgn(double x) { return x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0); }

Vector3 vabs(const Vector3& v) {
  return Vector3{std::abs(v.x), std::abs(v.y), std::abs(v.z)};
}

// Component-wise max with zero.
Vector3 vmax0(const Vector3& v) {
  return Vector3{std::max(v.x, 0.0), std::max(v.y, 0.0), std::max(v.z, 0.0)};
}

// Largest component.
double maxc(const Vector3& v) { return std::max(v.x, std::max(v.y, v.z)); }

BBox boxMinMax(const Vector3& lo, const Vector3& hi) {
  BBox b;
  b.min = lo;
  b.max = hi;
  return b;
}

} // namespace

// ---- Sphere -------------------------------------------------------------

SphereField::SphereField(const Vector3& center, double radius)
    : c_(center), r_(radius) {
  // radius == 0 is deliberately legal: a zero-radius sphere is the distance
  // field to a point, which the graded-lattice recipes use as a control field
  // (`sphere(radius=0)`). A NEGATIVE radius has no surface at all -- valueAt
  // is |p - c| + |r| > 0 everywhere -- so it contours to nothing.
  if (!(radius >= 0.0))
    throw std::invalid_argument(
        "SphereField: radius must be >= 0 (got " + std::to_string(radius) +
        "); a negative radius has no zero crossing anywhere");
}

double SphereField::valueAt(const Vector3& p) const {
  return (p - c_).norm() - r_;
}

Vector3 SphereField::gradientAt(const Vector3& p) const {
  const Vector3 d = p - c_;
  const double n = d.norm();
  return (n > 1e-12) ? d * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
}

BBox SphereField::bounds() const {
  return boxMinMax(c_ - Vector3::constant(r_), c_ + Vector3::constant(r_));
}

// ---- Box ----------------------------------------------------------------

BoxField::BoxField(const Vector3& min, const Vector3& max)
    : c_((min + max) * 0.5), h_((max - min) * 0.5) {}

double BoxField::valueAt(const Vector3& p) const {
  const Vector3 q = vabs(p - c_) - h_;
  return vmax0(q).norm() + std::min(maxc(q), 0.0);
}

BBox BoxField::bounds() const { return boxMinMax(c_ - h_, c_ + h_); }

// ---- Round box ----------------------------------------------------------

RoundBoxField::RoundBoxField(const Vector3& min, const Vector3& max,
                             double radius)
    : c_((min + max) * 0.5), h_((max - min) * 0.5), r_(radius) {}

double RoundBoxField::valueAt(const Vector3& p) const {
  const Vector3 q = vabs(p - c_) - h_ + Vector3::constant(r_);
  return std::min(maxc(q), 0.0) + vmax0(q).norm() - r_;
}

BBox RoundBoxField::bounds() const { return boxMinMax(c_ - h_, c_ + h_); }

// ---- Plane (infinite half-space) ----------------------------------------

PlaneField::PlaneField(const Vector3& normal, double offset)
    : n_(normal.unit()), o_(offset) {}

double PlaneField::valueAt(const Vector3& p) const {
  return dot(p, n_) + o_;
}

Vector3 PlaneField::gradientAt(const Vector3&) const { return n_; }

BBox PlaneField::bounds() const { return BBox::infinite(); }

// ---- Capsule ------------------------------------------------------------

CapsuleField::CapsuleField(const Vector3& a, const Vector3& b, double radius)
    : a_(a), b_(b), r_(radius) {}

double CapsuleField::valueAt(const Vector3& p) const {
  const Vector3 pa = p - a_;
  const Vector3 ba = b_ - a_;
  const double bb = dot(ba, ba);
  const double h = (bb > 1e-30) ? clampd(dot(pa, ba) / bb, 0.0, 1.0) : 0.0;
  return (pa - ba * h).norm() - r_;
}

BBox CapsuleField::bounds() const {
  return boxMinMax(componentwiseMin(a_, b_) - Vector3::constant(r_),
                   componentwiseMax(a_, b_) + Vector3::constant(r_));
}

// ---- Capped cylinder (exact, arbitrary axis) ----------------------------

CappedCylinderField::CappedCylinderField(const Vector3& a, const Vector3& b,
                                         double radius)
    : a_(a), b_(b), r_(radius) {}

double CappedCylinderField::valueAt(const Vector3& p) const {
  const Vector3 ba = b_ - a_;
  const Vector3 pa = p - a_;
  const double baba = dot(ba, ba);
  if (baba < 1e-30) return (p - a_).norm() - r_;  // degenerate: a == b

  const double paba = dot(pa, ba);
  const double x = (pa * baba - ba * paba).norm() - r_ * baba;
  const double y = std::abs(paba - baba * 0.5) - baba * 0.5;
  const double x2 = x * x;
  const double y2 = y * y * baba;

  double d;
  if (std::max(x, y) < 0.0) {
    d = -std::min(x2, y2);
  } else {
    d = (x > 0.0 ? x2 : 0.0) + (y > 0.0 ? y2 : 0.0);
  }
  return sgn(d) * std::sqrt(std::abs(d)) / baba;
}

BBox CappedCylinderField::bounds() const {
  return boxMinMax(componentwiseMin(a_, b_) - Vector3::constant(r_),
                   componentwiseMax(a_, b_) + Vector3::constant(r_));
}

// ---- Torus --------------------------------------------------------------

TorusField::TorusField(const Vector3& center, double majorRadius,
                       double minorRadius)
    : c_(center), R_(majorRadius), r_(minorRadius) {}

double TorusField::valueAt(const Vector3& p) const {
  const Vector3 d = p - c_;
  const double qx = std::sqrt(d.x * d.x + d.z * d.z) - R_;
  return std::sqrt(qx * qx + d.y * d.y) - r_;
}

BBox TorusField::bounds() const {
  const Vector3 e{R_ + r_, r_, R_ + r_};
  return boxMinMax(c_ - e, c_ + e);
}

// ---- Ellipsoid (bounded distance) ---------------------------------------

EllipsoidField::EllipsoidField(const Vector3& center, const Vector3& radii)
    : c_(center), rad_(radii) {}

double EllipsoidField::valueAt(const Vector3& p) const {
  const Vector3 d = p - c_;
  const Vector3 pr{d.x / rad_.x, d.y / rad_.y, d.z / rad_.z};
  const double k0 = pr.norm();
  const Vector3 pr2{d.x / (rad_.x * rad_.x), d.y / (rad_.y * rad_.y),
                    d.z / (rad_.z * rad_.z)};
  const double k1 = pr2.norm();
  if (k1 < 1e-30) {
    return -std::min(rad_.x, std::min(rad_.y, rad_.z));  // at the centre
  }
  return k0 * (k0 - 1.0) / k1;
}

BBox EllipsoidField::bounds() const {
  return boxMinMax(c_ - rad_, c_ + rad_);
}

} // namespace dualc
