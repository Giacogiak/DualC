#include "dualc/implicit.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "internal/distance_grid.h"
#include "internal/mesh_bvh.h"
#include "internal/parallel.h"
#include "internal/sign_oracle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace dualc {

namespace {

// Unpadded axis-aligned bounding box of the mesh's vertices. Mirrors the
// loop in sampler.cpp's autoFitBounds so the mesh sampling path stays
// bit-identical; padding is applied later, by the sampler.
BBox meshAABB(geometrycentral::surface::SurfaceMesh& mesh,
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
  return b;  // invalid (min > max) when the mesh has no vertices
}

} // namespace

struct MeshSource::Impl {
  // DECLARATION ORDER IS LOAD-BEARING: `oracle` stores a non-owning
  // `const MeshBVH&` (src/internal/sign_oracle.h) bound to `bvh` below, and
  // members are constructed in declaration order regardless of the order of the
  // initializer list. `bvh` must therefore stay declared first; putting
  // `oracle` first binds its reference to an unconstructed object, which is
  // undefined behaviour that still compiles and usually still appears to work.
  // Reordering the declarations alone trips -Wreorder on GCC/Clang, but MSVC --
  // this project's primary compiler -- has no equivalent, and reordering the
  // initializer list to match silences the warning everywhere. So the comment
  // is the guard.
  internal::MeshBVH    bvh;
  internal::SignOracle oracle;
  BBox                 aabb;

  Impl(geometrycentral::surface::SurfaceMesh& mesh,
       geometrycentral::surface::VertexPositionGeometry& geometry,
       bool       interpolateNormals,
       SignMethod signMethod)
      : bvh(mesh, geometry, interpolateNormals,
            /*buildWindingTree=*/
            signMethod == SignMethod::GENERALIZED_WINDING_NUMBER),
        oracle(bvh, signMethod),
        aabb(meshAABB(mesh, geometry)) {}
};

MeshSource::MeshSource(geometrycentral::surface::SurfaceMesh& mesh,
                       geometrycentral::surface::VertexPositionGeometry& geometry,
                       bool       interpolateNormals,
                       SignMethod signMethod)
    : impl_(std::make_unique<Impl>(mesh, geometry, interpolateNormals,
                                   signMethod)) {}

MeshSource::~MeshSource() = default;

double MeshSource::valueAt(const Vector3& p) const {
  Vector3 cp, cn;
  int     tri;
  if (!impl_->bvh.closestPoint(p, cp, cn, tri)) {
    return std::numeric_limits<double>::infinity();  // empty mesh: all outside
  }
  const double dist = (p - cp).norm();
  return impl_->oracle.isInside(p) ? -dist : dist;
}

Vector3 MeshSource::gradientAt(const Vector3& p) const {
  Vector3 cp, cn;
  int     tri;
  if (!impl_->bvh.closestPoint(p, cp, cn, tri)) {
    return Vector3{0.0, 0.0, 1.0};
  }
  const bool    inside = impl_->oracle.isInside(p);
  const Vector3 d  = p - cp;
  const double  dn = d.norm();
  if (dn > 1e-9) {
    // grad points toward increasing value (outward). p-cp points outward for
    // an outside p and inward for an inside p, so flip the sign when inside.
    const Vector3 g = d / dn;
    return inside ? (g * -1.0) : g;
  }
  // p sits on the surface: the triangle's outward normal is the gradient.
  return cn;
}

BBox MeshSource::bounds() const {
  return impl_->aabb;
}

bool MeshSource::isInside(const Vector3& p) const {
  return impl_->oracle.isInside(p);
}

bool MeshSource::edgeHit(const Vector3& a, const Vector3& b,
                         Vector3& outP, Vector3& outN) const {
  int tri;
  return impl_->bvh.segmentFirstHit(a, b, outP, outN, tri);
}

bool MeshSource::cellOverlaps(const BBox& cell) const {
  return impl_->bvh.cellOverlapsAABB(cell);
}

bool MeshSource::closestSurfacePoint(const Vector3& q,
                                     Vector3& outP, Vector3& outN) const {
  int tri;
  return impl_->bvh.closestPoint(q, outP, outN, tri);
}

std::size_t MeshSource::triangleCount() const {
  return impl_->bvh.numTriangles();
}

std::size_t MeshSource::boundaryEdgeCount() const {
  return impl_->bvh.numBoundaryEdges();
}

std::size_t MeshSource::nonManifoldEdgeCount() const {
  return impl_->bvh.numNonManifoldEdges();
}

