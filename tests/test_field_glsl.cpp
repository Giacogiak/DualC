#include "field_glsl.h"
#include "field_graph.h"

#include "example_common.h"

#include "dualc/implicit.h"
#include "dualc/porous.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
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

TEST_CASE("GLSL codegen uploads a baked node's own grid", "[field_glsl][bake]") {
  auto boxMesh = makeBox(1.0);
  FixedResolver resolver(std::get<0>(boxMesh).get(),
                         std::get<1>(boxMesh).get());

  // #57: a node with bake=N is the export's GridField, uploaded as is -- the
  // texture is the CPU grid bit for bit (preview == export), and neither the
  // compile bounds nor gridRes (--grid-res) re-bake it.
  for (const char* expr :
       {"mesh(path=\"box\",bake=24)", "mesh(path=\"box\",bake=24,band=0.5)",
        "winding(path=\"box\",bake=20)"}) {
    INFO(expr);
    GraphNode g = parseShorthand(expr);
    FieldGraph fg = FieldGraph::build(g, resolver);
    auto* grid = dynamic_cast<const dualc::GridField*>(&fg.field());
    REQUIRE(grid != nullptr);
    for (int gridRes : {48, 129}) {
      GlslScene s = compileToGlsl(g, resolver, box(-3, 3), gridRes);
      REQUIRE(s.meshes.size() == 1);
      CHECK(s.meshes[0].resolution.x == grid->resolution().x);
      CHECK(s.meshes[0].region.min.x == grid->bounds().min.x);
      CHECK(s.meshes[0].region.max.z == grid->bounds().max.z);
      CHECK(s.meshes[0].values == grid->values());
    }
  }

  // Under ancestors the texture is the grid baked with the margin they hand
  // down (childBakeMargin), exactly as FieldGraph::build bakes it.
  dualc::MeshSource src(*std::get<0>(boxMesh), *std::get<1>(boxMesh), true,
                        dualc::SignMethod::WINDING_NUMBER);
  NodeBake nb;
  nb.res = 24;
  const struct { const char* expr; double margin; } nested[] = {
      {"union(mesh(path=\"box\",bake=24),sphere(radius=2))", 0.0},
      {"smooth-union(translate(mesh(path=\"box\",bake=24),by=[0.3,0,0]),"
       "sphere(radius=2),k=0.7)", 0.7},
      {"smooth-union(scale(mesh(path=\"box\",bake=24),s=2),sphere(radius=2),k=1)",
       0.5},
      {"smooth-union(smooth-union(mesh(path=\"box\",bake=24),sphere(radius=0.3),"
       "k=0.1),sphere(radius=2),k=1)", 1.0},
      {"offset(smooth-union(mesh(path=\"box\",bake=24),sphere(radius=2),k=0.5),"
       "r=0.3)", 0.5},
      {"onion(offset(mesh(path=\"box\",bake=24),r=0.3),thickness=0.1)", 0.4},
  };
  for (const auto& t : nested) {
    INFO(t.expr);
    GlslScene s = compileToGlsl(parseShorthand(t.expr), resolver, box(-3, 3));
    REQUIRE(s.meshes.size() == 1);
    dualc::FieldPtr want = bakeMeshNode(src, nb, t.margin);
    auto* g = dynamic_cast<const dualc::GridField*>(want.get());
    REQUIRE(g != nullptr);
    CHECK(s.meshes[0].region.min.x == g->bounds().min.x);
    CHECK(s.meshes[0].values == g->values());
  }

  // One baked mesh under two margins is two textures; an unbaked mesh under
  // two margins stays one (the margin only keys a subtree that bakes).
  GlslScene twoMargins = compileToGlsl(
      parseShorthand("union(mesh(path=\"box\",bake=24),"
                     "onion(mesh(path=\"box\",bake=24),thickness=0.2))"),
      resolver, box(-2, 2));
  CHECK(twoMargins.meshes.size() == 2);
  GlslScene oneUnbaked = compileToGlsl(
      parseShorthand("union(mesh(path=\"box\"),"
                     "onion(mesh(path=\"box\"),thickness=0.2))"),
      resolver, box(-2, 2));
  CHECK(oneUnbaked.meshes.size() == 1);

  // Baked and unbaked forms of one mesh are two textures, not one memo hit.
  GlslScene two = compileToGlsl(
      parseShorthand("union(mesh(path=\"box\"),mesh(path=\"box\",bake=24))"),
      resolver, box(-2, 2));
  CHECK(two.meshes.size() == 2);
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

  SECTION("same mesh path, parity vs gwn bakes two textures") {
    auto boxMesh = makeBox(1.0);
    FixedResolver resolver(std::get<0>(boxMesh).get(),
                           std::get<1>(boxMesh).get());
    GraphNode g = parseShorthand(
        "union(mesh(path=\"box\",sign=parity),mesh(path=\"box\",sign=gwn))");
    GlslScene s = compileToGlsl(g, resolver, box(-2, 2));
    CHECK(s.meshes.size() == 2);
    CHECK(resolver.calls == 2);
  }
}

