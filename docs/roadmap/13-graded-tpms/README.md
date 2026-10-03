# Graded TPMS — spatially-varying wavelength & thickness

> Design study (2026-06-17) for letting a TPMS lattice vary its **wavelength**
> (cell period) and **thickness** (onion wall) across the volume, driven by a
> criterion such as distance-from-the-bounding-surface. Targets the
> [field-graph + `dualc_field_view`](../12-field-graph-and-app/README.md) pipeline: any new
> capability is a field-graph node that must build in C++, compile to GLSL, and
> pass the per-node parity gate. **[DONE 2026-06-17](#status)** — designed and shipped
> the same day (the banner first read "design only — not yet scheduled"). This
> page records the analysis (including why the "correct" version is out of scope)
> so the decision is not re-derived later.

## The pages of this topic

Split on 2026-09-11 from the single frozen file (the record is verbatim; only status
moved out of headings). The decision and the status stay on this page.

| Page | Sections |
| --- | --- |
| [Analysis](01-analysis.md) | [motivation](01-analysis.md#motivation) · [unifying insight](01-analysis.md#the-unifying-insight) · [the two options](01-analysis.md#evaluation-of-the-two-options) · [complexity](01-analysis.md#complexity--the-honest-headline) · [why wavelength is a PDE](01-analysis.md#why-the-fully-correct-version-is-a-pde-phase-field-solve--and-out-of-scope) |
| [Design and plan](02-design-and-plan.md) | [implementation subtleties](02-design-and-plan.md#implementation-subtleties-ifwhen-built) · [export quality](02-design-and-plan.md#why-export-quality-is-not-a-big-complication-for-thickness) · [the `graded-onion` node](02-design-and-plan.md#the-node-graded-onion) · [the full-stack plan](02-design-and-plan.md#implementation-plan-full-stack-smallest-diff) |

## Decision
**2026-06-17.**


- **Build graded thickness; drop graded wavelength** (the chirp/PDE problems in
  [01 § Why the fully-correct version is a PDE](01-analysis.md#why-the-fully-correct-version-is-a-pde-phase-field-solve--and-out-of-scope)). Wavelength is **dropped — out of scope** (no trigger; removed from the
  roadmap 2026-07-03).
- **Target export-quality, not just preview** — and for *thickness* this costs
  almost nothing extra (see below). The same node serves `dualc_field_view`
  (live) and `dualc_field` (contour + STL/3MF export).
- **Criterion = a generic control field (second graph child).** Do *not*
  hardcode criteria — every criterion is just a different control primitive, at
  zero extra code. Canonical recipes:
  - **Distance from a point** → control `sphere(center=[x,y,z], radius=0)`
    (`value = |p−center|`).
  - **Distance from a plane / linear along an axis** → control
    `plane(nx,ny,nz,offset)` (signed distance; the cleanest gradient to reason
    about).
  - **Distance from a surface / target volume** → control `mesh(path=…)` (signed
    distance, negative inside; often the same mesh as the clip volume).

## Status
**DONE (2026-06-17)** — graded thickness shipped; graded wavelength DROPPED.


Graded **thickness** shipped as the `graded-onion` node, full-stack and
export-quality. Verified:

- `GradedOnionField` + `gradedOnionOf` in `src/implicit/decorators.cpp` /
  `include/dualc/implicit.h`; builder in `field_graph.cpp`; GLSL emitter in
  `field_glsl.cpp`; `--list` vocabulary in `dualc_field.cpp`.
- **Parity gate 30/30** (was 28/28) — added a tight analytic case
  (`graded-onion` over sphere base + sphere control, maxErr 3.3e-7) and a
  realistic FD case (over `normalize(gyroid)`, maxErr 2.0e-3, within the FD
  tier).
- **Full test suite 157/157** — added `tests/test_field_graph.cpp` case pinning
  the constant→linear→constant ramp at three control regimes + JSON round-trip.
  *(2026-09-18: 157 is the Catch2 `TEST_CASE` count at `51b5efa`; `ctest -N` there is
  176 — 157 + 19 `add_test` CLI smoke tests. The "171/171" of 2026-06-14 in
  [12/03](../12-field-graph-and-app/03-raymarch-app.md#d-standalone-raymarch-app) is the
  ctest total at `2530acf` (153 + 18): two metrics, no conflict.)*
- **End-to-end export** — `dualc_field --expr "intersection(graded-onion(
  normalize(gyroid(wavelength=2)),sphere(radius=0),t1=0.04,t2=0.18,d0=2,d1=7),
  box(...))" --depth 6` contoured + wrote a valid STL (210 952 verts /
  452 196 faces).
- **Both GPU viewers build** against the new node (codegen shared via
  `compileToGlsl`, validated by the parity gate); the four scalars are live
  uniforms.
- Docs:
  - [`command_reference/11-dualc_field/`](../../command_reference/11-dualc_field/03-graded-and-morph.md) — node spec table (`t1`/`t2`/`d0`/`d1`),
    the three control recipes (point/plane/surface), the export-quality +
    `min(t1,t2)>0` notes, recipe (b′), and a **tested foot-mesh quick-test recipe**
    (`data/foot.obj`, control sphere → thick instep; 514 553 tris, ~4 min @ depth 7).
  - [`command_reference/12-dualc_field_view/`](../../command_reference/12-dualc_field_view/README.md) — the foot + mesh-free live
    examples and a worked **keyboard-control** table for the four scalars (no new
    keys: `tab` cycles to `t1`/`t2`/`d0`/`d1`, `↓`/`↑` nudge — they are ordinary
    scalar bindings on the existing tier-2 path).

Graded **wavelength** is **dropped — out of scope** (removed from the roadmap
2026-07-03; no trigger). The PDE/chirp analysis in [01](01-analysis.md#why-the-fully-correct-version-is-a-pde-phase-field-solve--and-out-of-scope) is the standing rationale.

---

← Back to the [Roadmap index](../README.md).
