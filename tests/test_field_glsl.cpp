#include "field_glsl.h"
#include "field_graph.h"

#include "example_common.h"

#include "dualc/implicit.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

// Tests for the field->GLSL codegen's DAG-dedup emission (examples/field_glsl.cpp;
// roadmap docs/roadmap/12 Track 2 #1). compileToGlsl is GL-free -- it generates
// source + bakes mesh sources on the CPU -- so these run as a normal CTest with
// no GL context. They assert the structural contract of dedup: a duplicated
// subtree emits ONE fN / one uniform set / (for meshes) one bake+sampler3D, while
// structurally-distinct nodes stay separate (no over-collapse).

using namespace dce::fieldgraph;
using dualc::Vector3;

namespace {

// Axis-aligned cube of side 2*half centred at the origin (mirrors the helper in
// test_field_graph.cpp), enough geometry for bakeToGrid to run.
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

// Hands back one fixed mesh regardless of path, counting resolve() calls -- lets
// the mesh-dedup path be exercised without file I/O.
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

dualc::BBox box(double a, double b) {
  return dualc::BBox{{a, a, a}, {b, b, b}};
}

// Count non-overlapping occurrences of `needle` in `hay`.
std::size_t countOccurrences(const std::string& hay, const std::string& needle) {
  std::size_t n = 0, pos = 0;
  while ((pos = hay.find(needle, pos)) != std::string::npos) {
    ++n;
    pos += needle.size();
  }
  return n;
}

std::size_t bindingsWithKey(const GlslScene& s, const std::string& key) {
  std::size_t n = 0;
  for (const UniformBinding& b : s.bindings)
    if (b.paramKey == key) ++n;
  return n;
}

}  // namespace

TEST_CASE("GLSL codegen dedups an identical analytic subtree", "[field_glsl]") {
  FileMeshResolver resolver;  // no mesh nodes touched here

  // Two structurally-identical gyroids collapse to one emitted node.
  GraphNode dup =
      parseShorthand("union(gyroid(wavelength=2),gyroid(wavelength=2))");
  GlslScene sd = compileToGlsl(dup, resolver, box(-1, 1));

  // The same shape with DISTINCT children must NOT collapse -- the baseline.
  GraphNode distinct =
      parseShorthand("union(gyroid(wavelength=1),gyroid(wavelength=2))");
  GlslScene sx = compileToGlsl(distinct, resolver, box(-1, 1));

  // Deduped: gyroid + union = 2 emitted nodes; distinct: gyroid + gyroid +
  // union = 3.
  CHECK(sd.nodeCount == 2);
  CHECK(sx.nodeCount == 3);

  // One gyroid helper call vs. two in the generated source.
  CHECK(countOccurrences(sd.generatedSource, "sdGyroid(") == 1);
  CHECK(countOccurrences(sx.generatedSource, "sdGyroid(") == 2);

  // One shared wavelength uniform vs. two -- so a slider edits the single value
  // that both references see (uniform coherence).
  CHECK(bindingsWithKey(sd, "wavelength") == 1);
  CHECK(bindingsWithKey(sx, "wavelength") == 2);
}

TEST_CASE("GLSL codegen bakes a duplicated mesh source once", "[field_glsl]") {
  auto boxMesh = makeBox(1.0);
  FixedResolver resolver(std::get<0>(boxMesh).get(),
                         std::get<1>(boxMesh).get());

  // The same mesh referenced twice: clipped once, onion-shelled once.
  GraphNode g = parseShorthand(
      "union(mesh(path=\"box\"),onion(mesh(path=\"box\"),thickness=0.1))");
  GlslScene s = compileToGlsl(g, resolver, box(-2, 2));

  // One bake -> one sampler3D, and the resolver was hit once.
  CHECK(s.meshes.size() == 1);
  CHECK(resolver.calls == 1);
}

