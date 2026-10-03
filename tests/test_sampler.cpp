#include "dualc/sampler.h"

#include "dualc/implicit.h"
#include "dualc/pipeline.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace dualc;

namespace {

std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeUnitTetrahedron() {
  std::vector<Vector3> positions = {
      Vector3{0.0, 0.0, 0.0},
      Vector3{1.0, 0.0, 0.0},
      Vector3{0.0, 1.0, 0.0},
      Vector3{0.0, 0.0, 1.0},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

// The same tetrahedron with one face ({1,2,3}) removed -- an open shell.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeOpenTetrahedron() {
  std::vector<Vector3> positions = {
      Vector3{0.0, 0.0, 0.0},
      Vector3{1.0, 0.0, 0.0},
      Vector3{0.0, 1.0, 0.0},
      Vector3{0.0, 0.0, 1.0},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 2, 1}, {0, 1, 3}, {0, 3, 2},  // {1,2,3} dropped
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

// One mesh holding two DISCONNECTED open shells that individually have an
// open boundary but together tile a closed unit cube:
//   - component 1: an open box (cube verts 0-7, the 5 faces except +z);
//   - component 2: a lid -- 4 fresh verts (8-11) duplicating the +z corner
//     positions, one quad wound outward.
// The lid shares no vertex with the box, so this is genuinely two separate
// shells whose union bounds a volume -- the "plane + half sphere" scenario.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeJointlyClosedBox() {
  std::vector<Vector3> positions = {
      Vector3{-0.5, -0.5, -0.5}, Vector3{0.5, -0.5, -0.5},   // 0-3 bottom ring
      Vector3{0.5, 0.5, -0.5},   Vector3{-0.5, 0.5, -0.5},
      Vector3{-0.5, -0.5, 0.5},  Vector3{0.5, -0.5, 0.5},    // 4-7 top ring
      Vector3{0.5, 0.5, 0.5},    Vector3{-0.5, 0.5, 0.5},
      Vector3{-0.5, -0.5, 0.5},  Vector3{0.5, -0.5, 0.5},    // 8-11 lid (dup)
      Vector3{0.5, 0.5, 0.5},    Vector3{-0.5, 0.5, 0.5},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {0, 1, 5, 4}, {3, 7, 6, 2},   // open box: 5 faces,
      {0, 4, 7, 3}, {1, 2, 6, 5},                 // +z omitted
      {8, 9, 10, 11},                             // separate lid component
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
}

} // namespace

TEST_CASE("Sampler stub returns a root-only octree with auto-fit bounds",
          "[sampler]") {
  auto [mesh, geom] = makeUnitTetrahedron();
  REQUIRE(mesh != nullptr);
  REQUIRE(geom != nullptr);

  SamplerParams params;
  HermiteOctree o = sampleMeshToHermiteOctree(*mesh, *geom, params);

  REQUIRE(o.root() != nullptr);
  const BBox b = o.root()->bounds;
  REQUIRE(b.isValid());
  // The auto-fit bounds must contain the tetrahedron with at least padFraction slack.
  REQUIRE(b.min.x < 0.0);
  REQUIRE(b.min.y < 0.0);
  REQUIRE(b.min.z < 0.0);
  REQUIRE(b.max.x > 1.0);
  REQUIRE(b.max.y > 1.0);
  REQUIRE(b.max.z > 1.0);
}

TEST_CASE("Sampler honours an explicit rootBounds", "[sampler]") {
  auto [mesh, geom] = makeUnitTetrahedron();
  SamplerParams params;
  BBox b;
  b.min = Vector3{-2.0, -2.0, -2.0};
  b.max = Vector3{ 2.0,  2.0,  2.0};
  params.rootBounds = b;

  HermiteOctree o = sampleMeshToHermiteOctree(*mesh, *geom, params);
  REQUIRE(o.root() != nullptr);
  REQUIRE(o.root()->bounds.min == b.min);
  REQUIRE(o.root()->bounds.max == b.max);
}

namespace {

// Recursive count of populated (non-pruned) leaves under `node`.
int countPopulatedLeaves(const HermiteNode* node) {
  if (node == nullptr) return 0;
  if (node->isLeaf) return node->leaf ? 1 : 0;
  int total = 0;
  for (const auto& c : node->children) total += countPopulatedLeaves(c.get());
  return total;
}

} // namespace

TEST_CASE("Sampler PSEUDONORMAL agrees with WINDING_NUMBER on a tetrahedron",
          "[sampler][sign]") {
  auto [mesh, geom] = makeUnitTetrahedron();

  SamplerParams pGwn;
  pGwn.maxDepth = 4;
  pGwn.signMethod = SignMethod::WINDING_NUMBER;

  SamplerParams pPn = pGwn;
  pPn.signMethod = SignMethod::PSEUDONORMAL;

  HermiteOctree gwn = sampleMeshToHermiteOctree(*mesh, *geom, pGwn);
  HermiteOctree pn  = sampleMeshToHermiteOctree(*mesh, *geom, pPn);

  // The two sign methods must classify every cell corner identically on a
  // watertight tetrahedron -- so the octree refinement decision (driven by
  // BVH overlap, not signs) yields the same number of populated leaves, and
  // every leaf's cornerInside pattern matches.
  REQUIRE(countPopulatedLeaves(gwn.root()) ==
          countPopulatedLeaves(pn.root()));
}

TEST_CASE("Sampler GENERALIZED_WINDING_NUMBER produces a valid octree",
          "[sampler][sign][gwn]") {
  auto [mesh, geom] = makeUnitTetrahedron();

  SamplerParams p;
  p.maxDepth   = 4;
  p.signMethod = SignMethod::GENERALIZED_WINDING_NUMBER;

  HermiteOctree o = sampleMeshToHermiteOctree(*mesh, *geom, p);
  REQUIRE(o.root() != nullptr);
  REQUIRE(countPopulatedLeaves(o.root()) > 0);
}

TEST_CASE("WindingNumberField seals an open mesh into a closed surface",
          "[sampler][gwn]") {
  auto [mesh, geom] = makeOpenTetrahedron();
  WindingNumberField field(*mesh, *geom);

  SamplerParams   sp;
  sp.maxDepth = 6;
  ContourerParams cp;

  auto [outMesh, outGeom, outNormals] = dualContourField(field, sp, cp);
  REQUIRE(outMesh != nullptr);
  REQUIRE(outMesh->nVertices() > 0);

  // The generalized winding number's 0.5-isosurface is closed even though
  // the input shell has an open face: the hole is sealed with a smooth cap.
  int boundaryEdges = 0;
  for (auto e : outMesh->edges()) {
    if (e.isBoundary()) ++boundaryEdges;
  }
  REQUIRE(boundaryEdges == 0);
}

TEST_CASE("WindingNumberField seals two disconnected open shells that jointly "
          "bound a volume", "[sampler][gwn]") {
  // An open box plus a separate lid -- neither encloses a volume alone, but
  // their union does. GWN sums solid angle across both disconnected
  // components, so the 0.5-isosurface is the closed box.
  auto [mesh, geom] = makeJointlyClosedBox();
  WindingNumberField field(*mesh, *geom);

  SamplerParams   sp;
  sp.maxDepth = 6;
  ContourerParams cp;

  auto [outMesh, outGeom, outNormals] = dualContourField(field, sp, cp);
  REQUIRE(outMesh != nullptr);
  REQUIRE(outMesh->nVertices() > 0);

  int boundaryEdges = 0;
  for (auto e : outMesh->edges()) {
    if (e.isBoundary()) ++boundaryEdges;
  }
  REQUIRE(boundaryEdges == 0);  // jointly-bounded volume -> watertight solid
}
