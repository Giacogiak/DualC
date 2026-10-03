# Field-graph & standalone raymarch app

> The strategic re-evaluation (2026-06-12) of how DualC delivers value to
> engineers, and the build sequence it produced. Spun out of the main
> [roadmap](../README.md) because it re-prioritizes several blocks and introduces a
> new keystone — the composable **field-graph** — that unifies the CLI, the GPU
> viewer, and the eventual C ABI. Cross-references the
> [dense-lattice deliverable](../11-dense-lattice-deliverable/README.md) (the Phase 0
> benchmark + streaming export), the [raymarch viewer](../08-raymarch.md) (#3, the
> GPU sphere-tracer this generalizes), and
> [infrastructure](../10-infrastructure-and-integration.md) #19 (the C ABI, now
> re-sequenced behind value validation).
>
> 🍄 **Moved to Boletus.** The **Rhino side-car** that originally lived here (§E),
> plus the Rhino/Grasshopper plugin work, are now owned by the Boletus project
> (`D:\Boletus`) — see [15-boletus-handoff.md](../15-boletus-handoff.md). This page
> keeps the field-graph, GLSL codegen, and standalone app — DualC's own engine.

## The re-evaluation
**2026-06-12.**

The prior headline was the C ABI + Rhino plugin (#19). Re-examined against the
**engineer's actual workflow** it was mis-sequenced. The *value* —
(1) generate a lattice from a volume, (2) see and edit it with fast feedback,
(3) boolean it against other volumes (watertight bodies *or* mesh soups),
(4) export to STL/3MF — was not yet validated *as one performant command-line
piece*, and the Rhino path explicitly substituted coarse LOD meshes for the GPU
raymarch that is the real visualization edge (the Polyscope viewer re-meshes on
every edit and cannot keep up).

**Decision: validate and ship the value from the command line first, then wrap
it.** Build the standalone GPU app (raymarch leads), authored portably so Web
becomes a cheap shell-swap and Rhino a C-ABI wrap later.

The [Phase 0 benchmark](../11-dense-lattice-deliverable/02-phase-0-benchmark.md#phase-0-benchmark)
made the case concrete: one boolean over a lattice *mesh* costs **10–49 min**; the
same composition as a *field* is **one ~40 s contour**. That is the field-graph's
reason to exist.

## The new sequence (value-first, then the C ABI)

1. **Field-graph + `dualc_field`** — field-level composition; kills the boolean
   mesh round-trip (pillar #3). **#1 build — DONE (JSON core, 2026-06-14).**
2. **Field→GLSL codegen + standalone raymarch app** — general, live-editable,
   portable preview (pillar #2). **#2 build — DONE (2026-06-14).**
3. **Streaming / slice-based export** — break the base RAM wall for dense parts
   (pillar #4). **#3 build — DONE** (STL 2026-06-15, 3MF 2026-07-03): `dualc_field
   --tile-depth` contours in uniform grid-aligned tiles and streams the mesh at
   one-tile peak RAM, identical to the monolithic mesh — the measurements, and the
   follow-ons that shipped later (seam-welding, `--mem`) or stayed deferred, are in
   [11 § 5](../11-dense-lattice-deliverable/03-streaming-export.md).
4. **C ABI (#19), re-scoped** — to batch proxy-mesh + export-mesh generation; a
   host side-car handles real-time visualization, so the ABI never feeds a live
   viewport. **DONE 2026-06-17** ([14-c-abi/](../14-c-abi/README.md)).
5. **Rhino side-car** — the plugin launches the standalone app and pushes the
   field-graph over IPC; Rhino shows an LOD proxy + exports via the C ABI. 🍄 Moved
   to Boletus with the plugin around it ([15](../15-boletus-handoff.md), Phase 5).

## The pages of this topic

Split on 2026-09-11 from the single file (the record is verbatim; only status moved out
of headings); § H joined on 2026-09-18 from the retired continuation file 16. Section
letters (§ A–§ H) are the stable addresses.

| Page | Sections |
| --- | --- |
| [Field-graph](01-field-graph.md) | [Field-graph (§ A)](01-field-graph.md#a-the-field-graph-keystone) and [the `dualc_field` CLI (§ C)](01-field-graph.md#c-dualc_field-cli) |
| [Field→GLSL codegen contract](02-glsl-codegen.md) | [Field→GLSL codegen contract (§ B)](02-glsl-codegen.md#b-fieldglsl-codegen-contract) |
| [Standalone raymarch app](03-raymarch-app.md) | [Standalone raymarch app (§ D)](03-raymarch-app.md#d-standalone-raymarch-app): the viewer, [section planes (§ D.1)](03-raymarch-app.md#section-planes-viewport-inspection), [file-watch](03-raymarch-app.md#disk-file-watch-auto-reload), [parameter push](03-raymarch-app.md#real-time-parameter-push-uniform-ipc-channel) (D-25; its Boletus-side write-up is Boletus 07/02 § 6) |
| [Viewer performance](04-preview-performance.md) | [Viewer performance (§ D.2)](04-preview-performance.md#d2-viewer-interactive-performance): [adaptive preview + metric fast path](04-preview-performance.md#adaptive-resolution-preview--metric-sdf-fast-path), [GPU selection](04-preview-performance.md#discrete-gpu-auto-selection-hybrid-graphics-laptops), [startup](04-preview-performance.md#viewer-startup--shader-compile-time) |
| [Isolating the open lattice surface](05-open-surface.md) | [Isolating the open lattice surface (§ F)](05-open-surface.md#f-isolating-the-open-lattice-surface--validated-workflow--deferred-clip-mask): [the thin-shell workflow](05-open-surface.md#validated-thin-shell-workflow-no-new-code), [the deferred clip mask](05-open-surface.md#deferred-optional--render-time-clip-mask-isolated-open-surface-preview) |
| [Post-contour QEM decimation](06-decimation.md) | [Post-contour QEM decimation (§ G)](06-decimation.md#g-post-contour-qem-decimation---decimate----simplify) |
| [Mesh-preview correctness sweep](07-mesh-preview-sweep.md) | [Mesh-preview correctness sweep (§ H)](07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep): [the metric fast path over baked grids (H1)](07-mesh-preview-sweep.md#h1-the-metric-fast-path-traced-a-field-that-is-not-a-distance-field), [the sign flood (H2)](07-mesh-preview-sweep.md#h2-the-bakes-far-field-sign-flood-assumed-a-padded-region), [the dead `--grid-res` (H3)](07-mesh-preview-sweep.md#h3---grid-res-was-dead), [the parity mesh cases (H4)](07-mesh-preview-sweep.md#h4-nothing-gated-any-of-it); three deferred follow-ups |
| [Inspection report — gyroid shell](08-quality-inspection-gyroid-shell/README.md) | The 2026-07-04 contouring-quality inspection behind § G: test case, root cause, measurements, the decimation approaches |

## E. Rhino side-car
🍄 **Moved to Boletus** (Phase 5, PLANNED) — the original DualC intent and the record:
[15 § From 12 §E](../15-boletus-handoff.md#from-12-e--rhino-side-car). Its C-ABI
prerequisite stayed here and is **DONE** ([14](../14-c-abi/README.md)); `dualc_field_view`
is the viewer binary it launches.

## Status summary

| Item | Deliverable | Status |
|---|---|---|
| A | Field-graph (node set, JSON + shorthand, deduped DAG) | **JSON node set + text shorthand DONE** (2026-06-14); DAG-refs DEFERRED ([D-12](../../decisions/README.md)) |
| B | Field→GLSL codegen contract | **DONE** (2026-06-14) — the v1 node slice; DAG-dedup emission, the `winding` node, the domain ops and the primitive long tail DONE 2026-06-24 ([§ B](02-glsl-codegen.md#b-fieldglsl-codegen-contract)) |
| C | `dualc_field` CLI + `examples/field_graph.{h,cpp}` | **DONE** (2026-06-14): JSON core + `--expr`/`--dump-json`/`--list` |
| D | Standalone raymarch app (portable) | **DONE** (2026-06-14): `dualc_field_view` on the shared `raymarch_gl` stack; WebGL2-subset shader |
| D.1 | Viewport section planes (solid-body inspection) | **DONE** (2026-06-16, `f19baa0`) — both GPU viewers, visualization-only ([§ D › Section planes](03-raymarch-app.md#section-planes-viewport-inspection)); UI sliders PLANNED ([D-17](../../decisions/README.md)) |
| D.2 | Viewer interactive performance | **DONE** (2026-07-09) — metric fast path, adaptive render-scale, discrete-GPU selection ([§ D.2](04-preview-performance.md#d2-viewer-interactive-performance)); startup / shader-compile time DEFERRED ([§ D.2 › Startup](04-preview-performance.md#viewer-startup--shader-compile-time), [D-29](../../decisions/README.md)) |
| — | **C ABI (proxy + export)** — the plugin bridge ([10 #19](../10-infrastructure-and-integration.md#phase-1--realized-2026-06-17-capi-host-side-shared-library)) | **DONE** (2026-06-17) — host-side `capi/dualc_capi` ([14](../14-c-abi/README.md)); in-memory mesh-source resolver DONE (2026-06-19, [14 § 9](../14-c-abi/README.md#9-deferred-explicitly-not-in-this-version)); C# wrapper → **Boletus** ([15](../15-boletus-handoff.md)) |
| E | Rhino side-car | 🍄 **moved to Boletus** (Phase 5, PLANNED) — [15](../15-boletus-handoff.md) |
| F | Open-lattice isolation (thin-shell workflow) | **DONE** (2026-06-16, validated with existing ops); render-time clip mask **DEFERRED** ([D-16](../../decisions/README.md)) |
| G | Post-contour QEM decimation (`--decimate`/`--simplify`) | **Approach A (monolithic) DONE** (2026-07-05) — [§ G](06-decimation.md#g-post-contour-qem-decimation---decimate----simplify), evidence in the [inspection report](08-quality-inspection-gyroid-shell/README.md); per-tile Approach B DEFERRED ([D-26](../../decisions/README.md)) |

~~One build-time check carried forward: verify `MeshSource` honors the `gwn` sign
mode (its ctor doc names only parity/pseudonormal); if not, the `mesh` node drops
`"gwn"` and the `winding` node covers GWN.~~ **Resolved 2026-06-14:** `MeshSource`
has no GWN ctor, so the `mesh` node accepts only `parity`/`pseudonormal` and
`gwn` is rejected with a hint to use the `winding` node.

---

← Back to the [Roadmap index](../README.md).
