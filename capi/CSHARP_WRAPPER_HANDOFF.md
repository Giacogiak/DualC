# Hand-over — C# P/Invoke wrapper for the DualC C ABI

> 🍄 **HAND-OFF COMPLETE.** This spec was consumed by the **Boletus** project
> (`D:\Boletus`): `Boletus.Core` is the managed wrapper (DONE 2026-06-17, 9/9
> marshaling tests; field-graph serializer DONE 2026-06-18). The live
> implementation and its roadmap now live in the Boletus repo
> (`docs/roadmap/03-phase2-core-wrapper.md` + `04-phase3-field-graph-serializer.md`).
> This document is **retained for provenance / as the frozen C-ABI contract** the
> wrapper targets — it is no longer an open task. DualC-side index of what moved:
> [`../docs/roadmap/15-boletus-handoff.md`](../docs/roadmap/15-boletus-handoff.md).

**Audience:** the agent/developer building the **managed C# wrapper** (a separate
project / repo) over DualC's native C ABI (`dualc_capi`). This document is
self-contained — you do **not** need the DualC C++ source to build the wrapper,
only the compiled `dualc_capi.dll` + the header `dualc_c.h` (reproduced below).

**Status of the native side:** DONE and verified (2026-06-17; in-memory mesh
sources added v0.3.0, 2026-06-19; diagnostics twins 0.4.0; cancel token + progress
callback 0.5.0, 2026-09-22 — §9). The entry-point list and count live in
[`README.md`](README.md); every addition since 0.3.0 is a new twin, never a changed
signature, so §3 below still holds verbatim for the calls it shows. Full record:
DualC repo `docs/roadmap/14-c-abi/`; authoritative header: `capi/dualc_c.h`.

---

## 1. What you are building, and where it fits

```
Rhino 8 / Grasshopper plugin  (.gha / .rhp, separate repo)   ← Phase 3
        │  uses
        ▼
C# managed wrapper            (THIS deliverable)             ← Phase 2 = you
        │  P/Invoke
        ▼
dualc_capi.dll                (native C ABI, DONE)           ← Phase 1
        │  statically links
        ▼
libdualc + field-graph parser + STL/3MF/OBJ writers
```

**Scope of the wrapper (Phase 2):** a thin, idiomatic .NET layer that lets managed
code

1. **build a field** from a field-graph string (JSON or the terse `--expr`
   shorthand),
2. **contour it to an in-memory triangle mesh** (the *proxy* a viewport draws —
   coarse depth = cheap), and
3. **export it to a file** (STL / 3MF / OBJ),

with C# exceptions instead of integer codes, `IDisposable`/deterministic cleanup,
and managed arrays instead of raw pointers. **Out of scope for you:** the Rhino
`.gha` itself (Phase 3, separate repo), and any change to the native ABI.

---

## 2. The native artifact you consume

- **`dualc_capi.dll`** — a **self-contained** native DLL. It statically links
  `libdualc`, geometry-central, and the host-side parser/writers, so its **only**
  runtime dependencies are `KERNEL32.dll`, the **VC++ runtime**
  (`MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`) and the Windows
  **UCRT** (`api-ms-win-crt-*`, present on Windows 10+). There is **no**
  `geometry-central.dll` / `dualc.dll` to ship.
- **Build it** (in the DualC repo; geometry-central is fetched at configure at its pinned
  commit, or use `-DDUALC_GC_DIR=/path` for a local tree):
  ```
  cmake -S . -B build -DDUALC_BUILD_C_ABI=ON -DDUALC_BUILD_EXAMPLES=ON
  cmake --build build --config Release --target dualc_capi
  ```
  Output: `build/capi/Release/dualc_capi.dll` (+ `.lib`, which you do **not** need
  for P/Invoke).
- **Platform/bitness:** built x64 (matches Rhino 8). The header's
  `DUALC_CAPI_EXPORT` resolves to `__declspec(dllexport)` in the DLL; you call the
  plain `extern "C"` symbols (no name mangling — verified via `dumpbin /exports`).
