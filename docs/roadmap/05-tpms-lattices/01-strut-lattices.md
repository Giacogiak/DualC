# #17 Strut-based lattices (SC, BCC, FCC, octet truss)

Part of [05 — TPMS lattices](README.md). The core strut-lattice item: why field-graph
nodes rather than a CLI, the oracle, the parity gate, and the decision record. Usage:
[command_reference/11 § Strut
lattices](../../command_reference/11-dualc_field/02-strut-lattices.md).

## 17. Strut-based lattices (SC, BCC, FCC, octet truss)
**DONE (2026-07-09).**


The follow-up to #16. Where TPMS lattices are smooth periodic surfaces, strut
lattices are wireframes -- a unit cell of capsules at the nodes and edges of a 3D
crystal, tiled across the interior. Un-deferred 2026-07-06 on a concrete request
(the deferral's re-trigger condition); `--mem` auto-budget had just landed, so
struts was the next open engine item.

**Scope decision (differs from the original DEFERRED note).** The original plan
called for a standalone `dualc_lattice`-style CLI. Since then the field-graph
keystone ([12](../12-field-graph-and-app/README.md)) made the *node* the unit of reuse, so
struts ship instead as a **first-class field-graph source node** -- available in
`dualc_field` (contour/export) **and** `dualc_field_view` (live GLSL preview),
wired to `--expr` exactly like TPMS. No standalone CLI, no Polyscope surface. This
is strictly more useful: struts compose with every boolean/decorator/mesh-clip at
the field level and preview == export for free.

**Crystals.** `sc` (simple cubic, 3 axis rods), `bcc` (body-centered, 8
centre→corner diagonals), `fcc` (face-centered, the 6 face "X"s = 24 half-struts),
`octet` (octet truss = fcc + the 12 octahedral edges = 36 struts). Params:
`center`=[0,0,0], `wavelength`=1 (cell side), `radius`=0.1 (metric strut half-
thickness). The field is a true Lipschitz-1 SDF (a `unionOf` of exact
`CapsuleField`s), so -- unlike TPMS -- it needs **no** `normalize` before a metric
`onion`/smooth boolean, and it inherits the conservative `cellOverlaps` (the
sparse-octree speedup applies; no always-refine hack). Extent is infinite (like
TPMS / `repeated`), so it must be clipped: `intersection(mesh(…), bcc(…))`,
`intersection(box(…), bcc(…))`, or a bare `--bounds` sample window (open ends).

**Implementation (branch `feat/strut-lattices`).** Everything reuses existing
pieces -- no new geometry kernel:

- `strutKinds()` / `strutCellSegments(kind, wavelength)` / `makeStrutLattice(kind,
  center, wavelength, radius)` in `examples/example_common.{h,cpp}`, parallel to
  `tpmsKinds()` / `makeTpmsField`. `strutCellSegments` is the single source of
  truth for the crystal geometry; `makeStrutLattice` unions a `CapsuleField` per
  segment, tiles with `repeated`, and translates by `center`.
- `isStrutOp` + a `buildField` branch in `examples/field_graph.cpp` (parallel to
  the `isTpmsOp` branch); a `--list` row in `dualc_field.cpp`.
- GLSL: `emitStrut` + `isStrutOp` in `examples/field_glsl.cpp`, reusing the
  `sdCapsule` helper and the same single round-fold (`dcRoundTA`) as
  `RepeatField`; the unit-cell coords scale with the `wavelength` uniform, so all
  three params stay live-editable in `dualc_field_view`.

**Tiling correctness (the crux).** The tiled field uses `RepeatField`'s single
round-fold (evaluates only the nearest tile). The failure mode is a *dropped*
boundary strut -- and the manifold test cannot catch it (each capsule is
independently closed, so a sparser lattice is still watertight). The gate is
therefore a value-level **oracle**: `test_strut_lattice.cpp` tiles the same
segments *independently* over a wide neighbour range, takes the exact min, and
asserts it equals `makeStrutLattice`'s value at 400 random points per crystal.
Result: single round-fold is **exact** for all four crystals -- `min` is
idempotent (shared boundary struts are not double-counted) and the centred /
face-replicated cells always present the nearest strut in the home tile -- so no
`repeatedLimited`/neighbour-min is needed.

**Tests (all green).** `test_strut_lattice.cpp` (oracle + segment counts + a
manifold end-to-end for bcc/octet clipped to a cube); a GL-free codegen test in
`test_field_glsl.cpp` (segment counts 3/8/24/36); the `cli_field_strut` CTest
smoke; GPU `dualc_glsl_parity` **66/66** (added sc/bcc/fcc/octet at ~1e-8 error).
Full `ctest` 197/197.

**Follow-on enhancements (Phases 3-5):** **Phase 3 (graded strut radius) DONE
2026-07-07** (`graded-offset`); **Phase 4 (tapered struts) DONE 2026-07-09**
(`nodeRadius`); **Phase 5 (crystal blending) DONE 2026-07-09** -- compound/hard-seam
were docs-only, the smooth-morph `mix` node is now implemented -- see
[§17b](02-strut-enhancements.md#17b-strut-lattice-enhancements-phases-3-5).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
