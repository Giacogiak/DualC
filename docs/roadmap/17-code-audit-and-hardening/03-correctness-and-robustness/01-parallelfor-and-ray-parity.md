# Correctness & robustness — `parallelFor` and ray parity (#22, #23)

Part of [03 — Correctness & robustness](README.md): the two items closed in the first batches: exception propagation through the
parallel build, and the ray-parity oracle's hit-merge rule. Headings are frozen at ID +
title; status, date and trigger live on the body lines.

## #22 `parallelFor` exception propagation
**DONE (2026-08-31).** A throw inside a worker escapes `std::thread`'s entry point and calls
`std::terminate`; a throw on the calling thread's own `worker()` unwinds past the `join()`
loop and destroys joinable threads — also `std::terminate`. Two distinct paths, both live
as soon as `numThreads > 1`, and reachable through any user-supplied field — exactly the
plugin case a managed callback crossing the C ABI ([14](../../14-c-abi/README.md)) creates. Fix:
capture the first `std::exception_ptr` per worker behind an atomic flag, join
unconditionally, rethrow after — about ten lines.
**Delivered.** `src/internal/parallel.h`: workers wrap `fn(i)` in a `try`/`catch(...)`,
the first `std::exception_ptr` is kept under a mutex, an atomic `failed` flag stops every
worker claiming further indices, all threads are joined unconditionally, and the captured
exception is rethrown after the joins. The serial path (`threads <= 1 || n <= 1`) is
unchanged and still propagates naturally, so the observable behaviour is now the same on
both paths. Gated by `tests/test_parallel.cpp` (`[parallel][exceptions]`): a throwing body
on the serial path, on the threaded path, one failing index out of 1,024, the early-out,
and an unaffected successful run.

*Verified:* **`ctest` 231/231.** The new case was run against the **pre-fix** header
(`git show HEAD:src/internal/parallel.h`, rebuilt): the process **aborts — exit 127 with
all buffered output lost, not a test failure** — while the same pre-fix binary passes
`[determinism]` (289,426 assertions) and the serial-path section alone, which isolates the
abort to the threaded path exactly as predicted.
*Source:* `docs/raw/study/B4-concurrency.md:132-143`, `docs/ARCHITECTURE.md` § 9.

## #23 Ray-parity advance epsilon is relative to hit distance, not feature size
**DONE (2026-09-01).** `countRayHits` advanced past each hit by `max(tHit × 1e-5, 1e-6)`. At
`tHit = 2000 mm` that is 0.02 mm, so a thin wall, a strut or a coincident triangle inside
that distance is skipped and the parity flips. The three-ray vote does **not** protect
against it: all three rays share the origin and the same epsilon, so they fail together.
The pack calls it the most concrete correctness bug in the BVH. Two parts: make the advance
relative to the bounding-box diagonal (or use a single any-hit traversal with `prim_id`
dedup), and give the hard-coded 4096-hit cap an out-of-band signal instead of silently
truncating to garbage parity.
*Source:* `docs/raw/study/A2-sampling-octree-oracles.md:239-251`.
*Verified 2026-08-31:* `src/internal/mesh_bvh.cpp:486` unchanged; cap at `:480`.

**Sizing it first.** Float ULP is ~1.2e-7 relative and the advance was 1e-5 relative, so the
defect is not "sub-precision features are lost" — it is a band roughly **84× wide** of
features that are perfectly resolvable in `float` and were dropped anyway. That number also
sets the reproducer: the crossing separation of a slab of thickness `w` at axial distance
`X` is `w/dx` and the advance is `(X/dx)·1e-5`, the `1/dx` cancels, so the far face is
swallowed **iff `w < X·1e-5`, for every direction at once** — and `w = X·1e-5` is a tie
decided by rounding, which demonstrates nothing.

