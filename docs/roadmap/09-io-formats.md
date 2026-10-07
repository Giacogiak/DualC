# Input & output formats

Mesh export (STL / 3MF / OBJ chosen by `-o` extension) and the decision around
input vertex-normal handling. Export sits inside the dense-lattice deliverable
(see [11-dense-lattice-deliverable/](11-dense-lattice-deliverable/README.md)); the
export-format dispatch table is also in
[../command_reference/README.md](../command_reference/README.md) (*Export formats*).

*2026-10-07: the robustness of the export's `.part` rename on Windows is tracked as
[17 #52](17-code-audit-and-hardening/16-windows-rename-race/README.md#52-windows-export-io-flake--the-part-rename-under-an-on-access-scanner).*

## 7. Read explicit input OBJ vn. [DEFERRED — 2026-05-26]

Skipped after analysis showed the original framing is both low-value and architecturally
wrong for DualC. **Current state.** Input vertex normals are computed area-weighted from
positions+topology by geometry-central inside `MeshBVH::packMesh`
(`src/internal/mesh_bvh.cpp:127-140`, `geometry.requireVertexNormals()`), then
barycentric-blended at each Hermite-edge crossing in `MeshBVH::segmentFirstHit`
(`mesh_bvh.cpp:430-448`, gated by `interpolateNormals`) — those blended normals feed the
QEF gradient input, so they do affect dual-contour vertex placement, not just shading.
**Why the functional impact is modest.** For purely smooth inputs (fine tessellation)
the area-weighted approximation is already near-exact; for sharp inputs `--sharp`
bypasses vertex normals entirely and uses face normals. The only case explicit `vn`
actually helps is the *middle case* — a single mesh mixing smooth and sharp regions
encoded via smoothing groups, i.e. one position carrying multiple `vn` indices across
different faces. Real but not the current bottleneck. **Why the original framing is
wrong.** The note proposed a custom OBJ parser inside DualC, conflicting with the
library-scope principle (mesh I/O and spatial transforms are the host's job, not
DualC's). The user's planned migration from OBJ to PLY binary makes embedding a format
reader even more unattractive. **Architecture-correct shape if revisited.** Don't parse
`vn` inside DualC. Instead, expose an optional per-vertex (or per-corner) normals
parameter on `MeshBVH` (forwarded through `MeshSource` / `WindingNumberField`); the host
application supplies whatever normals it already has from whatever format it reads.
Library surface ~15-25 LOC, no `src/` IO code; any `vn`-respecting OBJ helper for
`dualc_demo` lives in `examples/`. **Trigger to revisit.** A real input mesh whose
smoothing groups visibly degrade in DualC's output AND a host application that already
has those normals at hand.
*(2026-10-07: "area-weighted" above is wrong, and was wrong on the day it was written.
geometry-central weights the unit face normals by corner angle; the argument stands as made.
Found by semantic-lint run [06](19-docs-layers/08-semantic-lint/06-2026-10-07-after-ci-plan.md); the mechanism is
[`design/05` § 5](../design/05-conventions-and-tables.md#5-geometry-central-integration).)*

## Step 2. STL + 3MF-mesh writer — printable immediately, universal, what Rhino expects. [DONE — 2026-06-02]

Refactored the example writer so output format follows the `-o` extension in a
single contour pass: new `dce::writeField` in `examples/example_common.{h,cpp}`
runs `dualContourField` once (via `contourOrHint`, preserving the unbounded-field
`--bounds` hint) then dispatches on `lowerExt` to `writeObj` / `writeStl` /
`write3mf`; at the time `writeFieldToObj` was kept as a thin OBJ-forcing shim so
the other CLIs were untouched, and only `dualc_lattice` called `writeField`.
**(2026-06-12 follow-up:** the remaining field CLIs — `dualc_primitive`,
`dualc_boolean`, `dualc_lift`, `dualc_csg_demo` — were migrated to `writeField`
and the now-dead shim removed, so **every field CLI gains `.stl`/`.3mf` for
free** via `-o foo.stl`.) **Binary STL:** fan-triangulates DC
quads, per-facet normal from the (CCW) winding, 50-byte records packed via memcpy
(no struct padding), 80-byte header that never begins "solid". **3MF:** minimal
OPC ZIP (deflate via vendored miniz) with `[Content_Types].xml`, `_rels/.rels`,
and a hand-written `3D/3dmodel.model` (`unit="millimeter"`, 1 world unit = 1 mm).
Vendored examples-only deps in `examples/third_party/` (`stb_image_write.h` PD,
`miniz.{h,c}` MIT) compiled into a host-only `dualc_examples_io` static lib —
**libdualc stays dependency-free.** **Verified:** STL header facet count
17316 == nFaces, file size exactly 84 + 50·count; 3MF members present + correct
casing, XML well-formed, `unit=millimeter`, 10880 V / 17316 tris matching the
STL; 3MF 286 KB vs 865 KB STL vs 4.7 MB OBJ (~⅓-of-STL compaction confirmed);
`cli_lattice_stl` + `cli_lattice_3mf` ctests green; full 13-case CLI suite green.
Docs in `README.md` (Export formats / Slice sampler) and `THIRD_PARTY.md`.

---

← Back to the [Roadmap index](README.md).
