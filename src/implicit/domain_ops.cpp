#include "dualc/implicit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace dualc {

namespace {

// Central-difference gradient of a field's own valueAt -- the fallback used
// by the distorted (twist/bend/displace) operators, where the warp has no
// cheap closed-form Jacobian.
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

double sgn(double x) { return x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0); }

BBox boxMinMax(const Vector3& lo, const Vector3& hi) {
  BBox b;
  b.min = lo;
  b.max = hi;
  return b;
}

// ---------------------------------------------------------------------------
// Conservative cell mapping for cellOverlaps
// ---------------------------------------------------------------------------
//
// A domain operator evaluates its child at warp(p). To answer "does the surface
// pass through this cell", it must therefore ask the child about a region that
// CONTAINS warp(cell). Any superset is a valid answer -- over-refining costs
// time, under-refining silently drops geometry -- so each operator below maps
// the cell to a conservative bound and forwards.
//
// Forwarding matters beyond tightness: a child that is not Lipschitz-1 (every
// TPMS, and anything wrapping one) announces itself by overriding cellOverlaps
// to always report an overlap. The base-class default cannot see that, so an
// operator that does not forward loses the signal and prunes cells the surface
// really does pass through.

// AABB of the image of the rectangle [aLo,aHi] x [bLo,bHi] under rotation by
// any angle in [angLo, angHi], in the (a, b) plane about the origin.
//
// The two extreme angles bound the endpoints of each corner's arc; the arc
// itself bulges outside the chord by at most r*(1 - cos(dAng/2)) for a corner
// at radius r, so expanding the endpoint hull by that sagitta contains the
// whole sweep. For a sweep of a quarter turn or more the bound degenerates, so
// fall back to the full disc, which is always valid.
void rotatedRangeBound(double aLo, double aHi, double bLo, double bHi,
                       double angLo, double angHi,
                       double& outALo, double& outAHi,
                       double& outBLo, double& outBHi) {
  const double ma = std::max(std::abs(aLo), std::abs(aHi));
  const double mb = std::max(std::abs(bLo), std::abs(bHi));
  const double rMax = std::hypot(ma, mb);

  if (angHi < angLo) std::swap(angLo, angHi);
  const double dAng = angHi - angLo;

  if (!(dAng < 1.5)) {  // >= ~86 degrees, or non-finite: use the full disc
    outALo = outBLo = -rMax;
    outAHi = outBHi =  rMax;
    return;
  }

  double lo0 =  std::numeric_limits<double>::infinity();
  double hi0 = -std::numeric_limits<double>::infinity();
  double lo1 =  std::numeric_limits<double>::infinity();
  double hi1 = -std::numeric_limits<double>::infinity();
  for (const double ang : {angLo, angHi}) {
    const double c = std::cos(ang), sn = std::sin(ang);
    for (const double a : {aLo, aHi}) {
      for (const double b : {bLo, bHi}) {
        const double ra = c * a - sn * b;
        const double rb = sn * a + c * b;
        lo0 = std::min(lo0, ra);  hi0 = std::max(hi0, ra);
        lo1 = std::min(lo1, rb);  hi1 = std::max(hi1, rb);
      }
    }
  }
  const double sagitta = rMax * (1.0 - std::cos(0.5 * dAng));
  outALo = lo0 - sagitta;  outAHi = hi0 + sagitta;
  outBLo = lo1 - sagitta;  outBHi = hi1 + sagitta;
}

// ---- Mirror -------------------------------------------------------------