The governing quantity is feature thickness relative to the **sample-point-to-feature
distance**, not to absolute coordinates — this is unrelated to
[#24](02-precision-and-celloverlaps.md#24-far-from-origin-precision--re-centre-before-the-float-seam),
and because sample points are octree corners inside the model's own bounds it reduces to a
feature thinner than ~1e-5 of the model extent. A 200 mm part with a 2 mm wall is five
orders of magnitude clear of it; a 100 m model with a 1 mm membrane, or near-coincident
duplicate surfaces from a bad boolean, is the reachable class.

**Demonstrated before it was fixed**, on `w/X = 3e-6` (a 6 µm wall at `x = 2000`, 3.3×
inside the epsilon and still ~34 ULP of real separation). At `d48ad98`,
`countRayHits` returned **1** instead of 2, and — the part that matters —
`SignOracle(WINDING_NUMBER).isInside(origin)` returned **true**: the origin, 2 m from a
6 µm plate, classified as *inside*, with all three probe rays losing the back face
together. `windingNumber()` on the same mesh said outside, isolating the fault to the
parity path. That is a corner sign, so it decides which octree edges carry Hermite data at
all — the failure mode is changed topology, not a nudged vertex.

**Delivered.** `countRayHits` is now a single all-hits BVH walk with **no advance epsilon
and no cap**, so both parts of the item are closed — the 4096-hit truncation needs no
out-of-band signal because there is no longer a re-traversal to bound. nanort ships a
`MultiHitTraverse` for this but it is `#if 0`-ed out (`nanort.h:1899`) and geometry-central
is a sibling checkout, so the walk is written against nanort's public pieces (`GetNodes` /
`GetIndices`, `IntersectRayAABB`, `TriangleIntersector::PrepareTraversal` / `::Intersect`),
in the same iterative-DFS shape as `cellOverlapsAABB`.

**The epsilon had a second, undocumented job**, and removing it exposed it: a ray landing
exactly on an edge shared by two triangles is admitted by *both* — nanort's watertight test
rejects only mixed-sign edge functions — and the old advance swallowed the duplicate
because it arrived at the same `t`. The grazing control case caught this immediately
(4 hits where 2 were expected, on the z=0 diagonal of the unit cube). Rather than guess a
tolerance, the hit distances were instrumented: **duplicates come out bit-identical, 0 ULP
apart, while the two faces of a genuinely thin wall are 29–49 ULP apart** at the same
distances. Coincident hits are therefore merged at **4 ULP** — 7× below the measured real
separation, at the float resolution floor, and ~25,000× tighter than the 1e-5 it replaces.

Note the asymmetry in that evidence: the *feature* side of the margin is measured, the
*duplicate* side is generalised from the configurations tested. The Woop `t = D/det` is
evaluated from different edge functions for the two triangles sharing an edge and came out
bit-identical in every case here, but a very oblique shared edge on large triangles is not
covered. The tolerance should not be widened to cover it — that reopens the feature band —
because the residual risk is already absorbed one level up: a duplicate landing more than
4 ULP apart mis-counts **one** ray, and `SignOracle` takes a majority of three whose
directions are non-axis-aligned irrational permutations, so an exact-edge hit on all three
at once is not a reachable configuration.

One further divergence was caught by reading nanort's *live* `Traverse` rather than the
`#if 0`-ed `MultiHitTraverse` the walk was patterned on: `Traverse` computes
`1.0f / (dir[k] + 1e-12f)`, and the dead code omits the guard, carrying a
`@fixme { Check edge case; i.e., 1/0 }` where it belongs. Without it an axis-aligned
direction yields an infinite reciprocal and a ray lying exactly in a node's slab plane
evaluates `0 * inf = NaN`, dropping the subtree. The guard is copied verbatim, which also
keeps node acceptance in `countRayHits` identical to `segmentFirstHit`'s. It is not given a
bespoke test: `0 * inf` requires the ray to lie *within* a node's boundary plane, which is
inherently a coplanar grazing configuration with no well-defined expected count, and the
sign oracle emits no axis-aligned probes. Matching the live traversal removes the class
outright, and the demo digests and the four real meshes are unchanged with the guard in.

*Verified:* **`ctest` 235/235** (231 + four new `[parity]` cases: even/odd parity in and
out of a cube, the exact-edge and exact-corner grazing controls, the thin-wall reproducer,
and the `SignOracle` case). Because every existing demo assertion is a topology invariant
(boundary edges, χ, face count) and all three survive a vertex moving, a positional golden
digest was added — hidden `[.][golden]` in `tests/test_demo_meshes.cpp`, FNV-1a over raw
vertex `double`s and face indices — and captured either side of the change: **all eight
demo meshes byte-identical**. End-to-end against a rebuild of `d48ad98`, `dualc_demo` is
byte-identical on `bunny` (`--depth 8`) and on `molde`, `foot` and `mesh-soup`
(`--depth 7`) — the last being non-closed input, where the parity oracle is least
constrained — and ~6% faster on the bunny (25.6–27.2 s → 24.2–24.6 s), the expected result
of one descent per ray instead of one per hit.

---

← Back to the [correctness index](README.md) · the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
