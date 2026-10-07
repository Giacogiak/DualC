// Accuracy of the output, measured against analytic surfaces (roadmap 17 #32,
// audit findings C47-C50). The rest of the suite checks topology -- watertight,
// manifold, the right genus -- and stays silent on how far the vertices sit
// from the surface or which way the normals point. These cases put numbers on
// both: the vertex error must fall as maxDepth rises, the adaptive collapse
// must trade faces for a bounded error, the output normals must be unit and
// outward, and SamplerParams::interpolateNormals must do what its comment says.

#include "demo_meshes.h"

#include "dualc/pipeline.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "internal/mesh_bvh.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

using namespace dualc;

namespace {

// Vertex error of one contour: |field(v)| over the output vertices. Both
// fields used below are exact signed distances, so this is the distance from
// each vertex to the analytic surface.
struct ContourError {
  double      maxErr  = 0.0;
  double      meanErr = 0.0;
  std::size_t faces   = 0;
};

ContourError contourError(const ImplicitField& f, int maxDepth,
                          double simplificationError = 0.0) {
  SamplerParams sp;
  sp.maxDepth = maxDepth;
  ContourerParams cp;
  cp.simplificationError = simplificationError;
  auto [mesh, geom, normals] = dualContourField(f, sp, cp);
  ContourError out;
  for (auto v : mesh->vertices()) {
    const double e = std::abs(f.valueAt(geom->vertexPositions[v]));
    out.maxErr = std::max(out.maxErr, e);
    out.meanErr += e;
  }
  out.meanErr /= static_cast<double>(mesh->nVertices());
  out.faces = mesh->nFaces();
  return out;
}

void forEachLeaf(const HermiteNode* n,
                 const std::function<void(const HermiteNode&)>& fn) {
  if (n == nullptr) return;
  if (n->isLeaf) {
    if (n->leaf) fn(*n);
    return;
  }
  for (const auto& c : n->children) forEachLeaf(c.get(), fn);
}

// Off-centre, so no octree plane is a symmetry plane of the surface.
SphereField testSphere() { return SphereField(Vector3{0.13, -0.07, 0.05}, 0.71); }
TorusField  testTorus()  { return TorusField(Vector3{0.05, 0.02, -0.03}, 0.6, 0.22); }

} // namespace

// C47. Measured on this pair (Linux, GCC 15, Release): the max vertex error
// falls by 3.7x-4.7x per level from depth 4 to 7 -- the O(h^2) of a QEF vertex
// on a smooth surface -- from 4.0e-3 to 7.2e-5 on the sphere and 8.9e-3 to
// 1.4e-4 on the torus. The bar is 3x per level, every level, on both the max
// and the mean, so a regression that costs accuracy without breaking topology
// (a mis-solved QEF, a vertex clamped to the wrong cell) fails here.
TEST_CASE("Vertex error against an analytic surface falls as maxDepth rises",
          "[accuracy][convergence]") {
  const SphereField sphere = testSphere();
  const TorusField  torus  = testTorus();
  for (const ImplicitField* f :
       {static_cast<const ImplicitField*>(&sphere),
        static_cast<const ImplicitField*>(&torus)}) {
    std::vector<ContourError> byDepth;
    for (int d = 4; d <= 7; ++d) byDepth.push_back(contourError(*f, d));
    for (std::size_t i = 1; i < byDepth.size(); ++i) {
      INFO("depth " << (4 + i - 1) << " -> " << (4 + i) << ": max "
                    << byDepth[i - 1].maxErr << " -> " << byDepth[i].maxErr
                    << ", mean " << byDepth[i - 1].meanErr << " -> "
                    << byDepth[i].meanErr);
      CHECK(byDepth[i].maxErr * 3.0 < byDepth[i - 1].maxErr);
      CHECK(byDepth[i].meanErr * 3.0 < byDepth[i - 1].meanErr);
    }
    // Depth 7 on a ~1.5-unit root box: a cell is ~1.2e-2; the vertices sit
    // within a hundredth of a cell of the surface.
    CHECK(byDepth.back().maxErr < 2e-4);
  }
}

