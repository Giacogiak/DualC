# DualCEditor hand-off — the standalone-app UI (archived from DualC)

**What this file is.** The standalone-app UI that DualC's roadmap planned as its Track 2 now
lives in its own project, **DualCEditor** (<https://github.com/Giacogiak/DualCEditor>; a
local clone at `~/Documents/DualCEditor`). It is a Qt 6 editor that links DualC and was
built in October 2026. The items that crossed the repo line are indexed here, as
[15](../15-boletus-handoff.md) did for Boletus. Their original DualC intent is kept verbatim
in [01](01-archived-intent.md) and [02](02-archived-intent-continued.md); the topic files
keep a one-line pointer back here. Born 2026-10-21 from the owner's decision of 2026-10-19,
[D-66](../../decisions/01-settled.md) ([raw plan § 1](../../raw/2026-10-19-hygiene-and-editor-plan.md#1-the-owners-decisions)).
Every editor fact below was read in the editor's repo at `74023cf` (its `main`, 46 commits,
2026-10-09 … 10-15) and cites a commit or a file there.

**The boundary rule.** DualC owns the engine, the field graph and the GLSL codegen libraries
the editor links, the C ABI, and `dualc_field_view` as the reference viewer:

- **The libraries.** The editor links `dualc_examples_fieldglsl`, which brings
  `dualc_examples_fieldgraph`, `dualc_examples_common` and `dualc::dualc` ([E-06]; its
  [`CMakeLists.txt`][cmake] `:109-111`). It fetches them at a pinned DualC commit,
  `2fcb6ee` (`:24`), not through the C ABI.
- **The reference viewer.** `dualc_field_view` keeps its keyboard section planes, its
  `[` / `]` uniform nudge and its file-watch, and stays the binary Boletus's side-car runs
  ([15](../15-boletus-handoff.md)).

DualCEditor owns every interactive UI concern: the scene, panels, picking, gizmos, sections,
parameter editing, mesh import, the export dialog ([E-05]: "Mesh I/O, scene management,
transforms and UI live in the editor").

## Hand-off table — moved to DualCEditor

| DualC origin | What it was | DualCEditor home | Status |
| --- | --- | --- | --- |
| [D-17](../../decisions/README.md), [12/03 § Section planes](../12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection) | Section-plane UI sliders over `uClip[3]` / `uClipMask` | Commit [`1b59389`][c-1b5] (2026-10-14): `X` / `Y` / `Z` toggles and `−` / `+` buttons in a toolbar and `View ▸ Section`, on the same uniforms ([`trace_renderer.cpp`][tr] `:150-166`); usage in [cmd-ref § Section][cr-sec] | DONE in the editor, 2026-10-14 |
| [D-25](../../decisions/README.md), [12/03 § Parameter push](../12-field-graph-and-app/03-raymarch-app.md#real-time-parameter-push-uniform-ipc-channel) | Parameter edits as uniforms, with no recompile | The Parameters dock, commit [`2676c6a`][c-267] (P00 T4, 2026-10-10): `setBinding` is one `glUniform`, no recompile ([`trace_renderer.cpp`][tr] `:284-292`). Since [`95e1a8d`][c-95e] (2026-10-14) the dock follows the selection. Usage in [cmd-ref § Parameters][cr-par] | DONE in the editor, 2026-10-10 |
| [D-16](../../decisions/README.md), [12/05 § F › Clip mask](../12-field-graph-and-app/05-open-surface.md#deferred-optional--render-time-clip-mask-isolated-open-surface-preview) | Render-time clip mask: an open surface masked by a second field | Moved; not started there. No editor page names it at `74023cf` | moved — no editor plan yet |
| Track 2 item 5 ([08](../08-raymarch.md), [12](../12-field-graph-and-app/README.md)) | Web build (browser shell) on the WebGL2-subset shader | Moved; not started there. The editor is a Qt Widgets desktop app ([E-01]) | moved — no editor plan yet |
| — | Any future UI item | DualCEditor's roadmap | moves on arrival |

Two differences from what DualC planned, kept so nobody reads more into the DONE rows:

- **D-17 asked for sliders.** The editor shipped one cut at a time, normal to one axis,
  stepped by a twentieth of the trace box. DualC's viewers allow three planes at once. The
  cut is viewing only in both ([cmd-ref § Section][cr-sec]).
- **D-25 was a channel to push uniforms into `dualc_field_view`, driven by Boletus.** The
  editor edits uniforms inside its own process, so no channel exists. Boletus's side-car
  still drives `dualc_field_view` through the file-watch, which recompiles on every change.
  A channel that Boletus needs would be a new item; this hand-off does not carry it.

## DualC retains — editor-driven

These stay DualC's to do. The editor is the reason to do them, not the owner.

| Item | Lives in | Editor relationship |
| --- | --- | --- |
| Baked-preview hardening: a conservative baked far field (D-31), a mesh-aware `nodeFeatureScale` (D-30), bake and compile startup (D-29) | [12/07 § H](../12-field-graph-and-app/07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep), [12/04 § Startup](../12-field-graph-and-app/04-preview-performance.md#viewer-startup--shader-compile-time), [decisions](../../decisions/README.md) | The codegen side stays here. The editor compiles on a shared-context thread ([E-21], [`4b89008`][c-4b8]), which hides the D-29 stall but does not shorten it. It also defers [E-22], freezing an object to a baked grid |
| Promote the graph and codegen to a supported library | `examples/field_graph.*`, `examples/field_glsl.*` | The editor's [E-20], deferred there. Trigger: DualC changes their interface under the editor, or a third client needs them |
| The `vec2` binding type | [21/09](../21-cli-consolidation/09-plan-run.md#the-consolidation-plans-run--p01-to-p07-harvested) | DualC emits it since #58. The editor's `setBinding` uploads only `float`, `vec3` and `mat4` ([`trace_renderer.cpp`][tr] `:286-292`), so a re-pin past `2fcb6ee` reads a profile's 2D points as zero. The fix is the editor's; DualC keeps the binding stable |
| The editor's questions to DualC, Q1–Q9 | Its [`06-dualc-questions.md`][qs] | None raised in writing (its status, 2026-10-14), no DualC PR. Q1 `id=` in the mesh codegen; Q2 a parameter registry; Q3 `Diagnostics*` on the tiled writers; Q4 a public RAM estimate; Q5 per-node feature scale; Q6 a binding written back to a node; Q7 an OBJ-reader failure; Q8 a colour hook in the trace framework; Q9 a GPU id buffer |
| DAG references (`id` / `ref`) | [12 § A](../12-field-graph-and-app/01-field-graph.md#a-the-field-graph-keystone), D-12 | The editor's [E-24] cites D-12. Boletus drives it too ([15](../15-boletus-handoff.md)) |

## How the block was opened

Unit P07.1 of the hygiene-and-editor plan, 2026-10-21, on branch `feat/hygiene-and-editor`.
**Chosen in-run (pending owner review):**

- **A folder, not one file.** The verbatim prose alone is about 16 KB, over the page cap, so
  it is split across two pages.
- **More archived than the plan listed.** The whole Track 2 section and the three decision
  rows are quoted too, so that the stubs and re-stated rows P07.2 writes lose nothing.
- **Reading D-25 as delivered.** The owner's D-66 says the parameter push was delivered in
  the editor. The editor has no channel; the second note under the hand-off table records
  that difference.

## The pages of this block

| Page | Holds |
| --- | --- |
| [01](01-archived-intent.md) | Archived intent (1): 12/03 — the app's portable aim, the section-plane sliders, the parameter push |
| [02](02-archived-intent-continued.md) | Archived intent (2): the clip mask, the Web build, Track 2 as it stood, decision rows D-16 / D-17 / D-25 |

[E-01]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/decisions/01-settled.md#e-01--qt-6-widgets-for-the-ui
[E-05]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/decisions/01-settled.md#e-05--dualcs-library-scope-holds
[E-06]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/decisions/01-settled.md#e-06--consume-dualcs-host-side-libraries-by-pin
[E-20]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/decisions/03-deferred.md#e-20--promote-dualcs-graph-and-codegen-into-a-supported-library
[E-21]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/decisions/03-deferred.md#e-21--asynchronous-shader-compile-and-a-program-cache
[E-22]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/decisions/03-deferred.md#e-22--freeze-an-object-to-a-baked-grid
[E-24]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/decisions/03-deferred.md#e-24--reuse-of-one-object-in-several-operations-dag-references
[cmake]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/CMakeLists.txt
[tr]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/src/trace_renderer.cpp
[qs]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/roadmap/06-dualc-questions.md
[cr-sec]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/command_reference/01-viewport.md#section
[cr-par]: https://github.com/Giacogiak/DualCEditor/blob/74023cf4e88d2696db8bd7a30eeb25dca5cd2eaf/docs/command_reference/01-viewport.md#parameters
[c-1b5]: https://github.com/Giacogiak/DualCEditor/commit/1b59389700b1fe25fe2cbe99e3b291cf24b3d8da
[c-267]: https://github.com/Giacogiak/DualCEditor/commit/2676c6ae80a15feb2c4f25abe2aaab0ffef48bb4
[c-95e]: https://github.com/Giacogiak/DualCEditor/commit/95e1a8d212c71f07fe071509e01a85b31d65a17b
[c-4b8]: https://github.com/Giacogiak/DualCEditor/commit/4b89008c6f96a8936ef3adac9fd357e9c3b800b8

---

← Back to the [Roadmap index](../README.md).