// ---------------------------------------------------------------------------
// The 2D layer (#58): profiles under revolve / extrude. Values are read by
// field_graph's readProfile, so the GLSL path takes the builder's defaults,
// checks and typing errors; these assert the emission shape and the binding
// table (the scalars are float uniforms, so tab + arrows nudge them).
// ---------------------------------------------------------------------------

namespace {

const UniformBinding* findBinding(const GlslScene& s, const std::string& key,
                                  const std::string& type) {
  for (const UniformBinding& b : s.bindings)
    if (b.paramKey == key && b.glslType == type) return &b;
  return nullptr;
}

// The (pointer, message) a graph fails with on the GPU path, and on the CPU.
std::string glslError(const std::string& expr) {
  FileMeshResolver resolver;
  try {
    compileToGlsl(parseShorthand(expr), resolver, box(-2, 2));
  } catch (const GraphError& e) {
    return e.pointer() + ": " + e.what();
  }
  return "<no error>";
}

std::string buildError(const std::string& expr) {
  FileMeshResolver resolver;
  try {
    FieldGraph::build(parseShorthand(expr), resolver);
  } catch (const GraphError& e) {
    return e.pointer() + ": " + e.what();
  }
  return "<no error>";
}

}  // namespace

TEST_CASE("GLSL codegen emits the 2D profiles under both lifts",
          "[field_glsl][lift]") {
  FileMeshResolver resolver;
  struct Case {
    std::string expr;
    std::string marker;
  };
  const std::vector<Case> cases = {
      {"revolve(circle(radius=0.4),offset=1.2)", "sd2Circle(p,"},
      {"extrude(circle,height=0.6)", "sd2Circle(p,"},
      {"extrude(rect(half=[0.8,0.5]),height=0.4)", "sd2Rect(p,"},
      {"extrude(segment(radius=0.25))", "sd2Segment(p,"},
      {"extrude(polygon(0,0,1,0,1,1,0,1))", "vec2[4]("},
      {"revolve(polygon(points=[[0,0],[1,0],[0.5,1]]))", "vec2[3]("},
  };
  for (const Case& cs : cases) {
    GlslScene s = compileToGlsl(parseShorthand(cs.expr), resolver, box(-2, 2));
    CHECK(countOccurrences(s.generatedSource, cs.marker) == 1);
    const bool rev = cs.expr.rfind("revolve", 0) == 0;
    CHECK(countOccurrences(s.generatedSource,
                           rev ? "(dcRevolveQ(p," : "dcExtrude(f") == 1);
    CHECK(s.nodeCount == 2);  // the profile + the lift
    CHECK(s.stepScale == 1.0f);  // exact SDFs: the metric fast path
  }

  // The new scalars are float uniforms (nudgeable), the 2D points vec2.
  GlslScene r = compileToGlsl(
      parseShorthand("revolve(circle(center=[0.1,0.2],radius=0.4),offset=1.2)"),
      resolver, box(-2, 2));
  const UniformBinding* radius = findBinding(r, "radius", "float");
  const UniformBinding* offset = findBinding(r, "offset", "float");
  const UniformBinding* center = findBinding(r, "center", "vec2");
  REQUIRE(radius);
  REQUIRE(offset);
  REQUIRE(center);
  CHECK(radius->value == std::vector<float>{0.4f});
  CHECK(radius->nodePointer == "8");  // the circle (a shorthand char offset)
  CHECK(offset->nodePointer == "0");  // the lift
  CHECK(offset->value == std::vector<float>{1.2f});
  CHECK(center->value == std::vector<float>{0.1f, 0.2f});
  GlslScene e = compileToGlsl(parseShorthand("extrude(segment,height=0.7)"),
                              resolver, box(-2, 2));
  REQUIRE(findBinding(e, "height", "float"));
  CHECK(findBinding(e, "height", "float")->value == std::vector<float>{0.7f});
  CHECK(findBinding(e, "radius", "float"));  // the segment's
  CHECK(bindingsWithKey(e, "a") == 1);
  CHECK(bindingsWithKey(e, "b") == 1);

  // One profile under two lifts emits once (its own memo key space).
  GlslScene u = compileToGlsl(
      parseShorthand("union(revolve(circle,offset=2),extrude(circle))"),
      resolver, box(-3, 3));
  CHECK(countOccurrences(u.generatedSource, "sd2Circle(p,") == 1);
  CHECK(u.nodeCount == 4);
}

