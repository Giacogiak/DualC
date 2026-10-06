# Public delivery — #49 Linux as a build host

Part of [public delivery](README.md) (block 20); the page became a folder on 2026-10-06 and the section is verbatim.

## #49 Linux as a build host

**DONE — 2026-10-03.** The repo moved from the Windows machine to Linux (Ubuntu 26.04, GCC 15.2,
CMake 4.4.3, Ninja 1.13, Python 3.14). Until then every build, ctest and gate number in this
roadmap was a Visual Studio 2022 number; the GCC branches of the CMake files had never been
configured. Bringing the full gate up there found three defects, none of them in the engine.

**What broke, and why.**
- **C was never enabled.** `project(dualc … LANGUAGES CXX)`, yet `examples/` and `tests/`
  compile the vendored `miniz.c`. The Visual Studio generator compiles a `.c` file regardless;
  Ninja and Makefiles stop at generate time with `CMAKE_C_COMPILE_OBJECT` unset. Fix: the top
  `CMakeLists.txt` calls `enable_language(C)` when examples or tests are built, at top level
  so both sibling directories see it. The library alone still needs no C compiler.
- **The gate was Windows-shaped.** `check.py` hard-coded `-G "Visual Studio 17 2022" -A x64`,
  counted compiled TUs by MSBuild's echo of the bare file name, matched build errors as MSVC's
  `: error C…`, and looked for `examples/Release/dualc_glsl_parity.exe`. Now `generator_args()`
  keeps Visual Studio on Windows and uses Ninja elsewhere (Unix Makefiles when Ninja is
  missing) with `CMAKE_BUILD_TYPE`. TUs are also counted from `Building CXX/C object` progress
  lines, `: error:` is matched too, `--parallel` is passed off Windows, and the parity harness
  is looked up in either output layout.
- **CTest 4 changed its summary.** It prints `100% tests passed out of 270` and drops the
  `, 0 tests failed` clause when nothing failed, so the `ctest` check failed to parse a fully
  green run. The clause is now optional in the pattern.

**A dead helper GCC caught.** `cornerPos()` in `src/contourer.cpp` (anonymous namespace) had
no caller; `sampler.cpp` carries its own `cornerPosition()`. MSVC `/W4` does not report an
unused internal function; GCC's `-Wall` (`-Wunused-function`) does, and it was the only
DualC-origin warning of the Linux build. Deleted. The other 81 warnings of a clean build are
all in the fetched geometry-central (`-Wunused-parameter` 45, `-Wunused-but-set-variable` 25,
`-Wunused-function` 10, `-Wreturn-type` 1) and stay filtered by the `/_deps/` rule. CMake 4
also warns that geometry-central's `cmake_minimum_required` is below 3.10. That is only a
deprecation warning, and the configure succeeds.

**Rejected.** Installing CMake into a user-local Python venv to avoid `sudo`: it works, but the
distribution packages (`cmake ninja-build python-is-python3`) are the setup the docs can name.
`python-is-python3` keeps every `python scripts/check.py` in the docs valid on Linux.

**Verification.**
- First Linux run of the build tier, after the C fix: clean configure plus build in 10 min 43 s
  wall (43 min CPU, 8 cores); `ctest` **270/270** in 103 s, serial. The gate still reported
  FAIL, from the parse defect and the one warning above.
- After both fixes, the full gate from an empty `build/`: **PASS, 31/31**. Configure took 233 s
  with the fetches, the build 274 s; `warnings` read 254 TUs with 0 DualC-origin; `ctest`
  passed **270/270** in 97 s, `check_selftest` included.

Pages moved with it: `README.md` § Build, `AGENTS.md` (the Ninja line first, the
single-config output folder), `docs/command_reference/README.md` (where CLIs land), and a
dated pointer in [17/09/01](../17-code-audit-and-hardening/09-local-checks-gate/01-the-docs-checks.md).
Out of scope: the GL targets (`../polyscope` is not on this machine yet), and the historical
`D:\…` paths and `.exe` names quoted in dated roadmap text.

---

← Back to [20 — public delivery](README.md) · the [Roadmap index](../README.md).