// C48 (one of the four knobs; the other three are leaf-level cases in
// test_contourer.cpp). Measured on the depth-7 sphere: 127,740 faces at
// max error 7.2e-5 without collapse; 112,926 faces at 2.2e-4 with 1e-6.
TEST_CASE("ContourerParams::simplificationError trades faces for bounded error",
          "[accuracy][collapse]") {
  const SphereField sphere = testSphere();
  const ContourError off   = contourError(sphere, 7, 0.0);
  const ContourError tight = contourError(sphere, 7, 1e-6);
  const ContourError loose = contourError(sphere, 7, 1e-3);
  INFO("faces " << off.faces << " / " << tight.faces << " / " << loose.faces
                << ", max error " << off.maxErr << " / " << tight.maxErr
                << " / " << loose.maxErr);
  // A real reduction, not a rounding: at least 5 % fewer faces.
  CHECK(tight.faces * 20 < off.faces * 19);
  // Monotone in the threshold.
  CHECK(loose.faces <= tight.faces);
  // The collapse costs accuracy, but a bounded amount: a merged cell is twice
  // as wide, and its vertex still sits well inside a depth-7 cell (~1.2e-2).
  CHECK(tight.maxErr > off.maxErr);
  CHECK(loose.maxErr < 1e-3);
}

// C50. The output normals were checked for array size only. Each must be unit
// length and point outward: along the field gradient at the vertex, which for
// an SDF is the outward surface normal. Measured: |n| - 1 under 4e-16, and
// n . grad >= 0.999999 on the sphere and the torus, 1 on the box.
TEST_CASE("Output normals are unit length and point along the field gradient",
          "[accuracy][normals]") {
  const SphereField sphere = testSphere();
  const TorusField  torus  = testTorus();
  const BoxField    box(Vector3{-0.4, -0.3, -0.2}, Vector3{0.35, 0.3, 0.25});
  for (const ImplicitField* f :
       {static_cast<const ImplicitField*>(&sphere),
        static_cast<const ImplicitField*>(&torus),
        static_cast<const ImplicitField*>(&box)}) {
    SamplerParams sp;
    sp.maxDepth = 6;
    auto [mesh, geom, normals] = dualContourField(*f, sp, ContourerParams{});
    REQUIRE(normals.size() == mesh->nVertices());
    double worstLen = 0.0;
    double minDot   = 1.0;
    for (auto v : mesh->vertices()) {
      const Vector3 n = normals[v.getIndex()];
      worstLen = std::max(worstLen, std::abs(n.norm() - 1.0));
      const Vector3 g = f->gradientAt(geom->vertexPositions[v]);
      minDot = std::min(minDot, dot(n, g / g.norm()));
    }
    INFO("worst |n| - 1 = " << worstLen << ", min n . grad = " << minDot);
    CHECK(worstLen < 1e-9);
    CHECK(minDot > 0.99);
  }
}

