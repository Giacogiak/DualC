# TPMS lattices

Triply-periodic-minimal-surface lattice infill (`dualc_lattice`), the
`normalizedOf` metric-thickness fix, and the deferred follow-ups (strut lattices,
a tight Lipschitz `cellOverlaps` for TPMS). The cross-cutting effort to *visualize
and manufacture* dense lattices has its own page,
[11-dense-lattice-deliverable/](../11-dense-lattice-deliverable/README.md); the
per-command flag/recipe catalogue is
[../command_reference/06-dualc_lattice.md](../../command_reference/06-dualc_lattice.md).
Composing a lattice *beyond* a single clipped solid — isolating its **open**
surface for inspection, then thicken → skin → union as one field-level contour — is
the field-graph workflow in
[12-field-graph-and-app/ §
F](../12-field-graph-and-app/05-open-surface.md#f-isolating-the-open-lattice-surface--validated-workflow--deferred-clip-mask).

## The pages of this topic

Split on 2026-09-11 from the single frozen file (the record is verbatim; only status
moved out of headings). Item numbers (#16, #17, #17b, #18, #20) are the stable
addresses; the three short TPMS items stay on this page.

| Page | Items |
| --- | --- |
| [Strut lattices](01-strut-lattices.md) | [#17](01-strut-lattices.md#17-strut-based-lattices-sc-bcc-fcc-octet-truss) — the SC/BCC/FCC/octet field-graph source nodes |
| [Strut enhancements](02-strut-enhancements.md) | [#17b](02-strut-enhancements.md#17b-strut-lattice-enhancements-phases-3-5) — [the wiring template](02-strut-enhancements.md#the-template-every-phase-follows-how-the-17-core-was-wired), [Phase 3 graded radius](02-strut-enhancements.md#phase-3----graded-strut-radius), [Phase 4 tapered struts](02-strut-enhancements.md#phase-4----tapered-struts) |
| [Crystal blending](03-crystal-blending.md) | [#17b Phase 5](03-crystal-blending.md#phase-5----crystal-blending) — the `mix` morph node; [closing the feature](03-crystal-blending.md#closing-the-feature) |

## 16. TPMS lattices on input meshes
**DONE (2026-05-26).**


New `dualc_lattice` CLI takes a closed input mesh and fills its interior with a triply periodic minimal surface (Gyroid, Schwarz P, Schwarz D / Diamond, Fischer-Koch S, Lidinoid, Neovius). The lattice is composed as `intersectionOf(tpms_or_onion, MeshSource)` -- with `--offset T` the lattice is wrapped in the existing `onionOf` decorator, producing the closed double-sheet thick-wall shell `|F|−T` clipped to the mesh; without `--offset` the bare 0-isosurface of the lattice clipped to the mesh is the boundary of a solid phase. Both modes emit a closed manifold (the test `End-to-end: gyroid + onion clipped to a unit cube is manifold` asserts 0 boundary edges). Six new `PrimitiveField` subclasses live in `src/implicit/primitives_tpms.cpp` (~150 LOC of trig formulas, conventions cross-checked against PicoGK -- Apache-2.0, LEAP 71, reference only). A new `normalizedOf(field)` decorator in `src/implicit/decorators.cpp` returns `f / max(|grad f|, eps)` so `--normalize-thickness` makes `T` close to the metric wall thickness (TPMS values are trigonometric, not signed distances). One non-obvious fix landed alongside: `OnionField::cellOverlaps` was using the default Lipschitz-1 conservative test on `|f|-t`, which under-refines on high-frequency non-SDF children (TPMS); it now delegates to `child_->cellOverlaps(expandBox(cell, t))`, propagating any non-SDF child's refinement decision through the onion. Per-TPMS `cellOverlaps` returns true unconditionally because TPMS surfaces densely fill space. 8 new TPMS tests + 1 manifold end-to-end test, all green; the `cli_lattice` smoke test runs the gyroid case at depth 5.

*(2026-09-20: moved here from `command_reference/06-dualc_lattice.md`, whose usage page keeps the volume-fraction table and the when-to-reach-for-each advice; the derivation is the record's.)*

**Wavelength λ** is the *period* of the trig function — the side of one
unit cell. Bigger λ → bigger cells, fewer cells across the model. But the
surface is just rescaled: same shape, same topology, same volume fraction.
The bare lattice (T = 0) fills approximately **50% of the volume**
regardless of λ, because it is the boundary of one phase of the periodic
medial surface, and that phase is half of space by symmetry.

**Offset T** changes the *topology*: from one sheet (separating solid from
void) into a thin double-walled shell hugging the medial surface on both
sides. The bare lattice contours the level set `F = 0`. The offset version
contours `|F| - T = 0`, which is the pair of nearby level sets `F = +T`
and `F = -T` glued into a tube. With T small you get a wireframe-like
cellular structure (volume fraction well below 50%); with T large enough,
adjacent walls meet and you are back to a solid block.

**Why λ alone cannot reproduce the offset effect.** The bare lattice is
locked at ~50% volume fraction. No matter how large you grow λ, the result
is always a 50%-filled solid — just with fewer, larger cells. The offset
operation is what unlocks the volume fraction axis at all, going from 50%
down to wireframe-thin (`T/λ → 0`) or up to solid (`T/λ → 0.5`).

**Third axis (not exposed in `dualc_lattice` today): the Quilez offset
shift** `F − r`. This is *different* from `--offset T` — it produces a
single shifted sheet (the gyroid's solid phase grown or shrunk), not a
double-walled tube. `dualc_primitive` has no TPMS entries, so the shift is
reached through the field-graph: `dualc_field --expr 'offset(gyroid(wavelength=…),r=…)'`
([11-dualc_field/](../../command_reference/11-dualc_field/README.md)); it is not wired into the lattice
CLI. Reach for it if you want a tighter / looser single sheet without doubling.

**Field composition pipeline.** The CLI builds the following expression
tree before dual-contouring:

```
F_tpms       = <one of the 6 formulas above>
F_normalized = F_tpms / max(|grad F_tpms|, ε)   # only with --normalize-thickness
F_lattice    = |F_tpms_or_normalized| − T       # only when --offset T > 0
F_clipped    = max(F_lattice_or_tpms, F_mesh)   # intersect with input volume
output       = dualContour(F_clipped, --bounds, --depth)
```

Reuses the existing v2 implicit-field operators: `normalizedOf` (the
gradient-magnitude normalisation), `onionOf` (the `|F| − T` decorator) and
`intersectionOf` (the `max(a, b)` boolean) — see `include/dualc/implicit.h`.

## 20. `normalizedOf` gradient-magnitude fix (`--normalize-thickness` was a silent no-op)
**DONE (2026-06-01).**


Found while certifying the TPMS generator with `dualc_view` / `dualc_lattice`: a thin-wall gyroid offset (`--wavelength 0.5 --offset 0.05 --depth 6`) produced a multitude of disconnected thickened-surface fragments, and `--normalize-thickness` did nothing to fix it. **Root cause.** `NormalizedField::valueAt` (`src/implicit/decorators.cpp`) computed `f / max(child->gradientAt(p).norm(), eps)`, but `PrimitiveField::gradientAt` (`include/dualc/primitives.h`) returns a *unit-normalized* central difference — it discards `|grad f|` and hands back a direction (correct for SDF primitives and for the contourer's edge normals, which only need a direction). So the divisor was always ≈ 1.0 and `normalizedOf(tpms)` was an identity. The decorator the #16 entry introduced "so `--normalize-thickness` makes `T` close to the metric wall thickness" never actually rescaled anything. **Why it manifested as fragments.** At `wavelength = 0.5`, `k = 2π/λ ≈ 12.6`, so a *raw* offset band `|F| < 0.05` is only ≈ `2·0.05/k ≈ 0.008` thick metrically — well below a depth-6 leaf (`1/64 ≈ 0.016`). Sub-cell walls cannot be closed by the contour, so the shell breaks into open shards. Raising depth to 7 barely helped (still open); the real rescue is metric normalization, which was broken. **Diagnosis method (kept for reference).** F/V ratio certifies closure: a closed triangle manifold has F ≈ 2V. Bare lattice (`--offset 0`) and well-resolved thick offsets (0.3, 0.5) all measured **F/V = 2.00** → the generator core was never at fault; only the thin-wall + broken-normalize combination measured F/V ≈ 1.6 (open). **Fix.** `NormalizedField` now computes its **own** true `|grad f|` via a central difference on `child_->valueAt` (step `1e-4`, matching `PrimitiveField`), independent of the child's possibly-unit gradient; `PrimitiveField` left untouched. Also added `NormalizedField::cellOverlaps` forwarding to the child, so a high-frequency child's always-refine decision (TPMS `cellOverlaps = true`) propagates through the normalized wrapper instead of falling back to the base Lipschitz-1 test (this was the source of the earlier tiny byte differences between raw and "normalized" output). **Verification.** Same thin-wall config after the fix: `--normalize-thickness` at depth 6 now yields **68820 V / 137844 F (F/V = 2.003, watertight)** vs the pre-fix 43964 V / 69684 F (F/V = 1.585, fragmented); closes cleanly even at depth 5 (F/V = 2.013). Two new tests in `tests/test_tpms.cpp`: one pins `normalized == raw / |grad f|` and guards against the unit-gradient regression (the old `[normalize]` test only checked sign/zero, which the no-op passed); one end-to-end asserts the sub-cell thin-wall shell contours to **0 boundary edges** (fails pre-fix). Full suite green (135 cases / 2135 assertions). **Doc nuance not yet applied.** With a first-order-SDF onion `|d| − T` the metric wall is ≈ `2T`, so [`docs/command_reference/06-dualc_lattice.md`](../../command_reference/06-dualc_lattice.md)'s "`T` ≈ true wall thickness" is right in spirit but ~2× off; left as a known wording tweak pending a decision. *(2026-09-18: applied since — that page's offset table states `|F| − T` and the `2·T/λ` solid fraction, [12 § F](../12-field-graph-and-app/05-open-surface.md#validated-thin-shell-workflow-no-new-code) states wall ≈ 2·thickness, and the resolution rule is [design/10](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist).)*

## 18. Tight Lipschitz `cellOverlaps` for TPMS primitives
**DEFERRED (2026-05-31)** — trigger in the body below.


Performance optimization for `dualc_lattice`. **Current state.** Every TPMS in `src/implicit/primitives_tpms.cpp` returns `cellOverlaps = true` unconditionally because the trig-valued formulas are not Lipschitz-1 in metric units, so the base-class conservative test would silently drop cells the high-frequency surface passes through. Result: inside the input-mesh AABB the sampler octree is effectively dense (every cell down to `maxDepth` is built), and the v2 sparse-octree speedup (work scales with surface area, not volume) does not apply to the TPMS subtree. The mesh-clip via `intersectionOf(tpms, MeshSource)` still prunes everything outside the mesh, and the `OnionField::cellOverlaps` fix already forwards the always-true decision through the onion shell correctly. **Proposed change.** Each TPMS has a closed-form Lipschitz bound `|grad F| <= L` with `L = sqrt(n) * k` where `k = 2*pi / wavelength` and `n` is the term count (Gyroid n=3, Schwarz-P n=3, Diamond n=4, Fischer-Koch S n=3 with the 2k terms contributing 2x weight, Lidinoid n=6 with similar weighting, Neovius n=4). Override `cellOverlaps(cell)` per class to evaluate `F` at the 8 corners and return `true` iff `min|F(corner)| <= L * (cellDiagonal / 2)`. Constant time per cell (the sampler already needs the corner evaluations anyway for sign testing in the leaf path -- could be hoisted/shared). **Expected speedup.** Roughly `wavelength / cellSize` away from the surface band: at depth 7 across the mesh AABB with wavelength = box/10, cells across one wavelength ~12.8, so ~12x. Larger for thin walls (small `--offset T`) where the active band is narrower; smaller for fine wavelengths or shallow depth. **Risk.** A too-tight bound silently drops features. Mitigation: the existing end-to-end manifold test in `tests/test_tpms.cpp` (gyroid + onion clipped to a unit cube, asserts 0 boundary edges) catches under-refinement; add a second test at higher wavelength / lower depth where the bound is least forgiving. Derive each `L` from the gradient bound (chain rule on `sum |grad term_i|` summed over the terms), unit-test the bound is conservative on a dense Monte Carlo grid before trusting it in the sampler. **Why deferred.** No active performance complaint -- current TPMS contouring at depth 7 finishes in seconds. Trigger to revisit: a use case driving `dualc_lattice` at depth 8+ on a large mesh AABB, or a benchmark that puts TPMS in the regression suite (Tier 3 #11).

---

← Back to the [Roadmap index](../README.md).
