# Design and plan: `graded-onion`, export quality, implementation subtleties, the full-stack plan

Part of [13 — Graded TPMS](README.md). The node as designed, why export quality is not
the complication, and the implementation plan as written before it shipped (source line
numbers are as of 2026-06-17). Usage: [command_reference/11 §
Graded](../../command_reference/11-dualc_field/03-graded-and-morph.md).

## Implementation subtleties (if/when built)

These follow from the codebase contract and must be baked into any plan:

- **Architectural placement.** Wavelength is baked into the TPMS *primitive*'s
  evaluation, so it cannot be varied by a wrapping decorator. The graded TPMS
  must be a node that itself reads a control child — i.e. dedicated graded
  variants (or the TPMS node optionally taking a control input + remap params),
  not a pure post-op. Thickness *can* be a decorator (graded `onion`).
- **Gradient is finite-differenced.** `k(p)` makes the closed-form gradient
  messy, so graded variants finite-difference → the **loose FD parity tolerance
  tier already exists** in `dualc_glsl_parity` (`FA, FR = 3e-2`), and softer QEF
  normals if this ever feeds contour/export.
- **`cellOverlaps` stays `true`; depth must resolve the *smallest* wavelength.**
  Graded variants inherit the TPMS always-refine rule. The high-frequency end of
  the gradient sets the octree depth / export sampling cost — flag it.
- **Live wavelength uniform vs `stepScale` / `featureScale` floor.** Wavelength
  is a uniform multiply inside `sin(k·x)` → a genuine tier-2 live-edit (no
  recompile). But `stepScale` is computed at *compile time* from the smallest
  feature; dragging wavelength *below* that floor live makes the sphere-tracer
  overstep and walls drop out. Either clamp the uniform to the compile-time floor
  or recompute the step conservatively. This is a correctness edge, not polish.
- **Full-stack by construction.** Even though the ask is "in `dualc_field_view`",
  the parity gate makes this a full node addition — C++ field + GLSL emitter +
  `kLibrary` snippet + `primitiveParams` layout (kept identical in
  `field_graph.cpp` and `field_glsl.cpp`) + a parity case. See `12 § B` and the
  "adding a node" checklist.

## Why export-quality is *not* a big complication (for thickness)

The wavelength caveats above were frequency problems; thickness grading has
none of them:

- **Surface positions are exact.** `valueAt = |base(p)| − t(p)` carries the full
  `t(p)`; the contourer root-finds crossings on `valueAt`, so exported vertex
  positions are exact.
- **No sampling-cost inflation.** Grading thickness does **not** change the
  lattice period, so the octree depth needed is identical to a *uniform* onion at
  that wavelength. `cellOverlaps` just delegates to the base TPMS (always-refine),
  exactly as `OnionField` already does.
- **Gradient: reuse the onion approximation.** `∇(|f|−t) = sign(f)·∇f − ∇t`. We
  drop the `−∇t` term — precisely what `OnionField::gradientAt` already does for
  uniform `t` (`decorators.cpp:51`). `∇t` is tiny (thickness varies slowly:
  `(t2−t1)/(d1−d0) ≪ 1` against the high-magnitude TPMS `∇f`), so the QEF normal
  tilts negligibly *only inside the falloff band*, and vertex positions are
  unaffected. Same fidelity class as today's shipping onion. → analytic,
  closed-form, **tight parity**, no FD machinery of its own.
- **Watertightness.** Holds as long as `min(t1,t2) > 0` and that minimum is
  resolvable at the chosen depth — the *same* constraint as uniform onion, now
  checked against the smaller of the two thicknesses. Guard `t > 0` to avoid a
  zero-thickness pinch (non-manifold).

Net: the only export-specific work beyond the preview node is (a) documenting the
`∇t` normal approximation and (b) the `min(t1,t2)` resolvability note — both
inherited from how `onion` already behaves.

## The node: `graded-onion`

A two-child decorator (`in: [base, control]`) with four scalar params. It
*replaces* `onion` in the lattice recipe; `base` is the field being shelled
(e.g. `normalize(gyroid(…))`), `control` is the criterion field.

```
t(p) = mix(t1, t2, clamp((control(p) − d0) / (d1 − d0), 0, 1))
graded-onion(p) = |base(p)| − t(p)
```

