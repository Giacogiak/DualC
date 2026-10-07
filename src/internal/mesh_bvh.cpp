#include "mesh_bvh.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "nanort/nanort.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dualc {
namespace internal {

using NanortMesh   = nanort::TriangleMesh<float>;
using NanortPred   = nanort::TriangleSAHPred<float>;
using NanortIsect  = nanort::TriangleIntersector<float>;
using NanortBVH    = nanort::BVHAccel<float, NanortMesh, NanortPred, NanortIsect>;

namespace {

struct TriClosest {
  Vector3   point;
  TriRegion region;
};

int cornerOf(TriRegion r) {
  switch (r) {
    case TriRegion::VERT_A: return 0;
    case TriRegion::VERT_B: return 1;
    case TriRegion::VERT_C: return 2;
    default: return -1;
  }
}

int edgeOf(TriRegion r) {
  switch (r) {
    case TriRegion::EDGE_AB: return 0;
    case TriRegion::EDGE_BC: return 1;
    case TriRegion::EDGE_CA: return 2;
    default: return -1;
  }
}

} // namespace

struct MeshBVH::Impl {
  std::vector<float>        positions;       // 3 floats per vertex
  std::vector<unsigned int> indices;         // 3 indices per triangle
  std::vector<float>        vertexNormals;   // 3 floats per vertex (corner-angle weighted)
  std::unique_ptr<NanortMesh>  nanortMesh;
  std::unique_ptr<NanortPred>  sahPred;
  NanortBVH                    bvh;

  // --- Tier 2 #5 topology for PSEUDONORMAL signs ---
  //
  // Per-triangle local-edge -> global edge index (3 entries per triangle).
  // Local edge i spans triangle corners (i, (i+1) % 3). -1 marks an edge
  // whose triangulation gave a degenerate or out-of-range neighbour count;
  // the PSEUDONORMAL query then falls back to face normal there.
  std::vector<int>      triEdgeGlobal;
  std::size_t           numEdges = 0;

  // Edge-manifoldness tally, a by-product of the same edge book: edges with
  // exactly one incident triangle, and with three or more. The `faceCount`
  // it reads was already computed to decide which edges get a pseudonormal;
  // only the counters are new.
  std::size_t           numBoundaryEdges    = 0;
  std::size_t           numNonManifoldEdges = 0;

  // Per-vertex angle-weighted pseudonormal (Bærentzen-Aanæs). Unit length
  // on watertight oriented input; left zero if all incident faces degenerate.
  std::vector<Vector3>  vertexPseudoNormal;

  // Per-edge pseudonormal: mean of the two adjacent face normals on
  // manifold edges, zero on boundary / non-manifold edges (sentinel ->
  // query falls back to face normal).
  std::vector<Vector3>  edgePseudoNormal;

