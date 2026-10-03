#include "dualc/implicit2d.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace dualc {

namespace {

double clampd(double x, double lo, double hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

Vector2 vabs(const Vector2& v) {
  return Vector2{std::abs(v.x), std::abs(v.y)};
}

Vector2 vmax0(const Vector2& v) {
  return Vector2{std::max(v.x, 0.0), std::max(v.y, 0.0)};
}

BBox2D box2d(const Vector2& lo, const Vector2& hi) {
  BBox2D b;
  b.min = lo;
  b.max = hi;
  return b;
}

} // namespace

// ---- Circle -------------------------------------------------------------

Circle2D::Circle2D(const Vector2& center, double radius)
    : c_(center), r_(radius) {}

double Circle2D::valueAt(const Vector2& p) const {
  return (p - c_).norm() - r_;
}

Vector2 Circle2D::gradientAt(const Vector2& p) const {
  const Vector2 d = p - c_;
  const double n = d.norm();
  return (n > 1e-12) ? d * (1.0 / n) : Vector2{0.0, 1.0};
}

BBox2D Circle2D::bounds() const {
  return box2d(c_ - Vector2::constant(r_), c_ + Vector2::constant(r_));
}

// ---- Box ----------------------------------------------------------------

Box2D::Box2D(const Vector2& center, const Vector2& halfExtents)
    : c_(center), h_(halfExtents) {}

double Box2D::valueAt(const Vector2& p) const {
  const Vector2 q = vabs(p - c_) - h_;
  return vmax0(q).norm() + std::min(std::max(q.x, q.y), 0.0);
}

BBox2D Box2D::bounds() const { return box2d(c_ - h_, c_ + h_); }

// ---- Segment (2D capsule) -----------------------------------------------

Segment2D::Segment2D(const Vector2& a, const Vector2& b, double radius)
    : a_(a), b_(b), r_(radius) {}

double Segment2D::valueAt(const Vector2& p) const {
  const Vector2 pa = p - a_;
  const Vector2 ba = b_ - a_;
  const double bb = dot(ba, ba);
  const double h = (bb > 1e-30) ? clampd(dot(pa, ba) / bb, 0.0, 1.0) : 0.0;
  return (pa - ba * h).norm() - r_;
}

BBox2D Segment2D::bounds() const {
  return box2d(componentwiseMin(a_, b_) - Vector2::constant(r_),
               componentwiseMax(a_, b_) + Vector2::constant(r_));
}

// ---- Polygon ------------------------------------------------------------

Polygon2D::Polygon2D(std::vector<Vector2> vertices)
    : v_(std::move(vertices)) {}

double Polygon2D::valueAt(const Vector2& p) const {
  const std::size_t n = v_.size();
  if (n < 3) {
    // Degenerate: fall back to the distance to the first vertex (if any).
    return n == 0 ? 0.0 : (p - v_[0]).norm();
  }

  double d = dot(p - v_[0], p - v_[0]);
  double s = 1.0;
  for (std::size_t i = 0, j = n - 1; i < n; j = i, ++i) {
    const Vector2 e = v_[j] - v_[i];
    const Vector2 w = p - v_[i];
    const Vector2 b = w - e * clampd(dot(w, e) / dot(e, e), 0.0, 1.0);
    d = std::min(d, dot(b, b));

    // Winding: flip the sign on each edge the +y ray from p crosses.
    const bool c1 = p.y >= v_[i].y;
    const bool c2 = p.y < v_[j].y;
    const bool c3 = e.x * w.y > e.y * w.x;
    if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) s = -s;
  }
  return s * std::sqrt(d);
}

BBox2D Polygon2D::bounds() const {
  if (v_.empty()) return BBox2D{};
  Vector2 lo = v_[0], hi = v_[0];
  for (const Vector2& q : v_) {
    lo = componentwiseMin(lo, q);
    hi = componentwiseMax(hi, q);
  }
  return box2d(lo, hi);
}

} // namespace dualc
