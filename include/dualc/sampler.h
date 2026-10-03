#pragma once

#include "dualc/hermite_octree.h"
#include "dualc/progress.h"

#include <cstdint>
#include <optional>

namespace geometrycentral {
namespace surface {
class SurfaceMesh;
class VertexPositionGeometry;
} // namespace surface
} // namespace geometrycentral

namespace dualc {

class ImplicitField;

struct SamplerParams {
  int  maxDepth          = 7;
  int  minDepth          = 3;
  std::optional<BBox> rootBounds{};        // unset => auto-fit padded AABB
  double padFraction     = 0.05;
  SignMethod signMethod  = SignMethod::WINDING_NUMBER;
  // When true (default), Hermite-edge crossings record a barycentric blend
  // of the input mesh's per-vertex normals at the hit point -- preserves
  // smooth shading on organic meshes. When false, the hit triangle's
  // geometric face normal is recorded instead -- preserves sharp 90deg
  // corners on CAD-style inputs (the cube comes out perfectly axis-aligned).
  bool interpolateNormals = true;
  // Worker threads for the octree build. 0 => use all hardware threads. The
  // build is split into independent subtrees, so the octree (and the mesh it
  // contours to) is bit-identical regardless of this value.
  unsigned numThreads = 0;
};

// `diag`, when non-null, receives the input-mesh facts (triangle count, open
// and non-manifold edge counts) alongside the sampling fields below. Those
// input fields are set HERE and nowhere else: this is the library's only
// MeshSource construction site, so it is the only place that can see them.
HermiteOctree sampleMeshToHermiteOctree(
    geometrycentral::surface::SurfaceMesh& mesh,
    geometrycentral::surface::VertexPositionGeometry& geometry,
    const SamplerParams& params,
    Diagnostics* diag = nullptr,
    const CancelToken* cancel = nullptr,
    ProgressSink* progress = nullptr);

// Sample an arbitrary implicit field into a Hermite octree. The mesh overload
// above is a thin wrapper that builds a MeshSource and calls this.
//
// When `params.rootBounds` is unset the root box is auto-fit from
// `field.bounds()` padded by `params.padFraction`; a field with no finite
// bounds therefore requires an explicit `rootBounds`.
// `diag`, when non-null, receives Diagnostics::boundsFallback and
// Diagnostics::gridBoundsExceeded. Both are decided before the parallel
// build starts and written by the calling thread, so passing a Diagnostics
// changes neither the octree nor its bit-identical-across-thread-counts
// guarantee.
//
// `cancel`, when non-null, is polled at every internal node of the build (a
// relaxed load; at most 8 leaves are built between two polls) and the call
// throws Cancelled once it is requested. `progress`, when non-null, receives
// Stage::Sample reports on the calling thread only: (0, n) before the
// parallel build over the n frontier subtrees, (done, n) at most every
// ~100 ms while it runs, (n, n) after. Neither hook changes the octree; see
// dualc/progress.h.
HermiteOctree sampleFieldToHermiteOctree(
    const ImplicitField& field,
    const SamplerParams& params,
    Diagnostics* diag = nullptr,
    const CancelToken* cancel = nullptr,
    ProgressSink* progress = nullptr);

} // namespace dualc
