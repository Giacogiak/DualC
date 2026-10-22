# Archived intent (2) — clip mask, Web build, Track 2, decision rows

Part of [22 — DualCEditor hand-off](README.md); the continuation of
[01](01-archived-intent.md), which says where and when the text was lifted. The prose is
verbatim; only the link targets were re-based.

## The moved items, continued

### From [12/05 § F › Clip mask](../12-field-graph-and-app/05-open-surface.md#deferred-optional--render-time-clip-mask-isolated-open-surface-preview) — the render-time clip mask (D-16)

The whole subsection. The validated thin-shell workflow earlier in § F stays DualC's:

> > **Not to be confused with the shipped section planes (D.1).** To inspect the
> > *interior of a solid* lattice, the viewers already have axis-aligned **section
> > planes** ([§ D › Section planes](../12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection);
> > keys in [command_reference/12](../../command_reference/12-dualc_field_view/01-controls.md#section-planes-viewport-only-inspection)).
> > The clip mask below is a *different* feature: it isolates a genuinely **open**
> > surface (a zero-thickness membrane) by masking it with a *second field*, where a
> > section plane is a half-space cut of a *solid*.
>
> **What it is.** A visualization-only capability for `dualc_field_view`: march a
> **surface field** (e.g. a bare `gyroid`, the `gyroid=0` membrane) and accept a hit
> only where a **separate clip field** is inside, otherwise keep marching. Exposed as a
> second field-graph, e.g. `dualc_field_view --expr "<surface>" --clip "<volume>"`. The
> clip volume can be any GLSL-compilable field (box/sphere/their booleans — exact) or a
> mesh (baked to the existing `sampler3D` path). It is the literal "render the surface,
> masked by the volume," with no field-algebra change.
>
> **What it adds beyond the shipped thin-shell approach.**
>
> 1. **True zero-thickness open surface** — renders the genuine single membrane as a
>    2-sided sheet with real open boundary edges where it is cut, not a thin *closed*
>    shell with an artificial ±t wall and tiny strut-end caps.
> 2. **Decouples "what to draw" from "where to draw it"** — today you must bake a
>    thickness into the field just to make the membrane hit-able and to suppress the
>    skin; the mask draws any field's isosurface inside any volume *without* altering
>    the field, so the preview is exactly the design surface.
> 3. **Inspection independent of wall thickness** — judge cell geometry / wavelength /
>    how the lattice meets the boundary before committing a wall thickness.
> 4. **Cleaner clipped preview against arbitrary volumes** — open cuts at the volume
>    surface, no tiny boundary caps, for any analytic or mesh clip region.
>
> **Why it is deferred / optional.** Every downstream manufacturing step (thicken →
> watertight export → boolean) requires a real thickness anyway, and the thin-shell
> preview is visually faithful for that workflow. The mask is a pure inspection nicety;
> it unlocks no new *export*. *(2026-09-18: trigger written as row D-16 of the
> [decisions index](../../decisions/README.md).)*
>
> **Implementation sketch (examples-layer only — no core-DC changes).**
>
> - `examples/field_glsl.{h,cpp}`: emit a second `clipSDF(p)` alongside `sceneSDF(p)`
>   from the `--clip` graph; add its uniforms / `MeshTexture` bakes to the binding table.
> - The `kTraceFramework` trace loop (`field_glsl.cpp`) + `examples/raymarch_gl`: at the
>   hit test (and the pre-loop entry-inside case), accept only when `clipSDF(hp) <= 0`;
>   on rejection **continue marching** past the crossing instead of `break`. Needs care
>   for a solid surface field (a bare gyroid's `<0` half-space — march out of the solid
>   before resuming the sphere-trace) and for bisection / normal estimation right at the
>   open boundary.
> - `examples/dualc_field_view.cpp`: add the `--clip` flag and wire the second compile.
> - `examples/dualc_glsl_parity.cpp`: a parity / `--snapshot` case for the masked render.
> - Cost: two SDF evaluations per step (surface + clip) where masking is active.
>
> **Relationship to `#13`/`#14` (roadmap 01).** Distinct — those are **core-DC mesh**
> features; the clip mask is **viewer-only**. If an open-surface *mesh* (not just a
> preview) is ever required, that is the `#14` "open-boundary" direction; how it would be
> realized (the Hermite-edge source tag) is on the item itself,
> [01 Tier-4 item 14](../01-core-dual-contouring/README.md#14-multi-material--open-boundary-contouring)
> — a separate, larger item, PLANNED without a trigger.

### From [08](../08-raymarch.md) § Generalized — the Web build

08 has no Web-build section; the intent is one clause of this paragraph:

> This hand-written single-field viewer is now the reference case for the
> **general** field→GLSL codegen
> ([12 §B/§D](../12-field-graph-and-app/02-glsl-codegen.md#b-fieldglsl-codegen-contract)):
> `dualc_field_view` compiles *any* composed field-graph to a generated
> `sceneSDF()` and sphere-traces it on the same GL stack. The GL plumbing
> (window/compile/snapshot/orbit camera) was factored into the shared
> `examples/raymarch_gl.{h,cpp}`, which `dualc_raymarch` now also uses (behavior
> unchanged). The general shader is authored to the **WebGL2 (`#version 300 es`)
> subset** (a `--es` flag selects the header; desktop gets `330 core`), so the same
> engine later serves a Web build and the Rhino side-car. `dualc_raymarch` stays as
> the tuned, dependency-light single-TPMS view; the formulas it pioneered are now
> shared library functions in the codegen prelude, held to the C++ originals by the
> `dualc_glsl_parity` gate.

### From [the block-12 README](../12-field-graph-and-app/README.md) — Web as a shell-swap

The block-12 decision paragraph, then its sequence-table rows for § D, § D.1 and § F:

> **Decision: validate and ship the value from the command line first, then wrap
> it.** Build the standalone GPU app (raymarch leads), authored portably so Web
> becomes a cheap shell-swap and Rhino a C-ABI wrap later.

> | Item | Deliverable | Status |
> |---|---|---|
> | D | Standalone raymarch app (portable) | **DONE** (2026-06-14): `dualc_field_view` on the shared `raymarch_gl` stack; WebGL2-subset shader |
> | D.1 | Viewport section planes (solid-body inspection) | **DONE** (2026-06-16, `f19baa0`) — both GPU viewers, visualization-only ([§ D › Section planes](../12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection)); UI sliders PLANNED ([D-17](../../decisions/README.md)) |
> | F | Open-lattice isolation (thin-shell workflow) | **DONE** (2026-06-16, validated with existing ops); render-time clip mask **DEFERRED** ([D-16](../../decisions/README.md)) |

### From [the roadmap README § Next up › Track 2](../README.md#track-2--standalone-raymarch-app) — the track as it stood

The whole track, items 1–5. Item 1 stays DualC's, editor-driven
([README § DualC retains](README.md#dualc-retains--editor-driven)); items 2–5 moved:

>
> Shipped baseline — codegen, `dualc_field_view`, section planes, file-watch, the thin-shell
> workflow, graded-onion; GLSL codegen completeness DONE 2026-06-24 —
> [12](../12-field-graph-and-app/README.md) § B–§ F.
>
> 1. **Baked-source preview hardening**
>    ([12/07 § H](../12-field-graph-and-app/07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep))
>    — the sweep's three open items, all DEFERRED: a conservative baked far field
>    ([D-31](../../decisions/README.md)), a mesh-aware `nodeFeatureScale`
>    ([D-30](../../decisions/README.md)), bake startup at high `--grid-res`
>    ([D-29](../../decisions/README.md)).
> 2. **Real-time parameter push (uniform IPC channel)**
>    ([12 § D](../12-field-graph-and-app/03-raymarch-app.md#d-standalone-raymarch-app)) — a
>    `set <paramKey> <value>` channel, no per-change shader recompile. DEFERRED,
>    **Boletus-driven** — a latency polish ([D-25](../../decisions/README.md)).
> 3. **Render-time clip mask** — `dualc_field_view --clip "<volume>"`
>    ([12 § F](../12-field-graph-and-app/05-open-surface.md#deferred-optional--render-time-clip-mask-isolated-open-surface-preview)):
>    a zero-thickness open-surface preview, examples-layer only. DEFERRED
>    ([D-16](../../decisions/README.md)).
> 4. **Section-plane UI sliders** ([12 § D.1](../12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection)) —
>    the `uClip[3]`/`uClipMask` uniforms are the hook. PLANNED — its trigger, the public release,
>    fired 2026-10-03 ([D-17](../../decisions/README.md)).
> 5. **Web build (browser shell)** ([08](../08-raymarch.md), [12](../12-field-graph-and-app/README.md))
>    — the shader already targets the WebGL2 subset; the browser shell + distribution

### From [the decisions index](../../decisions/README.md) — rows D-16, D-17 and D-25

The three rows as they stood before the hand-off re-stated them:

> | ID | Decision | Status | Date | Where recorded |
> | --- | --- | --- | --- | --- |
> | D-16 | Render-time clip mask (`dualc_field_view --clip`) for a zero-thickness open-surface preview | DEFERRED — trigger †: an inspection needing the open surface at zero thickness against a clip volume the thin-shell recipe and the axis-aligned section planes cannot give | 2026-06-16 | [12/05 § Deferred](../12-field-graph-and-app/05-open-surface.md#deferred-optional--render-time-clip-mask-isolated-open-surface-preview) |
> | D-17 | Section-plane UI sliders — the `uClip[3]`/`uClipMask` uniforms are the hook; keyboard only | PLANNED — to be done: its trigger †, the public release, fired 2026-10-03 ([20 #47](../20-public-delivery/01-pin-geometry-central.md#47-pin-geometry-central-to-upstream-own-nanort-self-bootstrapping-clone)) | 2026-06-16 | [12/03 § Section planes](../12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection) |
> | D-25 | Real-time parameter push (a `set <key> <value>` uniform IPC channel); file-watch recompiles instead | DEFERRED — trigger: continuous slider scrubbing of a dense lattice finds the per-change recompile hitch limiting | 2026-07-03 | [12/03 § Parameter push](../12-field-graph-and-app/03-raymarch-app.md#real-time-parameter-push-uniform-ipc-channel) |

---

← Back to the [block README](README.md) · the [Roadmap index](../README.md).
