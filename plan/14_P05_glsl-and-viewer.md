# P05 — dualc_field_view (GLSL)

§ 3 P05 of [the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md); the GPU layout from the
block's `01-study.md` and P03's packed sizes. Branch `feat/porous-shell`. Never push.

### T1. The porous GLSL emitter

1. In `examples/field_glsl.{h,cpp}`:
   - A data-texture kind beside the mesh `sampler3D` (RGBA32F 2D, `texelFetch`), carried in
     `GlslScene` and uploaded by `raymarch_gl.cpp`.
   - The `porous` branch of `emitNodeImpl` for the four cases (bin search, distance, `smin`), with
     `smooth` as a uniform, reading every key so `rejectUnreadParams` passes.
   - `nodeStepScale` 1.0 and `nodeFeatureScale` = the smaller spacing.
   - The seeds and the complex come from the same builder code as the CPU node (shared through
     `example_common` or the library), never regenerated differently.
2. GL-free tests in `tests/test_field_glsl.cpp`: the source compiles to the expected uniforms and
   textures, and the texture sizes stay within the limits recorded in P01.
3. If the exact path does not fit this unit, route the node through `emitBakedGrid`. Record it as
   `**Chosen in-run (pending owner review)**` with the reason, and leave the exact path as a
   DEFERRED item with its trigger.

#### Acceptance criteria

- `ctest --test-dir build -R field_glsl` is green, and `build-gl` builds.

### T2. Parity and the viewer

1. Parity cases in `examples/dualc_glsl_parity.cpp`: at least the four named in the raw plan
   § 3 P05 T2. Bump `parity_expected_cases` in `scripts/check_data.json` in the same commit.
2. `dualc_field_view`: a change to a structural key rebuilds the scene and its textures (find how
   file-watch rebuilds today). `smooth` and the thickness keys nudge as uniforms.
3. Run the GPU tier yourself:
   `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict --build-dir build-gl`.
   If no GL context is available (an unattended host with a locked display), record it and leave
   the GPU tier to P08.
4. Snapshot PNGs of the sponge recipe (walls + struts), through the viewer's snapshot path, with
   the paths recorded for the owner (raw plan § 3 P05 T2 on where they go).

#### Acceptance criteria

- The parity cases exist and pass (or the missing GL context is recorded), and the count is
  bumped. The snapshots are listed in the handoff.

## Exit criteria

- The viewer shows the porous node live, matching the CPU field within the parity tolerances.

← Back to the [plan](README.md).
