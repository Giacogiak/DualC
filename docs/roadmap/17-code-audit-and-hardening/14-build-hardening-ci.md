# #33 — build hardening as CI jobs

The delivery record of the rest of [04 #33](04-engineering-quality.md#33-build--tooling-hardening),
Phase 3 of the [hosted-CI plan](../../raw/2026-10-05-hosted-ci-plan.md), on branch
`ci/build-hardening` (2026-10-06). The earlier batches are in
[12 § #33](12-engineering-quality-records.md#33--build-hardening-three-batches). Each
claim below was checked against HEAD (`e1e1f31`) before it was scoped.

## What changed

- **One helper sets the compile options per target.** `cmake/DualCWarnings.cmake`, a
  no-op stub before, now holds `dualc_target_options(<target> [VENDORED])`. Every DualC
  target calls it: the library, `dualc_tests`, every example library and CLI, the GL
  targets and the C ABI. It sets three things:
  - the dialect: `CXX_STANDARD 17`, required, no extensions;
  - the warning level: `/W4 /permissive-` or `-Wall -Wextra -Wpedantic`. Before, only
    `dualc` had it, so tests and examples built at the compiler default. The
    `examples/CMakeLists.txt` comment that said `field_graph.cpp` "builds /W4 /permissive-"
    is now true. `VENDORED` targets (miniz, stb, meshoptimizer, glad) get the dialect only;
  - `DUALC_SANITIZE`, below.
- **`DUALC_WERROR`** (option, `OFF`) adds `/WX` or `-Werror` to the same targets. The local
  gate keeps its log filter (the `warnings` check), because a hard local gate would break on
  a compiler bump. CI's three `build` jobs pass `-D DUALC_WERROR=ON` on top of the scan.
  `scripts/check.py` gained `-D KEY=VALUE` (repeatable) for this. An already-configured
  build directory is re-configured in place with the define, not skipped.
- **`DUALC_SANITIZE`** (cache string, empty) puts `-fsanitize=<list>
  -fno-sanitize-recover=all -fno-omit-frame-pointer` on every DualC target at compile and
  link. Without `-fno-sanitize-recover`, UBSan prints and the test still passes. It is
  GCC/Clang only: with MSVC the configure stops with a message. The new CI job `sanitize`
  runs `check.py --build --config RelWithDebInfo -D DUALC_SANITIZE=address,undefined` on
  Ubuntu 24.04, so every `ctest` case runs under ASan and UBSan. It was born allowed to fail
  ([D-49](../../decisions/01-settled.md)) and has been required since its first two runs came
  back green (`29baf7e`).
- **The layering assertion.** At the end of the root `CMakeLists.txt`, after every
  `add_subdirectory`, `dualc`'s `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES` are read.
  `$<LINK_ONLY:…>` is unwrapped, and anything other than `geometry-central` or
  `Threads::Threads` is a `FATAL_ERROR`. The rule `THIRD_PARTY.md` states in prose is now
  checked, and that page says so.
- **The dialect leak.** `set(CMAKE_CXX_STANDARD 17)` and its two siblings ran before the
  geometry-central and Catch2 fetches. They are gone, and the dialect is a target property
  now. Measured from `compile_commands.json` on GCC 15: before, the geometry-central and
  Catch2 TUs compiled with `-std=c++17`. After, geometry-central gets no `-std` flag (its own
  `cxx_std_11` is met by GCC 15's default) and GLFW keeps its own `-std=c99`. Catch2 is the
  exception, set by name in `tests/CMakeLists.txt`: it detects C++17 features per TU, so the
  library and the tests should agree, and the suite was proven on a C++17 Catch2. A configure
  guard rejects a `CMAKE_CXX_STANDARD` set as a normal variable at top level. A
  `-DCMAKE_CXX_STANDARD` on the command line is a cache entry and passes.
- **Third-party headers are SYSTEM.** geometry-central's headers, and happly's, raised
  16 warnings in *every* DualC TU that includes them on GCC 15 (`-Wunused-parameter`,
  `-Wunused-but-set-variable`, `-Wunused-function` in `halfedge_logic_templates.ipp`,
  `dependent_quantity.ipp`, and others). The warnings scan only hid them by path. When DualC
  adds the tree itself, those include directories are re-declared as
  `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES`, like the vendored nanort. Eigen was SYSTEM
  already. A target owned by an enclosing project is left alone.

## Findings, and what was done with each

| # | Where | Finding | Done |
| --- | --- | --- | --- |
| 1 | `examples/dualc_raymarch.cpp` | `comp()` defined, never used (`-Wunused-function`), found by the wider level locally | deleted |
| 2 | geometry-central / happly headers | 16 warnings per including TU (above) | SYSTEM includes; not ours to fix |
| 3 | `examples/example_common.cpp` ×7 | MSVC C4245: `MZ_DEFAULT_COMPRESSION` (`-1`) passed as `mz_uint` (CI run 1, `windows-2022`) | `MZ_DEFAULT_LEVEL`, the value miniz maps `-1` to (`miniz.c:6221`), so the output is unchanged |
| 4 | `examples/dualc_slice.cpp:284` | GCC 13 `-Wmaybe-uninitialized` on `px`/`py` at `-O3` (CI run 1, `ubuntu-24.04`; GCC 15 is silent). A false positive: `count == 2` fills both slots | zero-initialised, with a comment |
| 5 | Eigen, under ASan | `heap-buffer-overflow` in `Eigen::internal::handmade_aligned_free` on the first sanitized run (`dualc_gen_demo`). Eigen chooses its aligned allocator by `__SANITIZE_ADDRESS__` (`Eigen/src/Core/util/Memory.h:35`), so a matrix allocated in an uninstrumented geometry-central TU and freed in an instrumented DualC TU is freed by the wrong allocator | geometry-central is instrumented with DualC (`dualc_target_sanitize`), the one third-party target that is. Not a DualC defect: it is the build mixing the two |
| 6 | the engine | **no sanitizer finding**: 270/270 `ctest` cases pass under ASan + UBSan (LeakSanitizer included), locally (GCC 15, 769 s serial) and in CI | — |
| 7 | `capi/` on Linux | `-DDUALC_BUILD_C_ABI=ON` fails to link `libdualc_capi.so`: the static libs it links are not built `-fPIC` (`R_X86_64_TPOFF32 against __tls_guard`). Found while measuring warnings with every target on; unrelated to this item, and the C ABI is still Windows-only in CI | not fixed: recorded, untracked |
| 8 | `build (macos-14)`, run 1 | `parallelForPolled does the same work and polls on the caller only` failed once (0.01 s); the log needs a login, so which assertion is unknown. It passed in every later run with no change touching it, and did not reproduce on Linux (150 runs under ASan on 2 loaded cores, 400 in Release) | not fixed: recorded as an open flake, untracked. The suspect is the throwing-body section's `ran < 100000`, which three fast workers can reach before the first throw on a slow-unwinding runtime |
| 9 | `tests/test_cancel_progress.cpp`, run 3 (`sanitize`) | `a request during the collapse pass unwinds from it` failed: it counted the `Sample` reports of a probe run and cancelled at that count in a second run, but the threaded sampler polls on a timer, so under load the second run reported fewer and never cancelled (`expected Cancelled`). Reproduced locally (2 cores, three busy loops) | the test samples on one thread (`samplerAt(6, 1)`), whose report count is fixed; 30/30 under the same load. A test defect, not an engine one |

Two changes followed from the runs. Catch2 is given C++17 by name (item above). And
`check_configure` now puts CMake's `CMake Error` blocks first in its report: stderr comes
after stdout, and the fetched projects' deprecation warnings pushed the error past the ten
annotations CI keeps, so run 4 went red on every job with the message unreadable.

## Proven to fail

Each check was broken on the branch, seen red, and reverted. All runs are on
`ci/build-hardening`, GitHub-hosted runners:

| Run | Commit | What | Result |
| --- | --- | --- | --- |
| [1](https://github.com/Giacogiak/DualC/actions/runs/37516198297) | `8e7ec2f` | the options, first push | the three `build` jobs red on findings 3, 4 and 8. `sanitize` green on its first run (ctest 270, 430 s; job 14 min 54 s) |
| [2](https://github.com/Giacogiak/DualC/actions/runs/37518512354) | `893ba53` | findings 3 and 4 fixed | **every job green**, `windows-2022` under `/WX` (ctest 270; job 11 min 14 s). `sanitize` green a second time (ctest 249 s on a warm ccache; job 4 min 56 s) |
| [3](https://github.com/Giacogiak/DualC/actions/runs/37520360014) | `ddf20fb` | break: an unused variable, a heap read one past the end, a signed `INT_MAX + 1`, in `tests/test_main.cpp` | the three `build` jobs red at the build step: GCC `-Werror=unused-variable`, MSVC C4189 → C2220, Clang `-Werror,-Wunused-variable`. `sanitize` red: case 3 (ASan `heap-buffer-overflow`) and case 4 (UBSan `signed integer overflow`), plus finding 9. `gpu` and `docs` green. In this run `sanitize` was already required (`29baf7e`) |
| [4](https://github.com/Giacogiak/DualC/actions/runs/37522181627) | `0ce9103` | break: `glfw` linked into `dualc` | the configure failed on every job, but the message was unreadable (see above) |
| [5](https://github.com/Giacogiak/DualC/actions/runs/37522976374) | `0647e85` | the same break, after `b1cdfc5` | the configure failed on all four build-tier jobs with `libdualc's LINK_LIBRARIES names 'glfw'`; `gpu` red at its configure step |
| [6](https://github.com/Giacogiak/DualC/actions/runs/37526224365) | `7eb0f52` | break: `set(CMAKE_CXX_STANDARD 17)` back before the fetches | the configure failed on every job with `CMAKE_CXX_STANDARD is set as a normal variable before the third-party fetches` |

Every break is reverted. Run 3's `-Werror` and sanitizer cases were also seen locally
first, and so were the layering and dialect configure failures (GCC 15, CMake 4).

## Not taken

- **`clang-tidy`.** No check set exists, and a first run is a findings batch of its own,
  so it is not trivially cheap. `compile_commands.json` is exported for an ad-hoc run.
- **Coverage.** It belongs to #32: the plan's Phase 4 (`gcov` / `llvm-cov`) hosts its ledger
  counts there.

*2026-10-07, harvested from the P03.1 handoff before `plan/` was retired
([20/04](../20-public-delivery/04-hosted-ci-plan-run.md)):* the plan named
`target_compile_features(dualc PUBLIC cxx_std_17)` as the dialect fix, but that line was
already in `CMakeLists.txt`, and it cannot replace the globals alone. With no
`CXX_STANDARD`, GCC compiles at its default `gnu++17`, which has extensions on, so
`CXX_STANDARD 17` / `CXX_EXTENSIONS OFF` are set as target properties and the configure guard
keeps a global from coming back.

*2026-10-07, the untracked findings given homes ([20/04](../20-public-delivery/04-hosted-ci-plan-run.md)):*
finding 7, the Linux C ABI link failure, is tracked as
[#53](../14-c-abi/06-linux-shared-library.md); finding 8, the macOS flake, is
[D-52](../../decisions/README.md), DEFERRED until it fails a second time in CI. With a log
readable only after login, one failure in 0.01 s gives nothing to debug, and 550 Linux runs
did not reproduce it.

*2026-10-27, D-52's trigger met:* the second failure. The `push` run on `main` at `a345c1a`
(the hygiene merge, 2026-10-22, [run 39837665632](https://github.com/Giacogiak/DualC/actions/runs/39837665632))
failed in `sanitize` alone (Ubuntu, ASan + UBSan): `parallelForPolled does the same work
and polls on the caller only`, 1 of 369. Every other job was green. It passed in the
`sanitize` jobs of the next runs (`13ee7bf`, `4f02b51`), with no change touching it. The log
needs a login, so the failing assertion is still unknown: reading it is the first step of
the chase.

*2026-10-28, D-52 closed — a test defect, not an engine one.* The log was never needed: the
failure reproduces locally. The Release `dualc_tests` pinned to two cores (`taskset -c 0,1`),
running only the section *a throwing body propagates and joins, on the threaded path*, failed
**23 of 400** runs, every one on `REQUIRE(ran.load() < 100000u)` with `100000 < 100000` — the
row's suspect. Four workers, the body throws at `i == 10`; the throwing worker is descheduled
between its claim and the store to `failed` (an unwind, slower still under ASan + UBSan), and
the three free workers drain the other 99,990 indices in a few milliseconds, so every index
runs exactly once and `ran == n`. The engine kept its promise — the exception propagated and
every thread joined; the bound measured the scheduler, and a hosted runner with two to four
cores and a sanitized unwind is where it loses. The sibling `parallelFor` section (1e6
indices, `i == 0`) has the same shape with ten times the margin: 0/400 under the same load,
and never failed in CI.

*Fix, `tests/test_parallel.cpp`, both early-out sections.* The polled one is deterministic
now: index 0 throws, every other index parks its worker until the poll has run — with a 10 s
interval the poll runs only once the caller has seen `failed` — and a released worker
re-checks the flag before claiming, so `ran <= threads` holds by construction, and
`released` asserts the failure→poll wake on the way. The `parallelFor` section has no poll to
release on, so it keeps the bound and changes what the bound measures: each non-throwing
index parks 1 ms, `n = 1e5`, and a flake would need the thrower stalled for ~30 s; a broken
early-out fails after ~30 s instead of hanging. The 10 s park deadline on the polled side
only keeps a broken wake from hanging the case.

*Verified.* Release, two cores: **0/400 and 0/400** (9 s for the 800 runs; the two sections
take 3 ms and 1 ms). **Proven to fail:** a standalone harness with the two sections against a
copy of `parallel.h` whose `if (failed) break;` is removed — `parallelFor` `ran == 100000`
after 38 s, `parallelForPolled` `ran == 100000` in 8 ms, both red; against the real header
`ran == 4` and `ran == 1`, green. Under ASan + UBSan (`build-san`, RelWithDebInfo, the CI `sanitize` recipe), two cores: the
**old** section **4/200**, all `100000 < 100000` — CI's failure, reproduced in the CI
configuration; the **fixed** sections **0/200 and 0/200**, and `[parallel]` 7/7 cases
(579,099 assertions). The full gate on the Release tree: `ctest` 408/408, 0 warnings. Finding
8 joins finding 9: both were timing bounds in tests, neither an engine defect. Not changed:
the serial `ctest` gate ([15 § The serial gate](15-test-coverage-batches.md#the-serial-gate-is-kept-for-a-new-reason))
— its cost reason stands, and these two sections no longer depend on it.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
