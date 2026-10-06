# P04 — The test-coverage ledger (#32)

Section 5 of [the raw plan](../docs/raw/2026-10-05-hosted-ci-plan.md). The item is
[17/04 #32](../docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md#32-test-coverage-ledger);
its findings are C42–C54 in
[17/06](../docs/roadmap/17-code-audit-and-hardening/06-findings-ledger-testing.md). Two
batches, one session each. Work on `main` after the previous merge, on a branch named
`ci/test-coverage`; push that branch only, never `main`, never force-push. The full gate
before every code commit; the record page for both batches is `17/13` (or the next free
number), indexed in 17/README, with the before/after counts.

### T1. Batch A — correct the record, then the mechanical fixes

1. Correct the drifted claims in 17/04 #32: unguarded `== Approx(0.0)` comparisons are
   **38**, not 36; `REQUIRE_THROWS_AS` is **17 across five files** (`test_parallel` 6,
   `test_cancel_progress` 4, `test_field_graph` 4, `test_field_glsl` 2, `test_primitives` 1),
   not "exactly one outside the field-graph parser". Correct finding C43's disposition in
   17/06: matrix jobs on separate machines do not collide; the collision is `ctest -j`
   within one tree.
2. `.margin(…)` on the 38 unguarded `Approx(0.0)` comparisons; `check.py --metrics` should
   read 0 afterwards; a gate check may ratchet it there (proven to fail if added).
3. A per-test working directory or unique temp names in `tests/test_field_graph.cpp` (and
   any sibling), so `ctest -j` is safe; then revisit the gate's serial-`ctest` decision
   ([17/09/02](../docs/roadmap/17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md))
   — lift it or keep it, one dated line.

#### Acceptance criteria

- 17/04 #32 and 17/06 C43 state the corrected counts and disposition.
- `check.py --metrics` reports 0 unguarded `Approx(0.0)`; `ctest -j 8` passes three times
  in a row on the branch; the serial-`ctest` decision has its dated line.
- `python3 scripts/check.py` green locally; the record page `17/13` exists with Batch A's
  counts; the handoff names what Batch B inherits.

### T2. Batch B — the missing tests

Each small and named by the audit:

- `partitionCubeEdges` on all 256 sign configurations (pure, O(1), allocation-free; 9 of
  256 today in `tests/test_cube_components.cpp`), with manifoldness invariants per configuration;
- a refinement-convergence test: error against an analytic surface decreases as `maxDepth` rises;
- the four never-varied `ContourerParams` knobs each exercised at least once against a
  measurable effect;
- `interpolateNormals`' sharp/smooth claim unit-tested; output normals checked for unit
  length and outward orientation against the field gradient;
- CLI smoke tests asserting content (PNG signature and dimensions, STL triangle count, 3MF
  zip validity), not exit code only;
- the strut-lattice oracle that *"certifies less than it claims"*
  ([17/07](../docs/roadmap/17-code-audit-and-hardening/07-repeat-tiling-fix.md)): strengthen
  the sampling or restate the comment to what it checks.

#### Acceptance criteria

- Every bullet above has a test or a recorded reason it has none; new test files have
  `STRUCTURE.md` rows; the `ctest` count in 17/README *Latest verification* is updated.
- `python3 scripts/check.py` green locally; the branch's latest run green on its required
  jobs (`python3 plan/remote_run_check.py` exits 0).

## Exit criteria

- 17/04 #32's body line is DONE, or PARTIAL naming exactly what remains; C42–C54 in 17/06
  carry their dispositions; the decisions rows touched and the roadmap README snapshot agree
  (NEXT = the rest of #34).
- The branch's latest run is green on its required jobs (`python3 plan/remote_run_check.py`
  exits 0).
- The handoff lists the before/after counts for both batches and what Giacomo must merge.
