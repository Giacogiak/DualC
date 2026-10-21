# Handoff — P04.1: a continuous `GridField` normal, measured; #34 DONE

Unit P04.1 of [`plan/13_P04_gridfield-normal.md`](../13_P04_gridfield-normal.md) § T1. It is
the only unit of P04, so it also closes P04's exit criteria. Session of 2026-10-20, branch
`feat/hygiene-and-editor`: nothing pushed, no remote touched, `main` untouched. The record is
[17/20](../../docs/roadmap/17-code-audit-and-hardening/20-gridfield-normal.md): what changed,
the tests, both measurement tables, the reading, the Newton step and the in-run choices. This
handoff points at it and adds what P05–P08 need.

## What changed, and why

1. **The normal** (step 1): `GridField::gradientAt` (`src/implicit/grid_field.cpp`) is now
   the normalised trilinear blend of per-node gradients. Each node's gradient is a central
   difference over its two neighbours, one-sided at the grid boundary, and zero on a
   degenerate axis. Where the blend vanishes it falls back to `trueGradientAt`'s direction,
   then `(0,0,1)`. `trueGradientAt` is unchanged: the interpolant's exact gradient. The
   header's class comment and the override comment say so (`include/dualc/implicit.h`).
2. **On the fly, not stored**: 48 sample loads per call, no extra memory. A precomputed
   variant (3 floats per node) was built only for the timing: same wall time, +48 MB on
   the large pair. Under the plan's rule (store only if on the fly is > 1.2× slower), on the
   fly stays.
3. **Tests** (step 2), `tests/test_grid_field.cpp`:
   - new: *continuous across cell faces*, red before and green after (numbers below);
   - new: *closestSurfacePoint on a GridField steps by the trilinear gradient and returns the
     continuous normal* (step 4);
   - the *unit length and outward* test is unchanged.
4. **Docs:** 17/20 is new and indexed in 17's README. 17/19: the census row and the
   true-gradient row for `GridField`, plus a short § *The `GridField` normal (P04)*. 17/04
   § #34: the status line is now DONE, the last bullet struck through with its evidence.
   17's README: the #34 status cell is DONE, and Latest verification gains the local counts.
   Design 08 § 10.1 and the Sources bullet. Roadmap README: two "NEXT is the rest of #34"
   clauses rewritten.

## Measured, and under what

Linux, GCC 15, Ninja, Release, i7-920 (4 c / 8 t), `--threads 0`. Wall is the median of 3,
RSS the max of 3, both `/usr/bin/time`. *old* is the binary at `e4be7ec`.

| What | Result |
| --- | --- |
| Continuity test, worst angle at ε = 1e-2 / 1e-4 / 1e-6 cell (sphere baked at 13³, 12 probes) | old **0.2451 / 0.2450 / 0.2450 rad** (red) → new **4.9e-3 / 4.9e-5 / 4.9e-7** (green) |
| A: `difference(mesh(opA,bake=128),mesh(opB,bake=128))` depth 7 | wall 5.61 → 5.52 s (pre 5.54); faces 108,660 = 108,660; dev mean 0.0129 → **0.0174 (+35 %)**, max 1.713 = 1.713 |
| B: `smooth-union(mesh(torus,bake=64),mesh(cube,bake=64),k=1)` depth 6 | wall 0.66 → 0.65 s; faces 3,564 =; dev max 0.00851 → **0.00519**, mean 0.00186 → 0.00169 |
| C: `difference(mesh(sphere,bake=64),mesh(cube,bake=64))` depth 6 | faces 29,496 =; dev mean +6 %, max = |
| D: `mesh(bunny,bake=128)` depth 7 | faces 120,954 =; dev flat; 45–90° dihedrals 9,839 → 4,523 |
| Full gate `python3 scripts/check.py` | GATE_RESULT |
| `python3 scripts/check.py --docs --strict` | 28 checks, 27 passed + `index-staleness` INFO (report-only, the pre-existing list), PASS |
| GPU tier | **not run**: no GLSL path, field node, `field_graph.cpp`, `field_glsl.cpp` or `parity_expected_cases` moved. `GridField`'s GPU side uploads `values()` and differentiates the texture |

