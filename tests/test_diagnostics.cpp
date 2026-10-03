// Diagnostics channel (roadmap 17 #26): every degradation the pipeline used to
// apply in silence is demonstrated FIRING here, not merely asserted absent.
#include "dualc/pipeline.h"

#include "dualc/implicit.h"
#include "dualc/primitives.h"
#include "dualc/sampler.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <tuple>
#include <vector>

using namespace dualc;

namespace {

using MeshPair =
    std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
               std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>;

const std::vector<Vector3> kTetraPositions = {
    Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
    Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0},
};

// Closed, oriented: 4 faces, 6 edges, every edge shared by exactly 2 faces.
MeshPair makeClosedTetrahedron() {
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              kTetraPositions);
}

// The same tetrahedron with face {1,2,3} removed: 3 faces, and exactly the
// three edges of the missing face are left with one incident triangle.
MeshPair makeOpenTetrahedron() {
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 2, 1}, {0, 1, 3}, {0, 3, 2},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              kTetraPositions);
}

// Three triangles hinged on the shared edge (0,1): that one edge has three
// incident faces, and the six remaining edges have one each.
MeshPair makeNonManifoldFan() {
  std::vector<Vector3> positions = {
      Vector3{0.0, 0.0, 0.0}, Vector3{1.0, 0.0, 0.0},
      Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0},
      Vector3{0.0, -1.0, -1.0},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 1, 2}, {0, 1, 3}, {0, 1, 4},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              positions);
}

// A perfectly good sphere field that reports a bounds() box with max < min --
// neither valid nor the infinite() sentinel, so the sampler takes its
// BBox::unit() fallback.
class InvalidBoundsSphere : public ImplicitField {
public:
  double valueAt(const Vector3& p) const override { return p.norm() - 0.25; }
  Vector3 gradientAt(const Vector3& p) const override {
    const double n = p.norm();
    return (n > 1e-12) ? p * (1.0 / n) : Vector3{0.0, 0.0, 1.0};
  }
  BBox bounds() const override {
    BBox b;
    b.min = Vector3{ 1.0,  1.0,  1.0};
    b.max = Vector3{-1.0, -1.0, -1.0};
    return b;
  }
};

SamplerParams samplerAt(int maxDepth, const BBox& root) {
  SamplerParams sp;
  sp.maxDepth   = maxDepth;
  sp.minDepth   = 2;
  sp.rootBounds = root;
  return sp;
}

const BBox kUnitish{Vector3{-1.5, -1.5, -1.5}, Vector3{1.5, 1.5, 1.5}};
const BBox kTetraBox{Vector3{-0.5, -0.5, -0.5}, Vector3{1.5, 1.5, 1.5}};

} // namespace

// ---------------------------------------------------------------------------
// The control: nothing to report on a clean run.
// ---------------------------------------------------------------------------

TEST_CASE("a closed mesh through a well-bounded contour reports no issue",
          "[diagnostics]") {
  auto [mesh, geom] = makeClosedTetrahedron();
  Diagnostics d;
  auto [outMesh, outGeom, normals] =
      dualContourMesh(*mesh, *geom, samplerAt(5, kTetraBox),
                      ContourerParams{}, &d);

  CHECK_FALSE(d.anyIssue());
  CHECK_FALSE(d.inputEmpty);
  CHECK(d.inputBoundaryEdges == 0);
  CHECK(d.inputNonManifoldEdges == 0);
  CHECK(d.inputWatertight);
  CHECK_FALSE(d.boundsFallback);
  CHECK_FALSE(d.gridBoundsExceeded);
  CHECK_FALSE(d.emptyContour);
  CHECK(d.outputTriangles > 1);
  CHECK(d.outputVertices > 3);
  CHECK(d.outputBoundaryEdges == 0);
  CHECK(d.outputNonManifoldEdges == 0);
  CHECK(d.outputWatertight);
  CHECK(d.outputVertices == outMesh->nVertices());
}

// ---------------------------------------------------------------------------
// Input-mesh topology -- the precondition PSEUDONORMAL assumes and never states.
// ---------------------------------------------------------------------------

TEST_CASE("an open shell reports its boundary edges", "[diagnostics]") {
  auto [mesh, geom] = makeOpenTetrahedron();
  Diagnostics d;
  sampleMeshToHermiteOctree(*mesh, *geom, samplerAt(4, kUnitish), &d);

  CHECK(d.inputBoundaryEdges == 3);     // exactly the removed face's edges
  CHECK(d.inputNonManifoldEdges == 0);
  CHECK_FALSE(d.inputWatertight);
  CHECK_FALSE(d.inputEmpty);
  // Open input is a legitimate case for GENERALIZED_WINDING_NUMBER, so it is
  // reported without being escalated to an "issue".
  CHECK_FALSE(d.anyIssue());
}

