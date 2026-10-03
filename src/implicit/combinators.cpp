#include "dualc/implicit.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace dualc {

namespace {

double clamp01(double x) { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

double lerp(double x, double y, double h) { return x * (1.0 - h) + y * h; }

Vector3 lerp(const Vector3& x, const Vector3& y, double h) {
  return x * (1.0 - h) + y * h;
}

BBox bboxUnion(const BBox& a, const BBox& b) {
  if (a.isInfinite() || b.isInfinite()) return BBox::infinite();
  if (!a.isValid()) return b;
  if (!b.isValid()) return a;
  BBox r;
  r.min = Vector3{std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y),
                  std::min(a.min.z, b.min.z)};
  r.max = Vector3{std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y),
                  std::max(a.max.z, b.max.z)};
  return r;
}

BBox bboxIntersection(const BBox& a, const BBox& b) {
  BBox r;
  r.min = Vector3{std::max(a.min.x, b.min.x), std::max(a.min.y, b.min.y),
                  std::max(a.min.z, b.min.z)};
  r.max = Vector3{std::min(a.max.x, b.max.x), std::min(a.max.y, b.max.y),
                  std::min(a.max.z, b.max.z)};
  return r;  // invalid (min > max) when the boxes are disjoint
}

// A cell grown outward by `r` on every side.
BBox expandBox(const BBox& c, double r) {
  BBox e;
  e.min = Vector3{c.min.x - r, c.min.y - r, c.min.z - r};
  e.max = Vector3{c.max.x + r, c.max.y + r, c.max.z + r};
  return e;
}

// ---- Hard combinators ---------------------------------------------------

class UnionField : public ImplicitField {
public:
  UnionField(FieldPtr a, FieldPtr b) : a_(std::move(a)), b_(std::move(b)) {}

