#include "field_graph.h"

#include "example_common.h"

// nlohmann/json is vendored; quiet its warnings only (project builds /W4
// /permissive- and -Wall -Wextra -Wpedantic, which the amalgamation does not target).
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "third_party/json.hpp"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/simple_polygon_mesh.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/surface_mesh_factories.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Field-graph parser + builder. Pure wiring over the example_common registries
// (buildPrimitive / makeTpmsField / applyBoolOp / applyPostOps) and the
// implicit.h factories (transformed / normalizedOf / windingNumberField). See
// docs/roadmap/12-field-graph-and-app/01-field-graph.md for the locked schema.

using json = nlohmann::json;

namespace dce {
namespace fieldgraph {

// ===========================================================================
// FileMeshResolver -- load-and-cache meshes by path; non-owning handles.
// ===========================================================================

struct FileMeshResolver::Impl {
  // Each entry owns the mesh + geometry for the resolver's lifetime.
  struct Loaded {
    std::unique_ptr<geometrycentral::surface::SurfaceMesh> mesh;
    std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geom;
  };
  std::map<std::string, Loaded> cache;
};

FileMeshResolver::FileMeshResolver() : impl_(new Impl) {}
FileMeshResolver::~FileMeshResolver() = default;

MeshHandle FileMeshResolver::resolve(const std::string& path) {
  auto it = impl_->cache.find(path);
  if (it == impl_->cache.end()) {
    Impl::Loaded loaded;
    try {
      auto loadedTuple = geometrycentral::surface::readSurfaceMesh(path);
      loaded.mesh = std::move(std::get<0>(loadedTuple));
      loaded.geom = std::move(std::get<1>(loadedTuple));
    } catch (const std::exception& e) {
      throw GraphError("cannot read mesh '" + path + "': " + e.what(), "");
    }
    it = impl_->cache.emplace(path, std::move(loaded)).first;
  }
  return MeshHandle{it->second.mesh.get(), it->second.geom.get()};
}

// The base resolver has no in-memory registry: an `id=` source is only valid
// when a host (the C ABI / Grasshopper) supplies buffers via InMemoryMeshResolver.
MeshHandle MeshResolver::resolveId(const std::string& id) {
  throw GraphError("mesh id '" + id +
                       "' requires in-memory meshes, but none were provided "
                       "(use a file path, or a *_with_meshes C ABI create call)",
                   "");
}

// ===========================================================================
// InMemoryMeshResolver -- host-provided triangle buffers, copied into owned
// geometry-central meshes; `path=` still works via a delegated FileMeshResolver.
// ===========================================================================

struct InMemoryMeshResolver::Impl {
  struct Loaded {
    std::unique_ptr<geometrycentral::surface::SurfaceMesh> mesh;
    std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geom;
  };
  std::map<std::string, Loaded> registry;  // owned, keyed by host id
  FileMeshResolver files;                   // delegate for `path=` sources
};

InMemoryMeshResolver::InMemoryMeshResolver() : impl_(new Impl) {}
InMemoryMeshResolver::~InMemoryMeshResolver() = default;

void InMemoryMeshResolver::registerMesh(const std::string& id,
                                        const float* vertices,
                                        std::uint32_t vertexCount,
                                        const std::uint32_t* indices,
                                        std::uint32_t triangleCount) {
  if (id.empty())
    throw GraphError("in-memory mesh source has an empty id", "");
  if (impl_->registry.count(id))
    throw GraphError("duplicate in-memory mesh id '" + id + "'", "");
  if (!vertices || vertexCount == 0)
    throw GraphError("in-memory mesh '" + id + "' has no vertices", "");
  if (!indices || triangleCount == 0)
    throw GraphError("in-memory mesh '" + id + "' has no triangles", "");

  geometrycentral::surface::SimplePolygonMesh simple;
  simple.vertexCoordinates.resize(vertexCount);
  for (std::uint32_t i = 0; i < vertexCount; ++i) {
    simple.vertexCoordinates[i] = geometrycentral::Vector3{
        static_cast<double>(vertices[3 * i + 0]),
        static_cast<double>(vertices[3 * i + 1]),
        static_cast<double>(vertices[3 * i + 2])};
  }
  simple.polygons.resize(triangleCount);
  for (std::uint32_t t = 0; t < triangleCount; ++t) {
    simple.polygons[t].resize(3);
    for (std::uint32_t k = 0; k < 3; ++k) {
      const std::uint32_t idx = indices[3 * t + k];
      if (idx >= vertexCount)
        throw GraphError("in-memory mesh '" + id +
                             "' has an out-of-range vertex index",
                         "");
      simple.polygons[t][k] = static_cast<std::size_t>(idx);
    }
  }

  // Mirror readSurfaceMesh's construction exactly (for an OBJ, processLoadedMesh
  // is just stripUnusedVertices), so an `id=` mesh and the same geometry loaded
  // from a temp OBJ via `path=` produce an identical mesh + contour.
  simple.stripUnusedVertices();
  Impl::Loaded loaded;
  try {
    auto built = geometrycentral::surface::makeSurfaceMeshAndGeometry(
        simple.polygons, simple.vertexCoordinates);
    loaded.mesh = std::move(std::get<0>(built));
    loaded.geom = std::move(std::get<1>(built));
  } catch (const std::exception& e) {
    throw GraphError(
        "cannot build in-memory mesh '" + id + "': " + e.what(), "");
  }
  impl_->registry.emplace(id, std::move(loaded));
}

MeshHandle InMemoryMeshResolver::resolve(const std::string& path) {
  return impl_->files.resolve(path);
}

MeshHandle InMemoryMeshResolver::resolveId(const std::string& id) {
  auto it = impl_->registry.find(id);
  if (it == impl_->registry.end())
    throw GraphError("no in-memory mesh registered with id '" + id + "'", "");
  return MeshHandle{it->second.mesh.get(), it->second.geom.get()};
}

// ===========================================================================
// JSON -> GraphNode
// ===========================================================================

namespace {

ParamValue toParam(const json& v, const std::string& pointer) {
  ParamValue pv;
  if (v.is_number()) {
    pv.kind = ParamValue::Kind::Number;
    pv.num = v.get<double>();
  } else if (v.is_string()) {
    pv.kind = ParamValue::Kind::String;
    pv.str = v.get<std::string>();
  } else if (v.is_array()) {
    pv.kind = ParamValue::Kind::Vector;
    for (std::size_t i = 0; i < v.size(); ++i) {
      if (!v[i].is_number())
        throw GraphError("array parameter must contain only numbers",
                         pointer + "/" + std::to_string(i));
      pv.vec.push_back(v[i].get<double>());
    }
  } else {
    throw GraphError("unsupported parameter type (expected number, string or "
                     "number array)",
                     pointer);
  }
  return pv;
}

GraphNode toNode(const json& j, const std::string& pointer) {
  if (!j.is_object())
    throw GraphError("node must be a JSON object", pointer);
  auto opIt = j.find("op");
  if (opIt == j.end() || !opIt->is_string())
    throw GraphError("node is missing a string \"op\"", pointer);

  GraphNode node;
  node.op = opIt->get<std::string>();
  node.pointer = pointer;
  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string& key = it.key();
    if (key == "op") continue;
    if (key == "in") {
      if (!it->is_array())
        throw GraphError("\"in\" must be an array of child nodes",
                         pointer + "/in");
      for (std::size_t i = 0; i < it->size(); ++i)
        node.in.push_back(
            toNode((*it)[i], pointer + "/in/" + std::to_string(i)));
    } else {
      node.params[key] = toParam(*it, pointer + "/" + key);
    }
  }
  return node;
}

}  // namespace

GraphNode parseJson(const std::string& text) {
  json doc;
  try {
    doc = json::parse(text);
  } catch (const json::parse_error& e) {
    throw GraphError(std::string("invalid JSON: ") + e.what(), "");
  }
  // Accept either a full document {version, units, root} or a bare node.
  if (doc.is_object() && doc.contains("root"))
    return toNode(doc["root"], "/root");
  return toNode(doc, "");
}

// ===========================================================================
// Shorthand (terse text) -> GraphNode
//
// Uniform prefix-call grammar (no infix ops):
//   node := IDENT '(' args? ')' | IDENT
//   args := arg (',' arg)*
//   arg  := node                 -> child (appended to params' `in`, in order)
//         | key '=' value        -> params[key]
//         | number | '[' ... ']' -> flat positional, concatenated into
//                                   params["params"] (the primitive flat path)
//   value := number | '[' number (',' ...)* ']' | IDENT | "..."
// Op tokens accept underscore aliases for the hyphenated registry tokens; value
// IDENTs pass through verbatim. The parser is purely syntactic -- arity / param
// validity is the builder's job, so the existing builder-side error tests and
// JSON/shorthand parity both hold.
// ===========================================================================

namespace {

// Map a shorthand op token to its canonical (hyphenated) registry token. Applied
// ONLY at op position; never to value IDENTs.
std::string canonicalOpToken(std::string tok) {
  static const std::map<std::string, std::string> alias = {
      {"schwarz_p", "schwarz-p"},
      {"fischer_koch", "fischer-koch"},
      {"smooth_union", "smooth-union"},
      {"smooth_intersection", "smooth-intersection"},
      {"smooth_difference", "smooth-difference"},
      {"repeat_limited", "repeat-limited"},
  };
  auto it = alias.find(tok);
  return it == alias.end() ? tok : it->second;
}

class ShorthandParser {
 public:
  explicit ShorthandParser(const std::string& text) : s_(text) {}