class MirrorField : public ImplicitField {
public:
  MirrorField(FieldPtr child, const Vector3& planeNormal)
      : child_(std::move(child)), n_(planeNormal.unit()) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(fold(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    if (dot(p, n_) < 0.0) {
      // Reflected side: the world gradient is the child gradient mirrored.
      const Vector3 g = child_->gradientAt(fold(p));
      return g - n_ * (2.0 * dot(g, n_));
    }
    return child_->gradientAt(p);
  }
  BBox bounds() const override {
    const BBox cb = child_->bounds();
    if (!cb.isValid() || cb.isInfinite()) return cb;
    BBox r = cb;
    for (int c = 0; c < 8; ++c) {
      const Vector3 corner{(c & 1) ? cb.max.x : cb.min.x,
                           (c & 2) ? cb.max.y : cb.min.y,
                           (c & 4) ? cb.max.z : cb.min.z};
      const Vector3 m = corner - n_ * (2.0 * dot(corner, n_));
      r.min = componentwiseMin(r.min, m);
      r.max = componentwiseMax(r.max, m);
    }
    return r;
  }

  // fold() is the identity on the positive side of the plane and a reflection
  // on the negative side, so classify the cell first: only a cell that
  // straddles the plane needs the hull of both images. Taking the hull
  // unconditionally would be valid but wildly loose -- for a cell far from the
  // plane it spans the whole gap to its mirror image, and the child would
  // report an overlap almost everywhere.
  bool cellOverlaps(const BBox& cell) const override {
    // Signed distance range of the cell to the plane through the origin.
    double dLo = 0.0, dHi = 0.0;
    for (int a = 0; a < 3; ++a) {
      const double lo = n_[a] * cell.min[a], hi = n_[a] * cell.max[a];
      dLo += std::min(lo, hi);
      dHi += std::max(lo, hi);
    }
    if (dLo >= 0.0) return child_->cellOverlaps(cell);   // identity

    BBox r;
    bool first = true;
    for (int c = 0; c < 8; ++c) {
      const Vector3 corner{(c & 1) ? cell.max.x : cell.min.x,
                           (c & 2) ? cell.max.y : cell.min.y,
                           (c & 4) ? cell.max.z : cell.min.z};
      const Vector3 m = corner - n_ * (2.0 * dot(corner, n_));
      if (first) { r.min = r.max = m; first = false; }
      else {
        r.min = componentwiseMin(r.min, m);
        r.max = componentwiseMax(r.max, m);
      }
    }
    if (dHi <= 0.0) return child_->cellOverlaps(r);      // pure reflection

    // Straddles: the image is contained in the hull of both.
    r.min = componentwiseMin(r.min, cell.min);
    r.max = componentwiseMax(r.max, cell.max);
    return child_->cellOverlaps(r);
  }

private:
  // Reflect points on the negative-normal side onto the positive side.
  Vector3 fold(const Vector3& p) const {
    return p - n_ * (2.0 * std::min(dot(p, n_), 0.0));
  }

  FieldPtr child_;
  Vector3  n_;
};

// ---- Infinite repetition ------------------------------------------------

class RepeatField : public ImplicitField {
public:
  RepeatField(FieldPtr child, const Vector3& period)
      : child_(std::move(child)), s_(period),
        foldExact_(childFitsOnePeriod(*child_, period)) {}

  double valueAt(const Vector3& p) const override {
    if (foldExact_) return child_->valueAt(tileLocal(p));
    Vector3 ignored;
    return nearestCopy(p, ignored, /*wantGrad=*/false);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    if (foldExact_) return child_->gradientAt(tileLocal(p));
    Vector3 local;
    nearestCopy(p, local, /*wantGrad=*/true);
    return child_->gradientAt(local);
  }
  BBox bounds() const override {
    if (s_.x > 0.0 || s_.y > 0.0 || s_.z > 0.0) return BBox::infinite();
    return child_->bounds();
  }

