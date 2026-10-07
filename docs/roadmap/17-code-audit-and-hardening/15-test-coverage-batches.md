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

CI run [37584887029](https://github.com/Giacogiak/DualC/actions/runs/37584887029) on `e637913`:
every job green — `build` on Ubuntu (4 min 32 s), macOS (4 min 30 s) and Windows (10 min 49 s,
the listener's `_getpid` path), `sanitize` (14 min 17 s, ASan + UBSan over the listener),
`gpu`, `docs`.

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

## Batch B — the missing tests

Landed 2026-10-07. Every bullet has a test; each number below was measured on Linux, GCC 15,
Release, before its bar was written, and each new check was broken on purpose once.

| Finding | Test | What it asserts |
| --- | --- | --- |
| C45 | `test_cube_components.cpp`, the 256-configuration sweep | crossing ⇔ labelled, dense ids, ≥ 3 edges per component; on every face each crossing pairs inside its component (inside-pair rule on saddles); the component count equals an oracle that never pairs crossings — *k* loops cut the cube's surface into *k* + 1 regions; the census 2 / 162 / 82 / 8 / 2 for 0–4 components |
| C47 | `test_accuracy.cpp`, convergence | max and mean vertex error against an exact SDF fall ≥ 3× per level, depth 4→7, sphere and torus; measured 3.7–4.7× (O(h²)), 7.2e-5 and 1.4e-4 at depth 7 |
| C48 | `test_contourer.cpp`, three leaf cases; `test_accuracy.cpp`, collapse | `qefRegularization` 1e-3 solves a 0.046 eigenvalue, 0.1 drops it (0 selects the default, pinned as found); `clampVertexToCell` pulls a vertex 0.436 cells out back to the face; `clampToleranceCells` 1.0 and 0.5 project, 0.4 and 0 fall back to the mass point; `simplificationError` 1e-6 cuts the depth-7 sphere from 127,740 to 112,926 faces, error 7.2e-5 → 2.2e-4 |
| C49 | `test_accuracy.cpp`, two cases | `MeshBVH::segmentFirstHit`: sharp returns the face normal, smooth the barycentric blend recomputed from geometry-central's vertex normals; end to end on a cube at depth 5, sharp makes all 20,184 Hermite normals axis-aligned and every vertex lies on the cube, smooth leaves 24 axis-aligned and a vertex 1.2e-2 off |
| C50 | `test_accuracy.cpp`, normals | on a sphere, torus and box, every output normal is unit (worst 4e-16) and `n · ∇f ≥ 0.99` (measured ≥ 0.999999) |
| C52 | `examples/check_cli_output.cmake`, 21 `*_content` CTest cases | PNG signature and IHDR 128 × 128; binary STL size = 84 + 50 *n*; 3MF unpacks with its three parts; STL and 3MF triangle counts equal the same run's OBJ face count; OBJs all-triangle and non-empty; the SVG complete; `--dump-json` parses with root op `difference`; the generated cube exactly 8 / 12 |
| 07's oracle | `test_strut_lattice.cpp` | 600 seam-targeted points join the 400 uniform ones; the comment now says what it certifies — see below |

CI run [37591221067](https://github.com/Giacogiak/DualC/actions/runs/37591221067) on `2b3036b`:
every job green — `build` on Ubuntu, macOS and Windows (the content checks' `cmake -P` and
`ARCHIVE_EXTRACT` included), `sanitize` (all 300 under ASan + UBSan), `gpu`, `docs`.

The content checks are CTest fixtures of the tests that write their files, so `ctest -R
cli_slice_content` reruns `cli_slice` first and `ctest -j` orders them. One pass over a 10 MB
OBJ costs about 1 s.

### The strut-lattice oracle certified what it claimed

[07](07-repeat-tiling-fix.md) says the oracle passed clean over 678 misplaced vertices of a
shipped octet lattice, because uniform points never land on a seam. **Measured, that does
not hold.** The four strut cells are mirror-symmetric about their own faces, so any
neighbour's strut has a mirror image in the own tile that is at least as close: the single
fold is exact in value for them. With `RepeatField` forced back to the single fold, the gate
still passes — 1,000 points, 600 within a cap's reach of a seam or node — and a 41³ grid
across the seams reads a difference of exactly 0 for all four cells. Re-running 07's case
(`octet(wavelength=0.4,radius=0.05)`, depth 7) with the fold and with the fix: 260 vertices
differ, **all within two cells of the root box**, none near a seam, and the fold's are not
worse (summed |f| over the moved set 0.038 against 0.050). The fix is right — the off-centre
sphere in `test_domain_ops.cpp` is the defect — but it did not change strut geometry, and
strut lattices pay its 2.3× for exactness they already had: an input for
[#35](04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation). The oracle's
comment now says so, and the seam points stay as cheap cover for an asymmetric cell.

### Proven to fail

- **Saddle rule inverted** in `partitionCubeEdges` (outside-pair): the sweep fails 400
  assertions; the nine hand-picked cases catch it once.
- **Every QEF vertex replaced by its mass point:** the normals case (box) and the sharp-cube
  case fail. Convergence does **not** — a mass point also converges at O(h²) on a smooth
  surface — so that test proves convergence, not QEF quality.
- **A corrupt PNG signature, an STL 50 bytes short, a 3MF cut to 1,000 bytes**, run with
  `-FA '.*'` so the producers do not rewrite them: each `_content` case fails, naming the
  defect.

### Before and after

| Counter | Before (Batch A) | After |
| --- | --- | --- |
| `ctest` total | 270 | **300** (249 + 9 Catch cases, 21 content checks) |
| `ctest` serial, inside the gate | 78–90 s | 101.5 s (clean rebuild, 255 TUs, 0 DualC-origin warnings) |
| `partitionCubeEdges` configurations checked | 9 | 256 |
| `ContourerParams` knobs varied | 1 (`manifoldDC`) | 5 |
| CLI outputs whose content is checked | 0 | 20 files of 19 tests, plus `--dump-json` |

### Coverage, measured once

C51's other half. A separate build tree with `--coverage` on every target (GCC 15, Release),
the whole `ctest` run (300/300 pass), then `gcov` over libdualc's objects: **87.9 % of the
library's lines** — 2,469 of 2,810 over the 23 `.cpp` files of `src/` with executable lines,
headers left out. Lowest: `internal/qef.cpp` 53.7 % and `internal/svd.cpp` 67.9 % (the
vendored solver's unused entry points), `implicit/combinators.cpp` 63.8 %; seven files are at
99–100 %, `contourer.cpp` and `cube_components.cpp` among them. No option or CI job was added:
a percentage nobody acts on is not a gate, and no line of the gap is a known risk. Deferred
with its trigger as [D-50](../../decisions/README.md).

### 04 was condensed, not split

Batch A expected [04](04-engineering-quality.md) to become a folder before #32's close. A
folder would break three links in `docs/raw/2026-10-05-hosted-ci-plan.md`, which is
immutable, so the 2026-09-18 precedent was followed instead: #33's dated history moved
verbatim to its record page, [12](12-engineering-quality-records.md#33--build-hardening-three-batches),
and the close fits under the cap.

---

← Back to the [audit ledger](README.md) · the [Roadmap index](../README.md).
