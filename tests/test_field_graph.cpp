#include "field_graph.h"

#include "dualc/pipeline.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

// Tests for the field-graph parser/builder (examples/field_graph.{h,cpp}). The
// builder is wiring over the example_common registries + implicit.h factories;
// these tests cover the wiring contracts -- JSON shape, node dispatch, partial
// nodes, mesh-source resolution/caching, and the located error diagnostics.

using namespace dce::fieldgraph;
using dualc::Vector3;

namespace {

// Axis-aligned cube of side 2*half centred at the origin.
std::tuple<std::unique_ptr<geometrycentral::surface::SurfaceMesh>,
           std::unique_ptr<geometrycentral::surface::VertexPositionGeometry>>
makeBox(double half) {
  const double h = half;
  std::vector<Vector3> positions = {
      {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
      {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h},
  };
  std::vector<std::vector<std::size_t>> polygons = {
      {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
      {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
  };
  return geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons,
                                                              positions);
}

// In-memory resolver: hands back one fixed mesh regardless of path, counting
// calls (lets the mesh/winding builder paths be tested without file I/O).
struct FixedResolver : MeshResolver {
  geometrycentral::surface::SurfaceMesh* mesh;
  geometrycentral::surface::VertexPositionGeometry* geom;
  int calls = 0;
  FixedResolver(geometrycentral::surface::SurfaceMesh* m,
                geometrycentral::surface::VertexPositionGeometry* g)
      : mesh(m), geom(g) {}
  MeshHandle resolve(const std::string&) override {
    ++calls;
    return MeshHandle{mesh, geom};
  }
};

}  // namespace

TEST_CASE("field-graph builds and contours a composed analytic graph",
          "[field_graph]") {
  const std::string g = R"({
    "version": 1, "units": "mm",
    "root": { "op": "difference", "in": [
      { "op": "intersection", "in": [
        { "op": "box", "min": [-2,-2,-2], "max": [2,2,2] },
        { "op": "onion", "thickness": 0.3, "in": [
          { "op": "normalize", "in": [
            { "op": "gyroid", "wavelength": 1.0 } ] } ] } ] },
      { "op": "sphere", "center": [0,0,0], "radius": 1.0 } ] } })";

  FixedResolver meshes{nullptr, nullptr};  // not used by an all-analytic graph
  FieldGraph fg = FieldGraph::build(parseJson(g), meshes);

  dualc::SamplerParams sp;
  sp.maxDepth = 5;
  dualc::ContourerParams cp;
  auto [mesh, geom, normals] = dualc::dualContourField(fg.field(), sp, cp);
  REQUIRE(mesh->nVertices() > 0);
  REQUIRE(mesh->nFaces() > 0);
  REQUIRE(meshes.calls == 0);

  // Pin the boolean wiring (not just "non-empty"): the carved sphere makes the
  // origin solidly OUTSIDE (difference subtracts it -> would be inside if the
  // top op were a union), and the box clip makes a far point OUTSIDE (would be
  // in an infinite gyroid wall if the intersection-with-box were dropped).
  REQUIRE(fg.field().valueAt(Vector3{0, 0, 0}) > 0.0);
  REQUIRE(fg.field().valueAt(Vector3{10, 0, 0}) > 0.0);
}

TEST_CASE("partial node uses registry defaults", "[field_graph]") {
  FixedResolver meshes{nullptr, nullptr};
  FieldGraph fg = FieldGraph::build(parseJson(R"({"op":"sphere"})"), meshes);
  // {"op":"sphere"} == the unit sphere at the origin.
  REQUIRE(fg.field().valueAt(Vector3{0, 0, 0}) < 0.0);
  REQUIRE(fg.field().valueAt(Vector3{2, 0, 0}) > 0.0);
}

TEST_CASE("mesh and winding source nodes build a solid field",
          "[field_graph]") {
  auto box = makeBox(1.0);
  FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};

  FieldGraph mf = FieldGraph::build(parseJson(R"({"op":"mesh","path":"x"})"),
                                    meshes);
  REQUIRE(mf.field().valueAt(Vector3{0, 0, 0}) < 0.0);    // inside the cube
  REQUIRE(mf.field().valueAt(Vector3{5, 0, 0}) > 0.0);    // outside

  FieldGraph wf = FieldGraph::build(parseJson(R"({"op":"winding","path":"x"})"),
                                    meshes);
  REQUIRE(wf.field().valueAt(Vector3{0, 0, 0}) < 0.0);    // GWN inside
  REQUIRE(meshes.calls == 2);
}

TEST_CASE("mesh sign=gwn is the GWN sign oracle on a metric field",
          "[field_graph][gwn]") {
  // On a closed, outward mesh the GWN oracle agrees with parity everywhere the
  // sampler looks, so the contoured outputs are identical (#56).
  {
    auto box = makeBox(1.0);
    FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};
    FieldGraph parity = FieldGraph::build(
        parseShorthand("mesh(path=\"x\",sign=parity)"), meshes);
    FieldGraph gwn = FieldGraph::build(
        parseShorthand("mesh(path=\"x\",sign=gwn)"), meshes);

    dualc::SamplerParams sp;
    sp.maxDepth = 5;
    dualc::ContourerParams cp;
    auto [pMesh, pGeom, pNormals] = dualc::dualContourField(parity.field(), sp, cp);
    auto [gMesh, gGeom, gNormals] = dualc::dualContourField(gwn.field(), sp, cp);
    REQUIRE(pMesh->nFaces() > 0);
    REQUIRE(gMesh->nFaces() == pMesh->nFaces());
    REQUIRE(gMesh->nVertices() == pMesh->nVertices());
    for (std::size_t i = 0; i < pMesh->nVertices(); ++i)
      REQUIRE(gGeom->vertexPositions[i] == pGeom->vertexPositions[i]);
  }

  // On an open shell (the cube without its +z face) it stays a sign oracle on
  // the distance to the shell, which is NOT the `winding` node's sealing field
  // 0.5 - w: at the centre the walls are 1 away (mesh: -1) while the shell
  // subtends 5/6 of the sphere (winding: 0.5 - 5/6 = -1/3).
  {
    std::vector<Vector3> positions = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
    };
    std::vector<std::vector<std::size_t>> polygons = {
        {0, 3, 2, 1}, {0, 1, 5, 4}, {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5},
    };
    auto open =
        geometrycentral::surface::makeSurfaceMeshAndGeometry(polygons, positions);
    FixedResolver meshes{std::get<0>(open).get(), std::get<1>(open).get()};
    FieldGraph gwn = FieldGraph::build(
        parseShorthand("mesh(path=\"x\",sign=gwn)"), meshes);
    FieldGraph winding =
        FieldGraph::build(parseShorthand("winding(path=\"x\")"), meshes);
    const Vector3 centre{0, 0, 0};
    REQUIRE(gwn.field().valueAt(centre) == Catch::Approx(-1.0).margin(1e-9));
    REQUIRE(winding.field().valueAt(centre) ==
            Catch::Approx(-1.0 / 3.0).margin(0.05));
    // Far outside both read outside, but only `mesh` is a distance; `winding`
    // stays near 0.5 - 0 (the hierarchical GWN reads 0.5025 here).
    REQUIRE(gwn.field().valueAt(Vector3{5, 0, 0}) ==
            Catch::Approx(4.0).margin(1e-9));
    REQUIRE(winding.field().valueAt(Vector3{5, 0, 0}) ==
            Catch::Approx(0.5).margin(0.05));
  }
}

