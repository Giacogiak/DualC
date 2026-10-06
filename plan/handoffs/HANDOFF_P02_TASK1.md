# Handoff — P02.1: the parity gate made binding (#31)

Unit P02.1 of [`plan/11_P02_parity-gate.md`](../11_P02_parity-gate.md), session of 2026-10-06,
branch `ci/parity-gate` (pushed to `origin`; **not merged**, `main` untouched). The record is
[roadmap 17/13](../../docs/roadmap/17-code-audit-and-hardening/13-parity-gate-binding.md).
This handoff points at it and adds what the next unit and Giacomo need.

## Starting state

- Local `main` held P01's merge (`a774d53`, 12 commits ahead of `origin/main`, **not
  pushed**). `ci/parity-gate` branches from it, so pushing the branch also published those
  commits to the branch. `origin/main` is still the initial public release.
- There was no "merged run" on GitHub, because `main` was never pushed. So the blocker
  check read the last two runs of `ci/gate-workflow` instead (below). Run 4 is the merged
  tip less the one docs-only commit.

## What changed, and why

1. **`c864ea6` — the `gpu` job is required.**
   - `continue-on-error: true` is removed from `.github/workflows/gate.yml`, and the job
     comment now says why it is trustworthy.
   - The `check_parity` docstring and section banner in `scripts/check.py` no longer call #31
     deferred. The behaviour is unchanged: it still fails on a case count other than
     `parity_expected_cases` (73).
   - The comment on the target in `examples/CMakeLists.txt` now carries the CTest-half
     decision.
2. **`239000a` — the deliberate break.** The GLSL `opXor` was changed to
   `max(min(a,b), max(a,b))`, on the GLSL side only.
3. **`a25a94c` — the revert.** `examples/field_glsl.cpp` is byte-identical to `a774d53`.
4. **`764f7e3` — the docs.** The record is a new page, 17/13, because 17/04 is at 14.3 KB
   and the decisions index at 15.2 KB of the 15 KB cap. The pages updated:
   - the 17/04 #31 body, now DONE with a pointer to 17/13;
   - the 17 README: its index, the ledger row, and *Latest verification* (ctest 256 → 270,
     parity 73/73 now in CI);
   - the decisions row for #31, now DONE: it keeps its row and ID, only the status cell
     changes, as that page's rule says;
   - a dated note in 17/09/03 and in 12/02;
   - `design/10`, which owns the preview == export invariant;
   - `command_reference/12` README, a stale statement only; no flag changed;
   - the 20/03 job table, plus a dated note there;
   - the `STRUCTURE.md` workflow row;
   - the roadmap README snapshot: NEXT = Phase 3, #33. In the milestones window #31 comes
     in and the 2026-09-21 *Cap relief* row goes out.
5. **The CTest half: no `add_test`.** `check.py --gpu` stays the one runner. The binary
   exits 0 when its mesh cases skip, so a ctest exit code would reopen the hole that the
   count assertion closes.
6. **Not done: the optional `--snapshot` smoke (step 3).** D-49 says the workflow adds no
   check of its own. So the smoke would have to be a new `--gpu`-tier check in `check.py`,
   with its own proven-to-fail step. That is out of scope; it is recorded in 17/13 as
   untracked.

## Measured numbers (conditions)

These are GitHub job wall times. Parity lines are read from the gate's notices through the
public annotations API.

