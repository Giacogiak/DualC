// dualc_c.cpp -- implementation of the DualC C ABI (proxy + export).
//
// A thin shim: it owns nothing novel, only wires the existing host-side
// field-graph parser (examples/field_graph) + export writers
// (examples/example_common) + the libdualc contour pipeline behind a flat C
// surface. No C++ exception is allowed to cross any entry point.

#include "dualc_c.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "dualc/pipeline.h"
#include "dualc/progress.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include "example_common.h"
#include "field_graph.h"

#ifndef DUALC_C_VERSION_STR
#define DUALC_C_VERSION_STR "unknown"
#endif

// The opaque handle: the resolver MUST outlive the FieldGraph (a MeshSource holds
// bare refs into meshes the resolver owns). Member order matters -- `resolver` is
// declared first so it is destroyed LAST, after the graph that references it. The
// InMemoryMeshResolver handles both `path=` (disk) and `id=` (host buffer)
// sources; with no buffers registered it behaves exactly like a FileMeshResolver.
struct DualcField {
  dce::fieldgraph::InMemoryMeshResolver resolver;
  std::optional<dce::fieldgraph::FieldGraph> graph;
};

// The cancel token is the engine's, by containment: request() is one relaxed
// atomic store, which is what makes it legal from any host thread.
struct DualcCancelToken {
  dualc::CancelToken token;
};

static_assert(static_cast<int>(dualc::Stage::Sample) == DUALC_STAGE_SAMPLE, "");
static_assert(static_cast<int>(dualc::Stage::Contour) == DUALC_STAGE_CONTOUR, "");
static_assert(static_cast<int>(dualc::Stage::Write) == DUALC_STAGE_WRITE, "");
static_assert(static_cast<int>(dualc::Stage::Tile) == DUALC_STAGE_TILE, "");

namespace {

// Adapts the host callback to the engine's sink. Lives on the stack of the
// ABI call, so it is invoked on that call's thread only -- the engine never
// reports from a worker (dualc/progress.h). Counts saturate at UINT32_MAX.
struct CProgressSink final : dualc::ProgressSink {
  DualcProgressFn fn;
  void* user;
  CProgressSink(DualcProgressFn f, void* u) : fn(f), user(u) {}
  static std::uint32_t sat32(std::size_t v) {
    return v > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<std::uint32_t>(v);
  }
  void report(dualc::Stage s, std::size_t done,
              std::size_t total) noexcept override {
    fn(user, static_cast<int>(s), sat32(done), sat32(total));
  }
};

const dualc::CancelToken* engineToken(const DualcCancelToken* t) {
  return t ? &t->token : nullptr;
}

void setErr(char* err, int errlen, const std::string& msg) {
  if (!err || errlen <= 0) return;
  int n = static_cast<int>(msg.size());
  if (n > errlen - 1) n = errlen - 1;
  std::memcpy(err, msg.data(), static_cast<std::size_t>(n));
  err[n] = '\0';
}

void clearErr(char* err, int errlen) {
  if (err && errlen > 0) err[0] = '\0';
}

void applyParams(const DualcContourParams& p, dualc::SamplerParams& sp,
                 dualc::ContourerParams& cp) {
  sp.maxDepth = p.maxDepth;
  sp.minDepth = p.minDepth;
  sp.numThreads = p.numThreads;
  if (p.hasBounds) {
    dualc::BBox b;
    b.min = {p.boundsMin[0], p.boundsMin[1], p.boundsMin[2]};
    b.max = {p.boundsMax[0], p.boundsMax[1], p.boundsMax[2]};
    sp.rootBounds = b;
  }
  cp.simplificationError = p.collapse;
  cp.manifoldDC = (p.manifold != 0);
}

// Flatten the C++ diagnostics into the ABI struct. Booleans become 0/1 ints
// and counts become uint64 -- the C side must not depend on sizeof(bool) or
// on size_t width.
void fillDiag(const dualc::Diagnostics& d, DualcDiagnostics* out) {
  if (!out) return;
  out->inputEmpty             = d.inputEmpty ? 1 : 0;
  out->inputBoundaryEdges     = static_cast<std::uint64_t>(d.inputBoundaryEdges);
  out->inputNonManifoldEdges  = static_cast<std::uint64_t>(d.inputNonManifoldEdges);
  out->inputWatertight        = d.inputWatertight ? 1 : 0;
  out->boundsFallback         = d.boundsFallback ? 1 : 0;
  out->gridBoundsExceeded     = d.gridBoundsExceeded ? 1 : 0;
  out->emptyContour           = d.emptyContour ? 1 : 0;
  out->outputVertices         = static_cast<std::uint64_t>(d.outputVertices);
  out->outputTriangles        = static_cast<std::uint64_t>(d.outputTriangles);
  out->outputBoundaryEdges    = static_cast<std::uint64_t>(d.outputBoundaryEdges);
  out->outputNonManifoldEdges = static_cast<std::uint64_t>(d.outputNonManifoldEdges);
  out->outputWatertight       = d.outputWatertight ? 1 : 0;
  out->anyIssue               = d.anyIssue() ? 1 : 0;
}

int createImpl(bool shorthand, const char* text, const DualcMeshSource* meshes,
               int meshCount, DualcField** out, char* err, int errlen) {
  if (out) *out = nullptr;
  if (!text || !out) {
    setErr(err, errlen, "null argument");
    return DUALC_ERR_USAGE;
  }
  if (meshCount < 0 || (meshCount > 0 && !meshes)) {
    setErr(err, errlen, "invalid mesh source array");
    return DUALC_ERR_USAGE;
  }
  std::unique_ptr<DualcField> h(new DualcField());
  // Register host buffers first so the graph's `id=` sources resolve during
  // build. A malformed buffer is a USAGE error (caller's data), distinct from a
  // graph that references an unknown id (a GRAPH error, thrown during build).
  for (int i = 0; i < meshCount; ++i) {
    const DualcMeshSource& m = meshes[i];
    try {
      h->resolver.registerMesh(m.id ? m.id : "", m.vertices, m.vertexCount,
                               m.indices, m.triangleCount);
    } catch (const dce::fieldgraph::GraphError& e) {
      setErr(err, errlen, e.what());
      return DUALC_ERR_USAGE;
    } catch (const std::exception& e) {
      setErr(err, errlen, e.what());
      return DUALC_ERR_USAGE;
    } catch (...) {
      setErr(err, errlen, "unknown error registering mesh source");
      return DUALC_ERR_USAGE;
    }
  }
  try {
    dce::fieldgraph::GraphNode root =
        shorthand ? dce::fieldgraph::parseShorthand(text)
                  : dce::fieldgraph::parseJson(text);
    h->graph = dce::fieldgraph::FieldGraph::build(root, h->resolver);
  } catch (const dce::fieldgraph::GraphError& e) {
    std::string m = e.what();
    if (!e.pointer().empty()) m += " (at " + e.pointer() + ")";
    setErr(err, errlen, m);
    return DUALC_ERR_GRAPH;
  } catch (const std::exception& e) {
    setErr(err, errlen, e.what());
    return DUALC_ERR_UNKNOWN;
  } catch (...) {
    setErr(err, errlen, "unknown error building field-graph");
    return DUALC_ERR_UNKNOWN;
  }
  *out = h.release();
  clearErr(err, errlen);
  return DUALC_OK;
}

}  // namespace

