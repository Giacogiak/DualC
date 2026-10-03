# The surface and the contract (§ 3, § 4)

Part of [14 — C ABI](README.md). The graph string as the construction API, the
parameter mapping, and the ownership /
lifetime / error contract. The header itself is the reference:
[`capi/dualc_c.h`](../../../capi/dualc_c.h); later entries:
[04](04-abi-0-4-0.md).

## 3. The surface — the graph string *is* the construction API

Because the field-graph parser already *is* a complete construction API (every
primitive, boolean, decorator, TPMS, mesh/winding source — the same vocabulary
`dualc_field` accepts as JSON or `--expr`), the ABI does **not** expose
per-primitive factories. The original sketch imagined 25–40 factory calls
(`dualc_field_make_gyroid`, `dualc_field_intersection`, …); the realized surface
is **11 flat entry points** at 0.3.0 (the 9 of 0.2.0 + the two `*_with_meshes` create twins; 13 since 0.4.0). The
host composes by sending one string, plus — for geometry already in RAM — an
array of in-memory mesh buffers.

### 3.1 Complete header (`capi/dualc_c.h`)

The v0.2.0 header was reproduced here verbatim. It has changed twice since
(0.3.0 in-memory meshes, 0.4.0 diagnostics twins) and the copy had gone stale, so
it was removed on 2026-09-11: the header **is** the reference —
[`capi/dualc_c.h`](../../../capi/dualc_c.h), build and usage in
[`capi/README.md`](../../../capi/README.md).

### 3.2 Per-function reference

The per-function table that stood here was the 0.3.0 surface and had gone stale (no
0.4.0 diagnostics twins); removed 2026-09-18, as the header copy was on 2026-09-11. The
header **is** the reference — [`capi/dualc_c.h`](../../../capi/dualc_c.h), one doc
comment per entry point — and the contract, the return codes and the proxy-vs-export
lever are in [`capi/README.md`](../../../capi/README.md).

### 3.3 `DualcContourParams` → engine mapping

`applyParams` (in `capi/dualc_c.cpp`) maps the flat struct onto the two engine
param structs exactly as `dualc_field`'s CLI flags do:

| Field | Maps to | Notes |
|---|---|---|
| `maxDepth` | `dualc::SamplerParams::maxDepth` | proxy lever; `--depth` |
| `minDepth` | `dualc::SamplerParams::minDepth` | |
| `numThreads` | `dualc::SamplerParams::numThreads` | 0 = all cores |
| `hasBounds`,`boundsMin`,`boundsMax` | `dualc::SamplerParams::rootBounds` (`std::optional<BBox>`) | only set when `hasBounds!=0`; `--bounds` |
| `collapse` | `dualc::ContourerParams::simplificationError` | `--collapse`; 0 = off |
| `manifold` | `dualc::ContourerParams::manifoldDC` | `--no-manifold` ⇒ 0 |

All other engine params keep their library defaults (e.g. `signMethod`, which is
irrelevant for field sampling — sign comes from the field's own `valueAt`).
*(2026-09-18: checked against `applyParams` in `capi/dualc_c.cpp` — the table is still
exact, which is why it stays where the per-function table did not.)*

## 4. Contract — ownership, lifetime, errors

- **No C++ exception ever crosses the boundary.** Every entry point wraps its body
  in `try { … } catch (const GraphError&) … catch (const std::exception&) …
  catch (...)`. A `GraphError` becomes `DUALC_ERR_GRAPH` with
  `what() + " (at " + pointer() + ")"` written into `err`; the sampler's
  unbounded-field `std::invalid_argument` becomes `DUALC_ERR_BOUNDS`; anything
  else is `DUALC_ERR_UNKNOWN`.
- **`err`/`errlen`** is an optional caller buffer; the message is NUL-terminated
  and truncated to `errlen-1`. On success the buffer is cleared to `""`. Passing
  `err=NULL`/`errlen=0` is allowed (no message returned).
- **Field lifetime:** create → use → `dualc_field_destroy`. Internally the handle
  is `struct DualcField { InMemoryMeshResolver resolver; std::optional<FieldGraph> graph; }`
  — `resolver` is declared **first** so it is destroyed **last**, after the
  `FieldGraph` whose `MeshSource` nodes hold bare references into meshes the
  resolver owns. This ordering is the one piece of genuinely ABI-specific logic.
  (`InMemoryMeshResolver` handles both `id=` host buffers and `path=` disk loads;
  with no buffers registered it behaves exactly like the old `FileMeshResolver`.)
- **In-memory mesh-source lifetime:** the `DualcMeshSource` arrays passed to a
  `*_with_meshes` create call are read and **copied into owned geometry-central
  meshes during that call** — the host may free them immediately after. They are
  built the same way `readSurfaceMesh` builds a loaded OBJ (`SimplePolygonMesh` →
  `stripUnusedVertices` → `makeSurfaceMeshAndGeometry`). For geometry whose
  coordinates round-trip losslessly through OBJ ASCII, an `id=` mesh contours
  byte-for-byte identically to a temp-file `path=` (proven for the unit cube by
  `cli_c_abi_mesh_inmem`). For arbitrary float coordinates the in-memory path is
  **lossless** while a temp-OBJ fallback may differ by a ULP — i.e. the in-memory
  path is the *more* accurate one, not a deviation from a gold standard. The
  optional `normals` field is
  currently ignored — the engine derives normals from geometry via the node's
  `smooth`/`sharp` option; pass `NULL`.
- **Mesh lifetime is independent of the field.** `dualc_field_contour` returns a
  fully-owned copy (heap `float[]`/`uint32_t[]`); it stays valid after the field
  is destroyed and must be released with `dualc_mesh_release`. You may contour the
  same field repeatedly at different depths.
- **Thread-safety:** not declared. Treat one `DualcField` as single-threaded;
  independent handles are independent.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