| Run | Commit | Result | `gpu` job |
| --- | --- | --- | --- |
| [37446307563](https://github.com/Giacogiak/DualC/actions/runs/37446307563) (run 3, ci/gate-workflow) | `f064ea8` | success | 2 min 58 s, 73/73 (7.5 s), `continue-on-error` |
| [37447937660](https://github.com/Giacogiak/DualC/actions/runs/37447937660) (run 4, ci/gate-workflow) | `301b60c` | success | 3 min 0 s, 73/73 (11.0 s), `continue-on-error` |
| [37458550242](https://github.com/Giacogiak/DualC/actions/runs/37458550242) — **the proven-to-fail run** | `239000a` (break) | **failure** | 2 min 43 s, step `Parity` red: `[xor] FAIL maxErr 8.92e-01 (4096/4096 over tol)`, `72/73 passed`; docs, ubuntu, windows (10 min 17 s) and macOS all green |
| [37467164857](https://github.com/Giacogiak/DualC/actions/runs/37467164857) | `a25a94c` (revert) | **success** | 1 min 57 s, 73/73 (3.29 s); every job green |

Local checks, on Ubuntu with GCC 15 and Ninja:

- **Before the commits.** On `build-gl`, with Mesa on the owner's X display `:0`, the
  `check.py --gpu --build-dir build-gl` run gave 73/73 (3.5 s). With the break applied it
  gave **72/73**, failing on the same `xor` line. The full `python3 scripts/check.py` passed
  (31 checks, 0 failed, ctest 270/270, 104 s), as did `--docs --strict`.
- **After the revert and the docs.** The full `python3 scripts/check.py` passed again
  (ctest 270/270, 71 s), and so did `--docs --strict`.

## Exit criteria, one by one

1. **The `gpu` job is required and green on the branch's HEAD with 73/73 asserted, and the
   proven-to-fail run is named.**
   - `continue-on-error` is gone (`c864ea6`). The count is asserted by `check_parity` against
     `parity_expected_cases` = 73.
   - The latest code-bearing HEAD, `a25a94c`, is green with 73/73: run 37467164857.
   - The proven-to-fail run is 37458550242, named in 17/13.
   - The final HEAD (this handoff's commit) is pushed. The driver verifies it with
     `plan/remote_run_check.py`. Its URL cannot be quoted here, because writing it down
     would take another commit and so another run.
2. **`python3 scripts/check.py` is green locally; `--gpu` is green locally because the harness
   moved.** Both passed, see above. The harness source is identical to `main`, and `--gpu`
   was also run green on it before the break.
3. **The 17/README ledger, *Latest verification*, the decisions row and the roadmap README
   snapshot agree on #31's state.** All four say DONE on 2026-10-06 and link 17/13.
   `status-vocab`, `status-sync` and `decisions-index` are green.
4. **The handoff names the run URL, the CTest-half decision, and what Giacomo must merge.**
   This file: the runs table, item 5 above, and the list below.

## What I got wrong on the way

- **`git revert --no-edit HEAD -q`.** `-q` is not a `git revert` option, so the first revert
  attempt failed with a usage error. Nothing was lost.
- **The revert's commit message got ahead of the evidence.** It says "the gpu job went red
  on it", and it was written before the red run had finished. The run did go red, so the
  message holds. But it was not pushed until the red result had been read.
- **The session was interrupted** while it waited on the red run, and was resumed from the
  committed state. The work was not redone.

## Open, and what the next unit needs to know

- **For Giacomo:**
  1. Merge `ci/parity-gate` into `main`. It carries P01's commits too, because `main` was
     never pushed. Then push `main`.
  2. In the GitHub UI, add **`gpu`** to the required status checks of `main`'s branch
     protection, next to `docs` and the three `build` jobs. Only the UI can make a merge
     wait for a job.
- **For P03 (#33):**
  - Every job is now required, so a new job (sanitizers, `-Werror`, the layering assertion,
    the dialect leak) should be born with `continue-on-error`, as D-49 allows, until it is
    green twice.
  - The `gpu` job builds with `-DDUALC_BUILD_TESTS=OFF`. Keep it that way, or it pays for
    the whole test build.
- **Untracked candidate:** a `--gpu`-tier `--snapshot` smoke of `dualc_raymarch` and
  `dualc_field_view` (see 17/13 § What changed).
- **Still not fixed** (carried from P01): `dualc_gen_demo all --dir <missing>` aborts. The
  `gpu` job works around it with `mkdir -p data`.
- **Report-only lists.** `index-staleness` lists the same 10 files P01 left. It is INFO only,
  and `--strict` passes.
