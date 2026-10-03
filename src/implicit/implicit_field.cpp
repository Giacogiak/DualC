#include "dualc/implicit.h"

#include <cmath>

namespace dualc {

namespace {

// edgeHit root-finder tuning. An implicit field is (close to) a signed
// distance, so along a short cube edge its value is near-linear: a linear
// seed plus a handful of Illinois-modified false-position steps converges
// far faster than blind bisection. ~6 valueAt calls instead of 50.
constexpr int    kEdgeHitRefineSteps = 6;
constexpr double kEdgeHitParamTol    = 1e-6;   // bracket width in t-space
constexpr double kEdgeHitValueTol    = 1e-9;   // |field| considered on-surface

} // namespace

bool ImplicitField::isInside(const Vector3& p) const {
  return valueAt(p) < 0.0;
}

bool ImplicitField::edgeHit(const Vector3& a, const Vector3& b,
                            Vector3& outP, Vector3& outN) const {
  const double va = valueAt(a);
  const double vb = valueAt(b);

  // The endpoints must straddle the surface for a bracketed root to exist.
  if ((va < 0.0) == (vb < 0.0)) return false;

  // Root-find the crossing parameter t in [0, 1] along a -> b. The bracket
  // (tLo, vLo) / (tHi, vHi) always straddles the surface; `tStar` is the
  // current false-position estimate. The Illinois trick halves the value of
  // a bracket end that has gone stale, restoring fast two-sided convergence
  // even on a curved (smin-blended) field.
  double tLo = 0.0, tHi = 1.0;
  double vLo = va, vHi = vb;
  double tStar = va / (va - vb);   // false-position seed, in (0, 1)
  int    lastSide = 0;             // -1 = moved lo last, +1 = moved hi last

  for (int i = 0; i < kEdgeHitRefineSteps; ++i) {
    if (!(tStar > tLo && tStar < tHi)) tStar = 0.5 * (tLo + tHi);
    const Vector3 p  = a + (b - a) * tStar;
    const double  vt = valueAt(p);
    if (std::abs(vt) <= kEdgeHitValueTol) { tLo = tHi = tStar; break; }

    if ((vt < 0.0) == (vLo < 0.0)) {
      tLo = tStar; vLo = vt;
      if (lastSide < 0) vHi *= 0.5;       // stale hi end -> Illinois shrink
      lastSide = -1;
    } else {
      tHi = tStar; vHi = vt;
      if (lastSide > 0) vLo *= 0.5;       // stale lo end -> Illinois shrink
      lastSide = 1;
    }
    if ((tHi - tLo) <= kEdgeHitParamTol) break;

    const double denom = vLo - vHi;
    tStar = (std::abs(denom) > 1e-300) ? tLo + vLo * (tHi - tLo) / denom
                                       : 0.5 * (tLo + tHi);
  }

  tStar = 0.5 * (tLo + tHi);
  outP = a + (b - a) * tStar;
  const Vector3 g = gradientAt(outP);
  const double  gn = g.norm();
  outN = (gn > 1e-12) ? (g / gn) : Vector3{0.0, 0.0, 0.0};
  return true;
}

bool ImplicitField::closestSurfacePoint(const Vector3& q,
                                        Vector3& outP, Vector3& outN) const {
  // One Newton step on the field. On a Lipschitz-1 SDF q - (f/|grad|) * grad
  // lands within a small fraction of the true surface; refining once at the
  // projected point gives a clean normal. Fields with degenerate gradients
  // (a constant, an infinity, the empty field) return false.
  const double  f = valueAt(q);
  const Vector3 g = gradientAt(q);
  const double  m = g.norm();
  if (!(m > 1e-12)) return false;
  outP = q - g * (f / m);
  const Vector3 g2 = gradientAt(outP);
  const double  m2 = g2.norm();
  outN = (m2 > 1e-12) ? (g2 / m2) : (g / m);
  return true;
}

bool ImplicitField::cellOverlaps(const BBox& cell) const {
  // The surface passes through the cell if the 8 corner signs disagree.
  bool firstInside = false;
  bool haveFirst   = false;
  for (int c = 0; c < 8; ++c) {
    const Vector3 corner{
        (c & 1) ? cell.max.x : cell.min.x,
        (c & 2) ? cell.max.y : cell.min.y,
        (c & 4) ? cell.max.z : cell.min.z};
    const bool inside = isInside(corner);
    if (!haveFirst) {
      firstInside = inside;
      haveFirst   = true;
    } else if (inside != firstInside) {
      return true;
    }
  }

  // All corners agree: the surface could still dip into the cell without
  // touching a corner. Conservative Lipschitz-1 test -- if the field value
  // at the centre is within the half space-diagonal, a true SDF's zero set
  // may reach inside. For non-Lipschitz fields this can under-refine; the
  // corner test above still catches anything reaching a corner.
  const Vector3 ext = cell.extent();
  const double  halfDiag = 0.5 * ext.norm();
  return std::abs(valueAt(cell.center())) <= halfDiag;
}

} // namespace dualc
