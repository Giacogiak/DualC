/* dualc_c_demo.c -- the DualC C ABI smoke test, COMPILED AS C.
 *
 * Its real job is twofold: (1) prove the public header is C-clean (no C++
 * leaked in -- this file is built by the C compiler), and (2) exercise the full
 * proxy+export path end to end. It builds a small TPMS lattice clipped to a box
 * from an `--expr` string, contours it at a coarse depth (the proxy lever),
 * checks the mesh is non-empty, exports a binary STL, checks the file is
 * non-trivial, then releases everything. Returns 0 on success (the CTest gate).
 *
 * Modes (argv[2]):
 *   (default) analytic : TPMS + boolean, no mesh source.
 *   "mesh"             : a mesh(path="cube.obj") source -- drives the disk path.
 *   "inmem"            : the in-memory mesh ABID -- loads cube.obj into buffers,
 *                        builds one field via mesh(id="cube") + the *_with_meshes
 *                        create call and a reference field via mesh(path=...), and
 *                        asserts the two contour to a BYTE-IDENTICAL mesh (the
 *                        in-memory == temp-file acceptance gate). cube.obj coords
 *                        are +/-0.5, exactly representable as float and double, so
 *                        the float buffers and the OBJ-parsed doubles agree.
 *   "cancel"           : the 0.5.0 cancel token + progress callback -- a
 *                        pre-requested token cancels a contour before any work;
 *                        a callback that requests the token mid-way cancels a
 *                        tiled export and leaves no file, not even a `.part`;
 *                        the all-NULL *_with_progress twins are byte-identical
 *                        to the older calls; a live callback sees every stage
 *                        end at done == total. Cancelling from inside the
 *                        callback is legal (it runs on the calling thread) and
 *                        deterministic, so no threads are needed. */

#include "dualc_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DUALC_DEMO_EXPR_ANALYTIC                       \
  "intersection(onion(gyroid(wavelength=0.5),thickness=0.12)," \
  "box(min=[-1,-1,-1],max=[1,1,1]))"
#define DUALC_DEMO_EXPR_MESH                              \
  "intersection(onion(gyroid(wavelength=0.5),thickness=0.1)," \
  "mesh(path=\"cube.obj\"))"
#define DUALC_DEMO_EXPR_MESH_ID                           \
  "intersection(onion(gyroid(wavelength=0.5),thickness=0.1)," \
  "mesh(id=\"cube\"))"

/* Minimal Wavefront-OBJ reader: pulls vertex positions and (fan-triangulated)
 * face indices into flat buffers. Handles `f a b c`, quads, and `a/b/c` tokens.
 * Returns 1 on success; the caller frees *outV / *outI. */
static int loadObj(const char* path, float** outV, uint32_t* outVC,
                   uint32_t** outI, uint32_t* outTC) {
  FILE* f = fopen(path, "r");
  float* v = NULL;
  uint32_t* idx = NULL;
  uint32_t vc = 0, vcap = 0, ic = 0, icap = 0;
  char line[512];
  if (!f) return 0;
  while (fgets(line, (int)sizeof(line), f)) {
    if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t')) {
      float x, y, z;
      if (sscanf(line + 1, "%f %f %f", &x, &y, &z) == 3) {
        if ((vc + 1) * 3 > vcap) {
          vcap = vcap ? vcap * 2 : 48;
          v = (float*)realloc(v, vcap * sizeof(float));
        }
        v[vc * 3 + 0] = x;
        v[vc * 3 + 1] = y;
        v[vc * 3 + 2] = z;
        vc++;
      }
    } else if (line[0] == 'f' && (line[1] == ' ' || line[1] == '\t')) {
      unsigned fi[8];
      int n = 0, t;
      char* p = line + 1;
      while (n < 8) {
        unsigned val = 0;
        int got = 0;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '\r') break;
        while (*p >= '0' && *p <= '9') { val = val * 10 + (unsigned)(*p - '0'); p++; got = 1; }
        if (!got) break;
        fi[n++] = val; /* OBJ is 1-based */
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
      }
      for (t = 1; t + 1 < n; t++) {
        if ((ic + 1) * 3 > icap) {
          icap = icap ? icap * 2 : 72;
          idx = (uint32_t*)realloc(idx, icap * sizeof(uint32_t));
        }
        idx[ic * 3 + 0] = fi[0] - 1;
        idx[ic * 3 + 1] = fi[t] - 1;
        idx[ic * 3 + 2] = fi[t + 1] - 1;
        ic++;
      }
    }
  }
  fclose(f);
  *outV = v;
  *outVC = vc;
  *outI = idx;
  *outTC = ic;
  return (vc > 0 && ic > 0);
}

