#pragma once

#include "dualc/hermite_octree.h"
#include "dualc/progress.h"

#include <memory>
#include <tuple>
#include <vector>

namespace geometrycentral {
namespace surface {
class SurfaceMesh;
class VertexPositionGeometry;
} // namespace surface
} // namespace geometrycentral

namespace dualc {

struct ContourerParams {
  double qefRegularization   = 0.1;    // pseudo-inverse threshold passed to QefSolver. Matches Nick Gildea's reference.
  double simplificationError = 0.0;    // geometric-QEF-energy threshold for cell collapse; 0 disables. See below.
  bool   clampVertexToCell   = true;   // clamp QEF minimum back inside the cell when it drifts outside.
  double clampToleranceCells = 1.0;    // when clamping, allow drift up to this many cell widths before falling back to mass point.
  // Manifold Dual Contouring: place one QEF vertex per surface component in
  // a cube instead of always one per cell. Fixes the "pinch across one cell"
  // class of non-manifold edges (Schaefer/Ju/Warren 2007). When false the
  // contourer falls back to the pre-MDC single-vertex behaviour.
  bool   manifoldDC          = true;
};

// Bottom-up adaptive cell collapse (Ju/Schaefer/Warren simplifyOctree).
// For each internal node whose 8 children are all surface leaves (real or
// pseudo-leaves from prior collapses), try to merge them into a single
// pseudo-leaf carrying the merged QEF data. Collapse is permitted only when
// (a) the merged QEF energy at the solved vertex is at most
//     `params.simplificationError` AND
// (b) the topology-safe test passes (no sign change on any of the parent's
//     6 internal edges, and no hidden double-crossing on any of the parent's
//     12 outer edges, and no saddle face), AND
// (c) the merged sign configuration is still a single surface component.
//
// The error in (a) is the geometric QEF energy
//   sum over merged samples of (n_i . (x - p_i))^2,
// i.e. a summed squared distance in length^2 units. It scales with the number
// of merged samples, so a threshold tuned at one octree depth is not directly
// transferable to another.
//
// `params.qefRegularization` is used for the merged solve, exactly as it is
// for per-leaf solves.
//
// Caveat: the energy's `b^T b` term is accumulated in float and is not
// re-centred on the mass point, so far from the world origin it loses
// precision (the result is clamped at zero rather than allowed to go
// negative). Trustworthy near the origin; noisy in site coordinates.
//
// Mutates the octree in place. A `simplificationError` of 0 is a no-op.
// `cancel`, when non-null, is polled at every internal node; the pass throws
// Cancelled once it is requested (the octree is then partially collapsed and
// must be discarded).
void simplifyHermiteOctree(HermiteOctree& octree, const ContourerParams& params,
                           const CancelToken* cancel = nullptr);

// Convenience overload: collapse with the given error threshold and otherwise
// default contourer parameters.
void simplifyHermiteOctree(HermiteOctree& octree, double errorThreshold);

// Returns: output mesh, embedded geometry, and per-vertex unit normals
// (averaged from the QEF input normals at each cell, suitable for smooth
// shading). The normals vector is index-aligned with the mesh's vertices.
//
// Run `simplifyHermiteOctree` separately before this if you want adaptive
// collapse; the contourer itself only consumes the octree as-is.
//
// `diag`, when non-null, receives the contouring fields of Diagnostics --
// crucially Diagnostics::emptyContour, which is the ONLY way to distinguish
// "the octree carried no surface" (the returned mesh is then a synthesized
// placeholder triangle, because geometry-central rejects an empty polygon
// list) from "the surface really was one triangle". It also receives the
// output vertex / triangle counts and the output edge-manifoldness tally.
//
// Cost: passing a Diagnostics adds one O(E) hash pass over the emitted
// triangles to tally the edges. Nothing is computed when `diag` is null, so
// the default path -- including the per-tile streaming export -- is
// unaffected.
//
// `cancel`, when non-null, is polled per leaf in the QEF pre-solve and at
// every internal node of the traversal; the call throws Cancelled once it is
// requested. `progress`, when non-null, receives Stage::Contour reports on
// the calling thread only, with total = 2 * leafCount: the first half counts
// solved leaves (at most every ~100 ms), the second half leaves first touched
// by the traversal (every 4096), and the last report is (2N, 2N). Neither
// hook changes the mesh; see dualc/progress.h.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
           std::vector<Vector3>>
contourHermiteOctree(const HermiteOctree& octree, const ContourerParams& params,
                     Diagnostics* diag = nullptr,
                     const CancelToken* cancel = nullptr,
                     ProgressSink* progress = nullptr);

} // namespace dualc
