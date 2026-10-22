# Standalone raymarch app (§ D): the viewer, section planes, file-watch

Part of [12 — Field-graph & standalone raymarch app](README.md). Usage:
[command_reference/12](../../command_reference/12-dualc_field_view/README.md).

## D. Standalone raymarch app
**DONE (2026-06-14).**


The GPU visualizer built on the [raymarch](../08-raymarch.md) GL stack + the field→GLSL
codegen, authored to the **WebGL2-portable subset** so the same engine serves the
shell-CLI, the Rhino side-car, and (cheaply, later) a Web build. Live editing of an
arbitrary composed field — the thing the Polyscope viewer cannot do at lattice
density. Why a standalone app and not in-Rhino: Rhino cannot host the raymarch
viewport (the reason the in-Rhino path fell back to LOD meshes); a host side-car
provides raymarch *beside* Rhino instead (now a Boletus deliverable —
[15-boletus-handoff.md](../15-boletus-handoff.md)). *(2026-10-21: the Web build and every
interactive UI item moved to DualCEditor — [22](../22-dualceditor-handoff/README.md).)*

**Implemented:** `examples/dualc_field_view.cpp` (opt-in
`-DDUALC_BUILD_FIELD_VIEW=ON`). Loads a `.fld`/`.json`/stdin/`--expr` graph
(reusing the `dualc_field` parser + `FileMeshResolver`), `compileToGlsl`s it,
assembles `prelude + generated + trace framework`, uploads each `MeshTexture` to a
`sampler3D`, sets the binding uniforms + `uStepScale`, frames the field's bounds,
and orbit-raymarches it. **Two edit tiers** are wired: `[`/`]` nudge the selected
scalar binding via `glUniform` (no recompile, instant); `l` reloads the file and
recompiles (structural). A hidden `--snapshot PATH` renders one frame headless.
The GL plumbing (window/compile/snapshot/orbit camera) is factored into the shared
`examples/raymarch_gl.{h,cpp}`, now used by `dualc_raymarch` too (behavior
unchanged). **Verified:** clean MSVC build; full suite **171/171**; parity 28/28;
and `--snapshot` PNGs **inspected** for a composed analytic CSG (perforated cube),
a smooth union, a `mesh`-clipped lattice (the `sampler3D` path), and a twist warp
— all render correctly. The `--es` (`#version 300 es`) path also renders on the
desktop Intel driver (ARB_ES3_compatibility), confirming the body is in the
portable subset. Interactive orbit/edit needs a display (no CTest). Full page:
[command_reference/12](../../command_reference/12-dualc_field_view/README.md).

### Section planes (viewport inspection)
**DONE (2026-06-16).** Both GPU viewers (`dualc_field_view` and `dualc_raymarch`)
gained up to three axis-aligned section planes for inspecting a lattice interior
(the keys: [command_reference/12 › Section planes](../../command_reference/12-dualc_field_view/01-controls.md#section-planes-viewport-only-inspection)).
Controls are layout-independent — the rule and its reason are
[design/09 § Keyboard-layout independence](../../design/09-conventions.md#keyboard-layout-independence).
Implemented as
half-spaces intersected with the body (`max(sceneSDF, dot(n,p)+d)`) inside
`kTraceFramework` / the `dualc_raymarch` shader, driven by `uClip[3]`/`uClipMask`
(uploaded each frame from the bounds, so a structural `l` reload keeps the cut);
the slice face is a warm-tinted **capped** section. This is **visualization only**
— the clip lives entirely in the trace shader, never in the codegen, the binding
table, the parity value-shader, or the C++ export path, so exported geometry is
unchanged and `uClipMask==0` is bit-identical to today *(2026-09-20: asserted from the
shader — the cut is a `max()` gated by `uClipMask` — not measured by a snapshot diff)*. This is distinct from the
still-deferred open-surface **clip mask** in §F (which masks an *open* surface by a
*second* field; this is half-space clipping of a *solid* body). The UI sliders
(D-17) moved to DualCEditor, which shipped a section tool on these uniforms —
[22](../22-dualceditor-handoff/README.md).

### Disk file-watch (auto-reload)
**DONE (2026-07-03).** `dualc_field_view` now polls
its input file's mtime (~7 Hz, in the render loop) and triggers the existing
structural reload on any change — so editing the `.fld`/`.json` and saving updates
the window with **no `l` keypress** (manual `l` still works; `--expr`/stdin have no
file to watch). On reload the field's **auto-bounds are recomputed** (unless
`--bounds` was pinned) so a size-changing edit keeps the raymarch box + section
planes framed; the camera is kept. **Why (the reason a future agent will look for):**
this is the mechanism the **Boletus Rhino side-car drives** — Boletus **Phase 5b**
([15-boletus-handoff.md](../15-boletus-handoff.md); Boletus
`docs/roadmap/07-upstream-coordination.md §5`). The plugin rewrites the graph file on
every Grasshopper solve and the viewer live-reloads it, giving GH-parametric live
preview **without an IPC channel**. Pushing a *parameter* edit as a uniform-only
update (the instant `[`/`]` tier) would still need a real command channel and stays
deferred. Change is contained to `examples/dualc_field_view.cpp` (`<filesystem>`
mtime poll + bounds-recompute in the reload block); `libdualc` untouched; no CTest
(GL). Only Boletus re-vendors the rebuilt exe.

### Real-time parameter push (uniform IPC channel)
Moved to DualCEditor (D-25), its parameter edits uniform-only there; the intent, verbatim,
and how the delivered item differs: [22](../22-dualceditor-handoff/README.md).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
