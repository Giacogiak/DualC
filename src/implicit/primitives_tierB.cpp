#include "dualc/primitives.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace dualc {

namespace {

double clampd(double x, double lo, double hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

double sgn(double x) { return x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0); }

Vector3 vabs(const Vector3& v) {
  return Vector3{std::abs(v.x), std::abs(v.y), std::abs(v.z)};
}

Vector3 vmax0(const Vector3& v) {
  return Vector3{std::max(v.x, 0.0), std::max(v.y, 0.0), std::max(v.z, 0.0)};
}

BBox boxMinMax(const Vector3& lo, const Vector3& hi) {
  BBox b;
  b.min = lo;
  b.max = hi;
  return b;
}

// length(max(d, 0)) for a 2D vector (d.x, d.y).
double len2max0(double dx, double dy) {
  const double x = std::max(dx, 0.0);
  const double y = std::max(dy, 0.0);
  return std::sqrt(x * x + y * y);
}

} // namespace

// ---- Box frame ----------------------------------------------------------

BoxFrameField::BoxFrameField(const Vector3& min, const Vector3& max,
                             double edgeThickness)
    : c_((min + max) * 0.5), h_((max - min) * 0.5), e_(edgeThickness) {}

double BoxFrameField::valueAt(const Vector3& p) const {
  const Vector3 pp = vabs(p - c_) - h_;
  const Vector3 q = vabs(pp + Vector3::constant(e_)) - Vector3::constant(e_);

  auto strut = [](double a, double b, double c) {
    return vmax0(Vector3{a, b, c}).norm() +
           std::min(std::max(a, std::max(b, c)), 0.0);
  };
  return std::min({strut(pp.x, q.y, q.z),
                   strut(q.x, pp.y, q.z),
                   strut(q.x, q.y, pp.z)});
}

BBox BoxFrameField::bounds() const { return boxMinMax(c_ - h_, c_ + h_); }

// ---- Cone (exact, finite) -----------------------------------------------

ConeField::ConeField(const Vector3& center, double angleRad, double height)
    : c_(center), sin_(std::sin(angleRad)), cos_(std::cos(angleRad)),
      h_(height) {}

double ConeField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double qx = h_ * (sin_ / cos_);
  const double qy = -h_;
  const double wx = std::sqrt(pl.x * pl.x + pl.z * pl.z);
  const double wy = pl.y;

  const double t1 = clampd((wx * qx + wy * qy) / (qx * qx + qy * qy), 0.0, 1.0);
  const double ax = wx - qx * t1, ay = wy - qy * t1;
  const double t2 = clampd(wx / qx, 0.0, 1.0);
  const double bx = wx - qx * t2, by = wy - qy;

  const double k = sgn(qy);
  const double d = std::min(ax * ax + ay * ay, bx * bx + by * by);
  const double s = std::max(k * (wx * qy - wy * qx), k * (wy - qy));
  return std::sqrt(d) * sgn(s);
}

BBox ConeField::bounds() const {
  const double rb = h_ * (sin_ / cos_);
  return boxMinMax(c_ + Vector3{-rb, -h_, -rb}, c_ + Vector3{rb, 0.0, rb});
}

// ---- Capped cone --------------------------------------------------------

CappedConeField::CappedConeField(const Vector3& center, double height,
                                 double radiusLow, double radiusHigh)
    : c_(center), h_(height), r1_(radiusLow), r2_(radiusHigh) {}

double CappedConeField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double qx = std::sqrt(pl.x * pl.x + pl.z * pl.z);
  const double qy = pl.y;

  const double k1x = r2_, k1y = h_;
  const double k2x = r2_ - r1_, k2y = 2.0 * h_;

  const double cax = qx - std::min(qx, (qy < 0.0) ? r1_ : r2_);
  const double cay = std::abs(qy) - h_;

  const double t = clampd(((k1x - qx) * k2x + (k1y - qy) * k2y) /
                              (k2x * k2x + k2y * k2y),
                          0.0, 1.0);
  const double cbx = qx - k1x + k2x * t;
  const double cby = qy - k1y + k2y * t;

  const double s = (cbx < 0.0 && cay < 0.0) ? -1.0 : 1.0;
  return s * std::sqrt(std::min(cax * cax + cay * cay, cbx * cbx + cby * cby));
}

