#pragma once

#include <array>
#include <cstdint>

namespace dualc {
namespace internal {

// Partition of the 12 cube edges into surface components. An edge belongs
// to a component iff it has a sign-change crossing (hasCrossing[e] == true);
// otherwise componentOfEdge[e] == -1. Valid component IDs are 0..numComponents-1
// (numComponents in 0..4 by cube combinatorics).
struct EdgeComponents {
  std::array<std::int8_t, 12> componentOfEdge{};
  int numComponents = 0;
};

// Partition the cube's crossing edges into connected surface components. The
// connection rule walks the 6 cube faces:
//   * 0 sign changes around the face cycle -> nothing to connect.
//   * 2 sign changes -> the 2 face-boundary crossings are joined.
//   * 4 sign changes (saddle) -> ambiguous; we pair the two crossings that
//     share an inside corner (the "inside-pair" rule). This is one of the
//     two valid topology choices; Schaefer's asymptotic decider needs corner
//     SDF values, which DualC's HermiteLeafData does not store today.
//
// O(1) time, no allocations. See [src/internal/cube_components.cpp] for the
// derivation of the saddle pairing rule.
EdgeComponents partitionCubeEdges(
    const std::array<bool, 8>&  cornerInside,
    const std::array<bool, 12>& hasCrossing);

} // namespace internal
} // namespace dualc
