#include "sign_oracle.h"

#include "mesh_bvh.h"

#include <array>
#include <cmath>

namespace dualc {
namespace internal {

namespace {

// Three probe directions with no axis-aligned components and no two parallel
// to each other. The first is sqrt-3-derived and avoids axis-aligned mesh
// degeneracies; the other two are independent permutations. Inside-test is
// the majority of the three parity results, giving robustness when one ray
// happens to graze a coplanar edge or a T-junction.
const std::array<Vector3, 3> kProbeDirs = []() {
  Vector3 a{1.0, 0.7320508075688772, 0.5773502691896257};
  Vector3 b{0.5773502691896257, 1.0, 0.7320508075688772};
  Vector3 c{0.7320508075688772, 0.5773502691896257, 1.0};
  return std::array<Vector3, 3>{a / a.norm(), b / b.norm(), c / c.norm()};
}();

} // namespace

SignOracle::SignOracle(const MeshBVH& bvh, SignMethod method)
    : bvh_(bvh), method_(method) {}

bool SignOracle::isInside(const Vector3& p) const {
  if (method_ == SignMethod::PSEUDONORMAL) {
    // Bærentzen-Aanæs angle-weighted pseudonormal: one closest-point query
    // gives both the surface point cp and the feature-aware normal n; the
    // sign of dot(p - cp, n) is the inside test. Correct only for
    // watertight oriented input -- on broken meshes prefer WINDING_NUMBER.
    Vector3 cp, n;
    int     tri;
    if (!bvh_.closestPointWithPseudoNormal(p, cp, n, tri)) return false;
    return dot(p - cp, n) < 0.0;
  }

  if (method_ == SignMethod::GENERALIZED_WINDING_NUMBER) {
    // Hierarchically accelerated generalized winding number. w(p) ~ 1 inside
    // and ~0 outside; the 0.5 threshold recovers a robust inside/outside even
    // on non-watertight input (open shells, soup, self-intersections).
    return bvh_.windingNumberFast(p) > 0.5;
  }

  // WINDING_NUMBER: 3-ray majority parity. Correct for closed/oriented
  // meshes; robust against single-ray degeneracies (grazing edges,
  // coplanar near-misses). For broken input prefer GENERALIZED_WINDING_NUMBER.
  int votes = 0;
  for (const Vector3& d : kProbeDirs) {
    if ((bvh_.countRayHits(p, d) & 1) != 0) ++votes;
  }
  return votes >= 2;
}

} // namespace internal
} // namespace dualc