TEST_CASE("GLSL codegen bakes a winding node via the generic field bake",
          "[field_glsl]") {
  auto boxMesh = makeBox(1.0);
  auto* mesh = std::get<0>(boxMesh).get();
  auto* geom = std::get<1>(boxMesh).get();
  FixedResolver resolver(mesh, geom);

  const dualc::BBox region = box(-2, 2);
  GraphNode g = parseShorthand("winding(path=\"box\")");
  GlslScene s = compileToGlsl(g, resolver, region);

  // One baked sampler3D, and its values are bit-identical to a fresh generic
  // bakeToGrid of the same windingNumberField over the same region/resolution --
  // gates that emitWinding wired the right field, region, resolution and sign
  // (GL-free; the texture is just the GridField's values).
  REQUIRE(s.meshes.size() == 1);
  const dualc::Vector3i res = s.meshes[0].resolution;
  dualc::FieldPtr wf = dualc::windingNumberField(*mesh, *geom);
  dualc::FieldPtr gp = dualc::bakeToGrid(*wf, region, res);
  auto* grid = dynamic_cast<const dualc::GridField*>(gp.get());
  REQUIRE(grid != nullptr);
  CHECK(s.meshes[0].values == grid->values());
}

TEST_CASE("GLSL codegen honours the requested bake resolution", "[field_glsl]") {
  auto boxMesh = makeBox(1.0);
  auto* mesh = std::get<0>(boxMesh).get();
  auto* geom = std::get<1>(boxMesh).get();
  FixedResolver resolver(mesh, geom);
  GraphNode g = parseShorthand("mesh(path=\"box\")");

  // The gridRes argument must reach MeshSource::bakeToGrid. It was parsed by
  // dualc_field_view and then dropped on the floor for two months (the bake was
  // pinned at 96^3), which was invisible because every render looked the same.
  GlslScene lo = compileToGlsl(g, resolver, box(-2, 2), 48);
  GlslScene hi = compileToGlsl(g, resolver, box(-2, 2), 129);
  REQUIRE(lo.meshes.size() == 1);
  REQUIRE(hi.meshes.size() == 1);
  CHECK(lo.meshes[0].resolution.x == 48);
  CHECK(hi.meshes[0].resolution.x == 129);
  CHECK(lo.meshes[0].values.size() == 48u * 48u * 48u);
  CHECK(hi.meshes[0].values.size() == 129u * 129u * 129u);

  // Omitting it keeps the documented default, and out-of-range is clamped
  // rather than reaching GridField's resolution >= 2 throw.
  GlslScene def = compileToGlsl(g, resolver, box(-2, 2));
  CHECK(def.meshes[0].resolution.x == dce::fieldgraph::kDefaultMeshGridRes);
  GlslScene clamped = compileToGlsl(g, resolver, box(-2, 2), 1);
  CHECK(clamped.meshes[0].resolution.x == 2);
}

TEST_CASE("GLSL codegen emits every registered primitive", "[field_glsl]") {
  FileMeshResolver resolver;
  // Every primitive token in the registry must compile to a single-node scene
  // (a partial node = registry defaults) -- catches a missing emitPrimitive
  // branch or a uniform-slice out of range, across all ~29 in one test.
  for (const dce::PrimEntry& e : dce::primitiveCatalogue()) {
    GraphNode g;
    g.op = e.name;
    g.pointer = "/root";
    INFO("primitive: " << e.name);
    GlslScene s = compileToGlsl(g, resolver, box(-2, 2));
    CHECK(s.nodeCount == 1);
    CHECK(countOccurrences(s.generatedSource, "float sceneSDF") == 1);
  }
}

TEST_CASE("GLSL codegen emits every strut crystal", "[field_glsl]") {
  FileMeshResolver resolver;
  // Each crystal compiles to a single source node whose body unions exactly one
  // sdCapsule per unit-cell segment (sc 3, bcc 8, fcc 24, octet 36) and folds
  // with dcRoundTA -- the GPU counterpart of makeStrutLattice / strutCellSegments.
  struct Case { std::string kind; std::size_t caps; };
  const std::vector<Case> cases = {
      {"sc", 3}, {"bcc", 8}, {"fcc", 24}, {"octet", 36}};
  for (const Case& cs : cases) {
    GraphNode g = parseShorthand(cs.kind + "(wavelength=0.5,radius=0.05)");
    GlslScene s = compileToGlsl(g, resolver, box(-2, 2));
    INFO("crystal: " << cs.kind);
    CHECK(s.nodeCount == 1);
    CHECK(countOccurrences(s.generatedSource, "float sceneSDF") == 1);
    CHECK(countOccurrences(s.generatedSource, "sdCapsule(") == cs.caps);
    CHECK(countOccurrences(s.generatedSource, "dcRoundTA") == 3);  // 3 axes
  }
}

