#pragma once

#include "dualc/dualc.h"

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Field-graph: a composable, serialisable description of a dualc ImplicitField.
//
// This host-side module parses the canonical JSON form (docs/roadmap/12) into a
// lightweight node tree (`GraphNode`) and lowers it to a `dualc::FieldPtr` by
// reusing the central registries in example_common.h plus the implicit.h
// factories -- it adds NO new field maths, only wiring. It lives in examples/
// (not libdualc) because it touches mesh file I/O and the CLI registry tokens.
//
// The parser/builder is deliberately a separate translation unit so the
// dualc_field CLI, the future raymarch side-car, and an eventual C ABI all share
// ONE field-graph implementation rather than forking it.

namespace geometrycentral {
namespace surface {
class SurfaceMesh;
class VertexPositionGeometry;
}  // namespace surface
}  // namespace geometrycentral

namespace dce {
namespace fieldgraph {

// A parse/build failure located at a JSON-pointer-style path into the document
// (e.g. "/root/in/0"). The CLI prints `field-graph error at <pointer>: <msg>`.
class GraphError : public std::runtime_error {
 public:
  GraphError(const std::string& message, std::string pointer)
      : std::runtime_error(message), pointer_(std::move(pointer)) {}
  const std::string& pointer() const { return pointer_; }

 private:
  std::string pointer_;
};

// One field-graph node, decoded from JSON. Parameters are kept as a small typed
// map so this header never pulls in the heavy JSON dependency; the builder reads
// them back through the param helpers in field_graph.cpp. `pointer` is the
// node's JSON-pointer path, carried only for diagnostics.
struct ParamValue {
  enum class Kind { Number, Vector, String } kind = Kind::Number;
  double num = 0.0;             // Kind::Number
  std::vector<double> vec;      // Kind::Vector
  std::string str;              // Kind::String
};

struct GraphNode {
  std::string op;
  std::map<std::string, ParamValue> params;
  std::vector<GraphNode> in;    // children (sources: 0, decorators: 1, booleans: 2)
  std::string pointer;
};

// Resolves a mesh node's `path` to a loaded mesh + geometry. The handle's
// pointers are non-owning and stay valid for the resolver's lifetime, so the
// resolver MUST outlive any field built from it (MeshSource holds bare refs).
// The default FileMeshResolver loads from disk and caches by path (so a mesh
// referenced twice loads once); the C ABI / Grasshopper can later supply an
// in-memory resolver instead -- the builder is agnostic.
struct MeshHandle {
  geometrycentral::surface::SurfaceMesh* mesh = nullptr;
  geometrycentral::surface::VertexPositionGeometry* geom = nullptr;
};

class MeshResolver {
 public:
  virtual ~MeshResolver() = default;
  // Resolve a `mesh(path=...)` / `winding(path=...)` source from the filesystem.
  // Throws GraphError if the path cannot be loaded.
  virtual MeshHandle resolve(const std::string& path) = 0;
  // Resolve a `mesh(id=...)` / `winding(id=...)` source from host-provided
  // in-memory buffers. The base implementation has NO registry and always throws
  // -- only InMemoryMeshResolver (the C ABI / Grasshopper path) overrides it.
  // Kept separate from resolve() so an `id` is never silently treated as a path.
  virtual MeshHandle resolveId(const std::string& id);
};

class FileMeshResolver : public MeshResolver {
 public:
  FileMeshResolver();
  ~FileMeshResolver() override;
  MeshHandle resolve(const std::string& path) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Resolves `id=` mesh nodes against host-provided in-memory triangle buffers, and
// `path=` mesh nodes against the filesystem (delegating to an internal
// FileMeshResolver, so `path=` keeps working unchanged). registerMesh COPIES the
// buffers into an owned geometry-central mesh built byte-for-byte the way
// readSurfaceMesh builds a loaded OBJ -- so the host's buffers need only stay
// valid for the duration of the registerMesh call, not the field's lifetime.
class InMemoryMeshResolver : public MeshResolver {
 public:
  InMemoryMeshResolver();
  ~InMemoryMeshResolver() override;

  // Copy a triangle soup into an owned mesh, keyed by `id`. `vertices` is
  // 3*vertexCount floats (x,y,z); `indices` is 3*triangleCount 0-based indices.
  // Throws GraphError on a malformed buffer (empty id, null/empty arrays, or an
  // out-of-range index) or a duplicate id.
  void registerMesh(const std::string& id, const float* vertices,
                    std::uint32_t vertexCount, const std::uint32_t* indices,
                    std::uint32_t triangleCount);

  MeshHandle resolve(const std::string& path) override;   // file (delegated)
  MeshHandle resolveId(const std::string& id) override;   // registry only

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Parse the canonical JSON form into a node tree. Accepts either a full document
// `{ "version":1, "units":"mm", "root":<node> }` or a bare node `<node>`.
// Throws GraphError (with a JSON pointer) on malformed JSON or shape.
GraphNode parseJson(const std::string& text);

// Parse the terse text shorthand into the SAME node tree parseJson produces, so
// the builder (FieldGraph::build) is shared verbatim. The grammar is uniform
// prefix-call `op(arg, ...)` where each arg is a nested op-call (a child -> `in`,
// in order), a `key=value` named param, or a bare positional scalar / `[...]`
// array (flattened into params["params"], the primitive flat-param path). Op
// tokens accept underscore aliases for the hyphenated registry tokens
// (schwarz_p -> schwarz-p, smooth_union -> smooth-union, ...). String values are
// bare IDENTs (axis=x, fn=sine) or double-quoted literals (path="..."). Throws
// GraphError (with a best-effort character-offset pointer) on a parse error.
GraphNode parseShorthand(const std::string& text);

// Serialise a node tree to the canonical pretty JSON (the inverse of the JSON
// reader): `{ "version":1, "units":"mm", "root": { "op":..., <params>, "in":[...]
// } }` (the "in" array is omitted when empty). Round-trips through parseJson.
std::string dumpJson(const GraphNode& root);

// A built field: owns the FieldPtr tree. The meshes it references are owned by
// the MeshResolver passed to build(), which the caller must keep alive.
class FieldGraph {
 public:
  // Lower a parsed node tree to a FieldPtr, resolving mesh sources via `meshes`.
  // Throws GraphError on any semantic problem (unknown op, wrong arity, bad
  // param, gwn-on-mesh, mesh-not-found).
  static FieldGraph build(const GraphNode& root, MeshResolver& meshes);

  const dualc::ImplicitField& field() const { return *field_; }
  const dualc::FieldPtr& fieldPtr() const { return field_; }

 private:
  explicit FieldGraph(dualc::FieldPtr f) : field_(std::move(f)) {}
  dualc::FieldPtr field_;
};

}  // namespace fieldgraph
}  // namespace dce