  // --- Tier 2 #6 multipole tree for the GENERALIZED_WINDING_NUMBER sign ---
  //
  // One aggregate per nanort BVH node (indexed parallel to bvh.GetNodes()).
  // Empty unless the constructor was asked to build the winding tree.
  // windingNumberFast walks the BVH and approximates far subtrees with a
  // first-order expansion of the solid-angle kernel about each node.
  struct GwnNode {
    Vector3 centroid{0.0, 0.0, 0.0};       // area-weighted centroid
    Vector3 areaNormalSum{0.0, 0.0, 0.0};  // Σ area_T · n̂_T  (monopole)
    double  area = 0.0;                    // Σ area_T  (positive)
    // First moment about `centroid`, row-major M[i*3+j]
    //   = Σ area_T · n̂_T,i · (c_T − centroid)_j.
    std::array<double, 9> moment{};
  };
  std::vector<GwnNode> gwnNodes;
};

namespace {

void packMesh(geometrycentral::surface::SurfaceMesh& mesh,
              geometrycentral::surface::VertexPositionGeometry& geometry,
              std::vector<float>& outPositions,
              std::vector<unsigned int>& outIndices,
              std::vector<float>& outVertexNormals) {
  outPositions.clear();
  outPositions.reserve(mesh.nVertices() * 3);
  for (auto v : mesh.vertices()) {
    const Vector3 p = geometry.inputVertexPositions[v];
    outPositions.push_back(static_cast<float>(p.x));
    outPositions.push_back(static_cast<float>(p.y));
    outPositions.push_back(static_cast<float>(p.z));
  }

  outIndices.clear();
  outIndices.reserve(mesh.nFaces() * 3);
  for (auto f : mesh.faces()) {
    // Fan-triangulate any non-triangular face.
    std::vector<unsigned int> faceVerts;
    for (auto v : f.adjacentVertices()) {
      faceVerts.push_back(static_cast<unsigned int>(v.getIndex()));
    }
    if (faceVerts.size() < 3) continue;
    for (std::size_t i = 1; i + 1 < faceVerts.size(); ++i) {
      outIndices.push_back(faceVerts[0]);
      outIndices.push_back(faceVerts[i]);
      outIndices.push_back(faceVerts[i + 1]);
    }
  }

  // Recomputed vertex normals from the input topology (geometry-central's
  // corner-angle weighting of the unit face normals). Its SimplePolygonMesh
  // discards the OBJ `vn` block, so explicit input normals are not available;
  // the recomputed ones are a close-enough approximation for smooth shading
  // on typical triangulated inputs.
  geometry.requireVertexNormals();
  outVertexNormals.clear();
  outVertexNormals.reserve(mesh.nVertices() * 3);
  for (auto v : mesh.vertices()) {
    const Vector3 n = geometry.vertexNormals[v];
    outVertexNormals.push_back(static_cast<float>(n.x));
    outVertexNormals.push_back(static_cast<float>(n.y));
    outVertexNormals.push_back(static_cast<float>(n.z));
  }
  geometry.unrequireVertexNormals();
}

inline std::uint64_t packEdgeKey(unsigned int a, unsigned int b) {
  const std::uint64_t lo = (a < b) ? a : b;
  const std::uint64_t hi = (a < b) ? b : a;
  return (hi << 32) | lo;
}

Vector3 readPos(const std::vector<float>& positions, unsigned int v) {
  const std::size_t p = static_cast<std::size_t>(v) * 3;
  return Vector3{positions[p + 0], positions[p + 1], positions[p + 2]};
}

Vector3 faceNormalUnit(const Vector3& a, const Vector3& b, const Vector3& c) {
  const Vector3 n = cross(b - a, c - a);
  const double  m = n.norm();
  return (m > 0.0) ? (n / m) : Vector3{0.0, 0.0, 0.0};
}

constexpr double kPi     = 3.14159265358979323846;
constexpr double kInv4Pi = 1.0 / (4.0 * kPi);

// Signed solid angle of triangle (a, b, c) seen from q -- Van Oosterom &
// Strackee. Result in (-2π, 2π); summed over a closed, outward-oriented mesh
// it gives ±4π at interior points. Returns 0 when q coincides with a vertex.
double solidAngle(const Vector3& q, const Vector3& a,
                  const Vector3& b, const Vector3& c) {
  const Vector3 A = a - q, B = b - q, C = c - q;
  const double la = A.norm(), lb = B.norm(), lc = C.norm();
  if (la <= 0.0 || lb <= 0.0 || lc <= 0.0) return 0.0;
  const double num = dot(A, cross(B, C));
  const double den =
      la * lb * lc + dot(A, B) * lc + dot(B, C) * la + dot(C, A) * lb;
  return 2.0 * std::atan2(num, den);
}

// M += u ⊗ v  (3x3 row-major outer product accumulate).
void momentAddOuter(std::array<double, 9>& M, const Vector3& u,
                    const Vector3& v) {
  const double uu[3] = {u.x, u.y, u.z};
  const double vv[3] = {v.x, v.y, v.z};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) M[i * 3 + j] += uu[i] * vv[j];
}

} // namespace

