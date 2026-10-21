# #34 — a continuous `GridField` normal

The record of the last open bullet of
[04 #34](04-engineering-quality.md#34-api-hygiene-batch) (audit finding E12), unit P04 of the
[hygiene + editor run](../../raw/2026-10-19-hygiene-and-editor-plan.md), on branch
`feat/hygiene-and-editor`, 2026-10-20. It builds on P03's `trueGradientAt`
([19](19-gradient-contract.md#the-true-gradient-path)).

> **2026-10-21, P04b:** the blend below was reverted; `gradientAt` is the cell gradient's
> direction again and #34 is PARTIAL, E12 open — [§ P04b](#p04b--a-limited-node-stencil-measured).

## What changed

`GridField::gradientAt` (`src/implicit/grid_field.cpp`) was the normalised analytic gradient
of the trilinear interpolant. That gradient is only C0 inside a cell and jumps across its
faces, so the normal did too. It is now the **normalised trilinear blend of per-node
gradients**: a node's gradient is the central difference over its two neighbours on each axis,
`(v[i+1] - v[i-1]) / 2h`, one-sided at the grid boundary (`res ≥ 2`, so a neighbour always
exists), zero on a degenerate axis. Every cell that touches a node shares its gradient, so the
blend is continuous everywhere.

- **`trueGradientAt` is unchanged**: the exact gradient of the interpolant, the one the
  value's Newton step needs ([below](#the-newton-step)).
- **Where the blend vanishes**, `gradientAt` falls back to `trueGradientAt`'s direction, then
  to the contract's `(0,0,1)`. That happens on a feature thinner than two cells, where the
  central differences cancel. **Chosen in-run (pending owner review):** the plan names no
  fallback; this one keeps the pre-P04 answer wherever the new one is undefined.
- **On the fly, not stored**: 48 sample loads per call instead of 8, no memory. A
  precomputed variant (3 floats per node, built for the timing only) was no faster; numbers
  below. The plan's rule: on the fly unless that is more than 1.2× slower.
- **No GLSL path moved.** The GPU uploads the grid's values (`values()`) and the shader's
  normal is a finite difference of the texture, so the GPU tier was not run.

## Tests

- **`GridField::gradientAt is continuous across cell faces`** (`tests/test_grid_field.cpp`,
  new). A sphere of radius 1 baked at 13³ (cell 0.25), 4 points near its surface × 3 axes,
  each snapped onto a node plane. The test measures the angle between the normals at ±ε,
  ε = 10⁻², 10⁻⁴, 10⁻⁶ cell; the worst over the 12 probes is:

  | ε (cells) | 10⁻² | 10⁻⁴ | 10⁻⁶ |
  | --- | ---: | ---: | ---: |
  | before (normalised cell gradient) — **red** | 0.2451 rad | 0.2450 | 0.2450 |
  | after (node blend) — green | 4.9e-3 | 4.9e-5 | 4.9e-7 |

  Before, the jump is about 14° and does not shrink with ε. After, the angle is linear in ε.
  The test asserts < 10⁻¹, 10⁻³, 10⁻⁵ and unit length at every probe.
- **`closestSurfacePoint on a GridField steps by the trilinear gradient and returns the
  continuous normal`** (new). It checks that the step is `q - G·f/|G|²` with `G =
  trueGradientAt(q)` to 1e-15, that the normal is `gradientAt` at the result, and that
  `|f| < 0.02` there.
- **Kept unchanged:** `gradientAt is unit length and outward`, and test_gradient's property
  case (`|gradientAt| = 1 ± 1e-9` on a baked gyroid).
- **One P03 assertion changed:** `GridField trueGradientAt is the trilinear gradient,
  unnormalised` (`tests/test_gradient.cpp`) asserted `gradientAt == trueGradientAt/|…|` to
  1e-15. That identity is exactly what this unit removes. At its cell-centre probe the two now
  differ by 2.1e-8 (second order, as expected for a smooth field), and the assertion is now
  `< 1e-6`. **Chosen in-run (pending owner review).**

## Measured

Linux, GCC 15, Ninja, Release, Intel i7-920 (4 cores / 8 threads), `--threads 0`, run from
`data/`. Wall: median of 3 and peak RSS: max of 3, both from `/usr/bin/time` (MB = 2²⁰ bytes). *old* is the
binary at `e4be7ec`, *new* this unit, *pre* the precomputed variant. Deviation is `|valueAt|`
of the exact graph (the same expression, no `bake`) at every output vertex, measured by a
scratch evaluator linked against `libdualc_examples_fieldgraph`, 21/04's method. Its *old*
rows reproduce
[21/04](../21-cli-consolidation/04-mesh-node-bake.md#the-measurements)'s figures (0.00851 /
0.00186, 1.71 / 0.0129). Shift is the distance between the same vertex in old and new.

| Case | Wall s old / new / pre | RSS MB old / new / pre | Faces | Verts |
| --- | --- | --- | ---: | ---: |
| A `difference(mesh(opA,bake=128), mesh(opB,bake=128))`, depth 7 | 5.61 / 5.52 / 5.54 | 496 / 496 / 544 | 108,660 | 54,330 |
| B `smooth-union(mesh(torus,bake=64), mesh(cube,bake=64), k=1)`, depth 6 | 0.66 / 0.65 / 0.67 | 29 / 29 / 35 | 3,564 | 1,784 |
| C `difference(mesh(sphere,bake=64), mesh(cube,bake=64))`, depth 6 (one run) | 0.82 / 0.82 / — | 81 / 81 / — | 29,496 | 14,752 |
| D `mesh(bunny,bake=128)`, depth 7 (one run) | 5.38 / 5.39 / — | 293 / 293 / — | 120,954 | 60,603 |

The face count, the vertex count and the face lists are **identical** old vs new in all four
cases: the grid's signs did not change, only the Hermite normals and so the QEF positions.
The bakes dominate the wall time. The new normal costs nothing measurable, and storing the
gradients costs 12 bytes per node (+48 MB on A) for no gain. So the default stands: on the fly.

| Case | Dev max old → new | Dev mean old → new | Shift max / mean |
| --- | --- | --- | --- |
| A (CAD pair, ≈ 550 mm) | 1.713 → 1.713 | 0.0129 → **0.0174** (+35 %) | 2.29 / 0.045 |
| B (smooth blend, extent 2.7) | 0.00851 → **0.00519** | 0.00186 → **0.00169** | 0.0098 / 0.0011 |
| C (sphere − cube, extent 2) | 0.00520 → 0.00520 | 0.000259 → 0.000273 (+6 %) | 0.00078 / 0.00012 |
| D (bunny) | 0.01248 → 0.01231 | 0.002681 → 0.002685 | 0.0022 / 0.00016 |

Dihedral angles between adjacent faces over every interior edge (degrees, old → new):

| Case | < 1 | 1–5 | 5–10 | 10–20 | 20–45 | 45–90 | ≥ 90 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 125,727 → 122,919 | 25,565 → 27,522 | 4,228 → 4,521 | 3,432 → 3,896 | 1,796 → 2,142 | 1,247 → 991 | 995 → 999 |
| B | 1,178 → 1,018 | 2,462 → 2,712 | 994 → 960 | 630 → 598 | 82 → 58 | 0 → 0 | 0 → 0 |
| C | 22,842 → 22,284 | 19,422 → 20,934 | 1,404 → 450 | 0 → 0 | 552 → 552 | 24 → 24 | 0 → 0 |
| D | 36,505 → 32,265 | 63,579 → 66,791 | 30,387 → 33,629 | 19,051 → 23,807 | 19,154 → 17,686 | 9,839 → 4,523 | 2,765 → 2,579 |

**The reading.** On smooth geometry the continuous normal is as accurate or better (B: max
−39 %, mean −9 %; D: flat) and removes creases: D's 45–90° edges halve, C's 5–10° band drops
from 1,404 to 450. Its true seam stays put (552 / 24). On the sharp CAD pair A it costs
mean accuracy, +35 %, with the max unchanged. A likely cause, **not measured**: next to a
sharp edge the two-cell central stencil mixes both faces' values into a node, where the
gradient of a cell on one side saw only that face. **Owner review (P08):** the plan fixed
this method, and it is kept. The alternatives are a one-sided or limited node stencil near
large differences (untested), or the old cell gradient, which reopens E12.
*Reviewed 2026-10-21 by Giacomo:* keep continuity, but limit the node stencil at creases.
That is unit P04b of the run, which must bring A's mean back
to ≤ old while B and D keep their gains, or else restore the cell gradient and set #34 back to PARTIAL.

## The Newton step

`closestSurfacePoint`'s default steps `q - G·f/|G|²` with `G = trueGradientAt(q)` (P03).
On a `GridField` that is the interpolant's exact gradient, which this unit did not change.
Only the returned normal reads `gradientAt`, and it is now continuous. The step no longer
relies on the normalised direction at all; the new grid test asserts both halves. Its one
library caller is the sampler's miss fallback (`src/sampler.cpp:80`, a missed edge crossing).
`MeshSource` overrides `closestSurfacePoint` with its BVH query, so only a baked node (a
`GridField`) reaches the default on a mesh graph.

## P04b — a limited node stencil, measured

2026-10-21. Giacomo's review of A's +35 %: keep continuity, but limit the node stencil at
creases (unit P04b). **Outcome: no stencil kept A's mean at or below the cell gradient's, so
by the plan's rule `gradientAt` is that gradient's direction again, #34 is PARTIAL and E12
open.**

**Method.** As § Measured, same host. The *old* (`e4be7ec`) and *P04* (`f939a35`) binaries
first reproduced every figure above. Each variant was a branch of one scratch
`grid_field.cpp`, picked by an environment variable, measured once. Faces and vertices
equal old's in every run. The scratch tools (evaluator, variant source, bench script) are in
`/tmp/p04b/`, not committed. Per interior node and axis, with `a`, `b` the forward and
backward differences:

- **minmod**: the smaller of `a`, `b` in magnitude when they share a sign, else 0;
- **ratio switch r**: that smaller one when `max(|a|,|b|) > r·min(|a|,|b|)`, else central;
- **crease mask**: if any of the cell's 8 nodes is flagged, the query returns the cell
  gradient's direction. *Ratio r* flags opposite signs or a ratio above r on an axis;
  *angle θ* flags `|F − B| > 2 sin(θ/2)·max(|F|, |B|)`, `F`, `B` the forward and backward
  vectors. A first `atan2` angle test missed a zero side (crease test 9.7° at 20°
  and 40°) and was replaced; its A means: 0.0140 / 0.0134 / 0.0142 at 10° / 20° / 40°.

Crease: the test below, worst probe. Continuity: the sphere test above. A's max is 1.713
in every row.

| Stencil | A mean | B mean / max | C mean | D mean | D 45–90° | Crease | Continuity |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| *target* | ≤ 0.0129 | ≤ 0.00177 / 0.00545 | ≤ 0.000271 | ≤ 0.00282 | ≤ 4,975 | < 1° | green |
| old, cell gradient — **shipped** | 0.01293 | 0.00186 / 0.00851 | 0.000259 | 0.00268 | 9,839 | 0.0° | red |
| P04, central | 0.01739 | 0.00169 / 0.00519 | 0.000273 | 0.00269 | 4,523 | **9.7°** | green |
| minmod | 0.01529 | 0.00194 / 0.00724 | 0.000299 | 0.00270 | 5,273 | 0.0° | green |
| ratio, r = 2 | 0.01521 | 0.00177 / 0.00652 | 0.000282 | 0.00267 | 12,158 | 0.0° | green |
| ratio, r = 4 | 0.01453 | 0.00178 / 0.00647 | 0.000281 | 0.00267 | 12,087 | 0.0° | green |
| mask, ratio 1.25 | 0.01314 | 0.00186 / 0.00851 | 0.000262 | 0.00268 | 9,839 | 0.0° | red |
| mask, ratio 1.5 | 0.01314 | 0.00186 / 0.00851 | 0.000266 | 0.00268 | 9,840 | 0.0° | red |
| mask, ratio 2 | 0.01328 | 0.00186 / 0.00823 | 0.000270 | 0.00268 | 9,839 | 0.0° | red |
| mask, ratio 4 | 0.01341 | 0.00179 / 0.00823 | 0.000266 | 0.00268 | 9,833 | 0.0° | red |
| mask, angle 2° | 0.01300 | 0.00186 / 0.00851 | 0.000259 | 0.00268 | 9,839 | 0.0° | red |
| mask, angle 5° | 0.01333 | 0.00186 / 0.00851 | 0.000267 | 0.00268 | 9,839 | 0.0° | red |
| mask, angle 10° | 0.01389 | 0.00185 / 0.00851 | 0.000273 | 0.00268 | 9,839 | 0.0° | red |
| mask, angle 20° | 0.01334 | 0.00169 / 0.00579 | 0.000273 | 0.00268 | 9,836 | 0.0° | red |
| mask, angle 40° | 0.01365 | 0.00169 / 0.00519 | 0.000273 | 0.00268 | 9,787 | 0.0° | green |

Dihedral histograms (bins as above; the shipped rows are the *old* rows of § Measured):

| Case, stencil | < 1 | 1–5 | 5–10 | 10–20 | 20–45 | 45–90 | ≥ 90 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| A, minmod | 124,387 | 26,874 | 4,190 | 3,463 | 1,686 | 1,270 | 1,120 |
| A, mask angle 40° | 123,463 | 27,682 | 4,470 | 3,504 | 1,658 | 1,226 | 987 |
| D, minmod | 31,237 | 67,117 | 33,594 | 24,123 | 17,277 | 5,273 | 2,659 |
| D, mask angle 40° | 37,986 | 63,824 | 29,983 | 18,264 | 18,690 | 9,787 | 2,746 |

**Why A resists.** Bucketing A's output vertices by their largest incident dihedral (old
mesh): of P04's +0.0045 mean, +0.0025 sits on vertices of 45–90° edges and +0.0007 on flat
ones (< 1°). The 20° mask removes the crease share and leaves +0.0005 on flat vertices. A's
mean is also fragile: the old normal plus a fixed wobble of at most 0.1° raises it to 0.01316
(+1.8 %; 0.01295 at 0.01°). Any normal other than the cell gradient's lands above old.

**Reading.** Each continuous stencil trades A for B and D. The masks keep A within 8 % but
lose D's gain (the bunny's thin parts flag) and, below 40°, the sphere's continuity. Minmod
is the best fully continuous one: A +18 %, B's max −15 % and D's 45–90° edges −46 % against
old, the crease test green. **Chosen in-run (pending owner review):** the plan's fallback,
the cell gradient, #34 PARTIAL. Owner options: keep it, take minmod, or the 40° angle mask
(A +6 %, B as P04, D as old).

**Tests.** New: *`GridField::gradientAt` keeps a face's normal one cell from a sharp edge* (a
baked `BoxField`, three probes one cell from an edge, each within 1° of its face): red on
P04's code (9.66°), green now (0.0°). The continuity test is red on the cell gradient (0.245
rad at every ε), so it is hidden (`[.][grid][e12]`), kept for a future stencil: **chosen
in-run (pending owner review)**, the plan's alternative being deletion. P03's `gradientAt ==
trueGradientAt/|…|` assertion is back to 1e-15; the Newton-step test is renamed *… returns
gradientAt*.

**Timing** (median of 3, A / B): old 5.53 / 0.66 s, P04 5.71 / 0.69, shipped 5.74 / 0.70
(old's code: the spread is noise); within 1.2× of P04.

## Gates

Full gate `python3 scripts/check.py` (same host): see the
[block README's Latest verification](README.md). No GLSL path moved, so `--gpu` was not run;
the last parity count is P03.2's 82 / 82 ([19](19-gradient-contract.md#gates)).

---

← Back to the [block README](README.md) · the [Roadmap index](../README.md).