Four params give exactly the user's *constant → linear → constant* profile, in
**control-field units** (e.g. mm) — no separate normalization step. The four
params (`t1`, `t2`, `d0`, `d1`) and their defaults are documented once, in
[command_reference/11 § Graded shell & graded
inflation](../../command_reference/11-dualc_field/03-graded-and-morph.md#graded-shell--graded-inflation-two-children-base-control)
(the table that stood here was a copy; removed 2026-09-11).

The two constant plateaus are implicit (the `clamp` extends them to ±∞). To put
the falloff on the other "side", swap `t1`/`t2` (or flip the control, e.g. the
plane normal). Linear interpolation is intentional (the user's ask); a
`smooth=true` smoothstep variant is a trivial later knob, deferred. *(2026-09-18: trigger
written as row D-19 of the [decisions index](../../decisions/README.md).)*

**Live editing:** `t1`,`t2`,`d0`,`d1` all compile to **uniforms** → genuine
tier-2 live drag in `dualc_field_view`, no recompile. Swapping the control field
or the base is structural (tier-1 reload). No `stepScale` floor hazard (period
is fixed), so the wavelength live-edit gotcha does **not** apply here.

Example `--expr` (radial grade from a point, thin core → thick rim, clipped to a
box):

```
intersection(
  graded-onion(normalize(gyroid(wavelength=2)),
               sphere(radius=0),
               t1=0.04, t2=0.18, d0=2.0, d1=9.0),
  box(min=[-10,-10,-10], max=[10,10,10]))
```

## Implementation plan (full-stack, smallest diff)

A new node touches the five lockstep points from `12 § B`. `graded-onion` is a
two-child decorator, so — like `normalize`/`transform` — it is handled
**explicitly in the builder/codegen**, not via the single-child `PostOp` registry
or the two-child `BoolOp` registry.

1. **C++ field** — `src/implicit/decorators.cpp`: add `GradedOnionField`
   (mirror `OnionField`, `decorators.cpp:41`). Members: `base_`, `control_`,
   `t1_,t2_,d0_,d1_`.
   - `valueAt`: `|base→valueAt(p)| − t(control→valueAt(p))` with the clamp/mix
     remap; guard `denom = d1−d0`, treat `|denom|<eps` as a hard step.
   - `gradientAt`: `sign(base) · base→gradientAt(p)` (drop `∇t`, as `OnionField`).
   - `bounds`: `expandBBox(base→bounds(), max(t1,t2))`.
   - `cellOverlaps`: `base→cellOverlaps(expandBBox(cell, max(t1,t2)))`.
   - Factory `gradedOnionOf(base, control, t1, t2, d0, d1)` declared in
     `include/dualc/implicit.h` (near `onionOf`, line ~236).
2. **Graph builder** — `examples/field_graph.cpp`, `buildField()`: add a handler
   (alongside `normalize`/`transform`, ~line 711) —
   `requireChildren(n, 2)`; build `in[0]`,`in[1]`; read `t1`,`t2`,`d1` (required),
   `d0` (default 0); call `dualc::gradedOnionOf(...)`.
3. **GLSL codegen** — `examples/field_glsl.cpp`, `emitNode()` (alongside `onion`,
   line 719): emit `base` and `control` children, then declare four
   `declScalar` uniforms and emit
   `float fIdx(vec3 p){ float dn=d1-d0; …guard…; float u = clamp((fCtrl(p) - d0)/dn, 0., 1.);`
   `float t = mix(t1,t2,u); return abs(fBase(p)) - t; }`.
   *No `nodeStepScale()` entry needed* — graded-onion falls through to the default
   branch (line 776), which already takes the min step over **both** children
   (base + control), exactly as `onion` does. *No `kLibrary` helper needed* — the
   body is inline (like `onion`).
4. **Vocabulary** — `examples/dualc_field.cpp` (the `--list` printout): add a
   `graded-onion` entry under the decorator section, noting it is a two-child
   (base, control) node and the three control recipes.
5. **Parity gate** — `examples/dualc_glsl_parity.cpp` (`cases`, line 156): add two
   cases —
   - tight (A/R), graded-onion over an analytic base + analytic control to
     isolate the remap math (control = `sphere(radius=0)` = distance from origin):
     `graded-onion(sphere(center=[0,2,0],radius=1), sphere(radius=0), t1=0.05,t2=0.25,d0=0.5,d1=1.5)`
   - realistic (FA/FR, inherits `normalize`'s FD tier):
     `graded-onion(normalize(gyroid(wavelength=1)), sphere(radius=0), t1=0.05,t2=0.2,d0=0.2,d1=1.2)`
6. **Tests** — extend `tests/test_field_graph.cpp` with a build/round-trip case
   (JSON ⇄ shorthand) and a `valueAt` spot-check at three control values (below
   `d0`, mid-falloff, above `d1`).
7. **Docs** — `docs/command_reference/` (the `dualc_field` / `dualc_field_view`
   pages and the TPMS/lattice page): document `graded-onion`, the four params, and
   the three control recipes; note the `min(t1,t2)>0` resolvability rule.

**Effort:** small. One field class (~40 LOC, copy of `OnionField`), two ~10-line
codegen/builder handlers, two parity cases, docs. No new math, no FD node, no
`kLibrary` addition, no `stepScale` hazard.

**Known limitations (acceptable, documented):**
- QEF normals ignore the slowly-varying `∇t` (same as uniform `onion`); positions
  exact.
- DAG-dedup: the **GLSL preview** now dedups structurally-identical subtrees
  (`12 § B`, 2026-06-24), so a mesh used as both control and clip bakes once. The
  **contour/build path** (`field_graph.cpp`) still instantiates each occurrence
  separately — the file loads once via the resolver cache, but two field objects
  are built — so a mesh control is a known minor cost there until build-path dedup
  / DAG refs land. Cheap either way for point/plane controls.
- Linear (C0) falloff has a thickness-gradient kink at `d0`/`d1`; invisible in
  practice (continuous surface, `∇t` already dropped). `smooth=true` deferred.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