- **Calling convention:** the symbols are default `__cdecl` `extern "C"`. On x64
  there is a single native calling convention, but declare
  `CallingConvention.Cdecl` for portability (matters if a 32-bit build ever ships).
- **Deployment:** place `dualc_capi.dll` next to the managed assembly (or on the
  load path); ensure the **VC++ Redistributable (x64)** is installed on the
  target. For Rhino, ship the DLL beside the `.gha`/`.rhp`.

---

## 3. The exact ABI contract (authoritative reproduction of `dualc_c.h`)

```c
/* ---- status codes (int return of every fallible call) ---- */
#define DUALC_OK           0  /* success                                        */
#define DUALC_ERR_BOUNDS   1  /* unbounded field: set hasBounds + boundsMin/Max  */
#define DUALC_ERR_IO       2  /* unknown file extension or a writer/file failure */
#define DUALC_ERR_GRAPH    3  /* field-graph parse/build error (err + locator)   */
#define DUALC_ERR_USAGE    4  /* bad argument (e.g. NULL handle)                 */
#define DUALC_ERR_UNKNOWN  5  /* any other failure                              */

typedef struct DualcField DualcField;   /* opaque handle */

typedef struct {
  int      maxDepth;       /* octree max depth -- the PROXY lever (low=cheap) */
  int      minDepth;       /* octree min depth                               */
  double   collapse;       /* adaptive-collapse QEF error (0 = off)          */
  int      hasBounds;      /* 1 to use boundsMin/boundsMax; 0 = auto-fit      */
  double   boundsMin[3];   /* root box lower corner (when hasBounds)          */
  double   boundsMax[3];   /* root box upper corner (when hasBounds)          */
  int      manifold;       /* 1 = manifold dual contouring (default), 0 = off */
  unsigned numThreads;     /* 0 = all hardware threads                        */
} DualcContourParams;

typedef struct {           /* ABI-owned; release with dualc_mesh_release      */
  float*    positions;     /* 3 * vertexCount   (x,y,z per vertex)            */
  float*    normals;       /* 3 * vertexCount   (unit, index-aligned)         */
  uint32_t* indices;       /* 3 * triangleCount (0-based, fan-triangulated)   */
  uint32_t  vertexCount;
  uint32_t  triangleCount;
} DualcMesh;

typedef struct {           /* host-owned; read+copied during the create call only  */
  const char*     id;            /* graph reference key: mesh(id="...")             */
  const float*    vertices;      /* 3 * vertexCount (x,y,z)                          */
  uint32_t        vertexCount;
  const uint32_t* indices;       /* 3 * triangleCount (0-based)                      */
  uint32_t        triangleCount;
  const float*    normals;       /* optional; CURRENTLY IGNORED — pass NULL          */
} DualcMeshSource;

const char* dualc_version(void);                            /* "dualc <version>", never NULL */
void        dualc_default_params(DualcContourParams* out);  /* fills library defaults    */

int  dualc_field_create_from_json(const char* json, DualcField** out, char* err, int errlen);
int  dualc_field_create_from_expr(const char* expr, DualcField** out, char* err, int errlen);
/* in-memory mesh sources (v0.3.0) — register host buffers the graph references by id; see §4 */
int  dualc_field_create_from_json_with_meshes(const char* json, const DualcMeshSource* meshes, int meshCount, DualcField** out, char* err, int errlen);
int  dualc_field_create_from_expr_with_meshes(const char* expr, const DualcMeshSource* meshes, int meshCount, DualcField** out, char* err, int errlen);
void dualc_field_destroy(DualcField* field);

int  dualc_field_contour(DualcField* field, const DualcContourParams* params,
                         DualcMesh* out, char* err, int errlen);
void dualc_mesh_release(DualcMesh* mesh);

int  dualc_field_export(DualcField* field, const char* path,
                        const DualcContourParams* params, char* err, int errlen);
int  dualc_field_export_tiled_stl(DualcField* field, const char* path,
                                  const DualcContourParams* params, int tileDepth,
                                  char* err, int errlen);
```

