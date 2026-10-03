#include "dualc/implicit.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace dualc {

namespace {

// Expand a valid box outward by `r` on every side.
BBox expandBBox(BBox b, double r) {
  if (!b.isValid()) return b;
  const Vector3 e{r, r, r};
  b.min = b.min - e;
  b.max = b.max + e;
  return b;
}

// Axis-aligned hull of an arbitrary point set, built incrementally.
struct BoxAccum {
  BBox box;
  bool first = true;
  void add(const Vector3& p) {
    if (first) { box.min = box.max = p; first = false; return; }
    box.min = Vector3{std::min(box.min.x, p.x), std::min(box.min.y, p.y),
                      std::min(box.min.z, p.z)};
    box.max = Vector3{std::max(box.max.x, p.x), std::max(box.max.y, p.y),
                      std::max(box.max.z, p.z)};
  }
};

// The 8 corners of a box, in the usual (x, y, z) bit order.
Vector3 boxCorner(const BBox& b, int c) {
  return Vector3{(c & 1) ? b.max.x : b.min.x,
                 (c & 2) ? b.max.y : b.min.y,
                 (c & 4) ? b.max.z : b.min.z};
}

// ---- Offset -------------------------------------------------------------

class OffsetField : public ImplicitField {
public:
  OffsetField(FieldPtr child, double r) : child_(std::move(child)), r_(r) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(p) - r_;
  }
  Vector3 gradientAt(const Vector3& p) const override {
    return child_->gradientAt(p);  // a level-set shift leaves the gradient
  }
  BBox bounds() const override {
    return expandBBox(child_->bounds(), std::max(r_, 0.0));
  }
  // The 0-isosurface of f-r is the level set f = r, within metric distance
  // |r| of the child's surface (for a Lipschitz-1 child; for a non-Lipschitz
  // one the child's own cellOverlaps is what carries the always-overlap
  // signal, and it has to be asked). |r|, not the max(r, 0) `bounds()` uses:
  // an inward shift moves the surface inside the child, where a cell holding
  // it may not touch f = 0 at all, and growing by zero would prune it.
  bool cellOverlaps(const BBox& cell) const override {
    return child_->cellOverlaps(expandBBox(cell, std::abs(r_)));
  }

private:
  FieldPtr child_;
  double   r_;
};

// ---- Onion (hollow shell) -----------------------------------------------

class OnionField : public ImplicitField {
public:
  OnionField(FieldPtr child, double thickness)
      : child_(std::move(child)), t_(thickness) {}

  double valueAt(const Vector3& p) const override {
    return std::abs(child_->valueAt(p)) - t_;
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // d/dp [ |f| - t ] = sign(f) * grad f.
    const double v = child_->valueAt(p);
    const Vector3 g = child_->gradientAt(p);
    return (v >= 0.0) ? g : (g * -1.0);
  }
  BBox bounds() const override {
    return expandBBox(child_->bounds(), std::max(t_, 0.0));
  }
  // The 0-isosurface of |f|-t is the pair of level sets f = +t and f = -t,
  // each within metric distance ~t of the child's surface (for a Lipschitz-1
  // child; the lower bound the child's own cellOverlaps already encodes for
  // non-SDF children). Asking the child whether it overlaps the cell grown
  // by t is the tight never-misses test, and it propagates non-Lipschitz
  // refinement decisions (e.g. TPMS' always-overlap) correctly through the
  // onion -- the default Lipschitz-1 conservative on |f|-t would otherwise
  // under-refine on high-frequency children.
  bool cellOverlaps(const BBox& cell) const override {
    return child_->cellOverlaps(expandBBox(cell, std::max(t_, 0.0)));
  }

private:
  FieldPtr child_;
  double   t_;
};

// ---- Graded onion (control-field-driven wall thickness) -----------------