  GraphNode parse() {
    skipWs();
    GraphNode root = parseNode();
    skipWs();
    if (pos_ != s_.size())
      fail("unexpected trailing text after the root node");
    return root;
  }

 private:
  const std::string& s_;
  std::size_t pos_ = 0;

  [[noreturn]] void fail(const std::string& msg) {
    throw GraphError(msg, std::to_string(pos_));
  }

  bool eof() const { return pos_ >= s_.size(); }
  char peek() const { return s_[pos_]; }

  static bool isWs(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }
  void skipWs() {
    while (!eof() && isWs(peek())) ++pos_;
  }

  // An identifier: letter/underscore start, then letters/digits/'_'/'-' (so the
  // hyphenated registry tokens are also accepted directly, not only via alias).
  static bool isIdentStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  static bool isIdentChar(char c) {
    return isIdentStart(c) || (c >= '0' && c <= '9') || c == '-';
  }

  std::string readIdent() {
    std::size_t start = pos_;
    while (!eof() && isIdentChar(peek())) ++pos_;
    if (pos_ == start) fail("expected an identifier");
    return s_.substr(start, pos_ - start);
  }

  // Parse a number with strtod (leading '-' is part of the number -- there are
  // no infix operators in this grammar).
  double readNumber() {
    const char* begin = s_.c_str() + pos_;
    char* end = nullptr;
    double v = std::strtod(begin, &end);
    if (end == begin) fail("expected a number");
    pos_ += static_cast<std::size_t>(end - begin);
    return v;
  }