// Builds triEdgeGlobal + edge -> (face-count) book, then the pseudonormal
// lookup tables. Called once at construction, single-threaded; O(T) total.
void MeshBVH::buildPseudoNormalTopology(Impl& impl, std::size_t numTris,
                                        std::size_t numVerts) {
  impl.triEdgeGlobal.assign(numTris * 3, -1);
  impl.numEdges            = 0;
  impl.numBoundaryEdges    = 0;
  impl.numNonManifoldEdges = 0;
  impl.vertexPseudoNormal.assign(numVerts, Vector3{0.0, 0.0, 0.0});
  impl.edgePseudoNormal.clear();
  if (numTris == 0) return;

  // First pass: assign a global edge index to every triangle edge.
  // Map value: (edgeIndex, incidentFaceCount).
  std::unordered_map<std::uint64_t, std::pair<int, int>> em;
  em.reserve(numTris * 2);  // ~3/2 ratio of edges to faces on closed meshes.
  for (std::size_t tri = 0; tri < numTris; ++tri) {
    for (int i = 0; i < 3; ++i) {
      const unsigned int va = impl.indices[3 * tri + i];
      const unsigned int vb = impl.indices[3 * tri + ((i + 1) % 3)];
      const std::uint64_t key = packEdgeKey(va, vb);
      auto it = em.find(key);
      if (it == em.end()) {
        const int edgeIdx = static_cast<int>(impl.numEdges++);
        it = em.emplace(key, std::pair<int, int>{edgeIdx, 0}).first;
      }
      impl.triEdgeGlobal[3 * tri + i] = it->second.first;
      it->second.second += 1;
    }
  }

  impl.edgePseudoNormal.assign(impl.numEdges, Vector3{0.0, 0.0, 0.0});

  // Second pass: accumulate the angle-weighted vertex pseudonormal and the
  // (unweighted) sum of face normals on each edge.
  for (std::size_t tri = 0; tri < numTris; ++tri) {
    const unsigned int i0 = impl.indices[3 * tri + 0];
    const unsigned int i1 = impl.indices[3 * tri + 1];
    const unsigned int i2 = impl.indices[3 * tri + 2];
    const Vector3 v0 = readPos(impl.positions, i0);
    const Vector3 v1 = readPos(impl.positions, i1);
    const Vector3 v2 = readPos(impl.positions, i2);
    const Vector3 nFace = faceNormalUnit(v0, v1, v2);

    // Vertex pseudonormal: angle at each corner * face normal.
    // atan2(|cross|, dot) is robust against very thin / very obtuse tris.
    const Vector3 e01 = v1 - v0, e02 = v2 - v0;
    const Vector3 e10 = v0 - v1, e12 = v2 - v1;
    const Vector3 e20 = v0 - v2, e21 = v1 - v2;
    const double a0 = std::atan2(cross(e01, e02).norm(), dot(e01, e02));
    const double a1 = std::atan2(cross(e12, e10).norm(), dot(e12, e10));
    const double a2 = std::atan2(cross(e20, e21).norm(), dot(e20, e21));
    impl.vertexPseudoNormal[i0] = impl.vertexPseudoNormal[i0] + nFace * a0;
    impl.vertexPseudoNormal[i1] = impl.vertexPseudoNormal[i1] + nFace * a1;
    impl.vertexPseudoNormal[i2] = impl.vertexPseudoNormal[i2] + nFace * a2;

    // Edge pseudonormal: accumulate face normals; halve + normalize manifold
    // edges in the final walk (boundary / non-manifold left as sentinel zero).
    for (int i = 0; i < 3; ++i) {
      const int e = impl.triEdgeGlobal[3 * tri + i];
      if (e >= 0) impl.edgePseudoNormal[e] = impl.edgePseudoNormal[e] + nFace;
    }
  }

  // Normalize per-vertex pseudonormals (zero stays zero on degenerate input).
  for (auto& n : impl.vertexPseudoNormal) {
    const double m = n.norm();
    if (m > 1e-12) n = n / m;
    else           n = Vector3{0.0, 0.0, 0.0};
  }

  // Finalize per-edge pseudonormals: manifold edges halve + normalize;
  // boundary / non-manifold edges revert to zero (sentinel).
  for (const auto& kv : em) {
    const int eIdx = kv.second.first;
    const int faceCount = kv.second.second;
    if (faceCount == 2) {
      Vector3 n = impl.edgePseudoNormal[eIdx] * 0.5;
      const double m = n.norm();
      impl.edgePseudoNormal[eIdx] =
          (m > 1e-12) ? (n / m) : Vector3{0.0, 0.0, 0.0};
    } else {
      impl.edgePseudoNormal[eIdx] = Vector3{0.0, 0.0, 0.0};
      if (faceCount == 1)      ++impl.numBoundaryEdges;
      else if (faceCount > 2)  ++impl.numNonManifoldEdges;
    }
  }
}

std::size_t MeshBVH::numBoundaryEdges() const {
  return impl_->numBoundaryEdges;
}

std::size_t MeshBVH::numNonManifoldEdges() const {
  return impl_->numNonManifoldEdges;
}

