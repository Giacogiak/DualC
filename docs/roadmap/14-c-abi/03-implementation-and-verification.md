# Implementation map, build & deployment, verification record (§ 6–§ 8)

Part of [14 — C ABI](README.md). Where each piece lives, how it is built and shipped,
and the 2026-06-17 verification record.

## 6. Implementation map

| File | Role |
|---|---|
| `capi/dualc_c.h` | Public C header (§3.1): `DUALC_CAPI_EXPORT` macro (dllexport/dllimport on MSVC; `visibility("default")` elsewhere), `extern "C"` + `__cplusplus` guard, only C types. |
| `capi/dualc_c.cpp` | The shim. `createImpl` → `parseJson`/`parseShorthand` → `FieldGraph::build(root, handle.resolver)`. `dualc_field_contour` → `dualc::dualContourField(field, sp, cp)` then flat extraction. Export → `dce::writeField` / `dce::writeFieldTiledStl`. |
| `capi/dualc_c_demo.c` | The C harness (compiled **as C**). Analytic mode (default), `mesh` mode (`mesh(path="cube.obj")`), and `inmem` mode (loads cube.obj into buffers, builds via `mesh(id="cube")` + a `*_with_meshes` call, and asserts byte-identical to the `path=` form); builds → contours a proxy → exports STL → releases. |
| `capi/CMakeLists.txt` | `dualc_capi` SHARED target + the demo + the three CTests + the `cube.obj` POST_BUILD copy. |
| `capi/README.md` | Build + usage quick-start. |
| `capi/CSHARP_WRAPPER_HANDOFF.md` | The hand-over spec for the C# wrapper — consumed by the **Boletus** project (`Boletus.Core`); see [15-boletus-handoff.md](../15-boletus-handoff.md). |
| `examples/field_graph.{h,cpp}` (`dce::fieldgraph::InMemoryMeshResolver`) | The in-memory resolver itself (v0.3.0) lives here, not in `capi/` — it is host-side field-graph wiring (the C ABI just owns one and registers buffers into it). Holds a registry keyed by id + a delegate `FileMeshResolver` for `path=`. `registerMesh` copies a buffer into an owned geometry-central mesh via `SimplePolygonMesh` → `stripUnusedVertices` → `makeSurfaceMeshAndGeometry` (the exact construction `readSurfaceMesh` uses). |

**In-memory mesh design notes (v0.3.0).**
- **`id` is routed separately from `path`, never silently coerced.** `buildField`
  sends a `mesh(id=…)`/`winding(id=…)` node to `MeshResolver::resolveId(id)` and a
  `path=` node to `resolve(path)`; the base `resolveId` throws a clear "in-memory
  meshes not provided", and `InMemoryMeshResolver::resolveId` throws "no in-memory
  mesh registered with id '…'" on a miss — so a typo'd id is a precise graph error,
  not a stray attempt to open a file named after the id. A node must carry exactly
  one of `path`/`id` (both/neither → `GraphError`). Both negatives are asserted in
  the `inmem` demo.
- **Eager copy at create (a deliberate strengthening of the upstream lifetime
  rule).** Buffers are read and copied during the create call, so the host may free
  them immediately after — they need *not* outlive the `DualcField`. This is safer
  for a managed caller (pin for the call only) than "buffers must outlive the
  field".
- **`normals` is ignored** — `MeshSource` derives normals from geometry via the
  node's `smooth`/`sharp` option; the field is kept for forward-compat (pass NULL).
- **Driver:** the in-memory resolver unblocks **Boletus**, the .NET Rhino/
  Grasshopper plugin client, whose meshless `Volume` passes geometry between
  components without the per-hop temp-OBJ round-trip. The Boletus-side P/Invoke
  flip is **done** (`Boletus.Core` consumed the `*_with_meshes` twins, 2026-06-19;
  see [15-boletus-handoff.md](../15-boletus-handoff.md)).

