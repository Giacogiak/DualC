/* dualc_c.h -- the DualC C ABI (re-scoped: proxy + export).
 *
 * A flat, C-callable boundary over the host-side field-graph pipeline so a
 * plugin (Rhino/Grasshopper), a thin standalone shell, or any non-C++ host can:
 *   1. build an implicit field from a field-graph string (canonical JSON or the
 *      terse `--expr` shorthand -- the SAME grammar dualc_field accepts), then
 *   2. contour it to a flat triangle mesh in memory (the *proxy* a host draws --
 *      pass a coarse `maxDepth` for a cheap one), and/or
 *   3. export it to a file (STL / 3MF / OBJ, format chosen by the extension).
 *
 * The graph string IS the construction API -- there is intentionally no
 * per-primitive factory surface here. Compose with the field-graph vocabulary
 * (booleans, TPMS, decorators, mesh/winding sources, ...) exactly as on the
 * dualc_field command line.
 *
 * This boundary lives in the HOST layer (it links the field-graph parser, which
 * vendors nlohmann/json, and the export writers, which vendor miniz) -- by the
 * library-scope rule those never enter libdualc, so the ABI is a separate
 * shared library, not part of include/dualc/.
 *
 * Conventions:
 *   - Every entry point returns an int status (DUALC_OK == 0 on success); no C++
 *     exception ever crosses this boundary.
 *   - On a non-zero status the `err`/`errlen` buffer (when provided) receives a
 *     NUL-terminated human-readable message; graph errors also carry a locator
 *     (a JSON pointer for JSON input, a character offset for `--expr`).
 *   - A DualcMesh produced by dualc_field_contour is owned by the ABI; release
 *     it with dualc_mesh_release. A DualcField is released with
 *     dualc_field_destroy.
 *   - Silent degradation is reportable: the *_with_diagnostics twins of the
 *     contour and export calls fill a DualcDiagnostics. Added in 0.4.0 as new
 *     entry points, not changed signatures, so a host built against 0.3.0
 *     keeps working unchanged (same pattern as the *_with_meshes twins).
 *   - A long call is cancellable and reports progress: the *_with_progress
 *     twins (0.5.0) take a host-owned DualcCancelToken -- request it from any
 *     thread and the call returns DUALC_CANCELLED at its next checkpoint,
 *     leaving no file behind -- and a DualcProgressFn invoked on the calling
 *     thread only. Same additive pattern: every older call is a forwarder.
 *   - Export writes `path + ".part"` and renames it over `path` only on
 *     success, so `path` is either the complete file or untouched.
 *
 * In-memory mesh sources: a host with geometry already in RAM (Rhino/Grasshopper)
 * can skip the disk round-trip by passing buffers to the *_with_meshes create
 * calls and referencing them by id from the graph (`mesh(id="...")` /
 * `winding(id="...")`). The `mesh(path="...")` disk form keeps working unchanged.
 *
 * Deferred (not in this ABI version): 3MF-streaming / RAM-budget export behind the
 * export calls; cancellation inside the monolithic writers.
 */
#ifndef DUALC_C_H
#define DUALC_C_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(DUALC_CAPI_BUILD)
#    define DUALC_CAPI_EXPORT __declspec(dllexport)
#  else
#    define DUALC_CAPI_EXPORT __declspec(dllimport)
#  endif
#else
#  if defined(DUALC_CAPI_BUILD)
#    define DUALC_CAPI_EXPORT __attribute__((visibility("default")))
#  else
#    define DUALC_CAPI_EXPORT
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Status codes. */
#define DUALC_OK           0  /* success                                        */
#define DUALC_ERR_BOUNDS   1  /* unbounded field: pass bounds (hasBounds + min/max) */
#define DUALC_ERR_IO       2  /* unknown extension or a writer/file failure     */
#define DUALC_ERR_GRAPH    3  /* field-graph parse/build error (see err + locator) */
#define DUALC_ERR_USAGE    4  /* bad argument to an ABI call (e.g. NULL handle)  */
#define DUALC_ERR_UNKNOWN  5  /* any other failure                              */
#define DUALC_CANCELLED    6  /* the DualcCancelToken was requested; nothing written */

/* Progress stages, the `stage` argument of DualcProgressFn (mirror
 * dualc::Stage). The monolithic calls report SAMPLE, CONTOUR, then WRITE as
 * (0,1)/(1,1); the tiled call reports TILE only. */
#define DUALC_STAGE_SAMPLE  0
#define DUALC_STAGE_CONTOUR 1
#define DUALC_STAGE_WRITE   2
#define DUALC_STAGE_TILE    3