  // Must cover every point valueAt() can read the child at -- the invariant
  // tests/test_domain_ops.cpp pins ("Domain wrappers ask their child about a
  // superset of what they read"). The two evaluation paths read different
  // sets, so this asks about different things depending on foldExact_.
  bool cellOverlaps(const BBox& cell) const override {
    // Fold path: v -> v - s*round(v/s) lands in [-s/2, s/2]. If the cell spans
    // a tile boundary, or is wider than one period, its image is the whole
    // period; otherwise it is the cell translated by that tile's offset.
    if (foldExact_) {
      Vector3 lo = cell.min, hi = cell.max;
      for (int a = 0; a < 3; ++a) {
        const double s = s_[a];
        if (!(s > 0.0)) continue;
        const double kLo = std::round(cell.min[a] / s);
        const double kHi = std::round(cell.max[a] / s);
        if (kLo == kHi && (cell.max[a] - cell.min[a]) < s) {
          lo[a] = cell.min[a] - s * kLo;
          hi[a] = cell.max[a] - s * kLo;
        } else {
          lo[a] = -0.5 * s;
          hi[a] =  0.5 * s;
        }
      }
      return child_->cellOverlaps(boxMinMax(lo, hi));
    }

    // Neighbour path: nearestCopy() reads the child at p - s*rid for rid drawn
    // from round(p/s) and its +/-1 neighbour, so over a cell the ids span
    // [round(min/s) - 1, round(max/s) + 1] per axis. Ask about each id's box
    // separately rather than one box spanning them all -- the same reasoning,
    // and the same shape, as RepeatLimitedField::cellOverlaps below.
    double idLo[3], idHi[3];
    double span = 1.0;
    for (int a = 0; a < 3; ++a) {
      const double s = s_[a];
      if (!(s > 0.0)) { idLo[a] = idHi[a] = 0.0; continue; }
      idLo[a] = std::round(cell.min[a] / s) - 1.0;
      idHi[a] = std::round(cell.max[a] / s) + 1.0;
      span *= (idHi[a] - idLo[a] + 1.0);
    }

    if (span > 64.0) {  // a very shallow cell: one spanning box, valid but loose
      Vector3 lo = cell.min, hi = cell.max;
      for (int a = 0; a < 3; ++a) {
        if (!(s_[a] > 0.0)) continue;
        lo[a] = cell.min[a] - s_[a] * idHi[a];
        hi[a] = cell.max[a] - s_[a] * idLo[a];
      }
      return child_->cellOverlaps(boxMinMax(lo, hi));
    }

    for (double iz = idLo[2]; iz <= idHi[2]; iz += 1.0)
      for (double iy = idLo[1]; iy <= idHi[1]; iy += 1.0)
        for (double ix = idLo[0]; ix <= idHi[0]; ix += 1.0) {
          const Vector3 off{s_.x * ix, s_.y * iy, s_.z * iz};
          if (child_->cellOverlaps(boxMinMax(cell.min - off, cell.max - off)))
            return true;
        }
    return false;
  }

private:
  // Map p into the local frame of the tile it falls in.
  Vector3 tileLocal(const Vector3& p) const {
    auto fold1 = [](double v, double s) {
      return (s > 0.0) ? (v - s * std::round(v / s)) : v;
    };
    return Vector3{fold1(p.x, s_.x), fold1(p.y, s_.y), fold1(p.z, s_.z)};
  }

  // True when no copy of the child can reach across a tile boundary, i.e. the
  // child's bounds fit inside one period box centred on the origin on every
  // repeated axis. Only then is reading the single folded tile the same answer
  // as the true infinite union; see nearestCopy().
  static bool childFitsOnePeriod(const ImplicitField& child,
                                 const Vector3& s) {
    const BBox b = child.bounds();
    if (!b.isValid() || b.isInfinite()) return false;  // cannot prove it
    for (int a = 0; a < 3; ++a) {
      if (!(s[a] > 0.0)) continue;                     // axis not repeated
      const double half = 0.5 * s[a];
      if (b.min[a] < -half || b.max[a] > half) return false;
    }
    return true;
  }

  // Quilez's repetition done honestly: the home tile plus the 7 nearest
  // neighbours, keeping the closest copy -- the same 8-neighbour minimum
  // RepeatLimitedField::evaluate uses, minus the id clamping (there is no
  // count to clamp against here).
  //
  // The single fold this replaces reads only the tile p falls in, which is
  // wrong whenever a copy reaches across a boundary: a point inside the home
  // copy but past the seam folds to the NEXT tile and is reported outside. A
  // flipped sign flips a corner sign, which decides whether an octree edge
  // carries Hermite data -- changed topology, not a nudged vertex. Guarded by
  // foldExact_ so a child that provably fits in one period keeps paying for
  // exactly one child evaluation.
  double nearestCopy(const Vector3& p, Vector3& bestLocal,
                     bool wantGrad) const {
    auto idOf = [](double v, double s) {
      return (s > 0.0) ? std::round(v / s) : 0.0;
    };
    const Vector3 id{idOf(p.x, s_.x), idOf(p.y, s_.y), idOf(p.z, s_.z)};
    const Vector3 o{sgn(p.x - s_.x * id.x), sgn(p.y - s_.y * id.y),
                    sgn(p.z - s_.z * id.z)};

    double best = std::numeric_limits<double>::infinity();
    for (int k = 0; k < 2; ++k) {
      for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
          const Vector3 rid{id.x + i * o.x, id.y + j * o.y, id.z + k * o.z};
          const Vector3 r{p.x - s_.x * rid.x, p.y - s_.y * rid.y,
                          p.z - s_.z * rid.z};
          const double d = child_->valueAt(r);
          if (d < best) {
            best = d;
            if (wantGrad) bestLocal = r;
          }
        }
      }
    }
    return best;
  }

  FieldPtr child_;
  Vector3  s_;
  bool     foldExact_;
};

// ---- Finite repetition --------------------------------------------------

