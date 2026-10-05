# DualC hosted-CI plan — #51 and the three items it unblocks

**Drafted 2026-10-05**, the session after the repo was published (2026-10-03,
[20 § Update — published](../roadmap/20-public-delivery.md)). An input, not a record: status
lives in the roadmap blocks each phase names, never here. Each phase is **one fresh session**,
opened and closed with `/repo-docs-lifecycle`; a phase that does not fit a session is closed
partial, with what was verified and what remains written into its record page.

## 0. Why now, and what the record already says

The local checks gate's record,
[17/09](../roadmap/17-code-audit-and-hardening/09-local-checks-gate/README.md), split CI in
two on 2026-09-10: **(a)** one reproducible gate command — delivered as `scripts/check.py` —
and **(b)** *"unattended triggering by something that is not you"*, declared "genuinely absent
locally" and left open because the repo had no remote; self-hosted Jenkins, a bare mirror with a
`post-receive` hook and a Task Scheduler job were rejected as ceremony for a one-developer
local repo. The premise ended on 2026-10-03: the remote is
<https://github.com/Giacogiak/DualC>. Half (b) is now a GitHub Actions workflow whose job is
*the gate command itself*, nothing else — the gate stays the one definition of "green".

Three tracked items in [17](../roadmap/17-code-audit-and-hardening/README.md) name the absent
CI as blocker or trigger:

| Item | Status 2026-10-05 | What names CI |
| --- | --- | --- |
| [#31](../roadmap/17-code-audit-and-hardening/04-engineering-quality.md#31-make-the-cpugpu-parity-gate-binding) | DEFERRED | trigger: *"headless-GL CI becoming available"*; the harness opens its window hidden (`examples/raymarch_gl.cpp:29`) |
| [#33](../roadmap/17-code-audit-and-hardening/04-engineering-quality.md#33-build--tooling-hardening) | PARTIAL, rest DEFERRED | original trigger was *"CI being set up"*; the rest (`-Werror`, sanitizers, layering assertion, dialect leak) is cheap as CI jobs, dear locally |
| [#32](../roadmap/17-code-audit-and-hardening/04-engineering-quality.md#32-test-coverage-ledger) | PLANNED | finding C43: a fixed temp filename *"two concurrent CI jobs would collide on"* |

Five places state "there is no CI" and are rewritten by Phase 1: `README.md:62`,
`AGENTS.md:29`, `STRUCTURE.md:162`, the gate's own argparse description
(`scripts/check.py:1712`), and the 17/09 README's opening. The decisions row for
[#8](../decisions/README.md) lists *"CI artifact"* as a trigger for the install/packaging
work; a release job would fire it and is **out of this plan's scope** (§ 6).

## 1. Repo facts the plan relies on

All verified on the drafting day, 2026-10-05, against `main` at `6da2e6c`.

- **The gate.** `python scripts/check.py` with no flags runs the docs tier plus configure,
  build, warnings scan and serial `ctest`; `--docs --strict` is the docs tier with report-only
  lists made fatal; `--gpu` runs `dualc_glsl_parity` from the repo root and asserts
  `parity_expected_cases` (73) — it **SKIPs** when the binary is not built; `--build-dir`
  (default `build`), `--config` (default `Release`), `--clean` re-configures. Exit code is
  non-zero on any failure. The build tier picks its generator per OS
  ([20 #49](../roadmap/20-public-delivery.md#49-linux-as-a-build-host)): Visual Studio 17 2022
  / x64 on Windows, Ninja when on `PATH` elsewhere, else CMake's default, with
  `CMAKE_BUILD_TYPE`.
- **`ctest` is serial, never `-j`** — the gate encodes finding C43 (shared cwd, fixed temp
  filename `dualc_fg_box.obj` at `tests/test_field_graph.cpp:122`). Matrix jobs on separate
  machines do not collide; the claim in #32 is about `-j` within one tree.
- **The warnings scan is a log filter**, deliberately not `/WX`
  ([17/09/02](../roadmap/17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md));
  `/W4 /permissive-` and `-Wall -Wextra -Wpedantic` are `PRIVATE` on `dualc` only
  (`CMakeLists.txt:178-180`); tests and examples build at compiler default, and
  `examples/CMakeLists.txt:84` claims otherwise. A meaningful scan needs a **clean** build —
  CI gives one every run, which is the one thing the local gate cannot.
- **Dependencies at configure.** geometry-central (pinned commit), Eigen and Catch2 are
  fetched at first configure; GLFW only when a GL target is ON (`-DDUALC_GLFW_DIR` escape
  hatch). On Ubuntu the GL targets need `libx11-dev libxrandr-dev libxinerama-dev
  libxcursor-dev libxi-dev` ([07 #50](../roadmap/07-viewer-polyscope.md)); the core library,
  tests and CLIs need only `cmake ninja-build g++`.
- **Dialect leak still true**: `set(CMAKE_CXX_STANDARD 17)` at `CMakeLists.txt:41` precedes
  `FetchContent_MakeAvailable(geometry-central)` at `:82`; `CMAKE_EXPORT_COMPILE_COMMANDS` is
  already ON at top level (`:48`). `cmake_minimum_required` is 3.14.
- **Numbers.** The next free tracked item is **#51** (last: #50). Block 20
  (`docs/roadmap/20-public-delivery.md`) is **131 bytes under the 15,360-byte cap**: its next
  entry means the same-numbered folder split, headings and `#47-…` / `#49-…` anchors kept
  verbatim in the children. Decisions rows use `D-NN`; the last `D-NN` is read from
  `docs/decisions/README.md` at phase time, not from this plan.
- **Verification without `gh`.** The GitHub CLI is not installed on the build host. A public
  repo's runs are readable unauthenticated:
  `https://api.github.com/repos/Giacogiak/DualC/actions/runs?branch=<name>` (60 requests/hour)
  and `…/actions/runs/<id>/jobs`; a job's log needs authentication, so a failing job's
  diagnosis is read in the browser by the owner or reproduced locally. Branch protection
  (required checks) is an outward-facing UI step the owner takes.
- **`#32` has drifted**: unguarded `== Approx(0.0)` are **38** (the entry says 36);
  `REQUIRE_THROWS_AS` is **17 across five files** (`test_parallel` 6, `test_cancel_progress` 4,
  `test_field_graph` 4, `test_field_glsl` 2, `test_primitives` 1), not "exactly one outside the
  field-graph parser". Corrected in Phase 4, not before.

## 2. Phase 1 — #51 hosted CI: the gate as the job

**Goal.** Every push to `main` and every pull request runs the gate unattended on Linux,
Windows and macOS, and the result is visible on the commit. No new check is invented: a red
job is a red gate.

**Steps.**

1. `.github/workflows/gate.yml`, triggers `push` (main), `pull_request`, `workflow_dispatch`;
   `concurrency` keyed by ref with cancel-in-progress; `timeout-minutes` per job.
2. **Job `docs`** — `ubuntu-latest`, Python 3: `python3 scripts/check.py --docs --strict`.
   Seconds; it is the hook's tier and fails fast.
3. **Job `build`**, matrix:
   - `ubuntu-24.04` — `apt-get install ninja-build`; GCC 13 (the owner builds with GCC 15, so
     this is a second compiler, not a copy).
   - `windows-2022` — Visual Studio 17 2022, `python` on `PATH`; the generator the gate was born on.
   - `macos-14` — `brew install ninja`, AppleClang. **Never built before**:
     `continue-on-error: true` on the first run; kept as a required job if green, otherwise
     fixed in-phase when the cause is ours, or dropped with the log quoted in the record.
   Each runs `python scripts/check.py --build-dir build` — the full default gate, serial
   `ctest` included. Caching: the FetchContent sources (`build/_deps/*-src`, keyed on
   `CMakeLists.txt` hashes) and a compiler cache (`ccache` on Ubuntu/macOS through the
   `CMAKE_CXX_COMPILER_LAUNCHER` environment variable; sccache on Windows is optional and may be
   skipped). **Object files are not cached**: the warnings scan is only meaningful on a clean build.
4. **Job `gpu`** — `ubuntu-24.04`, the X11 dev packages, `xvfb`, Mesa (`libgl1-mesa-dri`,
   `LIBGL_ALWAYS_SOFTWARE=1`); configure with `-DDUALC_BUILD_GLSL_PARITY=ON`, build the
   harness, run `xvfb-run -a python3 scripts/check.py --gpu --build-dir build`.
   `continue-on-error: true` in this phase — it is **Phase 2's experiment**, started here so
   its first result is on file; its pass/fail does not gate #51.
5. Push on a branch (`ci/gate-workflow`), read the runs through the public API, iterate until
   `docs` and the Linux and Windows `build` jobs are green, then merge to `main` with the
   docs of step 6 in the same branch. Record the first green run's URL, each job's duration,
   and the macOS / gpu first results in the record page.
6. **Docs at close** (route by the table in `docs/README.md`):
   - block 20 → folder `20-public-delivery/README.md` + children (anchors verbatim, inbound
     links retargeted; `stale-paths` and `links` prove it), plus a child for **#51** with the
     workflow's shape, the matrix choice and what each job does and does not assert;
   - 17/09 README: half (b) now exists, pointing at 20's #51 child; 17/09/03
     *What this does and does not move* gets a dated note for #31/#32/#33;
   - the five "there is no CI" statements rewritten (the gate is what CI runs; locally it is
     still the one command); README gets the workflow badge if wanted;
   - `STRUCTURE.md`: a row for `.github/workflows/gate.yml`;
   - decisions: #31 row — its trigger's state after the first gpu run; #33 row — trigger
     reworded (CI exists; the rest is Phase 3); a `D-NN` row for the policy *CI runs the gate
     and nothing else; allowed-to-fail jobs are experiments, never a definition of green*;
   - `roadmap/README.md`: § Current focus (NEXT = Phase 2 of this plan), the five-row
     milestones window (oldest row out), block 20's row in § Principal blocks;
   - `docs/design/`: nothing, unless a conventions page states where the gate runs.
   Verification shown: the green run URL; `python3 scripts/check.py --docs --strict` green.

**Out of scope.** Branch protection (owner, UI); a release/artifact job (§ 6); any new check
in `check.py`; making `gpu` or `macos` required (Phase 2 / the first-run outcome).

## 3. Phase 2 — #31: the parity gate binding through headless GL

**Blocker check first.** Phase 1's `gpu` job result. Mesa's llvmpipe exposes OpenGL 4.5 core
and `R32F` is colour-renderable since GL 3.0, so the harness's FBO should work; if the first
run failed on something ours (a window-system assumption, `cwd`, missing package), fix it
here. If llvmpipe genuinely cannot run the harness, #31 stays DEFERRED with its trigger
reworded to the remaining candidate — a CPU reference evaluator for the emitted AST — and the
phase closes on that record.

**Steps when green.**

1. Make the `gpu` job required (`continue-on-error` removed); confirm the gate's parity check
   asserts **73/73** there (`check.py --gpu` fails on a short count by design,
   [17/09/02](../roadmap/17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md)).
2. Decide the CTest half: either `add_test(dualc_glsl_parity …)` guarded by
   `DUALC_BUILD_GLSL_PARITY` so a local `ctest` with the target ON runs it too, or leave the
   gate's `--gpu` tier as the single runner. One sentence of rationale either way.
3. Optional, same job: headless `--snapshot` smoke of `dualc_raymarch` and `dualc_field_view`
   (exit code and a PNG header), since the window is already hidden and the X11 stack is paid for.
4. Prove it fails: break one shared formula on a branch, see the job go red, revert.

**Docs at close.** 17/04 #31 body line → **DONE** (date, run URL); the 17/README ledger row
and *Latest verification* (the one home of the counts); decisions #31 row; 17/09/03 note;
[12/02](../roadmap/12-field-graph-and-app/02-glsl-codegen.md) if it states where parity runs;
`docs/design/` page that owns the parity convention, if one does; roadmap README snapshot.
`command_reference/` only if a flag changed (none planned).

## 4. Phase 3 — #33: the rest of build & tooling hardening

**Blockers.** None: its trigger ("the first external consumer that is not this repo") is
superseded — CI makes each sub-item near-free to run. Verify each claim against HEAD before
scoping, as #34 did.

**Sub-items, each proven to fail before it ships** (the repo's rule for every check):

1. **`-Werror` / `/WX` as an option** — `DUALC_WERROR`, OFF by default (the local gate keeps
   its log filter, a recorded decision), ON in CI. Widening `/W4` / `-Wall -Wextra -Wpedantic`
   to tests and examples is part of this, as is correcting `examples/CMakeLists.txt:84`'s
   claim. Expect a first batch of warnings in tests/examples; fix or justify each.
2. **Sanitizers** — `DUALC_SANITIZE` (`address,undefined`; GCC/Clang only), one Linux CI job
   in `RelWithDebInfo` running `ctest`. The audit calls this high-value given the raw-pointer
   octree, hand-written BVH and vendored float SVD. A real finding is a defect record (a dated
   entry, possibly a new `#NN`), not a silenced report.
3. **Layering assertion** — after every `add_subdirectory`, read `dualc`'s
   `LINK_LIBRARIES` / `INTERFACE_LINK_LIBRARIES` and `message(FATAL_ERROR)` on anything beyond
   geometry-central and threads (the rule `THIRD_PARTY.md` states in prose). Prove it by
   linking `glfw` into `dualc` on a branch.
4. **Dialect-flag leak** — replace the global `CMAKE_CXX_STANDARD` block with
   `target_compile_features(dualc PUBLIC cxx_std_17)` (or scope the globals after the fetches);
   confirm the warnings scan and `ctest` unchanged on all three runners.
5. **`clang-tidy` and coverage** — DEFERRED unless trivially cheap; a `gcov`/`llvm-cov` job is
   a natural host for Phase 4's ledger counts and may be taken there instead. Named either way.

**Docs at close.** 17/04 #33 body line (DONE, or PARTIAL naming the rest); the delivery record
in [17/12](../roadmap/17-code-audit-and-hardening/12-engineering-quality-records.md) — if it
trips the cap, a new same-numbered folder or a new `17/13` page, indexed in 17/README;
decisions #33 row; `README.md` build section and `AGENTS.md`'s opt-in list for the new CMake
options (defaults as they are); `STRUCTURE.md` for any new file; 17/09/03 note; roadmap README.

## 5. Phase 4 — #32: the test-coverage ledger

**Blockers.** None; large, so planned as two batches that may be two sessions.

**Batch A — correct the record, then the mechanical fixes.**

1. Correct the drifted claims in 17/04 #32 (§ 1 above) and finding C43's disposition in
   [17/06](../roadmap/17-code-audit-and-hardening/06-findings-ledger-testing.md): matrix jobs
   do not collide; the collision is `ctest -j` within one tree.
2. `.margin(…)` on the 38 unguarded `Approx(0.0)` comparisons; `check.py --metrics` should
   read 0 afterwards, and a gate check may ratchet it there.
3. A per-test working directory or unique temp names in `test_field_graph.cpp` (and any
   sibling), so `ctest -j` is safe; then revisit the gate's serial-`ctest` decision
   ([17/09/02](../roadmap/17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md))
   — lift it or keep it, one dated line.

**Batch B — the missing tests**, each small and named by the audit:

- `partitionCubeEdges` on all 256 sign configurations (pure, O(1), allocation-free; 9 of 256
  today in `tests/test_cube_components.cpp`), with manifoldness invariants per configuration;
- a refinement-convergence test: error against an analytic surface decreases as `maxDepth` rises;
- the four never-varied `ContourerParams` knobs each exercised at least once against a
  measurable effect;
- `interpolateNormals`' sharp/smooth claim unit-tested; output normals checked for unit length
  and outward orientation against the field gradient;
- CLI smoke tests asserting content (PNG signature and dimensions, STL triangle count, 3MF
  zip validity), not exit code only;
- the strut-lattice oracle that *"certifies less than it claims"*
  ([17/07](../roadmap/17-code-audit-and-hardening/07-repeat-tiling-fix.md)): strengthen the
  sampling or restate the comment to what it checks.

**Docs at close.** 17/04 #32 body line; a record page (`17/13` or the next free number,
indexed in 17/README) with the before/after counts; 17/README *Latest verification* (the
`ctest` count); C42–C54 dispositions in 17/06; `STRUCTURE.md` rows for new test files; the
design page that owns testing conventions, if one does; decisions rows touched; roadmap README.

## 6. After the four phases, and what stays out

- **#34 resumes as NEXT** (the three open bullets: the contourer thread knob, the `gradientAt`
  contract and `GridField::gradientAt`), then #35 on its trigger.
- **Out of scope for all phases**: a release workflow publishing the C ABI library per OS —
  it fires [#8](../decisions/README.md)'s "CI artifact" trigger and would give Boletus its
  re-vendor artifact ([15](../roadmap/15-boletus-handoff.md)); worth its own item when asked
  for. Self-hosted runners. Any change to what "green" means beyond the gate.

## 7. Session protocol for every phase

1. Open: `/repo-docs-lifecycle` — gate, reading order, the phase's section here, the roadmap
   block it advances, the design page of the mechanism; `git log` against the record for drift.
2. Before any code commit: the full gate (`python3 scripts/check.py`), `--gpu` when a field
   node or the codegen moved; commit without stopping for the owner.
3. CI evidence is a run URL plus the durations, written into the record page; never a chat line.
4. Close: `--docs --strict` green; harvest; route by the table in `docs/README.md`; `--strict`
   again; report files touched and what was left open. A phase closed partial says so on its
   item's body line.

**Prompt for each fresh session** (replace N):
`/repo-docs-lifecycle Execute Phase N of docs/raw/2026-10-05-hosted-ci-plan.md`.
