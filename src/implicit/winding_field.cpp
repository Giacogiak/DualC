#include "dualc/implicit.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "internal/mesh_bvh.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dualc {

namespace {

// Axis-aligned bounding box of the mesh vertices, padded outward by 10% of
// its diagonal. The GWN 0.5-isosurface can bow slightly past an open
// boundary (the smooth cap), so the field's reported bounds must be a little
// looser than the raw mesh AABB or the sampler's root box would clip it.
BBox paddedMeshAABB(geometrycentral::surface::SurfaceMesh& mesh,
                    geometrycentral::surface::VertexPositionGeometry& geometry) {
  BBox b;
  const double inf = std::numeric_limits<double>::infinity();
  b.min = Vector3{ inf,  inf,  inf};
  b.max = Vector3{-inf, -inf, -inf};
  for (auto v : mesh.vertices()) {
    const Vector3 p = geometry.inputVertexPositions[v];
    b.min.x = std::min(b.min.x, p.x);
    b.min.y = std::min(b.min.y, p.y);
    b.min.z = std::min(b.min.z, p.z);
    b.max.x = std::max(b.max.x, p.x);
    b.max.y = std::max(b.max.y, p.y);
    b.max.z = std::max(b.max.z, p.z);
  }
  if (!b.isValid()) return b;  // empty mesh
  const double pad = 0.1 * b.extent().norm();
  b.min = b.min - Vector3{pad, pad, pad};
  b.max = b.max + Vector3{pad, pad, pad};
  return b;
}

} // namespace

struct WindingNumberField::Impl {
  internal::MeshBVH bvh;
  BBox              aabb;
  double            fdEps;  // finite-difference step for gradientAt

  Impl(geometrycentral::surface::SurfaceMesh& mesh,
       geometrycentral::surface::VertexPositionGeometry& geometry)
      : bvh(mesh, geometry, /*interpolateNormals=*/true,
            /*buildWindingTree=*/true),
        aabb(paddedMeshAABB(mesh, geometry)) {
    const double diag = aabb.isValid() ? aabb.extent().norm() : 1.0;
    fdEps = 1e-4 * std::max(diag, 1e-9);
  }
};

WindingNumberField::WindingNumberField(
    geometrycentral::surface::SurfaceMesh& mesh,
    geometrycentral::surface::VertexPositionGeometry& geometry)
    : impl_(std::make_unique<Impl>(mesh, geometry)) {}

WindingNumberField::~WindingNumberField() = default;

double WindingNumberField::valueAt(const Vector3& p) const {
  // Pseudo-SDF: negative inside (w > 0.5), positive outside, zero on the
  // 0.5-isosurface. Sign-correct but not metric.
  return 0.5 - impl_->bvh.windingNumberFast(p);
}

bool WindingNumberField::isInside(const Vector3& p) const {
  return impl_->bvh.windingNumberFast(p) > 0.5;
}

Vector3 WindingNumberField::gradientAt(const Vector3& p) const {
  // Straddle-aware central finite difference. The winding number jumps by 1
  // across the input triangles, so a central difference that spans real
  // geometry is garbage; when the two probes disagree by more than the 0.5
  // jump, fall back to a one-sided difference on whichever side does not
  // cross the jump. In cap regions w is smooth and the central path is taken.
  const double e  = impl_->fdEps;
  const double v0 = valueAt(p);

  auto partial = [&](const Vector3& off) -> double {
    const double vp = valueAt(p + off);
    const double vm = valueAt(p - off);
    if (std::abs(vp - vm) <= 0.5) return (vp - vm) / (2.0 * e);  // central
    if (std::abs(vp - v0) <= 0.5) return (vp - v0) / e;          // forward
    if (std::abs(vm - v0) <= 0.5) return (v0 - vm) / e;          // backward
    return (vp - vm) / (2.0 * e);                                // degenerate
  };

  return Vector3{partial(Vector3{e, 0.0, 0.0}),
                 partial(Vector3{0.0, e, 0.0}),
                 partial(Vector3{0.0, 0.0, e})};
}

bool WindingNumberField::edgeHit(const Vector3& a, const Vector3& b,
                                 Vector3& outP, Vector3& outN) const {
  // Where the mesh has geometry the GWN 0.5-isosurface coincides exactly
  // with the triangles (w jumps 0->1 across them), so take the crossing and
  // its normal straight from the BVH -- sharp and correct. Off-surface w is
  // a near-step function, so a finite-difference gradient there would be
  // meaningless; this avoids it entirely on the geometry-coincident part.
  int tri;
  if (impl_->bvh.segmentFirstHit(a, b, outP, outN, tri)) return true;

  // No triangle on this segment: the crossing is on the smooth GWN cap that
  // seals an open boundary. There w is differentiable, so the base-class
  // root finder on valueAt (with the straddle-aware gradient) is correct.
  return ImplicitField::edgeHit(a, b, outP, outN);
}

BBox WindingNumberField::bounds() const {
  return impl_->aabb;
}

bool WindingNumberField::cellOverlaps(const BBox& cell) const {
  // Real geometry in the cell -> the surface is definitely there.
  if (impl_->bvh.cellOverlapsAABB(cell)) return true;

  // No triangles in the cell, but the GWN 0.5-isosurface (a smooth cap over
  // an open boundary) may still pass through it. A pure BVH-overlap test
  // would prune the cap and leave the hole unsealed -- so also test the 8
  // corner signs for disagreement.
  bool first = false, have = false;
  for (int c = 0; c < 8; ++c) {
    const Vector3 corner{(c & 1) ? cell.max.x : cell.min.x,
                         (c & 2) ? cell.max.y : cell.min.y,
                         (c & 4) ? cell.max.z : cell.min.z};
    const bool in = isInside(corner);
    if (!have) {
      first = in;
      have  = true;
    } else if (in != first) {
      return true;
    }
  }
  return false;
}

FieldPtr windingNumberField(
    geometrycentral::surface::SurfaceMesh& mesh,
    geometrycentral::surface::VertexPositionGeometry& geometry) {
  return std::make_shared<WindingNumberField>(mesh, geometry);
}

} // namespace dualc
