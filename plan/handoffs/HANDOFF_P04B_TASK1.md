# Handoff — P04b.1: a limited node stencil, measured; the cell gradient restored, #34 PARTIAL

Unit P04b.1 of [`plan/13_P04b_limited-stencil.md`](../13_P04b_limited-stencil.md) § T1, the
only unit of P04b, so it closes P04b's exit criteria too. Session of 2026-10-21, branch
`feat/hygiene-and-editor`, start commit `f21f4a6`. Nothing pushed, no remote touched, `main`
untouched. The record is
[17/20 § P04b](../../docs/roadmap/17-code-audit-and-hardening/20-gridfield-normal.md#p04b--a-limited-node-stencil-measured):
the variants table, the histograms, the reading, the tests and the in-run choices. This
handoff points at it and adds what P05–P08 need.

## Outcome in one paragraph

No stencil, continuous or masked, kept case A's (the CAD pair's) mean deviation at or below
the old cell gradient's 0.0129. So the plan's fallback applies: **`GridField::gradientAt` is
the normalised cell gradient again** (the code at `e4be7ec`, byte-identical meshes in all four
cases). `trueGradientAt` is unchanged. **#34 is PARTIAL with E12 reopened** in 17's table,
17/04 and the roadmap README. The closest continuous candidate for the owner is minmod: A
+18 %, B's max −15 %, D's 45–90° edges −46 % against old, crease test green.

## What changed, and why

1. **Code.** `src/implicit/grid_field.cpp`: `gradientAt` restored to `e4be7ec`'s body, plus a
   four-line comment saying why (#34, E12, 17/20). `include/dualc/implicit.h`: the class
   comment states the cell-gradient normal and that it jumps across faces, and the
   `trueGradientAt` comment keeps P04's "(exact for valueAt, so the Newton step's;
   discontinuous across cell faces)".
2. **Tests** (`tests/test_grid_field.cpp`, `tests/test_gradient.cpp`):
   - **new** *`GridField::gradientAt` keeps a face's normal one cell from a sharp edge*:
     `BoxField([-0.9]³, [0.9]³)` baked at 13³ (cell 0.25, faces off the node planes), three
     probes one cell from an edge, each within 1° of its face normal. **Red on P04.1's
     `grid_field.cpp` (9.66°), green on the shipped one (0.0°)**. Both runs were done by
     swapping the source in `build/`.
   - *continuous across cell faces*: body unchanged, its tags now `[.][grid][e12]` (hidden,
     not run by ctest) and its comment explains why. It is red on the cell gradient (0.245
     rad at every ε, as P04.1 measured). **Chosen in-run (pending owner review)**: hidden
     rather than deleted, so a future stencil can be checked with `dualc_tests "[e12]"`.
   - P03's assertion `gradientAt == trueGradientAt/|…|` is **back to `< 1e-15`**, its
     original form. P04.1 had loosened it to 1e-6.
   - The Newton-step test is renamed *… returns gradientAt* (it said "the continuous
     normal"). Its body is unchanged.
   - ctest count stays **362**: +1 crease test, −1 hidden continuity test.
3. **Docs.** 17/20: a pointer note under the intro and the new § *P04b — a limited node
   stencil, measured*. 17/04 § #34: the status line is PARTIAL, and the last bullet is
   un-struck as **OPEN again 2026-10-21** with the measured reason. 17 README: the #34 cell is
   PARTIAL, the 20 index line is updated, and the ctest date is 2026-10-21. Roadmap README:
   both #34 clauses are PARTIAL. 17/19: the census row and a 4-line P04b paragraph under
   § *The `GridField` normal (P04)*; the row's stale `:142` is now `:151`. Design 08: both
   `GridField` sentences rewritten in place.
4. **Plan file corrected** (`plan/13_P04b_limited-stencil.md`, a dated note above the
   acceptance criteria). Step 3's "minmod equals the central difference to second order on
   smooth data" is wrong, because minmod picks a one-sided, first-order difference. The
   continuity test stays green on minmod only because the nodes share values. The acceptance
   line "the smooth continuity test is unchanged and green" cannot hold on the plan's own
   fallback path.

## Measured, and under what

Linux, GCC 15, Ninja, Release, i7-920 (4 c / 8 t), `--threads 0`, run from `data/`, the same
expressions as 17/20 § Measured. *old* = binary at `e4be7ec`, *P04* = binary at `f939a35`
(both from `/tmp/p04`, copied to `/tmp/p04b`). **Step 1 reproduction:** before any change,
both binaries reproduced every figure of 17/20 § Measured exactly, for all four cases:
deviation, faces, verts and the dihedral histogram.

| Stencil | A mean | B mean / max | C mean | D 45–90° | crease test | continuity |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| target | ≤ 0.0129 | ≤ 0.00177 / 0.00545 | ≤ 0.000271 | ≤ 4,975 | < 1° | green |
| old = **shipped** | 0.01293 | 0.00186 / 0.00851 | 0.000259 | 9,839 | 0.0° | red |
| P04 central | 0.01739 | 0.00169 / 0.00519 | 0.000273 | 4,523 | 9.7° | green |
| minmod | 0.01529 | 0.00194 / 0.00724 | 0.000299 | 5,273 | 0.0° | green |
| ratio r = 2 / 4 | 0.01521 / 0.01453 | 0.00177 / 0.00652, 0.00178 / 0.00647 | 0.000282 / 0.000281 | 12,158 / 12,087 | 0.0° | green |
| best mask (angle 2°) | 0.01300 | = old | 0.000259 | 9,839 | 0.0° | red |
| mask angle 40° | 0.01365 | 0.00169 / 0.00519 | 0.000273 | 9,787 | 0.0° | green |

17/20 has every row (ratio masks at 1.25, 1.5, 2 and 4, angle masks at 2, 5, 10, 20 and 40°)
and the D means, which stay at 0.00267–0.00270 in every variant. A's max is 1.713
everywhere. Face and vertex counts equal old's in every run.

- **Sensitivity of A's mean** (scratch variant 5): the old normal plus a fixed deterministic
  wobble of at most ~0.1° gives 0.01316 (+1.8 %); at ~0.01° it gives 0.01295. A's mean
  punishes any normal other than the cell gradient's.
- **Where P04's A increase sits** (scratch `diag`, per-vertex |f| bucketed by the vertex's
  largest incident dihedral in the old mesh): +0.0025 of the +0.0045 on 45–90° vertices,
  +0.0007 on flat ones. The 20° mask removes the crease share and leaves +0.0005 on flat
  vertices.