### Semantics & contract (must mirror exactly)

- **No exception ever crosses the boundary** — every fallible call returns an
  `int` status. On failure the optional `err` buffer (you pass a byte buffer +
  its length) receives a NUL-terminated message; for `DUALC_ERR_GRAPH` it includes
  a **locator** (a JSON pointer like `/root/in/0/radius` for JSON input, or a
  character offset for `--expr`). Map non-`OK` to a C# exception (§5).
- **Strings are `const char*`.** Treat them as **UTF-8**. (The parser is byte-wise;
  ASCII is the common case, but mesh **paths** may contain non-ASCII — marshal
  UTF-8 to be correct, see §4.)
- **Field lifetime:** `create…` → use → `dualc_field_destroy`. One handle is
  single-threaded; independent handles are independent.
- **Mesh lifetime is independent of the field.** `dualc_field_contour` returns a
  fully-owned native copy. Read it out (copy to managed arrays), then **always**
  call `dualc_mesh_release`. The arrays are valid only until release. You may
  contour the same field repeatedly.
- **`dualc_field_contour` is the proxy lever.** Coarse `maxDepth` → cheap drawable
  proxy; full depth → export-grade mesh. There is no separate proxy call. **Caveat
  to surface in the wrapper docs:** a coarse proxy of a *lattice* is inherently
  lossy (thin walls drop out) — fine for solid/boundary parts and framing, not for
  judging lattice detail.
- **`dualc_default_params`** yields `maxDepth=7, minDepth=3, collapse=0,
  hasBounds=0, manifold=1, numThreads=0`. Always start from it, then override.
- **Export** picks the format from the file extension: `.obj` (per-vertex
  normals), `.stl` (binary), `.3mf` (3MF-mesh, **1 unit = 1 mm**).

---

## 4. Mesh sources: in-memory buffers **or** a file path (v0.3.0+)

The field-graph references meshes via a `mesh(...)` / `winding(...)` node — used to
**clip a TPMS lattice to a volume** (the headline workflow). There are two ways to
supply the geometry; **no temp file is required** for the diskless path:

- **In-memory (preferred for Rhino).** Pass an array of `DualcMeshSource` to
  `dualc_field_create_from_{json,expr}_with_meshes` and reference each by id with a
  `mesh(id="…")` / `winding(id="…")` node:
  ```c
  typedef struct {
    const char*     id;            /* graph reference key (non-empty)            */
    const float*    vertices;      /* 3 * vertexCount (x,y,z)                     */
    uint32_t        vertexCount;
    const uint32_t* indices;       /* 3 * triangleCount (0-based)                 */
    uint32_t        triangleCount;
    const float*    normals;       /* optional; CURRENTLY IGNORED — pass NULL     */
  } DualcMeshSource;

  int dualc_field_create_from_json_with_meshes(const char* json,
      const DualcMeshSource* meshes, int meshCount,
      DualcField** out, char* err, int errlen);  /* + the _from_expr twin */
  ```
  The arrays are **read and copied during the create call only** — pin them just
  for the call (`GCHandle.Alloc(..., Pinned)`), then free; they need not outlive
  the field. Geometry convention matches the file path: model units = mm, CCW =
  outward. The in-memory path is **lossless** (no ASCII round-trip); for
  exactly-representable coordinates it is byte-identical to a temp-OBJ `path=`
  (proven for the unit cube by `cli_c_abi_mesh_inmem`), and for arbitrary coords
  it is the *more* accurate of the two.

  > **The native C suite cannot test the P/Invoke layout.** Your
  > `DualcMeshSourceNative` must mirror this struct's **exact field order** (`id`,
  > `vertices`, `vertexCount`, `indices`, `triangleCount`, `normals`) with default
  > sequential layout. Your C# cube test (in-memory == temp-file count) is the real
  > acceptance gate for the marshaling contract — `cli_c_abi_mesh_inmem` proves the
  > resolver, not the boundary.

