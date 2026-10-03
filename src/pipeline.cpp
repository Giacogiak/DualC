#include "dualc/pipeline.h"

#include "dualc/implicit.h"
#include "dualc/sampler.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

namespace dualc {

namespace {

// Shared tail of both public entry points: optional adaptive collapse, then
// contour. Factored out so the two entry points differ only in how they build
// the octree.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
           std::vector<Vector3>>
simplifyAndContour(HermiteOctree& octree, const ContourerParams& params,
                   Diagnostics* diag, const CancelToken* cancel,
                   ProgressSink* progress) {
  if (params.simplificationError > 0.0) {
    simplifyHermiteOctree(octree, params, cancel);
  }
  return contourHermiteOctree(octree, params, diag, cancel, progress);
}

} // namespace

std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
           std::vector<Vector3>>
dualContourField(
    const ImplicitField&   field,
    const SamplerParams&   samplerParams,
    const ContourerParams& contourerParams,
    Diagnostics*           diag,
    const CancelToken*     cancel,
    ProgressSink*          progress) {

  HermiteOctree octree =
      sampleFieldToHermiteOctree(field, samplerParams, diag, cancel, progress);
  return simplifyAndContour(octree, contourerParams, diag, cancel, progress);
}

std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
           std::vector<Vector3>>
dualContourMesh(
    geometrycentral::surface::SurfaceMesh& mesh,
    geometrycentral::surface::VertexPositionGeometry& geometry,
    const SamplerParams&   samplerParams,
    const ContourerParams& contourerParams,
    Diagnostics*           diag,
    const CancelToken*     cancel,
    ProgressSink*          progress) {

  // Delegate to sampleMeshToHermiteOctree rather than building a MeshSource
  // here. Exactly one place in the library now constructs a MeshSource from a
  // SamplerParams, so the two entry points cannot drift apart -- an earlier
  // version of this function passed only `interpolateNormals` and silently
  // dropped `signMethod`, so a caller asking for GENERALIZED_WINDING_NUMBER
  // through this overload got ray-stabbing parity instead.
  HermiteOctree octree = sampleMeshToHermiteOctree(mesh, geometry, samplerParams,
                                                   diag, cancel, progress);
  return simplifyAndContour(octree, contourerParams, diag, cancel, progress);
}

} // namespace dualc