// |base| - t(p), where t(p) ramps from t1 (where control <= d0) to t2 (where
// control >= d1) with a clamped-linear falloff in between. `control` is an
// arbitrary field: a sphere of radius 0 gives distance-from-a-point, a plane
// gives a linear gradient, a mesh gives distance-from-a-surface. Like
// OnionField the gradient drops the slowly-varying d/dp t(p) term -- negligible
// against the (high-magnitude) base gradient, and exact for the uniform
// t1 == t2 case -- so this stays a closed-form node. valueAt carries the full
// t(p), so the contoured surface positions are exact.
class GradedOnionField : public ImplicitField {
public:
  GradedOnionField(FieldPtr base, FieldPtr control, double t1, double t2,
                   double d0, double d1)
      : base_(std::move(base)), control_(std::move(control)), t1_(t1), t2_(t2),
        d0_(d0), d1_(d1) {}

  double valueAt(const Vector3& p) const override {
    return std::abs(base_->valueAt(p)) - thicknessAt(p);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // d/dp [ |base| - t(p) ] ~= sign(base) * grad base  (the -grad t term is
    // dropped, as in OnionField; see the class comment).
    const double v = base_->valueAt(p);
    const Vector3 g = base_->gradientAt(p);
    return (v >= 0.0) ? g : (g * -1.0);
  }
  BBox bounds() const override {
    return expandBBox(base_->bounds(), maxThickness());
  }
  // The widest wall sets the never-misses growth; delegate to base so a
  // non-Lipschitz child (TPMS' always-overlap) still refines correctly.
  bool cellOverlaps(const BBox& cell) const override {
    return base_->cellOverlaps(expandBBox(cell, maxThickness()));
  }

private:
  double maxThickness() const {
    return std::max(std::max(t1_, t2_), 0.0);
  }
  double thicknessAt(const Vector3& p) const {
    double denom = d1_ - d0_;
    if (std::abs(denom) < 1e-9) denom = (denom < 0.0) ? -1e-9 : 1e-9;
    double u = (control_->valueAt(p) - d0_) / denom;
    u = std::min(1.0, std::max(0.0, u));
    return t1_ + (t2_ - t1_) * u;
  }

  FieldPtr base_, control_;
  double   t1_, t2_, d0_, d1_;
};

// ---- Graded offset (control-field-driven outward inflation) --------------

// base - t(p): grows the SOLID outward by t(p) (inflation), where t(p) ramps
// from t1 (control <= d0) to t2 (control >= d1) with the same clamped-linear
// falloff GradedOnionField uses. Unlike graded-onion (|base| - t, a hollow
// shell), this thickens a solid locally -- e.g. a strut lattice denser near a
// load path (control = distance from it) and sparser elsewhere. Positive t
// inflates; the -grad t term is dropped exactly as in the onion, so this stays
// a closed-form node whose valueAt carries the full t(p) (exact surface).
class GradedOffsetField : public ImplicitField {
public:
  GradedOffsetField(FieldPtr base, FieldPtr control, double t1, double t2,
                    double d0, double d1)
      : base_(std::move(base)), control_(std::move(control)), t1_(t1), t2_(t2),
        d0_(d0), d1_(d1) {}

  double valueAt(const Vector3& p) const override {
    return base_->valueAt(p) - thicknessAt(p);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // grad[ base - t(p) ] ~= sign(base) * grad base (the -grad t term dropped,
    // as in OnionField). At the surface base = t > 0, so the sign is + and this
    // returns +grad base -- exactly grad(base - t) there.
    const double v = base_->valueAt(p);
    const Vector3 g = base_->gradientAt(p);
    return (v >= 0.0) ? g : (g * -1.0);
  }
  BBox bounds() const override {
    return expandBBox(base_->bounds(), maxThickness());
  }
  // Outward growth by the widest t; delegate to base so a non-Lipschitz child
  // (e.g. a strut/TPMS always-overlap) still refines correctly.
  bool cellOverlaps(const BBox& cell) const override {
    return base_->cellOverlaps(expandBBox(cell, maxThickness()));
  }

private:
  double maxThickness() const {
    return std::max(std::max(t1_, t2_), 0.0);
  }
  double thicknessAt(const Vector3& p) const {
    double denom = d1_ - d0_;
    if (std::abs(denom) < 1e-9) denom = (denom < 0.0) ? -1e-9 : 1e-9;
    double u = (control_->valueAt(p) - d0_) / denom;
    u = std::min(1.0, std::max(0.0, u));
    return t1_ + (t2_ - t1_) * u;
  }