/* Opaque field handle. Owns the built field-graph and the resolver backing any
 * mesh sources it references (the resolver must outlive the field). */
typedef struct DualcField DualcField;

/* Opaque cooperative-cancellation token (0.5.0). Host-owned and independent of
 * any DualcField: create one PER JOB (it cannot be reset), pass it to a
 * *_with_progress call, and request it from ANY thread -- typically a UI
 * thread while the call runs on a worker -- to make that call return
 * DUALC_CANCELLED at its next checkpoint (sub-second on any input: the engine
 * polls between every few octree cells, every leaf solve and every tile).
 * dualc_cancel_token_request is the only function on a token that may run
 * concurrently with a call using it; the token must outlive every such call
 * (destroying it while one is in flight is a use-after-free). */
typedef struct DualcCancelToken DualcCancelToken;

/* Coarse progress callback (0.5.0). Invoked ONLY on the thread that made the
 * ABI call -- never from an engine worker -- at stage boundaries and at most
 * a few times per second in between; `done` is monotonic within a stage,
 * `total` constant, and the last report of a stage is (total, total). It must
 * not throw or unwind; it may call dualc_cancel_token_request on the job's
 * token. `user` is the pointer the call received. */
typedef void (*DualcProgressFn)(void* user, int stage, uint32_t done,
                                uint32_t total);

/* Flat, C-friendly sampling + contour settings (the dualc_field CLI flags).
 * Zero-initialise then set what you need; helper dualc_default_params() fills
 * the library defaults. */
typedef struct {
  int      maxDepth;       /* octree max depth -- the proxy lever (low = cheap/coarse) */
  int      minDepth;       /* octree min depth                                */
  double   collapse;       /* adaptive-collapse QEF energy, length^2 (0=off)  */
  int      hasBounds;      /* 1 to use boundsMin/boundsMax; 0 = auto-fit      */
  double   boundsMin[3];   /* root box lower corner (when hasBounds)          */
  double   boundsMax[3];   /* root box upper corner (when hasBounds)          */
  int      manifold;       /* 1 = manifold dual contouring (default), 0 = off */
  unsigned numThreads;     /* 0 = all hardware threads                        */
} DualcContourParams;

/* An ABI-owned flat triangle mesh. Faces are fan-triangulated. Release with
 * dualc_mesh_release (which also zeroes the struct). */
typedef struct {
  float*   positions;      /* 3 * vertexCount   (x,y,z per vertex)            */
  float*   normals;        /* 3 * vertexCount   (unit, index-aligned)         */
  uint32_t* indices;       /* 3 * triangleCount (0-based into positions)      */
  uint32_t vertexCount;
  uint32_t triangleCount;
} DualcMesh;

/* A host-provided in-memory triangle mesh, referenced from a field-graph by its
 * `id` (a `mesh(id="...")` / `winding(id="...")` source). The arrays are READ and
 * COPIED during the create call only -- they need not outlive the returned
 * DualcField. Geometry convention matches the file path: model units = mm, CCW
 * winding = outward.
 *   - id            : unique key the graph references (must be non-empty)
 *   - vertices      : 3 * vertexCount floats (x,y,z per vertex)
 *   - indices       : 3 * triangleCount uint32 (0-based into vertices)
 *   - normals       : optional, 3 * vertexCount; CURRENTLY IGNORED (the engine
 *                     derives normals from geometry via the node's smooth/sharp
 *                     option) -- present for forward compatibility, pass NULL. */
typedef struct {
  const char*     id;
  const float*    vertices;
  uint32_t        vertexCount;
  const uint32_t* indices;
  uint32_t        triangleCount;
  const float*    normals;
} DualcMeshSource;

/* What the engine degraded silently, reported instead of guessed. Zero-
 * initialise before the call; every field is written by the *_with_diagnostics
 * entry points below. Mirrors dualc::Diagnostics (include/dualc/types.h),
 * which carries the full per-field documentation.
 *
 * `anyIssue` is 1 when at least one of emptyContour / boundsFallback /
 * gridBoundsExceeded / inputEmpty fired, or the output mesh is not closed --
 * the single flag a host can surface to a user. Booleans are int (0/1);
 * counts are uint64 so they cannot wrap on a dense part.
 *
 * NOT reported: the per-probe clamp inside a baked grid nested below the root
 * of the graph. Counting it would need mutable state on a field evaluated
 * concurrently, which the engine's thread-safety contract forbids;
 * gridBoundsExceeded covers the root-level case. */