// Builds the per-BVH-node multipole aggregates. Post-order over the nanort
// nodes: leaves accumulate from their triangles, interior nodes merge their
// two children. O(N) total, single-threaded. Called once at construction.
void MeshBVH::buildWindingNumberTree(Impl& impl) {
  impl.gwnNodes.clear();
  if (!impl.bvh.IsValid()) return;
  const auto& nodes      = impl.bvh.GetNodes();
  const auto& bvhIndices = impl.bvh.GetIndices();
  if (nodes.empty()) return;

  impl.gwnNodes.assign(nodes.size(), Impl::GwnNode{});

  const float*        positions = impl.positions.data();
  const unsigned int* triIdx    = impl.indices.data();
  auto vertOf = [&](std::size_t tri, int corner) -> Vector3 {
    const unsigned int v = triIdx[tri * 3 + static_cast<std::size_t>(corner)];
    const std::size_t  p = static_cast<std::size_t>(v) * 3;
    return Vector3{positions[p + 0], positions[p + 1], positions[p + 2]};
  };

  // Collect node indices in pre-order; iterating that list back-to-front
  // visits every child before its parent (post-order).
  std::vector<unsigned int> order;
  order.reserve(nodes.size());
  std::vector<unsigned int> stack;
  stack.reserve(64);
  stack.push_back(0);
  while (!stack.empty()) {
    const unsigned int idx = stack.back();
    stack.pop_back();
    order.push_back(idx);
    const auto& n = nodes[idx];
    if (n.flag != 1) {
      stack.push_back(n.data[0]);
      stack.push_back(n.data[1]);
    }
  }

  for (std::size_t k = order.size(); k-- > 0;) {
    const unsigned int idx = order[k];
    const auto& n = nodes[idx];
    Impl::GwnNode& g = impl.gwnNodes[idx];
    const Vector3 nodeCenter{0.5 * (static_cast<double>(n.bmin[0]) + n.bmax[0]),
                             0.5 * (static_cast<double>(n.bmin[1]) + n.bmax[1]),
                             0.5 * (static_cast<double>(n.bmin[2]) + n.bmax[2])};

    if (n.flag == 1) {
      const unsigned int count  = n.data[0];
      const unsigned int offset = n.data[1];
      // Pass 1: area, area-weighted centroid, area-normal sum.
      Vector3 cSum{0.0, 0.0, 0.0};
      for (unsigned int t = 0; t < count; ++t) {
        const std::size_t tri = bvhIndices[offset + t];
        const Vector3 a = vertOf(tri, 0), b = vertOf(tri, 1), c = vertOf(tri, 2);
        const Vector3 cr = cross(b - a, c - a);   // 2·area·n̂
        const double  ar = 0.5 * cr.norm();
        g.area          += ar;
        g.areaNormalSum  = g.areaNormalSum + cr * 0.5;
        cSum             = cSum + (a + b + c) * (ar / 3.0);
      }
      g.centroid = (g.area > 1e-30) ? (cSum / g.area) : nodeCenter;
      // Pass 2: first moment about the centroid.
      for (unsigned int t = 0; t < count; ++t) {
        const std::size_t tri = bvhIndices[offset + t];
        const Vector3 a = vertOf(tri, 0), b = vertOf(tri, 1), c = vertOf(tri, 2);
        const Vector3 an = cross(b - a, c - a) * 0.5;        // area_T · n̂_T
        const Vector3 d  = (a + b + c) * (1.0 / 3.0) - g.centroid;
        momentAddOuter(g.moment, an, d);
      }
    } else {
      const Impl::GwnNode& A = impl.gwnNodes[n.data[0]];
      const Impl::GwnNode& B = impl.gwnNodes[n.data[1]];
      g.area          = A.area + B.area;
      g.areaNormalSum = A.areaNormalSum + B.areaNormalSum;
      g.centroid = (g.area > 1e-30)
          ? (A.centroid * A.area + B.centroid * B.area) / g.area
          : nodeCenter;
      // Merge each child's moment, shifting its expansion point from the
      // child centroid to the parent centroid:
      //   M_ij(p) = M_ij(p_child) + areaNormalSum_i · (p_child − p)_j.
      for (int e = 0; e < 9; ++e) g.moment[e] = A.moment[e] + B.moment[e];
      momentAddOuter(g.moment, A.areaNormalSum, A.centroid - g.centroid);
      momentAddOuter(g.moment, B.areaNormalSum, B.centroid - g.centroid);
    }
  }
}

MeshBVH::MeshBVH(geometrycentral::surface::SurfaceMesh& mesh,
                 geometrycentral::surface::VertexPositionGeometry& geometry,
                 bool interpolateNormals,
                 bool buildWindingTree)
    : impl_(std::make_unique<Impl>()),
      interpolateNormals_(interpolateNormals) {
  packMesh(mesh, geometry, impl_->positions, impl_->indices, impl_->vertexNormals);
  numTris_ = impl_->indices.size() / 3;
  const std::size_t numVerts = impl_->positions.size() / 3;

  if (numTris_ > 0) {
    constexpr std::size_t kStride = sizeof(float) * 3;
    impl_->nanortMesh = std::make_unique<nanort::TriangleMesh<float>>(
        impl_->positions.data(), impl_->indices.data(), kStride);
    impl_->sahPred = std::make_unique<nanort::TriangleSAHPred<float>>(
        impl_->positions.data(), impl_->indices.data(), kStride);

    nanort::BVHBuildOptions<float> opts;
    impl_->bvh.Build(static_cast<unsigned int>(numTris_), opts,
                     *impl_->nanortMesh, *impl_->sahPred);
  }

  // Topology + pseudonormal tables for the PSEUDONORMAL sign path. Cheap to
  // always build (a few MB even on big meshes); pays off on every sign query.
  MeshBVH::buildPseudoNormalTopology(*impl_, numTris_, numVerts);

  // Multipole tree for the GENERALIZED_WINDING_NUMBER sign path -- only when
  // a caller actually needs it (open / soup / self-intersecting input).
  if (buildWindingTree && numTris_ > 0) {
    MeshBVH::buildWindingNumberTree(*impl_);
  }
}

MeshBVH::~MeshBVH() = default;