class RepeatLimitedField : public ImplicitField {
public:
  RepeatLimitedField(FieldPtr child, const Vector3& period,
                     const Vector3i& count)
      : child_(std::move(child)), s_(period), n_(count) {}

  double valueAt(const Vector3& p) const override {
    Vector3 ignored;
    return evaluate(p, ignored, /*wantGrad=*/false);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    Vector3 local;
    evaluate(p, local, /*wantGrad=*/true);
    return child_->gradientAt(local);
  }
  BBox bounds() const override {
    const BBox cb = child_->bounds();
    if (!cb.isValid() || cb.isInfinite()) return cb;
    auto span = [](double s, int n) {
      const double far = s * static_cast<double>(std::max(n - 1, 0));
      return std::make_pair(std::min(0.0, far), std::max(0.0, far));
    };
    const auto sx = span(s_.x, n_.x);
    const auto sy = span(s_.y, n_.y);
    const auto sz = span(s_.z, n_.z);
    return boxMinMax(
        cb.min + Vector3{sx.first, sy.first, sz.first},
        cb.max + Vector3{sx.second, sy.second, sz.second});
  }

  // evaluate() reads the child at p - s*rid for tile ids rid drawn from the
  // clamped 8-neighbourhood of round(p/s). Over a cell those ids span, per
  // axis, [clampId(round(min/s) - 1), clampId(round(max/s) + 1)] -- the -1/+1
  // because the neighbour offset o is +/-1.
  //
  // Ask the child about the cell translated by each candidate id separately
  // rather than about one box spanning them all: the union of the per-id boxes
  // is the set the child is actually read on, whereas their bounding box is
  // roughly two periods wider than the cell and makes the child report an
  // overlap almost everywhere. The id range is normally one or two per axis
  // (a cell at any useful depth is smaller than a tile), so this is a handful
  // of calls; only the shallowest cells span more, and those refine anyway.
  bool cellOverlaps(const BBox& cell) const override {
    double idLo[3], idHi[3];
    double span = 1.0;
    for (int a = 0; a < 3; ++a) {
      const double s = s_[a];
      if (!(s > 0.0)) { idLo[a] = idHi[a] = 0.0; continue; }
      const int count = (a == 0) ? n_.x : (a == 1) ? n_.y : n_.z;
      idLo[a] = clampId(std::round(cell.min[a] / s) - 1.0, count);
      idHi[a] = clampId(std::round(cell.max[a] / s) + 1.0, count);
      span *= (idHi[a] - idLo[a] + 1.0);
    }

    // Too many tiles to enumerate (a very shallow cell): fall back to the
    // single spanning box, which is valid, just loose.
    if (span > 64.0) {
      Vector3 lo = cell.min, hi = cell.max;
      for (int a = 0; a < 3; ++a) {
        if (!(s_[a] > 0.0)) continue;
        lo[a] = cell.min[a] - s_[a] * idHi[a];
        hi[a] = cell.max[a] - s_[a] * idLo[a];
      }
      return child_->cellOverlaps(boxMinMax(lo, hi));
    }

    for (double iz = idLo[2]; iz <= idHi[2]; iz += 1.0) {
      for (double iy = idLo[1]; iy <= idHi[1]; iy += 1.0) {
        for (double ix = idLo[0]; ix <= idHi[0]; ix += 1.0) {
          const Vector3 off{s_.x * ix, s_.y * iy, s_.z * iz};
          if (child_->cellOverlaps(boxMinMax(cell.min - off, cell.max - off))) {
            return true;
          }
        }
      }
    }
    return false;
  }

private:
  // Quilez's limited repetition: test the home tile and the 7 nearest
  // neighbours, clamping each id into [0, count-1]; keep the closest copy.
  double evaluate(const Vector3& p, Vector3& bestLocal, bool wantGrad) const {
    auto idOf = [](double v, double s) {
      return (s > 0.0) ? std::round(v / s) : 0.0;
    };
    const Vector3 id{idOf(p.x, s_.x), idOf(p.y, s_.y), idOf(p.z, s_.z)};
    const Vector3 o{sgn(p.x - s_.x * id.x), sgn(p.y - s_.y * id.y),
                    sgn(p.z - s_.z * id.z)};

    double best = std::numeric_limits<double>::infinity();
    for (int k = 0; k < 2; ++k) {
      for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
          Vector3 rid{id.x + i * o.x, id.y + j * o.y, id.z + k * o.z};
          rid.x = clampId(rid.x, n_.x);
          rid.y = clampId(rid.y, n_.y);
          rid.z = clampId(rid.z, n_.z);
          const Vector3 r{p.x - s_.x * rid.x, p.y - s_.y * rid.y,
                          p.z - s_.z * rid.z};
          const double d = child_->valueAt(r);
          if (d < best) {
            best = d;
            if (wantGrad) bestLocal = r;
          }
        }
      }
    }
    return best;
  }

  static double clampId(double v, int count) {
    const double hi = static_cast<double>(std::max(count - 1, 0));
    return v < 0.0 ? 0.0 : (v > hi ? hi : v);
  }

  FieldPtr  child_;
  Vector3   s_;
  Vector3i  n_;
};

