# Archived intent (1) — 12/03: the app's aim, section planes, parameter push

Part of [22 — DualCEditor hand-off](README.md). This page and
[02](02-archived-intent-continued.md) hold the text lifted from the DualC topic files on
2026-10-21, at `3a72cd8`, when the standalone-app UI moved to DualCEditor
([D-66](../../decisions/01-settled.md)). They record why DualC planned each item and the
shape it expected. The prose is verbatim; only the link targets were re-based so they still
resolve from this folder. For what the editor built and where, follow the
[hand-off table](README.md#hand-off-table--moved-to-dualceditor).

## The moved items, as DualC wrote them

### From [12/03 § D](../12-field-graph-and-app/03-raymarch-app.md#d-standalone-raymarch-app) — the app's portable aim

The § D intro, which promised the Web build. The viewer it describes stays DualC's:

> The GPU visualizer built on the [raymarch](../08-raymarch.md) GL stack + the field→GLSL
> codegen, authored to the **WebGL2-portable subset** so the same engine serves the
> shell-CLI, the Rhino side-car, and (cheaply, later) a Web build. Live editing of an
> arbitrary composed field — the thing the Polyscope viewer cannot do at lattice
> density. Why a standalone app and not in-Rhino: Rhino cannot host the raymarch
> viewport (the reason the in-Rhino path fell back to LOD meshes); a host side-car
> provides raymarch *beside* Rhino instead (now a Boletus deliverable —
> [15-boletus-handoff.md](../15-boletus-handoff.md)).

### From [12/03 § D › Section planes](../12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection) — the UI sliders (D-17)

The whole subsection. Its keyboard planes in both DualC viewers stay DualC's; the "UI
sliders" it defers are the item that moved:

> **DONE (2026-06-16).** Both GPU viewers (`dualc_field_view` and `dualc_raymarch`)
> gained up to three axis-aligned section planes for inspecting a lattice interior
> (the keys: [command_reference/12 › Section planes](../../command_reference/12-dualc_field_view/01-controls.md#section-planes-viewport-only-inspection)).
> Controls are layout-independent — the rule and its reason are
> [design/09 § Keyboard-layout independence](../../design/09-conventions.md#keyboard-layout-independence).
> Implemented as
> half-spaces intersected with the body (`max(sceneSDF, dot(n,p)+d)`) inside
> `kTraceFramework` / the `dualc_raymarch` shader, driven by `uClip[3]`/`uClipMask`
> (uploaded each frame from the bounds, so a structural `l` reload keeps the cut);
> the slice face is a warm-tinted **capped** section. This is **visualization only**
> — the clip lives entirely in the trace shader, never in the codegen, the binding
> table, the parity value-shader, or the C++ export path, so exported geometry is
> unchanged and `uClipMask==0` is bit-identical to today *(2026-09-20: asserted from the
> shader — the cut is a `max()` gated by `uClipMask` — not measured by a snapshot diff)*. This is distinct from the
> still-deferred open-surface **clip mask** in §F (which masks an *open* surface by a
> *second* field; this is half-space clipping of a *solid* body). UI sliders are
> deferred to the public release — the uniforms are the hook a slider / the Rhino
> side-car drives. *(2026-09-18: trigger for the sliders is row D-17 of the
> [decisions index](../../decisions/README.md); 2026-10-03: the public release happened, the
> sliders are PLANNED.)*

### From [12/03 § D › Parameter push](../12-field-graph-and-app/03-raymarch-app.md#real-time-parameter-push-uniform-ipc-channel) — the uniform channel (D-25)

The whole subsection:

> **DEFERRED** — a performance optimization; trigger below. The file-watch above drives
> the **structural-reload** tier for
> *every* GH edit: it rewrites the file and the viewer **recompiles the whole shader**
> whether a node was added or a slider merely nudged. That is correct and universal,
> and it needed **no** new protocol — but each change pays a shader recompile
> (~tens-to-hundreds of ms by graph complexity). For the **number-only** case
> (dragging one knob on an existing node) that recompile is wasteful: the generated
> GLSL is identical, only a uniform moved. Recall the **two edit tiers** (§D above +
> [command_reference/12 › Why two tiers](../../command_reference/12-dualc_field_view/README.md)):
> `l` / file-watch = **recompile** (shape changed); `[`/`]` = **one `glUniform`, no
> recompile, instant** (a number changed). The deferred optimization is to let a host
> drive the *second* tier programmatically — a command channel carrying
> `set <paramKey> <value>` lines that map straight to `setBinding()`/`glUniform` with
> **no** recompile.
>
> - **Improvement:** latency only. Today a slider *scrub* updates in small hitches (a
>   recompile per step); with uniform push the lattice would **morph fluidly at frame
>   rate** — continuous, video-game-like. **Not** a capability the viewer lacks (every
>   field previews today); purely smoothness for continuous scrubbing.
> - **Why deferred:** needs a **real IPC channel** (e.g. `--command-port N` or a
>   non-blocking stdin protocol) on the viewer + a sender on the host — new plumbing
>   both sides, where the file-watch needed none. And it *only* helps scalar edits: a
>   structure change (add/remove node, swap op) **still** recompiles, so the host must
>   classify scalar-only vs. structural and route accordingly, keyed to the codegen's
>   binding-table `paramKey`s. Structure changes fall back to a `reload` command / the
>   file-watch.
> - **Ownership:** the channel lives in DualC (`dualc_field_view`); **Boletus is the
>   driver** (the reason to build it — its `Volume` diff classifies the edit and
>   sends). Companion write-up on the Boletus side, same detail:
>   [Boletus `docs/roadmap/07-upstream-coordination/02-viewer-and-uniform-push.md §6`](../../../Boletus/docs/roadmap/07-upstream-coordination/02-viewer-and-uniform-push.md#6-real-time-parameter-push-uniform-ipc-channel--deferred-performance-optimization).
> - **Trigger:** a workflow that leans on **continuous slider scrubbing** of a dense
>   lattice and finds the per-change recompile hitch limiting. Until then the
>   file-watch is sufficient.

---

← Back to the [block README](README.md) · the [Roadmap index](../README.md).