TEST_CASE("mesh and winding bake=N sample the node into its own grid",
          "[field_graph][bake]") {
  // #57: `bake=N` replaces the source with a GridField over the node's own
  // bounds padded by 10 % of their largest extent -- not the sampling box,
  // not a sibling's box.
  auto box = makeBox(1.0);
  FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};
  FieldGraph exact = FieldGraph::build(parseShorthand("mesh(path=\"x\")"), meshes);
  FieldGraph baked =
      FieldGraph::build(parseShorthand("mesh(path=\"x\",bake=32)"), meshes);

  auto* grid = dynamic_cast<const dualc::GridField*>(&baked.field());
  REQUIRE(grid != nullptr);
  REQUIRE(grid->resolution().x == 32);
  REQUIRE(grid->resolution().z == 32);
  const dualc::BBox r = grid->bounds();   // [-1,1]^3 padded by 0.1 * 2
  REQUIRE(r.min.x == Catch::Approx(-1.2));
  REQUIRE(r.max.z == Catch::Approx(1.2));
  const double cell = 2.4 / 31.0;

  // Near the surface the baked field is within one bake cell of the exact one,
  // and it never flips the sign more than a cell away from the surface.
  int nearCount = 0;
  for (int i = 0; i < 23; ++i)
    for (int j = 0; j < 23; ++j)
      for (int k = 0; k < 23; ++k) {
        const Vector3 p{-1.15 + i * 0.1037, -1.15 + j * 0.1037,
                        -1.15 + k * 0.1037};
        const double e = exact.field().valueAt(p);
        const double b = baked.field().valueAt(p);
        if (std::abs(e) < 2.0 * cell) {
          ++nearCount;
          REQUIRE(std::abs(b - e) <= cell);
        } else {
          REQUIRE((b < 0.0) == (e < 0.0));
        }
      }
  REQUIRE(nearCount > 100);

  // `band` is the exact-distance shell: lattice nodes inside it hold the exact
  // distance (to float precision), here 0.5 -- 4.6 cells -- deep. It also
  // widens the region by itself, so it is the margin a smooth parent's k
  // needs: [-1,1] + 0.2 + 0.5.
  FieldGraph banded = FieldGraph::build(
      parseShorthand("mesh(path=\"x\",bake=32,band=0.5)"), meshes);
  auto* bgrid = dynamic_cast<const dualc::GridField*>(&banded.field());
  REQUIRE(bgrid != nullptr);
  REQUIRE(bgrid->bounds().min.y == Catch::Approx(-1.7));
  REQUIRE(bgrid->bounds().max.x == Catch::Approx(1.7));
  const double bcell = 3.4 / 31.0;
  int inBand = 0;
  for (int i = 0; i < 32; ++i) {
    const double c = -1.7 + i * bcell;
    const Vector3 p{c, -1.7 + 15 * bcell, -1.7 + 15 * bcell};
    const double e = exact.field().valueAt(p);
    if (std::abs(e) >= 0.45) continue;
    ++inBand;
    REQUIRE(banded.field().valueAt(p) == Catch::Approx(e).margin(1e-5));
  }
  REQUIRE(inBand > 6);

  // winding bakes the full lattice of its own (already padded) bounds; away
  // from the surface its sign is the exact field's.
  FieldGraph wExact = FieldGraph::build(parseShorthand("winding(path=\"x\")"), meshes);
  FieldGraph wBaked =
      FieldGraph::build(parseShorthand("winding(path=\"x\",bake=24)"), meshes);
  auto* wgrid = dynamic_cast<const dualc::GridField*>(&wBaked.field());
  REQUIRE(wgrid != nullptr);
  REQUIRE(wgrid->resolution().y == 24);
  const dualc::BBox wb = wExact.field().bounds();
  const double wpad = 0.1 * std::max({wb.extent().x, wb.extent().y, wb.extent().z});
  REQUIRE(wgrid->bounds().min.x == Catch::Approx(wb.min.x - wpad));
  REQUIRE(wgrid->bounds().max.y == Catch::Approx(wb.max.y + wpad));
  const double wcell = wgrid->bounds().extent().x / 23.0;
  for (int i = 0; i < 15; ++i) {
    const Vector3 p{-1.4 + i * 0.2, 0.13, -0.07};
    if (std::abs(exact.field().valueAt(p)) < 1.5 * wcell) continue;
    REQUIRE((wBaked.field().valueAt(p) < 0.0) == (wExact.field().valueAt(p) < 0.0));
  }
}

TEST_CASE("childBakeMargin hands each child the distance its ancestors read",
          "[field_graph][bake]") {
  // #57, D-60's companion decision (b): smooth ops hand down max(margin, k),
  // offset / round / onion / displace add their amount, scale divides by |s|,
  // the graded ops add max(|t1|,|t2|) to the base only, all else passes it on.
  auto margin = [](const std::string& expr, std::size_t i, double m) {
    return childBakeMargin(parseShorthand(expr), i, m);
  };
  const std::string a = "sphere(radius=1)", b = "sphere(radius=2)";
  REQUIRE(margin("union(" + a + "," + b + ")", 0, 0.0) == 0.0);
  REQUIRE(margin("difference(" + a + "," + b + ")", 1, 0.4) == 0.4);
  REQUIRE(margin("smooth-union(" + a + "," + b + ",k=1)", 0, 0.0) == 1.0);
  REQUIRE(margin("smooth-difference(" + a + "," + b + ",k=0.5)", 1, 2.0) == 2.0);
  REQUIRE(margin("smooth-intersection(" + a + "," + b + ")", 0, 0.0) == 0.25);
  REQUIRE(margin("scale(" + a + ",s=2)", 0, 1.0) == 0.5);
  REQUIRE(margin("scale(" + a + ",s=-4)", 0, 1.0) == 0.25);
  REQUIRE(margin("offset(" + a + ",r=0.3)", 0, 0.5) == Catch::Approx(0.8));
  REQUIRE(margin("offset(" + a + ",r=-0.3)", 0, 0.5) == 0.5);
  REQUIRE(margin("round(" + a + ",r=0.2)", 0, 0.0) == Catch::Approx(0.2));
  REQUIRE(margin("onion(" + a + ",thickness=0.1)", 0, 0.0) == Catch::Approx(0.1));
  REQUIRE(margin("displace(" + a + ",amplitude=0.05)", 0, 0.0) ==
          Catch::Approx(0.05));
  const std::string g = "graded-onion(" + a + ",plane(0,1,0,0),t1=0.3,t2=0.1,d1=1)";
  REQUIRE(margin(g, 0, 0.0) == Catch::Approx(0.3));
  REQUIRE(margin(g, 1, 0.0) == 0.0);
  REQUIRE(margin("translate(" + a + ",by=[1,0,0])", 0, 0.7) == 0.7);
  REQUIRE(margin("normalize(" + a + ")", 0, 0.7) == 0.7);
}