extern "C" {

const char* dualc_version(void) { return "dualc " DUALC_C_VERSION_STR; }

void dualc_default_params(DualcContourParams* out) {
  if (!out) return;
  const dualc::SamplerParams sp;
  const dualc::ContourerParams cp;
  *out = DualcContourParams{};
  out->maxDepth = sp.maxDepth;
  out->minDepth = sp.minDepth;
  out->collapse = cp.simplificationError;
  out->hasBounds = 0;
  out->manifold = cp.manifoldDC ? 1 : 0;
  out->numThreads = sp.numThreads;
}

int dualc_field_create_from_json(const char* json, DualcField** out, char* err,
                                 int errlen) {
  return createImpl(/*shorthand=*/false, json, nullptr, 0, out, err, errlen);
}

int dualc_field_create_from_expr(const char* expr, DualcField** out, char* err,
                                 int errlen) {
  return createImpl(/*shorthand=*/true, expr, nullptr, 0, out, err, errlen);
}

int dualc_field_create_from_json_with_meshes(const char* json,
                                             const DualcMeshSource* meshes,
                                             int meshCount, DualcField** out,
                                             char* err, int errlen) {
  return createImpl(/*shorthand=*/false, json, meshes, meshCount, out, err,
                    errlen);
}

int dualc_field_create_from_expr_with_meshes(const char* expr,
                                             const DualcMeshSource* meshes,
                                             int meshCount, DualcField** out,
                                             char* err, int errlen) {
  return createImpl(/*shorthand=*/true, expr, meshes, meshCount, out, err,
                    errlen);
}

void dualc_field_destroy(DualcField* field) { delete field; }

int dualc_field_contour(DualcField* field, const DualcContourParams* params,
                        DualcMesh* out, char* err, int errlen) {
  return dualc_field_contour_with_diagnostics(field, params, out, nullptr, err,
                                              errlen);
}

int dualc_field_contour_with_diagnostics(DualcField* field,
                                         const DualcContourParams* params,
                                         DualcMesh* out, DualcDiagnostics* diag,
                                         char* err, int errlen) {
  return dualc_field_contour_with_progress(field, params, out, diag, nullptr,
                                           nullptr, nullptr, err, errlen);
}

int dualc_field_contour_with_progress(DualcField* field,
                                      const DualcContourParams* params,
                                      DualcMesh* out, DualcDiagnostics* diag,
                                      const DualcCancelToken* cancel,
                                      DualcProgressFn progress, void* user,
                                      char* err, int errlen) {
  if (out) *out = DualcMesh{};
  if (diag) *diag = DualcDiagnostics{};
  if (!field || !field->graph || !params || !out) {
    setErr(err, errlen, "null argument");
    return DUALC_ERR_USAGE;
  }
  try {
    dualc::SamplerParams sp;
    dualc::ContourerParams cp;
    applyParams(*params, sp, cp);

    std::optional<CProgressSink> sink;
    if (progress) sink.emplace(progress, user);

    dualc::Diagnostics d;
    auto [mesh, geom, normals] = dualc::dualContourField(
        field->graph->field(), sp, cp, diag ? &d : nullptr,
        engineToken(cancel), sink ? &*sink : nullptr);
    fillDiag(d, diag);

    const std::uint32_t nv = static_cast<std::uint32_t>(mesh->nVertices());
    std::unique_ptr<float[]> pos(new float[static_cast<std::size_t>(nv) * 3]);
    std::unique_ptr<float[]> nor(new float[static_cast<std::size_t>(nv) * 3]);
    for (auto v : mesh->vertices()) {
      const std::size_t i = v.getIndex();
      const dualc::Vector3& p = geom->inputVertexPositions[v];
      pos[i * 3 + 0] = static_cast<float>(p.x);
      pos[i * 3 + 1] = static_cast<float>(p.y);
      pos[i * 3 + 2] = static_cast<float>(p.z);
      const dualc::Vector3 n =
          (i < normals.size()) ? normals[i] : dualc::Vector3{0.0, 0.0, 1.0};
      nor[i * 3 + 0] = static_cast<float>(n.x);
      nor[i * 3 + 1] = static_cast<float>(n.y);
      nor[i * 3 + 2] = static_cast<float>(n.z);
    }

    // Dual contouring may emit polygonal faces -- fan-triangulate, exactly as
    // example_common.cpp's toTriMesh does, so the flat mesh matches the file
    // writers face-for-face.
    std::vector<std::uint32_t> tris;
    for (const std::vector<std::size_t>& f : mesh->getFaceVertexList()) {
      if (f.size() < 3) continue;  // degenerate
      for (std::size_t i = 1; i + 1 < f.size(); ++i) {
        tris.push_back(static_cast<std::uint32_t>(f[0]));
        tris.push_back(static_cast<std::uint32_t>(f[i]));
        tris.push_back(static_cast<std::uint32_t>(f[i + 1]));
      }
    }
    std::unique_ptr<std::uint32_t[]> idx(new std::uint32_t[tris.size()]);
    if (!tris.empty())
      std::memcpy(idx.get(), tris.data(), tris.size() * sizeof(std::uint32_t));

    out->positions = pos.release();
    out->normals = nor.release();
    out->indices = idx.release();
    out->vertexCount = nv;
    out->triangleCount = static_cast<std::uint32_t>(tris.size() / 3);
    clearErr(err, errlen);
    return DUALC_OK;
  } catch (const dualc::Cancelled& e) {
    // Caught before std::exception, which it derives from.
    if (out) *out = DualcMesh{};
    setErr(err, errlen, e.what());
    return DUALC_CANCELLED;
  } catch (const std::invalid_argument& e) {
    // The sampler throws this for an unbounded field with no explicit bounds.
    setErr(err, errlen,
           std::string("unbounded field: set hasBounds + boundsMin/Max. ") +
               e.what());
    return DUALC_ERR_BOUNDS;
  } catch (const std::exception& e) {
    setErr(err, errlen, e.what());
    return DUALC_ERR_UNKNOWN;
  } catch (...) {
    setErr(err, errlen, "unknown error during contour");
    return DUALC_ERR_UNKNOWN;
  }
}

void dualc_mesh_release(DualcMesh* mesh) {
  if (!mesh) return;
  delete[] mesh->positions;
  delete[] mesh->normals;
  delete[] mesh->indices;
  *mesh = DualcMesh{};
}

int dualc_field_export(DualcField* field, const char* path,
                       const DualcContourParams* params, char* err, int errlen) {
  return dualc_field_export_with_diagnostics(field, path, params, nullptr, err,
                                             errlen);
}

int dualc_field_export_with_diagnostics(DualcField* field, const char* path,
                                        const DualcContourParams* params,
                                        DualcDiagnostics* diag, char* err,
                                        int errlen) {
  return dualc_field_export_with_progress(field, path, params, diag, nullptr,
                                          nullptr, nullptr, err, errlen);
}

int dualc_field_export_with_progress(DualcField* field, const char* path,
                                     const DualcContourParams* params,
                                     DualcDiagnostics* diag,
                                     const DualcCancelToken* cancel,
                                     DualcProgressFn progress, void* user,
                                     char* err, int errlen) {
  if (diag) *diag = DualcDiagnostics{};
  if (!field || !field->graph || !path || !params) {
    setErr(err, errlen, "null argument");
    return DUALC_ERR_USAGE;
  }
  try {
    dualc::SamplerParams sp;
    dualc::ContourerParams cp;
    applyParams(*params, sp, cp);
    std::optional<CProgressSink> sink;
    if (progress) sink.emplace(progress, user);
    dualc::Diagnostics d;
    const int rc = dce::writeField(field->graph->field(), path, sp, cp,
                                   dce::DecimateOpts{}, diag ? &d : nullptr,
                                   engineToken(cancel), sink ? &*sink : nullptr);
    fillDiag(d, diag);
    if (rc == 1) {
      setErr(err, errlen, "unbounded field: set hasBounds + boundsMin/Max");
      return DUALC_ERR_BOUNDS;
    }
    if (rc == 3) {
      setErr(err, errlen, "cancelled: nothing written");
      return DUALC_CANCELLED;
    }
    if (rc != 0) {
      // The writer's own line (OS error text included) when it left one.
      const std::string& why = dce::lastError();
      setErr(err, errlen, why.empty()
                              ? std::string("export failed (unknown extension or writer error)")
                              : "export failed: " + why);
      return DUALC_ERR_IO;
    }
    clearErr(err, errlen);
    return DUALC_OK;
  } catch (const dualc::Cancelled& e) {
    setErr(err, errlen, e.what());
    return DUALC_CANCELLED;
  } catch (const std::exception& e) {
    setErr(err, errlen, e.what());
    return DUALC_ERR_UNKNOWN;
  } catch (...) {
    setErr(err, errlen, "unknown error during export");
    return DUALC_ERR_UNKNOWN;
  }
}

int dualc_field_export_tiled_stl(DualcField* field, const char* path,
                                 const DualcContourParams* params,
                                 int tileDepth, char* err, int errlen) {
  return dualc_field_export_tiled_stl_with_progress(
      field, path, params, tileDepth, nullptr, nullptr, nullptr, err, errlen);
}

int dualc_field_export_tiled_stl_with_progress(
    DualcField* field, const char* path, const DualcContourParams* params,
    int tileDepth, const DualcCancelToken* cancel, DualcProgressFn progress,
    void* user, char* err, int errlen) {
  if (!field || !field->graph || !path || !params) {
    setErr(err, errlen, "null argument");
    return DUALC_ERR_USAGE;
  }
  try {
    dualc::SamplerParams sp;
    dualc::ContourerParams cp;
    applyParams(*params, sp, cp);
    std::optional<CProgressSink> sink;
    if (progress) sink.emplace(progress, user);
    const int rc = dce::writeFieldTiledStl(field->graph->field(), path, sp, cp,
                                           tileDepth, engineToken(cancel),
                                           sink ? &*sink : nullptr);
    if (rc == 1) {
      setErr(err, errlen, "unbounded field: set hasBounds + boundsMin/Max");
      return DUALC_ERR_BOUNDS;
    }
    if (rc == 3) {
      setErr(err, errlen, "cancelled: nothing written");
      return DUALC_CANCELLED;
    }
    if (rc != 0) {
      const std::string& why = dce::lastError();
      setErr(err, errlen, why.empty()
                              ? std::string("tiled STL export failed (writer or validation error)")
                              : "tiled STL export failed: " + why);
      return DUALC_ERR_IO;
    }
    clearErr(err, errlen);
    return DUALC_OK;
  } catch (const dualc::Cancelled& e) {
    setErr(err, errlen, e.what());
    return DUALC_CANCELLED;
  } catch (const std::exception& e) {
    setErr(err, errlen, e.what());
    return DUALC_ERR_UNKNOWN;
  } catch (...) {
    setErr(err, errlen, "unknown error during tiled export");
    return DUALC_ERR_UNKNOWN;
  }
}

DualcCancelToken* dualc_cancel_token_create(void) {
  return new (std::nothrow) DualcCancelToken{};
}

void dualc_cancel_token_request(DualcCancelToken* token) {
  if (token) token->token.request();
}

int dualc_cancel_token_is_requested(const DualcCancelToken* token) {
  return (token && token->token.requested()) ? 1 : 0;
}

void dualc_cancel_token_destroy(DualcCancelToken* token) { delete token; }

}  // extern "C"