  FieldPtr base_, control_;
  double   t1_, t2_, d0_, d1_;
};

// ---- Elongation ---------------------------------------------------------

class ElongateField : public ImplicitField {
public:
  ElongateField(FieldPtr child, const Vector3& h)
      : child_(std::move(child)), h_(h) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(warp(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // The warp pins each axis component while |p_i| < h_i (a translational
    // sweep), so the world gradient has a zero component there.
    Vector3 g = child_->gradientAt(warp(p));
    if (std::abs(p.x) < h_.x) g.x = 0.0;
    if (std::abs(p.y) < h_.y) g.y = 0.0;
    if (std::abs(p.z) < h_.z) g.z = 0.0;
    if (g.norm() < 1e-12) return child_->gradientAt(warp(p));
    return g;
  }
  BBox bounds() const override {
    BBox b = child_->bounds();
    if (!b.isValid()) return b;
    b.min = b.min - h_;
    b.max = b.max + h_;
    return b;
  }

  // Per axis the warp is v - clamp(v, -h, h): monotone non-decreasing, so the
  // image of the cell's extent is exactly the extent of the images. Forwarding
  // rather than leaving the base test in place is what keeps a non-Lipschitz
  // child's always-overlap signal alive (see ImplicitField::cellOverlaps).
  bool cellOverlaps(const BBox& cell) const override {
    BBox r;
    r.min = warp(cell.min);
    r.max = warp(cell.max);
    return child_->cellOverlaps(r);
  }

private:
  Vector3 warp(const Vector3& p) const {
    return Vector3{
        p.x - std::clamp(p.x, -h_.x, h_.x),
        p.y - std::clamp(p.y, -h_.y, h_.y),
        p.z - std::clamp(p.z, -h_.z, h_.z)};
  }

  FieldPtr child_;
  Vector3  h_;
};

// ---- Rigid transform ----------------------------------------------------

class TransformField : public ImplicitField {
public:
  TransformField(FieldPtr child, const Mat4& worldFromLocal)
      : child_(std::move(child)),
        worldFromLocal_(worldFromLocal),
        localFromWorld_(worldFromLocal.inverseRigid()) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(localFromWorld_.transformPoint(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    const Vector3 gLocal =
        child_->gradientAt(localFromWorld_.transformPoint(p));
    return worldFromLocal_.transformDirection(gLocal);
  }
  bool isInside(const Vector3& p) const override {
    return child_->isInside(localFromWorld_.transformPoint(p));
  }
  BBox bounds() const override {
    const BBox cb = child_->bounds();
    if (!cb.isValid()) return cb;
    BBox r;
    bool first = true;
    for (int c = 0; c < 8; ++c) {
      const Vector3 corner{
          (c & 1) ? cb.max.x : cb.min.x,
          (c & 2) ? cb.max.y : cb.min.y,
          (c & 4) ? cb.max.z : cb.min.z};
      const Vector3 w = worldFromLocal_.transformPoint(corner);
      if (first) {
        r.min = r.max = w;
        first = false;
      } else {
        r.min = Vector3{std::min(r.min.x, w.x), std::min(r.min.y, w.y),
                        std::min(r.min.z, w.z)};
        r.max = Vector3{std::max(r.max.x, w.x), std::max(r.max.y, w.y),
                        std::max(r.max.z, w.z)};
      }
    }
    return r;
  }

  // The child is read at localFromWorld_ * p, so ask it about the AABB of the
  // transformed cell corners -- deliberately the same `localFromWorld_` that
  // `valueAt` uses, not a general inverse: the predicate must bound where the
  // evaluator actually reads. (The ctor's `inverseRigid()` is only correct for
  // a rigid matrix; a non-rigid one is already wrong in `valueAt`, and this
  // stays wrong in exactly the same way rather than disagreeing with it.)
  bool cellOverlaps(const BBox& cell) const override {
    BoxAccum acc;
    for (int c = 0; c < 8; ++c) {
      acc.add(localFromWorld_.transformPoint(boxCorner(cell, c)));
    }
    return child_->cellOverlaps(acc.box);
  }

private:
  FieldPtr child_;
  Mat4     worldFromLocal_;
  Mat4     localFromWorld_;
};

// ---- Gradient-normalised wrapper ----------------------------------------
//
// First-order SDF normalisation: returns f / max(|grad f|, eps). For a TPMS
// (whose value is trigonometric, not metric distance) the surface is in
// the same place but the level-set spacing becomes ~uniform in metric units
// near the surface, so onionOf(normalizedOf(tpms), t) produces a wall whose
// thickness is approximately t millimetres rather than t * a position-
// dependent factor. Not a true SDF (only first-order at the surface) but
// dramatically more uniform than the raw trig.

class NormalizedField : public ImplicitField {
public:
  explicit NormalizedField(FieldPtr child) : child_(std::move(child)) {}