bool MeshBVH::segmentFirstHit(const Vector3& a, const Vector3& b,
                              Vector3& outP, Vector3& outN, int& outTri) const {
  outTri = -1;
  outP = Vector3{0.0, 0.0, 0.0};
  outN = Vector3{0.0, 0.0, 0.0};
  if (numTris_ == 0) return false;

  const Vector3 d = b - a;
  const double  len = d.norm();
  if (len <= 0.0) return false;

  nanort::Ray<float> ray{};
  ray.org[0] = static_cast<float>(a.x);
  ray.org[1] = static_cast<float>(a.y);
  ray.org[2] = static_cast<float>(a.z);
  ray.dir[0] = static_cast<float>(d.x / len);
  ray.dir[1] = static_cast<float>(d.y / len);
  ray.dir[2] = static_cast<float>(d.z / len);
  ray.min_t = 0.0f;
  ray.max_t = static_cast<float>(len);

  constexpr std::size_t kStride = sizeof(float) * 3;
  nanort::TriangleIntersector<float> isect(
      impl_->positions.data(), impl_->indices.data(), kStride);
  nanort::BVHTraceOptions traceOpts;
  const bool hit = impl_->bvh.Traverse(ray, traceOpts, isect);
  if (!hit) return false;

  const float t = isect.intersection.t;
  const unsigned int tri = isect.intersection.prim_id;
  if (t < 0.0f || t > static_cast<float>(len)) return false;

  outTri = static_cast<int>(tri);
  outP = a + d * (static_cast<double>(t) / len);

  if (interpolateNormals_) {
    // Barycentric-interpolated vertex normal at the hit. nanort encodes the
    // hit as `interp(p) = (1-u-v)*v0 + u*v1 + v*v2`, so the same weights
    // apply to the per-vertex normals. Falls back to the face normal when
    // the interpolated normal is degenerate.
    const float u = isect.intersection.u;
    const float v = isect.intersection.v;
    const float w = 1.0f - u - v;
    const std::size_t i0 = impl_->indices[3 * tri + 0];
    const std::size_t i1 = impl_->indices[3 * tri + 1];
    const std::size_t i2 = impl_->indices[3 * tri + 2];
    const Vector3 n0{impl_->vertexNormals[3*i0+0], impl_->vertexNormals[3*i0+1], impl_->vertexNormals[3*i0+2]};
    const Vector3 n1{impl_->vertexNormals[3*i1+0], impl_->vertexNormals[3*i1+1], impl_->vertexNormals[3*i1+2]};
    const Vector3 n2{impl_->vertexNormals[3*i2+0], impl_->vertexNormals[3*i2+1], impl_->vertexNormals[3*i2+2]};
    Vector3 nInterp = n0 * static_cast<double>(w)
                    + n1 * static_cast<double>(u)
                    + n2 * static_cast<double>(v);
    const double nMag = nInterp.norm();
    outN = (nMag > 1e-12) ? (nInterp / nMag) : triFaceNormal(tri);
  } else {
    outN = triFaceNormal(tri);
  }
  return true;
}

