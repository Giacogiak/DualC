# Handoff — P02.1: deterministic seeds in any volume

Unit P02.1 of [`plan/11_P02_seeds.md`](../11_P02_seeds.md) § T1, the only unit of P02; session
of 2026-10-21, branch `feat/porous-shell` in `~/Documents/DualC-porous`. Nothing was pushed,
no remote was touched, and `main`, `feat/hygiene-and-editor` and `~/Documents/DualC` were left
alone. The code commit is `3c94d02`. The record is
[23/02 #63](../../docs/roadmap/23-porous-shell/02-record.md#63-deterministic-seeds-in-any-volume),
now **DONE**. This handoff points at it and adds what the next units need.

## What changed, and why

| File | What |
| --- | --- |
| `include/dualc/porous.h` (new, public, in the `dualc.h` umbrella) | `Pcg32`, `SeedDistribution`, `DensityRamp`, `densityRampWeight`, `SeedParams`, `SeedSet`, `generatePorousSeeds`, `kPorousMaxSeeds`, `kPorousMaxRelax` |
| `src/implicit/porous_seeds.cpp` (new) | Bridson with a variable radius, jitter, random, the pad, Lloyd, ceilings, cancel |
| `src/internal/seed_grid.h` (new, header-only) | `internal::SeedGrid`: a uniform linked-list grid of point indices, `insert`, `forEachNear`, `nearest` (exact, ties to the smaller index) |
| `CMakeLists.txt` | the new source; `-ffp-contract=off` on it for GCC/Clang |
| `tests/test_porous.cpp` (new) + `tests/CMakeLists.txt` | 10 `porous: …` cases and one hidden `[porous-bench]` case |
| `STRUCTURE.md` | the four new files |
| `docs/roadmap/23-porous-shell/02-record.md`, `README.md`, `docs/roadmap/README.md` | #63 DONE, the measurements and the in-run choices |
| `docs/roadmap/23-porous-shell/01-study.md` | italic P02 notes on three † rows of the defaults table |

### The public types P03 builds on

- **`dualc::SeedSet`**:
  - `points` (`std::vector<Vector3>`) and `isPad` (`std::vector<uint8_t>`, 1 where the
    volume's value > 0);
  - `box`: the volume's bounds grown by `pad`, and every seed lies in it;
  - `pad` = `maxSpacing`;
  - `minSpacing` and `maxSpacing`.

  These are the Delaunay input, and `box` is the padded box that the Voronoi-edge clamp
  needs.
- **`dualc::generatePorousSeeds(volume, control*, SeedParams, CancelToken*)`.**
- **`dualc::SeedParams`**: `dist`, `spacing`, `spacing2` (≤ 0 means equal to `spacing`), `d0`,
  `d1`, `ramp`, `relax`, `seed` (uint64), `jitter`, and `numThreads`, which does not change the
  output. P04's parser maps the node's keys onto these one to one.
- **`dualc::Pcg32(initState, initSeq)`**, with `next()` and `nextDouble()`. The streams in use:
  - placement uses `Pcg32(seed, 0)`;
  - Lloyd iteration `i` uses `Pcg32(seed, i + 1)`;
  - **P03 should take its own `initSeq`**, for example `1000`, if it needs randomness. It
    should not need any, because the insertion order is Morton.
- **`dualc::internal::SeedGrid`** (`src/internal/seed_grid.h`):
  - `SeedGrid(box, cell)`;
  - `insert(i, p)`, with indices in increasing order;
  - `forEachNear(p, r, fn)`;
  - `nearest(p, pts)`;
  - `static cellCount(box, cell)`.

  It is the "nearest-seed structure reused by P03" for the start-seed bins.
- **Errors.** Bad parameters and the ceilings throw `std::invalid_argument`, with messages
  that start `porous seeds: …`; P04 maps them to `GraphError`. A requested token throws
  `dualc::Cancelled`.

## The in-run choices

Each is **Chosen in-run (pending owner review)** and is recorded in 23/02 #63:

1. **Lloyd's weight is `(spacing/spacing(x))⁵`, not the table's `³`.** This corrects a P01
   default.
   - The basis is the CVT cell-size law ρ^(−1/(d+2)).
   - Measured on the graded test box, mean of 4 seeds, against an expected ratio of 4.096:
     the `relax=30` ratio is 3.96 with ⁵ and 3.78 with ³; at `relax=10` the two are equal
     (3.90 / 3.91).
2. **Lloyd draws fresh samples per iteration**, 32 per seed, uniform in the padded box. Those
   outside `V < pad` are dropped, not redrawn.
3. **Bridson's grid** is a linked-list grid at `max(rmin, rmax/2)` instead of `rmin/√3`. A
   restart scan over `rmax` cells, with 4 tries each, reaches every connected part.
4. **Graded `jitter` / `random` thin** a layout drawn at the smaller spacing, with keep
   probability `(min/spacing(x))³`.
5. **The ceilings:**
   - a padded box of more than 32 × 2,000,000 cells at the smaller spacing is refused before
     any allocation;
   - the seed ceiling is checked per accepted seed;
   - `relax` must lie in 0…50.
6. **Graded-parameter errors.** `spacing2 ≠ spacing` with no control is an error, and so is a
   graded density with `d1 = d0`.
7. **The gaussian ramp** uses DualC's own `exp`: range reduction plus a degree-13 Taylor
   polynomial, with basic arithmetic only. libm's `exp` is not bit-identical across
   platforms. It equals the libm formula to 1e-14 (tested).

## Measured, and under what

Host: this Linux box, 8 threads, GCC 15, Release (`-O3`), the `build/` tree.

- **Seeds per second.** Hidden case `build/tests/dualc_tests "[porous-bench]"`, `poisson`.
  Two runs were made and the slower is given; the full table is in 23/02 #63.

  | Volume | Spacing | Seeds | Seeds/s, `relax=0` | Seeds/s, `relax=5` |
  | --- | --- | --- | --- | --- |
  | Demo torus mesh (`MeshSource`, 9,216 triangles) | 0.1 | 2,562 | 5,760 | 1,129 |
  | Demo torus mesh | 0.05 | 15,594 | 5,511 | 991 |
  | Demo torus mesh | 0.025 | 106,484 | 5,466 | 913 |
  | Analytic sphere r = 1 | 0.1 | 3,458 | ≈ 55,000 | 29,888 |
  | Analytic sphere r = 1 | 0.05 | 23,344 | ≈ 55,000 | 27,482 |
  | Analytic sphere r = 1 | 0.025 | 171,247 | ≈ 55,000 | 23,302 |

  The volume's `valueAt` dominates: 8.6 calls per seed, about 21 µs each on that mesh
  (measured with a counting wrapper in scratch). Each Lloyd iteration adds 32 `valueAt` per
  seed, run in parallel.
- **Graded density ratio** (dense/sparse slabs, spacing 0.5 → 0.8, expected 4.096):

  | Distribution | Seed 5 | Mean over 20 seeds |
  | --- | --- | --- |
  | `poisson` | 4.23 | 4.03 |
  | `jitter` | 4.34 | 4.18 |
  | `random` | 3.47 | 4.08 |
  | `poisson` + `relax=10` | 3.93 | — |
- **Interior cell-volume CV** before and after `relax=10`, by Monte-Carlo (400k samples):

  | Volume | `poisson` | `random` |
  | --- | --- | --- |
  | box | 0.119 → 0.092 | 0.421 → 0.119 |
  | sphere | 0.152 → 0.095 | 0.388 → 0.124 |

## Checks run

The rules for P01–P07: no full gate.

- `cmake --build build -j`: no warnings in the new files (`-Wall -Wextra -Wpedantic`).
  `compile_commands.json` shows `-ffp-contract=off` on `porous_seeds.cpp`.
- `ctest --test-dir build -R porous`: 10/10 passed.
- `ctest --test-dir build -R 'porous|field-graph|GLSL codegen' -j8`: 29/29 passed, in 3.7 s.
- The driver's own smoke filter, `ctest -R 'porous|field_graph|field_glsl'`: 10/10 passed. The
  filter now finds the porous cases.
- `python3 scripts/check.py --docs --strict` on the staged tree: 27 OK + 1 INFO
  (`index-staleness`, pre-existing), PASS. The pre-commit hook passed on `3c94d02`.
- No docs file was deleted or moved, so no loss audit was due.

## What I got wrong on the way

- **The graded-density test's first slabs were too small.** At 2 × 4 × 4 the sparse slab held
  38 seeds and `REQUIRE(sparse > 50)` failed. The slabs are now 3 × 7 × 7. I checked over 20
  seeds that no distribution is biased: seed 5's `random` 3.47 is a fluctuation, and the mean
  is 4.08.
- **`SeedGrid`'s `dimFor` first divided by 1.0 instead of the cell size.** It was caught on
  reading, before the first build.

## Open, and what the next units need

- **P03:**
  - Take `SeedSet::points` as given; they are bit-stable.
  - Pad seeds are the ones flagged in `isPad`.
  - `SeedSet::box` is the padded box for the Voronoi-edge clamp.
  - Use `internal::SeedGrid` for the start bins (`nearest` is exact).
- **P04:**
  - The node's keys map onto `SeedParams`.
  - `jitter` and `random` grade by thinning, so P04's docs should say that a graded
    `dist=jitter` is no longer a lattice.
  - **`relax` on a mesh volume is costly**: about 1,000 seeds/s at `relax=5`. The docs, or a
    later item, could recommend baking a mesh volume to a grid first, or reusing one sample
    pool across iterations. That reuse cuts `valueAt` by the iteration count but costs about
    1 KB per seed. Neither is built.
- **The owner's review list (P08.2) gets** the 7 in-run choices above. The † rows of the 23/01
  table are annotated in italics where P02 departs from them.
- **No human check is raised by this unit:** everything was verified headless.

## Exit criteria of P02

*The seeds are deterministic, graded and relaxed, and tested, with the same output for the same
input on any OS (no platform-dependent RNG or distribution).*

**Holds.** The evidence:

| Claim | Evidence |
| --- | --- |
| Deterministic | `porous: same seed, same seeds; another seed, other seeds`: identical digest on repeat and across thread counts (0 and 1), on a sphere and a box, for every distribution, graded, `relax=2`; a different digest for another seed |
| No platform-dependent RNG or distribution | `Pcg32` matches O'Neill's reference stream (`porous: Pcg32 matches …`). The source has no `std::*_distribution`, no `<random>` and no libm transcendental: the only one, `exp`, is DualC's own. The file is compiled without FMA contraction, and Lloyd's sums run in sample order |
| Graded | `porous: the graded density follows (spacing/spacing2)^3` holds within 20 % for all three distributions and for `relax=10`. `porous: poisson keeps the local minimum distance` holds at ≥ 0.99 × the local radius, graded and uniform |
| Relaxed | `porous: relax lowers the variance of the cell volumes`: CV(`relax=10`) < 0.8 × CV(0) on a box and a sphere, for `poisson` and `random` |
| Tested | 10 `porous:` ctest cases, green |

Cross-OS bit identity was not run on macOS or Windows: this run reads no remote CI. The claim
rests on the construction above. The volume's own `valueAt` (for example libm `sqrt`, which is
correctly rounded) is the caller's. The first CI run of this branch would confirm it if a
golden digest were added; that is optional and was not done here.

**T1 acceptance:**

- `cmake --build build -j && ctest --test-dir build -R porous` is green.
- The measurement is in the block (23/02 #63).
- Everything is committed.
- The public types P03 builds on are named above.