// ---- Twist --------------------------------------------------------------

class TwistField : public ImplicitField {
public:
  TwistField(FieldPtr child, double k, int axis)
      : child_(std::move(child)), k_(k), axis_(axis % 3) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(warp(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    return fdGradient(*this, p);
  }
  BBox bounds() const override {
    const BBox cb = child_->bounds();
    if (!cb.isValid() || cb.isInfinite()) return cb;
    // The twist rotates the plane perpendicular to the axis: distance from
    // the axis is preserved, so the perpendicular extent is a disc of the
    // child's max perpendicular radius.
    const int u = (axis_ + 1) % 3, v = (axis_ + 2) % 3;
    const double mu = std::max(std::abs(cb.min[u]), std::abs(cb.max[u]));
    const double mv = std::max(std::abs(cb.min[v]), std::abs(cb.max[v]));
    const double rr = std::hypot(mu, mv);
    Vector3 lo, hi;
    lo[axis_] = cb.min[axis_];  hi[axis_] = cb.max[axis_];
    lo[u] = -rr;  hi[u] = rr;
    lo[v] = -rr;  hi[v] = rr;
    return boxMinMax(lo, hi);
  }

  // The twist preserves the axis coordinate and rotates the (u, v) plane by
  // k*p[axis], so over a cell the rotation angle is confined to
  // k * [axis range] and the image is bounded by rotatedRangeBound.
  bool cellOverlaps(const BBox& cell) const override {
    const int u = (axis_ + 1) % 3, v = (axis_ + 2) % 3;
    double uLo, uHi, vLo, vHi;
    rotatedRangeBound(cell.min[u], cell.max[u], cell.min[v], cell.max[v],
                      k_ * cell.min[axis_], k_ * cell.max[axis_],
                      uLo, uHi, vLo, vHi);
    Vector3 lo, hi;
    lo[axis_] = cell.min[axis_];  hi[axis_] = cell.max[axis_];
    lo[u] = uLo;  hi[u] = uHi;
    lo[v] = vLo;  hi[v] = vHi;
    return child_->cellOverlaps(boxMinMax(lo, hi));
  }

private:
  Vector3 warp(const Vector3& p) const {
    const int u = (axis_ + 1) % 3, v = (axis_ + 2) % 3;
    const double angle = k_ * p[axis_];
    const double c = std::cos(angle), s = std::sin(angle);
    Vector3 q;
    q[axis_] = p[axis_];
    q[u] = c * p[u] - s * p[v];
    q[v] = s * p[u] + c * p[v];
    return q;
  }

  FieldPtr child_;
  double   k_;
  int      axis_;
};

// ---- Bend ---------------------------------------------------------------

class BendField : public ImplicitField {
public:
  BendField(FieldPtr child, double k, int axis)
      : child_(std::move(child)), k_(k), axis_(axis % 3) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(warp(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    return fdGradient(*this, p);
  }
  BBox bounds() const override {
    const BBox cb = child_->bounds();
    if (!cb.isValid() || cb.isInfinite()) return cb;
    // The bend rotates the (axis, next) plane about the origin: distance from
    // the origin within that plane is preserved.
    const int u = (axis_ + 1) % 3, w = (axis_ + 2) % 3;
    const double ma = std::max(std::abs(cb.min[axis_]), std::abs(cb.max[axis_]));
    const double mu = std::max(std::abs(cb.min[u]), std::abs(cb.max[u]));
    const double rr = std::hypot(ma, mu);
    Vector3 lo, hi;
    lo[axis_] = -rr;  hi[axis_] = rr;
    lo[u] = -rr;  hi[u] = rr;
    lo[w] = cb.min[w];  hi[w] = cb.max[w];
    return boxMinMax(lo, hi);
  }

  // The bend rotates the (axis, u) plane by k*p[axis] and leaves w alone, so
  // the same arc bound applies -- here to the (axis, u) rectangle.
  bool cellOverlaps(const BBox& cell) const override {
    const int u = (axis_ + 1) % 3, w = (axis_ + 2) % 3;
    double aLo, aHi, uLo, uHi;
    rotatedRangeBound(cell.min[axis_], cell.max[axis_], cell.min[u], cell.max[u],
                      k_ * cell.min[axis_], k_ * cell.max[axis_],
                      aLo, aHi, uLo, uHi);
    Vector3 lo, hi;
    lo[axis_] = aLo;  hi[axis_] = aHi;
    lo[u] = uLo;  hi[u] = uHi;
    lo[w] = cell.min[w];  hi[w] = cell.max[w];
    return child_->cellOverlaps(boxMinMax(lo, hi));
  }

private:
  Vector3 warp(const Vector3& p) const {
    const int u = (axis_ + 1) % 3, w = (axis_ + 2) % 3;
    const double angle = k_ * p[axis_];
    const double c = std::cos(angle), s = std::sin(angle);
    Vector3 q;
    q[axis_] = c * p[axis_] - s * p[u];
    q[u] = s * p[axis_] + c * p[u];
    q[w] = p[w];
    return q;
  }

  FieldPtr child_;
  double   k_;
  int      axis_;
};

// ---- Displacement -------------------------------------------------------

class DisplaceField : public ImplicitField {
public:
  DisplaceField(FieldPtr child, std::function<double(const Vector3&)> bump)
      : child_(std::move(child)), bump_(std::move(bump)) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(p) + bump_(p);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    return fdGradient(*this, p);
  }
  BBox bounds() const override { return child_->bounds(); }
  // Displacement adds to the VALUE, it does not warp the domain, so the child
  // is asked about the cell unchanged. This forwards a non-Lipschitz child's
  // always-overlap signal, which is the point. It is NOT a bound on where the
  // displaced surface can be: the bump is an opaque callable with no known
  // magnitude, so a bump large enough to push the surface into a cell the
  // child does not reach can still be missed -- the same caveat bounds()
  // already carries.
  bool cellOverlaps(const BBox& cell) const override {
    return child_->cellOverlaps(cell) || ImplicitField::cellOverlaps(cell);
  }

private:
  FieldPtr child_;
  std::function<double(const Vector3&)> bump_;
};

} // namespace

