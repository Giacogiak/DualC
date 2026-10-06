# Handoff — P01.1: the gate as a GitHub Actions job (#51)

Unit P01.1 of [`plan/10_P01_hosted-ci.md`](../10_P01_hosted-ci.md), session of 2026-10-06,
branch `ci/gate-workflow` (pushed to `origin`; **not merged**, `main` untouched). The record
is [roadmap 20/03](../../docs/roadmap/20-public-delivery/03-hosted-ci.md). This handoff
points at it and adds what the next unit and Giacomo need.

## What changed, and why

- **`.github/workflows/gate.yml`** runs `scripts/check.py` and nothing else. Triggers: `push`
  (every branch), `pull_request`, `workflow_dispatch`. `concurrency` is per ref with
  cancel-in-progress, every job has `timeout-minutes`, and every step runs in bash with
  `pipefail`. The jobs:
  - `docs` — `--docs --strict`.
  - `build` — a matrix over `ubuntu-24.04`, `windows-2022` and `macos-14`, each running the
    full default gate `python scripts/check.py --build-dir build`. **All three are
    required.** macOS started allowed-to-fail and was made required after two green runs, as
    the plan says for that outcome.
  - `gpu` — Xvfb + llvmpipe, `--gpu --strict`, `continue-on-error`.
- **Caches.** ccache on Ubuntu and macOS through `CMAKE_{C,CXX}_COMPILER_LAUNCHER`; it replays
  stderr, so the warnings scan still sees every warning. The FetchContent cache holds
  `_deps/*-src`, `*-subbuild` and `geometry-central-build/deps/eigen-src`. **The plan's `*-src`
  alone was wrong**, measured locally (20/03 § Caching), and the plan file is corrected in
  step 3.
- **`.github/workflows/annotate.py`** re-prints the gate's report as annotations. Errors
  carry each `[FAIL]` line and its details. Notices carry configure, build, warnings, ctest,
  parity and the count line. Annotations can be read without a login; job logs cannot.
- **Clean-clone defects that the first run found in the docs tier** (the gate passed only on
  this machine):
  - `out_of_repo_links` lacked the five-level `../../../../../Boletus/` prefix used in 17/09/03.
  - `THIRD_PARTY.md` cited the gitignored `data/bunny.obj` as a path.
  - Both are fixed: `scripts/check_data.json` and the citation text.
- **`gpu` job deviations from the plan.**
  - It runs `mkdir -p data` before `dualc_gen_demo`, which aborts on a missing directory.
  - It runs `--strict`, because `--gpu` with no binary is SKIP→PASS (reproduced locally).