- **Timing** (median of 3, `/usr/bin/time`): A 5.53 / 5.71 / 5.74 s and B 0.66 / 0.69 / 0.70 s
  (old / P04 / shipped). The shipped code is old's, so the spread is host noise. Within 1.2×
  of P04. RSS ≈ 496 MB (A) and 29 MB (B), unchanged.
- **Gate**: `python3 scripts/check.py --clean` **after `cmake --build build --target clean`**:
  32 checks, 31 passed, 0 failed, 0 skipped; build 350 s; **warnings: 252 TUs compiled, 0
  DualC-origin warnings**; ctest **362 / 362**. Note: `--clean` alone only reconfigures. It
  reuses the objects, and the first run compiled only 2 TUs, so the tree was cleaned and the
  gate run again. P04.1's "zero warnings unproven" is now proven.
- `python3 scripts/check.py --docs --strict`: 28 checks, 27 passed + `index-staleness` INFO
  (report-only, the pre-existing list), PASS.
- **GPU tier not run**: no GLSL path, field node, `field_graph.cpp`, `field_glsl.cpp` or
  `parity_expected_cases` moved. `GridField`'s GPU side differentiates the uploaded texture.

**Scratch tools, where they live** (not committed, `/tmp` does not survive a reboot):
`/tmp/p04b/` holds `bench.sh` (the four cases; `bench.sh <binary> <runs> [cases]`),
`analyze` + `analyze.cpp` (P04.1's evaluator: |valueAt| of the exact graph, dihedral
histogram, vertex shift), `src/diag.cpp` + `diag` (the dihedral-bucketed comparison), and
`src/grid_field_variants.cpp` (every variant, selected with `P04B_V` / `P04B_R`). To build the
evaluators, link against `build/libdualc.a` and the `examples/libdualc_examples_*.a` set,
including `_decimate`, `_img` and `_slice` (meshoptimizer symbols).

## P04b's exit criteria and T1's acceptance, one by one

- **Exit: "#34's status in 17, 17/04 and the roadmap README matches what shipped."** — The
  shipped normal is the cell gradient, and all three say PARTIAL with E12 reopened: 17 README
  row "**PARTIAL** 2026-10-21 — E12 reopened"; 17/04 § #34 "**PARTIAL 2026-10-21** — all landed
  but the `GridField` normal" with the bullet "**OPEN again 2026-10-21**"; roadmap README "#34
  is PARTIAL (2026-10-21 …)" and "#34 PARTIAL 2026-10-21 (E12 reopened)". The docs tier's
  status-sync check passes (0 disagree).
