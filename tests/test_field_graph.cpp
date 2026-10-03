#include "field_graph.h"

#include "dualc/pipeline.h"

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("FileMeshResolver caches by path", "[field_graph]") {
  auto box = makeBox(1.0);
  const std::string path =
      (std::filesystem::temp_directory_path() / "dualc_fg_box.obj").string();
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
  // gwn is not a 'mesh' sign mode (the doc-12 fallback: use a 'winding' node).
  REQUIRE_THROWS_AS(
      FieldGraph::build(
          parseJson(R"({"op":"mesh","path":"x","sign":"gwn"})"), meshes),
      GraphError);

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
            Catch::Approx(0.0));
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
