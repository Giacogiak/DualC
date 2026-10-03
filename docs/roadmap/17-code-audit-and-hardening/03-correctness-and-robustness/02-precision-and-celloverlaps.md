# Correctness & robustness — precision and `cellOverlaps` (#24, #25, #46)

Part of [03 — Correctness & robustness](README.md): the float seam at the QEF, and the `cellOverlaps` family — the declared-bound design
item and the one decorator that forgot the forward. Headings are frozen at ID +
title; status, date and trigger live on the body lines.

## #24 Far-from-origin precision — re-centre before the float seam
**DEFERRED.** The public API and the octree are `double`; the BVH (nanort) and the QEF/SVD
(vendored) are entirely `float`, and neither narrowing re-centres. Invisible at ~6 nm on a
100 mm part, but at site coordinates (500,000 mm) the ULP is 0.03 mm and sub-0.1 mm features
vanish with no warning — the proviso "provided the model is near the origin" is load-bearing
and undocumented. The same absolute-epsilon assumption appears in the field layer: the
central-difference step `1e-4` is hard-coded in five files (`primitives.h:28`,
`implicit2d.h:50`, `decorators.cpp:313`, `domain_ops.cpp:17`, `lift.cpp:14`) in a library
documented as 1 unit = 1 mm. Fix: subtract the AABB centroid in `packMesh` and add it back
on query (one `Vector3` of state), and scale the finite-difference step to the model.
*Trigger:* a part authored at site coordinates, or any report of vanishing sub-0.1 mm features.
*Source:* `docs/raw/study/B3-memory-ownership-performance.md:181-207`,
`docs/raw/study/A5-implicit-field-algebra.md:264-266`, `docs/ARCHITECTURE.md` § 9.

## #25 `lipschitzBound()` — the silent correctness opt-out behind `cellOverlaps`
**DEFERRED.** `cellOverlaps` serves two unrelated purposes at once: a performance fast path
and a **correctness opt-out**. The base implementation encodes a Lipschitz-1 assumption that
is not machine-checkable, and forgetting to override it is silent — not a compile error. The
2026-08-21 batch fixed the nine wrappers that had forgotten
([01 § 4.9](../../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch)) and pinned
conservativeness with a property test, but that entry states the design problem itself is
unchanged: the next wrapper anyone adds inherits the same default and must be added to the
test list by hand. The audit's fix is one virtual —
`virtual double lipschitzBound() const { return 1.0; }` — replacing the hand-written
overrides and making the whole class machine-checkable.