TEST_CASE("a baked child inherits its ancestors' margin",
          "[field_graph][bake]") {
  // Outside its region a grid clamps to the face value, which does not grow
  // with distance; an ancestor reading below it -- a smooth blend of k, an
  // offset or onion of r -- makes material out of the clamp (measured on
  // smooth-union torus cube, k=1: 3x the faces, max deviation 1.56 -- roadmap
  // 21/04). The builder now hands the margin down, so the default band is
  // right through nesting; the bare library composition still shows the trap.
  auto box = makeBox(1.0);
  FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};
  const std::string other = "translate(sphere(radius=0.5),by=[2.2,0,0])";
  auto signsAgree = [&](const std::string& exactExpr,
                        const std::string& bakedExpr, double skip) {
    FieldGraph exact = FieldGraph::build(parseShorthand(exactExpr), meshes);
    FieldGraph baked = FieldGraph::build(parseShorthand(bakedExpr), meshes);
    int samples = 0;
    for (int i = 0; i < 31; ++i)
      for (int j = 0; j < 31; ++j)
        for (int k = 0; k < 31; ++k) {
          const Vector3 p{-2.9 + i * 0.2, -2.9 + j * 0.2, -2.9 + k * 0.2};
          const double e = exact.field().valueAt(p);
          if (std::abs(e) < skip) continue;
          ++samples;
          INFO(bakedExpr << " at " << p.x << "," << p.y << "," << p.z);
          REQUIRE((baked.field().valueAt(p) < 0.0) == (e < 0.0));
        }
    REQUIRE(samples > 15000);
  };
  const std::string m = "mesh(path=\"x\")", mb = "mesh(path=\"x\",bake=32)";
  // Directly under the smooth op, through a rigid move, through a scale, and
  // under a nested smooth op whose outer k is the larger.
  signsAgree("smooth-union(" + m + "," + other + ",k=1)",
             "smooth-union(" + mb + "," + other + ",k=1)", 0.25);
  signsAgree("smooth-union(translate(" + m + ",by=[0.3,0,0])," + other + ",k=1)",
             "smooth-union(translate(" + mb + ",by=[0.3,0,0])," + other + ",k=1)",
             0.25);
  signsAgree("smooth-union(scale(" + m + ",s=0.6)," + other + ",k=1)",
             "smooth-union(scale(" + mb + ",s=0.6)," + other + ",k=1)", 0.25);
  signsAgree("smooth-union(smooth-union(" + m + ",sphere(radius=0.3),k=0.1)," +
                 other + ",k=1)",
             "smooth-union(smooth-union(" + mb + ",sphere(radius=0.3),k=0.1)," +
                 other + ",k=1)",
             0.25);
  // An offset or onion wider than the 10 % pad: without the margin these
  // contoured to 1 and 588 faces against the exact 9,804 and 10,392.
  signsAgree("offset(" + m + ",r=0.3)", "offset(" + mb + ",r=0.3)", 0.25);
  signsAgree("onion(" + m + ",thickness=0.3)", "onion(" + mb + ",thickness=0.3)",
             0.25);

  // The trap itself, on the library composition with no margin handed down.
  dualc::MeshSource src(*std::get<0>(box), *std::get<1>(box), true,
                        dualc::SignMethod::WINDING_NUMBER);
  NodeBake nb;
  nb.res = 32;
  FieldGraph exact = FieldGraph::build(
      parseShorthand("smooth-union(" + m + "," + other + ",k=1)"), meshes);
  FieldGraph otherField = FieldGraph::build(parseShorthand(other), meshes);
  dualc::FieldPtr bare = dualc::smoothUnionOf(bakeMeshNode(src, nb, 0.0),
                                              otherField.fieldPtr(), 1.0);
  int wrong = 0;
  for (int i = 0; i < 31; ++i)
    for (int j = 0; j < 31; ++j)
      for (int k = 0; k < 31; ++k) {
        const Vector3 p{-2.9 + i * 0.2, -2.9 + j * 0.2, -2.9 + k * 0.2};
        const double e = exact.field().valueAt(p);
        if (std::abs(e) >= 0.25 && (bare->valueAt(p) < 0.0) != (e < 0.0)) ++wrong;
      }
  REQUIRE(wrong > 0);
}

TEST_CASE("an explicit band larger than the margin wins",
          "[field_graph][bake]") {
  auto box = makeBox(1.0);
  FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};
  dualc::MeshSource src(*std::get<0>(box), *std::get<1>(box), true,
                        dualc::SignMethod::WINDING_NUMBER);
  NodeBake nb;
  nb.res = 16;
  nb.band = 2.0;
  dualc::FieldPtr bigPtr = bakeMeshNode(src, nb, 1.0);
  auto big = dynamic_cast<const dualc::GridField*>(bigPtr.get());
  REQUIRE(big != nullptr);
  REQUIRE(big->bounds().min.x == Catch::Approx(-1.0 - 0.2 - 2.0));
  nb.band = 0.5;
  dualc::FieldPtr smallPtr = bakeMeshNode(src, nb, 1.0);
  auto small = dynamic_cast<const dualc::GridField*>(smallPtr.get());
  REQUIRE(small != nullptr);
  REQUIRE(small->bounds().max.y == Catch::Approx(1.0 + 0.2 + 1.0));
  // winding has no band: its region pads by the margin alone.
  FieldGraph w = FieldGraph::build(parseShorthand("winding(path=\"x\")"), meshes);
  dualc::FieldPtr wPtr = bakeWindingNode(w.field(), nb, 0.75);
  const dualc::BBox wb = w.field().bounds();
  const double wpad =
      0.1 * std::max({wb.extent().x, wb.extent().y, wb.extent().z}) + 0.75;
  REQUIRE(wPtr->bounds().min.z == Catch::Approx(wb.min.z - wpad));
}

TEST_CASE("mesh bake round-trips through dumpJson + parseJson",
          "[field_graph][bake]") {
  auto box = makeBox(1.0);
  FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};
  GraphNode viaShort = parseShorthand(
      "difference(sphere(radius=1.3),mesh(path=\"x\",bake=20,band=0.4))");
  const std::string json = dumpJson(viaShort);
  REQUIRE(json.find("\"bake\"") != std::string::npos);
  REQUIRE(json.find("\"band\"") != std::string::npos);
  GraphNode viaJson = parseJson(json);
  FieldGraph a = FieldGraph::build(viaShort, meshes);
  FieldGraph b = FieldGraph::build(viaJson, meshes);
  for (const Vector3& p : {Vector3{0, 0, 0}, Vector3{1.1, 0.3, -0.2},
                           Vector3{0.9, 0.9, 0.9}, Vector3{3, 0, 0}})
    REQUIRE(a.field().valueAt(p) == b.field().valueAt(p));
}