TEST_CASE("GLSL codegen keeps the builder's 2D typing and value errors",
          "[field_glsl][lift]") {
  // The same message, located on the same node, as FieldGraph::build.
  for (const char* expr :
       {"circle", "union(sphere,rect)", "revolve(sphere)", "extrude(revolve(circle))",
        "extrude(circle(radius=0))", "extrude(circle(radius=0.5,typo=1))",
        "extrude(rect(params=[0,0,1,1,1]))", "extrude(polygon(0,0,1,0))",
        "extrude(circle,height=0)", "revolve(circle,offset=1,height=1)",
        "revolve(circle,circle)"}) {
    INFO(expr);
    const std::string gpu = glslError(expr);
    CHECK(gpu != "<no error>");
    CHECK(gpu == buildError(expr));
  }
}

TEST_CASE("GLSL codegen caps the polygon at its vertex ceiling",
          "[field_glsl][lift]") {
  auto polygonOf = [](int n) {
    std::string pts;
    for (int i = 0; i < n; ++i) {
      const double a = 2.0 * 3.14159265358979 * i / n;
      pts += (i ? "," : "") + std::string("[") + std::to_string(std::cos(a)) +
             "," + std::to_string(std::sin(a)) + "]";
    }
    return "extrude(polygon(points=[" + pts + "]))";
  };
  FileMeshResolver resolver;
  GlslScene s = compileToGlsl(parseShorthand(polygonOf(kMaxGlslPolygonVertices)),
                              resolver, box(-2, 2));
  CHECK(countOccurrences(s.generatedSource,
                         "vec2[" + std::to_string(kMaxGlslPolygonVertices) +
                             "](") == 1);
  // One more is refused, located on the polygon; the export path builds it.
  const std::string over = polygonOf(kMaxGlslPolygonVertices + 1);
  const std::string err = glslError(over);
  CHECK(err.find("'polygon' has 65 vertices; the GLSL preview takes at most 64") !=
        std::string::npos);
  // Located on the polygon (shorthand pointers are character offsets).
  CHECK(err.rfind(std::to_string(over.find("polygon")) + ":", 0) == 0);
  CHECK_NOTHROW(FieldGraph::build(parseShorthand(over), resolver));
}

// ---- porous (roadmap 23 #66) ------------------------------------------------
// The node compiles to data textures holding the builder's own packed arrays
// (PorousField::packed), a `smooth` uniform and one dcPorous* call; the GPU
// value is gated by dualc_glsl_parity. Named "porous: field_glsl ..." so both
// the `-R porous` and the `-R field_glsl` filters run them.

namespace {

const dualc::PorousField& porousOf(const FieldGraph& fg) {
  const auto* f = dynamic_cast<const dualc::PorousField*>(&fg.field());
  REQUIRE(f != nullptr);
  return *f;
}

// The texture holds the array, then zeros to the end of its last row.
template <class T>
void checkTexture(const DataTexture& t, const dualc::PorousField::Array<T>& a,
                  const std::vector<T>& data) {
  const std::size_t ch = std::size_t(dataTextureChannels(t.format));
  REQUIRE(a.size % ch == 0);
  const std::size_t texels = a.size / ch;
  CHECK(t.width == kDataTextureWidth);
  CHECK(std::size_t(t.height) ==
        std::max<std::size_t>(1, (texels + kDataTextureWidth - 1) / kDataTextureWidth));
  REQUIRE(data.size() == ch * std::size_t(t.width) * std::size_t(t.height));
  CHECK(std::vector<T>(data.begin(), data.begin() + std::ptrdiff_t(a.size)) ==
        std::vector<T>(a.data, a.data + a.size));
  bool zeros = true;
  for (std::size_t i = a.size; i < data.size(); ++i) zeros = zeros && data[i] == T(0);
  CHECK(zeros);
}

}  // namespace