  // A double-quoted string literal. Backslashes are treated literally (only \"
  // is an escape) so Windows paths like "D:\bracket.obj" survive verbatim.
  std::string readString() {
    ++pos_;  // consume opening quote
    std::string out;
    while (true) {
      if (eof()) fail("unterminated string literal");
      char c = s_[pos_++];
      if (c == '"') break;
      if (c == '\\' && !eof() && s_[pos_] == '"') {
        out.push_back('"');
        ++pos_;
        continue;
      }
      out.push_back(c);
    }
    return out;
  }

  // A bracketed number array: '[' number (',' number)* ']'.
  std::vector<double> readArray() {
    ++pos_;  // consume '['
    std::vector<double> v;
    skipWs();
    if (!eof() && peek() == ']') {
      ++pos_;
      return v;
    }
    while (true) {
      skipWs();
      v.push_back(readNumber());
      skipWs();
      if (eof()) fail("unterminated array");
      char c = peek();
      if (c == ',') {
        ++pos_;
        continue;
      }
      if (c == ']') {
        ++pos_;
        break;
      }
      fail("expected ',' or ']' in array");
    }
    return v;
  }

  // A param/value: number, array, string literal, or bare IDENT (String).
  ParamValue readValue() {
    skipWs();
    if (eof()) fail("expected a value");
    char c = peek();
    ParamValue pv;
    if (c == '[') {
      pv.kind = ParamValue::Kind::Vector;
      pv.vec = readArray();
    } else if (c == '"') {
      pv.kind = ParamValue::Kind::String;
      pv.str = readString();
    } else if (c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9')) {
      pv.kind = ParamValue::Kind::Number;
      pv.num = readNumber();
    } else if (isIdentStart(c)) {
      pv.kind = ParamValue::Kind::String;
      pv.str = readIdent();
    } else {
      fail("expected a number, array, string or identifier value");
    }
    return pv;
  }

