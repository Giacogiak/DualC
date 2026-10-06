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
| `docs` | `ubuntu-latest` | `check.py --docs --strict` — the session-end mode, report-only lists fatal | yes |
| `build (ubuntu-24.04)` | Ubuntu 24.04, GCC from the image, Ninja from apt | `check.py --build-dir build` — the full default gate: docs tier, configure, build, warnings scan, serial `ctest` | yes |
| `build (windows-2022)` | Windows Server 2022, Visual Studio 17 2022 | the same command; the generator the gate was born on | yes |
| `build (macos-14)` | macOS 14 arm64, AppleClang, Ninja from brew | the same command; never built there before | no — `continue-on-error` |
| `gpu` | Ubuntu 24.04, the X11 headers, Xvfb, Mesa (`LIBGL_ALWAYS_SOFTWARE=1`) | configure with `-DDUALC_BUILD_GLSL_PARITY=ON`, build the harness, generate `data/`, `xvfb-run -a check.py --gpu` | no — the plan's Phase 2 experiment (#31) |

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

**Failures are readable without a login.** A job's log needs authentication; a check run's
annotations do not. `.github/workflows/annotate.py` re-prints the gate's `[FAIL]` lines and
their detail lines as error annotations when a gate step fails, so the public API names the
failing check.

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

**Out of scope, named.** Branch protection (the owner, in the GitHub UI); a release or
artifact job (it would fire [#8](../../decisions/README.md)'s "CI artifact" trigger); any new
check in `check.py`; making `gpu` or `macos-14` required.

---

← Back to [20 — public delivery](README.md) · the [Roadmap index](../README.md).