TEST_CASE("porous: field_glsl compiles the four cases to data textures and a smooth uniform",
          "[field_glsl][porous]") {
  using F = DataTexture::Format;
  struct Case {
    const char* keys;
    const char* fn;
    std::vector<std::pair<std::string, F>> textures;
  };
  const std::vector<Case> cases = {
      {"", "dcPorousVoronoiWalls",
       {{"seeds", F::RGBA32F}, {"offsets", F::R32I}, {"neighbours", F::R32I},
        {"start", F::R32I}}},
      {", cells=delaunay", "dcPorousDelaunayWalls",
       {{"planes", F::RGBA32F}, {"tetNeighbours", F::RGBA32I}, {"start", F::R32I}}},
      {", mode=struts", "dcPorousStruts",
       {{"segments", F::RGBA32F}, {"binOffsets", F::R32I}, {"binItems", F::R32I}}},
      {", cells=delaunay, mode=struts", "dcPorousStruts",
       {{"segments", F::RGBA32F}, {"binOffsets", F::R32I}, {"binItems", F::R32I}}},
  };
  FileMeshResolver resolver;
  for (const Case& c : cases) {
    INFO(c.keys);
    const GraphNode g = parseShorthand(std::string("porous(sphere(radius=1), spacing=0.3, "
                                                   "smooth=0.04") +
                                       c.keys + ")");
    const GlslScene s = compileToGlsl(g, resolver, box(-1.2, 1.2));
    const FieldGraph fg = FieldGraph::build(g, resolver);
    const dualc::PorousField::Packed pk = porousOf(fg).packed();

    REQUIRE(s.dataTextures.size() == c.textures.size());
    CHECK(s.meshes.empty());
    CHECK(s.notes.empty());
    for (std::size_t i = 0; i < c.textures.size(); ++i) {
      const DataTexture& t = s.dataTextures[i];
      CHECK(t.samplerName == "u_n0_" + c.textures[i].first);
      CHECK(t.format == c.textures[i].second);
      const std::string decl = std::string("uniform ") +
                               (t.format == F::RGBA32F ? "sampler2D " : "isampler2D ") +
                               t.samplerName + ";";
      CHECK(countOccurrences(s.generatedSource, decl) == 1);
      // Within the limits P01 recorded: WebGL2's 2048 rows, 16 units.
      CHECK(t.height <= GlslLimits{}.maxTextureSize);
    }
    CHECK(s.dataTextures.size() <= std::size_t(GlslLimits{}.maxTextureUnits));
    if (pk.mode == dualc::PorousMode::Struts) {
      checkTexture(s.dataTextures[0], pk.segments, s.dataTextures[0].floats);
      checkTexture(s.dataTextures[1], pk.binOffsets, s.dataTextures[1].ints);
      checkTexture(s.dataTextures[2], pk.binItems, s.dataTextures[2].ints);
      CHECK(pk.bound > 0.0f);
    } else if (pk.cells == dualc::PorousCells::Voronoi) {
      checkTexture(s.dataTextures[0], pk.seeds, s.dataTextures[0].floats);
      checkTexture(s.dataTextures[1], pk.neighbourOffsets, s.dataTextures[1].ints);
      checkTexture(s.dataTextures[2], pk.neighbours, s.dataTextures[2].ints);
      checkTexture(s.dataTextures[3], pk.binStart, s.dataTextures[3].ints);
    } else {
      checkTexture(s.dataTextures[0], pk.tetPlanes, s.dataTextures[0].floats);
      checkTexture(s.dataTextures[1], pk.tetNeighbours, s.dataTextures[1].ints);
      checkTexture(s.dataTextures[2], pk.binStart, s.dataTextures[2].ints);
    }
    // smooth is the one uniform; the call passes it, the textures and the bins.
    REQUIRE(s.bindings.size() == 1);
    CHECK(s.bindings[0].name == "u_n0_smooth");
    CHECK(s.bindings[0].paramKey == "smooth");
    CHECK(s.bindings[0].value == std::vector<float>{0.04f});
    CHECK(countOccurrences(s.generatedSource, std::string("return ") + c.fn + "(p, u_n0_smooth, ") == 1);
    CHECK(countOccurrences(s.generatedSource,
                           "ivec3(" + std::to_string(pk.binDims[0]) + ", " +
                               std::to_string(pk.binDims[1]) + ", " +
                               std::to_string(pk.binDims[2]) + ")") == 1);
    // The volume places the seeds and is not traced; the node is 1-Lipschitz.
    CHECK(s.generatedSource.find("sdSphere") == std::string::npos);
    CHECK(s.stepScale == 1.0f);
    CHECK(s.featureScale == 0.3f);
    // The ES header gives the samplers a precision.
    const std::string es = assembleTraceShader(s, true);
    CHECK(es.find("precision highp isampler2D;") != std::string::npos);
    CHECK(es.find("precision highp sampler2D;") != std::string::npos);
  }
}

