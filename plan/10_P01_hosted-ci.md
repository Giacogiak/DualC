# P01 — Hosted CI: the gate as a GitHub Actions job (#51)

Section 2 of [the raw plan](../docs/raw/2026-10-05-hosted-ci-plan.md); read its sections 0
and 1 first (why now, the verified repo facts). The remote is
<https://github.com/Giacogiak/DualC>; the gate is `scripts/check.py`, documented in
[17/09](../docs/roadmap/17-code-audit-and-hardening/09-local-checks-gate/README.md).

**Push rule for this unit.** Work on a branch named `ci/gate-workflow`. Push that branch to
`origin` as often as needed to see runs. **Never push `main`, never force-push.** The merge
to `main` is Giacomo's, at the approve gate after this unit.

### T1. Write the workflow, make it green on the required jobs, record it

1. `.github/workflows/gate.yml`, triggered on `push` to **every branch** (a feature branch
   with no pull request must still run — that is how this unit sees its runs), on
   `pull_request`, and on `workflow_dispatch`; `concurrency` keyed by ref with
   cancel-in-progress; `timeout-minutes` on every job.
2. **Job `docs`** — `ubuntu-latest`, Python 3: `python3 scripts/check.py --docs --strict`.
3. **Job `build`**, matrix, each running the full default gate
   `python scripts/check.py --build-dir build` (serial `ctest` included):
   - `ubuntu-24.04` — `ninja-build` from apt, GCC;
   - `windows-2022` — Visual Studio 17 2022, `python` on `PATH`;
   - `macos-14` — `brew install ninja`, AppleClang; **`continue-on-error: true`** on the
     first run (never built there); keep it required if green, fix in-unit when the cause is
     ours, or leave it allowed-to-fail with the log quoted in the record.
   Cache the FetchContent sources (`build/_deps/*-src`, keyed on the `CMakeLists.txt` hashes)
   and `ccache` on Ubuntu/macOS through the `CMAKE_CXX_COMPILER_LAUNCHER` environment
   variable. **Do not cache object files**: the warnings scan is only meaningful on a clean build.
4. **Job `gpu`** — `ubuntu-24.04`, `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev
   libxi-dev xvfb libgl1-mesa-dri`, `LIBGL_ALWAYS_SOFTWARE=1`; configure with
   `-DDUALC_BUILD_GLSL_PARITY=ON`, build the harness, run
   `xvfb-run -a python3 scripts/check.py --gpu --build-dir build`. **`continue-on-error:
   true`** — it is P02's experiment, started here so its first result is on file.
5. Push the branch, read the runs through the public API
   (`https://api.github.com/repos/Giacogiak/DualC/actions/runs?branch=ci/gate-workflow`;
   `plan/remote_run_check.py` does this for the driver), iterate until `docs` and the Linux
   and Windows `build` jobs are green. A failing job's log needs authentication: reproduce
   locally, or state in the handoff exactly which job and step failed for Giacomo to read.
6. **Docs, on the same branch**, routed by the table in `docs/README.md`:
   - block 20 → folder `docs/roadmap/20-public-delivery/README.md` + children, headings and
     the `#47-…` / `#49-…` anchors verbatim, inbound links retargeted (the gate's
     `stale-paths` and `links` checks prove it), plus a child for **#51**: the workflow's
     shape, the matrix choice, what each job does and does not assert, the first green run's
     URL and each job's duration, the macOS and gpu first results;
   - 17/09 README: half (b) now exists, pointing at 20's #51 child; 17/09/03 *What this does
     and does not move* gets a dated note for #31, #32, #33;
   - the five "there is no CI" statements rewritten (`README.md`, `AGENTS.md`,
     `STRUCTURE.md`, `scripts/check.py`'s argparse description, the 17/09 README opening):
     the gate is what CI runs; locally it is still the one command. A README badge is optional;
   - `STRUCTURE.md`: a row for `.github/workflows/gate.yml`;
   - decisions: #31 row — its trigger's state after the first gpu run; #33 row — trigger
     reworded (CI exists; the rest is P03); a new `D-NN` row (next free id read from
     `docs/decisions/README.md`) for the policy *CI runs the gate and nothing else;
     allowed-to-fail jobs are experiments, never a definition of green*;
   - `docs/roadmap/README.md`: § Current focus (NEXT = P02), the five-row milestones window
     (oldest row out), block 20's row in § Principal blocks.

Out of scope: branch protection (Giacomo, in the GitHub UI); a release or artifact job; any
new check in `check.py`; making `gpu` or `macos` required.

## Exit criteria

- `.github/workflows/gate.yml` exists on branch `ci/gate-workflow`, pushed to `origin`, and
  the latest run for the branch's HEAD has conclusion `success` with `docs`, `build
  (ubuntu-24.04)` and `build (windows-2022)` green (`python3 plan/remote_run_check.py` exits 0).
- `python3 scripts/check.py` is green locally on the branch (docs tier strict, configure,
  build, warnings, ctest).
- Block 20 is a folder with a #51 child carrying the run URL and durations; the five "there
  is no CI" statements are rewritten; `STRUCTURE.md`, the decisions rows and the roadmap
  README snapshot are updated as listed in step 6.
- The handoff names: the run URL, each job's duration, the macOS and gpu first results and
  their cause if red, and what Giacomo must do next (merge, branch protection).
