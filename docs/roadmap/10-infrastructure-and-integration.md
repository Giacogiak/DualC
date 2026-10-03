# Infrastructure & integration

Build/packaging, native-consumer integration (the C ABI), language bindings, the
benchmark suite, and the demo-data generator. **#19 was re-sequenced 2026-06-12**
(see [12-field-graph-and-app/](12-field-graph-and-app/README.md)): it now sits *behind*
validating the engineer's value (the field-graph + standalone app) and is
**re-scoped to batch proxy + export mesh generation** — a host plugin, not the
ABI, drives live visualization.

> 🍄 **Moved to Boletus.** The C# P/Invoke wrapper + `.gha` Grasshopper plugin that
> consume this ABI are now owned by the Boletus project (`D:\Boletus`); their
> record and the original DualC intent are in
> [15-boletus-handoff.md](15-boletus-handoff.md). Block 10 keeps only the
> **C ABI itself** — DualC's own work.

## 19. C ABI for native consumers. [Phase 1 (proxy + export) DONE — 2026-06-17]

> **Re-sequenced 2026-06-12** ([block 12](12-field-graph-and-app/README.md)). Still the
> integration path to a host plugin, but no longer the headline everything is
> shaped to fit: it follows the field-graph + standalone raymarch app, and its
> surface is **narrowed to batch proxy-mesh + export-mesh generation** (it never
> feeds a real-time viewport — a host side-car does). It wraps the same field-graph
> builder (`examples/field_graph.{h,cpp}`), so the ABI and the CLI share one
> parser. The live-viewport ambitions are dropped.

### Phase 1 — realized (2026-06-17): `capi/` host-side shared library

