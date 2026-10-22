#pragma once

#include "dualc/implicit.h"

namespace dualc {
namespace internal {

// The one central-difference stencil of a field's own valueAt, for the
// fields with no cheap closed-form gradient. Step 1e-4 in world units, the
// same step PrimitiveField's (header-inline) gradientAt uses.
constexpr double kFdStep = 1e-4;

// The three undivided central differences f(p + e) - f(p - e), one per axis.
// Kept undivided so the two callers below round exactly as the copies they
// replaced did.
inline Vector3 centralDifferences(const ImplicitField& f, const Vector3& p) {
  constexpr double e = kFdStep;
  const double dx = f.valueAt(Vector3{p.x + e, p.y, p.z}) -
                    f.valueAt(Vector3{p.x - e, p.y, p.z});
  const double dy = f.valueAt(Vector3{p.x, p.y + e, p.z}) -
                    f.valueAt(Vector3{p.x, p.y - e, p.z});
  const double dz = f.valueAt(Vector3{p.x, p.y, p.z + e}) -
                    f.valueAt(Vector3{p.x, p.y, p.z - e});
  return Vector3{dx, dy, dz};
}

// The gradientAt contract by finite differences: the unit direction of the
// central difference, (0,0,1) where it vanishes. Used by the distorted domain
// operators (twist/bend/displace) and the 2D->3D lifts.
inline Vector3 fdGradientDirection(const ImplicitField& f, const Vector3& p) {
  const Vector3 g = centralDifferences(f, p);
  const double n = g.norm();
  return (n > 1e-12) ? g * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
}

// The true gradient by finite differences, magnitude kept: the default of
// ImplicitField::trueGradientAt.
inline Vector3 fdTrueGradient(const ImplicitField& f, const Vector3& p) {
  return centralDifferences(f, p) * (1.0 / (2.0 * kFdStep));
}

} // namespace internal
} // namespace dualc
