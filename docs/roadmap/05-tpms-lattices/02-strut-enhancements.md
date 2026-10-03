# #17b Strut-lattice enhancements — the template, Phase 3 (graded radius), Phase 4 (tapered struts)

Part of [05 — TPMS lattices](README.md). The execution record of #17b as written at the
time (source line numbers are as of 2026-07-09 and have drifted since; the code, not
this page, is the reference). Phase 5 and the closing entry:
[03](03-crystal-blending.md).

## 17b. Strut-lattice enhancements (Phases 3-5)
**DONE (2026-07-09).**


Self-contained execution spec for the requested follow-ons. **Each phase is
independent** -- they ran in any order (Phase 5's compound / hard-seam parts were
docs-only). The #17 core landed in commit `7498b6b` on branch `feat/strut-lattices`;
build/run it first (§ *Build & test* at the top of this repo's `CLAUDE.md`) and skim
`examples/example_common.cpp` `makeStrutLattice` before starting. **All three phases
are implemented** (the `graded-offset` decorator, the `nodeRadius` taper, and the
`mix` morph node); the specs below are kept as the as-built record.

*(2026-09-17: the build section is now in `AGENTS.md`, which `CLAUDE.md` imports —
[19](../19-docs-layers/README.md) Phase 1.)*

### The template every phase follows (how the #17 core was wired)

A field-graph feature touches the same **six sites** on both the contour path and
the GLSL path. Replicate this pattern:

1. **Field maths** -- a `dualc::` factory/decorator in `src/implicit/` (public decl
   in `include/dualc/implicit.h`, or a primitive in `include/dualc/primitives.h`).
2. **Registry / factory** -- `examples/example_common.{h,cpp}` (e.g.
   `makeStrutLattice` / `strutCellSegments`, next to `makeTpmsField`).
3. **Contour lowering** -- a branch in `buildField()` in `examples/field_graph.cpp`
   (the strut branch sits just after the `isTpmsOp` branch; `graded-onion` is the
   model for a control-child decorator, `field_graph.cpp` ~ line 821).
4. **GLSL emitter** -- an `emit*` + dispatch in `examples/field_glsl.cpp`
   (`emitStrut`; the `graded-onion` emitter at `field_glsl.cpp:1263` is the model
   for a control-child node), plus the `nodeFeatureScale` / `nodeStepScale` hooks
   if the node has a characteristic length.
5. **Parity case** -- one row per new node in `examples/dualc_glsl_parity.cpp`
   (tight `A,R` tier for exact SDFs; `FA,FR` for FD/non-metric nodes). Must keep
   the count at 100 % (the live count: [17 § Latest verification](../17-code-audit-and-hardening/README.md#tracked-items--status-at-a-glance)). Build with `-DDUALC_BUILD_GLSL_PARITY=ON`.
6. **Tests + docs** -- a unit test (Catch2, `tests/`, added to `tests/CMakeLists.txt`);
   a GL-free codegen check in `tests/test_field_glsl.cpp`; a CTest smoke in
   `examples/CMakeLists.txt` if a new CLI surface; a `--list` row in
   `examples/dualc_field.cpp` `printVocabulary()` for a new source; and the
   `docs/command_reference/11-dualc_field.md` + `12-dualc_field_view.md` entries.

*(2026-09-18: the parity count has one home — the line linked above.)*

### Phase 3 -- graded strut radius
**DONE (2026-07-07).**


Shipped exactly as specced below: a sibling decorator **`gradedOffsetOf(base,
control, t1, t2, d0, d1)`** = `base(p) - t(p)` (inflate the solid) reusing
`GradedOnionField`'s control ramp verbatim, with `std::abs` dropped
(`src/implicit/decorators.cpp`, decl in `include/dualc/implicit.h`). Wired on both
paths: `graded-offset` branch in `examples/field_graph.cpp` (contour) and
`emitGradedOffset`-equivalent in `examples/field_glsl.cpp` (GLSL, emits
`<base>(p) - t` instead of `abs(<base>(p)) - t`). Tests: a `gradedOffsetOf` unit
test in `tests/test_combinators.cpp` (value == `base - t(control)`; `t1==t2==T`
collapses to `offsetOf(base, +T)`; bounds grow outward) + a codegen check in
`tests/test_field_glsl.cpp` (graded-offset omits the `abs`, keeps the ramp).
`dualc_glsl_parity` **67/67** (graded-offset at maxErr 3.3e-07, tight tier). Docs:
`graded-offset` decorator row + solid-inflation subsection + graded strut-radius
recipe in `11-dualc_field.md`; parity count bumped to 67/67 in
`12-dualc_field_view.md`. End-to-end verify (thin struts at origin thickening
outward) contours watertight (F/V = 2.006 at depth 7). `ctest` 199/199.

**Goal.** Vary strut radius across space, driven by the **same control-field
grading** `graded-onion` uses, so a lattice can be dense near a load path and
sparse elsewhere with the exact grading the client already knows.

**Locked design.** A *solid* lattice must be **inflated**, not onioned (onion
`|f|−t` would hollow each strut into a tube -- that is the *hollow-struts* recipe,
a different feature). Add a sibling decorator **`gradedOffsetOf(base, control, t1,
t2, d0, d1)`** whose value is `base(p) − t(p)` (inflate) where `t(p)` is the
*identical* ramp `GradedOnionField` computes (`t1..t2` as `control` goes `d0..d1`,
clamped). Positive `t` grows the solid outward by `t`, i.e. thickens every strut
by `t` locally. Composes with any solid field, not just struts.

**Files.**
- `src/implicit/decorators.cpp` -- add a `GradedOffsetField` class + a
  `gradedOffsetOf` free function. **Copy `GradedOnionField` verbatim**
  (`decorators.cpp:87-127`, and the wrapper at `:318-322`) and change exactly two
  things: `valueAt` returns `base_->valueAt(p) - thicknessAt(p)` (drop the
  `std::abs`); `bounds()` still expands by `maxThickness()` (outward growth), and
  keep `cellOverlaps`/`gradientAt` as-is (gradient of `base − t` ≈ `grad base`).
- `include/dualc/implicit.h` -- declare `gradedOffsetOf` next to `gradedOnionOf`
  (`:245`); document that positive `t` inflates.
- `examples/field_graph.cpp` -- add a `graded-offset` branch mirroring the
  `graded-onion` one (`~:821`): 2 children `(base, control)`, params `t1 t2 d0 d1`.
- `examples/field_glsl.cpp` -- add an `emitGradedOffset` mirroring the
  `graded-onion` emitter (`:1263`); the only change is the final line emits
  `return <base>(p) - t;` instead of `return abs(<base>(p)) - t;`.
- `examples/dualc_glsl_parity.cpp` -- add a `graded-offset` case (tight `A,R`; a
  metric base like a strut lattice or a sphere so the SDF is exact).
- `tests/test_combinators.cpp` (or `test_implicit.cpp`) -- assert
  `gradedOffsetOf(sphere, control, …)->valueAt(p) == sphere(p) - t(control(p))`
  for a few points, and that at `t1=t2=T` it equals a plain `offset` by `T`.
- Docs: a `graded-offset` row in `11-dualc_field.md` (Decorators) + a graded
  strut-radius recipe in the Strut-lattices section.

**Verify.**
```
dualc_field --expr "intersection(box(min=[-1,-1,-1],max=[1,1,1]),graded-offset(bcc(wavelength=0.4,radius=0.02),sphere(radius=0),t1=0.0,t2=0.06,d0=0.0,d1=1.0))" --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o graded.obj
```
Expect thin struts near the origin (control = distance-from-point ≈ 0 → `t≈t1=0`)
thickening outward. `dualc_field_view` the same expr; `dualc_glsl_parity` → 67/67.

### Phase 4 -- tapered struts
**DONE (2026-07-09).**


Shipped as specced: an optional `nodeRadius` param on every strut node (default =
`radius` => no taper, untapered graphs byte-identical). When `nodeRadius != radius`
each unit-cell segment is split at its midpoint into **two** `RoundConeField`s --
`RoundCone(a,mid,nodeRadius,radius)` U `RoundCone(mid,b,radius,nodeRadius)` -- giving
the symmetric fat-node / thin-span profile; round cones are exact SDFs so the field
stays Lipschitz-1 and the single round-fold tiling stays exact. Wired on both paths:
`makeStrutLattice` gains a `nodeRadius` arg (sentinel `-1` = `radius`) building the
two-cone cell (`examples/example_common.cpp`); `field_graph.cpp` reads
`numParam(n,"nodeRadius",radius)`; `emitStrut` (`examples/field_glsl.cpp`) emits two
`sdRoundCone` per segment with a `nodeRadius` uniform on the tapered path only (the
untapered path keeps the exact `sdCapsule` GLSL). Tests: the oracle extended to the
two-cone union (`test_strut_lattice.cpp`, 400 pts/crystal, exact single-fold) + a
node-vs-midspan profile test; a tapered codegen check in `test_field_glsl.cpp`
(2xsegcount `sdRoundCone`, 0 `sdCapsule`; untapered unchanged). `dualc_glsl_parity`
**68/68** (`bcc-taper` at maxErr 1.5e-08, tight tier). `ctest` 202/202. End-to-end
tapered bcc contours watertight (F/V = 2.005 at depth 7); the untapered default is
byte-identical to pre-Phase-4. `--list`, `11-dualc_field.md` (param + tapered recipe
+ tapered-and-graded recipe), and `12-dualc_field_view.md` (parity 68, tapered
preview) updated.

**Goal.** Struts fat at the nodes, thin mid-span (organic / stress-aligned look).

**Locked design.** Add an optional `nodeRadius` param to every strut node
(default = `radius` ⇒ no taper, so existing graphs are unchanged). A strut has a
node at **both** ends, so a single `RoundConeField(a,b,rA,rB)` (one taper a→b) is
wrong -- split each segment at its midpoint into **two** round cones
`RoundCone(a, mid, nodeRadius, radius) ∪ RoundCone(mid, b, radius, nodeRadius)`,
giving the symmetric fat-ends/thin-middle profile. `RoundConeField`
(`include/dualc/primitives.h:196`, ctor `(a,b,rA,rB)`) and the GLSL `sdRoundCone`
(`field_glsl.cpp:163`, node emit `:849`) already exist on both paths.

**Files.**
- `examples/example_common.{h,cpp}` -- add a `nodeRadius` arg to `makeStrutLattice`
  (default = `radius`). When `nodeRadius != radius`, build the cell from the
  two-round-cone split per segment instead of `CapsuleField`; else keep the capsule
  path (byte-identical to today). `strutCellSegments` is unchanged (still the
  authoritative segment list); the taper is applied where capsules are built.
- `examples/field_graph.cpp` -- read `numParam(n, "nodeRadius", <radius>)` in the
  strut branch and pass it through.
- `examples/field_glsl.cpp` -- in `emitStrut`, when tapered emit two `sdRoundCone`
  calls per segment (split at midpoint) unioned by `min`; declare a `nodeRadius`
  uniform so it stays live-editable.
- `examples/dualc_glsl_parity.cpp` -- one tapered case, e.g.
  `bcc(wavelength=1,radius=0.06,nodeRadius=0.14)` (tight `A,R`).
- `tests/test_strut_lattice.cpp` -- assert the on-axis field at a node is more
  negative (fatter) than at a span midpoint when `nodeRadius > radius`; extend the
  oracle to the tapered cell (tile the round-cone union independently).
- Docs: `nodeRadius` in the Strut-lattices param table + a tapered recipe.

**Verify.**
```
dualc_field --expr "intersection(box(min=[-1,-1,-1],max=[1,1,1]),bcc(wavelength=0.5,radius=0.03,nodeRadius=0.1))" --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o taper.obj
```
Nodes should bulge, spans pinch. `dualc_glsl_parity` → 68/68.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