- **Docs.**
  - Block 20 is now a folder. `README.md` holds the intro and the page table; `01` is #47
    with the publication update; `02` is #49; `03` is #51, new. Headings and anchors are
    verbatim, every inbound link is retargeted (including the two in the raw plan, link
    targets only), and `index_roots` gains the folder.
  - The five "there is no CI" statements are rewritten: `README.md`, `AGENTS.md`,
    `STRUCTURE.md`, the argparse description in `check.py`, and the opening of the 17/09
    README. The glossary's *Gate* row is rewritten too.
  - The 17/09 README has a dated note on half (b), and 17/09/03 a dated note on #31, #32
    and #33.
  - Decisions: D-49 is settled (CI runs the gate and nothing else; allowed-to-fail jobs are
    experiments). #33's trigger is reworded. #31's trigger is marked as fired.
  - `roadmap/README.md`: Current focus (NEXT = Phase 2, #31), the milestones window (#51 in,
    #47 out) and block 20's row.
  - `STRUCTURE.md` has rows for both workflow files.

## Measured numbers (conditions)

The first all-green run is **run 3**, `f064ea8`:
<https://github.com/Giacogiak/DualC/actions/runs/37446307563>. Conclusion `success`, 8 min
44 s wall. Durations are GitHub's job wall times; the per-check seconds come from the gate's
own report, read from the notices.

| Job | Duration | Gate report |
| --- | --- | --- |
| docs | 6 s | 27 checks, 0 failed, 0 skipped |
| build (ubuntu-24.04) | 1 min 24 s (ccache warm) | 254 TUs, 0 DualC-origin warnings, ctest 270/270 (49.6 s) |
| build (windows-2022) | 8 min 2 s (cold: no compiler cache, empty FetchContent cache) | build 365.9 s, 254 TUs, 0 warnings, ctest 270/270 (57.6 s) |
| build (macos-14) | 1 min 42 s (ccache warm) | 254 TUs, 0 warnings, ctest 270/270 (61.8 s) |
| gpu | 2 min 58 s | parity **73/73** (7.5 s), `--strict` |

The cold Ubuntu and macOS jobs (run 2, `961e92e`) took 4 min 41 s and 5 min 20 s. On this
host (GCC 15, CMake 4.4.3), the FetchContent cache took a configure from 60 s to 3 s.

**macOS first result:** green on its first complete run (run 2) and again in run 3. **gpu
first result:** run 1 red, because the clean clone had no `data/`. Run 2 green but not strict.
Run 3 green under `--strict` with 73/73: the harness works under llvmpipe in CI.

The local gate on the branch is green: `python3 scripts/check.py` and `--docs --strict`, run
before the final commit.

## Exit criteria, one by one

1. **The workflow is on `ci/gate-workflow`, pushed, and the latest run for HEAD is
   `success` with docs, Ubuntu and Windows green.** Run 3 is green on every job. The final
   HEAD (this handoff's commit) gets its own run, and the driver verifies it with
   `plan/remote_run_check.py`. That run cannot be quoted here, because writing it down would
   need another commit and so another run.
2. **`python3 scripts/check.py` is green locally on the branch.** It was run before the final
   commit and passed.
3. **Block 20 is a folder with a #51 child (run URL, durations); the five statements are
   rewritten; STRUCTURE, decisions and the roadmap snapshot are updated.** Done, see above;
   `links`, `anchors`, `stale-paths` and `indexes` are green.
4. **The handoff names the run URL, the durations, the macOS and gpu first results, and what
   Giacomo does next.** This file.

## What I got wrong on the way

- **The first push used a FetchContent cache that would have been useless.** It cached
  `*-src` and `*-subbuild` but not Eigen's tree. A local test caught that before CI did: the
  configure failed in Eigen's update step.
- **I took a green job at face value.** Run 2 was green in 4–5 min, which looked too fast.
  No log was readable, and a SKIP is green, so the green proved little. The notices and
  `--strict` on `gpu` are the fix.
- **Polling exhausted the API.** The unauthenticated limit is 60 requests an hour, and it
  ran out mid-session. The public HTML pages work without it: the run page
  (`/actions/runs/<id>`) lists the annotations, and `/job/<id>` gives the job name and
  duration. A future unit should poll at most once every 2–3 min, or read the HTML.

## Open, and what the next unit needs to know

- **For Giacomo:**
  1. Review run 3 and merge `ci/gate-workflow` into `main`.
  2. In the GitHub UI, set branch protection on `main`, requiring `docs`,
     `build (ubuntu-24.04)`, `build (windows-2022)` and `build (macos-14)`. **Not `gpu`.**
  3. Optionally, `git config core.hooksPath scripts/hooks`; it is unchanged.
- **For P02 (#31):**
  - The `gpu` job already passes 73/73 under Xvfb + llvmpipe with `--strict`.
  - Making it binding is now mostly removing `continue-on-error`, plus the CTest-half
    decision and the prove-it-fails step.
  - The job builds with `-DDUALC_BUILD_TESTS=OFF`, and runs `mkdir -p data` and
    `dualc_gen_demo` because `check.py --gpu` runs from the repo root.
- **Not fixed (CLI scope):** `dualc_gen_demo all --dir <missing>` aborts with an uncaught
  `std::runtime_error` (exit 134). So the `AGENTS.md` recipe `dualc_gen_demo all --dir data`
  fails on a fresh clone, which has no `data/`. Recorded in 20/03. It needs a small fix and a
  command-reference line, and is a candidate for a later item.
- **`ubuntu-latest` becomes Ubuntu 26 on 2026-10-19**, according to GitHub's notice. Only
  the `docs` job uses that label, and it needs only Python.
- **`index-staleness` (report-only).** It lists topic files whose only change this session was
  a retargeted link. Their index rows are still true, and `--strict` passes.
- **`docs/raw/2026-10-05-hosted-ci-plan.md`.** Two link targets were retargeted to the
  block-20 children, because the `links` check covers `raw/` too. The words of the file are
  untouched.