/* Assert a create call fails with DUALC_ERR_GRAPH (and leaves *out NULL). Used
 * for the negative id-resolution cases. Returns 0 on the expected failure. */
static int expectGraphErr(const char* label, const char* expr,
                          const DualcMeshSource* meshes, int meshCount) {
  char err[512];
  DualcField* f = NULL;
  int rc = dualc_field_create_from_expr_with_meshes(expr, meshes, meshCount, &f,
                                                    err, (int)sizeof(err));
  if (rc == DUALC_OK || f != NULL) {
    fprintf(stderr, "negative case '%s' unexpectedly succeeded\n", label);
    if (f) dualc_field_destroy(f);
    return 1;
  }
  if (rc != DUALC_ERR_GRAPH) {
    fprintf(stderr, "negative case '%s' rc=%d (expected DUALC_ERR_GRAPH=%d)\n",
            label, rc, DUALC_ERR_GRAPH);
    return 1;
  }
  printf("negative '%s' -> rc=%d: %s\n", label, rc, err);
  return 0;
}

/* Contour the in-memory and the disk forms of the same mesh and assert the
 * results are bit-identical. Returns 0 on success. */
static int runInmemParity(const char* outPath) {
  float* verts = NULL;
  uint32_t* idxs = NULL;
  uint32_t vc = 0, tc = 0;
  char err[512];
  DualcMeshSource src;
  DualcField* idField = NULL;
  DualcField* pathField = NULL;
  DualcContourParams params;
  DualcMesh a, b;
  int rc, ok = 1;
  uint32_t i;
  long sz = 0;
  FILE* f;

  if (!loadObj("cube.obj", &verts, &vc, &idxs, &tc)) {
    fprintf(stderr, "could not load cube.obj into buffers\n");
    return 1;
  }
  printf("loaded cube.obj -> %u verts, %u tris (in memory)\n", vc, tc);

  src.id = "cube";
  src.vertices = verts;
  src.vertexCount = vc;
  src.indices = idxs;
  src.triangleCount = tc;
  src.normals = NULL;

  rc = dualc_field_create_from_expr_with_meshes(DUALC_DEMO_EXPR_MESH_ID, &src, 1,
                                                &idField, err, (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "in-memory create failed (rc=%d): %s\n", rc, err);
    free(verts);
    free(idxs);
    return 1;
  }

  /* Negative cases: an unregistered id, and both path+id on one node. Both are
   * the friendly graph errors a host (a hash typo / a missing registration) will
   * hit -- verify they actually fire before we free the buffers. */
  if (expectGraphErr(
          "unknown id",
          "intersection(onion(gyroid(wavelength=0.5),thickness=0.1),"
          "mesh(id=\"nope\"))",
          &src, 1) != 0 ||
      expectGraphErr(
          "path+id together",
          "intersection(onion(gyroid(wavelength=0.5),thickness=0.1),"
          "mesh(path=\"cube.obj\",id=\"cube\"))",
          &src, 1) != 0) {
    dualc_field_destroy(idField);
    free(verts);
    free(idxs);
    return 1;
  }

  /* The arrays are copied during the create call -- safe to free now. */
  free(verts);
  free(idxs);
  rc = dualc_field_create_from_expr(DUALC_DEMO_EXPR_MESH, &pathField, err,
                                    (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "reference (path) create failed (rc=%d): %s\n", rc, err);
    dualc_field_destroy(idField);
    return 1;
  }

  dualc_default_params(&params);
  params.maxDepth = 6;

  rc = dualc_field_contour(idField, &params, &a, err, (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "in-memory contour failed (rc=%d): %s\n", rc, err);
    dualc_field_destroy(idField);
    dualc_field_destroy(pathField);
    return 1;
  }
  rc = dualc_field_contour(pathField, &params, &b, err, (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "reference contour failed (rc=%d): %s\n", rc, err);
    dualc_mesh_release(&a);
    dualc_field_destroy(idField);
    dualc_field_destroy(pathField);
    return 1;
  }

  printf("in-memory: %u verts, %u tris;  path: %u verts, %u tris\n",
         a.vertexCount, a.triangleCount, b.vertexCount, b.triangleCount);
  if (a.vertexCount == 0 || a.triangleCount == 0) {
    fprintf(stderr, "in-memory contour produced an empty mesh\n");
    ok = 0;
  }
  if (a.vertexCount != b.vertexCount || a.triangleCount != b.triangleCount) {
    fprintf(stderr, "MISMATCH: counts differ (in-memory vs path)\n");
    ok = 0;
  }
  for (i = 0; ok && i < a.vertexCount * 3; ++i) {
    if (a.positions[i] != b.positions[i]) {
      fprintf(stderr, "MISMATCH: position[%u] %g != %g\n", i,
              (double)a.positions[i], (double)b.positions[i]);
      ok = 0;
    }
  }
  for (i = 0; ok && i < a.triangleCount * 3; ++i) {
    if (a.indices[i] != b.indices[i]) {
      fprintf(stderr, "MISMATCH: index[%u] %u != %u\n", i, a.indices[i],
              b.indices[i]);
      ok = 0;
    }
  }
  dualc_mesh_release(&a);
  dualc_mesh_release(&b);
  dualc_field_destroy(pathField);
  if (!ok) {
    dualc_field_destroy(idField);
    return 1;
  }
  printf("in-memory == path: BYTE-IDENTICAL\n");

  /* Also exercise export through the in-memory field. */
  rc = dualc_field_export(idField, outPath, &params, err, (int)sizeof(err));
  dualc_field_destroy(idField);
  if (rc != DUALC_OK) {
    fprintf(stderr, "in-memory export failed (rc=%d): %s\n", rc, err);
    return 1;
  }
  f = fopen(outPath, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fclose(f);
  }
  if (sz < 84) {
    fprintf(stderr, "exported file too small (%ld bytes)\n", sz);
    return 1;
  }
  printf("exported %s (%ld bytes)\n", outPath, sz);
  return 0;
}

