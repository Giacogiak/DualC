#include "dualc/primitives.h"

#include <algorithm>
#include <cmath>

namespace dualc {

namespace {

double clampd(double x, double lo, double hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

double sgn(double x) { return x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0); }

Vector3 vabs(const Vector3& v) {
  return Vector3{std::abs(v.x), std::abs(v.y), std::abs(v.z)};
}

double dot2(const Vector3& v) { return dot(v, v); }

// length(max(d, 0)) for a 2D vector (dx, dy).
double len2max0(double dx, double dy) {
  const double x = std::max(dx, 0.0);
  const double y = std::max(dy, 0.0);
  return std::sqrt(x * x + y * y);
}

BBox boxMinMax(const Vector3& lo, const Vector3& hi) {
  BBox b;
  b.min = lo;
  b.max = hi;
  return b;
}

// Squared distance from `pe` to the segment along `e` from its base.
double edgeDist2(const Vector3& e, const Vector3& pe) {
  const Vector3 v = e * clampd(dot(e, pe) / dot2(e), 0.0, 1.0) - pe;
  return dot(v, v);
}

} // namespace

// ---- Capped torus (an arc) ----------------------------------------------

CappedTorusField::CappedTorusField(const Vector3& center, double angleRad,
                                   double majorRadius, double minorRadius)
    : c_(center), sin_(std::sin(angleRad)), cos_(std::cos(angleRad)),
      ra_(majorRadius), rb_(minorRadius) {}

double CappedTorusField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double px = std::abs(pl.x);
  const double k = (cos_ * px > sin_ * pl.y) ? (px * sin_ + pl.y * cos_)
                                             : std::hypot(px, pl.y);
  const double dd = px * px + pl.y * pl.y + pl.z * pl.z;
  return std::sqrt(std::max(dd + ra_ * ra_ - 2.0 * ra_ * k, 0.0)) - rb_;
}

BBox CappedTorusField::bounds() const {
  const double e = ra_ + rb_;
  return boxMinMax(c_ - Vector3::constant(e), c_ + Vector3::constant(e));
}

// ---- Link ---------------------------------------------------------------

LinkField::LinkField(const Vector3& center, double halfLength,
                     double majorRadius, double minorRadius)
    : c_(center), le_(halfLength), r1_(majorRadius), r2_(minorRadius) {}

double LinkField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double qy = std::max(std::abs(pl.y) - le_, 0.0);
  const double a = std::hypot(pl.x, qy) - r1_;
  return std::hypot(a, pl.z) - r2_;
}

BBox LinkField::bounds() const {
  const double rxz = r1_ + r2_;
  const double ry = le_ + r1_ + r2_;
  return boxMinMax(c_ - Vector3{rxz, ry, rxz}, c_ + Vector3{rxz, ry, rxz});
}

// ---- Cut sphere ---------------------------------------------------------

CutSphereField::CutSphereField(const Vector3& center, double radius,
                               double cutHeight)
    : c_(center), r_(radius), h_(cutHeight) {}

double CutSphereField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double w = std::sqrt(std::max(r_ * r_ - h_ * h_, 0.0));
  const double qx = std::hypot(pl.x, pl.z);
  const double qy = pl.y;

  const double s = std::max((h_ - r_) * qx * qx + w * w * (h_ + r_ - 2.0 * qy),
                            h_ * qx - w * qy);
  if (s < 0.0) return std::hypot(qx, qy) - r_;
  if (qx < w) return h_ - qy;
  return std::hypot(qx - w, qy - h_);
}

BBox CutSphereField::bounds() const {
  return boxMinMax(c_ + Vector3{-r_, h_, -r_}, c_ + Vector3{r_, r_, r_});
}

// ---- Cut hollow sphere --------------------------------------------------

CutHollowSphereField::CutHollowSphereField(const Vector3& center,
                                           double radius, double cutHeight,
                                           double thickness)
    : c_(center), r_(radius), h_(cutHeight), t_(thickness) {}

double CutHollowSphereField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double w = std::sqrt(std::max(r_ * r_ - h_ * h_, 0.0));
  const double qx = std::hypot(pl.x, pl.z);
  const double qy = pl.y;

  const double d = (h_ * qx < w * qy)
                       ? std::hypot(qx - w, qy - h_)
                       : std::abs(std::hypot(qx, qy) - r_);
  return d - t_;
}

