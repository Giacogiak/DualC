#pragma once

#include "dualc/porous.h"
#include "dualc/progress.h"
#include "dualc/types.h"

#include "internal/porous_delaunay.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dualc {
namespace internal {

// The porous cell complex (roadmap 23 #64): what the seeds' Delaunay
// tetrahedralization yields, packed in the flat arrays the GLSL node uploads
// (#66) and the CPU field reads (#65), with the distance query over them.
// Defined in src/implicit/porous_complex.cpp; the layout and every rule here
// are recorded in docs/roadmap/23-porous-shell/04-complex-and-bins.md.

// --- The dual: every primitive, in doubles ---------------------------------

struct PorousDual {
  // The seeds as the complex stores them: each coordinate rounded to float,
  // so the tetrahedralization is that of exactly the points packed.
  std::vector<Vector3> points;
  DelaunayTets tets;
  // Each Delaunay face once: faces[3f .. 3f+2] its vertices (ascending), and
  // faceTets[2f], faceTets[2f+1] the tets on its two sides (the lower index
  // first; -1 for the second on the hull). Face (t, k) -- the face of tet t
  // opposite its vertex k -- is listed where neighbours[4t+k] is -1 or > t,
  // in that (t, k) order. Its dual is a Voronoi edge: the segment between
  // the two tets' circumcentres, or a ray from the one on the hull.
  std::vector<std::int32_t> faces;
  std::vector<std::int32_t> faceTets;
  // Each Delaunay edge once: edges[2e], edges[2e+1], ascending, sorted.
  std::vector<std::int32_t> edges;
  // Each seed's Delaunay neighbours (CSR, ascending): the seeds whose
  // bisector planes bound its Voronoi cell.
  std::vector<std::int32_t> neighbourOffsets;  // points.size() + 1
  std::vector<std::int32_t> neighbours;
  // Each tet's circumcentre -- a Voronoi vertex -- clamped to the padded box
  // (SeedSet::box); clamped[t] says it was outside.
  std::vector<Vector3> circumcentres;
  std::vector<std::uint8_t> clamped;
  std::size_t clampedCount = 0;
};

// Rounds the seeds to float, tetrahedralizes them and derives the dual.
// Throws what tetrahedralize throws.
PorousDual derivePorousDual(const SeedSet& seeds, const CancelToken* cancel = nullptr);

// --- The packed complex ------------------------------------------------------

struct PorousComplexStats {
  std::size_t seeds = 0, tets = 0, duplicates = 0;
  std::size_t delaunayFaces = 0, hullFaces = 0, delaunayEdges = 0;
  std::size_t clampedCircumcentres = 0;
  // Struts: candidates before the drop rule, and what it dropped.
  std::size_t candidateSegments = 0;
  std::size_t droppedOutside = 0;     // the clipped segment is empty
  std::size_t droppedZeroLength = 0;  // both float ends equal
  std::size_t droppedDuplicate = 0;   // the same float segment twice
  std::size_t segments = 0;
  std::size_t bins = 0, binItems = 0, maxBinItems = 0;
  // The largest circumradius over the tets with a non-pad vertex whose
  // circumcentre lies in the volume's box: the largest cell radius.
  double largestCellRadius = 0.0;
};

// The bin grid: bin (i, j, k) covers origin + [i, i+1) x [j, j+1) x [k, k+1)
// times `size`, flat index (k * dims[1] + j) * dims[0] + i; a point outside
// the grid falls in the nearest boundary bin.
struct PorousBins {
  float origin[3] = {0.0f, 0.0f, 0.0f};
  float size = 1.0f;
  std::int32_t dims[3] = {1, 1, 1};

  int coord(double v, int axis) const {
    const double f = std::floor((v - double(origin[axis])) / double(size));
    if (!(f > 0.0)) return 0;  // also NaN
    if (f >= dims[axis] - 1) return dims[axis] - 1;
    return static_cast<int>(f);
  }
  std::size_t flat(int x, int y, int z) const {
    return (static_cast<std::size_t>(z) * std::size_t(dims[1]) + std::size_t(y)) *
               std::size_t(dims[0]) + std::size_t(x);
  }
  std::size_t count() const {
    return std::size_t(dims[0]) * std::size_t(dims[1]) * std::size_t(dims[2]);
  }
};

// The arrays one (cells, mode) case needs; the others stay empty. All
// indices are int32 (-1 = none); all geometry float.
struct PorousComplex {
  PorousCells cells = PorousCells::Voronoi;
  PorousMode  mode  = PorousMode::Walls;
  PorousBins  bins;
  // Struts: the clamp B (= bins.size): the value is min(distance, B).
  float bound = 0.0f;

  // Voronoi walls: seeds[4i .. 4i+3] = x, y, z, 1 if pad else 0; the
  // neighbour CSR; binStart[b] a seed near bin b's centre (the nearest of
  // the bin's own, else propagated from its neighbours).
  std::vector<float>        seeds;
  std::vector<std::int32_t> neighbourOffsets;
  std::vector<std::int32_t> neighbours;
  // Delaunay walls: tetPlanes[16t + 4k ..] = (n, d) of the face opposite
  // vertex k, n the outward unit normal (n.p + d > 0 beyond the face); a face
  // shared by two tets is stored as (n, d) in one and (-n, -d) in the other,
  // exactly; tetNeighbours[4t + k] as DelaunayTets::neighbours; binStart[b]
  // the tet holding bin b's centre, walked to from a tet at its start seed
  // (or the hull tet that walk leaves by).
  std::vector<float>        tetPlanes;
  std::vector<std::int32_t> tetNeighbours;
  std::vector<std::int32_t> binStart;
  // Struts: segments[8s .. 8s+7] = a.x, a.y, a.z, 0, b.x, b.y, b.z, 0; the
  // bins' CSR, each segment filed in every bin its box touches, in segment
  // order.
  std::vector<float>        segments;
  std::vector<std::int32_t> binOffsets;
  std::vector<std::int32_t> binItems;