TEST_CASE("mesh and winding bake reject bad values with a located error",
          "[field_graph][bake]") {
  auto box = makeBox(1.0);
  FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};
  // Each error must be located on the baked node, not on its parent.
  std::string childPointer;
  auto errorOf = [&](const std::string& child) -> GraphError {
    const GraphNode g = parseShorthand("union(sphere(radius=1)," + child + ")");
    childPointer = g.in.at(1).pointer;
    REQUIRE(!childPointer.empty());
    REQUIRE(childPointer != g.pointer);
    try {
      FieldGraph::build(g, meshes);
    } catch (const GraphError& e) {
      return e;
    }
    FAIL("expected a GraphError for " + child);
    return GraphError("", "");
  };
  for (const char* bad :
       {"mesh(path=\"x\",bake=1)", "mesh(path=\"x\",bake=513)",
        "mesh(path=\"x\",bake=32.5)", "winding(path=\"x\",bake=0)",
        "winding(path=\"x\",bake=-8)"}) {
    GraphError e = errorOf(bad);
    INFO(bad << " -> " << e.what());
    REQUIRE(std::string(e.what()).find("integer in [2, 512]") != std::string::npos);
    REQUIRE(e.pointer() == childPointer);
  }
  {
    GraphError e = errorOf("mesh(path=\"x\",band=0.3)");
    REQUIRE(std::string(e.what()).find("'band' needs 'bake'") != std::string::npos);
    REQUIRE(e.pointer() == childPointer);
  }
  for (const char* bad :
       {"mesh(path=\"x\",bake=16,band=0)", "mesh(path=\"x\",bake=16,band=-1)"}) {
    GraphError e = errorOf(bad);
    INFO(bad << " -> " << e.what());
    REQUIRE(std::string(e.what()).find("'band' must be a positive") !=
            std::string::npos);
    REQUIRE(e.pointer() == childPointer);
  }
  REQUIRE(meshes.calls == 0);   // every check above runs before the mesh loads
  // winding bakes the full lattice: a band is a key it does not read.
  {
    GraphError e = errorOf("winding(path=\"x\",bake=16,band=0.3)");
    REQUIRE(std::string(e.what()).find("unknown parameter 'band' for 'winding'") !=
            std::string::npos);
    REQUIRE(e.pointer() == childPointer);
  }
}

TEST_CASE("FileMeshResolver caches by path", "[field_graph]") {
  auto box = makeBox(1.0);
  // A bare name: the case runs in a working directory of its own
  // (test_main.cpp), so no other case or build tree can meet this file.
  const std::string path = "dualc_fg_box.obj";
  geometrycentral::surface::writeSurfaceMesh(*std::get<0>(box),
                                             *std::get<1>(box), path);

  FileMeshResolver r;
  MeshHandle h1 = r.resolve(path);
  MeshHandle h2 = r.resolve(path);
  REQUIRE(h1.mesh != nullptr);
  REQUIRE(h1.mesh == h2.mesh);   // same load reused, not re-read
  REQUIRE(h1.geom == h2.geom);

  std::filesystem::remove(path);
}

TEST_CASE("field-graph reports located errors", "[field_graph]") {
  FixedResolver meshes{nullptr, nullptr};

  // Malformed JSON.
  REQUIRE_THROWS_AS(parseJson("{ not valid json"), GraphError);
  // Unknown op.
  REQUIRE_THROWS_AS(
      FieldGraph::build(parseJson(R"({"op":"frobnicate"})"), meshes),
      GraphError);
  // Wrong boolean arity.
  REQUIRE_THROWS_AS(
      FieldGraph::build(parseJson(R"({"op":"difference","in":[{"op":"sphere"}]})"),
                        meshes),
      GraphError);
  // An unknown 'mesh' sign names all three accepted values and is located on
  // the mesh node, not its parent. (Until #56 'gwn' itself threw here.)
  try {
    FieldGraph::build(
        parseJson(R"({"op":"union","in":[{"op":"sphere"},
            {"op":"mesh","path":"x","sign":"bogus"}]})"),
        meshes);
    FAIL("expected a GraphError");
  } catch (const GraphError& e) {
    const std::string what = e.what();
    REQUIRE(what.find("'parity', 'pseudonormal' or 'gwn'") != std::string::npos);
    REQUIRE(e.pointer() == "/in/1");
  }

  // The error carries the offending node's JSON pointer.
  const std::string g =
      R"({"version":1,"root":{"op":"union","in":[
          {"op":"sphere"},{"op":"frobnicate"}]}})";
  try {
    FieldGraph::build(parseJson(g), meshes);
    FAIL("expected a GraphError");
  } catch (const GraphError& e) {
    REQUIRE(e.pointer() == "/root/in/1");
  }
}

TEST_CASE("field-graph rejects parameter keys the op does not read",
          "[field_graph]") {
  // Until 2026-09-11 an unknown key was absorbed silently: `plane` has no
  // grouped-key layout, so `plane(normal=[1,0,0],offset=0)` kept the registry
  // default {0,1,0,0} -- a Y-plane -- while every recipe that used it said X.
  // The CPU and GPU paths agreed on the wrong reading, so the parity gate could
  // not see it. Found by the 2026-09-11 docs screening (roadmap 17/10).
  FixedResolver meshes{nullptr, nullptr};

  // A primitive without a layout takes only positional values / params=[...].
  try {
    FieldGraph::build(parseShorthand("plane(normal=[1,0,0],offset=0)"), meshes);
    FAIL("expected a GraphError");
  } catch (const GraphError& e) {
    const std::string what = e.what();
    REQUIRE(what.find("unknown parameter 'normal'") != std::string::npos);
    REQUIRE(what.find("'plane'") != std::string::npos);
  }
  // The positional form is the correct spelling and builds an X-plane.
  {
    FieldGraph f = FieldGraph::build(parseShorthand("plane(1,0,0,0)"), meshes);
    REQUIRE(f.field().valueAt(Vector3{2.0, 0.0, 0.0}) ==
            Catch::Approx(2.0));
    REQUIRE(f.field().valueAt(Vector3{0.0, 2.0, 0.0}) ==
            Catch::Approx(0.0).margin(1e-12));
  }
  // A key that belongs to a different op is rejected with the accepted list
  // and the node's locator (the gyroid, not the enclosing union).
  try {
    FieldGraph::build(
        parseShorthand("union(sphere(radius=1),gyroid(wavelength=1,thickness=0.1))"),
        meshes);
    FAIL("expected a GraphError");
  } catch (const GraphError& e) {
    const std::string what = e.what();
    REQUIRE(what.find("unknown parameter 'thickness'") != std::string::npos);
    REQUIRE(what.find("accepted: center, wavelength") != std::string::npos);
    REQUIRE(e.pointer() != "");
  }
  // Every documented grouped key still passes.
  REQUIRE_NOTHROW(FieldGraph::build(
      parseShorthand("mix(sphere(center=[0,0,0],radius=1),"
                     "box(min=[-1,-1,-1],max=[1,1,1]),plane(1,0,0,0),"
                     "lo=-0.5,hi=0.5)"),
      meshes));
}

