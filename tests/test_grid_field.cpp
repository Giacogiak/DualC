#include "dualc/implicit.h"
#include "dualc/pipeline.h"

#include "internal/distance_grid.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace dualc;
using Catch::Approx;

namespace {

// Axis-aligned unit cube ([-0.5, 0.5]^3) translated so its centre is `c`.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeCube(const Vector3& c) {
  std::vector<Vector3> positions = {
      Vector3{-0.5, -0.5, -0.5} + c, Vector3{0.5, -0.5, -0.5} + c,
      Vector3{0.5, 0.5, -0.5} + c,   Vector3{-0.5, 0.5, -0.5} + c,
      Vector3{-0.5, -0.5, 0.5} + c,  Vector3{0.5, -0.5, 0.5} + c,
      Vector3{0.5, 0.5, 0.5} + c,    Vector3{-0.5, 0.5, 0.5} + c,
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
      {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              positions);
}

// A hollow cube: an outer shell of side `outer` around an inner shell of side
// `inner`, so the solid is the material between them and the middle is an
// enclosed CAVITY. Ray parity crosses two shells to reach the centre -> even ->
// outside, which is what the bake's sign must reproduce.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeHollowCube(double outer, double inner) {
  std::vector<Vector3> positions;
  std::vector<std::vector<std::size_t>> polygons;
  for (const double h : {outer * 0.5, inner * 0.5}) {
    const std::size_t o = positions.size();
    positions.insert(positions.end(),
                     {Vector3{-h, -h, -h}, Vector3{h, -h, -h},
                      Vector3{h, h, -h},   Vector3{-h, h, -h},
                      Vector3{-h, -h, h},  Vector3{h, -h, h},
                      Vector3{h, h, h},    Vector3{-h, h, h}});
    for (const auto& f : {std::vector<std::size_t>{0, 3, 2, 1},
                          {4, 5, 6, 7}, {0, 1, 5, 4},
                          {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}}) {
      std::vector<std::size_t> poly;
      for (const std::size_t v : f) poly.push_back(o + v);
      polygons.push_back(std::move(poly));
    }
  }
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              positions);
}

} // namespace

TEST_CASE("bakeToGrid reproduces the source field within cell size",
          "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-2.0, -2.0, -2.0}, Vector3{2.0, 2.0, 2.0}};
  FieldPtr grid = bakeToGrid(src, region, 129);   // cell ~= 0.031

  // Smooth, non-edge sample points: the trilinear grid should track the true
  // signed distance closely.
  const Vector3 probes[] = {Vector3{0.0, 0.0, 0.0},
                            Vector3{1.0, 0.0, 0.0},
                            Vector3{0.0, 1.2, 0.0},
                            Vector3{0.0, 0.0, -1.5}};
  for (const Vector3& p : probes) {
    REQUIRE(grid->valueAt(p) == Approx(src.valueAt(p)).margin(0.05));
  }
}

TEST_CASE("GridField::bounds equals the bake region", "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-1.5, -2.0, -2.5}, Vector3{1.5, 2.0, 2.5}};
  FieldPtr grid = bakeToGrid(src, region, 32);

  const BBox b = grid->bounds();
  REQUIRE(b.min.x == Approx(-1.5));
  REQUIRE(b.min.y == Approx(-2.0));
  REQUIRE(b.min.z == Approx(-2.5));
  REQUIRE(b.max.x == Approx(1.5));
  REQUIRE(b.max.y == Approx(2.0));
  REQUIRE(b.max.z == Approx(2.5));
}

TEST_CASE("GridField::gradientAt is unit length and outward", "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-2.0, -2.0, -2.0}, Vector3{2.0, 2.0, 2.0}};
  FieldPtr grid = bakeToGrid(src, region, 129);

  const Vector3 g = grid->gradientAt(Vector3{1.4, 0.0, 0.0});
  REQUIRE(g.norm() == Approx(1.0).margin(1e-6));
  REQUIRE(g.x == Approx(1.0).margin(0.1));
  REQUIRE(g.y == Approx(0.0).margin(0.1));
  REQUIRE(g.z == Approx(0.0).margin(0.1));
}