**Flat extraction (in `dualc_field_contour`).** `dualContourField` returns
`(unique_ptr<SurfaceMesh>, unique_ptr<VertexPositionGeometry>, vector<Vector3> normals)`.
The shim copies `geom->inputVertexPositions[v]` and `normals[v.getIndex()]`
(index-aligned) into `float[]`, and **fan-triangulates** every polygonal face
(`mesh->getFaceVertexList()`, emit `{f[0],f[i],f[i+1]}`) into `uint32_t[]` — the
exact logic of `example_common.cpp`'s `toTriMesh` (which is in an anonymous
namespace and so could not be reused directly). This is why the in-memory mesh
matches the file writers face-for-face.

**No new field maths.** The shim wires existing reusable code only:
`FieldGraph::build`, `dualContourField`, `writeField`, `writeFieldTiledStl`.

## 7. Build & deployment

- **Opt-in:** `option(DUALC_BUILD_C_ABI … OFF)` in the top-level `CMakeLists.txt`;
  `add_subdirectory(capi)` only when ON, and only valid with
  `DUALC_BUILD_EXAMPLES` (it links the two example static libs — enforced with a
  `FATAL_ERROR`). Default builds are unperturbed (the option only gates the
  subdir).
- **Target:** `add_library(dualc_capi SHARED …)` + `add_library(dualc::capi ALIAS …)`.
  Compiled with `DUALC_CAPI_BUILD` (switches the macro to `dllexport`),
  `POSITION_INDEPENDENT_CODE ON` (no-op on MSVC; enables a future Linux `.so`),
  and `DUALC_C_VERSION_STR="${PROJECT_VERSION}"`. `_CRT_SECURE_NO_WARNINGS` on MSVC.
- **Deployment is a single DLL.** `dualc_capi.dll` statically links `libdualc`,
  geometry-central, and the example libs, so its only runtime dependencies are
  `KERNEL32.dll`, the **VC++ runtime** (`MSVCP140.dll`, `VCRUNTIME140.dll`,
  `VCRUNTIME140_1.dll`), and the Windows **UCRT** (`api-ms-win-crt-*`). There is
  **no** `geometry-central.dll` or `dualc.dll` to ship. A consumer ships
  `dualc_capi.dll` and ensures the VC++ Redistributable is present.

## 8. Tests & verification record
**2026-06-17.**


- **CTests** (`-DDUALC_BUILD_C_ABI=ON -DDUALC_BUILD_TESTS=ON`):
  - `cli_c_abi` — analytic/TPMS/boolean graph (`intersection(onion(gyroid…),box…)`)
    via `--expr` → proxy contour (101 476 v / 163 740 t) → STL export.
  - `cli_c_abi_mesh` — the headline path: `intersection(onion(gyroid…),
    mesh(path="cube.obj"))` → exercises the `FileMeshResolver` and a clean destroy
    (70 032 v / 120 612 t). `cube.obj` is copied next to the demo by a POST_BUILD
    step (the `*.obj` are gitignored — regenerate with `dualc_gen_demo all --dir data`;
    since 2026-09-21 the build generates them itself, [20 #47](../20-public-delivery.md)).
  - `cli_c_abi_mesh_inmem` (v0.3.0) — the diskless gate: loads cube.obj into float/
    uint buffers, builds `intersection(onion(gyroid…),mesh(id="cube"))` via
    `dualc_field_create_from_expr_with_meshes`, and asserts it contours
    **byte-identical** to the `mesh(path="cube.obj")` form (counts + every position
    + every index), then exports through the in-memory field.
  - All green; full suite green with them registered.
- **Exports:** `dumpbin /exports dualc_capi.dll` shows **exactly the 11** `extern "C"`
  symbols (9 core + the two `*_with_meshes` twins), nothing mangled — and the
  C-compiled demo links them (proves the header is C-clean).
  *(2026-09-18: the 2026-06-17 run saw the **9** exports of 0.2.0 (`3287754`); the 11 are
  0.3.0's (`e345bf3`, 2026-06-19), edited in here when the twins landed; 13 since 0.4.0
  (`ebe929a`) — counted from `capi/dualc_c.h` at each commit.)*
- **Parity:** the same `--expr` through `dualc_field -o x.stl` and through
  `dualc_field_export` produces a **byte-identical** STL (same SHA-1), since both
  route through `writeField`.
- **Default (`OFF`) build unperturbed** — by construction (the option only gates
  `add_subdirectory(capi)`) and proven as a subset of the green `ON` run.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
