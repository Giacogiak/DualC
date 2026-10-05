# Invariants and tolerances

What the engine guarantees about its output, the rules of thumb that follow from how it
samples, and every numeric constant that decides a behaviour — each stated once, with the
code that enforces it and the page that explains the mechanism. A recipe that depends on one
of these links here rather than restating the number.

## Output invariants

- **A closed input at adequate depth contours to a watertight manifold of the same genus.**
  Watertight means 0 boundary edges and 0 non-manifold edges; "same genus" is the Euler
  characteristic `χ = V − E + F = 2 − 2g` of the output matching the input's. Both are
  integers, so the tests pin them exactly (`topologyOf` in `tests/test_demo_meshes.cpp`, the
  quality-bar table in
  [01 § 5](../roadmap/01-core-dual-contouring/01-engineering-record.md#5-current-quality-bars-depth-7-unless-stated)).
  Manifold DC is what makes the non-manifold count 0 on inputs with pinch cells
  ([§ 4.5](04-qef-manifold-collapse.md#45-manifold-dual-contouring)); "adequate depth" is the
  resolution rule below. The counts are reported on every run through
  `Diagnostics::outputBoundaryEdges` / `outputNonManifoldEdges` / `outputWatertight`, and the
  CLIs print a warning when the mesh is not closed
  ([17/05](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md)).
- **The output is bit-identical regardless of the thread count.** Every worker owns a
  disjoint subtree and no result depends on scheduling
  ([§ 3.2](01-sampler.md#32-parallel-octree-build)). A consequence worth knowing when adding
  a diagnostic: nothing on the field may count per-probe events from worker threads, because
  the count would be scheduling-dependent.
- **The mesh path is the field path.** `dualContourMesh` builds a `MeshSource` and runs the
  same sampler, collapse and contourer as `dualContourField`
  ([§ 1](README.md#1-pipeline-at-a-glance)), so a `SignMethod`, a depth or a collapse
  threshold means the same thing on both.
- **Tiled export equals the monolithic pass.** `--tile-depth` streams the same triangles the
  monolithic contourer would emit — STL bit-identical, 3MF geometry-identical, including a
  surface that lies on the root box — pinned by `tests/test_streaming_export.cpp`; the
  mechanism (ghost rings, owned cells) is
  [11/03](../roadmap/11-dense-lattice-deliverable/03-streaming-export.md).
- **Preview equals export.** `dualc_field_view` and `dualc_field` parse one graph
  (`examples/field_graph.{h,cpp}`) and the GPU renders the GLSL the codegen emits from that
  same tree, so the field on screen is the field that is contoured — no mesh round-trip. The
  GLSL prelude is held to the C++ formulas by `dualc_glsl_parity` at the tolerances in the
  table below; the harness needs a GL context, so it is opt-in and outside CTest.
- **Fallbacks are reported, never silent when asked.** An invalid root box and an empty
  contour each degrade to a defined placeholder and set a `Diagnostics` flag
  ([§ 9](07-limitations.md#behavioural-notes-worth-knowing)).
- **A destination file only ever appears complete.** Every export writes `path + ".part"`
  and renames it over `path` after a successful finish (`AtomicOutput` in
  `examples/example_common.cpp`); a cancel, a writer failure, an unbounded-field error or a
  killed process leaves at most the `.part`, and a file already at `path` survives a failed
  re-export. The hooks below are what make "cancel" reach a writer; the convention holds
  with or without them ([09 § Export-format dispatch](09-conventions.md#export-format-dispatch)).

## Hooks: cancellation and progress

Every driver takes the hooks as trailing optional pointers (`include/dualc/progress.h`):
the two samplers, the contourer, both `dualContour*` wrappers and `dce::writeField` take
`cancel, progress` after `Diagnostics*`; the two tiled writers take the same pair with no
`Diagnostics*` before it; the collapse pass takes `cancel` only:

- **`const CancelToken*`** — a host-set `std::atomic<bool>`, sticky, one per job,
  `request()` legal from any thread. The pipeline polls it with a relaxed load at every
  internal octree node of the build, every leaf of the QEF pre-pass, every internal node of
  the contour traversal and of the collapse pass, and at the top of every tile; a requested
  token makes the call throw `dualc::Cancelled` (a `std::runtime_error` naming the stage),
  which `internal::parallelFor` carries to the calling thread after joining every worker.
  At most the 8 leaves under one `maxDepth − 1` node are built between two polls, so the
  request-to-unwind latency is bounded by one such cell plus the octree's destruction.
  The `dce::` writers catch it and return **rc 3**; the C ABI returns `DUALC_CANCELLED`.
- **`ProgressSink*`** — `report(Stage, done, total) noexcept`, invoked **on the calling
  thread only**, never from a worker (the same rule as `Diagnostics`). Inside a parallel
  region that is `internal::parallelForPolled`'s doing: the caller spawns the workers,
  claims no index, and polls a condition variable at most every 100 ms. Within a stage
  `done` is monotonic, `total` constant and the last report is `(total, total)`;
  `Sample` and `Contour` come from the engine (Contour's total is twice the leaf count:
  QEF solves, then leaves first touched by the traversal), `Write` and `Tile` from the
  writers — a monolithic export reports the first three; a tiled export reports `Tile` only
  (its per-tile contours do not forward the sink), except on its single-pass fallback
  (`tileDepth >= depth`), where the one whole-box contour forwards it and `Sample` /
  `Contour` are what the host sees.
- **Neither hook changes the output.** An unrequested token and a recording sink yield a
  mesh and a file bit-identical to a call without them at every thread count
  (`tests/test_cancel_progress.cpp`). A sink must not throw — `noexcept` is what keeps
  `Cancelled` the only exception that ever unwinds a parallel region on the pipeline's
  behalf; a sink that wants to stop the job requests its token.

The record — why a token object rather than the callback's return value, the checkpoint
table with its latency argument, what is deferred — is
[14/05](../roadmap/14-c-abi/05-progress-and-cancel.md).

## The resolution rule: a feature is ≥ 2–3 cells or it does not exist

Dual contouring records **one crossing per octree cell edge**. Two surfaces inside one
cell — a thin wall (`onion`), a narrow gap between repeated copies, two struts nearly
touching — cannot both be recorded, so the mesh comes out fragmented: holes, non-manifold
edges, dropped slivers. The cell side is `rootExtent / 2^depth`
([§ 7](06-parameters-and-vendoring.md#7-configuration-parameters)), so the rule is: **the
thinnest wall or gap must span at least two to three cells**; otherwise raise `--depth` or
thicken the feature. No other parameter substitutes for it — `--collapse` merges cells, it
does not add them, and `mix`, `onion`, `repeat` all produce features that are only as real
as the cells that resolve them. Which depth a given recipe needs is that recipe's page in the
command reference.

The same sampling rule sets the **output size**: one vertex per surface cell, so the
triangle count scales with `4^maxDepth` (the surface-cell count), not with the shape — a
cube, a corner-carved cube and a small cube give similar counts at the same depth. Judge
whether a boolean carved by looking at the geometry, never by comparing counts;
`--collapse` is what reduces the count on flat regions
([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)).

## `mix` blends values, not shapes

`mix(A, B, control, lo, hi)` returns `lerp(A(p), B(p), w(p))` with `w` the control field's
value ramped over `[lo, hi]` (`MixField`, `src/implicit/combinators.cpp`). It interpolates two
**distance values**, not two geometries, and that has two consequences a user meets:

- Two solids that **coincide in space** — the same lattice family at two radii — blend into
  one continuous body, because their zero sets are close everywhere; the blend is then a
  graded radius, and `graded-offset` is the cheaper way to write it.
- Two solids that **do not coincide** — `bcc` against `fcc`, any two different crystals —
  taper to nothing around `w ≈ 0.5`: at a point on one crystal's strut the other crystal's
  value is far positive, and the average is positive. The output is two bodies with a gap
  slab between them. This is inherent to interpolating disjoint fields; no `--depth` fixes it,
  and the continuous alternative is a graft (overlap and union), which the
  [`dualc_field` morph page](../command_reference/11-dualc_field/03-graded-and-morph.md) shows.

Because the blended surface can float free of both operands' zero sets, `MixField` overrides
`cellOverlaps` to `true` and lets the sampler's sign test drive refinement — an OR of the
operands' answers would prune cells that hold blended surface
([05/03](../roadmap/05-tpms-lattices/03-crystal-blending.md)).

## Bounding-box sentinels

`BBox` (`include/dualc/types.h`) has three distinguished values, and one trap:

| Value | Meaning | `isValid()` | The sampler does |
| --- | --- | --- | --- |
| `BBox::infinite()` | the field has no finite extent (plane, infinite cylinder/cone, TPMS, `repeat`) | false | throws unless `SamplerParams::rootBounds` is set ([§ 10.5](08-implicit-field-layer.md#105-unbounded-fields)) |
| `BBox::empty()` | the field has no extent at all (an inverted box) | false | falls back to `BBox::unit()` and sets `Diagnostics::boundsFallback` ([§ 3.1](01-sampler.md#31-root-bounds)) |
| `BBox::unit()` | `[-0.5, 0.5]³` | true | uses it — it is the fallback box |
| `BBox{}` | the degenerate box at the origin | **true** | uses it as a real box |

The trap is the last row: a default-constructed `BBox{}` is valid, so an emptiness guard
written as `!b.isValid()` never fires for it and the origin is silently pulled into whatever
it is unioned with. **`BBox::empty()` is the only "no box" value**; `BBox{}` never stands for
it ([17/08](../roadmap/17-code-audit-and-hardening/08-argument-validation.md)). A
disjoint `intersection` yields an inverted box of the same class (`bboxIntersection`,
`src/implicit/combinators.cpp`), and the `unit()` fallback with the diagnostic is the correct
answer to it.

## `--tile-depth`: the legal range and the useful one

Settled by `forEachOwnedTile` (`examples/example_common.cpp`), which every tiled writer runs
through:

- **The only hard bound is `D ≥ 2`**, rejected at export time with
  `--tile-depth must be >= 2`. There is no upper bound and no check against `--depth`.
- A tile of depth `D` owns `2^D − 2` cells per axis (the outer ring is ghost cells). Tiling
  happens whenever `2^D − 2 < 2^depth`; when the owned tile covers the whole grid
  (`D ≥ depth + 1`) the writer falls back to **one streamed pass** and says so.
- So `D = depth` is legal but pointless — eight near-full tiles for no RAM saving — and the
  **useful range is `2 ≤ D ≤ depth − 1`**, which is exactly the range `--mem` searches
  (`pickTileDepthForBudget`: `[2, max(2, depth − 1)]`). `D < 4` is legal too, but the ghost
  ring dominates and the tile count balloons.

A page that says "`D < depth`" is stating the useful range; "`D = depth` accepted" is the
legal one; both are true. (The single-pass message reads `tile-depth >= depth` where the
condition is `D ≥ depth + 1`; the text is imprecise, the behaviour is as above.)

## The numeric constants

| Constant | Value | Decides | Where |
| --- | --- | --- | --- |
| Duplicate-hit merge, ray parity | 4 ULP of the hit distance | two hits on an edge shared by two triangles count once; a genuinely thin wall (tens of ULP apart) counts twice | `countRayHits`, `src/internal/mesh_bvh.cpp`; [§ 3.4](02-sign-oracles.md) |
| Root-box padding | `padFraction = 0.05` of the longest axis, every axis | the auto-fitted root box; explicit `rootBounds` gets no padding | `padBBox`, `src/sampler.cpp`; [§ 7](06-parameters-and-vendoring.md#7-configuration-parameters) |
| Lipschitz-1 prune test | `\|f(centre)\| ≤ halfDiagonal` | the default `cellOverlaps` after the corner-sign test; wrong for a field with `\|∇f\| > 1` | [§ 9](07-limitations.md#standing-limitations), [§ 10.1](08-implicit-field-layer.md#101-implicitfield) |
| `edgeHit` budget | 6 Illinois regula-falsi steps | where a crossing is placed on an edge and the normal is taken | [§ 10.1](08-implicit-field-layer.md#101-implicitfield) |
| Central-difference step | `e = 1e-4`, absolute, `double` | every finite-difference gradient: `PrimitiveField`, `PrimitiveField2D`, the distorting domain ops, `normalizedOf` | `include/dualc/primitives.h`, `implicit2d.h`, `src/implicit/domain_ops.cpp`, `decorators.cpp`; the absoluteness is [17 E20](../roadmap/17-code-audit-and-hardening/01-findings-ledger-engine.md) |
| Gradient-magnitude floor | `kEps = 1e-9` | `normalizedOf` divides by `max(\|∇f\|, kEps)`; a degenerate gradient returns `+z` | `NormalizedField`, `src/implicit/decorators.cpp` |
| QEF truncation | `qefRegularization = 0.1` on eigenvalues of `AᵀA` (≈ 0.316 on singular values of `A`) | which directions the pseudo-inverse keeps; leaf and collapse solves alike | [§ 4.4](04-qef-manifold-collapse.md#44-the-per-cell-qef), [§ 8](06-parameters-and-vendoring.md#8-vendored-third-party-code) |
| Vertex clamp tolerance | `clampToleranceCells = 1.0` | a minimiser more than one cell width outside its cell is replaced by the mass point | [§ 4.4](04-qef-manifold-collapse.md#44-the-per-cell-qef) |
| Collapse threshold | `simplificationError` (default 0, off), in length² | whether eight siblings merge; scales with the sample count, so it is depth-specific | [§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse) |
| Seam weld key | position quantised to `cs · 1e-6` per axis (`cs` = cell side) | which across-tile vertices `--weld` identifies | `examples/example_common.cpp`; [11/04](../roadmap/11-dense-lattice-deliverable/04-streaming-3mf-record.md) |
| GPU parity, analytic tier | abs `A = 2e-3`, rel `R = 2e-3` | pass/fail for closed-form nodes (CPU `double` vs GPU `float`, libm vs GPU trig) | `examples/dualc_glsl_parity.cpp` |
| GPU parity, finite-difference tier | abs `FA = 3e-2`, rel `FR = 3e-2` | pass/fail for `normalize` / `twist` / `bend` / `graded-onion` over TPMS: nodes whose C++ side differentiates with the `1e-4` step in `double`, where the GLSL takes a feature-relative `float` step for `normalize` (`min(diag·0.002, feat·0.003)`) and a pure point warp with no finite difference for `twist` / `bend` | `examples/dualc_glsl_parity.cpp`; [12/02](../roadmap/12-field-graph-and-app/02-glsl-codegen.md) |
| `mix` band guard | `\|hi − lo\| < 1e-9` → a signed `1e-9` | a zero-width band is a hard step, not a division by zero | `MixField::weightAt` |

A constant that appears in a test with a different value is the test's private tolerance,
not a behaviour; a constant that appears on a command-reference page is a copy of this row.

---

← Back to the [design index](README.md) · the [docs index](../README.md)