/* --- the "cancel" mode ----------------------------------------------------- */

typedef struct {
  DualcCancelToken* token;   /* requested when `cancelOn` fires, if non-NULL */
  int cancelStage;           /* stage to cancel on */
  uint32_t cancelAtDone;     /* ... once done >= this */
  uint32_t calls;
  uint32_t lastDone[4], lastTotal[4], seen[4];
  int monotonic;
} ProgressLog;

static void onProgress(void* user, int stage, uint32_t done, uint32_t total) {
  ProgressLog* log = (ProgressLog*)user;
  log->calls++;
  if (stage < 0 || stage > 3) { log->monotonic = 0; return; }
  if (log->seen[stage] && (done < log->lastDone[stage] ||
                           total != log->lastTotal[stage]))
    log->monotonic = 0;
  log->seen[stage] = 1;
  log->lastDone[stage] = done;
  log->lastTotal[stage] = total;
  if (log->token && stage == log->cancelStage && done >= log->cancelAtDone)
    dualc_cancel_token_request(log->token);
}

static int fileExists(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return 0;
  fclose(f);
  return 1;
}

static long fileSize(const char* path) {
  long sz = -1;
  FILE* f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END);
  sz = ftell(f);
  fclose(f);
  return sz;
}

static int filesIdentical(const char* a, const char* b) {
  FILE* fa = fopen(a, "rb");
  FILE* fb = fopen(b, "rb");
  int same = 1;
  if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
  for (;;) {
    int ca = fgetc(fa), cb = fgetc(fb);
    if (ca != cb) { same = 0; break; }
    if (ca == EOF) break;
  }
  fclose(fa);
  fclose(fb);
  return same;
}