TEST_CASE("porous: field_glsl reads every key, rejects an unknown one, keeps children untraced",
          "[field_glsl][porous]") {
  FileMeshResolver resolver;
  // Every key of the signature, so this TU's unread-key check sees each read.
  CHECK_NOTHROW(compileToGlsl(
      parseShorthand("porous(sphere(radius=1), plane(1,0,0,0), cells=voronoi, mode=walls, "
                     "dist=jitter, spacing=0.3, spacing2=0.4, d0=-1, d1=1, ramp=smooth, "
                     "relax=1, seed=7, jitter=0.5, smooth=0.02)"),
      resolver, box(-1.5, 1.5)));
  // A typo is a located error, as on the contour path.
  const std::string err = glslError("porous(sphere(radius=1), spacing=0.3, smoth=0.1)");
  CHECK(err.find("smoth") != std::string::npos);
  CHECK(err.rfind("0:", 0) == 0);
  // A gyroid control grades the seeds only: no TPMS in the trace, so neither
  // its step scale nor its wavelength reaches the scene.
  const GlslScene s = compileToGlsl(
      parseShorthand("porous(sphere(radius=1), gyroid(wavelength=0.1), spacing=0.25, "
                     "spacing2=0.35, d1=1)"),
      resolver, box(-1.2, 1.2));
  CHECK(s.generatedSource.find("sdGyroid(p") == std::string::npos);
  CHECK(s.stepScale == 1.0f);
  CHECK(s.featureScale == 0.25f);
  // A smooth boolean of 1-Lipschitz operands is 1-Lipschitz, so the sponge
  // recipe keeps the metric step (#70); a TPMS operand still lowers it.
  const GlslScene sponge = compileToGlsl(
      parseShorthand("smooth-intersection(sphere(radius=1), onion(porous(sphere(radius=1), "
                     "spacing=0.3, smooth=0.02), thickness=0.04), k=0.1)"),
      resolver, box(-1.2, 1.2));
  CHECK(sponge.stepScale == 1.0f);
  const GlslScene tpms = compileToGlsl(
      parseShorthand("smooth-union(sphere(radius=1), gyroid(wavelength=0.5), k=0.1)"),
      resolver, box(-1.2, 1.2));
  CHECK(tpms.stepScale == 0.5f);
  // A repeated porous node uploads its arrays once (DAG dedup).
  const GlslScene d = compileToGlsl(
      parseShorthand("union(onion(porous(sphere(radius=1), spacing=0.3), thickness=0.02), "
                     "offset(porous(sphere(radius=1), spacing=0.3), r=0.01))"),
      resolver, box(-1.2, 1.2));
  CHECK(d.dataTextures.size() == 4);
  CHECK(bindingsWithKey(d, "smooth") == 1);
}