- **File path (still supported, unchanged).** `mesh(path="…")` / `winding(path="…")`
  load from disk — fine for assets already on disk; for a live Rhino mesh, prefer
  the in-memory path above.

**Wrapper design:** marshal a Rhino `Mesh` to flat `float[]` vertices + `uint[]`
indices (triangulating quads), build one `DualcMeshSource` per leaf keyed by a
content hash, and call the `*_with_meshes` create. Keep a temp-OBJ fallback only if
you must support a pre-0.3.0 DLL.

---

## 5. Recommended managed design

Three layers: a `NativeMethods` P/Invoke class (internal), small blittable interop
structs, and an idiomatic public surface (`DualcField : IDisposable`,
`DualcMeshData`, `DualcContourParams`, `DualcException`).

### 5.1 Interop structs (blittable, sequential layout)

Flatten the `double[3]` fields into scalars so the struct is **blittable** (no
custom marshaling, fastest, unambiguous layout):

```csharp
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential)]
internal struct ContourParamsNative {
    public int    maxDepth;
    public int    minDepth;
    public double collapse;
    public int    hasBounds;
    public double bMinX, bMinY, bMinZ;   // boundsMin[3]
    public double bMaxX, bMaxY, bMaxZ;   // boundsMax[3]
    public int    manifold;
    public uint   numThreads;
}

[StructLayout(LayoutKind.Sequential)]
internal struct MeshNative {
    public IntPtr positions;     // float*  (3 * vertexCount)
    public IntPtr normals;       // float*  (3 * vertexCount)
    public IntPtr indices;       // uint32* (3 * triangleCount)
    public uint   vertexCount;
    public uint   triangleCount;
}

// Mirrors C `DualcMeshSource` — field ORDER and TYPES must match exactly
// (id, vertices, vertexCount, indices, triangleCount, normals). The IntPtr
// pointer fields must reference PINNED managed buffers (GCHandle.Alloc(...,
// Pinned)) kept alive only for the create call (the native side copies them).
[StructLayout(LayoutKind.Sequential)]
internal struct DualcMeshSourceNative {
    public IntPtr id;            // const char*  (UTF-8, NUL-terminated)
    public IntPtr vertices;      // const float* (3 * vertexCount)
    public uint   vertexCount;
    public IntPtr indices;       // const uint32* (3 * triangleCount)
    public uint   triangleCount;
    public IntPtr normals;       // const float* — pass IntPtr.Zero (ignored)
}
```

### 5.2 P/Invoke declarations

```csharp
internal static class NativeMethods {
    private const string Dll = "dualc_capi"; // dualc_capi.dll on the load path

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr dualc_version();

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern void dualc_default_params(out ContourParamsNative outParams);

    // const char* marshalled as a UTF-8 byte[] (see Utf8() helper).
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int dualc_field_create_from_json(byte[] json, out IntPtr outField, byte[] err, int errlen);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int dualc_field_create_from_expr(byte[] expr, out IntPtr outField, byte[] err, int errlen);

    // In-memory mesh sources (v0.3.0). `meshes` is a DualcMeshSourceNative[]; the
    // pointer fields must reference PINNED managed arrays (GCHandle/fixed) that stay
    // alive for the duration of the call only. Pass null/0 for the no-mesh case.
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int dualc_field_create_from_json_with_meshes(byte[] json, [In] DualcMeshSourceNative[] meshes, int meshCount, out IntPtr outField, byte[] err, int errlen);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int dualc_field_create_from_expr_with_meshes(byte[] expr, [In] DualcMeshSourceNative[] meshes, int meshCount, out IntPtr outField, byte[] err, int errlen);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern void dualc_field_destroy(IntPtr field);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int dualc_field_contour(IntPtr field, in ContourParamsNative p, out MeshNative outMesh, byte[] err, int errlen);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern void dualc_mesh_release(ref MeshNative mesh);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int dualc_field_export(IntPtr field, byte[] path, in ContourParamsNative p, byte[] err, int errlen);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int dualc_field_export_tiled_stl(IntPtr field, byte[] path, in ContourParamsNative p, int tileDepth, byte[] err, int errlen);
}
```