int MeshBVH::countRayHits(const Vector3& origin, const Vector3& direction) const {
  if (numTris_ == 0 || !impl_->bvh.IsValid()) return 0;

  const double dlen = direction.norm();
  if (dlen <= 0.0) return 0;

  nanort::Ray<float> ray{};
  ray.org[0] = static_cast<float>(origin.x);
  ray.org[1] = static_cast<float>(origin.y);
  ray.org[2] = static_cast<float>(origin.z);
  ray.dir[0] = static_cast<float>(direction.x / dlen);
  ray.dir[1] = static_cast<float>(direction.y / dlen);
  ray.dir[2] = static_cast<float>(direction.z / dlen);
  ray.min_t  = 0.0f;
  ray.max_t  = 1.0e10f;

  // One all-hits traversal rather than nanort's nearest-hit Traverse re-run
  // once per crossing. The re-run form has to push the ray past each hit by
  // some epsilon, and any epsilon relative to the hit distance silently eats
  // features thinner than it -- one lost crossing flips the parity, and the
  // three probe rays of SignOracle share an origin, so they flip together.
  // See docs/roadmap/17-code-audit-and-hardening item #23.
  //
  // nanort ships a MultiHitTraverse for this but it is #if 0'd out, so the
  // walk is written here against its public pieces: GetNodes / GetIndices,
  // IntersectRayAABB and TriangleIntersector::PrepareTraversal / ::Intersect.
  // This is also strictly less work than the old loop -- one descent instead
  // of one per hit -- and it retires the 4096-hit truncation cap along with
  // the epsilon.
  constexpr std::size_t kStride = sizeof(float) * 3;
  NanortIsect isect(impl_->positions.data(), impl_->indices.data(), kStride);
  nanort::BVHTraceOptions traceOpts;
  isect.PrepareTraversal(ray, traceOpts);

  const nanort::real3<float> rayOrg(ray.org[0], ray.org[1], ray.org[2]);
  // The + 1e-12f is nanort's own guard, copied from the live Traverse rather
  // than from the #if 0'd MultiHitTraverse (which omits it and carries a
  // "@fixme { Check edge case; i.e., 1/0 }" where it should be). Without it an
  // axis-aligned direction gives an infinite reciprocal, and a ray lying
  // exactly in a node's slab plane then evaluates 0 * inf = NaN and drops the
  // subtree. Matching Traverse also keeps node acceptance here identical to
  // segmentFirstHit's.
  const nanort::real3<float> rayInvDir(1.0f / (ray.dir[0] + 1.0e-12f),
                                       1.0f / (ray.dir[1] + 1.0e-12f),
                                       1.0f / (ray.dir[2] + 1.0e-12f));
  int dirSign[3] = {ray.dir[0] < 0.0f ? 1 : 0, ray.dir[1] < 0.0f ? 1 : 0,
                    ray.dir[2] < 0.0f ? 1 : 0};

  const auto& nodes = impl_->bvh.GetNodes();
  const auto& order = impl_->bvh.GetIndices();   // primitive permutation
  if (nodes.empty()) return 0;

  // Iterative DFS, same shape as cellOverlapsAABB below.
  std::vector<unsigned int> stack;
  stack.reserve(64);
  stack.push_back(0);

  std::vector<float> hits;
  hits.reserve(16);
  while (!stack.empty()) {
    const unsigned int idx = stack.back();
    stack.pop_back();
    const auto& n = nodes[idx];

    // Deliberately tested against the full ray extent: narrowing max_t as
    // hits accumulate -- which nanort's own traversal does to prune -- would
    // turn this back into a nearest-hit query.
    float tmin, tmax;
    if (!nanort::IntersectRayAABB(&tmin, &tmax, ray.min_t, ray.max_t, n.bmin,
                                  n.bmax, rayOrg, rayInvDir, dirSign)) {
      continue;
    }

    if (n.flag == 0) {                 // branch
      stack.push_back(n.data[0]);
      stack.push_back(n.data[1]);
      continue;
    }

    const unsigned int nPrims = n.data[0];
    const unsigned int offset = n.data[1];
    for (unsigned int i = 0; i < nPrims; ++i) {
      // localT is reset per primitive: Intersect() rejects any hit beyond the
      // value passed in, so carrying it over between primitives would again
      // collapse this to a nearest-hit search. Intersect() also does not
      // reject hits behind the origin -- nanort's TestLeafNode is what
      // normally applies min_t, and this replaces it.
      float localT = ray.max_t;
      if (isect.Intersect(&localT, order[offset + i]) && localT > ray.min_t) {
        hits.push_back(localT);
      }
    }
  }

  // Every primitive index appears in exactly one leaf -- nanort seeds its
  // permutation with the identity and only ever std::partitions it in place
  // over disjoint ranges -- so no triangle is visited twice. What does need
  // collapsing is a ray landing exactly on an edge shared by two triangles
  // (a mesh edge, or the interior diagonal of a triangulated quad): the
  // watertight test admits both, and the surface is still crossed once.
  //
  // Measured on the cases in test_mesh_bvh.cpp: those duplicate hits come out
  // bit-identical (0 ULP apart), while the two faces of a genuinely thin wall
  // are 29-49 ULP apart at the same distances. 4 ULP sits an order of
  // magnitude below the real separation and is the float resolution floor
  // anyway -- unlike the 1e-5 relative advance this replaces, which sat ~84x
  // *above* it and swallowed real geometry.
  std::sort(hits.begin(), hits.end());
  int count = 0;
  for (std::size_t i = 0; i < hits.size(); ++i) {
    if (i > 0) {
      const float t   = hits[i];
      const float tol = 4.0f * std::abs(t) * std::numeric_limits<float>::epsilon();
      if (t - hits[i - 1] <= tol) continue;   // same crossing, seen twice
    }
    ++count;
  }
  return count;
}

bool MeshBVH::cellOverlapsAABB(const BBox& cell) const {
  if (numTris_ == 0 || !impl_->bvh.IsValid()) return false;

  const auto& nodes = impl_->bvh.GetNodes();
  if (nodes.empty()) return false;

  const float cmin[3] = {static_cast<float>(cell.min.x),
                         static_cast<float>(cell.min.y),
                         static_cast<float>(cell.min.z)};
  const float cmax[3] = {static_cast<float>(cell.max.x),
                         static_cast<float>(cell.max.y),
                         static_cast<float>(cell.max.z)};

  // Iterative DFS to avoid recursion-depth concerns on very deep BVHs.
  std::vector<unsigned int> stack;
  stack.reserve(64);
  stack.push_back(0);

  while (!stack.empty()) {
    const unsigned int idx = stack.back();
    stack.pop_back();
    const auto& n = nodes[idx];

    if (n.bmax[0] < cmin[0] || n.bmin[0] > cmax[0]) continue;
    if (n.bmax[1] < cmin[1] || n.bmin[1] > cmax[1]) continue;
    if (n.bmax[2] < cmin[2] || n.bmin[2] > cmax[2]) continue;

    if (n.flag == 1) return true;     // leaf node overlaps -- enough for refinement.
    stack.push_back(n.data[0]);
    stack.push_back(n.data[1]);
  }
  return false;
}

