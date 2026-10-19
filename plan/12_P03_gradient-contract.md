# P03 — The gradient contract (#34)

§ 1 (gradient decision) and § 2 "Gradient contract" of
[the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md). Branch
`feat/hygiene-and-editor`. Never push. The owner chose the **unit direction** contract: do not
change what `gradientAt` returns for any field except `WindingNumberField` (T2).

### T1. The contract text and the census

1. **Census.** Re-verify the raw plan's census by grepping every `gradientAt` override under
   `include/` and `src/`. Classify each override as unit, pass-through, blend-shortened or raw-FD.
   Record the table, with file:line, in a new child page of block 17 (next free number, e.g.
   `18-gradient-contract.md`). Index the page in 17's README, and append a dated pointer to it in
   04 § #34.
2. **Header comment.** Rewrite `include/dualc/implicit.h:49-51` to say:
   - it is a unit outward direction;
   - inside smooth blends and `mix` its length may be below 1 (the lerp of two unit vectors);
   - it is (0,0,1) where the direction is undefined;
   - it is not the magnitude of ∇f. Name the true-gradient virtual for that here (default name
     `trueGradientAt`).

   Fix `GridField`'s header comment (`implicit.h:233-234`) the same way.
3. **Design page.** The `docs/design/` page that owns the field interface states the contract,
   rewritten in place. That makes this a src + design session, so P08 runs the semantic lint.

#### Acceptance criteria

- The census page exists and is indexed. The header and the design page state the unit contract.
- `--docs --strict` is green; the work is committed.

### T2. The true-gradient path, the Newton step, one FD helper

1. **The virtual.** Add `virtual Vector3 trueGradientAt(Vector3 p) const` (or the name T1 fixed)
   to `ImplicitField`.
   - Default: a central difference of `valueAt`. Move the code from `NormalizedField`
     (`decorators.cpp:363-377`, e=1e-4).
   - Override it analytically where that is cheap and exact: sphere, plane, and `GridField`'s
     trilinear gradient before normalisation (`grid_field.cpp:108-141`).
   - Pass-through decorators forward it, applying the chain rule for scale and transform.
2. **`NormalizedField`.** `NormalizedField::valueAt` divides by `|child->trueGradientAt(p)|`.
   `test_tpms.cpp:199-215` must stay green unchanged.
3. **Newton step.** `closestSurfacePoint` (`src/implicit/implicit_field.cpp:71-86`) takes the step
   `q - ĝ · f/|∇f|` with the true magnitude, and takes the normal at the result from `gradientAt`.
   Add tests on a gyroid (non-SDF) and on a winding field: the projected point's |f| must be smaller
   than with the old step. Record the numbers.
4. **`WindingNumberField`.** Its `gradientAt` (`winding_field.cpp:76-96`) now returns a unit
   direction, and its raw FD becomes its `trueGradientAt`.
5. **One FD helper.** Merge the two normalised `fdGradient` copies (`domain_ops.cpp:17-28`,
   `lift.cpp:12-23`) into one internal helper in `src/internal/`, with no behaviour change.
6. **Property test.** Every concrete field type's `gradientAt` has length ≤ 1 + 1e-9 over sample
   points, and length 1 outside blends.
7. **Gates.** Run the full gate, then
   `cmake --build build-gl -j && python3 scripts/check.py --gpu --build-dir build-gl`, since the
   CPU side of the `normalize` node moved. Parity must keep its count (82/82 at `aef902e`).

#### Acceptance criteria

- `trueGradientAt` exists with its tests, and `NormalizedField` and `closestSurfacePoint` use it.
- The full gate and `--gpu` are green, with the counts in the handoff and on the census page.

## Exit criteria

- 04 records #34's two gradient bullets: the contract DONE; the `GridField` normal next, in P04.
- Nothing is pushed.