> **.NET 7+ alternative:** Rhino 8 runs on modern .NET, so you may prefer the
> source-generated `[LibraryImport]` over `[DllImport]`. The signatures are the
> same; mark string params `[MarshalAs(UnmanagedType.LPUTF8Str)] string` to get
> UTF-8 marshaling for free instead of the manual `byte[]` helper below. Either
> approach is fine — `[DllImport]` + UTF-8 `byte[]` is shown here because it is
> explicit and version-agnostic.

### 5.3 String / error helpers

```csharp
using System.Text;

internal static class Native {
    // NUL-terminated UTF-8 bytes for a const char* in-param.
    public static byte[] Utf8(string s) => Encoding.UTF8.GetBytes((s ?? "") + '\0');

    public static string ReadUtf8(byte[] buf) {
        int n = Array.IndexOf(buf, (byte)0);
        if (n < 0) n = buf.Length;
        return Encoding.UTF8.GetString(buf, 0, n);
    }

    public static void Check(int rc, byte[] err) {
        if (rc == 0) return;                       // DUALC_OK
        throw new DualcException(rc, ReadUtf8(err));
    }
}

public sealed class DualcException : Exception {
    public int Code { get; }                       // DUALC_ERR_* value
    public DualcException(int code, string message)
        : base($"DualC error {code}: {message}") { Code = code; }
}
```

### 5.4 Public managed surface

```csharp
public struct DualcContourParams {
    public int    MaxDepth, MinDepth;
    public double Collapse;
    public bool   HasBounds;
    public (double X, double Y, double Z) BoundsMin, BoundsMax;
    public bool   Manifold;
    public uint   NumThreads;

    public static DualcContourParams Default() {
        NativeMethods.dualc_default_params(out var n);
        return new DualcContourParams {
            MaxDepth = n.maxDepth, MinDepth = n.minDepth, Collapse = n.collapse,
            HasBounds = n.hasBounds != 0, Manifold = n.manifold != 0,
            NumThreads = n.numThreads,
            BoundsMin = (n.bMinX, n.bMinY, n.bMinZ), BoundsMax = (n.bMaxX, n.bMaxY, n.bMaxZ),
        };
    }

    internal ContourParamsNative ToNative() => new ContourParamsNative {
        maxDepth = MaxDepth, minDepth = MinDepth, collapse = Collapse,
        hasBounds = HasBounds ? 1 : 0, manifold = Manifold ? 1 : 0, numThreads = NumThreads,
        bMinX = BoundsMin.X, bMinY = BoundsMin.Y, bMinZ = BoundsMin.Z,
        bMaxX = BoundsMax.X, bMaxY = BoundsMax.Y, bMaxZ = BoundsMax.Z,
    };
}

public sealed class DualcMeshData {
    public float[] Positions = Array.Empty<float>();  // 3 per vertex
    public float[] Normals   = Array.Empty<float>();  // 3 per vertex
    public int[]   Indices   = Array.Empty<int>();    // 3 per triangle
    public int VertexCount   => Positions.Length / 3;
    public int TriangleCount => Indices.Length / 3;

    internal static DualcMeshData CopyFrom(in MeshNative m) {
        int nv = (int)m.vertexCount, nt = (int)m.triangleCount;
        var d = new DualcMeshData {
            Positions = new float[nv * 3],
            Normals   = new float[nv * 3],
            Indices   = new int[nt * 3],
        };
        if (nv > 0) {
            Marshal.Copy(m.positions, d.Positions, 0, nv * 3);
            Marshal.Copy(m.normals,   d.Normals,   0, nv * 3);
        }
        if (nt > 0) Marshal.Copy(m.indices, d.Indices, 0, nt * 3); // uint32 -> int (indices < 2^31)
        return d;
    }
}

public sealed class DualcField : IDisposable {
    private IntPtr _handle;
    private DualcField(IntPtr h) => _handle = h;

    public static string Version() => Marshal.PtrToStringUTF8(NativeMethods.dualc_version()) ?? "";

    public static DualcField FromExpr(string expr) => Create(true,  expr);
    public static DualcField FromJson(string json) => Create(false, json);

    private static DualcField Create(bool expr, string text) {
        var err = new byte[512];
        int rc = expr
            ? NativeMethods.dualc_field_create_from_expr(Native.Utf8(text), out var h, err, err.Length)
            : NativeMethods.dualc_field_create_from_json(Native.Utf8(text), out var h2, err, err.Length);
        // (use one 'out' var in real code; split here only for illustration)
        Native.Check(rc, err);
        return new DualcField(/* h or h2 */ default);
    }

    public DualcMeshData Contour(DualcContourParams p) {
        var np = p.ToNative(); var err = new byte[512];
        Native.Check(NativeMethods.dualc_field_contour(_handle, np, out var mesh, err, err.Length), err);
        try { return DualcMeshData.CopyFrom(mesh); }
        finally { NativeMethods.dualc_mesh_release(ref mesh); }   // ALWAYS release
    }

    public void Export(string path, DualcContourParams p) {
        var np = p.ToNative(); var err = new byte[512];
        Native.Check(NativeMethods.dualc_field_export(_handle, Native.Utf8(path), np, err, err.Length), err);
    }

    public void ExportTiledStl(string path, DualcContourParams p, int tileDepth) {
        var np = p.ToNative(); var err = new byte[512];
        Native.Check(NativeMethods.dualc_field_export_tiled_stl(_handle, Native.Utf8(path), np, tileDepth, err, err.Length), err);
    }

    public void Dispose() {
        if (_handle != IntPtr.Zero) { NativeMethods.dualc_field_destroy(_handle); _handle = IntPtr.Zero; }
        GC.SuppressFinalize(this);
    }
    ~DualcField() => Dispose();
}
```