BBox CutHollowSphereField::bounds() const {
  const double e = r_ + t_;
  return boxMinMax(c_ - Vector3::constant(e), c_ + Vector3::constant(e));
}

// ---- Death star ---------------------------------------------------------

DeathStarField::DeathStarField(const Vector3& center, double radiusMain,
                               double radiusBite, double distance)
    : c_(center), ra_(radiusMain), rb_(radiusBite), d_(distance) {}

double DeathStarField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double a = (ra_ * ra_ - rb_ * rb_ + d_ * d_) / (2.0 * d_);
  const double b = std::sqrt(std::max(ra_ * ra_ - a * a, 0.0));
  const double px = pl.x;
  const double py = std::hypot(pl.y, pl.z);

  if (px * b - py * a > d_ * std::max(b - py, 0.0)) {
    return std::hypot(px - a, py - b);
  }
  return std::max(std::hypot(px, py) - ra_,
                  -(std::hypot(px - d_, py) - rb_));
}

BBox DeathStarField::bounds() const {
  return boxMinMax(c_ - Vector3::constant(ra_), c_ + Vector3::constant(ra_));
}

// ---- Vesica segment -----------------------------------------------------

VesicaSegmentField::VesicaSegmentField(const Vector3& a, const Vector3& b,
                                       double width)
    : a_(a), b_(b), w_(width) {}

double VesicaSegmentField::valueAt(const Vector3& p) const {
  const Vector3 mid = (a_ + b_) * 0.5;
  const double l = (b_ - a_).norm();
  if (l < 1e-30) return (p - a_).norm() - w_;  // degenerate: a == b

  const Vector3 v = (b_ - a_) * (1.0 / l);
  const double y = dot(p - mid, v);
  const double qx = (p - mid - v * y).norm();
  const double qy = std::abs(y);

  const double r = 0.5 * l;
  const double d = 0.5 * (r * r - w_ * w_) / w_;

  double hx, hy, hz;
  if (r * qx < d * (qy - r)) {
    hx = 0.0; hy = r; hz = 0.0;
  } else {
    hx = -d; hy = 0.0; hz = d + w_;
  }
  return std::hypot(qx - hx, qy - hy) - hz;
}

BBox VesicaSegmentField::bounds() const {
  return boxMinMax(componentwiseMin(a_, b_) - Vector3::constant(w_),
                   componentwiseMax(a_, b_) + Vector3::constant(w_));
}

// ---- Rhombus ------------------------------------------------------------

RhombusField::RhombusField(const Vector3& center, double la, double lb,
                           double height, double cornerRadius)
    : c_(center), la_(la), lb_(lb), h_(height), ra_(cornerRadius) {}

double RhombusField::valueAt(const Vector3& p) const {
  const Vector3 pl = vabs(p - c_);
  const double f = clampd(
      (la_ * pl.x - lb_ * pl.z + lb_ * lb_) / (la_ * la_ + lb_ * lb_),
      0.0, 1.0);
  const double wx = pl.x - la_ * f;
  const double wz = pl.z - lb_ * (1.0 - f);
  const double qx = std::hypot(wx, wz) * sgn(wx) - ra_;
  const double qy = pl.y - h_;
  return std::min(std::max(qx, qy), 0.0) + len2max0(qx, qy);
}

BBox RhombusField::bounds() const {
  return boxMinMax(c_ - Vector3{la_ + ra_, h_, lb_ + ra_},
                   c_ + Vector3{la_ + ra_, h_, lb_ + ra_});
}

// ---- Vertical capsule ---------------------------------------------------

VerticalCapsuleField::VerticalCapsuleField(const Vector3& center,
                                           double height, double radius)
    : c_(center), h_(height), r_(radius) {}

double VerticalCapsuleField::valueAt(const Vector3& p) const {
  Vector3 pl = p - c_;
  pl.y -= clampd(pl.y, 0.0, h_);
  return pl.norm() - r_;
}

BBox VerticalCapsuleField::bounds() const {
  return boxMinMax(c_ + Vector3{-r_, -r_, -r_},
                   c_ + Vector3{r_, h_ + r_, r_});
}

// ---- Rounded cylinder ---------------------------------------------------

