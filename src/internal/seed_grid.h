#pragma once

#include "dualc/types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace dualc {
namespace internal {

// A uniform grid of point indices over a box, for the porous build (roadmap
// 23): Bridson's conflict search inserts into it one point at a time, Lloyd
// and the complex's start bins ask it for the nearest point. Each cell is a
// singly linked list (head per cell, next per point), so a cell may hold any
// number of points and insertion is O(1). A point outside the box is filed
// in the nearest boundary cell, so queries stay correct for it.
//
// Deterministic: a query's answer never depends on the visiting order (the
// nearest point breaks distance ties by the smaller index).
class SeedGrid {
public:
  SeedGrid(const BBox& box, double cell) : min_(box.min), h_(cell) {
    const Vector3 e = box.extent();
    n_[0] = dimFor(e.x, cell);
    n_[1] = dimFor(e.y, cell);
    n_[2] = dimFor(e.z, cell);
    head_.assign(static_cast<std::size_t>(n_[0]) * n_[1] * n_[2], -1);
  }

  // The number of cells a grid over `box` at `cell` would hold (before
  // allocating it).
  static double cellCount(const BBox& box, double cell) {
    const Vector3 e = box.extent();
    return std::ceil(e.x / cell) * std::ceil(e.y / cell) * std::ceil(e.z / cell);
  }

  double cellSize() const { return h_; }

  // Files point `index` (its position `p`); indices must be inserted in
  // increasing order 0, 1, 2, ...
  void insert(std::int32_t index, const Vector3& p) {
    if (static_cast<std::size_t>(index) >= next_.size())
      next_.resize(static_cast<std::size_t>(index) + 1, -1);
    const std::size_t c = flat(coord(p.x, 0), coord(p.y, 1), coord(p.z, 2));
    next_[static_cast<std::size_t>(index)] = head_[c];
    head_[c] = index;
  }

  // Calls fn(index) for every point filed in a cell the box [p - r, p + r]
  // touches (a superset of the points within r of p).
  template <typename Fn>
  void forEachNear(const Vector3& p, double r, Fn fn) const {
    const int x0 = coord(p.x - r, 0), x1 = coord(p.x + r, 0);
    const int y0 = coord(p.y - r, 1), y1 = coord(p.y + r, 1);
    const int z0 = coord(p.z - r, 2), z1 = coord(p.z + r, 2);
    for (int z = z0; z <= z1; ++z)
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
          for (std::int32_t i = head_[flat(x, y, z)]; i >= 0;
               i = next_[static_cast<std::size_t>(i)])
            fn(i);
  }

  // The index of the point of `pts` nearest to p (ties: the smaller index),
  // or -1 when the grid is empty. Searches rings of cells around p's cell
  // until no unvisited cell can hold a nearer point.
  std::int32_t nearest(const Vector3& p, const std::vector<Vector3>& pts) const {
    const int c[3] = {coord(p.x, 0), coord(p.y, 1), coord(p.z, 2)};
    const double q[3] = {p.x, p.y, p.z};
    const double o[3] = {min_.x, min_.y, min_.z};
    const int maxRing = std::max(n_[0], std::max(n_[1], n_[2]));
    std::int32_t best = -1;
    double bestD2 = std::numeric_limits<double>::infinity();
    for (int k = 0; k <= maxRing; ++k) {
      for (int z = c[2] - k; z <= c[2] + k; ++z) {
        if (z < 0 || z >= n_[2]) continue;
        const bool zEdge = (z == c[2] - k || z == c[2] + k);
        for (int y = c[1] - k; y <= c[1] + k; ++y) {
          if (y < 0 || y >= n_[1]) continue;
          const bool yEdge = zEdge || y == c[1] - k || y == c[1] + k;
          for (int x = c[0] - k; x <= c[0] + k; ++x) {
            if (x < 0 || x >= n_[0]) continue;
            // Only the ring's shell: the inside was visited at k - 1.
            if (!yEdge && x != c[0] - k && x != c[0] + k) continue;
            for (std::int32_t i = head_[flat(x, y, z)]; i >= 0;
                 i = next_[static_cast<std::size_t>(i)]) {
              const Vector3 d = pts[static_cast<std::size_t>(i)] - p;
              const double d2 = d.x * d.x + d.y * d.y + d.z * d.z;
              if (d2 < bestD2 || (d2 == bestD2 && i < best)) {
                bestD2 = d2;
                best = i;
              }
            }
          }
        }
      }
      if (best >= 0) {
        // Every cell of ring k + 1 lies outside the block of rings 0..k, so
        // at least `gap` from p (a block face that is also the grid's edge
        // has nothing beyond it).
        double gap = std::numeric_limits<double>::infinity();
        for (int a = 0; a < 3; ++a) {
          if (c[a] - k > 0) gap = std::min(gap, q[a] - (o[a] + (c[a] - k) * h_));
          if (c[a] + k < n_[a] - 1)
            gap = std::min(gap, (o[a] + (c[a] + k + 1) * h_) - q[a]);
        }
        if (gap == std::numeric_limits<double>::infinity()) break;
        if (gap > 0.0 && gap * gap >= bestD2) break;
      }
    }
    return best;
  }

private:
  static int dimFor(double extent, double cell) {
    return std::max(1, static_cast<int>(std::ceil(extent / cell)));
  }

  int coord(double v, int axis) const {
    const double o = axis == 0 ? min_.x : axis == 1 ? min_.y : min_.z;
    const double f = std::floor((v - o) / h_);
    if (!(f > 0.0)) return 0;  // also NaN
    if (f >= n_[axis] - 1) return n_[axis] - 1;
    return static_cast<int>(f);
  }
  std::size_t flat(int x, int y, int z) const {
    return (static_cast<std::size_t>(z) * n_[1] + y) * n_[0] + x;
  }

  Vector3 min_;
  double h_;
  int n_[3] = {1, 1, 1};
  std::vector<std::int32_t> head_;
  std::vector<std::int32_t> next_;
};

} // namespace internal
} // namespace dualc
