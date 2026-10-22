#pragma once

#include "field_graph.h"

#include "dualc/dualc.h"

#include <cstdint>
#include <string>
#include <vector>

// Field-graph -> GLSL codegen (roadmap docs/roadmap/12 §B).
//
// Turns a parsed `GraphNode` tree into one generated `float sceneSDF(vec3 p)`
// plus a binding table, so the SAME field-graph that dual-contours for export
// (field_graph.cpp) also compiles to a GLSL sphere-tracer for live preview. The
// compiler walks the tree exactly as FieldGraph::build does and emits one pure
// `float fN(vec3 p)` per node (point flows DOWN through domain warps, distance
// flows UP) -- no recursion in GLSL, so functions are emitted in topological
// order with the root last.
//
// Two edit tiers (the binding table is a first-class output, not just a string):
//   * Structural edits (add/remove a node, change an op, swap a mesh) require a
//     recompile -- the generated source changes.
//   * Parameter edits (a radius, a wavelength, a smooth-k) only update a uniform
//     -- listed in `GlslScene::bindings`, so a slider-drag never recompiles.
//
// This is host-side (it consumes the host `GraphNode` + registry tokens and
// bakes mesh sources), so like field_graph it lives in examples/, not libdualc.
//
// Scope (2026-06-24): the codegen accepts the SAME node vocabulary as the contour
// path (field_graph.cpp) -- the 6 TPMS, all ~29 analytic primitives (incl. the
// open triangle/quad and the infinite plane/cylinder/cone), all 7 booleans,
// offset/round/onion/scale/elongate/normalize/transform/translate/rotate, the
// twist/bend/mirror/repeat/repeat-limited/displace domain ops, `mesh`/`winding`
// (baked to a sampler3D), and (#58, 2026-10-15) the 2D profiles circle/rect/
// segment/polygon under the lifts revolve/extrude -- a profile emits a
// `float fN(vec2 p)`, read through field_graph's readProfile, and a polygon
// above kMaxGlslPolygonVertices is refused, and (#66, 2026-10-22) `porous`: the
// builder's own PorousField, its packed complex uploaded as 2D data textures
// (DataTexture) and queried by the same walk and fold. A genuinely unknown op still throws a GraphError naming
// it. Each prelude helper is bit-faithful to its src/implicit/ formula (gated by
// dualc_glsl_parity). Emission is a **deduped DAG**: nodes are keyed by
// structural equality (op + params + children), so a duplicated subtree emits one
// `fN` / one uniform set that every reference shares, and a duplicated `mesh`
// bakes once into a single sampler3D. The GLSL body is authored to the WebGL2
// (`#version 300 es`) subset;
// `emitES` selects the version/precision header (desktop GL gets 330 core).