BBox CappedConeField::bounds() const {
  const double r = std::max(r1_, r2_);
  return boxMinMax(c_ + Vector3{-r, -h_, -r}, c_ + Vector3{r, h_, r});
}

// ---- Round cone (exact, arbitrary axis) ---------------------------------

RoundConeField::RoundConeField(const Vector3& a, const Vector3& b,
                               double radiusA, double radiusB)
    : a_(a), b_(b), r1_(radiusA), r2_(radiusB) {}

double RoundConeField::valueAt(const Vector3& p) const {
  const Vector3 ba = b_ - a_;
  const double l2 = dot(ba, ba);
  if (l2 < 1e-30) return (p - a_).norm() - r1_;  // degenerate: a == b

  const double rr = r1_ - r2_;
  const double a2 = l2 - rr * rr;
  const double il2 = 1.0 / l2;
  const Vector3 pa = p - a_;
  const double y = dot(pa, ba);
  const double z = y - l2;
  const Vector3 xv = pa * l2 - ba * y;
  const double x2 = dot(xv, xv);
  const double y2 = y * y * l2;
  const double z2 = z * z * l2;
  const double k = sgn(rr) * rr * rr * x2;

  if (sgn(z) * a2 * z2 > k) return std::sqrt(x2 + z2) * il2 - r2_;
  if (sgn(y) * a2 * y2 < k) return std::sqrt(x2 + y2) * il2 - r1_;
  return (std::sqrt(x2 * a2 * il2) + y * rr) * il2 - r1_;
}

BBox RoundConeField::bounds() const {
  return boxMinMax(componentwiseMin(a_ - Vector3::constant(r1_),
                                    b_ - Vector3::constant(r2_)),
                   componentwiseMax(a_ + Vector3::constant(r1_),
                                    b_ + Vector3::constant(r2_)));
}

// ---- Infinite cylinder --------------------------------------------------

InfiniteCylinderField::InfiniteCylinderField(const Vector3& axisPoint,
                                             const Vector3& axisDir,
                                             double radius)
    : p0_(axisPoint), dir_(axisDir.unit()), r_(radius) {}

double InfiniteCylinderField::valueAt(const Vector3& p) const {
  const Vector3 d = p - p0_;
  const Vector3 perp = d - dir_ * dot(d, dir_);
  return perp.norm() - r_;
}

BBox InfiniteCylinderField::bounds() const { return BBox::infinite(); }

// ---- Hexagonal prism (axis = z) -----------------------------------------

HexPrismField::HexPrismField(const Vector3& center, double radius,
                             double halfLength)
    : c_(center), r_(radius), hl_(halfLength) {}

double HexPrismField::valueAt(const Vector3& p) const {
  const double kx = -0.8660254, ky = 0.5, kz = 0.57735;
  const Vector3 pl = vabs(p - c_);

  const double m = 2.0 * std::min(kx * pl.x + ky * pl.y, 0.0);
  const double px = pl.x - m * kx;
  const double py = pl.y - m * ky;

  const double cx = clampd(px, -kz * r_, kz * r_);
  const double dx = std::sqrt((px - cx) * (px - cx) + (py - r_) * (py - r_)) *
                    sgn(py - r_);
  const double dy = pl.z - hl_;
  return std::min(std::max(dx, dy), 0.0) + len2max0(dx, dy);
}

BBox HexPrismField::bounds() const {
  const double rc = r_ / 0.8660254;  // hexagon circumradius
  return boxMinMax(c_ + Vector3{-rc, -rc, -hl_},
                   c_ + Vector3{rc, rc, hl_});
}

// ---- Triangular prism (bounded distance, axis = z) ----------------------

TriPrismField::TriPrismField(const Vector3& center, double radius,
                             double halfLength)
    : c_(center), r_(radius), hl_(halfLength) {}