  PorousComplexStats stats;

  // The bytes of the packed arrays (what #66 uploads).
  std::size_t packedBytes() const;
};

// One function from seeds to a queryable packed complex, for every
// cells x mode case. Deterministic: the same SeedSet gives the same arrays,
// bit for bit, on any OS. Throws std::invalid_argument ("porous complex: ...")
// on a SeedSet without positive spacings or whose isPad does not match its
// points, what tetrahedralize throws, and Cancelled.
PorousComplex buildPorousComplex(const SeedSet& seeds, PorousCells cells,
                                 PorousMode mode,
                                 const CancelToken* cancel = nullptr);

// The most bins a grid gets: one 2048 x 8192 texture of start bins or
// offsets. A sparse volume in a large box gets coarser bins instead.
constexpr double kPorousMaxBins = 2048.0 * 8192.0;

// The same, from a dual already derived (one dual serves the four cases);
// `maxBins` is lowered by the tests only.
PorousComplex packPorousComplex(const PorousDual& dual, const SeedSet& seeds,
                                PorousCells cells, PorousMode mode,
                                const CancelToken* cancel = nullptr,
                                double maxBins = kPorousMaxBins);

struct PorousQueryStats {
  long walkSteps = 0;     // seed or tet walk steps, summed
  long maxWalkSteps = 0;
  long walkCapHits = 0;   // walks stopped by kPorousMaxWalk (none expected)
  long candidates = 0;    // struts: bin items visited
};

// The longest walk before the query gives up (and returns 0, solid).
constexpr int kPorousMaxWalk = 1024;

// The unsigned distance from p to the complex, smoothed by `smooth` = k >= 0
// (k = 0: the exact distance):
//   - Voronoi walls: the nearest seed s1 (a greedy walk on the Delaunay graph
//     from the bin's start seed), then the distances to the bisector planes
//     of its neighbours, smin-folded, clamped at 0;
//   - Delaunay walls: the tet holding p (a visibility walk from the bin's
//     start tet), then the distances to its four face planes, smin-folded,
//     clamped at 0; outside the seeds' hull, 0 (solid: continuous with the
//     faces, 1-Lipschitz, and the pad keeps the hull outside the volume);
//   - struts: the segments within B in the 3 x 3 x 3 bins around p's bin,
//     smin-folded, then min(value, B) -- exact below B, B beyond -- clamped
//     at 0 as the walls are (a smoothed junction would dip below 0).
// The fold is porousSmoothFold's: ascending distance, ties by index.
double porousDistance(const PorousComplex& c, const Vector3& p, double smooth,
                      PorousQueryStats* stats = nullptr);

// --- The pieces the query and its brute-force test share ---------------------

// The quadratic smooth minimum: min(a, b) - h^2 k / 4, h = max(k - |a-b|, 0)/k.
inline double porousSmin(double a, double b, double k) {
  if (!(k > 0.0)) return std::min(a, b);
  const double h = std::max(k - std::fabs(a - b), 0.0) / k;
  return std::min(a, b) - h * h * k * 0.25;
}

// One primitive's distance, with its index for the tie order.
struct PorousCandidate {
  double d;
  std::int32_t index;
};

// The smooth minimum of the candidates (any order, any duplicates of one
// index, which count once): the minimum m, then porousSmin folded over the
// candidates with d < m + k in ascending (d, index) order. Ascending, the
// accumulator never exceeds m, so a primitive at m + k or beyond would leave
// it unchanged: the result depends only on the primitives within k of the
// minimum, is a continuous symmetric function of their distances (swapping
// two equal ones changes nothing), and each step is a convex combination of
// gradients, so it stays 1-Lipschitz. Reorders `c`. Empty: +infinity.
double porousSmoothFold(std::vector<PorousCandidate>& c, double k);

// The distance from p to the bisector plane of seeds s and t, positive on
// s's side: (|t - s|^2 / 2 - (p - s).(t - s)) / |t - s|.
inline double porousBisectorDistance(const Vector3& p, const Vector3& s,
                                     const Vector3& t) {
  const Vector3 e = t - s;
  const double l2 = e.x * e.x + e.y * e.y + e.z * e.z;
  const Vector3 q = p - s;
  return (0.5 * l2 - (q.x * e.x + q.y * e.y + q.z * e.z)) / std::sqrt(l2);
}

// The distance from p to the segment [a, b] (a point when a == b).
inline double porousSegmentDistance(const Vector3& p, const Vector3& a,
                                    const Vector3& b) {
  const Vector3 e = b - a;
  const Vector3 q = p - a;
  const double l2 = e.x * e.x + e.y * e.y + e.z * e.z;
  double t = l2 > 0.0 ? (q.x * e.x + q.y * e.y + q.z * e.z) / l2 : 0.0;
  t = std::min(std::max(t, 0.0), 1.0);
  const Vector3 r = q - t * e;
  return std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
}

// The signed distance of p to a packed plane (n, d): n.p + d.
inline double porousPlaneDistance(const float* plane, const Vector3& p) {
  return double(plane[0]) * p.x + double(plane[1]) * p.y + double(plane[2]) * p.z +
         double(plane[3]);
}

inline Vector3 porousPackedPoint(const float* xyz) {
  return Vector3{double(xyz[0]), double(xyz[1]), double(xyz[2])};
}

} // namespace internal
} // namespace dualc
