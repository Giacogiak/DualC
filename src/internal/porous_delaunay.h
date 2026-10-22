#pragma once

#include "dualc/progress.h"
#include "dualc/types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dualc {
namespace internal {

// The Delaunay tetrahedralization of the porous seeds (roadmap 23 #64), the
// input of the cell complex. Defined in src/implicit/porous_delaunay.cpp.
//
// Tet t has the vertices verts[4t .. 4t+3], indices into the input points in
// ascending order, and neighbours[4t + k] is the tet across the face opposite
// verts[4t + k], or -1 where that face lies on the convex hull. The tets are
// sorted by their vertex quadruples. Orientation is not encoded: the sorted
// order is positive or negative, and orient3d on the four points says which.
struct DelaunayTets {
  std::vector<std::int32_t> verts;
  std::vector<std::int32_t> neighbours;
  // Input indices whose point equals an earlier one (the same three doubles):
  // they are left out, so no tet names them. Ascending.
  std::vector<std::int32_t> duplicates;

  std::size_t size() const { return verts.size() / 4; }
};

// Bowyer-Watson on Shewchuk's exact predicates, sequential and deterministic:
// the points go in in 63-bit Morton order of their bounding box (ties by
// index), each by a visibility walk from the last tet made, then the cavity of
// the tets whose circumsphere strictly contains it. The hull is exact (an
// infinite vertex closes it, not a finite super-tetrahedron), and the strict
// cavity never makes a flat tet. A cospherical set (a regular grid) gets one of
// its Delaunay tetrahedralizations, chosen by the insertion order alone; the
// same points in the same order always give the same list, on any OS.
//
// Throws std::invalid_argument ("porous delaunay: ...") for a non-finite
// coordinate, more than kPorousMaxSeeds points, or points that do not span
// 3D (fewer than four distinct, or all coplanar); dualc::Cancelled when the
// token is requested (polled every 4096 insertions).
DelaunayTets tetrahedralize(const std::vector<Vector3>& points,
                            const CancelToken* cancel = nullptr);

} // namespace internal
} // namespace dualc