RoundedCylinderField::RoundedCylinderField(const Vector3& center,
                                           double radius, double roundRadius,
                                           double halfHeight)
    : c_(center), ra_(radius), rb_(roundRadius), h_(halfHeight) {}

double RoundedCylinderField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double dx = std::hypot(pl.x, pl.z) - ra_ + rb_;
  const double dy = std::abs(pl.y) - h_ + rb_;
  return std::min(std::max(dx, dy), 0.0) + len2max0(dx, dy) - rb_;
}

BBox RoundedCylinderField::bounds() const {
  return boxMinMax(c_ + Vector3{-ra_, -h_, -ra_},
                   c_ + Vector3{ra_, h_, ra_});
}

// ---- Triangle (open surface, unsigned distance) -------------------------

TriangleField::TriangleField(const Vector3& a, const Vector3& b,
                             const Vector3& c)
    : a_(a), b_(b), c_(c) {}

double TriangleField::valueAt(const Vector3& p) const {
  const Vector3 ba = b_ - a_, pa = p - a_;
  const Vector3 cb = c_ - b_, pb = p - b_;
  const Vector3 ac = a_ - c_, pc = p - c_;
  const Vector3 nor = cross(ba, ac);

  const double facing = sgn(dot(cross(ba, nor), pa)) +
                        sgn(dot(cross(cb, nor), pb)) +
                        sgn(dot(cross(ac, nor), pc));
  double d2;
  if (facing < 2.0) {
    d2 = std::min({edgeDist2(ba, pa), edgeDist2(cb, pb), edgeDist2(ac, pc)});
  } else {
    const double dn = dot(nor, pa);
    d2 = dn * dn / dot2(nor);
  }
  return std::sqrt(d2);
}

BBox TriangleField::bounds() const {
  return boxMinMax(componentwiseMin(componentwiseMin(a_, b_), c_),
                   componentwiseMax(componentwiseMax(a_, b_), c_));
}

// ---- Quad (open surface, unsigned distance) -----------------------------

QuadField::QuadField(const Vector3& a, const Vector3& b, const Vector3& c,
                     const Vector3& d)
    : a_(a), b_(b), c_(c), d_(d) {}

double QuadField::valueAt(const Vector3& p) const {
  const Vector3 ba = b_ - a_, pa = p - a_;
  const Vector3 cb = c_ - b_, pb = p - b_;
  const Vector3 dc = d_ - c_, pc = p - c_;
  const Vector3 ad = a_ - d_, pd = p - d_;
  const Vector3 nor = cross(ba, ad);

  const double facing = sgn(dot(cross(ba, nor), pa)) +
                        sgn(dot(cross(cb, nor), pb)) +
                        sgn(dot(cross(dc, nor), pc)) +
                        sgn(dot(cross(ad, nor), pd));
  double d2;
  if (facing < 3.0) {
    d2 = std::min({edgeDist2(ba, pa), edgeDist2(cb, pb),
                   edgeDist2(dc, pc), edgeDist2(ad, pd)});
  } else {
    const double dn = dot(nor, pa);
    d2 = dn * dn / dot2(nor);
  }
  return std::sqrt(d2);
}

BBox QuadField::bounds() const {
  Vector3 lo = componentwiseMin(componentwiseMin(a_, b_),
                                componentwiseMin(c_, d_));
  Vector3 hi = componentwiseMax(componentwiseMax(a_, b_),
                                componentwiseMax(c_, d_));
  return boxMinMax(lo, hi);
}

// ---- Infinite cone ------------------------------------------------------

InfiniteConeField::InfiniteConeField(const Vector3& center, double angleRad)
    : c_(center), sin_(std::sin(angleRad)), cos_(std::cos(angleRad)) {}

double InfiniteConeField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double qx = std::hypot(pl.x, pl.z);
  const double qy = -pl.y;
  const double m = std::max(qx * sin_ + qy * cos_, 0.0);
  const double ex = qx - sin_ * m, ey = qy - cos_ * m;
  const double d = std::hypot(ex, ey);
  return d * ((qx * cos_ - qy * sin_ < 0.0) ? -1.0 : 1.0);
}

BBox InfiniteConeField::bounds() const { return BBox::infinite(); }

} // namespace dualc