namespace dce {
namespace fieldgraph {

// One editable parameter -> one GLSL uniform. A parameter edit sets this uniform
// (no recompile). `value` carries the current value (1 float for a scalar, 3 for
// a vec3, 16 for a mat4); `nodePointer`/`paramKey` point back at the source node
// so a UI / the side-car can map a widget to a uniform.
struct UniformBinding {
  std::string name;        // GLSL uniform name, e.g. "u_n3_radius"
  std::string glslType;    // "float" | "vec2" | "vec3" | "mat4"
  std::vector<float> value; // 1 / 2 / 3 / 16 floats by glslType
  std::string nodePointer; // source node's diagnostic pointer
  std::string paramKey;    // which graph param drives it ("radius", "k", ...)
};

// One unique mesh source baked to a narrow-band SDF grid -> one sampler3D. The
// app uploads `values` to a GL_R32F 3D texture on its own texture unit and feeds
// the texMin/texScale/texDim uniforms (named after `samplerName`).
struct MeshTexture {
  std::string    samplerName;   // e.g. "u_n5_tex"
  dualc::Vector3i resolution;
  dualc::BBox     region;
  std::vector<float> values;    // resolution.x*y*z, x-fastest (GridField layout)
};

// One packed array of a `porous` node (roadmap 23 #66) -> one 2D data texture
// the app uploads unfiltered on its own texture unit and the GLSL reads with
// texelFetch: item i at texel ivec2(i % kDataTextureWidth, i / kDataTextureWidth),
// the last row zero-padded. RGBA32F holds geometry (a seed, a plane, a segment
// end), R32I / RGBA32I indices through an isampler2D, exact where a float index
// is not and able to say -1. `floats` (RGBA32F) or `ints` (the two integer
// formats) holds channels x width x height values, row-major.
struct DataTexture {
  enum class Format { RGBA32F, R32I, RGBA32I };
  std::string samplerName;  // e.g. "u_n4_seeds"
  Format      format = Format::RGBA32F;
  int         width = 0, height = 0;
  std::vector<float>        floats;
  std::vector<std::int32_t> ints;
};

// Values per texel: 4 for RGBA32F / RGBA32I, 1 for R32I.
inline int dataTextureChannels(DataTexture::Format f) {
  return f == DataTexture::Format::R32I ? 1 : 4;
}

// The width of every data texture: WebGL2's guaranteed MAX_TEXTURE_SIZE.
inline constexpr int kDataTextureWidth = 2048;

// What the target GL allows, for the codegen's fallback: a `porous` node whose
// data textures would pass `maxTextureSize` rows, or whose units would take the
// scene past `maxTextureUnits` (mesh grids and data textures together), is
// baked to a sampler3D like a mesh instead and says so in GlslScene::notes. The
// defaults are the guarantees (WebGL2's 2048, GL 3.3's and WebGL2's 16
// fragment units); an app with a context passes the values it queried.
struct GlslLimits {
  int maxTextureSize = 2048;
  int maxTextureUnits = 16;
};

// The compiled scene: generated GLSL + the tables the host needs to feed it.
struct GlslScene {
  std::string generatedSource;          // uniform decls + per-node fN + sceneSDF
  std::vector<UniformBinding> bindings; // tier-2 (parameter) edit table
  std::vector<MeshTexture>    meshes;   // one per unique mesh node
  std::vector<DataTexture>    dataTextures; // porous nodes' arrays; their units
                                            // follow the meshes', in this order
  std::vector<std::string>    notes;    // codegen choices the app reports, e.g.
                                        // a porous node baked past the limits
  float       stepScale = 1.0f;         // graph-level safe sphere-trace factor
  float       featureScale = 0.0f;      // smallest characteristic feature length
                                        // (min TPMS wavelength); 0 => none, the
                                        // app falls back to the bounds diagonal.
                                        // Drives the FD step + step cap so a dense
                                        // lattice in large bounds still resolves.
  dualc::BBox bounds;                   // trace box (root bounds)
  int         nodeCount = 0;
};

// Per-axis bake resolution for mesh/winding sources. One source of truth for
// the codegen default, the dualc_field_view flag, its usage text and the docs.
// 256^3 R32F is 64 MB per unique mesh node.
inline constexpr int kDefaultMeshGridRes = 96;
inline constexpr int kMaxMeshGridRes = 256;

// A `polygon` profile compiles to a constant vertex array walked by a loop of
// that fixed length, so the codegen takes at most this many vertices and
// refuses more with a located error. The export path (field_graph) has no
// ceiling; 64 keeps the unrolled-or-looped cost of one profile below a TPMS's.
inline constexpr int kMaxGlslPolygonVertices = 64;

// Compile a graph to GLSL. `bounds` is the trace/sampling box (the CLI flag, not
// a graph node); mesh sources bake within it. `gridRes` is the per-axis bake
// resolution of every mesh/winding node without its own `bake=N` (a node with
// one uploads the builder's grid unchanged) -- because the bake region IS `bounds`,
// tightening `bounds` concentrates that resolution on the region of interest
// rather than spreading it over the whole mesh AABB. `bakeThreads` is the
// worker count of every bake (0 = all hardware threads). Throws GraphError
// (op-located) on an op the codegen does not yet support, or any builder-style
// semantic problem.
GlslScene compileToGlsl(const GraphNode& root, MeshResolver& meshes,
                        const dualc::BBox& bounds,
                        int gridRes = kDefaultMeshGridRes,
                        unsigned bakeThreads = 0,
                        const GlslLimits& limits = GlslLimits{});

// Assemble the full fragment shader for the live sphere-tracer (app): version
// header + fixed primitive/op library + the scene's generated source + the fixed
// trace/camera/normals/shading framework. `emitES` => `#version 300 es`.
std::string assembleTraceShader(const GlslScene& scene, bool emitES);

// Assemble a fragment shader that outputs sceneSDF(p) over a sample grid to a
// single-channel float target -- the parity harness renders this and compares to
// the C++ field. Same library, a trivial value-emitting main.
std::string assembleValueShader(const GlslScene& scene, bool emitES);

// The shared full-screen-triangle vertex shader.
std::string vertexShaderSource(bool emitES);

}  // namespace fieldgraph
}  // namespace dce
