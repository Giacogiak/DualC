# P04 — A continuous `GridField` normal (#34)

§ 1 and § 2 "Gradient contract" of [the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md).
This phase builds on P03's `trueGradientAt`. Branch `feat/hygiene-and-editor`. Never push.

### T1. Node-gradient interpolation, measured, and #34 closed

1. **The new normal.** `GridField::gradientAt` (`src/implicit/grid_field.cpp:108-145`) returns
   the normalised trilinear interpolation of per-node gradients. Each node gradient is a central
   difference over the neighbouring nodes, one-sided at the grid boundary. The resulting
   direction is continuous across cell faces. `trueGradientAt` keeps the exact trilinear gradient
   of the interpolant (P03).

   Node gradients can be precomputed at construction (3 extra floats per node) or computed on
   the fly. The default is on the fly, unless the timing in step 3 shows it more than 1.2×
   slower. Record which you chose and why.
2. **Tests** (`tests/test_grid_field.cpp`).
   - Keep the existing "unit length and outward" test.
   - Add a continuity test: probes at ±ε either side of a cell face give normals whose angle
     tends to 0 as ε → 0. It must fail on the old code; show both runs.
3. **Measure before and after** on a baked-mesh demo: a graph with a `bake=N` mesh node from
   `docs/command_reference/11-dualc_field/`, or block 21/04's large pair.
   - Record the face count and the wall time at `--threads 0`.
   - Add a Hausdorff or max-vertex-shift figure if a tool for it exists in the tree. If none
     does, record the vertex-count change and a histogram of normal angles.
   - Write the results in P03's census page and in 04 § #34.
4. **The Newton step.** Check `closestSurfacePoint`'s users and the sampler fallback once more on
   a grid field. The step now uses `trueGradientAt` (P03), so it no longer relies on the
   normalised direction. State this in the record.
5. **Close #34.** Mark it DONE in 17's tracked-items table and in 04, with its evidence. If a
   bullet could not be closed, mark #34 PARTIAL with what remains; do not claim DONE.
6. **Gates.** Run the full gate. Add `--gpu` only if a GLSL path changed. Normally none has: the
   baked grid upload is trilinear on the GPU side, and the shader normal is a finite difference.

#### Acceptance criteria

- The continuity test is red before the change and green after, and the measurements are recorded.
- #34 is DONE (or honestly PARTIAL), the full gate is green, and the work is committed, not pushed.

## Exit criteria

- 17's table and § Latest verification are updated, and nothing is pushed.