> Clean up the `Create` illustration to use a single `out var handle`. Consider a
> `SafeHandle`-derived type instead of a raw `IntPtr` if you want
> finalizer-guaranteed release under partial-trust / async teardown.

### 5.5 Usage

```csharp
Console.WriteLine(DualcField.Version());           // "dualc <version>"

using var field = DualcField.FromExpr(
    "intersection(onion(gyroid(wavelength=0.5),thickness=0.12)," +
    "box(min=[-1,-1,-1],max=[1,1,1]))");

var p = DualcContourParams.Default();
p.MaxDepth = 6;                                    // coarse = drawable PROXY
DualcMeshData proxy = field.Contour(p);            // -> Rhino.Geometry.Mesh

p.MaxDepth = 8;                                    // full = export grade
field.Export(@"C:\tmp\part.stl", p);               // .obj/.stl/.3mf by extension
```

---

## 6. Rhino integration notes (for Phase 3, but design for them now)

- **Building a `Rhino.Geometry.Mesh`** from `DualcMeshData` requires populating
  `Mesh.Vertices`/`Mesh.Faces` — an unavoidable construction copy on the Rhino
  side (zero-copy at the C ABI boundary cannot remove it). For each triangle
  `t`, add face `(Indices[3t], Indices[3t+1], Indices[3t+2])`; vertices come from
  `Positions` in triples. Set normals from `Normals` if you want smooth shading.
- **Mesh clipping is diskless** (see §4): marshal the Rhino volume mesh to flat
  `float[]`/`uint[]` buffers, pass it as a `DualcMeshSource` to a `*_with_meshes`
  create call, and reference it as `mesh(id="<id>")` in the graph string — no temp
  file. (A temp-OBJ `mesh(path=…)` fallback is only needed for a pre-0.3.0 DLL.)