Deliberately **separate from [05
#18](../../05-tpms-lattices/README.md#18-tight-lipschitz-celloverlaps-for-tpms-primitives)**,
which is a *performance*
item (a closed-form per-TPMS bound, ~12× at depth 7) with its own trigger. Folding the two
would give a correctness fix a performance trigger that may never fire. #18 becomes cheap
once #25 exists, but neither blocks the other.
*Trigger:* the next domain wrapper or decorator added to the codebase — that is when the
silent-omission bug recurs.
*Source:* `docs/raw/study/A5-implicit-field-algebra.md:64-66`,
`docs/raw/study/B2-api-and-type-design.md:45-49`, `docs/ARCHITECTURE.md` § 9.
*Verified 2026-08-31:* no `lipschitzBound` in `include/` or `src/`; 73 `cellOverlaps` sites
across `combinators.cpp`, `decorators.cpp`, `domain_ops.cpp` and `implicit.h`.

*(2026-09-20: the omission this trigger predicts is already in the tree — `OffsetField` — and is
tracked as [#46](#46-offsetfield-does-not-forward-celloverlaps), opened the same day.
2026-09-21: #46 closed by the hand-written override; the design problem this item names is
untouched — the eleventh wrapper will forget the same way.)*

## #46 `OffsetField` does not forward `cellOverlaps`
**DONE** 2026-09-21 (opened 2026-09-20 from finding 4 of the first
[semantic lint](../../19-docs-layers/08-semantic-lint/01-2026-09-20-first-run.md), sequenced
ahead of #34 and #32 the same day). `OffsetField`
(`offsetOf` / `roundedOf`, `src/implicit/decorators.cpp`) is the one level-set decorator with no
`cellOverlaps` override, so a non-Lipschitz child's always-overlap signal stops at it and the
base Lipschitz-1 test prunes cells the surface passes through — the class the 2026-08-21 batch
fixed for nine wrappers ([01 § 4.9](../../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch)),
and the in-tree instance of #25's silent omission. Measured: `dualc_field --expr
'offset(gyroid(wavelength=0.15),r=0)' --bounds -1,-1,-1,1,1,1 --depth 6` — 331,918 faces and
57,072 boundary edges against 499,018 and 15,822 for the bare gyroid; at `wavelength=0.5` the
two are identical, which is why no recipe had shown it. Cmdref 06 names `offset(gyroid(…))`
as the field graph's Quilez shift, so a documented recipe reaches it; the `offset` row of
cmdref 11/01 carries the caveat until this closes.
*Fix:* forward as `OnionField` does — `child_->cellOverlaps(expandBBox(cell, std::abs(r_)))`,
the level set `child = r` lying within `|r|` of the child's surface — and add `offsetOf` to the
superset property test in `tests/test_domain_ops.cpp` plus a TPMS-under-`offset` case; the
`r = 0` A/B above must then be byte-identical. #25 (a declared bound) stays the design fix.
*Source:* [design/07](../../../design/07-limitations.md#standing-limitations); the lint run above.

**2026-09-21 — fixed as prescribed.** `OffsetField::cellOverlaps` forwards
`child_->cellOverlaps(expandBBox(cell, std::abs(r_)))` (`src/implicit/decorators.cpp`) —
`std::abs`, not the `std::max(r_, 0.0)` its `bounds()` uses: an inward shift puts the level
set `|r|` *inside* the child, where a cell holding it may not touch `f = 0`, and growing by
zero would prune it. Three tests in `tests/test_domain_ops.cpp`, each seen failing first:

- `offsetOf` (outward and inward) joins the superset property test. Pre-fix it failed on
  `asked.empty()` — the wrapper answered on its own — and on nothing else: `offset` reads its
  child at the very point it is asked, so *any* forwarded box covers every read and the
  geometry loop cannot tell `cell` from `cell` grown by `|r|`.
- "`offsetOf` asks its child about the cell grown by `|r|`" is what pins the growth: a unit
  sphere and a thin cell straddling radius `1 ± 0.2` but clear of the sphere, with the
  precondition `sphere->cellOverlaps(cell) == false` asserted so the case is known sharp. It
  passes pre-fix — the default test runs on `offset`'s own value, a valid SDF for a Lipschitz
  child — and fails on the tempting simplification `child_->cellOverlaps(cell)` (probed:
  both sections fail), which is the regression it guards against.
- `intersection(offset(gyroid(wavelength=0.25), 0), box ±0.5)` in the root box ±0.8 at
  depth 6 contours watertight, beside the twisted/mirrored TPMS-shell case. Pre-fix: 834
  boundary edges, 58,740 faces against the bare gyroid's 62,736. Whether the default test
  prunes depends on how the surface sits in the coarse cells: at `wavelength=0.15` in that
  same box it happened not to prune at all (identical meshes pre-fix), which is why the
  test's wavelength is the one that did.

*Measured post-fix:* the record's A/B — `offset(gyroid(wavelength=0.15),r=0)`, bounds ±1,
depth 6 — is **byte-identical** to the bare gyroid (`cmp`): 237,817 V / 499,018 F / 15,822
boundary edges, from 178,767 V / 331,918 F / 57,072; the wall time went 14.7 s → 20.8 s, the
bare gyroid's 20.1 s, as always-overlap now reaches every cell. The clipped test
configuration is byte-identical to its bare counterpart too. Gate: build clean, 0 DualC-origin
warnings, ctest 257/257, `--gpu` parity 73/73 (a CPU pruning gate, not codegen — parity did
not move). The `offset` row of cmdref 11/01 and the pointer in cmdref 06 dropped their
caveat; design/07 moved `OffsetField` into the forwarding group with its `|r|` rule.

---

← Back to the [correctness index](README.md) · the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
