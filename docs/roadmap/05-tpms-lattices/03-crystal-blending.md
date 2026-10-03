# #17b Phase 5 — crystal blending (`mix`), and closing the feature

Part of [05 — TPMS lattices](README.md); continues [02](02-strut-enhancements.md). The
value-blend morph node, the `cellOverlaps` decision caught in review, and the closing
verification.

## Phase 5 -- crystal blending
**DONE (2026-07-09).**


Three distinct things; **only the third was new code.**

- **Compound cell (free, docs-only).** `smooth-union(bcc(…), fcc(…), k=…)`
  superimposes *both* crystals and welds them: a denser hybrid, **not** a
  transition (the weld piles up at the bcc cell-centre hub). Documented in
  `11-dualc_field.md` -- no work.
- **Spatial blend, hard seam (free, docs-only).** A *different* crystal per region:
  `union(intersection(box_left, bcc(…)), intersection(box_right, fcc(…)))`. A real
  spatial transition with a discontinuous seam; verified to contour cleanly.
  Documented -- no work.
- **Spatial blend, smooth morph -- the `mix` node (IMPLEMENTED).** bcc → fcc
  *morphing* across the part via the position-driven **`mix(A, B, control, lo,
  hi)`** node: value `lerp(A(p), B(p), w)` with `w = clamp((control(p)−lo)/(hi−lo),
  0, 1)` (GLSL `mix`; the same control-ramp shape as `graded-onion`/Phase 3). A
  3-child node (A, B, control). **Not an SDF** (a lerp of two SDFs is not a
  distance), but a valid implicit for DC.

**As built (2026-07-09).** The six standard wiring sites:
- **Field maths:** `MixField` in `src/implicit/combinators.cpp` (next to
  `SmoothUnionField`, reusing the file's `clamp01`/`lerp`/`bboxUnion`) + `mixOf(a,
  b, control, lo, hi)` factory; declared in `include/dualc/implicit.h`. `valueAt` =
  `lerp(a, b, w)`; `gradientAt` = `lerp(∇a, ∇b, w)` (drops the `∇w` ramp term, the
  house convention shared by every blend). `bounds()` = union of both children.
  **`cellOverlaps()` returns `true`** (conservative, like the TPMS primitives) --
  **not** the `a || b` union pattern. This is a subtle correctness point: for
  `min`/smooth-union the surface provably stays within `k/4` of an operand surface,
  so `a || b` is a tight never-miss test; but a **lerp surface floats free of both
  operands** (`lerp(A,B,w)=0` wherever `A/B = −w/(1−w)`, i.e. where A and B are both
  far from their own zero-sets -- a stub cap deep inside strut A, in strut B's empty
  space). There, `a->cellOverlaps || b->...` is `false` and silently drops the cap.
  Confirmed empirically: an `a || b` build dropped ~38k faces (510k vs the correct
  548k) on the demo morph. A regression test (`test_combinators.cpp`) pins it -- a
  cell where both operand `cellOverlaps` are `false` but the mix surface passes
  through. The `lo==hi` band is guarded with a tiny signed epsilon (hard step, no
  div-by-0).
- **`field_graph.cpp`:** a `mix` branch (`requireChildren(n,3)`, `lo` default 0
  like graded's `d0`, `hi` required like `d1`).
- **`field_glsl.cpp`:** a `mix` emitter beside graded-onion -- three child funcs +
  `lo`/`hi` uniforms + the same `hi==lo` guard, `return mix(fA(p), fB(p), u);`.
- **Parity:** one `mix` case at the **tight `A,R` tier** (`dualc_glsl_parity.cpp`).
  Note the roadmap originally guessed the `FA,FR` FD tier -- but the harness
  compares **value only** (never gradients), and `mix`'s value is the *identical*
  `lerp/clamp` formula in C++ and GLSL, so with analytic children it parities
  tightly (`graded-onion` is the precedent). Measured maxErr **2.36e-07**; count
  **68 → 69**.
- **Tests:** a `mixOf` value/bounds/`lo==hi` unit test + the stub-cap refinement
  regression above (`test_combinators.cpp`), and a codegen test
  (`test_field_glsl.cpp`) asserting the `mix(f…` marker (distinct from the graded
  ops' `mix(u_n…` scalar blend) plus all three child subtrees.
- **Docs/CLI:** a "Spatial morph (three children)" section + a smooth-morph recipe
  in `11-dualc_field.md`; `mix` in the `--list` vocabulary; the live-uniform note +
  parity `69/69` in `12-dualc_field_view.md`.

**Caveat (documented, inherent) — `mix` blends values, not shapes.** A `lerp` of
two SDFs gives a continuous body *only where the two solids overlap*. **Same
family** (`mix(bcc(r1), bcc(r2), …)`): the struts coincide, so the blend reduces
exactly to a graded radius on one connected lattice -- fully fluid, one watertight
body (this is the intended lattice use; `graded-offset` is the cheaper equivalent).
**Two *different* crystals** (bcc↔fcc): their struts sit at different positions, so
at the mid-band `0.5·A + 0.5·B` is positive (empty) wherever one strut is far from
the other -- the struts **taper to nothing** at the plane and the output **splits
into two separate bodies with a gap slab**. This is inherent to interpolating
disjoint fields, not a bug, and no `--depth` fixes it. **To get a continuous body
across two different crystals you must GRAFT** (clip each to overlapping regions and
`union`/`smooth-union` so their struts cross and fuse), *not* `mix` -- documented in
`11-dualc_field.md` (§ Spatial morph critical callout + "Grafting two different
crystals"). This distinction is the key thing downstream clients (e.g. Boletus) must
know. ctest **205/205**, GPU parity **69/69**.

## Closing the feature
**DONE (2026-07-09).**


Phases 3-5 all landed on branch `feat/strut-lattices`: §17 + §17b flipped to DONE,
`dualc_glsl_parity` bumped to **69/69** in `12-dualc_field_view.md`, README index
row refreshed *(2026-09-20: Phase 5 and the flip are `aa0d577`, 2026-07-09)*. A
`dualc_lattice --lattice strut` convenience shortcut is explicitly **out of scope**
unless a non-field-graph entry point is ever requested.

**Manual validation of the #17 core** (already shipped): the copy-paste command
set (box-fill, mesh-clip, all four crystals, recipes, live preview) lives in
[command_reference/11 § Strut
lattices](../../command_reference/11-dualc_field/02-strut-lattices.md#strut-lattices-wireframe-crystals).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