// C49, unit level. MeshBVH::segmentFirstHit is where the sharp/smooth choice
// is made: smooth returns the input vertex normals blended at the hit's
// barycentric coordinates, sharp the hit triangle's face normal. Recomputed
// here from geometry-central's vertex normals, independently of the BVH.
TEST_CASE("interpolateNormals picks the blended vertex normal or the face normal",
          "[accuracy][normals][mesh_bvh]") {
  auto [mesh, geom] = dce::makeCube(1.0);  // [-0.5, 0.5]^3, triangulated
  geom->requireVertexNormals();
  const Vector3 a{2.0, 0.31, 0.17};  // a ray onto the +x face, off-centre
  const Vector3 b{0.0, 0.31, 0.17};

  internal::MeshBVH sharp(*mesh, *geom, /*interpolateNormals=*/false);
  internal::MeshBVH smooth(*mesh, *geom, /*interpolateNormals=*/true);
  Vector3 pS, nS, pM, nM;
  int     tS = -1, tM = -1;
  REQUIRE(sharp.segmentFirstHit(a, b, pS, nS, tS));
  REQUIRE(smooth.segmentFirstHit(a, b, pM, nM, tM));
  REQUIRE(tS == tM);

  // Sharp: exactly the +x face normal.
  CHECK(std::abs(nS.x - 1.0) < 1e-6);
  CHECK(std::abs(nS.y) < 1e-6);
  CHECK(std::abs(nS.z) < 1e-6);

  // Smooth: the barycentric blend of the triangle's three vertex normals.
  const auto face = mesh->face(static_cast<std::size_t>(tM));
  std::vector<Vector3> corner, cornerN;
  for (auto v : face.adjacentVertices()) {
    corner.push_back(geom->vertexPositions[v]);
    cornerN.push_back(geom->vertexNormals[v]);
  }
  REQUIRE(corner.size() == 3);
  const Vector3 e1 = corner[1] - corner[0];
  const Vector3 e2 = corner[2] - corner[0];
  const Vector3 ep = pM - corner[0];
  const double  d11 = dot(e1, e1), d12 = dot(e1, e2), d22 = dot(e2, e2);
  const double  dp1 = dot(ep, e1), dp2 = dot(ep, e2);
  const double  den = d11 * d22 - d12 * d12;
  const double  u = (d22 * dp1 - d12 * dp2) / den;
  const double  w = (d11 * dp2 - d12 * dp1) / den;
  Vector3 want = cornerN[0] * (1.0 - u - w) + cornerN[1] * u + cornerN[2] * w;
  want = want / want.norm();
  INFO("smooth normal " << nM << ", expected " << want);
  CHECK((nM - want).norm() < 1e-5);
  CHECK(std::abs(nM.norm() - 1.0) < 1e-6);
  // A cube's vertex normals lean along the corner diagonals, so the blend is
  // visibly off the face normal -- and still outward.
  CHECK(nM.x < 0.99);
  CHECK(nM.x > 0.5);
}

// C49, end to end: the claim in SamplerParams' comment -- sharp normals bring
// a cube out "perfectly axis-aligned". Measured at depth 5 (cells ~3.4e-2):
// sharp, all 20,184 Hermite normals are axis-aligned and every output vertex
// lies on the cube (distance 0); smooth, 24 of them are and the worst vertex
// sits 1.2e-2 off it, a third of a cell, rounding every edge and corner.
TEST_CASE("interpolateNormals=false reproduces a cube's sharp edges exactly",
          "[accuracy][normals][sampler]") {
  const BoxField cube(Vector3{-0.5, -0.5, -0.5}, Vector3{0.5, 0.5, 0.5});
  struct Run {
    std::size_t edges = 0, axisAligned = 0;
    double      worstVertex = 0.0;
  };
  auto run = [&](bool interpolate) {
    auto [mesh, geom] = dce::makeCube(1.0);
    SamplerParams sp;
    sp.maxDepth           = 5;
    sp.interpolateNormals = interpolate;
    Run r;
    const HermiteOctree oct = sampleMeshToHermiteOctree(*mesh, *geom, sp);
    forEachLeaf(oct.root(), [&](const HermiteNode& n) {
      for (const auto& e : n.leaf->edges) {
        if (!e.hasCrossing) continue;
        ++r.edges;
        const double m = std::max({std::abs(e.normal.x), std::abs(e.normal.y),
                                   std::abs(e.normal.z)});
        if (std::abs(m - 1.0) < 1e-9) ++r.axisAligned;
      }
    });
    auto [out, outGeom, normals] =
        dualContourMesh(*mesh, *geom, sp, ContourerParams{});
    for (auto v : out->vertices())
      r.worstVertex = std::max(
          r.worstVertex, std::abs(cube.valueAt(outGeom->vertexPositions[v])));
    return r;
  };
  const Run sharp  = run(false);
  const Run smooth = run(true);
  INFO("sharp: " << sharp.axisAligned << "/" << sharp.edges
                 << " axis-aligned, worst vertex " << sharp.worstVertex
                 << "; smooth: " << smooth.axisAligned << "/" << smooth.edges
                 << ", worst vertex " << smooth.worstVertex);
  REQUIRE(sharp.edges > 0);
  CHECK(sharp.axisAligned == sharp.edges);
  CHECK(sharp.worstVertex < 1e-6);
  CHECK(smooth.axisAligned * 10 < smooth.edges);
  CHECK(smooth.worstVertex > 5e-3);
}