TEST_CASE("porous: field_glsl bakes the node past the texture limits", "[field_glsl][porous]") {
  FileMeshResolver resolver;
  const GraphNode g = parseShorthand("porous(sphere(radius=1), spacing=0.3)");
  const dualc::BBox region = box(-1.2, 1.2);
  // Voronoi walls need 4 units: allow 3, and the node becomes a sampler3D.
  GlslLimits units;
  units.maxTextureUnits = 3;
  const GlslScene s = compileToGlsl(g, resolver, region, 24, 0, units);
  CHECK(s.dataTextures.empty());
  REQUIRE(s.meshes.size() == 1);
  REQUIRE(s.notes.size() == 1);
  CHECK(s.notes[0].find("baked to a 24^3 grid") != std::string::npos);
  CHECK(s.stepScale == 0.5f);
  // The grid is the builder's field baked over the trace box.
  const FieldGraph fg = FieldGraph::build(g, resolver);
  const dualc::FieldPtr grid = dualc::bakeToGrid(fg.field(), region, 24);
  const auto* gf = dynamic_cast<const dualc::GridField*>(grid.get());
  REQUIRE(gf != nullptr);
  CHECK(s.meshes[0].values == gf->values());
  // A texture taller than the limit takes the same path: the neighbour list
  // spans more than one 2048-wide row here.
  GlslLimits rows;
  rows.maxTextureSize = 1;
  const GlslScene r = compileToGlsl(g, resolver, region, 24, 0, rows);
  CHECK(r.dataTextures.empty());
  CHECK(r.meshes.size() == 1);
  // Within the defaults it stays exact.
  CHECK(compileToGlsl(g, resolver, region, 24).dataTextures.size() == 4);
}

namespace {

bool sameTextures(const GlslScene& a, const GlslScene& b) {
  if (a.dataTextures.size() != b.dataTextures.size()) return false;
  for (std::size_t i = 0; i < a.dataTextures.size(); ++i) {
    const DataTexture& x = a.dataTextures[i];
    const DataTexture& y = b.dataTextures[i];
    if (x.samplerName != y.samplerName || x.format != y.format || x.width != y.width ||
        x.height != y.height || x.floats != y.floats || x.ints != y.ints)
      return false;
  }
  return true;
}

}  // namespace

// What dualc_field_view relies on (roadmap 23 #66, P05.2): a reload recompiles
// and re-uploads the scene's textures (applyScene), so a structural key must
// reach the textures or the source; a nudge only rewrites a uniform, so smooth
// must leave both alone and live in its binding.
TEST_CASE("porous: field_glsl a structural key changes the textures, smooth only its uniform",
          "[field_glsl][porous]") {
  FileMeshResolver resolver;
  const dualc::BBox region = box(-1.2, 1.2);
  const auto compile = [&](const std::string& keys) {
    return compileToGlsl(parseShorthand("porous(sphere(radius=1), spacing=0.3, smooth=0.04" +
                                        keys + ")"),
                         resolver, region);
  };
  const GlslScene base = compile("");

  const GlslScene nudged = compile(", smooth=0.08");
  CHECK(nudged.generatedSource == base.generatedSource);
  CHECK(sameTextures(nudged, base));
  REQUIRE(nudged.bindings.size() == 1);
  CHECK(nudged.bindings[0].name == base.bindings[0].name);
  CHECK(nudged.bindings[0].value == std::vector<float>{0.08f});

  for (const char* keys :
       {", spacing=0.35", ", seed=7", ", dist=jitter", ", relax=2", ", cells=delaunay",
        ", mode=struts"}) {
    INFO(keys);
    const GlslScene s = compile(keys);
    CHECK((s.generatedSource != base.generatedSource || !sameTextures(s, base)));
  }
  CHECK_FALSE(sameTextures(compile(", dist=jitter"), compile(", dist=jitter, jitter=0.5")));
  // spacing2 needs a control, and the control is structural too: it grades
  // the seeds.
  const GlslScene graded = compileToGlsl(
      parseShorthand("porous(sphere(radius=1), sphere(radius=0), spacing=0.3, spacing2=0.4, "
                     "d1=1, smooth=0.04)"),
      resolver, region);
  const GlslScene graded2 = compileToGlsl(
      parseShorthand("porous(sphere(radius=1), sphere(center=[0.5,0,0],radius=0), "
                     "spacing=0.3, spacing2=0.4, d1=1, smooth=0.04)"),
      resolver, region);
  CHECK_FALSE(sameTextures(graded, graded2));

  // In the sponge recipe the shell's thicknesses and both smooth radii nudge.
  const GlslScene sponge = compileToGlsl(
      parseShorthand("smooth-intersection(sphere(radius=1), graded-onion(porous(sphere(radius=1), "
                     "spacing=0.4, smooth=0.02), sphere(radius=0), t1=0.04, t2=0.08, d1=1), k=0.1)"),
      resolver, region);
  for (const char* key : {"t1", "t2", "k", "smooth"}) {
    INFO(key);
    CHECK(bindingsWithKey(sponge, key) == 1);
  }
}