static int runCancel(const char* outPath) {
  char err[512];
  char partPath[600];
  char plainPath[600];
  int rc;
  DualcField* field = NULL;
  DualcContourParams params;
  DualcMesh mesh;
  DualcCancelToken* token;
  ProgressLog log;

  snprintf(partPath, sizeof(partPath), "%s.part", outPath);
  snprintf(plainPath, sizeof(plainPath), "%s.plain.stl", outPath);
  remove(outPath);
  remove(partPath);
  remove(plainPath);

  rc = dualc_field_create_from_expr(DUALC_DEMO_EXPR_ANALYTIC, &field, err,
                                    (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "create failed (rc=%d): %s\n", rc, err);
    return 1;
  }
  dualc_default_params(&params);
  params.maxDepth = 6;

  /* (a) a pre-requested token cancels a contour before any work. */
  token = dualc_cancel_token_create();
  if (!token || dualc_cancel_token_is_requested(token)) {
    fprintf(stderr, "token create / initial state wrong\n");
    dualc_field_destroy(field);
    return 1;
  }
  dualc_cancel_token_request(token);
  if (!dualc_cancel_token_is_requested(token)) {
    fprintf(stderr, "token request not observed\n");
    dualc_field_destroy(field);
    return 1;
  }
  mesh.positions = (float*)1; /* must be zeroed by the call */
  rc = dualc_field_contour_with_progress(field, &params, &mesh, NULL, token,
                                         NULL, NULL, err, (int)sizeof(err));
  if (rc != DUALC_CANCELLED || mesh.positions != NULL || mesh.vertexCount != 0 ||
      strstr(err, "cancel") == NULL) {
    fprintf(stderr, "(a) pre-requested contour: rc=%d err='%s'\n", rc, err);
    dualc_cancel_token_destroy(token);
    dualc_field_destroy(field);
    return 1;
  }
  printf("(a) pre-requested token -> DUALC_CANCELLED: %s\n", err);
  dualc_cancel_token_destroy(token);

  /* (b) a callback that requests the token at the second tile cancels a
     tiled export and leaves nothing -- not even the .part. */
  token = dualc_cancel_token_create();
  memset(&log, 0, sizeof(log));
  log.token = token;
  log.cancelStage = DUALC_STAGE_TILE;
  log.cancelAtDone = 1;
  log.monotonic = 1;
  rc = dualc_field_export_tiled_stl_with_progress(field, outPath, &params, 4,
                                                  token, onProgress, &log, err,
                                                  (int)sizeof(err));
  if (rc != DUALC_CANCELLED || fileExists(outPath) || fileExists(partPath) ||
      !log.seen[DUALC_STAGE_TILE] || strstr(err, "cancel") == NULL) {
    fprintf(stderr, "(b) tiled cancel: rc=%d err='%s' out=%d part=%d tile=%u\n",
            rc, err, fileExists(outPath), fileExists(partPath),
            log.seen[DUALC_STAGE_TILE]);
    dualc_cancel_token_destroy(token);
    dualc_field_destroy(field);
    return 1;
  }
  printf("(b) tiled export cancelled at tile %u/%u -> DUALC_CANCELLED, no file, "
         "no .part\n", log.lastDone[DUALC_STAGE_TILE],
         log.lastTotal[DUALC_STAGE_TILE]);
  dualc_cancel_token_destroy(token);

  /* (c) the all-NULL twin is byte-identical to the older export. */
  rc = dualc_field_export(field, plainPath, &params, err, (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "(c) plain export failed (rc=%d): %s\n", rc, err);
    dualc_field_destroy(field);
    return 1;
  }
  rc = dualc_field_export_with_progress(field, outPath, &params, NULL, NULL,
                                        NULL, NULL, err, (int)sizeof(err));
  if (rc != DUALC_OK || !filesIdentical(outPath, plainPath) ||
      fileExists(partPath) || fileSize(outPath) < 84) {
    fprintf(stderr, "(c) with_progress(NULL...) differs from export (rc=%d)\n",
            rc);
    dualc_field_destroy(field);
    return 1;
  }
  printf("(c) export == export_with_progress(NULL, NULL, NULL): BYTE-IDENTICAL "
         "(%ld bytes)\n", fileSize(outPath));
  remove(plainPath);

  /* (d) a live callback with an unrequested token: same mesh as the plain
     contour, every stage seen ends at done == total, all on this thread. */
  {
    DualcMesh plain;
    unsigned int i;
    rc = dualc_field_contour(field, &params, &plain, err, (int)sizeof(err));
    if (rc != DUALC_OK) {
      fprintf(stderr, "(d) plain contour failed (rc=%d): %s\n", rc, err);
      dualc_field_destroy(field);
      return 1;
    }
    token = dualc_cancel_token_create();
    memset(&log, 0, sizeof(log));
    log.monotonic = 1; /* log.token stays NULL: never requested */
    rc = dualc_field_contour_with_progress(field, &params, &mesh, NULL, token,
                                           onProgress, &log, err,
                                           (int)sizeof(err));
    if (rc != DUALC_OK || dualc_cancel_token_is_requested(token) ||
        mesh.vertexCount != plain.vertexCount ||
        mesh.triangleCount != plain.triangleCount || !log.monotonic ||
        !log.seen[DUALC_STAGE_SAMPLE] || !log.seen[DUALC_STAGE_CONTOUR] ||
        log.seen[DUALC_STAGE_WRITE] || log.seen[DUALC_STAGE_TILE] ||
        log.lastDone[DUALC_STAGE_SAMPLE] != log.lastTotal[DUALC_STAGE_SAMPLE] ||
        log.lastDone[DUALC_STAGE_CONTOUR] != log.lastTotal[DUALC_STAGE_CONTOUR]) {
      fprintf(stderr, "(d) live callback: rc=%d monotonic=%d sample=%u/%u "
              "contour=%u/%u\n", rc, log.monotonic,
              log.lastDone[DUALC_STAGE_SAMPLE], log.lastTotal[DUALC_STAGE_SAMPLE],
              log.lastDone[DUALC_STAGE_CONTOUR],
              log.lastTotal[DUALC_STAGE_CONTOUR]);
      dualc_mesh_release(&plain);
      dualc_mesh_release(&mesh);
      dualc_cancel_token_destroy(token);
      dualc_field_destroy(field);
      return 1;
    }
    for (i = 0; i < mesh.vertexCount * 3; ++i) {
      if (mesh.positions[i] != plain.positions[i] ||
          mesh.normals[i] != plain.normals[i]) {
        fprintf(stderr, "(d) MISMATCH at %u\n", i);
        dualc_mesh_release(&plain);
        dualc_mesh_release(&mesh);
        dualc_cancel_token_destroy(token);
        dualc_field_destroy(field);
        return 1;
      }
    }
    printf("(d) live callback: %u reports, sample %u/%u, contour %u/%u, mesh "
           "BYTE-IDENTICAL\n", log.calls, log.lastDone[DUALC_STAGE_SAMPLE],
           log.lastTotal[DUALC_STAGE_SAMPLE], log.lastDone[DUALC_STAGE_CONTOUR],
           log.lastTotal[DUALC_STAGE_CONTOUR]);
    dualc_mesh_release(&plain);
    dualc_mesh_release(&mesh);
    dualc_cancel_token_destroy(token);
  }

  /* NULL is a no-op everywhere on the token. */
  dualc_cancel_token_request(NULL);
  dualc_cancel_token_destroy(NULL);
  if (dualc_cancel_token_is_requested(NULL) != 0) {
    fprintf(stderr, "is_requested(NULL) != 0\n");
    dualc_field_destroy(field);
    return 1;
  }

  remove(outPath);
  dualc_field_destroy(field);
  printf("OK\n");
  return 0;
}