TEST_CASE("shorthand parses to the same field as the equivalent JSON",
          "[field_graph]") {
  // The gyroid_box graph in both forms (same named keys => structurally equal).
  const std::string jsonText = R"({
    "version": 1, "units": "mm",
    "root": { "op": "difference", "in": [
      { "op": "intersection", "in": [
        { "op": "box", "min": [-2,-2,-2], "max": [2,2,2] },
        { "op": "onion", "thickness": 0.3, "in": [
          { "op": "normalize", "in": [
            { "op": "gyroid", "wavelength": 1.0 } ] } ] } ] },
      { "op": "sphere", "center": [0,0,0], "radius": 1.0 } ] } })";
  const std::string expr =
      "difference(intersection(box(min=[-2,-2,-2],max=[2,2,2]),"
      "onion(normalize(gyroid(wavelength=1)),thickness=0.3)),"
      "sphere(center=[0,0,0],radius=1))";

  // dumpJson canonicalises both; structurally identical trees => equal dumps.
  REQUIRE(dumpJson(parseJson(jsonText)) == dumpJson(parseShorthand(expr)));

  // And the fields agree at sample points (belt and suspenders).
  FixedResolver meshes{nullptr, nullptr};
  FieldGraph fj = FieldGraph::build(parseJson(jsonText), meshes);
  FieldGraph fs = FieldGraph::build(parseShorthand(expr), meshes);
  for (const Vector3& p :
       {Vector3{0, 0, 0}, Vector3{1.3, 0.2, -0.7}, Vector3{10, 0, 0}})
    REQUIRE(fj.field().valueAt(p) == Catch::Approx(fs.field().valueAt(p)));
}

TEST_CASE("shorthand round-trips through dumpJson + parseJson",
          "[field_graph]") {
  const std::string expr =
      "intersection(box(min=[-2,-2,-2],max=[2,2,2]),"
      "onion(normalize(gyroid(wavelength=1)),thickness=0.3))";
  GraphNode viaShort = parseShorthand(expr);
  GraphNode viaJson = parseJson(dumpJson(viaShort));

  FixedResolver meshes{nullptr, nullptr};
  FieldGraph fg = FieldGraph::build(viaJson, meshes);
  dualc::SamplerParams sp;
  sp.maxDepth = 5;
  dualc::ContourerParams cp;
  auto [mesh, geom, normals] = dualc::dualContourField(fg.field(), sp, cp);
  (void)geom;
  (void)normals;
  REQUIRE(mesh->nVertices() > 0);
  REQUIRE(mesh->nFaces() > 0);
}

TEST_CASE("graded-onion ramps wall thickness with a control field",
          "[field_graph]") {
  // Distance-from-a-point control (sphere radius 0 => |p|): thickness ramps
  // from t1 where |p| <= d0 to t2 where |p| >= d1, clamped-linear between.
  const std::string expr =
      "graded-onion(normalize(gyroid(wavelength=1)),sphere(radius=0),"
      "t1=0.05,t2=0.2,d0=1.0,d1=3.0)";

  GraphNode g = parseShorthand(expr);
  REQUIRE(g.op == "graded-onion");
  REQUIRE(g.in.size() == 2);

  // Shorthand -> JSON -> JSON is idempotent (params + 2 children serialize).
  REQUIRE(dumpJson(g) == dumpJson(parseJson(dumpJson(g))));

  FixedResolver meshes{nullptr, nullptr};
  FieldGraph fg = FieldGraph::build(g, meshes);
  // Reference base to subtract: |normalize(gyroid)|.
  FieldGraph base =
      FieldGraph::build(parseShorthand("normalize(gyroid(wavelength=1))"),
                        meshes);

  auto expectedThickness = [](double r) {
    double u = (r - 1.0) / (3.0 - 1.0);
    u = u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u);
    return 0.05 + (0.2 - 0.05) * u;
  };
  // Probe the three regimes: below d0 (constant t1), mid-falloff, above d1
  // (constant t2). valueAt + t(|p|) must recover |base|.
  for (const Vector3& p :
       {Vector3{0.3, 0, 0}, Vector3{2, 0, 0}, Vector3{5, 0, 0}}) {
    const double r = p.norm();
    const double expected =
        std::abs(base.field().valueAt(p)) - expectedThickness(r);
    REQUIRE(fg.field().valueAt(p) == Catch::Approx(expected));
  }
}

TEST_CASE("shorthand maps underscore op aliases to hyphenated tokens",
          "[field_graph]") {
  REQUIRE(parseShorthand("schwarz_p()").op == "schwarz-p");
  REQUIRE(parseShorthand("fischer_koch()").op == "fischer-koch");

  GraphNode su = parseShorthand("smooth_union(sphere(),box(),k=0.3)");
  REQUIRE(su.op == "smooth-union");
  REQUIRE(su.in.size() == 2);
  auto k = su.params.find("k");
  REQUIRE(k != su.params.end());
  REQUIRE(k->second.kind == ParamValue::Kind::Number);
  REQUIRE(k->second.num == Catch::Approx(0.3));
}

TEST_CASE("shorthand positional primitive params build a primitive",
          "[field_graph]") {
  // sphere(0,0,0,0.5) -> params["params"]=[0,0,0,0.5] (flat primitive order).
  GraphNode n = parseShorthand("sphere(0,0,0,0.5)");
  REQUIRE(n.op == "sphere");
  auto p = n.params.find("params");
  REQUIRE(p != n.params.end());
  REQUIRE(p->second.kind == ParamValue::Kind::Vector);
  REQUIRE(p->second.vec.size() == 4);

  FixedResolver meshes{nullptr, nullptr};
  FieldGraph fg = FieldGraph::build(n, meshes);
  REQUIRE(fg.field().valueAt(Vector3{0, 0, 0}) < 0.0);  // inside r=0.5
  REQUIRE(fg.field().valueAt(Vector3{2, 0, 0}) > 0.0);   // outside
}

TEST_CASE("shorthand value identifiers become string params", "[field_graph]") {
  // twist(axis=x,...) -> axis is a String value; rotate(axis=[...]) -> Vector.
  GraphNode t = parseShorthand("twist(sphere(),radiansPerUnit=0.5,axis=z)");
  auto ax = t.params.find("axis");
  REQUIRE(ax != t.params.end());
  REQUIRE(ax->second.kind == ParamValue::Kind::String);
  REQUIRE(ax->second.str == "z");
}