The C ABI shipped as a host-side **shared library** (`dualc_capi`, opt-in
`-DDUALC_BUILD_C_ABI=ON`) — **not** the originally-sketched `src/c_abi/` +
`include/dualc_c.h` placement. **Why the deviation:** the re-scope (build a field
from a graph string + export files) makes the ABI depend on
`examples/field_graph` (vendors nlohmann/json) **and** `examples/example_common`
(vendors miniz) — both deliberately *outside* `libdualc` by the library-scope
rule. So the ABI lives in `capi/` and *links* those host-side static libs +
`libdualc`; it cannot sit in `src/`. The surface is **11 flat entry points** (at 0.3.0 —
9 at 0.2.0, 13 since 0.4.0; [14/04](14-c-abi/04-abi-0-4-0.md)) (the
field-graph string *is* the construction API, so no per-primitive factory wall); the
names and the contract: [14 § 3](14-c-abi/02-surface-and-contract.md#3-the-surface--the-graph-string-is-the-construction-api).

> **Full development record + complete API reference, build/deployment, the
> verification record, and the deferred list: [14-c-abi/](14-c-abi/README.md).** The
> C# P/Invoke wrapper that consumes these entry points is now a Boletus
> deliverable — see [15-boletus-handoff.md](15-boletus-handoff.md).

**In one line each:** `dualc_field_contour` *is* the proxy lever (coarse
`maxDepth` = cheap proxy, full = export-grade); mesh output is ABI-allocated /
caller-released (`dualc_mesh_release`, no cross-heap hazard); no C++ exception
crosses the boundary (integer codes + an `err` buffer with a JSON-pointer /
char-offset locator); the DLL is self-contained (only the VC++ runtime + UCRT).
**Verified:** 11 unmangled `extern "C"` exports; STL byte-identical (same SHA-1) to
`dualc_field`; analytic + `mesh(path=…)` + in-memory `mesh(id=…)` paths all covered
(`cli_c_abi`, `cli_c_abi_mesh`, `cli_c_abi_mesh_inmem`); default `OFF` build
unperturbed. **In-memory mesh-source resolver DONE (v0.3.0, 2026-06-19):** a host
with geometry in RAM passes `DualcMeshSource` buffers to the `*_with_meshes` create
calls and references them by id (`mesh(id="…")`/`winding(id="…")`) — byte-identical
to the disk path, no temp file. This was the key Rhino constraint; now lifted.
*(2026-09-18: the counts, from `capi/dualc_c.h` at each commit — **9** at 0.2.0
(`3287754`, 2026-06-17, what this entry's verification run saw), **11** at 0.3.0
(`e345bf3`, 2026-06-19, the `*_with_meshes` twins), **13** at 0.4.0 (`ebe929a`); the
list above is the 0.3.0 surface, edited in place when the twins landed.)*
**Deferred:** 3MF-streaming behind the export calls. *(The C# wrapper + `.gha` —
formerly "Phase 2" here — moved to Boletus; see
[15-boletus-handoff.md](15-boletus-handoff.md).)* *(2026-09-18: trigger for the
3MF-streaming deferral written as row D-21 of the [decisions index](../decisions/README.md).)*

The Tier 3 integration deliverable: a narrow, stable C ABI around DualC's C++ public
surface (opaque handles + flat structs + integer error codes), a thin shim over the
existing C++ API. Why a C ABI rather than direct P/Invoke against the C++ API, and why it
is also the right shape for every other external consumer:
[14 § 1](14-c-abi/01-design.md#1-why-it-exists-and-why-it-landed-when-it-did). *(2026-09-18:
the paragraph that stood here was a verbatim copy of 14 § 1; the original entry's API
sketch — per-primitive factory calls in an `include/dualc_c.h` — never shipped (the graph
string is the construction API, [14 § 3](14-c-abi/02-surface-and-contract.md#3-the-surface--the-graph-string-is-the-construction-api))
and was deleted; the C#-side marshaling patterns and the Phase-2 `.gha` plan moved with
the wrapper work to [15](15-boletus-handoff.md).)*

**Status.** The C ABI itself (scoped to proxy + export) **DONE — 2026-06-17** (see
"Phase 1 — realized" above; it landed as a host-side `capi/` shared lib, ahead of any
Rhino side-car since `dualc_field_view` already covers standalone live preview). The C#
P/Invoke wrapper + the separate-repo `.gha` Grasshopper plugin are now **Boletus**
deliverables ([15](15-boletus-handoff.md)). Supersedes the dominant role of #9.

## 8. install() rules and dualc::dualc ALIAS target. [PHASE 1 DONE — 2026-05-26; PHASE 2 DEFERRED]

so other CMake projects can find_package(dualc REQUIRED) instead of
add_subdirectory-ing. **Phase 1 (done).** Top-level `CMakeLists.txt` now declares
`project(dualc VERSION 0.2.0 LANGUAGES CXX)`, exposes the namespaced
`add_library(dualc::dualc ALIAS dualc)`, and adds `$<INSTALL_INTERFACE:include>` to the
public include path so future install rules slot in without revisiting the
target_include_directories block. `examples/CMakeLists.txt` and `tests/CMakeLists.txt`
now link via `dualc::dualc` so the ALIAS is exercised by every internal build — any
regression that breaks it is caught locally, not by a downstream user. `README.md`
documents the supported `add_subdirectory(...)` + `target_link_libraries(... PRIVATE
dualc::dualc)` consumption pattern. **Phase 2 (deferred).** Full `install()` rules +
`dualcConfig.cmake` + `dualcConfigVersion.cmake` so consumers can `find_package(dualc
REQUIRED)` against an installed copy. **Why deferred.** Two reasons. (a) The realistic
consumption pattern under the planned roadmap (DualC becomes a public library with
geometry-central as a git submodule) is source-tree `add_subdirectory`, which Phase 1
already supports cleanly — there is no concrete consumer for a precompiled DualC
distribution yet. (b) geometry-central ships no `geometrycentralConfig.cmake` and no
namespaced ALIAS (verified in `D:\geometry-central\CMakeLists.txt`), so any
`dualcConfig.cmake` calling `find_dependency(geometrycentral)` cannot work end-to-end
without either upstream GC patches or a DualC-shipped `FindGeometryCentral.cmake` Find
module — both add infrastructure ahead of a real consumer. **Trigger to revisit.** A
concrete precompiled-distribution use case (vcpkg/Conan/CI artifact) OR geometry-central
upstream gaining its own Config. *(2026-09-20: Phase 1 is `aab403b`, 2026-05-26.)*

*2026-09-21 — trigger (b) fired.* Upstream geometry-central v1.1.0 ships a `GeometryCentralConfig.cmake`
and a namespaced export (`geometry-central::geometry-central`); the "verified in
`D:\geometry-central`" above had been checked against the owner's fork, 19 commits behind.
DualC now pins and fetches that upstream commit ([20 #47](20-public-delivery.md#47-pin-geometry-central-to-upstream-own-nanort-self-bootstrapping-clone)).
Phase 2 stays deferred on trigger (a) alone: a `find_package(GeometryCentral)` rung and
`dualcConfig.cmake` are additive to the resolver that landed, and no precompiled consumer
exists yet. Note the target-name split when it comes: `geometry-central` in a build tree,
`geometry-central::geometry-central` from an installed package.

## 9. Python bindings (pybind11). [DEFERRED — 2026-05-31]

Skipped after the deployment-target decision: Rhino/Grasshopper is the first integration
target (see #19) and three.js is the second -- neither benefits from CPython bindings
(three.js never sees CPython; Rhino 8's Grasshopper Python is CPython 3 but cannot
reliably load native pybind11 wheels into the Rhino process, and Rhino's native
integration path is C# via RhinoCommon). The narrow C ABI from #19 is also the right
foundation for any future Python binding (`ctypes`/`cffi` over the C ABI), so this is
deferred-with-prerequisite rather than dropped. **Trigger to revisit.** A Python-native
consumer materializes (Jupyter notebook workflow, GH Python shellout pattern that wants
to skip the OBJ round-trip, batch processing script) after #19 lands -- at which point
bindings are a thin layer over the existing C ABI rather than a parallel pybind11
surface.

## 11. Benchmark suite + perf regression tracking. [DEFERRED — 2026-05-31]

Defensive infrastructure; deferred until a concrete trigger fires. **Trigger to
revisit.** Any of: (a) a real perf complaint from a deployed plugin (Rhino `.gha`
latency on a slider drag, three.js latency in the browser); (b) work that depends on
honest before/after measurements -- most directly the Lipschitz `cellOverlaps`
optimization for TPMS (#18), but also any future contourer/sampler internal tweak; (c)
maintenance discipline before a 1.0 cut. Original scope unchanged: pin runtimes for
cube/sphere/molde at depths 5/7/9 and catch accidental slowdowns when changes land.

## 12. More demo data. [DONE — 2026-06-01]

a sphere (validates curvature handling, Euler χ = 2), a torus (Euler χ = 0, validates non-zero genus), a hand or skull (non-trivial curvature), a CAD part with sharp edges (validates the sharp-feature toggle). **Cheap follow-up to #10**: with Polyscope wired up, each new demo mesh becomes immediately useful for visual QA (octree refinement, sign classification, contour quality side-by-side). Delivered a richer-than-asked palette of **procedural** demo meshes plus one downloaded organic scan. New generator CLI `examples/dualc_gen_demo` (subcommands `sphere|uvsphere|torus|knot|genus2|cylinder|bracket|hexbore|all`) backed by reusable builders in `examples/demo_meshes.{h,cpp}` (a `dualc_demo_meshes` static lib); both live in `examples/`, the library is untouched (mesh generation is host scope, like mesh I/O). geometry-central ships no primitive generators, so each shape builds positions+faces → `makeSurfaceMeshAndGeometry`; `genus2` is the union of two coplanar tori merged in one neck, contoured by DualC itself. The eight `.obj` are written to `data/` and copied next to every example binary by a `DEMO_MESHES` POST_BUILD loop. Note the repo gitignores `*.obj` (even `cube.obj`/`molde.obj` are working-tree-only), so the meshes are NOT version-controlled — the committed generator is the source of truth; run `dualc_gen_demo all --dir data` after checkout to populate them. *(2026-09-21: the build runs the generator itself into `build/data/` — [20 #47](20-public-delivery.md).)* **Meshes + regimes:** icosphere & uv-sphere (χ=2; ico uniform, uv exposes pole slivers), torus (χ=0, genus 1), trefoil-knot tube (χ=0, genus 1, near-approaching strands — the licensing-clean stand-in for "hand/skull", chosen for varying curvature), genus-2 double torus (χ=−2), cylinder (χ=2, sharp rims), **L-bracket (χ=2, BOTH convex 90° and concave 270° sharp edges — the `--sharp` validator)**, hex prism + bore (χ=0, genus 1). Plus `data/bunny.obj` (Stanford bunny, an open scan — attribution in `THIRD_PARTY.md`; sealed via `--gwn-field`). **Tests:** `tests/test_demo_meshes.cpp` contours each procedural mesh and asserts 0 boundary edges + the exact Euler χ (2/0/−2) at a per-mesh depth pinned in comments; new `cli_gen_demo`, `cli_demo_sphere`, `cli_demo_torus`, `cli_demo_bracket_sharp` smoke tests. Full suite green (153 ctest cases). **One non-obvious fix landed:** the L-cap is a non-convex hexagon, and geometry-central fan-triangulates a polygon face from its first listed vertex — emitting the back cap in reversed order put the fan apex on a reflex-blocked vertex, so its triangles crossed the notch (boundary edges that *grew* with depth, the malformed-input signature). Caps are now triangulated explicitly from the origin corner, which sees the whole star-shaped L. **Why procedural (not licensing):** Giacomo confirmed that permissive-only is a preference, not a hard rule (and approved the downloaded bunny); procedural generation was kept for engineering reasons — clean topology, exact χ, regeneratable, tunable, no large binaries. **Known follow-up:** the genus-2 source is built by DualC's own contourer, so its committed mesh is a DC output (depth 6, ~22k verts) rather than a parametric grid; fine for a demo but not a hand-authored manifold. Full generator docs in [`../command_reference/09-dualc_gen_demo.md`](../command_reference/09-dualc_gen_demo.md); palette table in `README.md`. Tools count Part I bumped 7 → 8.

---

← Back to the [Roadmap index](README.md).
