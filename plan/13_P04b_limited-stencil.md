# P04b — A crease-aware node stencil for the `GridField` normal (#34)

Added 2026-10-21 by Giacomo's review of P04.1
([17/20 § Measured](../docs/roadmap/17-code-audit-and-hardening/20-gridfield-normal.md#measured)).
The continuous node-gradient normal improved smooth bakes (case B: max −39 %, mean −9 %;
case D's 45–90° edges halved) but raised the sharp CAD pair's mean deviation by 35 %
(case A: 0.0129 → 0.0174, max unchanged, face lists identical). Sharp features are DualC's
core promise, so the owner chose: **keep continuity, but stop the node stencil from
averaging across a crease.** Branch `feat/hygiene-and-editor`. Never push.

### T1. Limit the node difference near a crease, measured against P04.1

1. Reproduce P04.1's four cases A–D first (17/20 § Measured: the same graphs, depths,
   `--threads 0`, run from `data/`, the same scratch deviation evaluator and dihedral
   histogram). Use binaries at `e4be7ec` (*old*, the cell gradient) and at P04.1's commit
   (*P04*). Rebuild the evaluator if it is gone, and record where it lives.
2. In `GridField`'s node gradient (`src/implicit/grid_field.cpp`), replace the plain central
   difference on each axis with a **limited** one. The default to try first is **minmod**: of
   the two one-sided differences `(v[i+1]-v[i])/h` and `(v[i]-v[i-1])/h`, take the one with
   the smaller magnitude when they have the same sign, and 0 when they differ. If the whole
   node gradient collapses, fall back as P04.1 does (`trueGradientAt`'s direction, then
   (0,0,1)).
   - If minmod does not meet the targets in step 4, try in this order:
     - **a ratio switch**: use the one-sided difference on the side whose value is closer to
       the node's, when the two differ by more than a factor *r*. Try r = 2, 4 and record each.
     - **a crease mask**: where the two one-sided differences differ by more than an angle or
       ratio threshold, the cells on each side keep their own cell gradient (P04.1's old
       behaviour, locally). Continuity then holds away from creases only.
   - Record every variant tried with its numbers. A rejected variant is a fact for the record,
     not something to drop.
3. Keep P04.1's tests, adjusting only what a limiter must change:
   - The continuity test on the smooth sphere must stay green **unchanged**: minmod equals the
     central difference to second order on smooth data.
   - Add a crease test: a baked box, or the `difference` of two boxes. At a probe one cell
     from a sharp edge, the normal must be within 1° of the true face normal of its side.
     Show it red on P04.1's stencil and green on the new one.
4. **Targets**, against the *old* and *P04* rows of 17/20, same host and the same method:
   - A (CAD pair): mean deviation ≤ old (0.0129), max ≤ old (1.713).
   - B (smooth blend): mean and max no worse than P04 + 5 % (0.00169 / 0.00519).
   - C, D: mean no worse than old + 5 %; D's 45–90° edge count no worse than P04 + 10 %.
   - Face and vertex counts still identical to old in all four cases.
   - Wall time within 1.2× of P04's.
5. **Outcome.** The record is a new section on 17/20, appended and dated, not a rewrite. It
   holds the variants table, the chosen stencil and why, and the dihedral histograms.
   - Targets met: #34 stays DONE. Note the stencil in 17/04 § #34 in one line.
   - No variant meets the A target without losing B's gain: **Chosen in-run (pending owner
     review)**: ship the variant closest to the targets that keeps A's mean ≤ old. If none
     keeps A's mean ≤ old, restore the old cell gradient for `gradientAt` (keeping
     `trueGradientAt`, the tests that still apply, and the record). Then set #34 back to
     **PARTIAL** in 17's table, 17/04 and `docs/roadmap/README.md`, with E12 reopened and
     the measured reason.
   - Either way, the design page (`docs/design/08-implicit-field-layer.md`) states the
     shipped stencil, rewritten in place.
6. Run the full gate with `--clean` (P04.1's warnings scan compiled nothing, so zero warnings
   is unproven). `--gpu` is not needed unless a GLSL path moved. Run the docs loss audit
   **per touched page** (`--source <page>`, as P03.2's handoff shows), not over the whole
   tree, which takes longer than ten minutes. If it must run long, run it in the foreground
   with a long timeout, never as a background call that can be killed.

*Corrected by P04b.1 (2026-10-21):* step 3's "minmod equals the central difference to second
order on smooth data" is wrong. Minmod picks a one-sided difference, first order. The smooth
continuity test stayed green on minmod anyway, because continuity comes from nodes sharing a
value, not from the stencil's order. And on the fallback path the cell gradient is restored, so
the continuity test cannot stay green there; P04b.1 hid it (`[.]`) instead of deleting it
(17/20 § P04b).

#### Acceptance criteria

- The crease test is red on P04.1 and green now. The smooth continuity test is unchanged and
  green.
- The four cases are measured against old and P04, the targets are met or the fallback above
  is applied, and the record and status say which.
- Full gate (`--clean`) and `--docs --strict` green; committed, not pushed.

## Exit criteria

- #34's status in 17, 17/04 and the roadmap README matches what shipped.
- Nothing pushed.
