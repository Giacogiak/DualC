#pragma once

#include "dualc/types.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace dualc {
namespace internal {

// Pure-arithmetic helpers for the narrow-band mesh bake. They operate on flat
// grids in x-fastest-then-y-then-z order (matching GridField). No BVH, no SDF.
//
// `band` flags the voxels that already carry an exact signed distance; the
// helpers fill in the sign and a monotone magnitude for every other voxel.

inline std::size_t gridIndex(const Vector3i& res, int x, int y, int z) {
  return static_cast<std::size_t>(x) +
         static_cast<std::size_t>(res.x) *
             (static_cast<std::size_t>(y) +
              static_cast<std::size_t>(res.y) * static_cast<std::size_t>(z));
}

// Classify every NON-band voxel as outside (+1) or inside (-1).
//
// For WATERTIGHT input the band is a closed shell at least ~1 voxel thick
// (bakeToGrid floors bandWidth at 1.5*maxCell and flags a voxel whenever a
// triangle lies within that Chebyshev half-width), so a surface passing between
// two 6-adjacent voxel centres would have flagged BOTH. The true sign is
// therefore constant over each 6-connected component of non-band voxels: label
// the components, then ask the caller for the sign of one voxel per component.
//
// That is the same closed-surface precondition `MeshSource` already carries (a
// soup / open shell has no meaningful inside, and routes through the
// generalized-winding-number field instead). Given it, this makes NO assumption
// about the bake REGION: correct for a padded box, a box flush with the mesh
// AABB, a box the mesh only partly overlaps, and a box strictly inside the solid
// (empty band -> one component -> all inside). The previous version seeded the 6
// region faces as outside, which dissolved interior material whenever the region
// was not padded -- the GPU preview bakes over the raymarch box, which is
// routinely a sub-box of the mesh (`intersection(mesh, smaller_shape)`, or a
// --bounds zoom). It also inferred "unreachable from the faces => inside", which
// filled enclosed cavities; the sign query resolves those correctly too.
//
// On an OPEN shell the band has a hole, so one component can span it and carry
// both signs; the probes then paint it a single sign. The old face-flood
// degraded differently there (everything reachable was called outside), not
// better -- neither is correct, and neither is in contract.
//
// `insideAt(i)` takes a flat voxel index and returns true when that voxel's
// world position is inside the solid; it is called on up to 3 DISTINCT voxels
// per component (the caller's oracle may already vote internally, so this is a
// guard against one degenerate probe position, not a second majority).
// `farSign` is sized res.x*res.y*res.z; entries for band voxels are left 0
// (the caller uses the band's exact value there instead).
template <class InsideAt>
inline void floodFarSign(const Vector3i& res, const std::vector<char>& band,
                         std::vector<signed char>& farSign, InsideAt insideAt) {
  const int rx = res.x, ry = res.y, rz = res.z;
  const std::size_t n = static_cast<std::size_t>(rx) *
                        static_cast<std::size_t>(ry) *
                        static_cast<std::size_t>(rz);
  farSign.assign(n, 0);

  const std::size_t plane = static_cast<std::size_t>(rx) *
                            static_cast<std::size_t>(ry);
  // One component at a time; `comp` is reused across components. `farSign`
  // doubles as the visited mark: 2 = claimed but undecided, so the BFS test
  // stays `== 0` and the final sign overwrites it.
  std::vector<std::size_t> comp;

  for (std::size_t start = 0; start < n; ++start) {
    if (band[start] != 0 || farSign[start] != 0) continue;

    comp.clear();
    farSign[start] = 2;
    comp.push_back(start);
    for (std::size_t head = 0; head < comp.size(); ++head) {
      const std::size_t i = comp[head];
      const int z   = static_cast<int>(i / plane);
      const int rem = static_cast<int>(i % plane);
      const int y = rem / rx;
      const int x = rem % rx;
      const int nb[6][3] = {{x - 1, y, z}, {x + 1, y, z}, {x, y - 1, z},
                            {x, y + 1, z}, {x, y, z - 1}, {x, y, z + 1}};
      for (const auto& q : nb) {
        if (q[0] < 0 || q[0] >= rx || q[1] < 0 || q[1] >= ry ||
            q[2] < 0 || q[2] >= rz)
          continue;
        const std::size_t j = gridIndex(res, q[0], q[1], q[2]);
        if (band[j] == 0 && farSign[j] == 0) {
          farSign[j] = 2;
          comp.push_back(j);
        }
      }
    }

    // Probe up to three DISTINCT voxels spread through the component (a
    // 1- or 2-voxel component is probed once or twice, not three times over the
    // same position). Constancy is structural (see above), so this guards a
    // single degenerate probe, not a genuinely mixed component.
    const std::size_t n3 = comp.size() < 3 ? comp.size() : 3;
    const std::size_t probe[3] = {comp.front(), comp[comp.size() / 2],
                                  comp.back()};
    int votesInside = 0;
    for (std::size_t k = 0; k < n3; ++k)
      if (insideAt(probe[k])) ++votesInside;
    const signed char s = (votesInside * 2 > int(n3)) ? -1 : 1;
    for (const std::size_t i : comp) farSign[i] = s;
  }
}

// Fill `dist` with a strictly growing (monotone) approximate distance for
// every NON-band voxel, leaving band voxels fixed at their exact magnitude.
// A 2-pass forward/backward chamfer over the 26-neighbourhood: band voxels
// are immutable Dirichlet seeds, non-band voxels start at a finite cap and
// only ever take `neighbour + stepLength`. Because a non-band voxel always
// gains a positive step over its band neighbour, its magnitude is guaranteed
// >= the band's -- which keeps trilinear interpolation monotone across the
// band boundary (no spurious zero crossing) and keeps the contour octree
// from over-refining the far field.
inline void chamferGrow(const Vector3i& res, const Vector3& cell,
                        const std::vector<char>& band,
                        const std::vector<float>& bandAbs,
                        std::vector<float>& dist) {
  const int rx = res.x, ry = res.y, rz = res.z;
  const std::size_t n = static_cast<std::size_t>(rx) *
                        static_cast<std::size_t>(ry) *
                        static_cast<std::size_t>(rz);

  const double dx = cell.x * (rx - 1), dy = cell.y * (ry - 1),
               dz = cell.z * (rz - 1);
  const double diag = std::sqrt(dx * dx + dy * dy + dz * dz);
  const float  cap  = static_cast<float>(diag > 0.0 ? diag : 1.0);

  dist.assign(n, cap);
  for (std::size_t i = 0; i < n; ++i)
    if (band[i]) dist[i] = bandAbs[i];

  struct Off { int dx, dy, dz; float w; };
  std::vector<Off> fwd, bwd;
  for (int oz = -1; oz <= 1; ++oz)
    for (int oy = -1; oy <= 1; ++oy)
      for (int ox = -1; ox <= 1; ++ox) {
        if (ox == 0 && oy == 0 && oz == 0) continue;
        const float w = static_cast<float>(std::sqrt(
            static_cast<double>(ox) * ox * cell.x * cell.x +
            static_cast<double>(oy) * oy * cell.y * cell.y +
            static_cast<double>(oz) * oz * cell.z * cell.z));
        const bool causal =
            (oz < 0) || (oz == 0 && oy < 0) || (oz == 0 && oy == 0 && ox < 0);
        (causal ? fwd : bwd).push_back(Off{ox, oy, oz, w});
      }

  auto pass = [&](const std::vector<Off>& offs, bool forward) {
    for (int zz = 0; zz < rz; ++zz) {
      const int z = forward ? zz : rz - 1 - zz;
      for (int yy = 0; yy < ry; ++yy) {
        const int y = forward ? yy : ry - 1 - yy;
        for (int xx = 0; xx < rx; ++xx) {
          const int x = forward ? xx : rx - 1 - xx;
          const std::size_t i = gridIndex(res, x, y, z);
          if (band[i]) continue;                   // immutable seed
          float d = dist[i];
          for (const Off& o : offs) {
            const int nx = x + o.dx, ny = y + o.dy, nz = z + o.dz;
            if (nx < 0 || nx >= rx || ny < 0 || ny >= ry ||
                nz < 0 || nz >= rz)
              continue;
            const float cand = dist[gridIndex(res, nx, ny, nz)] + o.w;
            if (cand < d) d = cand;
          }
          dist[i] = d;
        }
      }
    }
  };
  pass(fwd, true);
  pass(bwd, false);
}

} // namespace internal
} // namespace dualc
