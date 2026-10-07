# Handoff — P04.1: #32 Batch A, correct the record and the mechanical fixes

Unit P04.1 of [`plan/13_P04_test-coverage.md`](../13_P04_test-coverage.md) § T1, session of
2026-10-07, branch `ci/test-coverage` (pushed to `origin`; **not merged**, `main` untouched).
The branch starts at local `main` = `ab5ce1e`, which is `ci/build-hardening`'s head, so it
carries P01–P03 too; `origin/main` is still the initial release. The record is
[roadmap 17/15](../../docs/roadmap/17-code-audit-and-hardening/15-test-coverage-batches.md).
This handoff points at it and adds what the next unit and Giacomo need.

## What changed, and why

Commits: `b38e43a` (code), `e637913` (docs), then this handoff.

- **The record corrected** (T1 step 1):
  - 17/04 #32: a dated note, and the body line flipped to `PARTIAL: DONE (2026-10-07, Batch A); the rest PLANNED`.
    The note gives the counts: **38** unguarded `Approx(0.0)`, not 36; **17**
    `REQUIRE_THROWS_AS` in five files (`test_parallel` 6, `test_cancel_progress` 4,
    `test_field_graph` 4, `test_field_glsl` 2, `test_primitives` 1), not one. Both were
    re-measured on `ab5ce1e`.
  - 17/06: C42 and C43 are DONE, with C43's disposition corrected (matrix jobs run on
    separate machines; the collision is `ctest -j` within one tree). C46 has the corrected
    count.
- **`.margin(1e-12)` on all 38** (step 2): 29 in `test_primitives.cpp`, 3 in `test_octree.cpp`
  (inside a ternary), 3 in `test_lift.cpp`, and one each in `test_mesh_bvh.cpp`,
  `test_validation.cpp` and `test_field_graph.cpp` (split over two lines). A margin only
  widens the tolerance, so nothing that passed can fail.
- **`approx-zero`**, a new fast check in `scripts/check.py`, with the fixture pair
  `scripts/check_fixtures/approx-zero/{pass,fail}`. It fails on `Approx(<zero literal>)` in
  `tests/` with no `.margin(` after it, across lines. **`--metrics` was wrong**: it counted
  guarded uses too and read 70. It now counts what the check counts and reads 0.
- **A working directory per test case** (step 3): `tests/test_main.cpp`, empty until now,
  registers a Catch2 `EventListenerBase`. Each case `chdir`s into
  `<build>/tests/dualc_test_work/<pid>-<n>`; the directory is removed on pass and kept on
  failure. This is the generic fix the plan allowed ("or unique temp names"); it covers the
  `"nope.stl"` that three cases write by bare name, and any future name.
- **`test_field_graph.cpp`** writes `dualc_fg_box.obj` by bare name, no longer under the
  machine-wide `temp_directory_path()`.
- **A second `-j` race the audit did not name:** `cli_gen_demo` ran in `build/examples` and
  rewrote `cube.obj`, `sphere.obj`, `torus.obj` and `bracket.obj` while the other CLI smoke
  tests read them. It now runs in `build/examples/cli_gen_demo_out/`, created at configure
  time (`examples/CMakeLists.txt`).
- **The serial-`ctest` decision: kept, with a new reason.** The dated line is in
  [17/09/02](../../docs/roadmap/17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md),
  and `check_ctest`'s docstring is rewritten. Correctness no longer requires serial. Cost does
  not justify `-j`: 86 s serial against 57 s at `-j 8`, because the engine parallelises inside
  each case, and oversubscription exposes the timing-dependent cancel/progress tests (17/14
  findings 8 and 9).