TEST_CASE("GridField clamps queries outside the region", "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-2.0, -2.0, -2.0}, Vector3{2.0, 2.0, 2.0}};
  FieldPtr grid = bakeToGrid(src, region, 48);

  const double v = grid->valueAt(Vector3{100.0, -250.0, 80.0});
  REQUIRE(std::isfinite(v));
  REQUIRE(v > 0.0);   // far outside the cube -> positive (outside) distance
}

TEST_CASE("smooth boolean of two baked mesh grids contours to a mesh",
          "[grid]") {
  auto [meshA, geomA] = makeCube(Vector3{-0.3, 0.0, 0.0});
  auto [meshB, geomB] = makeCube(Vector3{0.3, 0.0, 0.0});
  MeshSource srcA(*meshA, *geomA);
  MeshSource srcB(*meshB, *geomB);

  const BBox region{Vector3{-1.5, -1.2, -1.2}, Vector3{1.5, 1.2, 1.2}};
  FieldPtr ga = bakeToGrid(srcA, region, 64);
  FieldPtr gb = bakeToGrid(srcB, region, 64);

  FieldPtr field = smoothUnionOf(ga, gb, 0.3);

  SamplerParams sp;
  sp.maxDepth = 5;
  ContourerParams cp;
  auto [mesh, geom, normals] = dualContourField(*field, sp, cp);

  REQUIRE(mesh->nFaces() > 0);
  REQUIRE(mesh->nVertices() > 0);
}

// ===========================================================================
// Narrow-band bake (MeshSource::bakeToGrid)
// ===========================================================================

TEST_CASE("narrow-band bake matches valueAt near the surface", "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-2.5, -2.5, -2.5}, Vector3{2.5, 2.5, 2.5}};
  FieldPtr grid = src.bakeToGrid(region, Vector3i{96, 96, 96}, 0.3);

  // Points within the band (|distance| < 0.3 from the x = 0.5 face) must
  // carry the exact signed distance, up to grid resolution.
  const Vector3 nearProbes[] = {Vector3{0.56, 0.0, 0.0},   // just outside
                                Vector3{0.44, 0.0, 0.0},   // just inside
                                Vector3{0.0, 0.62, 0.0}};
  for (const Vector3& p : nearProbes) {
    REQUIRE(grid->valueAt(p) == Approx(src.valueAt(p)).margin(0.08));
  }
}

TEST_CASE("narrow-band bake gets far-field sign right", "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-2.5, -2.5, -2.5}, Vector3{2.5, 2.5, 2.5}};
  FieldPtr grid = src.bakeToGrid(region, Vector3i{96, 96, 96}, 0.3);

  // The cube centre is far from any face (0.5 > band 0.3) -> far-inside.
  REQUIRE(grid->valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);
  // Well outside the cube -> far-outside.
  REQUIRE(grid->valueAt(Vector3{2.0, 0.0, 0.0}) > 0.0);
  REQUIRE(grid->valueAt(Vector3{0.0, -1.8, 1.5}) > 0.0);
}

TEST_CASE("narrow-band bake is monotone leaving the surface", "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-2.5, -2.5, -2.5}, Vector3{2.5, 2.5, 2.5}};
  FieldPtr grid = src.bakeToGrid(region, Vector3i{96, 96, 96}, 0.3);

  // Marching out along +x from the x = 0.5 face: the field stays positive
  // and never decreases -- no spurious zero crossing at the band boundary.
  double prev = -1e9;
  for (double x = 0.6; x <= 2.4; x += 0.1) {
    const double v = grid->valueAt(Vector3{x, 0.0, 0.0});
    REQUIRE(v > 0.0);
    REQUIRE(v >= prev - 1e-6);
    prev = v;
  }
}

// The bake region is NOT required to be padded around the mesh: the GPU
// preview bakes over the raymarch box, which is routinely a sub-box of the
// mesh (`intersection(mesh, smaller_shape)`, or a --bounds zoom). The three
// cases below pin every region/mesh relationship.

