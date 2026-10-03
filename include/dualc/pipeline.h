#pragma once

#include "dualc/contourer.h"
#include "dualc/sampler.h"

namespace dualc {

// Returns: output mesh, geometry, and per-vertex unit normals (index-aligned
// with the mesh's vertices, suitable for smooth-shaded export).
//
// `dualContourMesh` is a thin wrapper over `sampleMeshToHermiteOctree` (which
// is the library's single MeshSource construction site, so every
// `SamplerParams` field -- `signMethod` included -- reaches the sampler),
// followed by the same optional-collapse-then-contour tail as
// `dualContourField`.
//
// `diag`, when non-null, is filled in completely: the input-mesh facts from
// the sampler and the output facts from the contourer. It is the only way to
// learn that the result came out of a fallback -- an empty contour returns a
// placeholder triangle, and an unusable `bounds()` silently becomes the unit
// cube. `Diagnostics::anyIssue()` summarises. See dualc/types.h.
//
// `cancel` and `progress` (dualc/progress.h) are forwarded to every stage:
// a requested token makes the call throw Cancelled at the next checkpoint
// (sub-second on any input); a sink sees Stage::Sample then Stage::Contour,
// each ending at (total, total), on the calling thread only. With both null
// the output is bit-identical to a call without them.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
           std::vector<Vector3>>
dualContourMesh(
    geometrycentral::surface::SurfaceMesh& mesh,
    geometrycentral::surface::VertexPositionGeometry& geometry,
    const SamplerParams&   samplerParams,
    const ContourerParams& contourerParams,
    Diagnostics*           diag = nullptr,
    const CancelToken*     cancel = nullptr,
    ProgressSink*          progress = nullptr);

// Sample an arbitrary implicit field, optionally simplify, and contour it in
// one call. The mesh overload above shares this function's collapse+contour
// tail, but builds its octree through the mesh sampler rather than here.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>,
           std::vector<Vector3>>
dualContourField(
    const ImplicitField&   field,
    const SamplerParams&   samplerParams,
    const ContourerParams& contourerParams,
    Diagnostics*           diag = nullptr,
    const CancelToken*     cancel = nullptr,
    ProgressSink*          progress = nullptr);

} // namespace dualc