namespace {

// Squared distance from q to an axis-aligned box [bmin, bmax]; 0 if inside.
double aabbDist2(const Vector3& q, const float* bmin, const float* bmax) {
  double s = 0.0;
  const double qc[3] = {q.x, q.y, q.z};
  for (int i = 0; i < 3; ++i) {
    double e = 0.0;
    if (qc[i] < static_cast<double>(bmin[i])) e = static_cast<double>(bmin[i]) - qc[i];
    else if (qc[i] > static_cast<double>(bmax[i])) e = qc[i] - static_cast<double>(bmax[i]);
    s += e * e;
  }
  return s;
}

// Closest point on triangle (a, b, c) to q, with Voronoi-region tag.
// Ericson, Real-Time Collision Detection, section 5.1.5.
TriClosest closestPtPointTriangle(const Vector3& q, const Vector3& a,
                                  const Vector3& b, const Vector3& c) {
  const Vector3 ab = b - a;
  const Vector3 ac = c - a;
  const Vector3 ap = q - a;
  const double d1 = dot(ab, ap);
  const double d2 = dot(ac, ap);
  if (d1 <= 0.0 && d2 <= 0.0) return TriClosest{a, TriRegion::VERT_A};

  const Vector3 bp = q - b;
  const double d3 = dot(ab, bp);
  const double d4 = dot(ac, bp);
  if (d3 >= 0.0 && d4 <= d3) return TriClosest{b, TriRegion::VERT_B};

  const double vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
    const double v = d1 / (d1 - d3);
    return TriClosest{a + ab * v, TriRegion::EDGE_AB};
  }

  const Vector3 cp = q - c;
  const double d5 = dot(ab, cp);
  const double d6 = dot(ac, cp);
  if (d6 >= 0.0 && d5 <= d6) return TriClosest{c, TriRegion::VERT_C};

  const double vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
    const double w = d2 / (d2 - d6);
    return TriClosest{a + ac * w, TriRegion::EDGE_CA};
  }

  const double va = d3 * d6 - d5 * d4;
  if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
    const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return TriClosest{b + (c - b) * w, TriRegion::EDGE_BC};
  }

  const double denom = 1.0 / (va + vb + vc);
  const double v = vb * denom;
  const double w = vc * denom;
  return TriClosest{a + ab * v + ac * w, TriRegion::FACE_INTERIOR};
}

} // namespace

// Shared BVH branch-and-bound DFS used by both closest-point public methods.
// Returns false iff the mesh has no triangles or the BVH is empty.
bool MeshBVH::findClosest(const Impl& impl, std::size_t numTris,
                          const Vector3& q, ClosestHit& out) {
  out.tri = -1;
  if (numTris == 0 || !impl.bvh.IsValid()) return false;

  const auto& nodes      = impl.bvh.GetNodes();
  const auto& bvhIndices = impl.bvh.GetIndices();
  if (nodes.empty()) return false;

  const float*        positions = impl.positions.data();
  const unsigned int* triIdx    = impl.indices.data();

  auto vertOf = [&](std::size_t tri, int corner) -> Vector3 {
    const unsigned int v = triIdx[tri * 3 + static_cast<std::size_t>(corner)];
    const std::size_t  p = static_cast<std::size_t>(v) * 3;
    return Vector3{positions[p + 0], positions[p + 1], positions[p + 2]};
  };

  double    bestD2  = std::numeric_limits<double>::infinity();
  Vector3   bestP{0.0, 0.0, 0.0};
  int       bestTri = -1;
  TriRegion bestReg = TriRegion::FACE_INTERIOR;

  std::vector<unsigned int> stack;
  stack.reserve(64);
  stack.push_back(0);

  while (!stack.empty()) {
    const unsigned int idx = stack.back();
    stack.pop_back();
    const auto& n = nodes[idx];

    if (aabbDist2(q, n.bmin, n.bmax) >= bestD2) continue;

    if (n.flag == 1) {
      const unsigned int count  = n.data[0];
      const unsigned int offset = n.data[1];
      for (unsigned int k = 0; k < count; ++k) {
        const std::size_t tri = bvhIndices[offset + k];
        const Vector3 a = vertOf(tri, 0);
        const Vector3 b = vertOf(tri, 1);
        const Vector3 c = vertOf(tri, 2);
        const TriClosest tc = closestPtPointTriangle(q, a, b, c);
        const Vector3 d  = q - tc.point;
        const double  d2 = dot(d, d);
        if (d2 < bestD2) {
          bestD2  = d2;
          bestP   = tc.point;
          bestTri = static_cast<int>(tri);
          bestReg = tc.region;
        }
      }
    } else {
      stack.push_back(n.data[0]);
      stack.push_back(n.data[1]);
    }
  }

  if (bestTri < 0) return false;
  out.point  = bestP;
  out.tri    = bestTri;
  out.region = bestReg;
  return true;
}

bool MeshBVH::closestPoint(const Vector3& q, Vector3& outP, Vector3& outN,
                           int& outTri) const {
  ClosestHit hit;
  if (!findClosest(*impl_, numTris_, q, hit)) return false;
  outP   = hit.point;
  outTri = hit.tri;
  outN   = triFaceNormal(static_cast<std::size_t>(hit.tri));
  return true;
}