// ---------------------------------------------------------------------------
// The 2D layer (#58): four profiles, legal only as the one child of a lift.
// ---------------------------------------------------------------------------

namespace {

// The field at a spread of points inside, on and outside a unit-ish shape.
std::vector<Vector3> liftSamples() {
  std::vector<Vector3> pts;
  for (double x : {-2.6, -1.1, -0.3, 0.0, 0.45, 1.0, 2.2})
    for (double y : {-1.4, -0.2, 0.0, 0.5, 1.3})
      for (double z : {-0.9, 0.0, 0.3, 1.7}) pts.push_back(Vector3{x, y, z});
  return pts;
}

void requireSameField(const dualc::ImplicitField& a,
                      const dualc::ImplicitField& b) {
  for (const Vector3& p : liftSamples()) REQUIRE(a.valueAt(p) == b.valueAt(p));
  const dualc::BBox ba = a.bounds(), bb = b.bounds();
  REQUIRE(ba.min.x == bb.min.x);
  REQUIRE(ba.min.y == bb.min.y);
  REQUIRE(ba.min.z == bb.min.z);
  REQUIRE(ba.max.x == bb.max.x);
  REQUIRE(ba.max.y == bb.max.y);
  REQUIRE(ba.max.z == bb.max.z);
}

}  // namespace

TEST_CASE("2D profiles under revolve and extrude build the library lifts",
          "[field_graph][lift]") {
  using dualc::Vector2;
  FixedResolver meshes{nullptr, nullptr};
  auto build = [&](const std::string& e) {
    return FieldGraph::build(parseShorthand(e), meshes).fieldPtr();
  };
  const auto circle = std::make_shared<dualc::Circle2D>(Vector2{0.2, -0.1}, 0.5);
  const auto rect =
      std::make_shared<dualc::Box2D>(Vector2{0.1, 0.3}, Vector2{0.8, 0.4});
  const auto seg = std::make_shared<dualc::Segment2D>(Vector2{-1, 0},
                                                      Vector2{1, 0.5}, 0.25);
  // Concave (an L), so the crossing-count sign is exercised too.
  const auto poly = std::make_shared<dualc::Polygon2D>(std::vector<Vector2>{
      {0, 0}, {1, 0}, {1, 0.4}, {0.4, 0.4}, {0.4, 1}, {0, 1}});

  SECTION("each profile, named keys, under both lifts") {
    const std::vector<std::pair<std::string, dualc::Field2DPtr>> cases = {
        {"circle(center=[0.2,-0.1],radius=0.5)", circle},
        {"rect(center=[0.1,0.3],half=[0.8,0.4])", rect},
        {"segment(a=[-1,0],b=[1,0.5],radius=0.25)", seg},
        {"polygon(points=[[0,0],[1,0],[1,0.4],[0.4,0.4],[0.4,1],[0,1]])",
         poly}};
    for (const auto& c : cases) {
      INFO(c.first);
      requireSameField(*build("revolve(" + c.first + ",offset=1.5)"),
                       dualc::RevolveField(c.second, 1.5));
      requireSameField(*build("extrude(" + c.first + ",height=0.3)"),
                       dualc::ExtrudeField(c.second, 0.3));
    }
  }
  SECTION("the flat params form and the shorthand positionals agree") {
    requireSameField(*build("extrude(circle(params=[0.2,-0.1,0.5]),height=0.3)"),
                     dualc::ExtrudeField(circle, 0.3));
    requireSameField(*build("extrude(rect(0.1,0.3,0.8,0.4),height=0.3)"),
                     dualc::ExtrudeField(rect, 0.3));
    requireSameField(*build("extrude(segment(-1,0,1,0.5,0.25),height=0.3)"),
                     dualc::ExtrudeField(seg, 0.3));
    requireSameField(
        *build("extrude(polygon(0,0,1,0,1,0.4,0.4,0.4,0.4,1,0,1),height=0.3)"),
        dualc::ExtrudeField(poly, 0.3));
    // A short flat list overrides from the front, as on a primitive.
    requireSameField(*build("extrude(circle(params=[0.2]))"),
                     dualc::ExtrudeField(std::make_shared<dualc::Circle2D>(
                                             Vector2{0.2, 0}, 0.5),
                                         0.5));
  }
  SECTION("defaults are dualc_lift's") {
    requireSameField(*build("revolve(circle)"),
                     dualc::RevolveField(std::make_shared<dualc::Circle2D>(
                                             Vector2{0, 0}, 0.5),
                                         1.0));
    requireSameField(*build("extrude(rect)"),
                     dualc::ExtrudeField(std::make_shared<dualc::Box2D>(
                                             Vector2{0, 0}, Vector2{1, 0.5}),
                                         0.5));
    requireSameField(*build("extrude(segment)"),
                     dualc::ExtrudeField(std::make_shared<dualc::Segment2D>(
                                             Vector2{-1, 0}, Vector2{1, 0}, 0.3),
                                         0.5));
  }
  SECTION("a lift composes like any 3D node, and contours") {
    FieldGraph fg = FieldGraph::build(
        parseShorthand("difference(revolve(circle(radius=0.5),offset=2),"
                       "sphere(center=[2,0,0],radius=0.4))"),
        meshes);
    dualc::SamplerParams sp;
    sp.maxDepth = 5;
    dualc::ContourerParams cp;
    auto [mesh, geom, normals] = dualc::dualContourField(fg.field(), sp, cp);
    (void)geom;
    (void)normals;
    REQUIRE(mesh->nFaces() > 0);
  }
}

