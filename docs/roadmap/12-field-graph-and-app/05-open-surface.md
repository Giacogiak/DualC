# Isolating the open lattice surface (§ F)

Part of [12 — Field-graph & standalone raymarch app](README.md). The validated
thin-shell workflow and the deferred render-time clip mask.

## F. Isolating the open lattice surface — validated workflow + deferred clip mask

> **Status:** thin-shell workflow **validated 2026-06-16** (existing ops, no new
> code); the render-time clip mask is **deferred / optional**, and DualCEditor's since
> 2026-10-21 ([22](../22-dualceditor-handoff/README.md)).

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

**DEFERRED** — moved to DualCEditor (D-16), not started there; the intent, verbatim, and
its relation to #13 / #14: [22](../22-dualceditor-handoff/README.md).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