- **Docs:**
  - 17/README: the index entry for 15, the #32 row (PARTIAL), and *Latest verification*,
    which adds the `ctest -j 8` reading;
  - 17/09/01 and 03: dated notes;
  - the roadmap README: the focus line and the milestones window (+#32 Batch A, −#49);
  - `STRUCTURE.md`: the `test_main.cpp` row; `check_fixtures` now says "per fast check";
  - the plan file, corrected below.

## Measured numbers (conditions)

Local: Ubuntu, GCC 15, CMake 4, Ninja, Release, 8 cores.

| What | Before (`ab5ce1e` + margins) | After |
| --- | --- | --- |
| unguarded `Approx(0.0)` | 38 (`--metrics` printed 70) | 0 (`--metrics` and `approx-zero`) |
| `ctest -j 8` ×3 | 270/270 each; 49, 55, 59 s | 270/270 each; 57, 61, 63 s |
| `ctest -j 4` | — | 270/270, 64 s |
| `ctest` serial | — | 270/270, 86 s (78–90 s inside the gate) |
| `dualc_tests` as one process | — | 249 cases, 589,341 assertions, all pass; `dualc_test_work/` empty after |
| `ctest` total | 270 | 270 (no tests added in Batch A) |
| gate checks | 31 | 32 |

- **The full gate** `python3 scripts/check.py` passed on the final code: a forced clean
  rebuild, 254 TUs, 0 DualC-origin warnings, ctest 270/270, 32 checks.
  `--docs --strict` passed.
- **`--selftest`**: 30 fixture runs, 0 wrong.
- **`strace -e chdir,openat`** on `FileMeshResolver caches by path` shows the case `chdir`ing
  into `dualc_test_work/<pid>-0`, opening `dualc_fg_box.obj` relative to it, and `chdir`ing
  back.

## Proven to fail

- Fixture `approx-zero/fail` (a two-line `Catch::Approx(0.0)`) answers FAIL.
- On the real tree, removing the margin from `tests/test_lift.cpp:28` turned the check red
  with that file and line. Restoring it turned it green.

## What I got wrong on the way

- **My first regex for the check** also matched `Approx()` and `Approx(.)`: `0*` allowed an
  empty literal. It was fixed before the commit to require at least one zero digit, and the
  pass fixture gained a `0.25` case so a non-zero literal starting with `0` is proven not to
  match.
- **The first full gate ran while I was still writing docs**, so its `links` check saw links to
  the record page before the page existed. It was re-run.
- **`--clean` only reconfigures; it does not rebuild**, so its warnings scan is skipped as
  incremental. A meaningful scan needed `cmake --build build --target clean` first. This is
  worth knowing for every unit.
- **The record page's first heading carried a date.** `heading-status` caught it only once the
  file was tracked, at the pre-commit hook. The date moved to the body.
- **`ctest -j 8` was already green before the fix.** The hazards were latent, not
  reproducible on demand. The before/after evidence is therefore structural (the `strace`,
  the directory layout), not a red-to-green run.

## Corrections to the plan file

- The record page is `17/15`, not `17/13`: 13 and 14 are #31's and #33's records. Both
  mentions in `plan/13_P04_test-coverage.md` are corrected with a dated note.

## Open, and what the next unit needs to know

- **For Giacomo:** merge `ci/test-coverage` at the approve gate. It sits on
  `ci/build-hardening`'s head; if that branch is merged first, this one is its fast-forward
  plus 3 commits.
- **For P04.2 (Batch B), what it inherits:**
  - The six T2 bullets are untouched: `partitionCubeEdges` on 256 configurations, refinement
    convergence, the four `ContourerParams` knobs, `interpolateNormals` and normal content,
    content-checking CLI smoke tests, and the strut-lattice oracle.
  - **[17/04](../../docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md) is at
    15,305 of 15,360 bytes.** Batch B's DONE entry for #32 will not fit. Split 04 into a
    same-numbered folder first (`04-engineering-quality/`). There are 117 mentions of
    `04-engineering-quality.md` under `docs/`, `plan/` and `scripts/`. The live ones to
    rewrite are in `docs/` (outside `docs/raw/`, which is immutable), `plan/`, and
    `scripts/check_data.json`'s `ordered_item_files`. `scripts/docs_loss_audit.json` is a
    past audit's output and keeps its paths.
  - The roadmap README has about 20 bytes of headroom (15,338 / 15,360). The milestones row
    for #32 DONE must replace this unit's Batch A row, not sit beside it.
  - A test may write files by bare name now; the listener isolates it. CLI smoke tests
    still share `build/examples` and must keep distinct output names.
  - Keep new tests off timed poll counts (17/14 finding 9); the `sanitize` job runs them under
    ASan.
  - Coverage measurement (`gcov`/`llvm-cov`, C51's other half) is #32's per 17/04 #33. It is
    not in T2's bullets: Batch B should either do it or record why not.
- **Remote run:** run [37584887029](https://github.com/Giacogiak/DualC/actions/runs/37584887029)
  on `e637913` (the code and docs commits) was green on every job. Job wall times:
  - `build`: ubuntu-24.04 272 s, macos-14 270 s, windows-2022 649 s. The listener's
    `_getpid` and `getpid` paths compile under `/WX` and `-Werror` and pass.
  - `sanitize`: 857 s.
  - `gpu`: 148 s.
  - `docs`: 6 s.

  The commit that adds this handoff and the run note in 17/15 triggers one more run. The
  driver's `remote-run` check verifies that run.
- **Still open from earlier units:**
  - the Linux C-ABI link failure (17/14 finding 7);
  - the macOS `parallelForPolled` flake (finding 8);
  - `dualc_gen_demo all --dir <missing>` aborts. Batch A sidestepped it by creating the
    directory at configure time.
