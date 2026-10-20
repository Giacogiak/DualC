# #34 — the `gradientAt` contract and its census

The record of the `gradientAt` bullet of
[04 #34](04-engineering-quality.md#34-api-hygiene-batch), unit P03 of the
[hygiene + editor run](../../raw/2026-10-19-hygiene-and-editor-plan.md), on branch
`feat/hygiene-and-editor`. Opened 2026-10-20 by P03.1 (the contract's text and this census,
checked against `db8ffef`); P03.2 adds the true-gradient path and records it here.

## The contract

By the owner's decision ([D-65](../../decisions/01-settled.md)), `gradientAt` stays what the
code already made it — a **unit outward direction** — and that is written down as the
contract, in `include/dualc/implicit.h` (the base virtual and `GridField`'s class comment)
and in [design § 10.1](../../design/08-implicit-field-layer.md#101-implicitfield):

- length 1, the outward normal where `valueAt == 0`;
- shorter, never longer, where a node blends directions: the smooth booleans and `mix` (a
  lerp of two unit vectors) and `elongated`'s flat band (components zeroed);
- `(0,0,1)` where the direction is undefined;
- **not** |∇f|: a caller that needs the magnitude uses `trueGradientAt()` (P03.2).

The header comment had said "Field gradient at p. Normalized, this is the outward surface
normal", which promised a gradient and returned a direction; 04's bullet is that sentence.

**Chosen in-run (pending owner review):** the plan's wording names smooth blends and `mix`
as the only places the length drops below 1; the census found `elongated` does too (it
zeroes components of a unit vector), so the header and the design page name it as well.

## The census

Every override of `ImplicitField::gradientAt` under `include/` and `src/` (grep, `db8ffef`
plus this unit's header edit); `examples/`, `capi/` and `tests/` define none. 30 overrides
in 3D; the 2D layer's two are listed after.

| Override | file:line | Class | How |
| --- | --- | --- | --- |
| `PrimitiveField` (base of 36 primitives; 34 use it, the six TPMS included) | `include/dualc/primitives.h:27` | unit | central FD of `valueAt`, e=1e-4, normalised; `(0,0,1)` below 1e-12 |
| `SphereField` | `src/implicit/primitives_tierA.cpp:57` | unit | analytic: `p-c`, normalised; `(0,0,1)` at the centre |
| `PlaneField` | `src/implicit/primitives_tierA.cpp:101` | unit | the stored unit normal |
| `MeshSource` | `src/implicit/mesh_source.cpp:89` | unit | `p-cp` normalised, negated inside; on the surface the triangle's face normal (zero for a degenerate triangle); `(0,0,1)` if no closest point |
| `GridField` | `src/implicit/grid_field.cpp:108` | unit | analytic trilinear gradient, normalised at `:142`; discontinuous across cell faces (P04) |
| `WindingNumberField` | `src/implicit/winding_field.cpp:76` | **raw FD** | straddle-aware central/one-sided FD of `0.5 - w`, step 1e-4·diag, **not normalised** — the one override off the contract |
| `RevolveField` | `src/implicit/lift.cpp:44` | unit | `fdGradient` copy at `lift.cpp:12` |
| `ExtrudeField` | `src/implicit/lift.cpp:70` | unit | same `fdGradient` |
| `MirrorField` | `src/implicit/domain_ops.cpp:110` | pass-through | child's, reflected on the far side |
| `RepeatField` | `src/implicit/domain_ops.cpp:193` | pass-through | child's at the tile-local / nearest-copy point |
| `RepeatLimitedField` | `src/implicit/domain_ops.cpp:346` | pass-through | child's at the active copy |
| `TwistField` | `src/implicit/domain_ops.cpp:467` | unit | `fdGradient` copy at `domain_ops.cpp:17` |
| `BendField` | `src/implicit/domain_ops.cpp:530` | unit | same `fdGradient` |
| `DisplaceField` | `src/implicit/domain_ops.cpp:591` | unit | same `fdGradient` |
| `UnionField` | `src/implicit/combinators.cpp:57` | pass-through | the active operand's |
| `IntersectionField` | `src/implicit/combinators.cpp:83` | pass-through | the active operand's |
| `DifferenceField` | `src/implicit/combinators.cpp:111` | pass-through | `a`'s or `-b`'s |
| `XorField` | `src/implicit/combinators.cpp:137` | pass-through | the active operand's, sign by region |
| `SmoothUnionField` | `src/implicit/combinators.cpp:182` | blend-shortened | `lerp(∇b, ∇a, h)` |
| `SmoothIntersectionField` | `src/implicit/combinators.cpp:220` | blend-shortened | `lerp(∇b, ∇a, h)` |
| `SmoothDifferenceField` | `src/implicit/combinators.cpp:247` | blend-shortened | `lerp(-∇b, ∇a, h)` |
| `MixField` | `src/implicit/combinators.cpp:283` | blend-shortened | `lerp(∇a, ∇b, u)`, the d/dp u term dropped |
| `OffsetField` | `src/implicit/decorators.cpp:51` | pass-through | child's |
| `OnionField` | `src/implicit/decorators.cpp:82` | pass-through | `sign(f)·` child's |
| `GradedOnionField` | `src/implicit/decorators.cpp:128` | pass-through | `sign(base)·` base's, ∇t dropped |
| `GradedOffsetField` | `src/implicit/decorators.cpp:179` | pass-through | same, ∇t dropped |
| `ElongateField` | `src/implicit/decorators.cpp:222` | blend-shortened | child's with the flat-band components zeroed; child's again if that leaves 0 |
| `TransformField` | `src/implicit/decorators.cpp:275` | pass-through | child's, rotated (length kept only for a rigid matrix, the only kind the class supports) |
| `NormalizedField` | `src/implicit/decorators.cpp:344` | unit | child's, re-normalised; its magnitude for `valueAt` is a separate FD (`trueGradMag`, `:368`, e=1e-4) |
| `ScaleField` | `src/implicit/decorators.cpp:403` | pass-through | child's at `p/s`, flipped for `s < 0` |

Totals: unit 11, pass-through 13, blend-shortened 5, raw FD 1. "Pass-through" inherits the child's class: unit for a unit child, raw under a
`WindingNumberField`. So the contract holds for every tree that has no winding node; with
one, every ancestor that forwards it is off the contract too.

The 2D layer: `PrimitiveField2D` (`include/dualc/implicit2d.h:49`, normalised FD, `(0,1)`)
and `Circle2D` (`src/implicit/field2d.cpp:41`, analytic) are unit, and **no library path
calls them**: `RevolveField` and `ExtrudeField` take their 3D gradient by FD of their own
`valueAt`, never through the profile's `gradientAt`.

**The callers** of `gradientAt` in the library are two, both in
`src/implicit/implicit_field.cpp`: `edgeHit` (`:65`, normalises the result) and
`closestSurfacePoint` (`:78`, `:82`, the Newton step, whose only caller is the sampler's miss
fallback, `src/sampler.cpp:80`). `QefSolver::add` normalises again
(`src/internal/qef.cpp:149`). The forwarding nodes above call their children's. The GLSL
codegen and the C ABI expose no gradient.

## Against the raw plan's census

The raw plan's § 2 census holds; its line numbers match the code at `db8ffef`. It omitted
`RepeatLimitedField`, `GradedOnionField`, `GradedOffsetField` and `XorField` /
`DifferenceField` by name (all pass-through), filed `NormalizedField` with neither list (unit,
re-normalised), and did not mention the 2D layer (unit, uncalled).

## Gates

P03.1 changed comments and docs only, no behaviour: full gate `python3 scripts/check.py`
(Linux, GCC 15, Ninja, Release) 30 passed, 0 failed, 1 skipped (configure), 46 TUs
recompiled after the header edit with 0 DualC-origin warnings, **ctest 350 / 350**;
`--docs --strict` green. No field node or codegen moved, so the GPU tier was not run.

---

← Back to the [block README](README.md) · the [Roadmap index](../README.md).
