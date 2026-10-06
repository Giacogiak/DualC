# P02 — The parity gate made binding through headless GL (#31)

Section 3 of [the raw plan](../docs/raw/2026-10-05-hosted-ci-plan.md). The item is
[17/04 #31](../docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md#31-make-the-cpugpu-parity-gate-binding);
its trigger, *"headless-GL CI becoming available"*, is what P01's `gpu` job tested. Work on
`main` after Giacomo's merge of P01, on a branch named `ci/parity-gate`; push that branch
only, never `main`, never force-push.

### T1. Read the gpu job's first result, then bind the gate or record why not

**Blocker check first.** Read P01's handoff and the `gpu` job's result on the merged run.
Mesa's llvmpipe exposes OpenGL 4.5 core and `R32F` is colour-renderable since GL 3.0, so
the harness's FBO should work. If the first run failed on something ours (a window-system
assumption, `cwd`, a missing package), fix it here. If llvmpipe genuinely cannot run the
harness, #31 stays DEFERRED: reword its trigger to the remaining candidate — a CPU reference
evaluator for the emitted AST — in 17/04 and in its decisions row, record the evidence, and
return `done` on that record.

**When green:**

1. Make the `gpu` job required (`continue-on-error` removed); confirm the gate's parity
   check asserts **73/73** there — `check.py --gpu` fails on a short count by design
   ([17/09/02](../docs/roadmap/17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md)).
2. Decide the CTest half: either `add_test(dualc_glsl_parity …)` guarded by
   `DUALC_BUILD_GLSL_PARITY`, so a local `ctest` with the target ON runs it too, or leave the
   gate's `--gpu` tier as the single runner. One sentence of rationale either way, in the record.
3. Optional, same job: a headless `--snapshot` smoke of `dualc_raymarch` and
   `dualc_field_view` (exit code and a PNG signature), since the window is already hidden and
   the X11 stack is paid for.
4. Prove it fails: break one shared formula on the branch, see the job go red, revert.

**Docs at close**, routed by the table in `docs/README.md`: 17/04 #31 body line → DONE
(date, run URL); the 17/README ledger row and *Latest verification* (the one home of the
counts); the decisions #31 row; a 17/09/03 note; 12/02 if it states where parity runs; the
`docs/design/` page that owns the parity convention, if one does; the roadmap README snapshot
(NEXT = P03). `command_reference/` only if a flag changed (none planned).

## Exit criteria

- Either the `gpu` job is required and green on the branch's HEAD with 73/73 asserted
  (`python3 plan/remote_run_check.py` exits 0), and the proven-to-fail run is named in the
  record; or #31 is still DEFERRED with its new trigger recorded in 17/04 and the decisions row.
- `python3 scripts/check.py` is green locally; `--gpu` green locally when the harness moved.
- The 17/README ledger, *Latest verification*, the decisions row and the roadmap README
  snapshot agree on #31's state.
- The handoff names the run URL, the CTest-half decision, and what Giacomo must merge.
