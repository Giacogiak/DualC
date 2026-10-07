# DualC C ABI (`dualc_capi`)

A flat, C-callable boundary over the DualC field-graph pipeline — the bridge a
plugin (Rhino/Grasshopper), a thin standalone shell, or any non-C++ host uses to
**build a field from a graph string, contour a proxy mesh, and export STL/3MF/OBJ**.
This is roadmap item [#19](../docs/roadmap/10-infrastructure-and-integration.md)
(re-scoped to *proxy + export*).

> **Full record & API reference:** [`../docs/roadmap/14-c-abi/`](../docs/roadmap/14-c-abi/README.md).
> **Building the C# wrapper over these entry points?** Start with the hand-over
> spec: [`CSHARP_WRAPPER_HANDOFF.md`](CSHARP_WRAPPER_HANDOFF.md).

## Why it lives here (not in `libdualc`)

The ABI builds a field from a JSON/`--expr` string (via `examples/field_graph`,
which vendors nlohmann/json) and writes files (via `examples/example_common`,
which vendors miniz). Both are **host-side** code, deliberately outside
`libdualc` by the library-scope rule. So the ABI is a separate **shared library**
that *links* those static libs + `libdualc` — it must never drag host-only deps
into the core. The **graph string is the construction API**, so the surface is
just **20 entry points** (9 core, two `*_with_meshes` create twins for hosts with
geometry already in RAM, two `*_with_diagnostics` twins that also report what
the engine degraded, and — since 0.5.0 — three `*_with_progress` twins plus the
four functions of the cancel token), not a per-primitive factory wall.

## Build

Opt-in (off by default; requires the examples layer):

```bash
cmake -S . -B build -DDUALC_BUILD_C_ABI=ON -DDUALC_BUILD_EXAMPLES=ON -DDUALC_BUILD_TESTS=ON
cmake --build build --config Release --target dualc_capi dualc_c_demo
ctest --test-dir build -C Release -R cli_c_abi --output-on-failure
```

Produces `dualc_capi.dll` (+ import lib) and the C demo `dualc_c_demo`.

## Usage

The graph string is the same vocabulary `dualc_field` accepts (JSON or the terse
`--expr` shorthand). Minimal flow (see [`dualc_c_demo.c`](dualc_c_demo.c) for the
full, error-checked version):

```c
#include "dualc_c.h"

DualcField* field = NULL;
char err[512];
dualc_field_create_from_expr(
    "intersection(onion(gyroid(wavelength=0.5),thickness=0.12),"
    "box(min=[-1,-1,-1],max=[1,1,1]))",
    &field, err, sizeof(err));

DualcContourParams p;
dualc_default_params(&p);
p.maxDepth = 6;                 /* coarse = a cheap drawable PROXY */

DualcMesh mesh;
dualc_field_contour(field, &p, &mesh, err, sizeof(err));   /* flat verts/normals/indices */
/* ... draw mesh.positions / mesh.indices ... */
dualc_mesh_release(&mesh);

p.maxDepth = 8;                 /* full = export-grade */
dualc_field_export(field, "part.stl", &p, err, sizeof(err));   /* .obj/.stl/.3mf by extension */

dualc_field_destroy(field);
```

### In-memory mesh sources (diskless)

A host with geometry already in RAM passes it as `DualcMeshSource` buffers and
references each by id with a `mesh(id="…")` / `winding(id="…")` node — no temp file:

```c
DualcMeshSource src = { "cube", verts, vertexCount, idx, triCount, NULL };
dualc_field_create_from_expr_with_meshes(
    "intersection(onion(gyroid(wavelength=0.5),thickness=0.1),mesh(id=\"cube\"))",
    &src, 1, &field, err, sizeof(err));
```

The buffers are copied during the call (need not outlive `field`); the optional
`normals` is ignored. An `id=` mesh contours byte-identical to the same geometry
loaded via `mesh(path=…)` (see `cli_c_abi_mesh_inmem`). `path=` still works.

## Contract

- **Return codes** (`int`): `DUALC_OK` (0), `DUALC_ERR_BOUNDS` (1, unbounded field
  — set `hasBounds` + `boundsMin/Max`), `DUALC_ERR_IO` (2 — since 0.5.1 `err` carries
  the writer's own line, e.g. `export failed: cannot open '<p>.part' for writing: <OS text>`
  or `tiled STL export failed: cannot move '<p>.part' to '<p>': <OS text>`), `DUALC_ERR_GRAPH` (3,
  parse/build — `err` carries the message + a JSON-pointer / char-offset locator),
  `DUALC_ERR_USAGE` (4), `DUALC_ERR_UNKNOWN` (5), `DUALC_CANCELLED` (6, the
  token was requested — nothing written, `*out` zeroed). **No C++ exception ever
  crosses the boundary.**
- **Ownership.** A `DualcMesh` from `dualc_field_contour` is ABI-allocated — free
  it with `dualc_mesh_release` (alloc + free both inside the DLL, no cross-heap
  hazard). A `DualcField` is freed with `dualc_field_destroy`.
- **Diagnostics (0.4.0).** `dualc_field_contour_with_diagnostics` and
  `dualc_field_export_with_diagnostics` fill a flat `DualcDiagnostics`: an empty
  contour, a bounds fallback, sampling past a baked grid, the input-mesh edge
  tally and the output vertex/triangle/edge counts, plus a single `anyIssue`
  flag. **Read it even on `DUALC_OK`** — a field that crosses no surface returns
  OK with a one-triangle placeholder mesh, and `emptyContour` is the only way to
  tell that from real geometry. These are *new* entry points, not changed
  signatures: the three original calls still exist as forwarders passing `NULL`,
  so a host built against 0.3.0 is unaffected in source and in binary. Details:
  [roadmap 17 #26](../docs/roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md).
- **Proxy vs export** is just the `maxDepth` you pass to `contour`/`export` — there
  is no separate proxy call. A coarse-depth proxy of a *lattice* is inherently
  lossy (under-resolved walls drop out); use `dualc_field_view` for true live
  lattice preview.
- **Export never leaves a partial file.** Every export writes `path + ".part"` and
  renames it over `path` only on success; on any non-`OK` return `path` is either
  absent or the file that was already there. A host process killed mid-export
  leaves a stray `.part` to delete, never a truncated `path`.
- **Output handles never leave the process (0.5.1).** Every file the export opens
  is non-inheritable, so a child the host starts with handle inheritance while an
  export runs — .NET's `Process.Start` with any stream redirected does — cannot
  hold the `.part` and make the rename fail (the cause of Boletus's flaky Windows
  gate, [roadmap 17 #52](../docs/roadmap/17-code-audit-and-hardening/16-windows-rename-race/README.md)).
  On Windows a short foreign hold on the `.part` is retried for up to ~0.5 s; a
  longer one returns `DUALC_ERR_IO` with the OS text in `err`, and the `.part`
  may be left behind for the holder's lifetime.

## Cancellation & progress (0.5.0)

Three `*_with_progress` twins — of `contour`, `export` and `export_tiled_stl` —
take a host-owned **cancel token** and a **progress callback**, each nullable; with
both `NULL` they are byte-identical to the older calls, which are forwarders
(`contour → contour_with_diagnostics → contour_with_progress`, likewise for
`export`; `export_tiled_stl → export_tiled_stl_with_progress`). Nothing else
changed: no struct grew, no signature moved.

```c
DualcCancelToken* tok = dualc_cancel_token_create();   /* one per job; host-owned */
/* ... on another thread, when the user clicks Cancel: */
dualc_cancel_token_request(tok);                       /* thread-safe, sticky */

static void onProgress(void* user, int stage, uint32_t done, uint32_t total) {
  /* runs on the thread that made the ABI call, never on an engine worker */
}
int rc = dualc_field_export_tiled_stl_with_progress(field, "part.stl", &p, 6,
                                                    tok, onProgress, NULL,
                                                    err, sizeof err);
if (rc == DUALC_CANCELLED) { /* nothing at part.stl, not even a .part */ }
dualc_cancel_token_destroy(tok);                       /* after the call returned */
```

- **The token** is one atomic flag: `request` from any thread, `is_requested`
  reads it, no reset — create a fresh token per job. The engine polls it between
  every few octree cells, every leaf solve and every tile, and the call returns
  `DUALC_CANCELLED` within well under a second on any input. The token must
  outlive every call it was passed to; `request` is the only function on it that
  may run concurrently with such a call.
- **The callback** reports coarse stages (`DUALC_STAGE_SAMPLE`, `_CONTOUR`, `_WRITE`
  for the monolithic calls — `WRITE` is `(0,1)` then `(1,1)`, there is no
  checkpoint inside the writers — and `_TILE` only for the tiled call): `done`
  is monotonic within a stage, `total` constant, the last report `(total, total)`;
  inside a parallel region it arrives at most every ~100 ms. It is invoked on
  the calling thread only, must not throw, and may itself request the token.
- **The mechanism and the record** — why a token object rather than the
  callback's return value, the checkpoint table, the latency bound —
  [`docs/roadmap/14-c-abi/05-progress-and-cancel.md`](../docs/roadmap/14-c-abi/05-progress-and-cancel.md);
  the C# shape: [`CSHARP_WRAPPER_HANDOFF.md` § 9](CSHARP_WRAPPER_HANDOFF.md#9-cancellation-and-progress-050).

## Deferred

- **3MF-streaming / RAM-budget** behind `dualc_field_export_tiled_stl`.
- **Cancellation inside the monolithic writers** and at bake time
  (`dualc_field_create_*`): today cancel is honoured up to the end of the contour
  and per tile (D-45 in the decisions index).

*(The in-memory mesh resolver — passing host meshes as arrays instead of disk paths
— landed in v0.3.0; see the in-memory usage section above.)*
