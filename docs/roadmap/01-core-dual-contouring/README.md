# Core dual contouring — the DC engine

The v1 engine that powers `dualc_demo`: the sampler → contourer pipeline, the
QEF/octree internals, the sign oracles, manifold DC, adaptive collapse,
multi-threading, and the catalogue of bugs fixed along the way.

## The pages of this topic

Split on 2026-09-11 from the single file (the record is verbatim; section numbers
§ 2–§ 7 are the stable addresses). The item dates below were reconstructed from git on
2026-09-20 (the items were closed without one).

| Page | Sections |
| --- | --- |
| [Engineering record](01-engineering-record.md) | [§ 2 decisions](01-engineering-record.md#2-architectural-decisions-locked-in-early-still-hold) · [§ 3 pipeline](01-engineering-record.md#3-pipeline-as-it-stands-today) · [§ 5 quality bars](01-engineering-record.md#5-current-quality-bars-depth-7-unless-stated) · [§ 7 conventions](01-engineering-record.md#7-conventions-worth-knowing-before-you-touch-any-dc-code) |
| [Bug catalogue](02-bug-catalogue.md) | [§ 4](02-bug-catalogue.md#4-significant-bugs-and-their-fixes): § 4.1–4.5 the v1/v2 defects · [§ 4.9 the 2026-08-21 code-screening batch](02-bug-catalogue.md#49-code-screening-batch) |

## Roadmap items

### Tier 1 — high leverage, do these next

#### 1. Adaptive cell collapse (Ju/Schaefer/Warren)
**DONE (2026-04-30, `4d72221`).** ~500-800 LOC, the big v1.1
lift the plan already mentions. Bottom-up: for each octant whose 8 children are leaves,
merge their QefData (additive A^TA / A^Tb / btb) and solve at the parent scale. Collapse
iff (a) merged QEF residual ≤ simplificationError AND (b) the topology-safe test passes
(the merged 8-corner sign config is consistent with the children's edge sign-changes —
no new connected components introduced). Wires up the existing
ContourerParams::simplificationError field that's currently parsed-but-ignored. Effect
on molde at depth 7: probably 3-5× fewer triangles for the same visual fidelity. More
importantly, lets you crank --depth to 9 or 10 without 1.5 GB outputs — adaptive
collapse keeps the cell count bounded by surface complexity, not by the maxDepth grid.

#### 2. Sharp-feature toggle on the sampler
**DONE (2026-05-15, `5c16aea` — `interpolateNormals` first in `sampler.h`).** Small, ~20 LOC. Add
SamplerParams::interpolateNormals = true. When false, MeshBVH::segmentFirstHit returns
the face normal instead of the barycentric blend. With this off, the cube reverts to
perfectly sharp corners; with it on (default), molde stays smooth. Lets DualC serve both
organic and CAD use cases without recompiling.

#### 3. Multi-threading the sampler + contourer
**DONE (2026-05-16, `00abec4` — `parallel.h` added; marked done `bbae479`, 2026-05-21).** `src/internal/parallel.h`
provides `parallelFor(n, threads, fn)` (atomic-counter dynamic scheduling) +
`resolveThreadCount(0 = all hw threads)`. Wired into: the sampler's octree build
(serially refine to depth 3 to collect a frontier, then `parallelFor` over frontier
subtrees — see `src/sampler.cpp`); the contourer's leaf-QEF pre-solve (`parallelFor`
over every leaf into `leafSolveCache` before `cellProc`, see `src/contourer.cpp`); both
narrow-band bakers (`MeshSource::bakeToGrid` band detection + exact-SDF passes; free
`bakeToGrid` z-slab sampling). Deterministic output regardless of thread count. Knob:
`SamplerParams::numThreads = 0`.

### Tier 2 — meaningful improvements when you hit specific cases

#### 4. Manifold dual contouring (multi-vertex-per-cell)
**DONE (2026-05-21, `6f6d48b`).** Place one QEF vertex per
surface component in a cube instead of one per cell. Partition the 12 cube edges into
components via face-adjacency union-find (saddle faces paired by the inside-corner
rule); solve a separate QEF per component; route `emitQuadAtEdge` per local edge ->
component. On molde at depth 7: the 3 intrinsic non-manifold edges drop to 0 (with or
without `--collapse`), face count unchanged, +3 vertices (one extra per pinch). Cube and
existing booleans unchanged (single-component cells everywhere). Default-on with
`--no-manifold` opt-out for the pre-MDC reference; opt-out output is byte-equivalent to
the prior baseline (3 non-manifold edges preserved). Saddle disambiguation uses the
fixed inside-pair rule; the asymptotic decider with corner SDF values is a future <=80
LOC follow-up if a real saddle case needs it.

#### 5. Closest-point query on the BVH
**DONE (2026-05-22, `669652c`).** The branch-and-bound
`MeshBVH::closestPoint` traversal was already on main (it backs
`MeshSource::valueAt`/`gradientAt`); this item wired up the two callers it was meant to
unlock. (a) `MeshBVH::closestPointWithPseudoNormal` returns the Bærentzen-Aanæs
angle-weighted feature normal — face normal in a face region, the two-face average on a
manifold edge, the angle-weighted incident-face sum at a vertex — from topology +
pseudonormal LUTs built once at construction. `SignOracle` now dispatches on its
`SignMethod`: PSEUDONORMAL classifies inside/outside with a single closest-point query
(`dot(p-cp, n) < 0`), WINDING_NUMBER keeps the 3-ray parity vote; `SignMethod` is
threaded through `MeshSource` and the sampler, with a `--pseudonormal` opt-in flag on
`dualc_demo`. (b) New `ImplicitField::closestSurfacePoint` virtual (default = one Newton
step; `MeshSource` overrides with the BVH) replaces the Hermite-edge `(midpoint,
zeroNormal)` fallback when `segmentFirstHit` misses a known crossing. PSEUDONORMAL
output is vert/face-identical to WINDING_NUMBER on watertight cube/molde. Saddle-region
pseudonormals degrade to face normal on non-manifold / boundary edges — documented as
watertight-input-only. The asymptotic decider is not needed here.

#### 6. Generalized winding number with hierarchical acceleration
**DONE (2026-05-22, `084a19c` + `f06d60f`).** `MeshBVH`
gained exact `windingNumber` (O(N) Van Oosterom-Strackee solid-angle sum — the
ground-truth oracle) and hierarchical `windingNumberFast` (per-BVH-node first-order
multipole aggregates, descend-when-near traversal, O(log N) average), both behind an
opt-in `buildWindingTree` ctor flag. New `SignMethod::GENERALIZED_WINDING_NUMBER`,
threaded through `SignOracle` and `MeshSource`, classifies inside/outside as `w > 0.5` —
robust on nearly-closed input where 3-ray parity coin-flips. New `WindingNumberField`
(`src/implicit/winding_field.cpp`) exposes GWN as an `ImplicitField`: its 0.5-isosurface
seals open shells, triangle soup and self-intersecting input into one watertight solid.
Edge crossings come from the BVH where geometry exists (the GWN field is a near-step
there, so a finite-difference gradient would be meaningless) and from a root-find on the
smooth GWN cap elsewhere. `dualc_demo` gained `--gwn` (GWN sign method on a mesh) and
`--gwn-field` (contour the GWN field directly). 10 GWN test cases, including a
jointly-closed test — two disconnected open shells in one mesh that together bound a
volume contour into a single watertight solid. Hierarchical accuracy is ~1e-2
(first-order expansion, beta=2), exact near the surface via descend-when-near, so the
0.5 threshold and contouring are unaffected. A planned multi-mesh "soup source" CLI was
dropped by design: a multi-piece soup is just one mesh (GWN is connectivity-agnostic),
and mesh I/O / spatial transforms belong to the host application embedding DualC, not
the library.

### Tier 4 — open algorithmic extensions for special needs

#### 13. Intersection-free contouring (Tao Ju 2006)
Guarantees no self-intersections in
the output. Needed only for CSG / boolean pipelines that can't tolerate them.

#### 14. Multi-material / open-boundary contouring
Different surface labels share the
input; contour all boundaries simultaneously. Useful for medical / multi-tissue
applications.

> **Note — isolating an *open* TPMS/lattice surface.** A common request is to see a
> TPMS lattice's surface *open* inside its clip volume (not welded to the volume's
> faces). The welded "skin" is the closed-only nature of DC, working as intended.
> For **visualization** this is met *without* core-DC work: a raymarch preview of a
> thin shell (`intersection(onion(normalize(tpms)), volume)`), plus an optional
> deferred render-time **clip mask** — a **viewer** feature, **not** `#13`/`#14`
> (see [roadmap/12 § F](../12-field-graph-and-app/05-open-surface.md#f-isolating-the-open-lattice-surface--validated-workflow--deferred-clip-mask)).
> `#13` (intersection-free) is unrelated — field-level booleans are already
> non-self-intersecting. `#14` is the right home only if the open surface is ever
> needed **as a mesh**: realize it by tagging each Hermite edge crossing with the
> source/material it came from (an `int` label forwarded through combinators to the
> active operand) and suppressing / splitting output faces by that label — the
> simplify/collapse pass must propagate the tag.

#### 15. GPU acceleration
Fast_dual_contouring-style; only worth it if you're processing batches.

The findings § 4.9's audit raised and that batch did **not** close are tracked in [17 —
Code audit & engine hardening](../17-code-audit-and-hardening/README.md) (items
#22–#35).

---

← Back to the [Roadmap index](../README.md).