TEST_CASE("GLSL codegen tapers struts into round cones when nodeRadius is set",
          "[field_glsl]") {
  FileMeshResolver resolver;
  // With nodeRadius != radius each unit-cell segment splits at its midpoint into
  // TWO sdRoundCone calls (fat node, thin span); no sdCapsule is emitted.
  struct Case { std::string kind; std::size_t caps; };
  const std::vector<Case> cases = {
      {"sc", 3}, {"bcc", 8}, {"fcc", 24}, {"octet", 36}};
  for (const Case& cs : cases) {
    GraphNode g =
        parseShorthand(cs.kind + "(wavelength=0.5,radius=0.03,nodeRadius=0.1)");
    GlslScene s = compileToGlsl(g, resolver, box(-2, 2));
    INFO("crystal: " << cs.kind);
    CHECK(countOccurrences(s.generatedSource, "sdRoundCone(") == 2 * cs.caps);
    CHECK(countOccurrences(s.generatedSource, "sdCapsule(") == 0);
  }

  // nodeRadius == radius (or omitted) keeps the untapered capsule path exactly.
  GlslScene plain = compileToGlsl(
      parseShorthand("bcc(wavelength=0.5,radius=0.03,nodeRadius=0.03)"), resolver,
      box(-2, 2));
  CHECK(countOccurrences(plain.generatedSource, "sdCapsule(") == 8);
  CHECK(countOccurrences(plain.generatedSource, "sdRoundCone(") == 0);
}

TEST_CASE("GLSL codegen emits the domain operators", "[field_glsl]") {
  FileMeshResolver resolver;
  struct Case {
    std::string expr;
    std::string helperOrToken;  // a marker that must appear in the generated src
  };
  // Each compiles without throwing and references the expected prelude helper /
  // warp shape (catches a missing dispatch branch or wrong helper).
  const std::vector<Case> cases = {
      {"round(sphere(radius=1),r=0.2)", ") - u_n"},          // offset form
      {"mirror(sphere(center=[0.5,0,0],radius=0.6),normal=[1,0,0])", "normalize("},
      {"repeat(sphere(radius=0.3),period=[1,1,1])", "dcRoundTA"},
      {"repeat-limited(sphere(radius=0.3),period=[1,1,1],count=[2,2,2])", "dcRoundTA"},
      {"displace(sphere(radius=1),fn=sine,amplitude=0.2,frequency=4)", "dcDispSine"},
      {"displace(sphere(radius=1),fn=gyroid)", "dcDispGyroid"},
      {"displace(sphere(radius=1),fn=bumps)", "dcDispBumps"},
  };
  for (const Case& cs : cases) {
    GraphNode g = parseShorthand(cs.expr);
    GlslScene s = compileToGlsl(g, resolver, box(-2, 2));
    CHECK(countOccurrences(s.generatedSource, cs.helperOrToken) >= 1);
  }
}

TEST_CASE("GLSL codegen distinguishes graded-offset from graded-onion",
          "[field_glsl]") {
  FileMeshResolver resolver;
  const char* params = "sphere(radius=0),t1=0.05,t2=0.25,d0=0.5,d1=1.5)";

  // graded-onion hollows: |base| - t  (abs present).
  GraphNode onion = parseShorthand(std::string("graded-onion(sphere(radius=1),") +
                                   params);
  GlslScene so = compileToGlsl(onion, resolver, box(-2, 2));
  CHECK(countOccurrences(so.generatedSource, "abs(f") >= 1);

  // graded-offset inflates: base - t  (no abs on the base term). Both share the
  // control-ramp (mix / clamp), so the only structural difference is the abs.
  GraphNode offset =
      parseShorthand(std::string("graded-offset(sphere(radius=1),") + params);
  GlslScene sf = compileToGlsl(offset, resolver, box(-2, 2));
  CHECK(countOccurrences(sf.generatedSource, "clamp(") >= 1);  // ramp present
  CHECK(countOccurrences(sf.generatedSource, "mix(") >= 1);
  CHECK(countOccurrences(sf.generatedSource, "abs(f") == 0);   // no hollowing
}