typedef struct {
  int      inputEmpty;              /* input mesh had no triangles          */
  uint64_t inputBoundaryEdges;      /* input edges with 1 incident triangle */
  uint64_t inputNonManifoldEdges;   /* input edges with 3 or more           */
  int      inputWatertight;         /* both input counts are zero           */
  int      boundsFallback;          /* bounds() unusable -> unit cube used  */
  int      gridBoundsExceeded;      /* sampled outside a root baked grid    */
  int      emptyContour;            /* no surface: result is a placeholder  */
  uint64_t outputVertices;
  uint64_t outputTriangles;
  uint64_t outputBoundaryEdges;
  uint64_t outputNonManifoldEdges;
  int      outputWatertight;        /* both output counts are zero          */
  int      anyIssue;                /* any degradation above fired          */
} DualcDiagnostics;

/* Library / ABI version string, e.g. "dualc 0.5.0". Never NULL. */
DUALC_CAPI_EXPORT const char* dualc_version(void);

/* Fill `out` with the library default sampling/contour settings (maxDepth 7,
 * minDepth 3, manifold on, auto bounds). Safe to call with out == NULL (no-op). */
DUALC_CAPI_EXPORT void dualc_default_params(DualcContourParams* out);

/* Build a field from a canonical-JSON field-graph string. On success writes the
 * handle to *out and returns DUALC_OK; otherwise *out is set NULL and a message
 * (+ JSON-pointer locator) is written to err. */
DUALC_CAPI_EXPORT int dualc_field_create_from_json(const char* json,
                                                   DualcField** out,
                                                   char* err, int errlen);

/* Build a field from the terse `--expr` shorthand string (same result tree as
 * the JSON form). On error the locator is a best-effort character offset. */
DUALC_CAPI_EXPORT int dualc_field_create_from_expr(const char* expr,
                                                   DualcField** out,
                                                   char* err, int errlen);

/* Like dualc_field_create_from_json, but also registers `meshCount` host-provided
 * in-memory meshes that the graph may reference by id (`mesh(id="...")` /
 * `winding(id="...")`). `path=` sources still load from disk. The mesh arrays are
 * copied during this call; they need not outlive *out. Pass meshes==NULL/
 * meshCount==0 for the no-mesh case (identical to dualc_field_create_from_json).
 * Returns DUALC_ERR_USAGE for a malformed buffer, DUALC_ERR_GRAPH for a graph
 * that references an unknown id. */
DUALC_CAPI_EXPORT int dualc_field_create_from_json_with_meshes(
    const char* json, const DualcMeshSource* meshes, int meshCount,
    DualcField** out, char* err, int errlen);

/* The `--expr` twin of dualc_field_create_from_json_with_meshes. */
DUALC_CAPI_EXPORT int dualc_field_create_from_expr_with_meshes(
    const char* expr, const DualcMeshSource* meshes, int meshCount,
    DualcField** out, char* err, int errlen);

/* Release a field handle. Safe on NULL. */
DUALC_CAPI_EXPORT void dualc_field_destroy(DualcField* field);

/* Contour `field` with `params` into the ABI-owned flat mesh `*out`. Pass a
 * coarse maxDepth for a cheap drawable proxy, the full depth for export-grade
 * geometry. Returns DUALC_ERR_BOUNDS for an unbounded field with hasBounds==0
 * (set hasBounds + boundsMin/Max). Release the result with dualc_mesh_release. */
DUALC_CAPI_EXPORT int dualc_field_contour(DualcField* field,
                                          const DualcContourParams* params,
                                          DualcMesh* out, char* err, int errlen);

/* dualc_field_contour, plus a filled-in DualcDiagnostics. `diag` may be NULL,
 * in which case this is exactly dualc_field_contour -- which is itself kept as
 * a forwarder passing NULL, so existing callers are unchanged in source and in
 * binary. Same twin pattern as the *_with_meshes create calls.
 *
 * Read `diag` even on DUALC_OK: a field that crosses no surface in the sampled
 * region returns OK with a one-triangle placeholder mesh, and emptyContour is
 * the only way to tell that from a genuine one-triangle result. */
DUALC_CAPI_EXPORT int dualc_field_contour_with_diagnostics(
    DualcField* field, const DualcContourParams* params, DualcMesh* out,
    DualcDiagnostics* diag, char* err, int errlen);

/* dualc_field_contour_with_diagnostics, plus a cancel token and a progress
 * callback -- each nullable (`diag`, `cancel`, `progress`; `user` is passed
 * to `progress` verbatim). With all of them NULL it is exactly
 * dualc_field_contour, which is itself kept as a forwarder. On DUALC_CANCELLED
 * *out is zeroed and err reads "cancelled (sampling|contouring|collapsing)". */
