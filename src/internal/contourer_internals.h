#pragma once

#include "dualc/contourer.h"
#include "dualc/hermite_octree.h"
#include "dualc/types.h"
#include "internal/cube_components.h"

#include <array>

namespace dualc {
namespace internal {

// One component's QEF solution: vertex position and the unit-normalized
// average of the component's input normals.
struct LeafSolve {
  Vector3 vertex{0.0, 0.0, 0.0};
  Vector3 normal{0.0, 0.0, 0.0};
  bool    hasVertex = false;
};

// MDC payload: one QEF solve per surface component inside a leaf cell.
// `parts.numComponents` is the active prefix length of `perComponent`.
struct MultiLeafSolve {
  EdgeComponents           parts{};
  std::array<LeafSolve, 4> perComponent{};
};

// Solve a leaf's per-component QEFs. See contourer.cpp for the implementation.
// Exposed here so the test suite can exercise the multi-vertex path on
// hand-built HermiteLeafData without depending on the recursive contour
// traversal (which only allocates vertices when triangles emit).
MultiLeafSolve solveLeaf(const HermiteNode& node, const ContourerParams& params);

} // namespace internal
} // namespace dualc
