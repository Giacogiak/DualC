# #32 — the test-coverage ledger, in two batches

The delivery record of [04 #32](04-engineering-quality.md#32-test-coverage-ledger), Phase 4
of the [hosted-CI plan](../../raw/2026-10-05-hosted-ci-plan.md), on branch `ci/test-coverage`.
Its findings are C42–C54 in [06](06-findings-ledger-testing.md). Batch A (2026-10-07) corrects
the record and makes the mechanical fixes; Batch B writes the missing tests. Each claim below
was checked against `ab5ce1e` before it was scoped.

## Batch A — correct the record, then the mechanical fixes

Landed 2026-10-07.

### The record was wrong in three places

- **Unguarded `Approx(0.0)`: 38, not 36.** 29 in `test_primitives.cpp`, 3 in `test_octree.cpp`
  (inside a ternary), 3 in `test_lift.cpp`, one each in `test_mesh_bvh.cpp`,
  `test_validation.cpp` and `test_field_graph.cpp`. The last is split over two lines
  (`==` on one, `Catch::Approx(0.0)` on the next), which no line-based count sees.
- **`REQUIRE_THROWS_AS`: 17 across five files**, not "exactly one outside the field-graph
  parser" (C46): `test_parallel` 6, `test_cancel_progress` 4, `test_field_graph` 4,
  `test_field_glsl` 2, `test_primitives` 1.
- **C43's disposition.** The fixed temp filename was `temp_directory_path() /
  "dualc_fg_box.obj"`. "Two concurrent CI jobs would collide on it" is not true of CI: matrix
  jobs run on separate machines. The collision is `ctest -j` within one tree, or two build
  trees on one machine.

The `--metrics` needle was wrong too. It matched `== Approx(0.0)` whether or not a `.margin(`
followed, so it read **70** on `ab5ce1e`, and it could not see the split comparison.

### What changed

- **The 38 comparisons take `.margin(1e-12)`.** `Approx`'s default tolerance is relative,
  so against zero it was exact equality. A margin only widens it: all 38 passed before and
  after.
- **`approx-zero`, a fast gate check**, fails on any `Approx(<zero literal>)` in `tests/`
  with no `.margin(` after it. It spans lines, and has a pass/fail fixture pair. `--metrics`
  now counts what the check counts. The metric was report-only so as not to block "a
  legitimate new `Approx(0.0)`". There is no such thing: against zero the relative epsilon is
  `== 0.0`, and a tolerance is a margin.
- **Each Catch case runs in a working directory of its own.** `tests/test_main.cpp`, empty
  until now, registers a Catch2 event listener. Each case moves into
  `<build>/tests/dualc_test_work/<pid>-<n>`, and the directory is removed when the case
  passes, kept when it fails. Several cases write by bare name: the streaming and cancel
  exports, and `"nope.stl"` in three of them. Under `ctest -j` that was safe only because no
  two concurrent cases picked the same name. The listener makes it safe for any name.
- **`test_field_graph.cpp` writes `dualc_fg_box.obj` by bare name**, inside its case's
  directory, not under the machine-wide temp directory.
- **`cli_gen_demo` writes into `build/examples/cli_gen_demo_out/`.** It ran in the CLI tools'
  shared output directory and rewrote `cube.obj`, `sphere.obj`, `torus.obj` and `bracket.obj`
  while `cli_boolean`, `cli_lattice*`, `cli_slice`, `cli_field_mesh`, `cli_field_strut` and
  `cli_demo_*` read them. This second `-j` race was not in the audit; it was found while
  scoping this batch.

### Before and after

| Counter | Before (`ab5ce1e`) | After |
| --- | --- | --- |
| unguarded `Approx(0.0)` in `tests/` | 38 (`--metrics` printed 70) | **0**, gated by `approx-zero` |
| fixed temp filename under `temp_directory_path()` | `test_field_graph.cpp` | none |
| CTest cases sharing one working directory | 249 Catch cases in `build/tests`, 20 CLI tests in `build/examples` | 0 Catch cases; the CLI tests write distinct names, and `cli_gen_demo` has its own directory |
| `REQUIRE_THROWS_AS` | 17 in five files | 17 (Batch B's) |
| `ctest` total | 270 | 270 |
| gate checks | 31 | 32 |

### Measured (Linux, GCC 15, Ninja, Release, 8 cores)

| Run | Before | After |
| --- | --- | --- |
| `ctest -j 8`, three in a row | 270/270 each, 49 s, 55 s, 59 s | 270/270 each, 57 s, 61 s, 63 s |
| `ctest -j 4` | — | 270/270, 64 s |
| `ctest` serial | — | 270/270, 86 s |
| `dualc_tests` as one process (all 249 cases in turn) | — | all pass, 589,341 assertions, `dualc_test_work/` empty afterwards |

`ctest -j 8` was green **before** the fix too. The hazards were real but latent: no two
concurrent cases shared a name on this run order, and `cli_gen_demo` writes the same bytes it
replaces, so a reader only fails inside the short truncate-and-rewrite window. That is what
"parallel safety is luck" meant. `strace` on `FileMeshResolver caches by path` shows the
`chdir` into `dualc_test_work/<pid>-0`, the relative `open` of `dualc_fg_box.obj`, and the
`chdir` back.

### The serial gate is kept, for a new reason

The `ctest` check stays serial. Its correctness reason (C43) is gone. Its cost reason is not:
`-j 8` saves about a third (86 s → 57 s), because the engine already parallelises inside each
case. Oversubscribing the cores is what exposes the timing-dependent cancel/progress tests:
[14](14-build-hardening-ci.md) finding 9 was one, and finding 8, the macOS `parallelForPolled`
flake, is still open. `ctest -j` by hand is safe. The dated line is in
[09/02](09-local-checks-gate/02-three-decisions.md), the reason in `check_ctest`'s docstring.

### Proven to fail

- The selftest fixture `approx-zero/fail` (`Catch::Approx(0.0)` split over two lines) answers
  FAIL, `approx-zero/pass` answers OK (`--selftest`, 30 fixture runs, 0 wrong).
- On the real tree: `.margin(1e-12)` removed from `test_lift.cpp:28` turned the check red
  with `tests/test_lift.cpp:28: Approx(0) without .margin(...) is exact equality`; restored, green.

### What Batch B inherits

- The six bullets of the plan's T2: `partitionCubeEdges` on all 256 configurations,
  refinement convergence, the four unvaried `ContourerParams` knobs, `interpolateNormals` and
  output-normal content, CLI smoke tests that check content, and the strut-lattice oracle.
- A file written by a test can use a bare name; the case's directory isolates it.
- [04](04-engineering-quality.md) is at about 15.3 KB of the 15 KB cap after this batch's
  note. Batch B's closing entry for #32 will not fit: the page is split into a same-numbered
  folder first, never appended past the cap.

---

← Back to the [audit ledger](README.md) · the [Roadmap index](../README.md).
