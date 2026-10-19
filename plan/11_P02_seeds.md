# P02 — Seeds

§ 3 *The node, as designed* and § 3 P02 of
[the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md); the defaults table in the block's
`01-study.md` (P01) overrides the raw plan where they differ. Branch `feat/porous-shell`. Never push.

### T1. Deterministic seed generation in any volume

1. Add `include/dualc/porous.h` (the public seed + complex types and the field class P04 will
   fill in; declare only what this unit implements) and `src/implicit/porous_seeds.cpp`, wired
   into `CMakeLists.txt` the way the other `src/implicit/` files are.
2. Implement:
   - `Pcg32` with its own `[0,1)` double conversion (no `std::*_distribution`).
   - `poisson`: Bridson in 3D over the volume's box. Its radius comes from
     `spacing → spacing2` through the control value, `d0`, `d1` and the ramp
     (`linear`, `smooth` = smoothstep, `gaussian` = `1 − exp(−x²/2σ²)` with σ chosen so that
     `x = 1` reaches 99 %; record the formula). A background grid sized to the minimum radius.
   - `jitter`: a grid at `spacing`, each point offset by `jitter · spacing · U(−½, ½)³`.
   - `random`: N = volume / spacing³, uniform, rejected outside.
   - Acceptance region: the volume's value `< pad`, with pad = the larger of the two spacings.
     Seeds in the pad are flagged.
   - `relax=N`: Lloyd by sampling (a fixed number of uniform samples per seed, each assigned to
     its nearest seed, each seed moved to its centroid). Pad seeds are relaxed but kept in the
     acceptance region.
   - Ceilings (2,000,000 seeds, `relax` ≤ 50) give a clear error. Poll the cancel token through
     the existing plumbing (`include/dualc/progress.h`); a nearest-seed structure that is reused
     by P03 is welcome.
3. Tests in a new `tests/test_porous.cpp` (registered in `tests/CMakeLists.txt`): the six
   properties listed in the raw plan § 3 P02, each with tolerances, on a sphere volume and on a
   box. Keep runtimes in the existing tests' range.
4. Measure the seeds per second on a demo mesh volume at three spacings, and put it in the
   block's record page.

#### Acceptance criteria

- `cmake --build build -j && ctest --test-dir build -R porous` is green.
- The measurement is recorded in the block, everything is committed, and the handoff names the
  public types P03 builds on.

## Exit criteria

- The seeds are deterministic, graded and relaxed, and tested, with the same output for the same
  input on any OS (no platform-dependent RNG or distribution).

← Back to the [plan](README.md).
