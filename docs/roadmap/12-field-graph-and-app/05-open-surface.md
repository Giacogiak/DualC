# Isolating the open lattice surface (§ F)

Part of [12 — Field-graph & standalone raymarch app](README.md). The validated
thin-shell workflow and the deferred render-time clip mask.

## F. Isolating the open lattice surface — validated workflow + deferred clip mask

> **Status:** thin-shell workflow **validated 2026-06-16** (existing ops, no new
> code); the render-time clip mask is **deferred / optional**.

A recurring engineer question: a zero-thickness TPMS clipped to a volume (e.g.
`dualc_lattice cube.obj`, or `intersection(gyroid, volume)`) contours into a
**closed** block — the volume's faces come out welded to the gyroid. The ask is to
(a) inspect the **lattice surface alone**, open, inside the volume; (b) thicken it
to a watertight solid with **no** volume skin; (c) thicken the volume's surface into
a **skin**; (d) **union** the two into one part.

**Key finding.** The welded skin is a **dual-contouring artifact** — DC only emits
closed, watertight surfaces (the same property that bakes a mesh soup into one solid;
a feature). A **raymarcher renders an isosurface directly** and has no such
constraint, so isolating the open surface is a *visualization* concern, not a
core-DC one. On the roadmap-01 Tier-4 question: `#13` (intersection-free contouring)
is **not** relevant (field-level booleans are already non-self-intersecting); `#14`
(multi-material / open-boundary) is the closest match but is only needed to emit the
open surface **as a mesh** — and the open surface here is preview-only, with every
downstream step thickened, so **no Tier-4 core-DC work is required** (see the
clarifying note in
[01-core-dual-contouring/](../01-core-dual-contouring/README.md#tier-4--open-algorithmic-extensions-for-special-needs)).

### Validated thin-shell workflow (no new code)

The accepted path, verified 2026-06-16 against `cube.obj` and `foot.obj`, composes
existing operators — the recipe is `intersection(onion(normalize(tpms)), volume)`:

- `onion(f, t)` = `|f| − t` turns the zero-thickness membrane into a **thin closed
  shell** (solid only for `−t < f < +t`; empty in the bulk of both channels). Because
  the shell is empty between its sheets, the intersection shows the volume boundary
  **only** as tiny strut-end cross-sections — no big welded skin. Wall `≈ 2·thickness`.
- `normalize` makes `thickness` first-order metric (mm). A TPMS has **infinite**
  bounds, so the **volume operand supplies both the clip and the finite domain**.
- The four steps are all field-level (one contour each): **(a)** meshless preview in
  `dualc_field_view`; **(b)** thicken → watertight export via `dualc_field`; **(c)**
  skin = `onion(volume)`; **(d)** `union(lattice-shell, skin)`. Full recipes:
  [command_reference/11 › Workflow](../../command_reference/11-dualc_field/04-workflow-open-surface.md#workflow-isolate--thicken--skin--union-a-tpms-lattice)
  and [command_reference/12 › Isolating a TPMS surface](../../command_reference/12-dualc_field_view/04-open-surface-preview.md#isolating-a-tpms-surface-inside-a-volume-meshless-preview).

The thin shell is **closed** (a tiny real wall), not a true zero-thickness open
sheet — visually faithful for inspection and exactly what the manufacturing steps
thicken anyway. The one thing it cannot show is the genuinely open membrane, which
motivates the deferred feature below.

### Deferred (optional) — render-time clip mask: isolated open-surface preview

> **Not to be confused with the shipped section planes (D.1).** To inspect the
> *interior of a solid* lattice, the viewers already have axis-aligned **section
> planes** ([§ D › Section planes](03-raymarch-app.md#section-planes-viewport-inspection);
> keys in [command_reference/12](../../command_reference/12-dualc_field_view/01-controls.md#section-planes-viewport-only-inspection)).
> The clip mask below is a *different* feature: it isolates a genuinely **open**
> surface (a zero-thickness membrane) by masking it with a *second field*, where a
> section plane is a half-space cut of a *solid*.

**What it is.** A visualization-only capability for `dualc_field_view`: march a
**surface field** (e.g. a bare `gyroid`, the `gyroid=0` membrane) and accept a hit
only where a **separate clip field** is inside, otherwise keep marching. Exposed as a
second field-graph, e.g. `dualc_field_view --expr "<surface>" --clip "<volume>"`. The
clip volume can be any GLSL-compilable field (box/sphere/their booleans — exact) or a
mesh (baked to the existing `sampler3D` path). It is the literal "render the surface,
masked by the volume," with no field-algebra change.

**What it adds beyond the shipped thin-shell approach.**

1. **True zero-thickness open surface** — renders the genuine single membrane as a
   2-sided sheet with real open boundary edges where it is cut, not a thin *closed*
   shell with an artificial ±t wall and tiny strut-end caps.
2. **Decouples "what to draw" from "where to draw it"** — today you must bake a
   thickness into the field just to make the membrane hit-able and to suppress the
   skin; the mask draws any field's isosurface inside any volume *without* altering
   the field, so the preview is exactly the design surface.
3. **Inspection independent of wall thickness** — judge cell geometry / wavelength /
   how the lattice meets the boundary before committing a wall thickness.
4. **Cleaner clipped preview against arbitrary volumes** — open cuts at the volume
   surface, no tiny boundary caps, for any analytic or mesh clip region.

**Why it is deferred / optional.** Every downstream manufacturing step (thicken →
watertight export → boolean) requires a real thickness anyway, and the thin-shell
preview is visually faithful for that workflow. The mask is a pure inspection nicety;
it unlocks no new *export*. *(2026-09-18: trigger written as row D-16 of the
[decisions index](../../decisions/README.md).)*

**Implementation sketch (examples-layer only — no core-DC changes).**

- `examples/field_glsl.{h,cpp}`: emit a second `clipSDF(p)` alongside `sceneSDF(p)`
  from the `--clip` graph; add its uniforms / `MeshTexture` bakes to the binding table.
- The `kTraceFramework` trace loop (`field_glsl.cpp`) + `examples/raymarch_gl`: at the
  hit test (and the pre-loop entry-inside case), accept only when `clipSDF(hp) <= 0`;
  on rejection **continue marching** past the crossing instead of `break`. Needs care
  for a solid surface field (a bare gyroid's `<0` half-space — march out of the solid
  before resuming the sphere-trace) and for bisection / normal estimation right at the
  open boundary.
- `examples/dualc_field_view.cpp`: add the `--clip` flag and wire the second compile.
- `examples/dualc_glsl_parity.cpp`: a parity / `--snapshot` case for the masked render.
- Cost: two SDF evaluations per step (surface + clip) where masking is active.

**Relationship to `#13`/`#14` (roadmap 01).** Distinct — those are **core-DC mesh**
features; the clip mask is **viewer-only**. If an open-surface *mesh* (not just a
preview) is ever required, that is the `#14` "open-boundary" direction; how it would be
realized (the Hermite-edge source tag) is on the item itself,
[01 Tier-4 item 14](../01-core-dual-contouring/README.md#14-multi-material--open-boundary-contouring)
— a separate, larger item, PLANNED without a trigger.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