Deviation is `|valueAt|` of the exact graph at every output vertex, from a scratch evaluator
(`/tmp`, not committed) linked against `libdualc_examples_fieldgraph`. It reproduces 21/04's
*old* figures exactly. No Hausdorff or vertex-shift tool exists in the tree. Because the face
lists are identical old vs new, the max/mean vertex shift is on 17/20 too, along with the full
dihedral histogram (the plan's fallback).

## P04's exit criteria and T1's acceptance, one by one

- **"17's table and § Latest verification are updated, and nothing is pushed."** — 17 README
  #34 row: "**DONE** 2026-10-20 — last batches [18], [19], [20]". Latest verification gains
  "Local, branch `feat/hygiene-and-editor`, not yet in CI: ctest **362** (2026-10-20, Linux,
  [20]), parity **82/82** ([19])". The CI counts (302/304) stay as CI's. `git branch -vv`:
  `feat/hygiene-and-editor` has no upstream, and no push was run.
- **"The continuity test is red before the change and green after, and the measurements are
  recorded."** — red/green in the table above (the red run: the test against the unchanged
  `grid_field.cpp`, `worst[0] = 0.2451 < 0.1` FAILED). Measurements on 17/20 § Measured, a
  summary on 17/19 and 17/04.
- **"#34 is DONE (or honestly PARTIAL), the full gate is green, and the work is committed, not
  pushed."** — DONE: every bullet of 04 § #34 is struck through with a date and a record. The
  bullet's defect (a discontinuous normal) is gone, as the test shows. The CAD-pair cost is
  recorded as a trade-off for the owner, not hidden; see below. Gate: the row above.

## Chosen in-run (pending owner review) — for P08

Also on 17/20:

- **(a) The fallback** where the node blend vanishes (a feature under two cells): the
  cell gradient's direction, then `(0,0,1)`. The plan names none.
- **(b) One P03 assertion changed.** `tests/test_gradient.cpp`, *GridField trueGradientAt is
  the trilinear gradient, unnormalised*, asserted `gradientAt == trueGradientAt/|…|` to 1e-15.
  That identity is what the plan removes, so it failed (2.1e-8) after the change. It is now
  `< 1e-6`, with a comment. This is a passing test edited to pass, and the plan's own
  step 1 is the reason; it is flagged here for that reason.
- **(c) The record is a new page, 17/20**, not appended to 19 as the plan says ("P03's census
  page"). 19 has a short § with the headline numbers and the link. The full tables would have
  pushed 19 near the cap.

## The trade-off the owner should see (P08)

The continuous normal is as accurate or better on smooth geometry (B: max −39 %, mean
−9 %; D: flat, fewer creases). On the **sharp-edged CAD pair A, the mean vertex deviation is
+35 %** (max unchanged), and on C +6 %. A likely cause, **not measured**: the two-cell
central stencil mixes both faces' values at nodes next to a sharp edge. The plan fixed the
method and it was kept. The options are on 17/20: a one-sided or limited stencil
(untested), or the old normal, which reopens E12.

## What I got wrong on the way

- First RSS column was in 10³ KB. 21/04's figures (29 MB on B) are 2²⁰-byte MB; converted
  before committing.
- I first wrote "MB as on 21/04" as if 21/04 stated its unit. It does not; that was an
  inference from the matching B figure, so I removed the claim.

## Loss audit

LOSS_AUDIT

## Left open — for P05–P08

- **P08 milestone row**: #34 DONE is not yet in the roadmap README's five-row milestones
  window. That is harvest work.
- **P08 semantic lint**: this unit edited `src/` and `docs/design/08` again.
- **Cap room now:** 17 README 15,336 / 15,360 (the 09 and 10 index entries were condensed
  to make room; their facts are on those pages), 17/04 15,296, roadmap README 15,199,
  17/19 12,955, 17/20 ≈ 8.0 KB, design 08 ≈ 11.3 KB.
- Next free IDs (read from the tree): **D-67, #63, block 22**; block 17's next
  page is **21**.

## Closed by hand (2026-10-21, the owner's session)

The session stopped before committing because the whole-tree loss audit hit the 10-minute
background limit. The owner's session ran it per touched page instead
(`--base e4be7ec --head <scratch commit of the working tree> --scope docs/ --source <page>`,
with a scratch triage under key `e4be7ec` and the `3d73220` thresholds). The whole-tree
untriaged count was 10,053, the baseline noise earlier units also saw. Every deleted docs line
of the diff was checked by hand, and all are intentional:
- the #34 status lines (PARTIAL → DONE) and the roadmap README's "NEXT … #34";
- design 08's two `GridField` sentences, rewritten in place;
- 17/19's two `GridField` census rows, updated;
- 17 README's 09 and 10 index descriptions, shortened for cap room with the same facts;
- the dropped batch dates in the #34 cell, still in 17/04 § #34.

Giacomo's review of the +35 % on case A: **keep continuity, limit the stencil at creases**. That
is the new unit P04b, `plan/13_P04b_limited-stencil.md`, and it runs next. The warnings scan of
this unit compiled nothing, so P04b runs the gate with `--clean`.