double TriPrismField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const Vector3 q = vabs(pl);
  return std::max(q.z - hl_,
                  std::max(q.x * 0.866025 + pl.y * 0.5, -pl.y) - r_ * 0.5);
}

BBox TriPrismField::bounds() const {
  return boxMinMax(c_ + Vector3{-r_, -r_, -hl_},
                   c_ + Vector3{r_, r_, hl_});
}

// ---- Octahedron ---------------------------------------------------------

OctahedronField::OctahedronField(const Vector3& center, double size,
                                 bool exact)
    : c_(center), s_(size), exact_(exact) {}

double OctahedronField::valueAt(const Vector3& p) const {
  const Vector3 pl = vabs(p - c_);
  if (!exact_) {
    return (pl.x + pl.y + pl.z - s_) * 0.57735027;
  }

  const double m = pl.x + pl.y + pl.z - s_;
  Vector3 q;
  if (3.0 * pl.x < m) {
    q = pl;
  } else if (3.0 * pl.y < m) {
    q = Vector3{pl.y, pl.z, pl.x};
  } else if (3.0 * pl.z < m) {
    q = Vector3{pl.z, pl.x, pl.y};
  } else {
    return m * 0.57735027;
  }
  const double k = clampd(0.5 * (q.z - q.y + s_), 0.0, s_);
  return Vector3{q.x, q.y - s_ + k, q.z - k}.norm();
}

BBox OctahedronField::bounds() const {
  return boxMinMax(c_ - Vector3::constant(s_), c_ + Vector3::constant(s_));
}

// ---- Pyramid ------------------------------------------------------------

PyramidField::PyramidField(const Vector3& center, double height)
    : c_(center), h_(height) {}

double PyramidField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double m2 = h_ * h_ + 0.25;

  double ax = std::abs(pl.x), az = std::abs(pl.z);
  if (az > ax) std::swap(ax, az);  // p.xz = (p.z>p.x) ? p.zx : p.xz
  ax -= 0.5;
  az -= 0.5;

  const double qx = az;
  const double qy = h_ * pl.y - 0.5 * ax;
  const double qz = h_ * ax + 0.5 * pl.y;

  const double s = std::max(-qx, 0.0);
  const double t = clampd((qy - 0.5 * az) / (m2 + 0.25), 0.0, 1.0);
  const double a = m2 * (qx + s) * (qx + s) + qy * qy;
  const double b = m2 * (qx + 0.5 * t) * (qx + 0.5 * t) +
                   (qy - m2 * t) * (qy - m2 * t);
  const double d2 =
      (std::min(qy, -qx * m2 - qy * 0.5) > 0.0) ? 0.0 : std::min(a, b);
  return std::sqrt((d2 + qz * qz) / m2) * sgn(std::max(qz, -pl.y));
}

BBox PyramidField::bounds() const {
  return boxMinMax(c_ + Vector3{-0.5, 0.0, -0.5},
                   c_ + Vector3{0.5, h_, 0.5});
}

// ---- Solid angle --------------------------------------------------------

SolidAngleField::SolidAngleField(const Vector3& center, double angleRad,
                                 double radius)
    : c_(center), sin_(std::sin(angleRad)), cos_(std::cos(angleRad)),
      ra_(radius) {}

double SolidAngleField::valueAt(const Vector3& p) const {
  const Vector3 pl = p - c_;
  const double qx = std::sqrt(pl.x * pl.x + pl.z * pl.z);
  const double qy = pl.y;

  const double l = std::sqrt(qx * qx + qy * qy) - ra_;
  const double dt = clampd(qx * sin_ + qy * cos_, 0.0, ra_);
  const double mx = qx - sin_ * dt, my = qy - cos_ * dt;
  const double m = std::sqrt(mx * mx + my * my);
  return std::max(l, m * sgn(cos_ * qx - sin_ * qy));
}

BBox SolidAngleField::bounds() const {
  return boxMinMax(c_ - Vector3::constant(ra_), c_ + Vector3::constant(ra_));
}

} // namespace dualc