TEST_CASE("GLSL codegen emits a mix morph over both child subtrees",
          "[field_glsl]") {
  FileMeshResolver resolver;
  // mix(A, B, control): A=sphere, B=box (distinct call markers), control=plane.
  GraphNode g = parseShorthand(
      "mix(sphere(radius=1),box(min=[-1,-1,-1],max=[1,1,1]),"
      "plane(1,0,0,0),lo=-0.5,hi=0.5)");
  GlslScene s = compileToGlsl(g, resolver, box(-2, 2));

  // The morph lerps two CHILD FIELD calls -> "mix(f<i>(p), f<j>(p), u)". This is
  // the marker that separates it from graded-onion/offset, which also emit mix(
  // + clamp( but on scalar uniforms ("mix(u_n.."), never on a child-func call.
  CHECK(countOccurrences(s.generatedSource, "mix(f") >= 1);
  CHECK(countOccurrences(s.generatedSource, "clamp(") >= 1);  // control ramp
  // Both operand subtrees AND the control are emitted (calls are "sd*(p, ..";
  // the always-present library definitions are "sd*(vec3 p, ..").
  CHECK(countOccurrences(s.generatedSource, "sdSphere(p,") >= 1);  // A
  CHECK(countOccurrences(s.generatedSource, "sdBox(p,") >= 1);     // B
  CHECK(countOccurrences(s.generatedSource, "sdPlane(p,") >= 1);   // control
}

TEST_CASE("GLSL codegen rejects parameter keys the op does not read",
          "[field_glsl]") {
  // Same rule as FieldGraph::build, on the preview path: an unknown key used to
  // be absorbed here too, so CPU and GPU were wrong identically and parity
  // stayed green (roadmap 17/10). The memo (DAG dedup) must not skip the check.
  FileMeshResolver resolver;
  REQUIRE_THROWS_AS(
      compileToGlsl(parseShorthand("plane(normal=[1,0,0],offset=0)"), resolver,
                    box(-2, 2)),
      GraphError);
  REQUIRE_THROWS_AS(
      compileToGlsl(parseShorthand("union(gyroid(wavelength=1,thickness=0.1),"
                                   "gyroid(wavelength=1,thickness=0.1))"),
                    resolver, box(-2, 2)),
      GraphError);
  REQUIRE_NOTHROW(compileToGlsl(parseShorthand("plane(1,0,0,0)"), resolver,
                                box(-2, 2)));
}

TEST_CASE("GLSL codegen does not over-collapse distinct nodes", "[field_glsl]") {
  SECTION("different params keep two analytic functions + two uniform sets") {
    FileMeshResolver resolver;
    GraphNode g =
        parseShorthand("union(gyroid(wavelength=1),gyroid(wavelength=2))");
    GlslScene s = compileToGlsl(g, resolver, box(-1, 1));
    CHECK(countOccurrences(s.generatedSource, "sdGyroid(") == 2);
    CHECK(bindingsWithKey(s, "wavelength") == 2);
  }

  SECTION("same mesh path, different sign mode bakes two textures") {
    auto boxMesh = makeBox(1.0);
    FixedResolver resolver(std::get<0>(boxMesh).get(),
                           std::get<1>(boxMesh).get());
    GraphNode g = parseShorthand(
        "union(mesh(path=\"box\",sign=parity),"
        "mesh(path=\"box\",sign=pseudonormal))");
    GlslScene s = compileToGlsl(g, resolver, box(-2, 2));
    CHECK(s.meshes.size() == 2);
    CHECK(resolver.calls == 2);
  }
}