TEST_CASE("narrow-band bake signs a region strictly inside the solid",
          "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  // Deep interior box: the nearest face is 0.3 away, so no voxel is banded and
  // the whole grid is one component. Every voxel must read inside.
  const BBox region{Vector3{-0.2, -0.2, -0.2}, Vector3{0.2, 0.2, 0.2}};
  FieldPtr grid = src.bakeToGrid(region, Vector3i{16, 16, 16}, 0.05);

  REQUIRE(grid->valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);
  REQUIRE(grid->valueAt(Vector3{0.1, -0.05, 0.15}) < 0.0);
  for (const double x : {-0.2, 0.2})
    for (const double y : {-0.2, 0.2})
      for (const double z : {-0.2, 0.2})
        REQUIRE(grid->valueAt(Vector3{x, y, z}) < 0.0);
}

TEST_CASE("narrow-band bake signs a region only partly containing the mesh",
          "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  // +x/+y/+z faces are genuinely outside the cube; -x/-y/-z cut through its
  // interior, so the interior component TOUCHES a region face.
  const BBox region{Vector3{-0.2, -0.2, -0.2}, Vector3{2.0, 2.0, 2.0}};
  FieldPtr grid = src.bakeToGrid(region, Vector3i{48, 48, 48}, 0.06);

  REQUIRE(grid->valueAt(Vector3{-0.1, -0.1, -0.1}) < 0.0);  // inside the cube
  REQUIRE(grid->valueAt(Vector3{1.5, 1.5, 1.5}) > 0.0);     // far outside
}

TEST_CASE("narrow-band bake with the region flush with the mesh AABB",
          "[grid]") {
  auto [mesh, geom] = makeCube(Vector3{0.0, 0.0, 0.0});
  MeshSource src(*mesh, *geom);

  // Region == the mesh AABB exactly: every face voxel sits on the surface and
  // is banded, so there is no non-band voxel on any face to seed from.
  const BBox region{Vector3{-0.5, -0.5, -0.5}, Vector3{0.5, 0.5, 0.5}};
  FieldPtr grid = src.bakeToGrid(region, Vector3i{32, 32, 32}, 0.05);

  REQUIRE(grid->valueAt(Vector3{0.0, 0.0, 0.0}) < 0.0);
  REQUIRE(grid->valueAt(Vector3{0.5, 0.0, 0.0}) == Approx(0.0).margin(0.05));
}

TEST_CASE("narrow-band bake leaves an enclosed cavity empty", "[grid]") {
  // Hollow cube: solid between side 1.0 and side 0.4, cavity in the middle.
  auto [mesh, geom] = makeHollowCube(1.0, 0.4);
  MeshSource src(*mesh, *geom);

  const BBox region{Vector3{-1.0, -1.0, -1.0}, Vector3{1.0, 1.0, 1.0}};
  FieldPtr grid = src.bakeToGrid(region, Vector3i{64, 64, 64}, 0.05);

  // The cavity centre is enclosed by the band on all sides. Sign now comes from
  // an actual parity query (two shell crossings -> outside), where the old
  // face-seeded flood inferred "unreachable from the region faces => inside"
  // and filled the cavity with material.
  REQUIRE(grid->valueAt(Vector3{0.0, 0.0, 0.0}) > 0.0);
  REQUIRE(grid->valueAt(Vector3{0.0, 0.0, 0.0}) ==
          Approx(src.valueAt(Vector3{0.0, 0.0, 0.0})).margin(0.1));
  // ... while the shell material between the two surfaces stays inside.
  REQUIRE(grid->valueAt(Vector3{0.35, 0.0, 0.0}) < 0.0);
  // ... and the far field outside the outer shell stays outside.
  REQUIRE(grid->valueAt(Vector3{0.9, 0.9, 0.9}) > 0.0);
}