  double valueAt(const Vector3& p) const override {
    return child_->valueAt(p) / std::max(trueGradMag(p), kEps);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // First-order: away from where grad f goes to zero, the normalised
    // field is approximately a true SDF, so its own gradient is grad f /
    // |grad f| -- a unit direction. The child's gradientAt() already returns
    // that direction (the primitives normalise it); re-normalise defensively.
    const Vector3 g = child_->gradientAt(p);
    const double n = g.norm();
    return (n > kEps) ? g * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
  }
  // The normalised field's 0-isosurface is exactly the child's, so a cell
  // overlaps iff the child says so. Forwarding this is essential: the base
  // Lipschitz-1 default under-refines high-frequency children (TPMS report
  // always-overlap), which would silently drop surface cells here.
  bool cellOverlaps(const BBox& cell) const override {
    return child_->cellOverlaps(cell);
  }
  BBox bounds() const override { return child_->bounds(); }

private:
  // True |grad f| via central differences on the child's *value*. The child's
  // gradientAt() returns a unit direction (correct for SDF primitives and for
  // contour normals) and so cannot supply the magnitude this normalisation
  // needs -- dividing by it would be a no-op. Step matches PrimitiveField's
  // central difference.
  double trueGradMag(const Vector3& p) const {
    constexpr double e = 1e-4;
    const double dx = child_->valueAt(Vector3{p.x + e, p.y, p.z}) -
                      child_->valueAt(Vector3{p.x - e, p.y, p.z});
    const double dy = child_->valueAt(Vector3{p.x, p.y + e, p.z}) -
                      child_->valueAt(Vector3{p.x, p.y - e, p.z});
    const double dz = child_->valueAt(Vector3{p.x, p.y, p.z + e}) -
                      child_->valueAt(Vector3{p.x, p.y, p.z - e});
    return Vector3{dx, dy, dz}.norm() / (2.0 * e);
  }

  static constexpr double kEps = 1e-9;
  FieldPtr child_;
};

// ---- Uniform scale ------------------------------------------------------

class ScaleField : public ImplicitField {
public:
  ScaleField(FieldPtr child, double s) : child_(std::move(child)), s_(s) {
    // Every query below divides by s.
    if (s == 0.0)
      throw std::invalid_argument("scaled(): scale factor must not be 0");
  }

  double valueAt(const Vector3& p) const override {
    // Multiplying the value by s keeps the field's magnitude meaningful.
    // |s|, not s. A negative factor is a scale composed with a point
    // reflection through the origin -- a distance-preserving map, so the field
    // stays a proper SDF -- but multiplying the child's value by a negative s
    // NEGATES it, which swaps inside for outside and returns the complement of
    // the solid the caller asked for. Taking the magnitude restores world
    // units without touching the sign.
    return std::abs(s_) * child_->valueAt(p / s_);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // f(p) = |s|*child(p/s), so grad f = sign(s) * grad child(p/s); the child
    // returns a unit direction, and a reflected domain flips it.
    const Vector3 g = child_->gradientAt(p / s_);
    return (s_ < 0.0) ? Vector3{-g.x, -g.y, -g.z} : g;
  }
  bool isInside(const Vector3& p) const override {
    return child_->isInside(p / s_);
  }
  BBox bounds() const override {
    BBox b = child_->bounds();
    if (!b.isValid()) return b;
    // A negative s maps min above max on every axis, which would report an
    // INVALID box and silently send the sampler to its BBox::unit() fallback.
    // Re-order per component instead.
    const Vector3 a = b.min * s_;
    const Vector3 c = b.max * s_;
    b.min = Vector3{std::min(a.x, c.x), std::min(a.y, c.y), std::min(a.z, c.z)};
    b.max = Vector3{std::max(a.x, c.x), std::max(a.y, c.y), std::max(a.z, c.z)};
    return b;
  }

