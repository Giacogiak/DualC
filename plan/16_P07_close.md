# P07 — Close the plan

§ 3 P07 of [the raw plan](../docs/raw/2026-10-14-cli-consolidation-plan.md); D-51 for what
happens to `plan/`. Branch `feat/cli-consolidation`.

### T1. Harvest, close block 21, hand over the PR

1. Read every handoff in `plan/handoffs/`; route each fact not yet in the repo by the table in
   `docs/README.md` — measurements, rejected approaches and deviations from the plan into block
   21's record (a dated entry), corrections the sessions made to the plan files as a list there.
2. Block 21's items DONE with their evidence; the decision rows final; `roadmap/README.md`
   § Current focus / § Next up and a milestones row (the rolling five-row window, D-43).
3. `/docs-semantic-lint` — this run touched `src/`-level code paths and `docs/design/`; record
   the run in roadmap 19's lint log as earlier runs are.
4. Run `scripts/docs_loss_audit.py` over `plan/` (`--annex plan/`) and triage it. **Do not
   delete `plan/` in this unit**: the driver still needs the manifest and this unit's handoff to
   verify it. The deletion (D-51) is one commit made after the driver marks P07 done, in the
   `/plan-run` session that hands back.
5. Final `python3 scripts/check.py --clean`, `--gpu --build-dir build-gl`,
   `--docs --strict`; push; `python3 plan/remote_run_check.py` exits 0.
6. Give the owner the compare link
   `https://github.com/Giacogiak/DualC/compare/main...feat/cli-consolidation` and a PR
   description: what changed, the retired tools and their replacements, the ctest and parity
   counts, the ABI version.

#### Acceptance criteria

- `plan/`'s facts harvested (the folder itself is deleted after the run); every gate green
  locally and on the remote run; the handoff carries the compare link and the PR text.

## Exit criteria

- Block 21 closed; the branch ready for the owner's PR.
