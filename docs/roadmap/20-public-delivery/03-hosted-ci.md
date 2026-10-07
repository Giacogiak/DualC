# Public delivery — #51 hosted CI

Part of [public delivery](README.md) (block 20). Phase 1 of the
[hosted-CI plan](../../raw/2026-10-05-hosted-ci-plan.md#2-phase-1--51-hosted-ci-the-gate-as-the-job),
run as the plan-run unit P01.1 on branch `ci/gate-workflow`.

## #51 Hosted CI — the gate as a GitHub Actions job

**DONE — 2026-10-06.** The gate's record split CI in two
([17/09](../17-code-audit-and-hardening/09-local-checks-gate/README.md#what-ci-actually-is-split-in-two)):
(a) one reproducible command, delivered as `scripts/check.py` on 2026-09-10, and (b)
unattended triggering, left open because the repo had no remote. The remote exists since
2026-10-03 ([#47's update](01-pin-geometry-central.md)), so (b) is now a workflow whose job is
the gate command itself and nothing else ([D-49](../../decisions/01-settled.md)).

**The workflow's shape.** `.github/workflows/gate.yml`, triggered on `push` to every branch
(a branch with no pull request still runs), on `pull_request` and on `workflow_dispatch`;
`concurrency` keyed by ref with cancel-in-progress; `timeout-minutes` on every job; bash with
`pipefail` for every step, so `check.py | tee gate.log` keeps the gate's exit code.

| Job | Runner | Runs | Required |
| --- | --- | --- | --- |
| `docs` | `ubuntu-latest`; `ubuntu-24.04` since 2026-10-07, pinned like the other jobs before the label moved to Ubuntu 26 | `check.py --docs --strict` — the session-end mode, report-only lists fatal | yes |
| `build (ubuntu-24.04)` | Ubuntu 24.04, GCC from the image, Ninja from apt | `check.py --build-dir build` — the full default gate: docs tier, configure, build, warnings scan, serial `ctest` | yes |
| `build (windows-2022)` | Windows Server 2022, Visual Studio 17 2022 | the same command; the generator the gate was born on | yes |
| `build (macos-14)` | macOS 14 arm64, AppleClang, Ninja from brew | the same command; never built there before #51 | yes — allowed to fail until its first complete run came back green |
| `sanitize` | Ubuntu 24.04, GCC from the image, Ninja from apt | `check.py --build --config RelWithDebInfo -D DUALC_SANITIZE=address,undefined` — ASan + UBSan over every `ctest` case (added 2026-10-06, [17/14](../17-code-audit-and-hardening/14-build-hardening-ci.md)) | yes — allowed to fail until its first two runs came back green (2026-10-06) |
| `gpu` | Ubuntu 24.04, the X11 headers, Xvfb, Mesa (`LIBGL_ALWAYS_SOFTWARE=1`) | configure with `-DDUALC_BUILD_GLSL_PARITY=ON`, build the harness, generate `data/`, `xvfb-run -a check.py --gpu --strict` | no — the plan's Phase 2 experiment (#31); **yes** since 2026-10-06 ([17/13](../17-code-audit-and-hardening/13-parity-gate-binding.md)) |

**The matrix choice.** Ubuntu with the image's GCC is a second compiler next to the owner's
GCC 15, not a copy; Windows keeps the Visual Studio path the gate was written on; macOS adds
AppleClang, the one toolchain never tried. Each build job runs the *whole* default gate, so
the docs tier runs four times per push — about a second each, and it keeps "green" one
command, not a composition of jobs.

**What each job asserts, and what it does not.** A green `build` job asserts what the local
gate asserts: the docs contract (non-strict), a clean configure from a bare clone (the three
fetches), a build with zero DualC-origin warnings, and every `ctest` case passing serially.
Because every CI build is clean, the warnings scan always reads a full log; locally an
incremental build makes it SKIP. It does **not** assert the GL targets (only the `gpu` job
builds one), the C ABI (`DUALC_BUILD_C_ABI` stays off, as in the local gate), `-Werror`, the
sanitizers or a warnings level on tests and examples — #33's rest, the plan's Phase 3.
*2026-10-06: the three `build` jobs pass `-D DUALC_WERROR=ON`, the warnings level covers
tests and examples, and the `sanitize` job runs ASan + UBSan — [17/14](../17-code-audit-and-hardening/14-build-hardening-ci.md).*

**Caching.** `ccache` on Ubuntu and macOS through `CMAKE_C_COMPILER_LAUNCHER` /
`CMAKE_CXX_COMPILER_LAUNCHER`; it replays the compiler's stderr on a hit, so a cached object
still reports its warnings to the scan. Object files themselves are never cached. The
FetchContent cache holds `build/_deps/*-src` **and** `*-subbuild`, plus Eigen's tree
under `geometry-central-build/deps/eigen-src`, keyed on the `CMakeLists.txt` hashes. The plan
named `*-src` alone. That does not work: the git clone step wipes a source directory whose
sub-build stamps are missing and clones again. A local test showed it: with only `*-src`
and `*-subbuild` restored, the configure failed in Eigen's update step, because
geometry-central downloads Eigen into its own build tree. With all three restored, the
configure took 3 s instead of 60 s (GCC 15, CMake 4.4.3, this host).

**Results are readable without a login.** A job's log needs authentication; a check run's
annotations do not. After every gate step, `.github/workflows/annotate.py` re-prints the
gate's report as annotations: an error for each `[FAIL]` line and its detail lines, and a
notice for each build- or gpu-tier result plus the closing count. The notices exist because
a green job proves little by itself: the gate passes on SKIP, and `--gpu` with no harness
binary is one SKIP and a PASS (reproduced locally). For the same reason the `gpu` job runs
`--strict`, which turns a SKIP into a failure. The plan's command had no `--strict`.

**What the first run found.** The docs tier was not clean-clone-true. Two checks passed on
the owner's machine only because of files outside git:
- `links` — a cross-project link to Boletus five levels deep in 17/09/03 resolved because
  `~/Documents/Boletus` exists here; `out_of_repo_links` listed only the two- and three-level
  prefixes. The five-level prefix is added.
- `vendoring` — `THIRD_PARTY.md` cited `` `data/bunny.obj` ``, a gitignored download that exists
  only in the owner's `data/`. The citation now names `bunny.obj` in `data/`.

The `gpu` job's first attempt stopped before the harness: a clean clone has no `data/`, and
`dualc_gen_demo all --dir data` aborts on a missing directory (an uncaught
`std::runtime_error`, exit 134) instead of creating it. The job creates `data/` first. The
CLI's behaviour is left as it is: AGENTS.md's `dualc_gen_demo all --dir data` recipe fails
the same way on a fresh clone.

**Verification.** Run 3, on `f064ea8`:
<https://github.com/Giacogiak/DualC/actions/runs/37446307563>. It is the first run with every
job green, and the first with the notices. Conclusion `success`, 8 min 44 s wall.

| Job | Duration | Gate report (the notices) |
| --- | --- | --- |
| `docs` | 6 s | 27 checks, 26 passed, 0 failed, 0 skipped |
| `build (ubuntu-24.04)` | 1 min 24 s | configure 3.9 s, build 9.4 s, 254 TUs and 0 DualC-origin warnings, `ctest` 270/270 in 49.6 s; 31 checks, 0 failed, 0 skipped |
| `build (windows-2022)` | 8 min 2 s | configure 40.7 s, build 365.9 s, 254 TUs and 0 DualC-origin warnings, `ctest` 270/270 in 57.6 s; 31 checks, 0 failed, 0 skipped |
| `build (macos-14)` | 1 min 42 s | configure 9.6 s, build 9.7 s, 254 TUs and 0 DualC-origin warnings, `ctest` 270/270 in 61.8 s; 31 checks, 0 failed, 0 skipped |
| `gpu` | 2 min 58 s | parity **73/73** in 7.5 s under Xvfb + llvmpipe |

How to read the times. The Ubuntu and macOS builds are ccache-warm: run 2 had filled the
cache. Their cold runs (run 2, `961e92e`, same commands but no notices yet) took 4 min 41 s and
5 min 20 s for the whole job. Windows has no compiler cache, and its FetchContent cache was
still empty in run 3, so its 8 min is a cold build. Each job's duration is wall time, setup
steps included. Every job was read through the public run and job pages, because the API's
unauthenticated limit (60 requests an hour) ran out during the session.

**The macOS and gpu first results.** `macos-14` built and passed the whole gate on its first
complete run (run 2) and again in run 3, whose notice shows zero DualC-origin warnings under
AppleClang.
As the plan says for that outcome, it is now a required job. The `gpu` job's first attempt
(run 1) failed on the missing `data/`. Its first complete run (run 2) passed without
`--strict`, which cannot rule out a SKIP. Run 3 ran under `--strict`, and its notice shows
**73/73** cases, so the harness really ran. This is the trigger of [#31](../../decisions/README.md) and the input of the plan's
Phase 2. The job stays allowed to fail until that phase binds it.
*2026-10-06: bound — the `gpu` job is required ([17/13](../17-code-audit-and-hardening/13-parity-gate-binding.md)).*

**Runs on the branch.** Run 1 (`f4e6319`): `docs` and `gpu` red as described above, `macos-14`
red after 2 min 43 s with the log unread. That job runs the docs tier first, so it was probably
the same two clean-clone failures. The run was cancelled by the next push. Run 2 (`961e92e`):
all green except `windows-2022`, which was cancelled by run 3's push.

**Out of scope, named.** Branch protection (the owner, in the GitHub UI); a release or
artifact job (it would fire [#8](../../decisions/README.md)'s "CI artifact" trigger); any new
check in `check.py`; making `gpu` or `macos-14` required.

*2026-10-07:* run [37634791783](https://github.com/Giacogiak/DualC/actions/runs/37634791783)
(`d62e3ea`, #54) failed on `build (windows-2022)` alone, at configure:
`Build step for eigen failed: 1` in FetchContent, before any DualC code compiled. Every
other job passed, including Ubuntu and macOS, which fetched Eigen cold in the same run. The
commit had edited `examples/CMakeLists.txt`, and the cache key hashed every
`CMakeLists.txt`, so all three OSes missed the cache. The Windows fetch is read as a
transient download failure; its log needs a login. To make cold fetches rare, the
FetchContent cache key (build matrix and `sanitize`) now hashes only the two files that
declare fetches, the root `CMakeLists.txt` and `tests/CMakeLists.txt`. The next run,
[37638160763](https://github.com/Giacogiak/DualC/actions/runs/37638160763) on `515effa`, was
cold on every OS under the new keys and passed every job, ctest 304 with
[10 #54](../10-infrastructure-and-integration.md)'s cases on all three OSes: `docs` 8 s, ubuntu 3 min 33 s,
macOS 1 min 47 s, windows 7 min 17 s, `sanitize` 9 min 40 s, `gpu` 4 min 36 s.

---

← Back to [20 — public delivery](README.md) · the [Roadmap index](../README.md).
