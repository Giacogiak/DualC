# P08 — Gates, repair, close

§ 1 *Checks* and § 3 P08 of [the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md); D-51
governs what happens to `plan/`. Branch `feat/porous-shell`. Never push.

### T1. Run every gate and repair until green

1. `cmake --build build -j && cmake --build build-gl -j`.
2. `python3 scripts/check.py` (the full gate: docs, configure, build, warnings, serial ctest).
3. `python3 scripts/check.py --gpu --strict --build-dir build-gl`.
4. `python3 scripts/check.py --docs --strict`.
5. `/docs-semantic-lint`: this run touched `src/` and `docs/design/`. Record its run file where
   `scripts/check_data.json` says, as earlier runs were recorded.
6. Fix every failure in its own commit, then rerun the gate that failed, and loop until all four
   are green. A failure that cannot be fixed in this unit is written into the block's record as
   an open item, with its output and what was tried. Never weaken a check to pass it.

#### Acceptance criteria

- All four gates are green, or each remaining failure is recorded in the block with its output.
- The handoff lists each gate's final line.

### T2. Harvest, statuses, the owner's review list

1. **Harvest.** Read every handoff in `plan/handoffs/` and route each fact not yet in the repo by
   the table in `docs/README.md`. The block's plan-run record page (its next free child) holds:
   - the per-unit table;
   - the measurements;
   - the corrections made to the plan files;
   - the lessons.
2. **Statuses.** The block's items DONE with evidence (or PARTIAL / DEFERRED with the trigger);
   the decision rows final; `docs/roadmap/README.md` § Current focus, § Next up and a milestones
   row, keeping the rolling five-row window (D-43).
3. **The owner's review list.** It goes in the record page and in this handoff:
   - every `Chosen in-run (pending owner review)` line, with its page;
   - the snapshot paths and the cavity counts;
   - the Boletus plan's path and branch;
   - the ctest and parity counts;
   - the rebase / push command `git push -u origin feat/porous-shell`, and the compare link
     `https://github.com/Giacogiak/DualC/compare/main...feat/porous-shell`;
   - the PR text.
4. **Loss audit:** `scripts/docs_loss_audit.py` with `--annex plan/`, triaged. **Do not delete
   `plan/` in this unit.** The deletion (D-51) is one commit, made after the driver marks P08.2
   done.
5. Final `python3 scripts/check.py --docs --strict`.

#### Acceptance criteria

- `plan/`'s facts are harvested, and every local gate is green.
- The handoff carries the review list, the push command, the compare link and the PR text.

## Exit criteria

- The run is closed on the local branch, ready for the owner's review, push and PR.

← Back to the [plan](README.md).
