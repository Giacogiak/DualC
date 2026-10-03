#pragma once

#include "dualc/types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace geometrycentral {
namespace surface {
class SurfaceMesh;
class VertexPositionGeometry;
} // namespace surface
} // namespace geometrycentral

namespace dualc {
namespace internal {

// Voronoi region a closest-point-on-triangle query landed in. Carried out
// of the BVH walk so the PSEUDONORMAL path knows which LUT to consult.
// At namespace scope rather than nested in MeshBVH so the anonymous-namespace
// helpers in mesh_bvh.cpp can name it directly.
enum class TriRegion : std::uint8_t {
  FACE_INTERIOR,
  EDGE_AB, EDGE_BC, EDGE_CA,  // local edges (corner i, corner (i+1) % 3)
  VERT_A,  VERT_B,  VERT_C    // local corners 0, 1, 2
};

// Triangle-mesh BVH backed by nanort.
//
// The constructor copies the input mesh into packed float arrays and builds
// a BVH over them. The BVH owns its own data; the input mesh / geometry are
// not referenced after construction.
class MeshBVH {
public:
  // `interpolateNormals` controls what `segmentFirstHit` returns as the
  // edge-crossing normal: barycentric-interpolated input vertex normals
  // (smooth, default) or the hit triangle's face normal (sharp).
  //
  // `buildWindingTree` opts into the per-BVH-node multipole aggregates that
  // `windingNumberFast` consults. O(N) extra work at construction and a few
  // structs per BVH node of memory; off by default since most callers never
  // ask for a generalized winding number.
  MeshBVH(geometrycentral::surface::SurfaceMesh& mesh,
          geometrycentral::surface::VertexPositionGeometry& geometry,
          bool interpolateNormals = true,
          bool buildWindingTree   = false);
  ~MeshBVH();

  MeshBVH(const MeshBVH&)            = delete;
  MeshBVH& operator=(const MeshBVH&) = delete;

  // Returns true on hit. Reports the closest hit on the segment [a, b].
  // outP is the hit point, outN is the unit face normal of the hit triangle,
  // outTri is the triangle index.
  bool segmentFirstHit(const Vector3& a, const Vector3& b,
                       Vector3& outP, Vector3& outN, int& outTri) const;

  // Counts the number of triangle intersections along a ray from `origin` in
  // direction `direction` (need not be unit). Used by SignOracle for parity
  // tests on closed meshes. Returns 0 if there are no triangles.
  //
  // Exact and uncapped: a single all-hits BVH walk, with no advance epsilon
  // and so no thinnest-resolvable-feature limit beyond float precision at the
  // hit distance. A ray landing exactly on a shared edge or a corner is
  // counted once, by nanort's watertight triangle test.
  int countRayHits(const Vector3& origin, const Vector3& direction) const;

  // Returns true iff the BVH contains at least one triangle whose AABB
  // overlaps the given cell. Conservative (BVH-leaf-AABB based, not
  // per-triangle), so it may return true when no triangle actually touches
  // the cell -- fine for refinement decisions. Cost: O(log N) average.
  bool cellOverlapsAABB(const BBox& cell) const;

  // Finds the closest point on the mesh surface to q. Returns false only
  // when the mesh has no triangles. outP = closest surface point, outN =
  // outward unit face normal of the owning triangle, outTri = triangle
  // index. Branch-and-bound DFS over the BVH; cost ~O(log N) average.
  bool closestPoint(const Vector3& q, Vector3& outP, Vector3& outN,
                    int& outTri) const;

  // Like closestPoint, but outN is the Bærentzen-Aanæs angle-weighted
  // *pseudonormal* of the surface element the closest point lies on:
  //   - face interior  -> the triangle's face normal
  //   - manifold edge  -> mean of the two adjacent face normals
  //   - vertex         -> angle-weighted sum of incident face normals
  // Used by the PSEUDONORMAL sign method to classify inside/outside via
  // dot(q - outP, outN) < 0. Non-manifold / boundary regions silently
  // degrade to the face normal -- correct only for watertight oriented input.
  bool closestPointWithPseudoNormal(const Vector3& q, Vector3& outP,
                                    Vector3& outPseudoNormal,
                                    int& outTri) const;

  // Exact generalized winding number at q: (1/4π) Σ_T Ω_T, the sum of the
  // signed solid angles every triangle subtends at q. For a watertight,
  // consistently oriented mesh this is ~1 inside and ~0 outside; for broken
  // input (open shells, soup, self-intersections) it degrades smoothly.
  // O(N) per query -- the ground-truth oracle that windingNumberFast is
  // validated against. Returns 0 when the mesh has no triangles.
  double windingNumber(const Vector3& q) const;

  // Hierarchically accelerated generalized winding number. Walks the BVH:
  // a subtree far from q (centroid distance > beta * node radius) contributes
  // a first-order multipole expansion, a near subtree is summed exactly.
  // O(log N) average. `beta` is the accuracy knob (larger -> more exact work,
  // tighter approximation). Requires the winding tree -- pass
  // buildWindingTree = true to the constructor -- and returns 0 otherwise.
  double windingNumberFast(const Vector3& q, double beta = 2.0) const;

  // Read-only access to the packed triangle list, used by SignOracle.
  std::size_t numTriangles() const { return numTris_; }

  // Input-mesh edge topology, tallied for free while
  // `buildPseudoNormalTopology` builds the pseudonormal tables: edges
  // incident to exactly one triangle (open boundary) and to three or more
  // (non-manifold junction). Both are zero on watertight manifold input --
  // exactly the precondition the PSEUDONORMAL sign path assumes and silently
  // degrades on. Surfaced through dualc::Diagnostics.
  std::size_t numBoundaryEdges() const;
  std::size_t numNonManifoldEdges() const;
  Vector3 triVertex(std::size_t triIdx, int corner) const;
  Vector3 triFaceNormal(std::size_t triIdx) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  struct ClosestHit {
    Vector3   point;
    int       tri = -1;
    TriRegion region = TriRegion::FACE_INTERIOR;
  };

  // Shared branch-and-bound DFS used by both closest-point public methods.
  // Returns false iff the mesh has no triangles or the BVH is empty.
  static bool findClosest(const Impl& impl, std::size_t numTris,
                          const Vector3& q, ClosestHit& out);

  // Build the per-edge global topology + the vertex / edge pseudonormal
  // lookup tables that PSEUDONORMAL signs consult. Called once at
  // construction; reads `impl.indices` + `impl.positions`, writes the
  // remaining LUT fields on `impl`.
  static void buildPseudoNormalTopology(Impl& impl, std::size_t numTris,
                                        std::size_t numVerts);

  // Build the per-BVH-node multipole aggregates that windingNumberFast
  // consults. Called once at construction when buildWindingTree is set;
  // reads the built nanort BVH + `impl.positions` / `impl.indices`.
  static void buildWindingNumberTree(Impl& impl);

  // Cached count so we can answer numTriangles() without dereferencing impl_.
  std::size_t numTris_ = 0;
  bool        interpolateNormals_ = true;
};

} // namespace internal
} // namespace dualc