TEST_CASE("narrow-band smooth boolean contours to a closed mesh", "[grid]") {
  auto [meshA, geomA] = makeCube(Vector3{-0.3, 0.0, 0.0});
  auto [meshB, geomB] = makeCube(Vector3{0.3, 0.0, 0.0});
  MeshSource srcA(*meshA, *geomA);
  MeshSource srcB(*meshB, *geomB);

  const BBox region{Vector3{-1.5, -1.2, -1.2}, Vector3{1.5, 1.2, 1.2}};
  const double k = 0.3;
  FieldPtr ga = srcA.bakeToGrid(region, Vector3i{80, 80, 80}, k + 0.1);
  FieldPtr gb = srcB.bakeToGrid(region, Vector3i{80, 80, 80}, k + 0.1);

  FieldPtr field = smoothUnionOf(ga, gb, k);

  SamplerParams sp;
  sp.maxDepth = 6;
  ContourerParams cp;
  auto [mesh, geom, normals] = dualContourField(*field, sp, cp);

  REQUIRE(mesh->nFaces() > 0);
  REQUIRE(mesh->nVertices() > 0);
}

// ===========================================================================
// distance_grid.h helpers
// ===========================================================================

TEST_CASE("floodFarSign separates an enclosed voxel from the outside",
          "[distgrid]") {
  // 5^3 grid; band = the hollow shell of voxels at Chebyshev radius 1 from
  // the centre. The centre voxel is enclosed; the outer layer is open.
  const Vector3i res{5, 5, 5};
  std::vector<char> band(125, 0);
  auto cheby = [](int x, int y, int z) {
    return std::max({std::abs(x - 2), std::abs(y - 2), std::abs(z - 2)});
  };
  for (int z = 0; z < 5; ++z)
    for (int y = 0; y < 5; ++y)
      for (int x = 0; x < 5; ++x)
        if (cheby(x, y, z) == 1)
          band[dualc::internal::gridIndex(res, x, y, z)] = 1;

  std::vector<signed char> farSign;

  SECTION("only the enclosed voxel is inside") {
    dualc::internal::floodFarSign(res, band, farSign, [&](std::size_t i) {
      return i == dualc::internal::gridIndex(res, 2, 2, 2);
    });
    REQUIRE(farSign[dualc::internal::gridIndex(res, 2, 2, 2)] == -1);
    REQUIRE(farSign[dualc::internal::gridIndex(res, 0, 0, 0)] == 1);
    REQUIRE(farSign[dualc::internal::gridIndex(res, 4, 2, 2)] == 1);
  }

  SECTION("the outer layer is inside too -- the sign comes from the query, "
          "not from touching a region face") {
    dualc::internal::floodFarSign(res, band, farSign,
                                  [](std::size_t) { return true; });
    REQUIRE(farSign[dualc::internal::gridIndex(res, 2, 2, 2)] == -1);
    REQUIRE(farSign[dualc::internal::gridIndex(res, 0, 0, 0)] == -1);
    REQUIRE(farSign[dualc::internal::gridIndex(res, 4, 2, 2)] == -1);
  }
}

TEST_CASE("chamferGrow keeps band seeds and grows monotonically",
          "[distgrid]") {
  const Vector3i res{5, 5, 5};
  const Vector3 cell{1.0, 1.0, 1.0};
  std::vector<char>  band(125, 0);
  std::vector<float> bandAbs(125, 0.0f);
  auto cheby = [](int x, int y, int z) {
    return std::max({std::abs(x - 2), std::abs(y - 2), std::abs(z - 2)});
  };
  for (int z = 0; z < 5; ++z)
    for (int y = 0; y < 5; ++y)
      for (int x = 0; x < 5; ++x)
        if (cheby(x, y, z) == 1) {
          const std::size_t i = dualc::internal::gridIndex(res, x, y, z);
          band[i]    = 1;
          bandAbs[i] = 0.25f;
        }

  std::vector<float> dist;
  dualc::internal::chamferGrow(res, cell, band, bandAbs, dist);

  // Band voxels keep their exact magnitude.
  REQUIRE(dist[dualc::internal::gridIndex(res, 1, 2, 2)] == Approx(0.25f));
  // Far voxels grow strictly beyond the band seed.
  const float centre = dist[dualc::internal::gridIndex(res, 2, 2, 2)];
  const float corner = dist[dualc::internal::gridIndex(res, 0, 0, 0)];
  REQUIRE(centre > 0.25f);
  REQUIRE(corner > 0.25f);
}