- **Proxy vs export:** draw the coarse-depth `Contour` result in the viewport;
  call `Export` at full depth on demand. (Live raymarch is the side-car's job, not
  the plugin viewport's.)
- **Units:** `.3mf` export is 1 unit = 1 mm; make sure the document tolerance/units
  match what you feed the graph.

---

## 7. How to test the wrapper

Mirror the native C demo (`capi/dualc_c_demo.c`) in C#:

1. `DualcField.Version()` returns non-empty.
2. `FromExpr(<analytic gyroid∩box>)` → `Contour(maxDepth=6)` → assert
   `VertexCount > 0 && TriangleCount > 0`.
3. `Export("out.stl", …)` → assert the file exists and is > 84 bytes (binary-STL
   header + count).
4. **In-memory mesh:** marshal a small mesh (e.g. a cube) to `float[]`/`uint[]`,
   build `intersection(onion(gyroid…),mesh(id="cube"))` via the `*_with_meshes`
   create call, contour → non-empty. (Verifies the diskless resolver path Rhino
   uses; mirrors the native `cli_c_abi_mesh_inmem` byte-identical gate. A
   `mesh(path="<temp>.obj")` form also still works.)
5. **Error path:** `FromExpr("onion(gyroid(),0.1)")` (positional `thickness` is
   rejected — must be `thickness=0.1`) → expect a `DualcException` with
   `Code == 3` (`DUALC_ERR_GRAPH`) and a locator in the message.
6. **Optional parity:** export the same graph via the DualC CLI
   `dualc_field --expr "…" --depth N -o ref.stl` and byte-compare — they are
   identical (both route through the same writer).

The field-graph vocabulary is exactly what `dualc_field` accepts; its catalogue is
in the DualC repo `docs/command_reference/11-dualc_field/` (op tokens, params,
the `--expr` grammar). `dualc_field --list` prints the vocabulary; `--dump-json`
canonicalises an `--expr` string (a handy round-trip aid for the wrapper).

---

## 8. Gotchas & coordination

- **Always `dualc_mesh_release`** after copying a contour result (the `finally` in
  §5.4). Forgetting it leaks native memory.
- **`maxDepth` cost is exponential** (~4–8× per level): a deep contour of a dense
  lattice is slow and large. Expose it and document it; default to a modest proxy
  depth for interactive paths.
- **`hasBounds` is required for unbounded fields** (a bare TPMS, `plane`, infinite
  primitives, `repeat`). Without a bounding operand or `HasBounds=true` you get
  `DUALC_ERR_BOUNDS` — surface that as actionable. (In practice the clip volume —
  a `box`/`mesh` — supplies the finite domain.)
- **`--tile-depth`/`ExportTiledStl` is STL-only** and cannot combine with
  `Collapse > 0` (per-tile collapse cracks seams). 3MF streaming is deferred.
- **Pin the DLL build.** Check `Version()` at startup and ship the matching
  `dualc_capi.dll`; the ABI is stable but the field-graph vocabulary can grow.
- **In-memory mesh resolver has landed** (§4, v0.3.0): use the `*_with_meshes`
  create calls + `mesh(id="…")` for live Rhino meshes — no temp file. Keep a
  temp-OBJ `path=` fallback only if you must support a pre-0.3.0 DLL.
- **A running export can be cancelled** (§9, 0.5.0) — the "single-flight, queued
  restart" stopgap and the indeterminate "Writing… Ns" status can go.

---

## 9. Cancellation and progress (0.5.0)

This **supersedes the sketch in Boletus's D-30 / 07 § 7** (a `_cb` twin whose
callback *return value* cancels): cancellation is a separate **token object** the
host sets, and progress is a separate **callback**. The reason is robustness under
threads — every engine checkpoint is a relaxed atomic load safe from any worker,
there is no reverse P/Invoke in the hot path, and the token's lifetime is the
host's, independent of the field handle (DualC decision D-47).

