# P06 — `RepeatField` neighbour pruning (#35)

Sources: § 2 "#35" of [the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md), the
levers in `docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md` § #35, and the
cost record in `07-repeat-tiling-fix.md`. Branch `feat/hygiene-and-editor`. Never push.

### T1. Measure the baseline, then prune the neighbour set

1. **Baseline, on this host, before any change.** Re-measure 17/07's table: octet at depth 7,
   the single fold vs the 8-neighbour minimum (39 s → 91 s there). Use `--threads 1` or a fixed
   count so the runs are comparable (P02), and run each three times.
2. **Reference meshes, before any change.** Write them to the scratchpad from the current
   8-neighbour code: the octet case, a BCC case, and the off-centre sphere across a seam
   (`test_domain_ops.cpp:57`'s field). T2 compares against them.
3. **Lever (a).** In `nearestCopy` (`src/implicit/domain_ops.cpp:302-327`), visit neighbours only
   on the axes where the child's bounds overhang the home cell. The fold vs neighbour choice
   (`childFitsOnePeriod`, `:278-288`) stays as it is.
4. **Lever (b).** Skip a neighbour whose box lower bound (its distance from `p` to that copy's
   bounds) is already ≥ the current best value. This holds only where the child is
   Lipschitz-bounded; check `lipschitzBound()` and apply it only when it is safe.
   *(2026-10-21, P06.1: there is no `lipschitzBound()`, and Lipschitz-1 is not the property
   needed — it bounds the slope, not the value far away. The certificate is the new opt-in
   `ImplicitField::boundsBoundValue()`; [17/21](../docs/roadmap/17-code-audit-and-hardening/21-repeat-pruning.md).)*
5. **Lever (c).** A cell symmetry flag that restores the single fold for mirror-symmetric strut
   cells (17/07's 2026-10-07 note). Do it **only if** (a) + (b) still leave more than about 1.3×
   over the fold. Record which levers you kept.
6. **`RepeatLimitedField`.** Apply the same change to `RepeatLimitedField::evaluate`
   (`:414-445`). Keep `cellOverlaps` (`:212-262`) consistent with the new visit set.
7. **GLSL.** Leave the mirror (`examples/field_glsl.cpp:1070,1628`) unchanged unless the change
   breaks parity. Run `cmake --build build-gl -j && python3 scripts/check.py --gpu --build-dir build-gl`.

#### Acceptance criteria

- The baseline and reference meshes exist before the change. The pruned code passes the full
  gate and `--gpu`.

### T2. Prove byte-identity, record, close #35

1. **Byte-identity.** The meshes from T1's reference cases must be byte-identical to the
   references. Add a Catch2 test that checks pruned vs unpruned equality on a small case at
   field level: the same `valueAt` and `gradientAt` over a dense point set. The
   `cellOverlaps` superset property test (`test_domain_ops.cpp:290-319`) must stay green.
2. **Speed-up table.** Same host, same counts as the baseline. Put it in a new child page of
   block 17 (the next free number after P03's page), indexed in 17's README. Append a dated
   pointer in 04 § #35.
3. **Close #35.** Mark it DONE in 17's table, with the evidence.
4. **Gates.** Run the full gate and `--docs --strict`.

#### Acceptance criteria

- The meshes are byte-identical, the speed-up is recorded, and #35 is DONE.
- The gates are green. The work is committed, not pushed.

## Exit criteria

- #35 closed in 17 with its measurements; nothing pushed.