bool MeshBVH::closestPointWithPseudoNormal(const Vector3& q,
                                           Vector3& outP,
                                           Vector3& outPseudoNormal,
                                           int& outTri) const {
  ClosestHit hit;
  if (!findClosest(*impl_, numTris_, q, hit)) return false;

  const std::size_t triIdx = static_cast<std::size_t>(hit.tri);
  const Vector3 fallback   = triFaceNormal(triIdx);

  outP   = hit.point;
  outTri = hit.tri;

  switch (hit.region) {
    case TriRegion::FACE_INTERIOR:
      outPseudoNormal = fallback;
      break;
    case TriRegion::VERT_A:
    case TriRegion::VERT_B:
    case TriRegion::VERT_C: {
      const int c = cornerOf(hit.region);
      const unsigned int gv = impl_->indices[3 * triIdx + c];
      const Vector3& n = impl_->vertexPseudoNormal[gv];
      outPseudoNormal = (n.norm() > 1e-12) ? n : fallback;
      break;
    }
    case TriRegion::EDGE_AB:
    case TriRegion::EDGE_BC:
    case TriRegion::EDGE_CA: {
      const int e  = edgeOf(hit.region);
      const int ge = impl_->triEdgeGlobal[3 * triIdx + e];
      if (ge < 0) {
        outPseudoNormal = fallback;
      } else {
        const Vector3& n = impl_->edgePseudoNormal[ge];
        outPseudoNormal = (n.norm() > 1e-12) ? n : fallback;
      }
      break;
    }
  }
  return true;
}

double MeshBVH::windingNumber(const Vector3& q) const {
  if (numTris_ == 0) return 0.0;
  double acc = 0.0;
  for (std::size_t tri = 0; tri < numTris_; ++tri) {
    acc += solidAngle(q, triVertex(tri, 0), triVertex(tri, 1),
                      triVertex(tri, 2));
  }
  return acc * kInv4Pi;
}

double MeshBVH::windingNumberFast(const Vector3& q, double beta) const {
  if (numTris_ == 0 || impl_->gwnNodes.empty()) return 0.0;
  if (!impl_->bvh.IsValid()) return 0.0;

  const auto& nodes      = impl_->bvh.GetNodes();
  const auto& bvhIndices = impl_->bvh.GetIndices();
  if (nodes.empty()) return 0.0;

  const double beta2 = beta * beta;
  double acc = 0.0;  // accumulated raw solid angle

  std::vector<unsigned int> stack;
  stack.reserve(64);
  stack.push_back(0);

  while (!stack.empty()) {
    const unsigned int idx = stack.back();
    stack.pop_back();
    const auto& n = nodes[idx];
    const Impl::GwnNode& g = impl_->gwnNodes[idx];

    // Node radius (half AABB diagonal) and distance from q to its centroid.
    const double rx = 0.5 * (static_cast<double>(n.bmax[0]) - n.bmin[0]);
    const double ry = 0.5 * (static_cast<double>(n.bmax[1]) - n.bmin[1]);
    const double rz = 0.5 * (static_cast<double>(n.bmax[2]) - n.bmin[2]);
    const double r2 = rx * rx + ry * ry + rz * rz;
    const Vector3 R  = g.centroid - q;
    const double  d2 = dot(R, R);

    if (d2 > beta2 * r2 && d2 > 1e-24) {
      // Far subtree: first-order multipole expansion of the solid-angle
      // kernel K(r) = r / |r|^3 about the node centroid.
      //   ∫ n̂·K dA ≈ (R·areaNormalSum)/s^3 + tr(M)/s^3 − 3·(Rᵀ M R)/s^5
      const double s  = std::sqrt(d2);
      const double s3 = d2 * s;
      const double s5 = s3 * d2;
      const double trM = g.moment[0] + g.moment[4] + g.moment[8];
      const double Rv[3] = {R.x, R.y, R.z};
      double rMr = 0.0;
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          rMr += Rv[i] * g.moment[i * 3 + j] * Rv[j];
      acc += (dot(R, g.areaNormalSum) + trM) / s3 - 3.0 * rMr / s5;
    } else if (n.flag == 1) {
      // Near leaf: exact solid angle of every triangle.
      const unsigned int count  = n.data[0];
      const unsigned int offset = n.data[1];
      for (unsigned int t = 0; t < count; ++t) {
        const std::size_t tri = bvhIndices[offset + t];
        acc += solidAngle(q, triVertex(tri, 0), triVertex(tri, 1),
                          triVertex(tri, 2));
      }
    } else {
      stack.push_back(n.data[0]);
      stack.push_back(n.data[1]);
    }
  }
  return acc * kInv4Pi;
}

Vector3 MeshBVH::triVertex(std::size_t triIdx, int corner) const {
  const std::size_t base = triIdx * 3 + static_cast<std::size_t>(corner);
  const unsigned int v = impl_->indices[base];
  const std::size_t p = static_cast<std::size_t>(v) * 3;
  return Vector3{impl_->positions[p + 0],
                 impl_->positions[p + 1],
                 impl_->positions[p + 2]};
}

Vector3 MeshBVH::triFaceNormal(std::size_t triIdx) const {
  const Vector3 a = triVertex(triIdx, 0);
  const Vector3 b = triVertex(triIdx, 1);
  const Vector3 c = triVertex(triIdx, 2);
  const Vector3 n = cross(b - a, c - a);
  const double  m = n.norm();
  return (m > 0.0) ? (n / m) : Vector3{0.0, 0.0, 0.0};
}

} // namespace internal
} // namespace dualc
