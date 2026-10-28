# The `GridField` normal — minmod shipped

Part of [20 — a continuous `GridField` normal](README.md) (#34, E12). Session of 2026-10-28,
branch `feat/e12-minmod-normal`, on the plan approved that day: the owner's pick among the
stencils [01 § P04b](01-stencils-measured.md#p04b--a-limited-node-stencil-measured) measured,
the measurement rebuilt as a committed tool, one confirming round, then the ship.

## The decision

[D-76](../../../decisions/03-settled-continued.md). The record left four options and
Giacomo picked the first: **minmod, after one confirming round** — fully continuous, the
crease test green, D's 45–90° edges −46 %, B's max −15 %, at the cost of case A's mean +18 %
(0.0129 → 0.0153 on a ≈ 550 mm part, the max unchanged at 1.713). The others: the 40° angle
mask (A +6 %, no gain on D, continuous only where no cell is flagged); one more measured
round first (an untested ENO-style side pick, chosen by the smaller second difference); keep
the cell gradient and close E12 by decision. The plan's rule of 2026-10-19, *A's mean at or
below the cell gradient's*, was the rule that sent P04b back to the cell gradient; the
owner's pick replaces it with the acceptance below. The second pick of the day: the P04b
bench, gone with `/tmp/p04b`, is **rebuilt as an opt-in tool** this time
([13](../../../command_reference/13-dualc_mesh_deviation.md)), #11's trigger (b) having
fired twice (#34, #35) on scratch evaluators.

The fallback where the blend vanishes — the interpolant's own direction, then `(0,0,1)` —
was P04's in-run choice; it is the owner's by this plan.

## What changed

`GridField::gradientAt` (`src/implicit/grid_field.cpp`, `minmodAxis`) is the normalised
trilinear blend of the 8 corner nodes' **minmod** gradients: per node and axis, the forward
and backward differences, the smaller in magnitude when they share a sign, zero when they
disagree, the one existing difference at the grid's boundary, zero on a degenerate axis; on
the fly, 8 × 7 sample loads, nothing stored. `trueGradientAt` is untouched: the
interpolant's exact gradient, the Newton step's. The GPU path did not move (its normal is a
finite difference of the uploaded texture), so `--gpu` was not run.

**Tests.** *`gradientAt` is continuous across cell faces* is live again (`[grid][e12]`,
the `[.]` dropped): the worst angle over the 12 probes is linear in ε, as under P04. The
crease test (*keeps a face's normal one cell from a sharp edge*) stays green at 0.0°. The
Newton-step test is unchanged. P03's identity `gradientAt == trueGradientAt/|…|` to 1e-15
(`tests/test_gradient.cpp`) is exactly what a one-sided stencil removes: it is now unit length
to 1e-12 and `dot > 0.95` with the interpolant's direction, **measured 0.99910** at the
gyroid's cell-centre probe (cell 1/16, |∇f| ≈ 2π: first order in the cell, as expected).
ctest 408 → 409.

**The regression guard** (the owner's ask at the review, 2026-10-28): *a baked box contours
with its edges sharp, not chamfered* (`[grid][e12][accuracy]`). A `BoxField` baked at 17³
over ±1.5 (cell 0.1875) with its faces on node planes (±0.75 = 4 cells), contoured at depth
5 over the same root; the output's worst `|box.valueAt|` must be < 0.01, its 20–45° dihedral
band empty and its ≥ 45° band over 160 edges. With the faces on node planes the interpolant
is exact at the faces and edges, so the normal alone decides the edge; off the planes
(±0.9) the interpolant itself rounds it and every stencil chamfers (408 edges with minmod,
435 with the cell gradient). Measured: minmod 0 chamfer / 180 crease / worst 0.0015; the
cell gradient 0 / 180 / 0.0022; P04's central blend (`f939a35`) **72 / 156 / 0.042, red** —
the guard catches the stencil that softened edges. On the CLI, the same cube baked by a
`mesh` node (`cube.obj`, `bake=16`, depth 5) went from 768 chamfer edges and max 0.0298
under the cell gradient to 0 and 0.0200 under minmod; the L-bracket (`bake=32`, depth 6)
from max 0.0130 to 0.0078, its concave edges' bands kept. ctest 409 → 410.

## The tool, checked against the record

`dualc_mesh_deviation` ([13](../../../command_reference/13-dualc_mesh_deviation.md),
`-DDUALC_BUILD_BENCH=ON`) is 21/04's and 17/20's method as a binary: `|valueAt|` of the exact
graph at every vertex, the dihedral histogram over interior edges in the record's seven bins,
the per-vertex shift against a reference OBJ. `scripts/bench_e12.py` runs the cases through
an old and a new `dualc_field` and prints the tables below. **Acceptance:** the same binary
(`6c01f03`'s) passed twice as *old* and *new* on A–D reproduced every figure of
[01 § Measured](01-stencils-measured.md#measured)'s *old* rows to the digit — deviation max
and mean, every dihedral bin — with a zero shift. The tool reads the OBJ the run wrote, so
it measures what shipped, not an in-process copy.

## Measured

Same host as [01](01-stencils-measured.md#measured) (Linux, GCC 15, Ninja, Release, i7-920,
`--threads 0`, run from `data/`); wall the median of 3, RSS the max of 3; *old* is `6c01f03`
(`main`), *new* this branch. Cases A–D are the record's; **E is new**: `mesh(sphere.obj,
bake=16)` at depth 6, about four octree cells per bake cell — the only case where E12's
artifact, false creases along bake-cell faces, is visible (A–D contour at the bake's own
resolution). Deviation is `|valueAt|` of the exact graph at every output vertex.

| Case | Wall s old / new | RSS MB old / new | Faces | Verts | Face lists |
| --- | --- | --- | ---: | ---: | --- |
| A `difference(mesh(opA,bake=128), mesh(opB,bake=128))`, depth 7 | 5.85 / 5.87 | 497 / 496 | 108,660 | 54,330 | identical |
| B `smooth-union(mesh(torus,bake=64), mesh(cube,bake=64), k=1)`, depth 6 | 0.69 / 0.70 | 29 / 29 | 3,564 | 1,784 | identical |
| C `difference(mesh(sphere,bake=64), mesh(cube,bake=64))`, depth 6 | 0.87 / 0.90 | 82 / 82 | 29,496 | 14,752 | identical |
| D `mesh(bunny,bake=128)`, depth 7 | 5.81 / 5.77 | 293 / 294 | 120,954 | 60,603 | identical |
| E `mesh(sphere,bake=16)`, depth 6 | 0.41 / 0.42 | 51 / 51 | 21,756 | 10,880 | identical |

| Case | Dev max old → new | Dev mean old → new | Shift max / mean |
| --- | --- | --- | --- |
| A (≈ 550 mm) | 1.713 → 1.713 | 0.01293 → **0.01529** (+18 %) | 2.57 / 0.0292 |
| B (extent 2.7) | 0.00851 → **0.00724** | 0.00186 → 0.00194 (+4 %) | 0.0187 / 0.00134 |
| C (extent 2) | 0.00520 → 0.00519 | 0.000259 → 0.000299 (+16 %) | 0.00094 / 0.00014 |
| D (bunny) | 0.01248 → **0.01193** | 0.002681 → 0.002695 (+1 %) | 0.0026 / 0.0002 |
| E (sphere, oversampled) | 0.00753 → 0.00725 | 0.003837 → 0.003841 (+0 %) | 0.0028 / 0.0006 |

| Case | < 1 | 1–5 | 5–10 | 10–20 | 20–45 | 45–90 | ≥ 90 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 125,727 → 124,387 | 25,565 → 26,874 | 4,228 → 4,190 | 3,432 → 3,463 | 1,796 → 1,686 | 1,247 → 1,270 | 995 → 1,120 |
| B | 1,178 → 1,094 | 2,462 → 2,606 | 994 → 912 | 630 → 676 | 82 → 58 | 0 → 0 | 0 → 0 |
| C | 22,842 → 22,444 | 19,422 → 20,438 | 1,404 → **786** | 0 → 0 | 552 → 552 | 24 → 24 | 0 → 0 |
| D | 36,505 → 31,237 | 63,579 → 67,117 | 30,387 → 33,594 | 19,051 → 24,123 | 19,154 → 17,277 | 9,839 → **5,273** | 2,765 → 2,659 |
| E | 16,038 → 12,702 | 12,696 → 18,240 | 3,780 → **1,632** | 84 → **24** | 30 → **0** | 6 → 36 | 0 → 0 |

**The reading.** Every A–D figure is P04b's minmod row to the digit (A 0.01529, B 0.00194 /
0.00724, C 0.000299, D 0.00270 and 5,273 edges at 45–90°): the re-implementation is the
measured stencil. The face lists are identical in all five cases, as under P04: the signs
are the grid's, only the Hermite normals and so the QEF positions moved. Wall and RSS are
within run noise. On E, the oversampled sphere, the artifact E12 named goes: the 5–10°,
10–20° and 20–45° bands — the creases along bake-cell faces — fall from 3,780 / 84 / 30 to
1,632 / 24 / 0, the max deviation drops 4 % and the mean is flat; 30 edges move into the
45–90° band (36 from 6), on a mesh of 32,634 edges, not chased. C's seam stays put (552 / 24)
and its 5–10° band halves.

**Against the plan's acceptance table** (every row green but one): face lists identical ✓;
A mean ≤ 0.0160 and max 1.713 ✓; B max ≤ 0.00851 and mean ≤ 0.00194 ✓; D mean within
+10 % ✓ and 45–90° ≤ 60 % of old ✓; E's 5–45° bands below old's ✓; wall ≤ 1.2× ✓; ctest
green ✓. **C's mean is +16 %, above the +10 % the plan wrote** — beside the P04b figure
0.000299 it quoted, which *is* +16 %: the threshold was inconsistent with the number next to
it, not the measurement with the pick. Shipped on the P04b figure the owner read; flagged in
the PR text for the review.

## Gates

Full gate `python3 scripts/check.py` green at every commit (ctest 408 with the tool, 409 with
the stencil, 410 with the guard), counts on the [block README](../README.md); `--gpu` not run (no GLSL path
moved; the last parity count is P03.2's 82 / 82). Docs gate `--docs --strict` green at the
close. `docs_loss_audit.py` on the split of page 20 into this folder: clean.

---

← Back to [20 — the `GridField` normal](README.md) · the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
