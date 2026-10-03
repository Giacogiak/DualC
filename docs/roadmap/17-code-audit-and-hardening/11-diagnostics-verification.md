# The Diagnostics channel (#26) — re-scoping and verification

The evidence pages of [05](05-diagnostics-channel.md), the record of tracked item
[**#26**](03-correctness-and-robustness/03-diagnostics-item.md#26-a-diagnostics-channel-for-the-silent-failure-surface):
the claim-by-claim re-scoping against `71b8018`, the verification of what shipped, and
the one branch no test exercises. 05 keeps the argument, the shape of the channel and
what is deliberately not reported.

*(Moved here verbatim from 05 on 2026-09-18, when that page was brought under the size cap —
[19 Phase 5](../19-docs-layers/README.md).)*

## Re-scoping the item

**Done 2026-09-09, before any code.** #26 was written against six silent
degradations. Checked against `71b8018`, **three of the six no longer
exist**, and one of those three never did:

| Audit claim | State at `71b8018` |
| --- | --- |
| The 4096-hit ray cap truncates | Gone — retired by [#23](03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#23-ray-parity-advance-epsilon-is-relative-to-hit-distance-not-feature-size) (`71b8018`) |
| `seed` does nothing | Gone — deleted by [#28](04-engineering-quality.md#28-delete-the-two-dead-public-parameters) (`1492647`) |
| `windingNumberFast` reports "everything outside" when no tree was built | **Never held.** Both call sites build the tree when they use it: `src/implicit/mesh_source.cpp:64` gates `buildWindingTree` on `GENERALIZED_WINDING_NUMBER`, which is exactly the method `src/internal/sign_oracle.cpp:46` dispatches to it for, and `src/implicit/winding_field.cpp:52` passes `true` unconditionally. So `gwnNodes.empty()` at `mesh_bvh.cpp:802` is unreachable on a non-empty mesh; the only surviving path is `numTris_ == 0`, which is *empty input* — a different condition, and one that is now reported as `inputEmpty`. |
| An invalid `field.bounds()` becomes `BBox::unit()` | Real — `src/sampler.cpp:171` |
| Grid probes outside the baked region clamp | Real — `src/implicit/grid_field.cpp:34-44` |
| A non-manifold edge falls back to the face normal | Real — `src/internal/mesh_bvh.cpp:258-262` |
| An empty contour returns a placeholder triangle | Real — `src/contourer.cpp:706-708` |

This is a ledger correction of the same kind as the 228→229 test-count fix recorded in
[the topic README](README.md) (2026-09-01 update), and
it stands whether or not the channel had shipped. The C22 row in
[02](02-findings-ledger-craft.md) carries it.

The audit's *other* claim about this item **holds**: `buildPseudoNormalTopology`
already computes a per-edge incident-face count (`mesh_bvh.cpp:205-215`) and
only ever tests it for `== 2` (`:258`). The watertightness measurement was
being computed and thrown away, so recovering it costs two counters.

## Verification

- **`ctest` 245/245**, up from the 235 baseline — ten new `[diagnostics]`
  cases, 8,281 assertions.
- Every reported signal is demonstrated **firing**, not merely asserted
  absent: an open tetrahedron reports exactly the three edges of its missing
  face; a three-triangle fan reports its one hinge edge and six free edges; a
  field whose `bounds()` has `max < min` reports `boundsFallback` *and* the
  octree root is verified to be `BBox::unit()`; a sphere outside the sampled
  region reports `emptyContour` with `outputTriangles == 1`,
  `outputVertices == 3`, `outputBoundaryEdges == 3` and `nFaces() == 1` —
  which is also the standing proof that the old `nFaces() == 0` check could
  never fire. A baked grid is checked in all three configurations (root box
  inside, root box outside, non-grid field with a 100× oversized root box).
- **The channel is inert.** One case contours the same field with `nullptr`,
  with a `Diagnostics` at `numThreads = 1`, and with one at `numThreads = 4`,
  and compares every vertex position exactly: identical in all three, and the
  report itself is thread-count-independent.
- **All eight demo meshes byte-identical** across the change, on the hidden
  `[.][golden]` positional digest that
  [#23](03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#23-ray-parity-advance-epsilon-is-relative-to-hit-distance-not-feature-size)
  added for exactly this purpose (the suite's own assertions are topology
  invariants, which survive a moved vertex). Captured either side against a
  worktree built at `71b8018` and diffed: **identical, all eight** —
  `icosphere e98985492616d634`, `uvsphere 2d56844bb4d025f3`,
  `torus 1b7cb06a6cfe142b`, `trefoil d428f9327f7dc4f3`,
  `genus2 918c5dc23cc33cb1`, `cylinder 8fb453dc9f504a40`,
  `lbracket f55107b116907e53`, `hexprismbore 35ea82350ca904b4`.
- **The CLI end-to-end**, checked by hand on `dualc_field`:
  `--expr "sphere(radius=0.4)" --bounds 10,10,10,11,11,11` now prints the
  empty-contour warning and reports `(3 verts, 1 faces)` — before this change
  the same command wrote that file with no warning at all. The same expression
  with default bounds prints **no** warning (7,980 faces), so the channel is
  not merely always-on noise.
- **C ABI:** `capi/dualc_c_demo.c` now contours twice — through
  `dualc_field_contour` and through the `_with_diagnostics` twin — and fails
  unless every position is bit-identical and the report agrees with the mesh
  it describes. `cli_c_abi`, `cli_c_abi_mesh` and `cli_c_abi_mesh_inmem` pass.

## Not covered by a test

`inputEmpty`'s **true** branch is reported but unexercised: geometry-central
will not construct a `SurfaceMesh` from an empty polygon list, so a
zero-triangle `MeshSource` cannot be built through the public API. What is
pinned instead is the wiring — `inputEmpty` is exactly
`MeshSource::triangleCount() == 0`, and the edge fields are exactly the other
two accessors, asserted on a known-good mesh. Stated here rather than left to
look verified.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
