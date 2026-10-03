# Findings ledger — engine (study A2–A5)

The 33 findings the [2026-08-19 audit](../../raw/study/README.md) raises in its algorithm
documents: **A2** (sampling, octree, BVH, sign oracles), **A3** (the QEF), **A4** (the
contouring recursion) and **A5** (the implicit-field algebra). The craft half is in
[`02-findings-ledger-craft.md`](02-findings-ledger-craft.md) (B1–B5) and
[`06-findings-ledger-testing.md`](06-findings-ledger-testing.md) (T).

Each row is the audit's claim, its citation, and a disposition. **Nothing here is an
independently verified defect** unless the [topic README](README.md) says so; the citation
is the evidence, and the audit's reasoning is not restated.

**Dispositions.** `→ #NN` promoted to a tracked item ([#22–#26](03-correctness-and-robustness/README.md),
[#27–#35](04-engineering-quality.md)) ·
`ACCEPTED` a deliberate trade-off or an honest limitation, recorded so no future thread
re-litigates it · `MINOR-OPEN` real but below the tracking bar. The five defects and two
coverage gaps the 2026-08-21 screening batch closed are **not** listed in any of the three ledgers —
they are recorded in
[01 § 4.9](../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch).

## A2 — Sampling, octree, BVH, sign oracles

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| E1 | Root-box padding is isotropic from the largest extent, so a 100×1×1 plate gets 128 divisions along its length and ~13 through its thickness at `maxDepth=7`. Sound, but the resolution consequence is undocumented. | `A2:36-50` | MINOR-OPEN — a documentation fix |
| E2 | Padding is applied twice on the GWN path (`WindingNumberField::bounds()` pads 10%, then the sampler pads again) while `MeshSource::bounds()` does not pad at all. | `A2:52` | MINOR-OPEN — two documented but inconsistent conventions |
| E3 | Refinement is purely geometric, with no curvature or error term — the biggest algorithmic difference from Ju/Schaefer/Warren. | `A2:72-82` | ACCEPTED — deliberate; the adaptivity is moved downstream into the collapse pass, which is now tested |
| E4 | The miss-fallback's weak rung substitutes `closestSurfacePoint(midpoint)` with no distance sanity check, so a wrong sign-oracle answer can feed the QEF a full-weight plane constraint a whole cell away. | `A2:184-188` | MINOR-OPEN — one-line fix (reject beyond the cell diagonal) |
| E5 | `interpolateNormals` is a blunt global switch: the mesh-path default `true` rounds every CAD corner by ~1 cell at *every* resolution, so halving `h` does not fix it. The correct answer is per-edge, dihedral-angle-thresholded, using adjacency `buildPseudoNormalTopology` already builds. | `A2:192-211` | MINOR-OPEN — the fix is recorded here rather than tracked; raise it if CAD re-meshing becomes a use case |
| E6 | `countRayHits`' advance epsilon is relative to hit distance, not feature size, and all three probe rays fail together; plus a hard-coded 4096-hit cap that truncates silently. | `A2:239-251` | → **#23** |
| E7 | The default sign method is also the slowest — `WINDING_NUMBER` 3-ray parity is O(3k·log T) against O(log T) for pseudonormal, 3-10× slower. | `A2:221-237` | MINOR-OPEN — changing a default is a behaviour change, not a bug fix |
| E8 | `buildPseudoNormalTopology` computes whether the mesh is watertight and discards it, runs unconditionally, and carries a stale memory estimate ("a few MB"; actually ~1.5 GB at 10⁷ triangles). | `A2:266` | → **#26** (the discarded watertightness is the Diagnostics channel's first payload) |
| E9 | The multipole winding-tree far-field test mixes two centres — `d²` from the area-weighted centroid, `r²` from the AABB half-diagonal — so the stated β=2 bound is not the one enforced; it passes only because the 0.5 threshold has a large margin. β is also not exposed via `SamplerParams`. | `A2:319-335` | MINOR-OPEN |
| E10 | `findClosest` does not distance-order BVH children, missing a 1.5-3× traversal speedup. | `A2:317` | → **#27** |
| E11 | The narrow-band bake's chamfer distance over-estimates by 2-4% and is not a metric; nothing verifies the band is actually closed (it relies on the 1.5-cell floor), and the serial O(res³) band-index scan is the routine's Amdahl floor. | `A2:379-383` | ACCEPTED — a documented precondition written for octree pruning, where over-estimating is the safe direction. Its preview consequence is the deferred conservative baked far field, [12/07 § H](../12-field-graph-and-app/07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep) |
| E12 | `GridField::gradientAt` returns a normalised direction that is discontinuous across cell boundaries, so consumers assuming a true gradient magnitude implicitly assume Lipschitz-1. | `A2:385-391` | → **#34** |

## A3 — The QEF

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| E13 | `tryCollapse` re-accumulates up to 96 raw samples into a fresh solver instead of using `QefData`'s O(1) additive merge. | `A3:55-57`, `A4:309` | MINOR-OPEN — a missed simplification rather than a bug |
| E14 | `clampVertexToCell` / `clampToleranceCells` have no test coverage, despite being the mechanism that stops near-degenerate QEFs spraying vertices outside their cells. | `A3:218` | → **#32** |

## A4 — The contouring recursion

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| E15 | `emitQuadAtEdge`'s dedupe is global though its comment says "adjacent". One configuration (a two-component pseudo-leaf filling all four slots as `[A,B,A,B]`) would give `n == 2` and wrongly early-return. | `A4:150` | MINOR-OPEN — the audit could not construct a reaching configuration and will not claim it is unreachable; recorded so nobody re-derives it |
| E16 | Saddle-face disambiguation uses the fixed inside-pair heuristic, not Schaefer's exact asymptotic decider. | `A4:231-248` | ACCEPTED — architectural: `HermiteLeafData` stores signs, not corner values, so the mesh and field paths can share one contourer. The exact decider is costed at ≤80 LOC + 8 doubles/leaf if a real saddle case ever needs it |
| E17 | The vertex-index compaction pass is dead code; the code's own comment admits it should never fire given lazy allocation. | `A4:350` | ACCEPTED — kept as defence in depth |
| E18 | The empty-output fallback synthesises a placeholder triangle rather than signalling emptiness, because geometry-central rejects an empty polygon list — so "no surface here" is indistinguishable from "one triangle", and there is no error channel out of `contourHermiteOctree`. A stale test comment calls this a "v0 stub". | `A4:352-362` | → **#26** |

## A5 — The implicit-field algebra

| ID | Finding | Source | Disposition |
| --- | --- | --- | --- |
| E19 | Exactness and Lipschitz-ness are prose-only — no `isExact()` / `lipschitzConstant()` accessor on the interface. Named as the root cause of the whole opt-out-forwarding bug class. | `A5:64-66`, `A5:111` | → **#25** |
| E20 | The central-difference epsilon `1e-4` is absolute and hard-coded in five files (`primitives.h:28`, `implicit2d.h:50`, `decorators.cpp:313`, `domain_ops.cpp:17`, `lift.cpp:14`), which breaks at micron scale in a library documented as 1 unit = 1 mm. | `A5:264-266` | → **#24** |
| E21 | `RepeatField` uses the naive single-tile fold — wrong when the child pokes past a tile boundary — while `RepeatLimitedField` in the same file uses the corrected 8-neighbour minimum. | `A5:239-245` | → **#34** |
| E22 | `twisted` / `bent` use a 6-call finite-difference gradient although both have closed-form Jacobians (~10 lines each); the cost compounds to 6ⁿ for nested distorted operators with no memoisation. Only `displaced`, with its opaque `std::function`, genuinely needs the finite difference. | `A5:247-262` | → **#27** |
| E23 | `mixOf`'s `cellOverlaps` returns `true` unconditionally, killing adaptivity under the morph node. | `A5:190-194` | ACCEPTED — backed by an impossibility argument: a lerp surface can float free of both operands' zero sets, so no cheap tight bound exists. Already recorded with its measured consequence at [05 #17b](../05-tpms-lattices/02-strut-enhancements.md#17b-strut-lattice-enhancements-phases-3-5) |
| E24 | `gradedOnion` / `gradedOffset` drop the ∇t term from their gradient. | `A5:204`, `A5:215` | ACCEPTED — `valueAt` carries the full `t(p)`, so surface *positions* stay exact through `edgeHit`; only the normal is approximate, and only inside the grading band |
| E25 | The smooth-boolean blend radius `k` is an absolute world-unit distance rather than scale-invariant. | `A5:146` | ACCEPTED — deliberate; a relative parameterisation is less predictable for manufacturing |
| E26 | The smooth-boolean gradient lerp is exact only for true unit-gradient SDF operands, and degrades when blending a non-SDF field such as a raw TPMS. | `A5:180` | ACCEPTED — surface position is unaffected; only the normal tilts |
| E27 | `SmoothUnionField`'s bbox padding is `k`, four times the `k/4` bulge its own comment and `cellOverlaps` both enforce. | `A5:368`, `A5:378` | MINOR-OPEN — harmless but internally inconsistent |
| E28 | `DisplaceField::bounds()` returns the child's bounds unchanged, known-wrong when the bump pushes the surface outward; the header documents it rather than fixing it. | `A5:376`, `A5:380` | MINOR-OPEN — the proposed fix is an optional amplitude parameter on the factory |
| E29 | A default-constructed `BBox{}` passes `isValid()`, so `bboxUnion`'s empty-box guard never fires for it and the origin is silently dragged into unioned bounds; `RevolveField::bounds` on an invalid profile is affected too. | `A5:382` | → **#34** |
| E30 | There are no 2D boolean combinators — `ImplicitField2D` has no union or difference, and `Polygon2D` is the only composite profile. | `A5:355` | MINOR-OPEN — a ~60-line feature gap, not a defect |
| E31 | Lift axis conventions are inconsistent: `RevolveField` spins about y, `ExtrudeField` sweeps along z. | `A5:355` | ACCEPTED — documented; changing it would break every existing graph |
| E32 | `EllipsoidField` is a tight lower bound, not an exact SDF — no closed form exists without a sextic root or Newton iteration. | `A5:60` | ACCEPTED — documented, sign-correct and Lipschitz-1-safe |
| E33 | `TriangleField` / `QuadField` are unsigned distance only, so feeding one directly to the sampler yields an empty octree. | `A5:62` | ACCEPTED — flagged loudly in the header |

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