- **Exit: "Nothing pushed."** — No `git push` was run, and `feat/hygiene-and-editor` has no
  upstream.
- **Acceptance: crease test red on P04.1, green now** — 9.66° → 0.0°, above. **The smooth
  continuity test unchanged and green**: *not met, by the plan's own fallback.* The body is
  unchanged and it is red on the restored normal, so it is hidden (above), and the plan file
  is corrected to say so. On minmod, the closest continuous candidate, it is green.
- **Acceptance: four cases measured against old and P04, targets met or the fallback
  applied, the record and the status say which** — The targets are not met (A's mean ≤ old
  fails for every continuous variant). The fallback is applied, and 17/20, 17/04, 17 README,
  the roadmap README, 17/19 and design 08 all say so.
- **Acceptance: full gate (`--clean`) and `--docs --strict` green; committed, not pushed** —
  above.

## Chosen in-run (pending owner review) — for P08

- **(a) The fallback itself.** The plan's rule, applied as written. Owner options, all
  measured: keep the cell gradient (shipped); **minmod** (continuous everywhere, crease test
  green, A +18 %); the **40° angle mask** (A +6 %, B as P04, D as old, continuous on the smooth
  sphere but not at creases). Taking either means re-applying its branch from
  `grid_field_variants.cpp` and setting #34 back to DONE.
- **(b) The continuity test is hidden, not deleted** (`[.][grid][e12]`).
- **(c) The angle mask's criterion** is the chord form `|F − B| > 2 sin(θ/2)·max(|F|,|B|)`.
  The plan says "angle or ratio" without a formula. A first `atan2` version missed a zero
  one-sided vector, and its numbers are recorded as rejected.

## What I got wrong on the way

- The first `atan2` angle mask: a node where one side is flat (F or B = 0) reads angle 0, so
  the mask missed the box's crease (9.7° at 20°/40°). The crease test caught it. It was
  replaced, and its A numbers are on 17/20.
- I first wrote the 17/20 heading with the date in it. The design page links to it, and
  design pages may not carry dates (`design-no-history`), so the date moved into the body
  before anything was committed.
- I ran the first `--clean` gate believing it rebuilt everything; it compiled 2 TUs. See Gate.
- I ran three loss audits in parallel with the gate; they hit the 590 s timeout and were
  re-run alone.

## Loss audit

Per touched page, foreground, `--base f21f4a6 --head <scratch commit-tree of the staged
index> --scope docs/ --source <page> --untriaged --triage /tmp/p04b/triage.json`, the triage
keyed `f21f4a6` with the `3d73220` thresholds. The pages: design 08, 17/04, 17/19, 17/20, 17
README, roadmap README. The whole-tree untriaged count is **10,060**, the known baseline noise
(P04.1 saw 10,053). The rows on lines this unit touched are all intentional:
- 17/04:133 and :165: the old status line and the struck bullet's numbers. The numbers stay
  in 17/20 § Measured and § Tests.
- design 08:40: the `GridField` sentence, rewritten in place.
- 17 README :48, :129 and :154: the index line, the ctest date and the #34 row.
- roadmap README :65/68: "#34 DONE 2026-10-20".

The other rows on these pages fall on lines this unit did not edit (17/20 :3, 41, 43, 75 and
96 only gained text above or below them). Every deleted docs line in `git diff -U0 -- docs/`
was also checked by hand. All are the status flips and the in-place rewrites listed above.

## Left open — for P05–P08

- **P08 review**: (a)–(c) above. #34 is PARTIAL only because of E12, and the owner's choice
  among the three measured options closes it.
- **P08 semantic lint**: this unit edited `src/` and `docs/design/08` again.
- **Cap room now:** 17/20 has **2,488 / 2,500 words** (13.8 KB). Any further P04b-style
  addition there means splitting 20 into a folder. 17 README is at 15,341 / 15,360 bytes,
  17/04 at 15,312, roadmap README at 15,318, 17/19 at ≈13.3 KB and design 08 at ≈11.3 KB.
- Next free IDs (unchanged by this unit): **D-67, #63, block 22**. Block 17's next page is
  **21**.
- The P08 milestone row for #34 should now say PARTIAL, not DONE.
