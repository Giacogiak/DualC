# The Diagnostics channel (#26)

The full record for tracked item
[**#26**](03-correctness-and-robustness/03-diagnostics-item.md#26-a-diagnostics-channel-for-the-silent-failure-surface),
which the [2026-08-19 audit](../../raw/study/README.md) ranked as the single
highest-value change across the whole surface. The item's entry in
[03](03-correctness-and-robustness/README.md) keeps its heading and its one-line
status; the argument, the re-scoping and the evidence live here. Usage — what
the CLI now prints and what to do about it — is in
[cmd-ref README § Diagnostics warnings](../../command_reference/00-shared-behaviour.md#diagnostics-warnings-every-exporting-tool);
the per-field contract is in the header, `include/dualc/types.h`.

The usage page is the shared **README** rather than a `dualc_field` page because the
warnings are emitted by *every* tool exporting through `dce::writeField`, not by one tool
— the cross-tool case the command reference's index owns;
[`11-dualc_field/`](../../command_reference/11-dualc_field/README.md) carries a one-line
pointer.

*(2026-09-18: the re-scoping table, the verification record and the untested-branch note
moved verbatim to [11](11-diagnostics-verification.md) to bring this page under the size
cap — [19 Phase 5](../19-docs-layers/README.md).)*

> **Open items found 2026-09-09.** Three limits were accepted knowingly and
> are stated in the header rather than hidden: the per-probe grid clamp is not
> counted (see [§ What is deliberately not reported](#what-is-deliberately-not-reported));
> a `GridField` **nested** below the root of a graph is not detected; and the
> input-mesh fields are filled only by the mesh entry points, not by a field
> graph that happens to contain a `MeshSource`.

## Re-scoping the item

**Done 2026-09-09, before any code.** #26 was written against six silent degradations;
checked against `71b8018`, **three of the six no longer exist**, and one of those three
never did (`windingNumberFast` with no tree — both call sites build the tree when they use
it). The claim-by-claim table, and the ledger correction it is:
[11 § Re-scoping](11-diagnostics-verification.md#re-scoping-the-item). The audit's *other*
claim about this item **holds**: the watertightness measurement was being computed and
thrown away, so recovering it costs two counters.

## The channel as delivered

**DONE (2026-09-09).** `struct Diagnostics` in `include/dualc/types.h`, passed
as a trailing `Diagnostics* diag = nullptr` on every driver:
`sampleFieldToHermiteOctree`, `sampleMeshToHermiteOctree`
(`include/dualc/sampler.h`), `contourHermiteOctree`
(`include/dualc/contourer.h`), `dualContourField` and `dualContourMesh`
(`include/dualc/pipeline.h`). **Source-compatible** — no existing call site
changed, and `nullptr` reproduces the old code path exactly.

The shape was chosen over a `DiagnosticSink` callback and over changing the
return types. A sink would have put a virtual into the parameter structs of
the hot path and added a second public polymorphic type; changing the return
tuple would have broken every caller including the C ABI and Boletus, for a
feature that is opt-in by nature. CLAUDE.md's "don't widen the public surface
casually" is answered by one aggregate struct with no methods but a predicate.

Fields, and where each is set:

| Field | Set by | Meaning |
| --- | --- | --- |
| `inputEmpty` | mesh samplers | the input mesh had no triangles |
| `inputBoundaryEdges` | mesh samplers | input edges with one incident triangle |
| `inputNonManifoldEdges` | mesh samplers | input edges with three or more |
| `inputWatertight` | mesh samplers | both of the above are zero |
| `boundsFallback` | both samplers | `bounds()` was unusable; the unit cube was sampled |
| `gridBoundsExceeded` | both samplers | the root box reaches outside a root-level baked grid |
| `emptyContour` | contourer | no surface: the result is the placeholder triangle |
| `outputVertices` / `outputTriangles` | contourer | counts of the mesh actually returned |
| `outputBoundaryEdges` / `outputNonManifoldEdges` | contourer | output edge tally |
| `outputWatertight` | contourer | both output counts are zero |
| `anyIssue()` | — | predicate over the degradations worth surfacing |

`anyIssue()` deliberately excludes a non-watertight **input**: open input is
the legitimate case for `GENERALIZED_WINDING_NUMBER`, so the edge counts are
there to be read, not to raise an alarm. A non-watertight **output** does
count — dual contouring is supposed to produce a closed mesh.

Two supporting surfaces were added to reach the input facts:
`MeshBVH::numBoundaryEdges()` / `numNonManifoldEdges()` (internal), and
`MeshSource::triangleCount()` / `boundaryEdgeCount()` / `nonManifoldEdgeCount()`
(public, on the class whose preconditions they describe).

**Threading.** Every field is written by the calling thread, outside the
parallel region — never from a worker. That is why the channel cannot perturb
the bit-identical-across-thread-counts guarantee that
[#30](04-engineering-quality.md#30-threading-determinism-test) now tests, and
it is the constraint that shaped what could be reported at all.

**Cost.** Nothing is computed when `diag` is null. When it is non-null the
contourer adds one O(E) hash pass over the emitted triangles; the input edge
tally is free (two increments in a loop that already ran), and
`gridBoundsExceeded` is one `dynamic_cast` and one box test.

The host-side `contourOrHint` therefore takes an explicit `collectDiag`
opt-in: only the monolithic `writeField` sets it. The tiled/streaming driver
(`forEachOwnedTile`) and the `--mem` probe contour go through the same helper
and would otherwise have paid the tally **per tile** — on the one path whose
entire purpose is bounded cost. This too was caught in review, after the first
pass wired the diagnostics in unconditionally.

## What is deliberately not reported

**Per-probe grid clamping — DEFERRED.** `grid_field.cpp:34-44` clamps every
coordinate into the baked lattice, so a probe outside the region silently
reads the nearest face value. Counting those probes truly would require
mutable state on a field that is evaluated concurrently from every worker —
which contradicts the `ImplicitField` thread-safety contract fixed in
[#29](04-engineering-quality.md#29-thread-safety-contract-for-user-derived-implicitfield)
and the determinism guarantee in
[#30](04-engineering-quality.md#30-threading-determinism-test). **Recording
that reason is the point of this entry**: it is not an oversight to be picked
up later without argument. Trigger to revisit: a caller reporting a wrong
result traced to a baked-region read that `gridBoundsExceeded` did not catch.

`gridBoundsExceeded` is what ships instead, and it is **not** a general "root
box exceeds `field.bounds()`" test. That generalisation was written first and
then rejected: for every field other than a baked grid, `bounds()` says where
the *surface* is, not where the field is defined, so sampling past it is the
normal case — a padded root box over a `MeshSource` exceeds its AABB on
essentially every run. A flag that always fires is worse than no flag. The
shipped check therefore fires only for a `GridField` **at the root of the
sampled graph** and only for an **explicit `rootBounds`**.

That second condition was missed on the first pass and caught in review. The
auto-fit branch sets `bounds = padBBox(field.bounds(), padFraction)`, so on a
root-level `GridField` the padded box lies outside the bake **by
construction** — the same always-fires failure, reintroduced in the narrow
case, on the most common shape of all (no `--bounds`). The first version of
the test suite could not see it because every grid case passed an explicit
root box. Both are fixed: `sampleFieldToHermiteOctree` skips the check unless
the caller supplied `rootBounds`, and a new section contours a baked grid with
default `SamplerParams`. Demonstrated failing first — with the guard removed
that section reports `gridBoundsExceeded = true` and `anyIssue() = true`. The
flag now means strictly **"you asked for a region the bake does not cover"**.

## The dead warning this exposed

`examples/example_common.cpp` carried `warnIfEmpty`, which tested
`c.mesh->nFaces() == 0` and printed "the field produced an empty mesh". The
contourer can never return zero faces — it synthesizes the placeholder
triangle precisely so geometry-central will accept the polygon list — so
**that warning could not fire**, and a field that crossed no surface wrote a
one-triangle file in silence. It is replaced by `warnDiagnostics`, which
reports `emptyContour`, `boundsFallback`, `gridBoundsExceeded` and a
non-closed output as `[dualc] warning:` lines. Every CLI that exports through
`dce::writeField` gets this — `dualc_field`, `dualc_primitive`,
`dualc_boolean`, `dualc_lattice`, `dualc_lift`, `dualc_csg_demo`. The exact
messages, and what each one means, are in
[cmd-ref README § Diagnostics warnings](../../command_reference/00-shared-behaviour.md#diagnostics-warnings-every-exporting-tool).

## The C ABI mirror

`DualcDiagnostics` (flat, `int` booleans and `uint64_t` counts) plus
`dualc_field_contour_with_diagnostics` and
`dualc_field_export_with_diagnostics` in `capi/dualc_c.h`. **New entry points,
not changed signatures** — the same twin pattern the header already
establishes with `dualc_field_create_from_{json,expr}_with_meshes`. The three
original calls become forwarders passing `NULL`, so a host built against
0.3.0 keeps working in source *and* in binary; Boletus needs no change to
keep compiling and can adopt the twins when it wants the report.

Version bumped **0.3.0 → 0.4.0** in the root `CMakeLists.txt`, which is what
`dualc_version()` returns. The entry points are additive, but the version
*string* is not — a host that pattern-matches `"dualc 0.3.0"` rather than
displaying it would break. Boletus displays it (Boletus' core wrapper surfaces
it as a version readout), so nothing there needs changing; a host that does
match on it should be told.
`dce::writeField` gained the same optional out-parameter to feed the export
twin; its output counts describe the contoured mesh, so they read high when
`--decimate` is also in play (documented at the declaration).

## Verification

`ctest` **245/245**, up from the 235 baseline — ten new `[diagnostics]` cases; every signal
demonstrated firing, the channel shown inert, all eight demo meshes byte-identical on the
positional digest, the CLI and the C ABI checked end to end. The digests and the case list:
[11 § Verification](11-diagnostics-verification.md#verification).

## A note on the not-closed warning

`outputWatertight` is false whenever the surface is **clipped by the root
box** — e.g. `sphere(radius=0.4) --bounds -0.6,-0.6,-0.6,0.1,0.1,0.1`, which
reports 138 boundary edges. That is not a false positive: the mesh really is
open along the cut, and a slicer really may reject it. It is nonetheless a
deliberate and common workflow, so the warning names both causes (thin feature
vs. intentional clip) rather than implying a defect
([cmd-ref README § Diagnostics warnings](../../command_reference/00-shared-behaviour.md#diagnostics-warnings-every-exporting-tool)).
Under-resolution is the other cause and is the one worth acting on: a
`gyroid(wavelength=0.5, thickness=0.02)` at `--depth 5` over a 2-unit box
(cell 0.0625, wall 0.02) reports 2,022 boundary edges — previously it wrote
that fragmented shell in silence.

## Not covered by a test

`inputEmpty`'s **true** branch is reported but unexercised — what is pinned instead, and
why: [11 § Not covered](11-diagnostics-verification.md#not-covered-by-a-test).

## Follow-ups

- **Per-probe grid clamp counting** — DEFERRED, trigger above.
- **A `GridField` nested below the root** is not detected. Finding one would
  need graph introspection (a defaulted virtual on `ImplicitField`, forwarded
  by ~40 nodes) — a much larger widening than this item justified. Trigger:
  the same wrong-result report as above, on a graph where the grid is not the
  root.
- **The tiled/streaming export path passes no diagnostics.** Deliberate, and
  now enforced by the `collectDiag` opt-in: it contours per tile, so
  `emptyContour` is the normal case for most tiles and the O(E) tally would
  run once per tile. Trigger: a streamed export producing a bad part with no
  explanation.
- **`isEmptyPlaceholder` in `example_common.cpp` is now redundant.** The
  streamer drops empty tiles by matching the placeholder's three literal
  vertex positions `(0,0,0)/(1,0,0)/(0,1,0)` — a heuristic that is wrong for a
  genuine one-triangle mesh at those exact coordinates, which is precisely the
  ambiguity #26 exists to remove. `Diagnostics::emptyContour` answers it
  exactly. Not switched here because the streamer would then pay the O(E)
  tally per tile; the real fix is to split the cheap flags from the edge tally
  so a caller can ask for `emptyContour` alone. Trigger: that split, or any
  report of a dropped legitimate tile.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
