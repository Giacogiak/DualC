# Semantic lint — after the hosted-CI plan

Run on 2026-10-07, after the hosted-CI plan's four phases
([20/04](../../20-public-delivery/04-hosted-ci-plan-run.md)): #51, #31, #33 and #32 moved the
build, the tests, the examples layer and `scripts/check.py`, and rewrote `design/10` and
`design/11` in place. It is a **delta run** over the full [05](05-2026-10-05-after-50.md)
two days earlier. For (a) and (b) it reads every design page that names a file changed since
`6ca5943`, and every design claim the #32 tests now exercise. Classes (c), (d) and (e) are
swept whole. Line numbers are as read at `9c66de9`, before this run's fixes. The gate was
green before the read (`check.py --docs`, 28 checks, 27 OK, 1 INFO).

**Inputs.**

- **Last code change:** `12fb369` (2026-10-07, #32 Batch B).
- **Last design change:** `764f7e3` (2026-10-06, `design/10`); `design/11` changed at
  `301b60c` the same day.
- **`doc-lag`:** 0 commits.
- **Code changed since `6ca5943`:**
  - `CMakeLists.txt`, `cmake/DualCWarnings.cmake`, `capi/CMakeLists.txt`,
    `tests/CMakeLists.txt` and `examples/CMakeLists.txt` (#33's options and #32's content
    checks);
  - `examples/check_cli_output.cmake` (new);
  - three one-line `-Werror` fixes in `examples/` (`dualc_raymarch.cpp`, `dualc_slice.cpp`,
    `example_common.cpp`);
  - the tests (`test_accuracy.cpp` new, `test_main.cpp` filled, eleven files extended or
    given margins);
  - `scripts/check.py`, `.github/workflows/`.
- **No change to `src/` or `include/`.**

**Pages read for (a) and (b).**

- `design/05 § 5` against `CMakeLists.txt:48-123,200-275` and `cmake/DualCWarnings.cmake`.
- `design/06 § 7` against `include/dualc/{sampler,contourer}.h`, `src/contourer.cpp:60-140`,
  `src/internal/mesh_bvh.cpp:50-60,130-145` and the new `tests/test_contourer.cpp` /
  `tests/test_accuracy.cpp` cases.
- `design/04 § 4.5` (the component rules, at most four components) against
  `src/internal/cube_components.cpp` and the 256-configuration sweep in
  `tests/test_cube_components.cpp`.
- `design/08 § 10.4` (domain operators) against `src/implicit/domain_ops.cpp:170-330`.
- `design/09` against `examples/example_common.cpp`.
- `design/10:36-81` against `tests/test_cancel_progress.cpp` and `.github/workflows/gate.yml`.
- `design/11` (the *Gate* row).
- `design/README.md:66-74` against `tests/test_pipeline.cpp`.

## Findings

| # | Class | Finding | Docs `file:line` | Code `file:line` | Disposition |
| --- | --- | --- | --- | --- | --- |
| 1 | (a) | The `interpolateNormals` row says the blend is of "the input's area-weighted per-vertex normals". geometry-central weights the unit face normals by corner angle: run 05's finding 1, fixed in `design/05` but not here. The same wrong word survives in three more places: a member comment in `mesh_bvh.cpp`, `dualc_demo`'s `--help` text, and roadmap 09 #7's dated analysis | `design/06-parameters-and-vendoring.md:19`; `roadmap/09-io-formats.md:12,19` | `src/internal/mesh_bvh.cpp:55,134-139`; `examples/dualc_demo.cpp:116`; `tests/test_accuracy.cpp:156-167` | fixed in this run's commit: the design row, the code comment, the help text, and a dated correction on roadmap 09 #7 |
| 2 | (b) | `§ 5` describes how geometry-central is resolved and linked, but not three build rules #33 added: when DualC adds the tree itself, the geometry-central and happly include directories reach DualC's targets as SYSTEM; under `DUALC_SANITIZE`, geometry-central is instrumented too, because Eigen picks its allocator by `__SANITIZE_ADDRESS__`; and the configure fails if `dualc` links anything but `geometry-central` and `Threads::Threads`. Nor is the per-target C++17 dialect stated anywhere in `design/` | `design/05-conventions-and-tables.md:26-32` | `CMakeLists.txt:48-60,102-122,257-275`; `cmake/DualCWarnings.cmake:24-61` | fixed in this run's commit: three rules and the per-target dialect in `design/05 § 5` |
| 3 | (c) | The test-suite conventions #32 and #33 created are stated in the present tense only in the record. Their rules are: each Catch case runs in its own working directory, removed on pass and kept on failure; `Approx` against zero takes a `.margin(`, which the gate enforces; each CLI content check is a CTest fixture of the test that writes its file; a cancel or progress test never reuses a timed poll count across runs. A contributor writing a test has no `design/` page to read them on | `roadmap/17-code-audit-and-hardening/15-test-coverage-batches.md:39-44,131-133`; `…/14-build-hardening-ci.md:68`; `…/09-local-checks-gate/01-the-docs-checks.md:81-82` | `tests/test_main.cpp:38-67`; `scripts/check.py:987-988`; `examples/CMakeLists.txt:399-409`; `tests/test_cancel_progress.cpp:222-231` | fixed in this run's commit: `design/09` § Testing conventions, indexed in `design/README.md` |

**(a) — 1 finding, above.** Read and true:

- **`design/06 § 7`:** every default against the headers. Each of the three knobs #32 varied
  for the first time does what its row says: `clampVertexToCell`, `clampToleranceCells`
  (projection up to the tolerance, then the mass point), and `qefRegularization` (1e-3 keeps
  a 0.046 eigenvalue that 0.1 truncates).
- **`design/04:109-110`:** "a cube has at most 4 surface components". The 256-configuration
  census reaches exactly 4, on 2 configurations.
- **`design/08:113-115`:** the active copy gives the gradient, so seams stay sharp.
- **`design/10:79-81`:** the bit-identical claim, held by `test_cancel_progress.cpp` at every
  thread count, monolithic and tiled.
- **`design/10:36-41`** and the glossary's *Gate* row against `gate.yml`.

**(b) — 1 finding, above.** The other design pages that name a changed file do not depend on
the change:

- `design/09:16,27,40` name `example_common.cpp`, whose one change maps
  `MZ_DEFAULT_COMPRESSION` to the same level, so the output bytes are unchanged;
- `design/01:105` names `test_parallel.cpp`, unchanged;
- `design/README.md:71` names `test_pipeline.cpp`, unchanged.

Notes, not findings — omissions for a session to take or leave:

- `design/06:26` does not say that `qefRegularization <= 0` selects the default 0.1
  (`src/contourer.cpp:67,138-140`). #32 pinned this "as found"
  (`tests/test_contourer.cpp:397-398`). The `simplificationError` row next to it does state
  its `<= 0` sentinel.
- `design/08:113-115` does not describe the `repeated` cost rule: one child evaluation when
  the child provably fits in one period (`foldExact_`), else the home tile and its seven
  nearest neighbours (`src/implicit/domain_ops.cpp:186-194,274-301`). The rule holds; 17/15
  measured that for the four strut cells the single fold is already exact, an input to #35,
  not a design error.

**(c) — 1 finding, above.** Other hits:

- The seed grep (`today|currently|at present`) returns the hits the earlier runs cleared, plus
  `roadmap/17-code-audit-and-hardening/07-repeat-tiling-fix.md:103` ("the comment currently
  overstates"). That line now carries its own dated correction.
- The pages born in this plan describe what changed, dated by their page: 17/13, 17/14,
  17/15, 20/03 and 20/04.
- 20/03's job table is the one place that states which CI jobs exist and which are required.
  It is kept current by dated cell appends ("**yes** since 2026-10-06"), not rewritten, and
  `.github/workflows/gate.yml` is the ground truth. CI is not an engine mechanism, so it is
  not a `design/` finding.

**(d) — 0 findings.** The only page with no inbound link outside its folder README is
[05](05-2026-10-05-after-50.md), the last run, and this run links it.

**(e) — 28 DEFERRED rows read, 0 triggers met.** The decisions index now has #31 and #33 as
DONE and adds D-50.

- **D-40** (`docs/raw/` over 50 text files or 1 MB): 24 text files and 0.96 MB, still the
  nearest to firing.
- **#8**: the "CI artifact" in its trigger now has a CI to come from, but no workflow
  publishes an artifact. The hosted-CI plan put that out of scope (§ 6), so the trigger is
  unmet. It fires with the first release job.
- **#25** (the next domain wrapper or decorator): none added; `src/` is unchanged.
- **#11** / **#27**: no benchmark suite, and no work in #18 or #27 asking for before/after
  numbers. #35's new input (a symmetry flag for strut cells) is not one of #11's named
  triggers.
- **D-44**, **D-45**: no stray-`.part` report, no CLI progress display, no write phase
  measured. #52's Windows rename fix is not a duration.
- **D-50**: born on this day.
- The rest are unchanged.

## Counts

(a) 1 · (b) 1 · (c) 1 · (d) 0 · (e) 0 — **3 findings, all fixed in this run's commit**, with
the first note (`qefRegularization`'s sentinel, `design/06:26`); the second is left to #35. Finding 1 is run 05's finding 1 surviving outside the page run 05
fixed: a full run reads pages, but the wrong word also lived in a code comment, a help text
and a record entry. When a finding is a wrong name for a mechanism, grepping the whole tree
for the same words finds every copy.

---

← Back to the [semantic-lint runs](README.md) · the [Docs layers index](../README.md).
