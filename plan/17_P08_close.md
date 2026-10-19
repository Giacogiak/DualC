# P08 — Close the plan

§ 3 P08 of [the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md); D-51 governs
what happens to `plan/`. Branch `feat/hygiene-and-editor`. Never push.

### T1. Harvest, lint, loss audit, final gates, PR text for the owner

1. **Harvest the handoffs.** Read every handoff in `plan/handoffs/` and route each fact not yet
   in the repo by the table in `docs/README.md`. Measurements, rejected approaches and
   deviations go into a dated record page. Use the next free child of block 17, or a section of
   the editor block for P07's facts, as 21/09 did for the last run. The record also lists the
   corrections the sessions made to the plan files, and every **Chosen in-run (pending owner
   review)** item, so the owner can review them in one place.
2. **Status.**
   - Mark #34, #35 and #53 in 17's and 14's tables with their evidence. #53 stays PARTIAL until
     the owner's remote run is read.
   - Update `docs/roadmap/README.md` § Current focus and § Next up. Add a milestones row and
     keep the rolling five-row window (D-43).
3. **Semantic lint.** Run `/docs-semantic-lint`: this run touched `src/` and `docs/design/`.
   Record the run in roadmap 19's lint log as earlier runs are recorded.
4. **Loss audit.** Run `scripts/docs_loss_audit.py` over `plan/` (`--annex plan/`) and triage
   it. **Do not delete `plan/` in this unit**: the driver still needs the manifest and this
   unit's handoff to verify it. The deletion (D-51) is one commit, made after the driver marks
   P08 done.
5. **Final gates:** `python3 scripts/check.py --clean`, `--gpu --build-dir build-gl`, and
   `--docs --strict`.
6. **Handoff for the owner.** It contains:
   - the push command `git push -u origin feat/hygiene-and-editor`;
   - the compare link `https://github.com/Giacogiak/DualC/compare/main...feat/hygiene-and-editor`;
   - the CI jobs to read for #53;
   - the PR description: what changed, the ABI version, the ctest and parity counts, the
     decisions, the in-run choices awaiting review.

#### Acceptance criteria

- `plan/`'s facts are harvested; every local gate is green; the handoff carries the push command,
  the compare link and the PR text.

## Exit criteria

- The run is closed on the local branch, ready for the owner's push and PR.
