// Smoke test for the vendored Unlicense QEF/SVD solver. Confirms that the
// translation unit links and the solver produces a sensible result on a
// single-plane Hermite configuration.

#include "internal/qef.h"
#include "internal/svd.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("QefSolver minimizes a single-plane configuration to a point on the plane",
          "[qef]") {
  // Plane: z = 0.5, with normal (0,0,1). Three Hermite samples on that plane,
  // each with normal (0,0,1). The QEF minimum must lie on z = 0.5.
  svd::QefSolver q;
  q.add(0.10f, 0.10f, 0.5f, 0.0f, 0.0f, 1.0f);
  q.add(0.40f, 0.20f, 0.5f, 0.0f, 0.0f, 1.0f);
  q.add(0.20f, 0.70f, 0.5f, 0.0f, 0.0f, 1.0f);

  svd::Vec3 out;
  const float residual = q.solve(out, /*svdTol=*/1e-6f, /*sweeps=*/50, /*pinvTol=*/1e-6f);

  REQUIRE(std::isfinite(out.x));
  REQUIRE(std::isfinite(out.y));
  REQUIRE(std::isfinite(out.z));
  REQUIRE(std::abs(out.z - 0.5f) < 1e-4f);
  REQUIRE(residual < 1e-4f);
}

// The vendored `Svd::pinv` carries one documented local modification: upstream
// zeroes any eigenvalue with `fabs(1/x) < tol` as well as any with
// `fabs(x) < tol`. The rows of A are unit normals, so lambda_max(A^T A) is at
// most the sample count -- with DualC's default tolerance of 0.1 that second
// clause discarded EVERY direction once more than 10 aligned samples had been
// accumulated, and the solve fell back to the mass point. Per-leaf QEFs hold at
// most 12 samples so they were nearly always fine; adaptive collapse merges up
// to 96 and was hit routinely. See THIRD_PARTY.md.
TEST_CASE("QefSolver resolves a sharp corner at any sample multiplicity",
          "[qef]") {
  // Three orthogonal planes x = 0.3, y = 0.3, z = 0.3 meeting at
  // (0.3, 0.3, 0.3); sample points deliberately off the corner so the mass
  // point (0.4333, 0.5333, 0.4333) is somewhere else entirely.
  for (const int reps : {1, 5, 10, 11, 32}) {
    svd::QefSolver q;
    for (int i = 0; i < reps; ++i) {
      q.add(0.3f, 0.7f, 0.9f, 1.0f, 0.0f, 0.0f);
      q.add(0.8f, 0.3f, 0.1f, 0.0f, 1.0f, 0.0f);
      q.add(0.2f, 0.6f, 0.3f, 0.0f, 0.0f, 1.0f);
    }
    svd::Vec3 out;
    q.solve(out, /*svdTol=*/1e-6f, /*sweeps=*/50, /*pinvTol=*/0.1f);
    INFO("sample repetitions: " << reps);
    REQUIRE(std::abs(out.x - 0.3f) < 1e-3f);
    REQUIRE(std::abs(out.y - 0.3f) < 1e-3f);
    REQUIRE(std::abs(out.z - 0.3f) < 1e-3f);
    // The corner is an exact fit, so the geometric energy is zero there.
    REQUIRE(q.getError(out) < 1e-4f);
  }
}
