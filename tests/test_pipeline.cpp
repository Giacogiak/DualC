#include "dualc/pipeline.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>
#include <vector>

using namespace dualc;

TEST_CASE("dualContourMesh runs end-to-end on a tiny mesh (stub level)",
          "[pipeline]") {
  std::vector<Vector3> positions = {
      Vector3{0.0, 0.0, 0.0},
      Vector3{1.0, 0.0, 0.0},
      Vector3{0.0, 1.0, 0.0},
      Vector3{0.0, 0.0, 1.0},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
  };
  auto [inMesh, inGeom] =
      geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
  REQUIRE(inMesh != nullptr);

  auto [outMesh, outGeom, outNormals] =
      dualContourMesh(*inMesh, *inGeom, SamplerParams{}, ContourerParams{});
  REQUIRE(outMesh != nullptr);
  REQUIRE(outGeom != nullptr);
  REQUIRE(outNormals.size() == outMesh->nVertices());
}

// ===========================================================================
// dualContourMesh must forward every SamplerParams field
// ===========================================================================
//
// `dualContourMesh` used to build its own MeshSource from just
// `interpolateNormals`, so a caller who asked for GENERALIZED_WINDING_NUMBER
// (or PSEUDONORMAL) silently got ray-parity instead. It now delegates to
// `sampleMeshToHermiteOctree`, which is the library's single MeshSource
// construction site. These cases pin that: the convenience entry point must
// agree with the explicit sampler + contourer path for every sign method.

namespace {

using MeshPair =
    std::pair<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
              std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>;

MeshPair makeTetrahedron(bool openBottom) {
  std::vector<Vector3> positions = {
      Vector3{0.0, 0.0, 0.0},
      Vector3{1.0, 0.0, 0.0},
      Vector3{0.0, 1.0, 0.0},
      Vector3{0.0, 0.0, 1.0},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
  };
  if (!openBottom) polygons.push_back({0, 2, 1});  // the closing face
  auto mg = geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                                 positions);
  return MeshPair{std::move(std::get<0>(mg)), std::move(std::get<1>(mg))};
}

int boundaryEdgeCount(geometrycentral::surface::SurfaceMesh& m) {
  int n = 0;
  for (auto e : m.edges())
    if (e.isBoundary()) ++n;
  return n;
}

} // namespace

TEST_CASE("dualContourMesh matches the explicit sampler path for every sign "
          "method", "[pipeline][sign]") {
  for (const SignMethod method : {SignMethod::WINDING_NUMBER,
                                  SignMethod::PSEUDONORMAL,
                                  SignMethod::GENERALIZED_WINDING_NUMBER}) {
    SamplerParams sp;
    sp.maxDepth   = 5;
    sp.signMethod = method;
    ContourerParams cp;

    auto viaPipeline = makeTetrahedron(/*openBottom=*/false);
    auto [pMesh, pGeom, pNormals] =
        dualContourMesh(*viaPipeline.first, *viaPipeline.second, sp, cp);

    auto viaSampler = makeTetrahedron(/*openBottom=*/false);
    HermiteOctree octree =
        sampleMeshToHermiteOctree(*viaSampler.first, *viaSampler.second, sp);
    auto [sMesh, sGeom, sNormals] = contourHermiteOctree(octree, cp);

    INFO("sign method index " << static_cast<int>(method));
    REQUIRE(pMesh->nVertices() == sMesh->nVertices());
    REQUIRE(pMesh->nEdges()    == sMesh->nEdges());
    REQUIRE(pMesh->nFaces()    == sMesh->nFaces());
  }
}

TEST_CASE("dualContourMesh honours GENERALIZED_WINDING_NUMBER on an open mesh",
          "[pipeline][sign][gwn]") {
  // A tetrahedron with one face removed. GWN's 0.5-isosurface seals the hole,
  // so the output is watertight; ray parity cannot, so it is not. If the
  // entry point ever drops `signMethod` again, the GWN request degrades to
  // parity and this fails.
  SamplerParams sp;
  sp.maxDepth   = 6;
  sp.signMethod = SignMethod::GENERALIZED_WINDING_NUMBER;
  ContourerParams cp;

  auto gwnIn = makeTetrahedron(/*openBottom=*/true);
  auto [gwnMesh, gwnGeom, gwnNormals] =
      dualContourMesh(*gwnIn.first, *gwnIn.second, sp, cp);
  REQUIRE(gwnMesh != nullptr);
  REQUIRE(gwnMesh->nVertices() > 0);
  REQUIRE(boundaryEdgeCount(*gwnMesh) == 0);

  SamplerParams parity = sp;
  parity.signMethod = SignMethod::WINDING_NUMBER;
  auto parityIn = makeTetrahedron(/*openBottom=*/true);
  auto [parityMesh, parityGeom, parityNormals] =
      dualContourMesh(*parityIn.first, *parityIn.second, parity, cp);
  REQUIRE(parityMesh != nullptr);
  // The two oracles must actually disagree here, or the case above proves
  // nothing about forwarding.
  CHECK(parityMesh->nFaces() != gwnMesh->nFaces());
}