  double valueAt(const Vector3& p) const override {
    return std::min(a_->valueAt(p), b_->valueAt(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // Active operand = the one bounding the union here (smaller value). Its
    // un-blended gradient is what makes the seam come out sharp.
    return (a_->valueAt(p) <= b_->valueAt(p)) ? a_->gradientAt(p)
                                              : b_->gradientAt(p);
  }
  BBox bounds() const override { return bboxUnion(a_->bounds(), b_->bounds()); }
  bool isInside(const Vector3& p) const override {
    return a_->isInside(p) || b_->isInside(p);
  }
  bool cellOverlaps(const BBox& c) const override {
    return a_->cellOverlaps(c) || b_->cellOverlaps(c);
  }

private:
  FieldPtr a_, b_;
};

class IntersectionField : public ImplicitField {
public:
  IntersectionField(FieldPtr a, FieldPtr b)
      : a_(std::move(a)), b_(std::move(b)) {}

  double valueAt(const Vector3& p) const override {
    return std::max(a_->valueAt(p), b_->valueAt(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    return (a_->valueAt(p) >= b_->valueAt(p)) ? a_->gradientAt(p)
                                              : b_->gradientAt(p);
  }
  BBox bounds() const override {
    return bboxIntersection(a_->bounds(), b_->bounds());
  }
  bool isInside(const Vector3& p) const override {
    return a_->isInside(p) && b_->isInside(p);
  }
  bool cellOverlaps(const BBox& c) const override {
    // Conservative: over-refines where a cell straddles one operand but is
    // wholly inside the other. Wasted work, never a missed surface.
    return a_->cellOverlaps(c) || b_->cellOverlaps(c);
  }

private:
  FieldPtr a_, b_;
};

class DifferenceField : public ImplicitField {
public:
  DifferenceField(FieldPtr a, FieldPtr b)
      : a_(std::move(a)), b_(std::move(b)) {}

  double valueAt(const Vector3& p) const override {
    return std::max(a_->valueAt(p), -b_->valueAt(p));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    // Carved surface (b's contribution) faces inward relative to b, so its
    // gradient is the negated b gradient.
    if (a_->valueAt(p) >= -b_->valueAt(p)) return a_->gradientAt(p);
    return b_->gradientAt(p) * -1.0;
  }
  BBox bounds() const override { return a_->bounds(); }  // a \ b is subset of a
  bool isInside(const Vector3& p) const override {
    return a_->isInside(p) && !b_->isInside(p);
  }
  bool cellOverlaps(const BBox& c) const override {
    return a_->cellOverlaps(c) || b_->cellOverlaps(c);
  }

private:
  FieldPtr a_, b_;
};

class XorField : public ImplicitField {
public:
  XorField(FieldPtr a, FieldPtr b) : a_(std::move(a)), b_(std::move(b)) {}

  double valueAt(const Vector3& p) const override {
    const double va = a_->valueAt(p), vb = b_->valueAt(p);
    return std::max(std::min(va, vb), -std::max(va, vb));
  }
  Vector3 gradientAt(const Vector3& p) const override {
    const double va = a_->valueAt(p), vb = b_->valueAt(p);
    const double t1 = std::min(va, vb);
    const double t2 = -std::max(va, vb);
    if (t1 >= t2) {
      return (va <= vb) ? a_->gradientAt(p) : b_->gradientAt(p);
    }
    return ((va >= vb) ? a_->gradientAt(p) : b_->gradientAt(p)) * -1.0;
  }
  BBox bounds() const override { return bboxUnion(a_->bounds(), b_->bounds()); }
  bool isInside(const Vector3& p) const override {
    return a_->isInside(p) != b_->isInside(p);
  }
  bool cellOverlaps(const BBox& c) const override {
    return a_->cellOverlaps(c) || b_->cellOverlaps(c);
  }

private:
  FieldPtr a_, b_;
};

// ---- Smooth combinators -------------------------------------------------
//
// Quilez polynomial smin/smax. The field is C-infinity, so its gradient is a
// genuine blend of the operand gradients -- no sharp-feature special-casing.

double smoothMinValue(double a, double b, double k, double& h) {
  h = clamp01(0.5 + 0.5 * (b - a) / k);
  return lerp(b, a, h) - k * h * (1.0 - h);
}

double smoothMaxValue(double a, double b, double k, double& h) {
  h = clamp01(0.5 - 0.5 * (b - a) / k);
  return lerp(b, a, h) + k * h * (1.0 - h);
}

class SmoothUnionField : public ImplicitField {
public:
  SmoothUnionField(FieldPtr a, FieldPtr b, double k)
      : a_(std::move(a)), b_(std::move(b)), k_(k) {}

  double valueAt(const Vector3& p) const override {
    double h;
    return smoothMinValue(a_->valueAt(p), b_->valueAt(p), k_, h);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    double h;
    smoothMinValue(a_->valueAt(p), b_->valueAt(p), k_, h);
    return lerp(b_->gradientAt(p), a_->gradientAt(p), h);
  }
  BBox bounds() const override {
    // The blend fillet bulges outward by up to k/4; pad to stay conservative.
    BBox u = bboxUnion(a_->bounds(), b_->bounds());
    if (u.isValid()) {
      const Vector3 e{k_, k_, k_};
      u.min = u.min - e;
      u.max = u.max + e;
    }
    return u;
  }
  // smin shifts the value by at most k/4, so the smooth surface lies within
  // k/4 of one operand's surface: expanding the cell by k/4 is the tight
  // never-misses test, and lets a mesh operand answer via its fast BVH
  // descent instead of an smin valueAt storm.
  bool cellOverlaps(const BBox& c) const override {
    const BBox e = expandBox(c, 0.25 * k_);
    return a_->cellOverlaps(e) || b_->cellOverlaps(e);
  }

private:
  FieldPtr a_, b_;
  double   k_;
};

class SmoothIntersectionField : public ImplicitField {
public:
  SmoothIntersectionField(FieldPtr a, FieldPtr b, double k)
      : a_(std::move(a)), b_(std::move(b)), k_(k) {}

  double valueAt(const Vector3& p) const override {
    double h;
    return smoothMaxValue(a_->valueAt(p), b_->valueAt(p), k_, h);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    double h;
    smoothMaxValue(a_->valueAt(p), b_->valueAt(p), k_, h);
    return lerp(b_->gradientAt(p), a_->gradientAt(p), h);
  }
  BBox bounds() const override {
    return bboxIntersection(a_->bounds(), b_->bounds());
  }
  bool cellOverlaps(const BBox& c) const override {
    const BBox e = expandBox(c, 0.25 * k_);
    return a_->cellOverlaps(e) || b_->cellOverlaps(e);
  }

private:
  FieldPtr a_, b_;
  double   k_;
};

class SmoothDifferenceField : public ImplicitField {
public:
  SmoothDifferenceField(FieldPtr a, FieldPtr b, double k)
      : a_(std::move(a)), b_(std::move(b)), k_(k) {}

  double valueAt(const Vector3& p) const override {
    double h;
    return smoothMaxValue(a_->valueAt(p), -b_->valueAt(p), k_, h);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    double h;
    smoothMaxValue(a_->valueAt(p), -b_->valueAt(p), k_, h);
    // d1 = a (grad a), d2 = -b (grad -b); blend lerp(grad_d2, grad_d1, h).
    return lerp(b_->gradientAt(p) * -1.0, a_->gradientAt(p), h);
  }
  BBox bounds() const override { return a_->bounds(); }
  bool cellOverlaps(const BBox& c) const override {
    const BBox e = expandBox(c, 0.25 * k_);
    return a_->cellOverlaps(e) || b_->cellOverlaps(e);
  }

private:
  FieldPtr a_, b_;
  double   k_;
};

// A position-driven linear blend of two fields: value = lerp(a, b, w) with
// w = clamp((control - lo)/(hi - lo), 0, 1). Used to *morph* one solid into
// another across space (e.g. a bcc strut crystal into an fcc one). It is NOT
// an SDF -- a lerp of two distances is not a distance -- but it is a valid
// implicit for dual contouring (its sign changes bound a surface). As with
// every blend here (and the graded decorators), gradientAt treats w as locally
// constant and lerps the operand gradients; the dropped d/dp w term only
// perturbs the normal inside the transition band, which the DC sampler
// tolerates.
class MixField : public ImplicitField {
public:
  MixField(FieldPtr a, FieldPtr b, FieldPtr control, double lo, double hi)
      : a_(std::move(a)), b_(std::move(b)), control_(std::move(control)),
        lo_(lo), hi_(hi) {}

  double valueAt(const Vector3& p) const override {
    const double u = weightAt(p);
    return lerp(a_->valueAt(p), b_->valueAt(p), u);
  }
  Vector3 gradientAt(const Vector3& p) const override {
    const double u = weightAt(p);
    return lerp(a_->gradientAt(p), b_->gradientAt(p), u);
  }
  // The mix surface lies somewhere between a's and b's zero-sets, so the
  // conservative extent is the union of both operands' bounds. The control
  // field only reweights -- it never moves the surface outward -- so it is
  // deliberately left out.
  BBox bounds() const override {
    return bboxUnion(a_->bounds(), b_->bounds());
  }
  // Conservatively refine everywhere inside the bounds (like the TPMS
  // primitives). Unlike min/smooth-union -- whose surface provably stays within
  // k/4 of an operand surface, so `a || b` is a tight never-miss test -- a lerp
  // surface floats FREE of both operands: lerp(A,B,w)=0 wherever A/B = -w/(1-w),
  // which happens at points where A and B are both far from their own zero-sets
  // (e.g. a stub cap deep inside strut A hanging in strut B's empty space). A
  // cell there touches neither operand surface, so `a->cellOverlaps || b->...`
  // returns false and silently drops the cap. There is no cheap tight bound, so
  // we return true and let the sampler's sign test drive refinement. (A tighter
  // future test could OR in an A-vs-B corner-sign-disagreement check.)
  bool cellOverlaps(const BBox&) const override { return true; }

private:
  double weightAt(const Vector3& p) const {
    double denom = hi_ - lo_;
    if (std::abs(denom) < 1e-9) denom = (denom < 0.0) ? -1e-9 : 1e-9;
    return clamp01((control_->valueAt(p) - lo_) / denom);
  }

  FieldPtr a_, b_, control_;
  double   lo_, hi_;
};

} // namespace

FieldPtr unionOf(FieldPtr a, FieldPtr b) {
  return std::make_shared<UnionField>(std::move(a), std::move(b));
}

FieldPtr intersectionOf(FieldPtr a, FieldPtr b) {
  return std::make_shared<IntersectionField>(std::move(a), std::move(b));
}

FieldPtr differenceOf(FieldPtr a, FieldPtr b) {
  return std::make_shared<DifferenceField>(std::move(a), std::move(b));
}

FieldPtr xorOf(FieldPtr a, FieldPtr b) {
  return std::make_shared<XorField>(std::move(a), std::move(b));
}

FieldPtr smoothUnionOf(FieldPtr a, FieldPtr b, double k) {
  return std::make_shared<SmoothUnionField>(std::move(a), std::move(b), k);
}

FieldPtr smoothIntersectionOf(FieldPtr a, FieldPtr b, double k) {
  return std::make_shared<SmoothIntersectionField>(std::move(a), std::move(b),
                                                   k);
}

FieldPtr smoothDifferenceOf(FieldPtr a, FieldPtr b, double k) {
  return std::make_shared<SmoothDifferenceField>(std::move(a), std::move(b),
                                                 k);
}

FieldPtr mixOf(FieldPtr a, FieldPtr b, FieldPtr control, double lo, double hi) {
  return std::make_shared<MixField>(std::move(a), std::move(b),
                                    std::move(control), lo, hi);
}

} // namespace dualc