TEST_CASE("a three-triangle fan reports its non-manifold edge",
          "[diagnostics]") {
  auto [mesh, geom] = makeNonManifoldFan();
  Diagnostics d;
  sampleMeshToHermiteOctree(*mesh, *geom, samplerAt(4, kUnitish), &d);

  CHECK(d.inputNonManifoldEdges == 1);  // the hinge edge (0,1), 3 faces
  CHECK(d.inputBoundaryEdges == 6);     // the three free edge pairs
  CHECK_FALSE(d.inputWatertight);
}

// ---------------------------------------------------------------------------
// The surviving silent degradations.
// ---------------------------------------------------------------------------

TEST_CASE("an unusable bounds() reports the unit-cube fallback",
          "[diagnostics]") {
  InvalidBoundsSphere field;
  REQUIRE_FALSE(field.bounds().isValid());
  REQUIRE_FALSE(field.bounds().isInfinite());

  SamplerParams sp;          // no rootBounds -> the fallback path
  sp.maxDepth = 5;
  sp.minDepth = 2;

  Diagnostics d;
  HermiteOctree octree = sampleFieldToHermiteOctree(field, sp, &d);

  CHECK(d.boundsFallback);
  CHECK(d.anyIssue());
  // The fallback really is the unit cube, and the r=0.25 sphere does fit
  // inside it -- so a caller gets a plausible mesh with no other hint that
  // the region sampled was not the one the field described.
  REQUIRE(octree.root() != nullptr);
  const BBox root = octree.root()->bounds;
  CHECK(root.min.x == BBox::unit().min.x);
  CHECK(root.max.x == BBox::unit().max.x);
}

TEST_CASE("a valid bounds() does not report the fallback", "[diagnostics]") {
  SphereField field(Vector3{0.0, 0.0, 0.0}, 0.4);
  SamplerParams sp;
  sp.maxDepth = 5;
  sp.minDepth = 2;

  Diagnostics d;
  sampleFieldToHermiteOctree(field, sp, &d);
  CHECK_FALSE(d.boundsFallback);
}

TEST_CASE("an empty contour is distinguishable from a one-triangle surface",
          "[diagnostics]") {
  // A sphere the sampled region never reaches. The contourer synthesizes a
  // placeholder triangle so geometry-central accepts the polygon list; before
  // Diagnostics there was no way to tell that apart from real geometry.
  SphereField faraway(Vector3{0.0, 0.0, 0.0}, 0.4);
  const BBox elsewhere{Vector3{10.0, 10.0, 10.0}, Vector3{11.0, 11.0, 11.0}};

  Diagnostics d;
  auto [mesh, geom, normals] =
      dualContourField(faraway, samplerAt(5, elsewhere), ContourerParams{}, &d);

  CHECK(d.emptyContour);
  CHECK(d.anyIssue());
  // The placeholder is reported as what it is: one open triangle.
  CHECK(d.outputTriangles == 1);
  CHECK(d.outputVertices == 3);
  CHECK(d.outputBoundaryEdges == 3);
  CHECK_FALSE(d.outputWatertight);
  // The pre-#26 host-side check for "no surface" was nFaces() == 0. This is
  // the demonstration that it could never fire.
  CHECK(mesh->nFaces() == 1);
}

TEST_CASE("sampling past a baked grid reports the clamped region",
          "[diagnostics]") {
  SphereField src(Vector3{0.0, 0.0, 0.0}, 0.4);
  const BBox baked{Vector3{-1.0, -1.0, -1.0}, Vector3{1.0, 1.0, 1.0}};
  FieldPtr grid = bakeToGrid(src, baked, 33);

  SECTION("a root box inside the baked region is clean") {
    Diagnostics d;
    sampleFieldToHermiteOctree(
        *grid,
        samplerAt(4, BBox{Vector3{-0.9, -0.9, -0.9}, Vector3{0.9, 0.9, 0.9}}),
        &d);
    CHECK_FALSE(d.gridBoundsExceeded);
    CHECK_FALSE(d.anyIssue());
  }

  SECTION("a root box reaching outside it is reported") {
    Diagnostics d;
    sampleFieldToHermiteOctree(
        *grid,
        samplerAt(4, BBox{Vector3{-3.0, -3.0, -3.0}, Vector3{3.0, 3.0, 3.0}}),
        &d);
    CHECK(d.gridBoundsExceeded);
    CHECK(d.anyIssue());
  }

  SECTION("auto-fit bounds never flag the grid they were fitted to") {
    // The regression this guards: the auto-fit path pads the field's own
    // bounds outward, so a containment test against the bake is false BY
    // CONSTRUCTION and the flag would fire on every default contour of a
    // baked grid -- the always-fires failure this field exists to avoid.
    SamplerParams sp;        // no rootBounds
    sp.maxDepth = 4;
    sp.minDepth = 2;
    Diagnostics d;
    sampleFieldToHermiteOctree(*grid, sp, &d);
    CHECK_FALSE(d.gridBoundsExceeded);
    CHECK_FALSE(d.anyIssue());
  }

  SECTION("a non-grid field is never flagged, however large the root box") {
    Diagnostics d;
    sampleFieldToHermiteOctree(
        src,
        samplerAt(4, BBox{Vector3{-50.0, -50.0, -50.0},
                          Vector3{50.0, 50.0, 50.0}}),
        &d);
    CHECK_FALSE(d.gridBoundsExceeded);
  }
}

