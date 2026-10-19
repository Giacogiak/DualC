# P07 — The Boletus implementation plan

§ 1 *Boletus* and § 3 P07 of [the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md).
DualC branch `feat/porous-shell`; Boletus branch `feat/porous-shell-plan`. Never push either.

### T1. Write the Boletus plan in the Boletus repository

1. In `~/Documents/Boletus`:
   - Read its `AGENTS.md` / `CLAUDE.md` and `docs/README.md`.
   - Check `git status` is clean. If it is not, write the plan into DualC's block instead, record
     why, and skip the Boletus commit.
   - `git switch -c feat/porous-shell-plan` from the current branch's tip.
2. Write `docs/raw/<run date>-porous-shell-plan.md` in Boletus's raw-plan style, with the content
   the raw plan § 3 P07 lists. Its source is the DualC docs P06 wrote: parameter names and
   defaults exactly as the parser has them. Add its row to Boletus's `docs/raw/README.md`. Read the
   next free D-NN from Boletus's decisions only to *name* the rows the plan would open; do not add
   rows to Boletus's decisions.
3. Run Boletus's docs check if it has one (`scripts/`), and commit on the Boletus branch only.
4. In DualC, add a pointer line in the block's README and in
   `docs/roadmap/15-boletus-handoff.md` (in the DualC-retains section or where requests to Boletus
   are listed), naming the Boletus branch and file.

#### Acceptance criteria

- The Boletus commit exists on `feat/porous-shell-plan`, and Boletus's other branches are
  untouched.
- The DualC pointer is committed, and `python3 scripts/check.py --docs` is green.

## Exit criteria

- Boletus has a ready-to-run implementation plan for the porous node.

← Back to the [plan](README.md).
