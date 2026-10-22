# #34 — the `gradientAt` contract and its census

The record of the `gradientAt` bullet of
[04 #34](04-engineering-quality.md#34-api-hygiene-batch), unit P03 of the
[hygiene + editor run](../../raw/2026-10-19-hygiene-and-editor-plan.md), on branch
`feat/hygiene-and-editor`. Opened 2026-10-20 by P03.1 (the contract's text and this census,
checked against `db8ffef`); P03.2, the same day, added the true-gradient path —
[below](#the-true-gradient-path).

## The contract

By the owner's decision ([D-65](../../decisions/01-settled.md)), `gradientAt` stays what the
code already made it — a **unit outward direction** — and that is written down as the
contract, in `include/dualc/implicit.h` (the base virtual and `GridField`'s class comment)
and in [design § 10.1](../../design/08-implicit-field-layer.md#101-implicitfield):

- length 1, the outward normal where `valueAt == 0`;
- shorter, never longer, where a node blends directions: the smooth booleans and `mix` (a
  lerp of two unit vectors) and `elongated`'s flat band (components zeroed);
- `(0,0,1)` where the direction is undefined;
- **not** |∇f|: a caller that needs the magnitude uses `trueGradientAt()`.

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
| `GridField` | `src/implicit/grid_field.cpp:108` | unit | analytic trilinear gradient, normalised at `:151`; discontinuous across cell faces — P04's continuous node blend was reverted by P04b, E12 open ([20](20-gridfield-normal.md)) |
| `WindingNumberField` | `src/implicit/winding_field.cpp:76` | **raw FD** → unit (P03.2) | straddle-aware central/one-sided FD of `0.5 - w`, step 1e-4·diag, **not normalised** — the one override off the contract; since P03.2 that FD is its `trueGradientAt` and `gradientAt` its unit direction |
| `RevolveField` | `src/implicit/lift.cpp:44` | unit | `fdGradient` copy at `lift.cpp:12` (P03.2: the shared `internal::fdGradientDirection`) |
| `ExtrudeField` | `src/implicit/lift.cpp:70` | unit | same `fdGradient` |
| `MirrorField` | `src/implicit/domain_ops.cpp:110` | pass-through | child's, reflected on the far side |
| `RepeatField` | `src/implicit/domain_ops.cpp:193` | pass-through | child's at the tile-local / nearest-copy point |
| `RepeatLimitedField` | `src/implicit/domain_ops.cpp:346` | pass-through | child's at the active copy |
| `TwistField` | `src/implicit/domain_ops.cpp:467` | unit | `fdGradient` copy at `domain_ops.cpp:17` (P03.2: the shared helper) |
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
| `NormalizedField` | `src/implicit/decorators.cpp:344` | unit | child's, re-normalised; its magnitude for `valueAt` was a separate FD (`trueGradMag`, `:368`, e=1e-4) — since P03.2 the child's `trueGradientAt` |
| `ScaleField` | `src/implicit/decorators.cpp:403` | pass-through | child's at `p/s`, flipped for `s < 0` |

Totals at `db8ffef`: unit 11, pass-through 13, blend-shortened 5, raw FD 1. "Pass-through" inherits the child's class: unit for a unit child, raw under a
`WindingNumberField`, so a winding node put every ancestor that forwards it off the contract
too. Since P03.2: unit 12, raw FD 0 — the contract holds for every tree. (Line numbers in the
table are `db8ffef`'s.)

The 2D layer: `PrimitiveField2D` (`include/dualc/implicit2d.h:49`, normalised FD, `(0,1)`)
and `Circle2D` (`src/implicit/field2d.cpp:41`, analytic) are unit, and **no library path
calls them**: `RevolveField` and `ExtrudeField` take their 3D gradient by FD of their own
`valueAt`, never through the profile's `gradientAt`.

**The callers** of `gradientAt` in the library are two, both in
`src/implicit/implicit_field.cpp`: `edgeHit` (`:65`, normalises the result) and
`closestSurfacePoint` (`:78`, `:82`, the Newton step, whose only caller is the sampler's miss
fallback, `src/sampler.cpp:80`; since P03.2 it reads `gradientAt` only for the normal at the
projected point). `QefSolver::add` normalises again
(`src/internal/qef.cpp:149`). The forwarding nodes above call their children's. The GLSL
codegen and the C ABI expose no gradient.

## Against the raw plan's census

The raw plan's § 2 census holds; its line numbers match the code at `db8ffef`. It omitted
`RepeatLimitedField`, `GradedOnionField`, `GradedOffsetField` and `XorField` /
`DifferenceField` by name (all pass-through), filed `NormalizedField` with neither list (unit,
re-normalised), and did not mention the 2D layer (unit, uncalled).

## The true-gradient path

P03.2 (2026-10-20). `ImplicitField::trueGradientAt(p)` is ∇f with its magnitude; its
default is a central difference of `valueAt`, step 1e-4 — the code moved out of
`NormalizedField` into `src/internal/fd_gradient.h`, which also holds the direction stencil
the two `fdGradient` copies (`domain_ops.cpp`, `lift.cpp`) merged into, rounding unchanged.

| Override | file:line | What |
| --- | --- | --- |
| `ImplicitField` (default) | `src/implicit/implicit_field.cpp:73` | central FD, e=1e-4 |
| `SphereField`, `PlaneField` | `src/implicit/primitives_tierA.cpp:63`, `:111` | analytic: unit radial (zero at the centre), the stored normal |
| `GridField` | `src/implicit/grid_field.cpp:108` | the trilinear gradient, unnormalised; `gradientAt` normalised it until P04 ([20](20-gridfield-normal.md)) |
| `WindingNumberField` | `src/implicit/winding_field.cpp:76` | the straddle-aware FD that was its `gradientAt` |
| offset, onion, transform, scale | `src/implicit/decorators.cpp:54`, `:91`, `:288`, `:411` | forwarded; `sign(f)` for onion, the rotation for transform, `sign(s)` for scale |
| mirror, repeat, repeatLimited | `src/implicit/domain_ops.cpp:104`, `:192`, `:351` | forwarded from the active copy, reflected for mirror |
| union, intersection, difference, xor | `src/implicit/combinators.cpp:63`, `:91`, `:125`, `:158` | the active operand's, by the same rule as `gradientAt` |

The rest — smooth booleans, `mix`, both graded decorators, `elongated`, `normalizedOf`,
twist/bend/displace, the lifts, `MeshSource`, the 30 FD primitives — keep the default.

The two callers: `NormalizedField::valueAt` divides by `|child->trueGradientAt(p)|`
(`test_tpms.cpp`'s regression unchanged and green), and `closestSurfacePoint` steps
`q - G·f/|G|²` with `G = trueGradientAt(q)`, the normal at the result from `gradientAt`; it
returns false when `G` vanishes or `f`, `G` are not finite.

**The Newton step, measured** (`tests/test_gradient.cpp`; |f| at the projected point, mean,
new step vs the old `q - ĝ·f`):

| Field | Points | New | Old |
| --- | --- | --- | --- |
| gyroid, wavelength 1 (\|∇f\| ≈ 2π), lattice points with \|f\| ≤ 0.3 | 53 | 9.6e-4 | 0.953 |
| winding cap, open tetrahedron of edge 4 (\|∇w\| ≈ 0.25) | 24 | 3.2e-3 | 0.054 |
| winding cap, edge 0.25 (\|∇w\| ≈ 4) | 24 | 3.2e-3 | 0.190 |
| winding cap, edge 1 (\|∇w\| ≈ 1.013) | 24 | 3.2e-3 | **1.7e-3** |

New beats old at every point of the first three rows (the tests assert it). Newton's
residual is the same at every edge length, because w is scale-invariant. At edge 1, though,
the cap is near-metric, the two steps almost coincide, and w's curvature lets the old step
land closer at 10 of 24 points. The test does not claim that case; it was measured once and is
recorded here.

**Tests** (`tests/test_gradient.cpp`, 10 cases, tag `[gradient]`): the default equals the
FD; sphere, plane and grid are analytic; every forwarding node's and every default node's
`trueGradientAt` matches its own FD wherever two FD steps a decade apart agree (more than
150 of 343 points per node); `normalizedOf` reads the virtual (a field declaring slope 8
normalises `2x` to `x/4`); both Newton tables above; the winding `gradientAt` is unit; and
the property: over 343 points, every catalogue primitive, TPMS, strut lattice, mesh, winding,
grid and lift source and every node over a gyroid and over a winding child has
`|gradientAt| ≤ 1 + 1e-9`, and `= 1 ± 1e-9` outside the smooth booleans, `mix` and
`elongated`. **Proven red**: with the old Newton step and the raw winding `gradientAt`
restored, 4 of the 10 cases fail (both Newton cases, the winding unit case, the property).

**Chosen in-run (pending owner review):**
- the plan names the decorators as the forwarders; the hard booleans, mirror and both repeats
  forward too (the census's pass-through class), so a winding child keeps its straddle-aware
  FD through them; the graded decorators do not, because their `gradientAt` drops ∇t;
- `SphereField::trueGradientAt` is zero at the centre (no fallback direction for a magnitude);
- the winding Newton test runs at edge 4 and 0.25, not 1 (above);
- `MeshSource` keeps the FD default; its `gradientAt` is unit and exact off the surface, so
  forwarding it would be cheaper. The plan does not list it.

## The `GridField` normal (P04)

P04 (2026-10-20) made `GridField::gradientAt` continuous across cell faces; `trueGradientAt`
and so the Newton step are unchanged. The angle across a face at ε = 10⁻⁶ cell went from
0.245 rad to 4.9e-7; the face counts of four baked demos are identical. Mean vertex deviation
is −9 % on a smooth blend and +35 % on the sharp CAD pair. The full record, the timing and
the owner-review note: [20](20-gridfield-normal.md).

P04b (2026-10-21) limited the node stencil at creases (minmod, a ratio switch, a crease mask);
no variant kept the CAD pair's mean deviation at or below the cell gradient's, so
`gradientAt` is that gradient's direction again and #34 is PARTIAL (E12 open). The variants
table: [20 § P04b](20-gridfield-normal.md#p04b--a-limited-node-stencil-measured).

## Gates

P03.1 changed comments and docs only, no behaviour: full gate `python3 scripts/check.py`
(Linux, GCC 15, Ninja, Release) 30 passed, 0 failed, 1 skipped (configure), 46 TUs
recompiled after the header edit with 0 DualC-origin warnings, **ctest 350 / 350**;
`--docs --strict` green. No field node or codegen moved, so the GPU tier was not run.

P03.2 (same host): full gate 30 passed, 0 failed, 1 skipped (configure), 0 DualC-origin
warnings, **ctest 360 / 360** (the 10 new `[gradient]` cases); `--docs --strict` green. The
CPU side of the `normalize` node moved, so the GPU tier ran: `cmake --build build-gl -j &&
python3 scripts/check.py --gpu --strict --build-dir build-gl` — **parity 82 / 82**, the
count at `aef902e`.

---

← Back to the [block README](README.md) · the [Roadmap index](../README.md).