```c
#define DUALC_CANCELLED    6
#define DUALC_STAGE_SAMPLE 0   /* engine: octree build           */
#define DUALC_STAGE_CONTOUR 1  /* engine: QEF solve + traversal  */
#define DUALC_STAGE_WRITE  2   /* monolithic export: (0,1),(1,1) */
#define DUALC_STAGE_TILE   3   /* tiled export: (i, T)           */

typedef struct DualcCancelToken DualcCancelToken;
DualcCancelToken* dualc_cancel_token_create(void);
void dualc_cancel_token_request(DualcCancelToken*);          /* any thread; sticky */
int  dualc_cancel_token_is_requested(const DualcCancelToken*);
void dualc_cancel_token_destroy(DualcCancelToken*);          /* after the call returned */

typedef void (*DualcProgressFn)(void* user, int stage, uint32_t done, uint32_t total);

int dualc_field_contour_with_progress(DualcField*, const DualcContourParams*, DualcMesh* out,
    DualcDiagnostics* diag, const DualcCancelToken* cancel, DualcProgressFn progress, void* user,
    char* err, int errlen);
int dualc_field_export_with_progress(DualcField*, const char* path, const DualcContourParams*,
    DualcDiagnostics* diag, const DualcCancelToken* cancel, DualcProgressFn progress, void* user,
    char* err, int errlen);
int dualc_field_export_tiled_stl_with_progress(DualcField*, const char* path,
    const DualcContourParams*, int tileDepth, const DualcCancelToken* cancel,
    DualcProgressFn progress, void* user, char* err, int errlen);
```

**Contract.** All-NULL twins are byte-identical to the older calls (which are now
forwarders). On `DUALC_CANCELLED` the mesh out-param is zeroed, `err` reads
`cancelled (...)`, and **no file exists at `path`** — the writer works on
`path + ".part"` and renames only on success (a file already at `path` is left as
it was; that holds for every export, cancelled or not). The callback runs **on the
thread that made the P/Invoke**, never on an engine worker; within a stage `done`
is monotonic, `total` constant, the last report is `(total, total)`; inside a
parallel region it fires at most every ~100 ms. Cancellation is honoured up to the
end of the contour and per tile — the write phase itself is not interrupted
(D-45). `DualcContourParams` did not grow; your 80-byte mirror stays valid.

**Managed shape (recommended).**

- `DualcCancelTokenHandle : SafeHandle` — `ReleaseHandle` calls
  `dualc_cancel_token_destroy`. Keep the handle alive for the whole native call
  (`DangerousAddRef` / `GC.KeepAlive`), and never let the finalizer run while a
  call that received it is in flight: that is the one use-after-free in this API.
- `CancellationToken ct` → `using var reg = ct.Register(() =>
  NativeMethods.dualc_cancel_token_request(handle));` before the call, disposed
  after. `DUALC_CANCELLED` → throw `OperationCanceledException(ct)`.
- `[UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void
  DualcProgressFn(IntPtr user, int stage, uint done, uint total);` — root the
  delegate instance in a field (or `GCHandle`) for the call's duration so the
  GC cannot collect the thunk; forward to `IProgress<(int stage, uint done,
  uint total)>`. The delegate **must never throw**: an exception unwinding into
  native code crashes the process.
- `ExportTiledStl(path, params, tileDepth, IProgress<…>, CancellationToken)` and
  the `Export` / `Contour` overloads: create the token on the worker thread that
  runs the call, request it from the UI thread.

**Once it lands on the Boletus side:** re-vendor `dualc_capi.dll` (check
`Version()` reads `dualc 0.5.0`), bind the three twins and the four token
functions, flip D-30, and let `Write to File`'s button become "Cancel ■" with a real
percentage.

---

*Native side reference: DualC repo `capi/dualc_c.h` (authoritative),
`capi/dualc_c.cpp` (shim), `capi/dualc_c_demo.c` (worked example),
`docs/roadmap/14-c-abi/` (full record).*