TEST_CASE("a 2D profile outside a lift, or a 3D child under one, is located",
          "[field_graph][lift]") {
  FixedResolver meshes{nullptr, nullptr};
  auto errorOf = [&](const std::string& json) -> GraphError {
    try {
      FieldGraph::build(parseJson(json), meshes);
    } catch (const GraphError& e) {
      return e;
    }
    FAIL("expected a GraphError for " << json);
    return GraphError("", "");
  };

  // A profile as a boolean's child, as the root, and under a decorator.
  GraphError e = errorOf(
      R"({"op":"union","in":[{"op":"sphere"},{"op":"circle","radius":1}]})");
  REQUIRE(std::string(e.what()).find("'circle' is a 2D profile") !=
          std::string::npos);
  REQUIRE(e.pointer() == "/in/1");
  REQUIRE(errorOf(R"({"op":"rect"})").pointer() == "");
  e = errorOf(R"({"op":"union","in":[{"op":"extrude","in":[{"op":"segment"}]},
      {"op":"translate","by":[1,0,0],"in":[{"op":"rect"}]}]})");
  REQUIRE(e.pointer() == "/in/1/in/0");
  e = errorOf(R"({"op":"onion","thickness":0.1,"in":[{"op":"polygon",
      "points":[[0,0],[1,0],[0,1]]}]})");
  REQUIRE(e.pointer() == "/in/0");
  REQUIRE(std::string(e.what()).find("2D profile") != std::string::npos);

  // A 3D node (and an unknown op) under a lift, located on the child.
  e = errorOf(R"({"op":"revolve","in":[{"op":"sphere"}]})");
  REQUIRE(std::string(e.what()).find("'revolve' takes one 2D profile") !=
          std::string::npos);
  REQUIRE(std::string(e.what()).find("not 'sphere'") != std::string::npos);
  REQUIRE(e.pointer() == "/in/0");
  e = errorOf(R"({"op":"extrude","in":[{"op":"extrude","in":[{"op":"circle"}]}]})");
  REQUIRE(e.pointer() == "/in/0");
  REQUIRE(errorOf(R"({"op":"extrude","in":[{"op":"frobnicate"}]})").pointer() ==
          "/in/0");

  // Arity: exactly one child, and a profile takes none.
  REQUIRE(std::string(errorOf(R"({"op":"revolve"})").what())
              .find("expects 1 child") != std::string::npos);
  REQUIRE(errorOf(R"({"op":"revolve","in":[{"op":"circle"},{"op":"rect"}]})")
              .pointer() == "");
  REQUIRE(errorOf(R"({"op":"extrude","in":[{"op":"circle","in":[
      {"op":"rect"}]}]})").pointer() == "/in/0");
}

TEST_CASE("2D profiles and lifts reject bad values with a located error",
          "[field_graph][lift]") {
  FixedResolver meshes{nullptr, nullptr};
  struct Bad {
    const char* expr;
    const char* what;
  };
  const Bad bad[] = {
      {"extrude(circle(radius=0))", "'circle' radius must be > 0"},
      {"extrude(segment(radius=-1))", "'segment' radius must be > 0"},
      {"extrude(rect(half=[1,0]))", "'rect' half extents must be > 0"},
      {"extrude(circle(1,2,3,4))", "'circle' takes at most 3 values"},
      {"extrude(circle(center=[0,0,0]))", "'center' must be an array of 2"},
      {"extrude(circle(radius=1,major=2))", "unknown parameter 'major'"},
      {"extrude(polygon)", "'polygon' needs points="},
      {"extrude(polygon(0,0,1,0))", "'polygon' needs >= 3 [x,y] vertices"},
      {"extrude(polygon(0,0,1,0,1))", "'polygon' needs >= 3 [x,y] vertices"},
      {"extrude(polygon(points=[[0,0,0],[1,0,0],[0,1,0]]))",
       "'polygon' needs >= 3 [x,y] vertices"},
      {"extrude(polygon(points=[[0,0],[1,0],[0,1]],params=[0,0,1,0,0,1]))",
       "not both"},
      {"extrude(circle,height=0)", "'extrude' height"},
      {"extrude(circle,height=-1)", "'extrude' height"},
      {"extrude(circle,offset=1)", "unknown parameter 'offset'"},
      {"revolve(circle,height=1)", "unknown parameter 'height'"},
  };
  for (const Bad& b : bad) {
    INFO(b.expr);
    try {
      FieldGraph::build(parseShorthand(b.expr), meshes);
      FAIL("expected a GraphError");
    } catch (const GraphError& e) {
      REQUIRE(std::string(e.what()).find(b.what) != std::string::npos);
      REQUIRE(e.pointer() != "");
    }
  }
  // A ragged or doubly nested array is a parse error, in both readers.
  REQUIRE_THROWS_AS(parseShorthand("polygon(points=[[0,0],[1]])"), GraphError);
  REQUIRE_THROWS_AS(parseShorthand("polygon(points=[[[0,0]]])"), GraphError);
  REQUIRE_THROWS_AS(parseJson(R"({"op":"polygon","points":[[0,0],[1]]})"),
                    GraphError);
  REQUIRE_THROWS_AS(parseJson(R"({"op":"polygon","points":[[0,0],1]})"),
                    GraphError);
  REQUIRE_THROWS_AS(parseJson(R"({"op":"polygon","points":[[0,[0]]]})"),
                    GraphError);
}

TEST_CASE("2D profiles and lifts round-trip through dumpJson + parseJson",
          "[field_graph][lift]") {
  FixedResolver meshes{nullptr, nullptr};
  const std::string expr =
      "union(revolve(circle(center=[0.1,0],radius=0.5),offset=2),"
      "union(extrude(polygon(points=[[0,0],[1,0],[1,1],[0,1]]),height=0.5),"
      "union(extrude(rect(params=[0,0,1,0.5])),"
      "revolve(segment(a=[-0.5,-1],b=[0.5,1],radius=0.2),offset=1))))";
  const GraphNode viaShort = parseShorthand(expr);
  const std::string dumped = dumpJson(viaShort);
  // The polygon's rows come back as rows, not a flat list.
  REQUIRE(dumped.find("\"points\": [\n") != std::string::npos);
  const GraphNode viaJson = parseJson(dumped);
  REQUIRE(dumpJson(viaJson) == dumped);

  // The JSON form written by hand reads to the same tree.
  const std::string json = R"({"op":"extrude","height":0.5,"in":[
      {"op":"polygon","points":[[0,0],[1,0],[1,1],[0,1]]}]})";
  REQUIRE(dumpJson(parseJson(json)) ==
          dumpJson(parseShorthand(
              "extrude(polygon(points=[[0,0],[1,0],[1,1],[0,1]]),height=0.5)")));

  const FieldGraph a = FieldGraph::build(viaShort, meshes);
  const FieldGraph b = FieldGraph::build(viaJson, meshes);
  requireSameField(a.field(), b.field());
}

// --- porous (roadmap 23 #65) --------------------------------------------------

namespace {

std::string porousError(const std::string& shorthand) {
  FixedResolver meshes{nullptr, nullptr};
  try {
    FieldGraph::build(parseShorthand(shorthand), meshes);
  } catch (const GraphError& e) {
    REQUIRE_FALSE(e.pointer().empty());
    return e.what();
  }
  FAIL("expected a GraphError for " << shorthand);
  return {};
}

}  // namespace