FieldPtr MeshSource::bakeToGrid(const BBox& region,
                                const Vector3i& resolution,
                                double bandWidth) const {
  if (resolution.x < 2 || resolution.y < 2 || resolution.z < 2) {
    throw std::invalid_argument(
        "MeshSource::bakeToGrid: resolution must be >= 2 on every axis");
  }

  const int rx = resolution.x, ry = resolution.y, rz = resolution.z;
  const Vector3 ext = region.extent();
  const Vector3 cell{ext.x / (rx - 1), ext.y / (ry - 1), ext.z / (rz - 1)};
  const double maxCell = std::max({cell.x, cell.y, cell.z, 1e-12});
  // Correctness floor: the band must be a closed shell at least ~1 voxel
  // thick, or the far-field sign flood could leak across the surface.
  if (bandWidth < 1.5 * maxCell) bandWidth = 1.5 * maxCell;

  const std::size_t n = static_cast<std::size_t>(rx) *
                        static_cast<std::size_t>(ry) *
                        static_cast<std::size_t>(rz);

  // 1. Band detection: a voxel is in the band if a triangle lies within
  //    `bandWidth` of it. Cheap BVH-AABB overlap, no SDF. Parallel z-slabs.
  std::vector<char> band(n, 0);
  internal::parallelFor(
      static_cast<std::size_t>(rz), internal::resolveThreadCount(0),
      [&](std::size_t izu) {
        const int iz = static_cast<int>(izu);
        const double z = region.min.z + cell.z * iz;
        for (int iy = 0; iy < ry; ++iy) {
          const double y = region.min.y + cell.y * iy;
          for (int ix = 0; ix < rx; ++ix) {
            const double x = region.min.x + cell.x * ix;
            BBox probe;
            probe.min = Vector3{x - bandWidth, y - bandWidth, z - bandWidth};
            probe.max = Vector3{x + bandWidth, y + bandWidth, z + bandWidth};
            if (impl_->bvh.cellOverlapsAABB(probe)) {
              band[internal::gridIndex(resolution, ix, iy, iz)] = 1;
            }
          }
        }
      });

  // 2. Exact signed distance on the band only. Parallel over the band list.
  std::vector<std::size_t> bandIdx;
  for (std::size_t i = 0; i < n; ++i)
    if (band[i]) bandIdx.push_back(i);

  std::vector<float> value(n, 0.0f);
  std::vector<float> bandAbs(n, 0.0f);
  const std::size_t plane = static_cast<std::size_t>(rx) *
                            static_cast<std::size_t>(ry);
  internal::parallelFor(
      bandIdx.size(), internal::resolveThreadCount(0), [&](std::size_t k) {
        const std::size_t i = bandIdx[k];
        const int iz  = static_cast<int>(i / plane);
        const int rem = static_cast<int>(i % plane);
        const int iy  = rem / rx;
        const int ix  = rem % rx;
        const Vector3 lp{region.min.x + cell.x * ix,
                         region.min.y + cell.y * iy,
                         region.min.z + cell.z * iz};
        const double d = this->valueAt(lp);
        value[i]   = static_cast<float>(d);
        bandAbs[i] = static_cast<float>(std::abs(d));
      });

  // 3. Sign of the far field: up to 3 exact inside/outside queries per
  //    connected component of non-band voxels. A solid part has a handful of
  //    components (outside, interior, one per cavity); a porous/lattice mesh has
  //    one per pore, so the count follows the topology, not the resolution.
  //    Region-agnostic: `region` may be padded, flush with the mesh AABB, partly
  //    overlapping, or strictly inside the solid. 4. magnitude by chamfer growth.
  std::vector<signed char> farSign;
  internal::floodFarSign(resolution, band, farSign, [&](std::size_t i) {
    const int iz  = static_cast<int>(i / plane);
    const int rem = static_cast<int>(i % plane);
    const int iy  = rem / rx;
    const int ix  = rem % rx;
    return impl_->oracle.isInside(Vector3{region.min.x + cell.x * ix,
                                          region.min.y + cell.y * iy,
                                          region.min.z + cell.z * iz});
  });
  std::vector<float> dist;
  internal::chamferGrow(resolution, cell, band, bandAbs, dist);

  // 5. Assemble: band voxels keep their exact value, the rest get
  //    sign * grown-distance.
  std::vector<float> samples(n);
  for (std::size_t i = 0; i < n; ++i) {
    samples[i] = band[i] ? value[i]
                         : static_cast<float>(farSign[i]) * dist[i];
  }

  return std::make_shared<GridField>(region, resolution, std::move(samples));
}

} // namespace dualc