  // The child is read at p / s_, so the image is the cell divided by s_ (ends
  // swapped for a negative scale). A zero scale is already a division by zero
  // in `valueAt`; refuse to prune rather than propagate it.
  bool cellOverlaps(const BBox& cell) const override {
    if (!(s_ != 0.0)) return true;
    BoxAccum acc;
    acc.add(cell.min / s_);
    acc.add(cell.max / s_);
    return child_->cellOverlaps(acc.box);
  }

private:
  FieldPtr child_;
  double   s_;
};

} // namespace

FieldPtr offsetOf(FieldPtr f, double r) {
  return std::make_shared<OffsetField>(std::move(f), r);
}

FieldPtr roundedOf(FieldPtr f, double r) {
  return offsetOf(std::move(f), r);  // rounding is an outward level-set shift
}

FieldPtr onionOf(FieldPtr f, double thickness) {
  return std::make_shared<OnionField>(std::move(f), thickness);
}

FieldPtr gradedOnionOf(FieldPtr base, FieldPtr control, double t1, double t2,
                       double d0, double d1) {
  return std::make_shared<GradedOnionField>(std::move(base), std::move(control),
                                            t1, t2, d0, d1);
}

FieldPtr gradedOffsetOf(FieldPtr base, FieldPtr control, double t1, double t2,
                        double d0, double d1) {
  return std::make_shared<GradedOffsetField>(std::move(base), std::move(control),
                                             t1, t2, d0, d1);
}

FieldPtr elongated(FieldPtr f, const Vector3& h) {
  return std::make_shared<ElongateField>(std::move(f), h);
}

FieldPtr transformed(FieldPtr f, const Mat4& worldFromLocal) {
  // TransformField inverts with Mat4::inverseRigid(), which is [R^T | -R^T t]
  // -- exact for a rotation + translation and WRONG for anything else. A
  // sheared or scaled matrix produced a silently misplaced field, so the
  // precondition the inverse assumes is checked here instead of documented and
  // hoped for. Test: R * R^T == I to a tolerance, and det(R) > 0 (a reflection
  // inverts correctly but flips inside/outside).
  const Mat4& m = worldFromLocal;
  double g[3][3];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      g[i][j] = 0.0;
      for (int k = 0; k < 3; ++k) g[i][j] += m.m[i][k] * m.m[j][k];
    }
  constexpr double kTol = 1e-9;
  bool orthonormal = true;
  for (int i = 0; i < 3 && orthonormal; ++i)
    for (int j = 0; j < 3; ++j)
      if (std::abs(g[i][j] - (i == j ? 1.0 : 0.0)) > kTol) {
        orthonormal = false;
        break;
      }
  const double det =
      m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) -
      m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) +
      m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
  if (!orthonormal || det <= 0.0)
    throw std::invalid_argument(
        "transformed(): the linear part must be a rotation (orthonormal, "
        "det = +1); scale, shear and reflection are not invertible by "
        "Mat4::inverseRigid(). Use scaled() for uniform scale.");
  return std::make_shared<TransformField>(std::move(f), worldFromLocal);
}

FieldPtr scaled(FieldPtr f, double s) {
  return std::make_shared<ScaleField>(std::move(f), s);
}

FieldPtr normalizedOf(FieldPtr f) {
  return std::make_shared<NormalizedField>(std::move(f));
}

} // namespace dualc