TEST_CASE("the input facts come from the MeshSource accessors the sampler reads",
          "[diagnostics]") {
  // `inputEmpty` is exactly `triangleCount() == 0`, and the edge fields are
  // exactly the other two accessors -- pinned here on a known-good mesh.
  // The TRUE branch of inputEmpty is not exercised: geometry-central will not
  // construct a SurfaceMesh from an empty polygon list, so a zero-triangle
  // MeshSource cannot be built through the public API. Noted as such in
  // docs/roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md rather
  // than implied to be verified.
  auto [mesh, geom] = makeClosedTetrahedron();
  MeshSource full(*mesh, *geom);
  REQUIRE(full.triangleCount() == 4);
  REQUIRE(full.boundaryEdgeCount() == 0);
  REQUIRE(full.nonManifoldEdgeCount() == 0);

  // The accessor the sampler reads for inputEmpty is the same one, so the
  // reported flag is exactly `triangleCount() == 0`.
  Diagnostics d;
  sampleMeshToHermiteOctree(*mesh, *geom, samplerAt(4, kTetraBox), &d);
  CHECK_FALSE(d.inputEmpty);
  CHECK(d.inputWatertight);
}

// ---------------------------------------------------------------------------
// The channel must be inert: the same geometry with and without it, at any
// thread count (the guarantee #30 pins).
// ---------------------------------------------------------------------------

TEST_CASE("passing Diagnostics changes neither the mesh nor its determinism",
          "[diagnostics]") {
  SphereField field(Vector3{0.05, -0.03, 0.02}, 0.4);
  const ContourerParams cp;

  auto positionsOf = [&](Diagnostics* d, unsigned threads) {
    SamplerParams sp = samplerAt(6, kUnitish);
    sp.numThreads = threads;
    auto [mesh, geom, normals] = dualContourField(field, sp, cp, d);
    std::vector<Vector3> out;
    out.reserve(mesh->nVertices());
    for (auto v : mesh->vertices()) out.push_back(geom->inputVertexPositions[v]);
    return out;
  };

  const std::vector<Vector3> baseline = positionsOf(nullptr, 1);
  REQUIRE(baseline.size() > 100);

  Diagnostics d1;
  const std::vector<Vector3> withDiag = positionsOf(&d1, 1);
  REQUIRE(withDiag.size() == baseline.size());
  for (std::size_t i = 0; i < baseline.size(); ++i) {
    CHECK(withDiag[i].x == baseline[i].x);
    CHECK(withDiag[i].y == baseline[i].y);
    CHECK(withDiag[i].z == baseline[i].z);
  }

  Diagnostics d4;
  const std::vector<Vector3> multi = positionsOf(&d4, 4);
  REQUIRE(multi.size() == baseline.size());
  for (std::size_t i = 0; i < baseline.size(); ++i) {
    CHECK(multi[i].x == baseline[i].x);
    CHECK(multi[i].y == baseline[i].y);
    CHECK(multi[i].z == baseline[i].z);
  }

  // ...and the report itself is thread-count-independent.
  CHECK(d1.outputVertices == d4.outputVertices);
  CHECK(d1.outputTriangles == d4.outputTriangles);
  CHECK(d1.outputBoundaryEdges == d4.outputBoundaryEdges);
  CHECK(d1.outputWatertight == d4.outputWatertight);
}

// ---------------------------------------------------------------------------
// One instance threaded through a hand-rolled sequence collects the whole
// picture, as the header promises.
// ---------------------------------------------------------------------------

TEST_CASE("one Diagnostics carries across a hand-rolled sample+contour",
          "[diagnostics]") {
  auto [mesh, geom] = makeOpenTetrahedron();
  Diagnostics d;
  SamplerParams sp = samplerAt(5, kTetraBox);
  sp.signMethod = SignMethod::GENERALIZED_WINDING_NUMBER;

  HermiteOctree octree = sampleMeshToHermiteOctree(*mesh, *geom, sp, &d);
  auto [outMesh, outGeom, normals] =
      contourHermiteOctree(octree, ContourerParams{}, &d);

  // Sampler-stage facts survive the contour call...
  CHECK(d.inputBoundaryEdges == 3);
  CHECK_FALSE(d.inputWatertight);
  // ...and the contour stage adds its own. GWN seals the open shell, so the
  // OUTPUT is closed even though the input was not.
  CHECK(d.outputTriangles > 1);
  CHECK(d.outputWatertight);
  CHECK_FALSE(d.emptyContour);
}