int main(int argc, char** argv) {
  const char* outPath = (argc > 1) ? argv[1] : "c_abi_demo.stl";
  const char* mode = (argc > 2) ? argv[2] : "";
  const int meshMode = (mode[0] == 'm');  /* "mesh" */
  const int inmemMode = (mode[0] == 'i'); /* "inmem" */
  const int cancelMode = (mode[0] == 'c'); /* "cancel" */
  const char* expr = meshMode ? DUALC_DEMO_EXPR_MESH : DUALC_DEMO_EXPR_ANALYTIC;
  char err[512];
  int rc;
  DualcField* field = NULL;
  DualcContourParams params;
  DualcMesh mesh;
  FILE* f;
  long sz = 0;

  printf("dualc C ABI demo -- %s\n", dualc_version());

  if (inmemMode) return runInmemParity(outPath);
  if (cancelMode) return runCancel(outPath);

  rc = dualc_field_create_from_expr(expr, &field, err, (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "create failed (rc=%d): %s\n", rc, err);
    return 1;
  }

  dualc_default_params(&params);
  params.maxDepth = 6; /* coarse proxy resolution */

  rc = dualc_field_contour(field, &params, &mesh, err, (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "contour failed (rc=%d): %s\n", rc, err);
    dualc_field_destroy(field);
    return 1;
  }
  printf("contoured: %u verts, %u tris\n", mesh.vertexCount, mesh.triangleCount);
  if (mesh.vertexCount == 0 || mesh.triangleCount == 0) {
    fprintf(stderr, "contour produced an empty mesh\n");
    dualc_mesh_release(&mesh);
    dualc_field_destroy(field);
    return 1;
  }
  /* The *_with_diagnostics twin must return the SAME mesh, plus the report.
     dualc_field_contour above is a forwarder that passes NULL, so this also
     pins that the forwarder did not change what a 0.3.0 host sees. */
  {
    DualcMesh mesh2;
    DualcDiagnostics diag;
    unsigned int i;
    memset(&diag, 0, sizeof(diag));
    rc = dualc_field_contour_with_diagnostics(field, &params, &mesh2, &diag,
                                             err, (int)sizeof(err));
    if (rc != DUALC_OK) {
      fprintf(stderr, "diagnostics contour failed (rc=%d): %s\n", rc, err);
      dualc_mesh_release(&mesh);
      dualc_field_destroy(field);
      return 1;
    }
    printf("diagnostics: %llu verts, %llu tris, watertight=%d, "
           "emptyContour=%d, anyIssue=%d\n",
           (unsigned long long)diag.outputVertices,
           (unsigned long long)diag.outputTriangles,
           diag.outputWatertight, diag.emptyContour, diag.anyIssue);
    if (mesh2.vertexCount != mesh.vertexCount ||
        mesh2.triangleCount != mesh.triangleCount) {
      fprintf(stderr, "MISMATCH: diagnostics contour changed the mesh size\n");
      dualc_mesh_release(&mesh2);
      dualc_mesh_release(&mesh);
      dualc_field_destroy(field);
      return 1;
    }
    for (i = 0; i < mesh.vertexCount * 3; ++i) {
      if (mesh2.positions[i] != mesh.positions[i]) {
        fprintf(stderr, "MISMATCH: diagnostics position[%u] differs\n", i);
        dualc_mesh_release(&mesh2);
        dualc_mesh_release(&mesh);
        dualc_field_destroy(field);
        return 1;
      }
    }
    /* The demo field is a closed analytic solid inside its bounds, so the
       report must be clean and must agree with the mesh it describes. */
    if (diag.anyIssue || !diag.outputWatertight || diag.emptyContour ||
        diag.outputTriangles != (unsigned long long)mesh.triangleCount ||
        diag.outputVertices != (unsigned long long)mesh.vertexCount) {
      fprintf(stderr, "MISMATCH: diagnostics disagree with the mesh\n");
      dualc_mesh_release(&mesh2);
      dualc_mesh_release(&mesh);
      dualc_field_destroy(field);
      return 1;
    }
    printf("contour == contour_with_diagnostics: BYTE-IDENTICAL\n");
    dualc_mesh_release(&mesh2);
  }

  dualc_mesh_release(&mesh);

  rc = dualc_field_export(field, outPath, &params, err, (int)sizeof(err));
  if (rc != DUALC_OK) {
    fprintf(stderr, "export failed (rc=%d): %s\n", rc, err);
    dualc_field_destroy(field);
    return 1;
  }

  /* A binary STL is at least an 80-byte header + a 4-byte count (84). */
  f = fopen(outPath, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fclose(f);
  }
  if (sz < 84) {
    fprintf(stderr, "exported file too small (%ld bytes)\n", sz);
    dualc_field_destroy(field);
    return 1;
  }
  printf("exported %s (%ld bytes)\n", outPath, sz);

  dualc_field_destroy(field);
  printf("OK\n");
  return 0;
}
