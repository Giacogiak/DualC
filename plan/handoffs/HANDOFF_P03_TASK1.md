# Handoff — P03.1: build hardening as CI jobs (#33)

Unit P03.1 of [`plan/12_P03_build-hardening.md`](../12_P03_build-hardening.md), session of
2026-10-06, branch `ci/build-hardening` (pushed to `origin`; **not merged**, `main` untouched).
The branch starts at local `main` (`e1e1f31`, which carries P01 and P02). The record is
[roadmap 17/14](../../docs/roadmap/17-code-audit-and-hardening/14-build-hardening-ci.md).
This handoff points at it and adds what the next unit and Giacomo need.

## What changed, and why

- **`cmake/DualCWarnings.cmake`** was a no-op stub. It now holds `dualc_target_options(<t> [VENDORED])`,
  called on every DualC target: the library, tests, examples, GL targets and C ABI. It sets
  three things:
  - the C++17 dialect, as target properties;
  - the warning level, `/W4 /permissive-` or `-Wall -Wextra -Wpedantic`, which before
    covered `dualc` only;
  - `-Werror` / `/WX` under **`DUALC_WERROR`** (option, OFF), and
    `-fsanitize=<list> -fno-sanitize-recover=all` under **`DUALC_SANITIZE`** (cache string,
    empty; GCC/Clang only, so MSVC stops the configure).

  `dualc_target_sanitize()` is the sanitizer half on its own. It is also applied to
  geometry-central (finding 5 below).
- **`CMakeLists.txt`**:
  - The global `CMAKE_CXX_STANDARD`/`_REQUIRED`/`_EXTENSIONS` are removed, and a guard
    fails a top-level configure that sets the standard as a normal variable.
  - geometry-central's and happly's include directories become SYSTEM for DualC's targets,
    only when DualC added the tree itself.
  - The **layering assertion** sits at the end of the file: `dualc`'s `LINK_LIBRARIES` and
    `INTERFACE_LINK_LIBRARIES` may name only `geometry-central` and `Threads::Threads`;
    anything else is a `FATAL_ERROR`.
- **The plan named `target_compile_features(dualc PUBLIC cxx_std_17)` for the dialect fix.**
  That line was already there. It cannot replace the globals alone: with no
  `CXX_STANDARD`, GCC compiles at its default `gnu++17`, which has extensions on. Hence the
  per-target properties and the guard.
- **`tests/CMakeLists.txt`**: Catch2 gets C++17 by name. It used to get it from the leak,
  and it detects C++17 features per TU.
- **`scripts/check.py`**:
  - `-D KEY=VALUE`, repeatable, is passed to the configure step. A configured build
    directory is re-configured in place rather than skipped.
  - A failed configure lists its `CMake Error` blocks first.
  - The `warnings` docstring is updated.
- **`.github/workflows/gate.yml`**:
  - The build matrix passes `-D DUALC_WERROR=ON`.
  - A new **`sanitize`** job runs `check.py --build --config RelWithDebInfo -D DUALC_SANITIZE=address,undefined`
    on Ubuntu 24.04. It has ccache, the FetchContent cache and a 90 min timeout. It was
    born with `continue-on-error`, and is **required** since `29baf7e`, after two green
    runs.
- **Code fixed by the new checks**: `examples/dualc_raymarch.cpp`,
  `examples/example_common.cpp`, `examples/dualc_slice.cpp` and
  `tests/test_cancel_progress.cpp` (findings 1, 3, 4, 9 below).