DUALC_CAPI_EXPORT int dualc_field_contour_with_progress(
    DualcField* field, const DualcContourParams* params, DualcMesh* out,
    DualcDiagnostics* diag, const DualcCancelToken* cancel,
    DualcProgressFn progress, void* user, char* err, int errlen);

/* Free a mesh produced by dualc_field_contour and zero the struct. Safe on NULL. */
DUALC_CAPI_EXPORT void dualc_mesh_release(DualcMesh* mesh);

/* Contour `field` with `params` and write it to `path`, with the format chosen
 * by the extension: ".obj" (per-vertex normals), ".stl" (binary), ".3mf"
 * (3MF-mesh, 1 unit = 1 mm). Returns DUALC_OK / DUALC_ERR_BOUNDS / DUALC_ERR_IO. */
DUALC_CAPI_EXPORT int dualc_field_export(DualcField* field, const char* path,
                                         const DualcContourParams* params,
                                         char* err, int errlen);

/* dualc_field_export, plus a filled-in DualcDiagnostics describing the mesh
 * that was written. `diag` may be NULL. The output counts describe the
 * contoured mesh, which is what this call writes (the export path applies no
 * decimation). */
DUALC_CAPI_EXPORT int dualc_field_export_with_diagnostics(
    DualcField* field, const char* path, const DualcContourParams* params,
    DualcDiagnostics* diag, char* err, int errlen);

/* dualc_field_export_with_diagnostics, plus a cancel token and a progress
 * callback (each nullable). Reports DUALC_STAGE_SAMPLE, _CONTOUR, then _WRITE
 * as (0,1)/(1,1). On DUALC_CANCELLED no file exists at `path`: the writer
 * works on `path + ".part"` and renames it only on success, and a file that
 * was already at `path` is left as it was. Cancellation is honoured up to the
 * end of the contour; the write itself is not interrupted. */
DUALC_CAPI_EXPORT int dualc_field_export_with_progress(
    DualcField* field, const char* path, const DualcContourParams* params,
    DualcDiagnostics* diag, const DualcCancelToken* cancel,
    DualcProgressFn progress, void* user, char* err, int errlen);

/* Streaming / tiled binary-STL export (bounded RAM for dense parts): contours in
 * uniform grid-aligned cubic tiles of `tileDepth` octree levels each. `path`
 * should end in ".stl". `tileDepth` <= params->maxDepth (>= collapses to a
 * single streamed pass). Returns DUALC_OK / DUALC_ERR_BOUNDS / DUALC_ERR_IO. */
DUALC_CAPI_EXPORT int dualc_field_export_tiled_stl(DualcField* field,
                                                   const char* path,
                                                   const DualcContourParams* params,
                                                   int tileDepth,
                                                   char* err, int errlen);

/* dualc_field_export_tiled_stl, plus a cancel token and a progress callback
 * (each nullable). Reports DUALC_STAGE_TILE only: (0, T) before the loop,
 * (i, T) as tile i starts, (T, T) after the rename. Cancellation is polled at
 * every tile and inside every tile's contour, so on DUALC_CANCELLED `err`
 * names whichever noticed -- "cancelled (tiling)" at a tile boundary,
 * "cancelled (sampling)" / "(contouring)" inside a tile; do not match on the
 * stage word. Same no-partial-file guarantee as
 * dualc_field_export_with_progress. (No `diag`: the tiled writer never
 * tallies one.) */
DUALC_CAPI_EXPORT int dualc_field_export_tiled_stl_with_progress(
    DualcField* field, const char* path, const DualcContourParams* params,
    int tileDepth, const DualcCancelToken* cancel, DualcProgressFn progress,
    void* user, char* err, int errlen);

/* The cancel token (0.5.0). create never returns NULL except on allocation
 * failure; request is thread-safe and sticky; is_requested returns 0/1;
 * every function is a no-op on NULL. See the DualcCancelToken typedef for the
 * lifetime rule. */
DUALC_CAPI_EXPORT DualcCancelToken* dualc_cancel_token_create(void);
DUALC_CAPI_EXPORT void dualc_cancel_token_request(DualcCancelToken* token);
DUALC_CAPI_EXPORT int dualc_cancel_token_is_requested(const DualcCancelToken* token);
DUALC_CAPI_EXPORT void dualc_cancel_token_destroy(DualcCancelToken* token);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* DUALC_C_H */