  // node := IDENT '(' args? ')' | IDENT
  GraphNode parseNode() {
    skipWs();
    if (eof() || !isIdentStart(peek()))
      fail("expected an op name");
    std::size_t opPos = pos_;
    GraphNode node;
    node.op = canonicalOpToken(readIdent());
    node.pointer = std::to_string(opPos);

    skipWs();
    if (eof() || peek() != '(')
      return node;  // bare op, no args
    ++pos_;         // consume '('
    skipWs();
    if (!eof() && peek() == ')') {
      ++pos_;
      return node;  // empty arg list
    }

    std::vector<double> positional;  // accumulates bare scalars / arrays
    while (true) {
      parseArg(node, positional);
      skipWs();
      if (eof()) fail("expected ',' or ')'");
      char c = peek();
      if (c == ',') {
        ++pos_;
        continue;
      }
      if (c == ')') {
        ++pos_;
        break;
      }
      fail("expected ',' or ')' between arguments");
    }
    if (!positional.empty()) {
      ParamValue pv;
      pv.kind = ParamValue::Kind::Vector;
      pv.vec = std::move(positional);
      node.params["params"] = std::move(pv);
    }
    return node;
  }

  // One argument: child node, key=value, or a bare positional scalar/array.
  void parseArg(GraphNode& node, std::vector<double>& positional) {
    skipWs();
    if (eof()) fail("expected an argument");
    char c = peek();

    // Positional scalar or array (never a child -- bare IDENTs are children).
    if (c == '[') {
      std::vector<double> a = readArray();
      positional.insert(positional.end(), a.begin(), a.end());
      return;
    }
    if (c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9')) {
      positional.push_back(readNumber());
      return;
    }
    if (!isIdentStart(c)) fail("expected an argument");

    // An IDENT: peek past it to decide key=value vs. a child node.
    std::size_t save = pos_;
    std::string ident = readIdent();
    skipWs();
    if (!eof() && peek() == '=') {
      ++pos_;  // consume '='
      node.params[ident] = readValue();
      return;
    }
    // Not a named param -> it's a child node. Rewind and parse as a node so the
    // child can carry its own '(' args ')'.
    pos_ = save;
    node.in.push_back(parseNode());
  }
};

}  // namespace

GraphNode parseShorthand(const std::string& text) {
  return ShorthandParser(text).parse();
}

// ===========================================================================
// GraphNode -> canonical JSON (inverse of toNode)
// ===========================================================================

namespace {

// Use ordered_json for the dump so insertion order is preserved: each node reads
// op-first then params then in-last (the shape the schema documents), instead of
// nlohmann's default alphabetical key sort which would float `in` above `op`.
using ojson = nlohmann::ordered_json;

ojson paramToJson(const ParamValue& pv) {
  switch (pv.kind) {
    case ParamValue::Kind::Number:
      return ojson(pv.num);
    case ParamValue::Kind::String:
      return ojson(pv.str);
    case ParamValue::Kind::Vector:
      return ojson(pv.vec);
  }
  return ojson(nullptr);
}

ojson nodeToJson(const GraphNode& n) {
  ojson j = ojson::object();
  j["op"] = n.op;
  // params come from a std::map (alphabetical) -- deterministic and fine.
  for (const auto& kv : n.params)
    j[kv.first] = paramToJson(kv.second);
  if (!n.in.empty()) {
    ojson arr = ojson::array();
    for (const GraphNode& child : n.in)
      arr.push_back(nodeToJson(child));
    j["in"] = std::move(arr);
  }
  return j;
}

}  // namespace

std::string dumpJson(const GraphNode& root) {
  ojson doc = ojson::object();
  doc["version"] = 1;
  doc["units"] = "mm";
  doc["root"] = nodeToJson(root);
  return doc.dump(2);
}

// ===========================================================================
// GraphNode -> FieldPtr
// ===========================================================================

namespace {

// ---------------------------------------------------------------------------
// Unknown-parameter rejection. Every key a node carries must be READ by the op
// that consumes it; anything left over is a typo or a key this op does not
// take, and until 2026-09-11 it was absorbed silently -- `plane(normal=[1,0,0])`
// kept the registry default and morphed along Y while the recipe said X, on the
// CPU and the GPU identically, so the parity gate could not see it. findParam is
// the single read point, so the set of read keys is exact by construction and no
// per-op table has to be kept in sync with the dispatcher. Keyed by node
// address, scoped by an RAII guard so a throw does not leak an entry; recursion
// into children is safe because each node has its own entry.
// ---------------------------------------------------------------------------
thread_local std::map<const GraphNode*, std::set<std::string>> g_readKeys;

struct ReadScope {
  const GraphNode& n;
  ~ReadScope() { g_readKeys.erase(&n); }
};

const ParamValue* findParam(const GraphNode& n, const std::string& key) {
  g_readKeys[&n].insert(key);
  auto it = n.params.find(key);
  return it == n.params.end() ? nullptr : &it->second;
}

void rejectUnreadParams(const GraphNode& n) {
  auto it = g_readKeys.find(&n);
  static const std::set<std::string> none;
  const std::set<std::string>& read = it == g_readKeys.end() ? none : it->second;
  for (const auto& kv : n.params) {
    if (read.count(kv.first)) continue;
    std::string accepted;
    for (const std::string& k : read) accepted += (accepted.empty() ? "" : ", ") + k;
    std::string hint = accepted.empty() ? " (this op takes no named parameters)"
                                        : " (accepted: " + accepted + ")";
    if (findPrimitive(n.op))
      hint = " (a primitive takes its values positionally, in registry order --"
             " see --list -- or as params=[...]" +
             (accepted == "params" ? std::string() : "; named keys: " + accepted) +
             ")";
    throw GraphError("unknown parameter '" + kv.first + "' for '" + n.op + "'" + hint,
                     n.pointer);
  }
}

// Scalar accessor: a Number, or a single-element Vector (lenient). Falls back to
// `def` when absent.
double numParam(const GraphNode& n, const std::string& key, double def) {
  const ParamValue* p = findParam(n, key);
  if (!p) return def;
  if (p->kind == ParamValue::Kind::Number) return p->num;
  if (p->kind == ParamValue::Kind::Vector && p->vec.size() == 1)
    return p->vec[0];
  throw GraphError("parameter '" + key + "' must be a number", n.pointer);
}

double requireNum(const GraphNode& n, const std::string& key) {
  if (!findParam(n, key))
    throw GraphError("missing required parameter '" + key + "'", n.pointer);
  return numParam(n, key, 0.0);
}

std::vector<double> vecParam(const GraphNode& n, const std::string& key,
                             std::size_t arity, bool required,
                             const std::vector<double>& def) {
  const ParamValue* p = findParam(n, key);
  if (!p) {
    if (required)
      throw GraphError("missing required parameter '" + key + "'", n.pointer);
    return def;
  }
  if (p->kind != ParamValue::Kind::Vector || p->vec.size() != arity)
    throw GraphError("parameter '" + key + "' must be an array of " +
                         std::to_string(arity) + " numbers",
                     n.pointer);
  return p->vec;
}

dualc::Vector3 vec3Param(const GraphNode& n, const std::string& key,
                         const dualc::Vector3& def) {
  const ParamValue* p = findParam(n, key);
  if (!p) return def;
  std::vector<double> v = vecParam(n, key, 3, false, {def.x, def.y, def.z});
  return dualc::Vector3{v[0], v[1], v[2]};
}

std::string strParam(const GraphNode& n, const std::string& key,
                     const std::string& def) {
  const ParamValue* p = findParam(n, key);
  if (!p) return def;
  if (p->kind != ParamValue::Kind::String)
    throw GraphError("parameter '" + key + "' must be a string", n.pointer);
  return p->str;
}

int axisIndex(const GraphNode& n, const std::string& s) {
  if (s == "x" || s == "0") return 0;
  if (s == "y" || s == "1") return 1;
  if (s == "z" || s == "2") return 2;
  throw GraphError("axis must be \"x\", \"y\" or \"z\"", n.pointer);
}

void requireChildren(const GraphNode& n, std::size_t k) {
  if (n.in.size() != k)
    throw GraphError("'" + n.op + "' expects " + std::to_string(k) +
                         " child node(s), got " + std::to_string(n.in.size()),
                     n.pointer);
}

bool isTpmsOp(const std::string& op) {
  for (const TpmsKind& k : tpmsKinds())
    if (k.name == op) return true;
  return false;
}

bool isStrutOp(const std::string& op) {
  for (const StrutKind& k : strutKinds())
    if (k.name == op) return true;
  return false;
}

// Per-primitive grouped-key layout: which JSON key fills which slice of the
// registry's flat default vector. Only the schema-pinned core/common ops are
// listed; every other primitive (and any of these) also accepts a flat
// `params:[...]` array, which overrides positionally.
struct Slot {
  std::string key;
  std::size_t offset;
  std::size_t arity;
};
const std::map<std::string, std::vector<Slot>>& primitiveLayouts() {
  static const std::map<std::string, std::vector<Slot>> m = {
      {"sphere", {{"center", 0, 3}, {"radius", 3, 1}}},
      {"box", {{"min", 0, 3}, {"max", 3, 3}}},
      {"roundbox", {{"min", 0, 3}, {"max", 3, 3}, {"radius", 6, 1}}},
      {"capsule", {{"a", 0, 3}, {"b", 3, 3}, {"radius", 6, 1}}},
      {"cappedcylinder", {{"a", 0, 3}, {"b", 3, 3}, {"radius", 6, 1}}},
      {"torus", {{"center", 0, 3}, {"major", 3, 1}, {"minor", 4, 1}}},
      {"ellipsoid", {{"center", 0, 3}, {"radii", 3, 3}}},
  };
  return m;
}

std::vector<double> primitiveParams(const GraphNode& n) {
  const PrimEntry* e = findPrimitive(n.op);  // caller already checked non-null
  std::vector<double> p = e->defaults;

  // Grouped semantic keys (for the pinned ops) fill named slices.
  auto layoutIt = primitiveLayouts().find(n.op);
  if (layoutIt != primitiveLayouts().end()) {
    for (const Slot& s : layoutIt->second) {
      const ParamValue* pv = findParam(n, s.key);
      if (!pv) continue;
      std::vector<double> vals =
          (s.arity == 1)
              ? std::vector<double>{numParam(n, s.key, 0.0)}
              : vecParam(n, s.key, s.arity, false, {});
      for (std::size_t i = 0; i < s.arity && s.offset + i < p.size(); ++i)
        p[s.offset + i] = vals[i];
    }
  }

  // Universal flat escape hatch: `params:[...]` overrides positionally.
  const ParamValue* flat = findParam(n, "params");
  if (flat) {
    if (flat->kind != ParamValue::Kind::Vector)
      throw GraphError("'params' must be a number array", n.pointer);
    for (std::size_t i = 0; i < flat->vec.size() && i < p.size(); ++i)
      p[i] = flat->vec[i];
  }
  return p;
}

// Decorator / domain-op nodes that route through the example_common PostOp
// machinery (applyPostOps is the single source of truth for how a PostOp maps to
// a field). `transform` and `normalize` are handled separately (not PostOps).
bool tryMakePostOp(const GraphNode& n, PostOp& out) {
  const std::string& op = n.op;
  if (op == "offset" || op == "round") {
    out = PostOp{op == "offset" ? PostOpKind::Offset : PostOpKind::Round,
                 {requireNum(n, "r")},
                 {}};
    return true;
  }
  if (op == "onion") {
    out = PostOp{PostOpKind::Onion, {requireNum(n, "thickness")}, {}};
    return true;
  }
  if (op == "scale") {
    out = PostOp{PostOpKind::Scale, {numParam(n, "s", 1.0)}, {}};
    return true;
  }
  if (op == "elongate") {
    std::vector<double> h = vecParam(n, "h", 3, true, {});
    out = PostOp{PostOpKind::Elongate, h, {}};
    return true;
  }
  if (op == "translate") {
    std::vector<double> by = vecParam(n, "by", 3, true, {});
    out = PostOp{PostOpKind::Translate, by, {}};
    return true;
  }
  if (op == "rotate") {
    std::vector<double> a = vecParam(n, "axis", 3, true, {});
    a.push_back(requireNum(n, "degrees"));
    out = PostOp{PostOpKind::Rotate, a, {}};
    return true;
  }
  if (op == "mirror") {
    out = PostOp{PostOpKind::Mirror, vecParam(n, "normal", 3, true, {}), {}};
    return true;
  }
  if (op == "repeat") {
    out = PostOp{PostOpKind::Repeat, vecParam(n, "period", 3, true, {}), {}};
    return true;
  }
  if (op == "repeat-limited") {
    std::vector<double> p = vecParam(n, "period", 3, true, {});
    std::vector<double> c = vecParam(n, "count", 3, true, {});
    p.insert(p.end(), c.begin(), c.end());
    out = PostOp{PostOpKind::RepeatLimited, p, {}};
    return true;
  }
  if (op == "twist") {
    double rate = requireNum(n, "radiansPerUnit");
    int axis = axisIndex(n, strParam(n, "axis", "x"));
    out = PostOp{PostOpKind::Twist, {rate, static_cast<double>(axis)}, {}};
    return true;
  }
  if (op == "bend") {
    double curv = requireNum(n, "curvature");
    int axis = axisIndex(n, strParam(n, "axis", "x"));
    out = PostOp{PostOpKind::Bend, {curv, static_cast<double>(axis)}, {}};
    return true;
  }
  if (op == "displace") {
    std::string fn = strParam(n, "fn", "sine");
    double amp = numParam(n, "amplitude", 0.1);
    double freq = numParam(n, "frequency", 6.0);
    out = PostOp{PostOpKind::Displace,
                 {},
                 fn + "," + std::to_string(amp) + "," + std::to_string(freq)};
    return true;
  }
  return false;
}

dualc::Mat4 mat4From16(const std::vector<double>& v) {
  dualc::Mat4 m{};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) m.m[i][j] = v[static_cast<std::size_t>(i) * 4 + j];
  return m;
}

dualc::SignMethod meshSignMethod(const GraphNode& n) {
  std::string sign = strParam(n, "sign", "parity");
  if (sign == "parity") return dualc::SignMethod::WINDING_NUMBER;
  if (sign == "pseudonormal") return dualc::SignMethod::PSEUDONORMAL;
  if (sign == "gwn")
    throw GraphError("mesh sign 'gwn' is not supported by the 'mesh' node; use "
                     "a 'winding' node for generalized-winding-number soups",
                     n.pointer);
  throw GraphError("mesh sign must be 'parity' or 'pseudonormal'", n.pointer);
}

dualc::FieldPtr buildFieldUnchecked(const GraphNode& n, MeshResolver& meshes);

dualc::FieldPtr buildField(const GraphNode& n, MeshResolver& meshes) {
  ReadScope scope{n};
  try {
    dualc::FieldPtr f = buildFieldUnchecked(n, meshes);
    rejectUnreadParams(n);
    return f;
  } catch (const std::invalid_argument& e) {
    // The library validates its own arguments and throws std::invalid_argument
    // (a negative radius, a zero wavelength, a sheared transform, ...). Give
    // that message the offending node's locator, the same as every parse error,
    // so the user is pointed at the node rather than at a bare what().
    // GraphError derives from std::runtime_error, so a child's already-located
    // error passes straight through this handler instead of being re-wrapped.
    throw GraphError(e.what(), n.pointer);
  }
}

dualc::FieldPtr buildFieldUnchecked(const GraphNode& n, MeshResolver& meshes) {
  const std::string& op = n.op;

  // 1. Booleans (two children).
  BoolOp bop;
  if (parseBoolOp(op, bop)) {
    requireChildren(n, 2);
    double k = numParam(n, "k", 0.25);
    return applyBoolOp(bop, buildField(n.in[0], meshes),
                       buildField(n.in[1], meshes), k);
  }

  // 2. TPMS sources.
  if (isTpmsOp(op))
    return makeTpmsField(op, vec3Param(n, "center", dualc::Vector3{0, 0, 0}),
                         numParam(n, "wavelength", 1.0));

  // 2b. Strut-lattice sources (wireframe crystals). Like TPMS, each crystal is
  // its own op token (sc/bcc/fcc/octet) reading named params; bounds() is
  // infinite, so a clip (intersection with a mesh/box) or --bounds is required.
  if (isStrutOp(op)) {
    const double radius = numParam(n, "radius", 0.1);
    return makeStrutLattice(op, vec3Param(n, "center", dualc::Vector3{0, 0, 0}),
                            numParam(n, "wavelength", 1.0), radius,
                            numParam(n, "nodeRadius", radius));
  }

  // 3. Mesh sources. Each takes exactly one of `path` (load from disk) or `id`
  // (a host buffer registered with an InMemoryMeshResolver -- the C ABI / GH path;
  // an `id` is an ABI-only source, it has no meaning on the dualc_field CLI).
  if (op == "mesh") {
    std::string path = strParam(n, "path", "");
    std::string id = strParam(n, "id", "");
    if (path.empty() == id.empty())
      throw GraphError("'mesh' requires exactly one of 'path' or 'id'",
                       n.pointer);
    // Validate sign/normals before touching the resolver, so a bad mode is
    // reported independently of whether the source exists.
    std::string normals = strParam(n, "normals", "smooth");
    bool interp;
    if (normals == "smooth") interp = true;
    else if (normals == "sharp") interp = false;
    else throw GraphError("mesh normals must be 'smooth' or 'sharp'", n.pointer);
    dualc::SignMethod sm = meshSignMethod(n);
    MeshHandle h = id.empty() ? meshes.resolve(path) : meshes.resolveId(id);
    return std::make_shared<dualc::MeshSource>(*h.mesh, *h.geom, interp, sm);
  }
  if (op == "winding") {
    std::string path = strParam(n, "path", "");
    std::string id = strParam(n, "id", "");
    if (path.empty() == id.empty())
      throw GraphError("'winding' requires exactly one of 'path' or 'id'",
                       n.pointer);
    MeshHandle h = id.empty() ? meshes.resolve(path) : meshes.resolveId(id);
    return dualc::windingNumberField(*h.mesh, *h.geom);
  }

  // 4. Decorators not covered by the PostOp machinery (one child).
  if (op == "normalize") {
    requireChildren(n, 1);
    return dualc::normalizedOf(buildField(n.in[0], meshes));
  }
  if (op == "transform") {
    requireChildren(n, 1);
    return dualc::transformed(buildField(n.in[0], meshes),
                              mat4From16(vecParam(n, "matrix", 16, true, {})));
  }
  // graded-onion: a two-child decorator (base, control) -- not a single-child
  // PostOp, so it is wired explicitly like normalize/transform above.
  if (op == "graded-onion") {
    requireChildren(n, 2);
    return dualc::gradedOnionOf(buildField(n.in[0], meshes),
                               buildField(n.in[1], meshes), requireNum(n, "t1"),
                               requireNum(n, "t2"), numParam(n, "d0", 0.0),
                               requireNum(n, "d1"));
  }
  // graded-offset: same two-child (base, control) shape as graded-onion, but
  // inflates the solid (base - t) instead of hollowing it (|base| - t).
  if (op == "graded-offset") {
    requireChildren(n, 2);
    return dualc::gradedOffsetOf(buildField(n.in[0], meshes),
                                buildField(n.in[1], meshes), requireNum(n, "t1"),
                                requireNum(n, "t2"), numParam(n, "d0", 0.0),
                                requireNum(n, "d1"));
  }
  // mix: a three-child morph (A, B, control) -- value lerp(A, B, w) with the
  // control ramp w = clamp((control-lo)/(hi-lo)). lo defaults to 0 (like
  // graded's d0); hi is required (like d1).
  if (op == "mix") {
    requireChildren(n, 3);
    return dualc::mixOf(buildField(n.in[0], meshes), buildField(n.in[1], meshes),
                        buildField(n.in[2], meshes), numParam(n, "lo", 0.0),
                        requireNum(n, "hi"));
  }

  // 5. Decorators / domain ops via the shared PostOp registry (one child).
  PostOp pop;
  if (tryMakePostOp(n, pop)) {
    requireChildren(n, 1);
    return applyPostOps(buildField(n.in[0], meshes), {pop});
  }

  // 6. Analytic primitives (leaf source).
  if (findPrimitive(op))
    return buildPrimitive(op, primitiveParams(n));

  // 7. Unknown.
  throw GraphError("unknown op '" + op + "'", n.pointer);
}

}  // namespace

FieldGraph FieldGraph::build(const GraphNode& root, MeshResolver& meshes) {
  return FieldGraph(buildField(root, meshes));
}

}  // namespace fieldgraph
}  // namespace dce