TEST_CASE("porous: the node builds from JSON and shorthand and round-trips",
          "[field_graph][porous]") {
  FixedResolver meshes{nullptr, nullptr};
  const std::string sh =
      "porous(sphere(radius=1), spacing=0.3, cells=delaunay, mode=struts, "
      "smooth=0.05, seed=3, relax=2)";
  const std::string js = R"({"op":"porous","spacing":0.3,"cells":"delaunay",
      "mode":"struts","smooth":0.05,"seed":3,"relax":2,
      "in":[{"op":"sphere","radius":1}]})";
  const GraphNode a = parseShorthand(sh);
  const std::string dumped = dumpJson(a);
  const GraphNode b = parseJson(dumped);
  REQUIRE(dumpJson(b) == dumped);
  FieldGraph fa = FieldGraph::build(a, meshes);
  FieldGraph fb = FieldGraph::build(b, meshes);
  FieldGraph fj = FieldGraph::build(parseJson(js), meshes);
  const auto* pf = dynamic_cast<const dualc::PorousField*>(&fa.field());
  REQUIRE(pf != nullptr);
  REQUIRE(pf->params().cells == dualc::PorousCells::Delaunay);
  REQUIRE(pf->params().mode == dualc::PorousMode::Struts);
  REQUIRE(pf->params().seeds.relax == 2);
  REQUIRE(pf->params().seeds.seed == 3);
  REQUIRE(pf->params().smooth == 0.05);
  REQUIRE(fa.field().bounds().max.x == Catch::Approx(1.0));
  for (int i = 0; i < 64; ++i) {
    const Vector3 p{std::sin(1.3 * i), std::cos(0.7 * i), std::sin(0.31 * i + 1)};
    const double v = fa.field().valueAt(p);
    REQUIRE(v >= 0.0);
    REQUIRE(fb.field().valueAt(p) == v);
    REQUIRE(fj.field().valueAt(p) == v);
  }
}

TEST_CASE("porous: every key of the signature is read and defaulted",
          "[field_graph][porous]") {
  FixedResolver meshes{nullptr, nullptr};
  // All twelve keys, with the control child: no unread-key error.
  FieldGraph g = FieldGraph::build(
      parseShorthand("porous(sphere(radius=1), plane(1,0,0,0), cells=voronoi, "
                     "mode=walls, dist=jitter, spacing=0.25, spacing2=0.4, d0=-1, "
                     "d1=1, ramp=smooth, relax=1, seed=7, smooth=0.02, jitter=0.5)"),
      meshes);
  const auto& p = dynamic_cast<const dualc::PorousField&>(g.field()).params();
  REQUIRE(p.seeds.dist == dualc::SeedDistribution::Jitter);
  REQUIRE(p.seeds.ramp == dualc::DensityRamp::Smooth);
  REQUIRE(p.seeds.spacing2 == 0.4);
  REQUIRE(p.seeds.d0 == -1.0);
  REQUIRE(p.seeds.d1 == 1.0);
  REQUIRE(p.seeds.jitter == 0.5);
  REQUIRE(p.smooth == 0.02);

  // The defaults of the raw plan's signature.
  FieldGraph d = FieldGraph::build(parseShorthand("porous(sphere(radius=1), spacing=0.3)"),
                                   meshes);
  const auto& q = dynamic_cast<const dualc::PorousField&>(d.field()).params();
  REQUIRE(q.cells == dualc::PorousCells::Voronoi);
  REQUIRE(q.mode == dualc::PorousMode::Walls);
  REQUIRE(q.seeds.dist == dualc::SeedDistribution::Poisson);
  REQUIRE(q.seeds.spacing2 == 0.3);
  REQUIRE(q.seeds.d0 == 0.0);
  REQUIRE(q.seeds.ramp == dualc::DensityRamp::Linear);
  REQUIRE(q.seeds.relax == 0);
  REQUIRE(q.seeds.seed == 0);
  REQUIRE(q.seeds.jitter == 1.0);
  REQUIRE(q.smooth == 0.0);
}

TEST_CASE("porous: unknown keys a missing spacing and bad values are located errors",
          "[field_graph][porous]") {
  const std::string v = "sphere(radius=1)";
  std::string e = porousError("porous(" + v + ", spacing=0.3, spacng=0.2)");
  REQUIRE(e.find("unknown parameter 'spacng' for 'porous'") != std::string::npos);
  REQUIRE(e.find("spacing2") != std::string::npos);  // the accepted list
  REQUIRE(porousError("porous(" + v + ")").find("missing required parameter 'spacing'") !=
          std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0)").find("spacing must be > 0") !=
          std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, cells=foam)")
              .find("cells must be voronoi or delaunay (got 'foam')") != std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, mode=wall)").find("walls or struts") !=
          std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, dist=blue)")
              .find("poisson, jitter or random") != std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, relax=2.5)").find("relax must be") !=
          std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, relax=51)").find("relax must be") !=
          std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, seed=-1)").find("seed must be") !=
          std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, spacing2=0.5)")
              .find("give a control field") != std::string::npos);
  REQUIRE(porousError("porous(" + v + ", plane(1,0,0,0), spacing=0.3, spacing2=0.5)")
              .find("missing required parameter 'd1'") != std::string::npos);
  REQUIRE(porousError("porous(spacing=0.3)").find("expects 1 or 2 child") != std::string::npos);
  REQUIRE(porousError("porous(" + v + "," + v + "," + v + ", spacing=0.3)")
              .find("expects 1 or 2 child") != std::string::npos);
  // The library's own checks come back located.
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, smooth=-1)").find("smooth must be") !=
          std::string::npos);
  REQUIRE(porousError("porous(" + v + ", spacing=0.3, jitter=-1)").find("jitter must be") !=
          std::string::npos);
}

TEST_CASE("porous: a mesh volume through the graph", "[field_graph][porous]") {
  auto box = makeBox(1.0);
  FixedResolver meshes{std::get<0>(box).get(), std::get<1>(box).get()};
  FieldGraph g = FieldGraph::build(
      parseShorthand("intersection(mesh(path=\"x\"), "
                     "onion(porous(mesh(path=\"x\"), spacing=0.4), thickness=0.02))"),
      meshes);
  REQUIRE(meshes.calls == 2);
  const dualc::BBox b = g.field().bounds();
  REQUIRE(b.min.x == Catch::Approx(-1.0));
  REQUIRE(b.max.z == Catch::Approx(1.0));
  // Material (< 0) and pores (> 0) both occur inside the cube.
  int solid = 0, pore = 0;
  for (int i = 0; i < 400; ++i) {
    const Vector3 p{0.9 * std::sin(1.3 * i), 0.9 * std::cos(0.7 * i),
                    0.9 * std::sin(0.31 * i + 1)};
    (g.field().valueAt(p) < 0.0 ? solid : pore)++;
  }
  REQUIRE(solid > 0);
  REQUIRE(pore > 0);
}

TEST_CASE("porous: childBakeMargin hands the volume the pad and the control nothing",
          "[field_graph][porous][bake]") {
  const std::string a = "sphere(radius=1)";
  REQUIRE(childBakeMargin(parseShorthand("porous(" + a + ", spacing=0.3)"), 0, 0.7) == 0.3);
  const GraphNode g =
      parseShorthand("porous(" + a + ", plane(1,0,0,0), spacing=0.3, spacing2=0.5, d1=1)");
  REQUIRE(childBakeMargin(g, 0, 0.0) == 0.5);
  REQUIRE(childBakeMargin(g, 1, 0.7) == 0.0);
}