FieldPtr mirrored(FieldPtr f, const Vector3& planeNormal) {
  return std::make_shared<MirrorField>(std::move(f), planeNormal);
}

namespace {

// A period component of 0 means "do not repeat this axis" and is the documented
// way to tile along one or two axes only. A NEGATIVE one is silently treated
// the same by every `s > 0.0` test below, which reads as a typo that quietly
// does nothing rather than as a request.
void requireNonNegativePeriod(const char* what, const Vector3& s) {
  for (int a = 0; a < 3; ++a) {
    if (!(s[a] >= 0.0))
      throw std::invalid_argument(
          std::string(what) + ": period components must be >= 0 (got " +
          std::to_string(s.x) + ", " + std::to_string(s.y) + ", " +
          std::to_string(s.z) + "); use 0 to leave an axis un-repeated");
  }
}

}  // namespace

FieldPtr repeated(FieldPtr f, const Vector3& period) {
  requireNonNegativePeriod("repeated()", period);
  return std::make_shared<RepeatField>(std::move(f), period);
}

FieldPtr repeatedLimited(FieldPtr f, const Vector3& period,
                         const Vector3i& count) {
  requireNonNegativePeriod("repeatedLimited()", period);
  // clampId() folds a count of 0 (or less) to the single id 0, so asking for
  // zero copies silently produced exactly one.
  if (count.x < 1 || count.y < 1 || count.z < 1)
    throw std::invalid_argument(
        "repeatedLimited(): every count must be >= 1 (got " +
        std::to_string(count.x) + ", " + std::to_string(count.y) + ", " +
        std::to_string(count.z) + ")");
  return std::make_shared<RepeatLimitedField>(std::move(f), period, count);
}

FieldPtr twisted(FieldPtr f, double radiansPerUnit, int axis) {
  return std::make_shared<TwistField>(std::move(f), radiansPerUnit, axis);
}

FieldPtr bent(FieldPtr f, double curvature, int axis) {
  return std::make_shared<BendField>(std::move(f), curvature, axis);
}

FieldPtr displaced(FieldPtr f, std::function<double(const Vector3&)> bump) {
  return std::make_shared<DisplaceField>(std::move(f), std::move(bump));
}

} // namespace dualc