- **Docs**:
  - 17/04 #33 → DONE; the 17/12 pointer; the 17 README (index, ledger row, *Latest verification*);
  - the decisions #33 row → DONE; ledger rows C2/C32/C35/C41 (02) and C51 (06);
  - the 17/09/03 note; the 20/03 job table and a note there;
  - `README.md` build section, `AGENTS.md` opt-in list, `STRUCTURE.md` (the module and
    workflow rows), `THIRD_PARTY.md` (the rule is now asserted);
  - the roadmap README snapshot (NEXT = #32, Phase 4); the milestones window takes #33 and
    drops #48.

## Every warning and sanitizer finding, and what was done

1. `examples/dualc_raymarch.cpp`: `comp()` was unused (`-Wunused-function`, GCC 15, the
   wider level). **Deleted.**
2. geometry-central and happly headers raised 16 warnings in every TU that includes them
   (`-Wunused-parameter`, `-Wunused-but-set-variable`, `-Wunused-function`). **Made SYSTEM**;
   they are third-party.
3. `examples/example_common.cpp`: MSVC C4245 ×7 (`MZ_DEFAULT_COMPRESSION`, which is `-1`,
   passed as `mz_uint`), CI run 1, `windows-2022`. **Replaced with `MZ_DEFAULT_LEVEL`**, which
   is what miniz maps `-1` to (`miniz.c:6221`), so the bytes are unchanged.
4. `examples/dualc_slice.cpp:284`: GCC 13 `-Wmaybe-uninitialized` on `px`/`py` at `-O3`, CI
   run 1, `ubuntu-24.04`. A false positive. **Zero-initialised**, with a comment.
5. ASan `heap-buffer-overflow` in `Eigen::internal::handmade_aligned_free`, on the first local
   sanitized build. Eigen picks its allocator by `__SANITIZE_ADDRESS__`, so geometry-central,
   which was uninstrumented, and DualC disagreed. **geometry-central is instrumented too.**
   This is not a DualC defect.
6. **No engine finding.** 270/270 cases pass under ASan + UBSan + LSan: locally, and in CI
   runs 1 and 2.
7. `-DDUALC_BUILD_C_ABI=ON` does **not link on Linux**: `libdualc_capi.so` against non-PIC
   static libs, `R_X86_64_TPOFF32 against __tls_guard`. It was found while measuring
   warnings with every target on, and it predates this unit. **Not fixed; recorded in 17/14,
   untracked.**
8. macOS, CI run 1: `parallelForPolled does the same work and polls on the caller only`
   failed once (0.01 s). The log needs a login. It passed in every later run with no change
   touching it, and did not reproduce on Linux: 150 runs under ASan on 2 loaded cores, and
   400 in Release. **Recorded as an open flake, untracked.** The suspect is the
   throwing-body section's `ran < 100000`.
9. `a request during the collapse pass unwinds from it` failed in CI run 3's `sanitize` job.
   The test counted the timed `Sample` polls of one run and reused the count in a second
   run. I reproduced it locally on 2 cores with three busy loops. **Fixed in the test**: it
   samples on one thread, so the count is fixed. It then passed 30/30 under the same load.
   This is a test defect, not an engine one.

## Measured numbers (conditions)

These are GitHub job wall times, read from the gate's notices through the public annotations
API. Locally: Ubuntu, GCC 15, CMake 4, Ninja, 8 cores.

| Run | Commit | Result |
| --- | --- | --- |
| [37516198297](https://github.com/Giacogiak/DualC/actions/runs/37516198297) | `8e7ec2f` | the three `build` jobs red on findings 3, 4 and 8. `sanitize` green: ctest 270 in 430 s; build 431 s; job 14 min 54 s, cold ccache |
| [37518512354](https://github.com/Giacogiak/DualC/actions/runs/37518512354) | `893ba53` | **every job green**. windows 11 min 14 s (build 513 s under `/WX`); ubuntu 4 min 28 s; macOS 4 min 21 s; `sanitize` 4 min 56 s (ctest 249 s, warm ccache); every job at ctest 270 |
| [37520360014](https://github.com/Giacogiak/DualC/actions/runs/37520360014) | `ddf20fb` (break) | red as intended. Build ×3 at the build step (`-Werror` / C2220). `sanitize`: the ASan case and the UBSan case failed, plus finding 9 |
| [37522181627](https://github.com/Giacogiak/DualC/actions/runs/37522181627) | `0ce9103` (break) | the configure red everywhere, the message unreadable. That led to `b1cdfc5` |
| [37522976374](https://github.com/Giacogiak/DualC/actions/runs/37522976374) | `0647e85` (break) | **layering assertion**: every build-tier job red with `libdualc's LINK_LIBRARIES names 'glfw'` |
| [37526224365](https://github.com/Giacogiak/DualC/actions/runs/37526224365) | `7eb0f52` (break) | **dialect guard**: every job red with `CMAKE_CXX_STANDARD is set as a normal variable…` |

Local measurements:

- **Sanitized build.** `RelWithDebInfo`, `DUALC_SANITIZE=address,undefined`,
  `DUALC_WERROR=ON`: ctest 270/270 in 769 s, serial.
- **Dialect leak.** Read from `compile_commands.json`. Before, geometry-central and Catch2
  TUs had `-std=c++17`. After, geometry-central has no `-std` and GLFW keeps `-std=c99`;
  Catch2 has `-std=c++17` again, by name.
- **The full `python3 scripts/check.py`** passed after each code commit: 31 checks, ctest
  270, 0 DualC-origin warnings over 250 TUs on the clean rebuild. `--docs --strict` passed.

## Exit criteria, one by one

1. **`DUALC_WERROR` and `DUALC_SANITIZE` exist, OFF by default, documented in `README.md`
   and `AGENTS.md`, and the CI workflow runs them; the layering assertion and the dialect fix
   are in `CMakeLists.txt`; each was proven to fail and the proving run is named in the
   record.**
   - The option and the cache string are in `CMakeLists.txt` and
     `cmake/DualCWarnings.cmake`. Both are documented, as the bullets above say.
   - `gate.yml` runs them: the build matrix and the `sanitize` job.
   - The proving runs are 37520360014 (`-Werror` and the sanitizers), 37522976374 (layering)
     and 37526224365 (dialect), named in 17/14 § Proven to fail.
2. **`python3 scripts/check.py` green locally; the branch's latest run green on its required
   jobs.**
   - Local: green, see above.
   - Remote: the final HEAD is pushed with this handoff, and the driver verifies it with
     `plan/remote_run_check.py`. The HEAD before the docs commit, `7eb0f52`'s revert, carries
     the same code as `893ba53` plus `29baf7e`, `3985b88` and `b1cdfc5`.
3. **17/04 #33, its record page, the decisions row and the roadmap README snapshot agree.**
   All say DONE 2026-10-06 and link 17/14. `status-sync`, `status-vocab` and
   `decisions-index` are green.
4. **The handoff lists every warning or sanitizer finding.** See the section above, items
   1–9.

## What I got wrong on the way

- **The first sanitized build failed** (finding 5). Instrumenting only DualC's own targets
  was not enough when an Eigen type crosses into geometry-central.
- **Run 4's break went red without a readable cause.** I had not checked that
  `check_configure`'s ten annotation lines would carry the CMake error. The fix is
  `b1cdfc5`, and the break was re-run as run 5. History therefore shows the layering break
  twice, with a revert between.
- **Pushing the break commit together with the `sanitize` flip** meant finding 9 surfaced on
  a run that was meant only to go red. It is listed, not hidden.
- **`pkill -f` on a pattern that matched my own shell** killed the local load loop twice. No
  effect on the repo.
- **The session was interrupted twice** while it waited on long builds, and was resumed
  from the tree each time.

## Open, and what the next unit needs to know

- **For Giacomo:**
  1. Merge `ci/build-hardening` into `main`. It carries P01 and P02 too, because `main` was
     never pushed.
  2. Add **`sanitize`** to `main`'s required status checks in the GitHub UI, next to the
     others.
- **For P04 (#32):**
  - Coverage was explicitly left to #32 (17/04, the decisions row). A `gcov` / `llvm-cov` job
    can copy the `sanitize` job's shape: `check.py --build -D …`.
  - `check.py -D` is how to add flags without a workflow-only step, and D-49 stands.
  - The `sanitize` job's ctest is about 250 to 430 s. Timing-dependent tests are exposed
    there: see finding 9, and keep new progress or cancel tests off the timed poll counts.
- **Untracked, recorded in 17/14:**
  - the Linux C-ABI link failure (finding 7);
  - the macOS `parallelForPolled` flake (finding 8);
  - `clang-tidy`, not taken (no check set; a first run is its own batch).
- **Still open, from earlier units:** `dualc_gen_demo all --dir <missing>` aborts. In
  `index-staleness`, 13 files are INFO only, and `--strict` passes.
