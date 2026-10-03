# `repeat` tiling — the single-fold defect (#34)

The full record for the first item taken out of
[**#34**](04-engineering-quality.md#34-api-hygiene-batch). The audit filed it under API
hygiene; it was not hygiene. `RepeatField` produced **wrong geometry in shipped strut
lattices**, which makes it the same class as the domain-operator forwarding bug the
[2026-08-21 screening batch](../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch)
fixed, and it is recorded here for the same reason that one was.

The follow-up performance item this created is
[**#35**](04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation). Usage
impact is one line in the `repeat` row of
[cmd-ref 11](../../command_reference/11-dualc_field/README.md).

## The defect

**DONE (2026-09-10).** `RepeatField::valueAt` folded `p` into the tile it falls in
(`v - s·round(v/s)`) and read the child once. That is the true infinite union **only when
no copy of the child reaches across a tile boundary**. `RepeatLimitedField`, in the same
file, had the corrected form all along — the 8-neighbour minimum — so the two repetition
operators disagreed about what repetition means.

Demonstrated failing first, in `tests/test_domain_ops.cpp` ("repeated agrees with the true
infinite union across a tile boundary"). With a child both off-centre and wider than half
its period — a sphere at `x = 0.4`, `r = 0.3`, period `1.0`, so its geometry spans
`x ∈ [0.1, 0.7]` and crosses the `+0.5` seam — the fold reports:

| at `x = 0.55` | value |
| --- | --- |
| true infinite union (oracle over ±3 copies) | **−0.15** — inside |
| `RepeatField::valueAt` before the fix | **+0.55** — outside |

A **sign** flip, not a nudged value. A flipped sign flips a corner sign, which decides
whether an octree edge carries Hermite data at all, so the failure mode is changed
topology. 16 of the case's 43 assertions failed before the fix.

The test that existed above it could never have caught this: it repeats a sphere of radius
1 with a period of 4, which never comes near a seam.

## The fix is conditional, because the fold is not always wrong

Whenever the child fits inside one period, the fold *is* the infinite union, and that is
the common case. So `RepeatField` now decides once, at construction: `foldExact_` is true
when `child->bounds()` is valid, finite, and contained in `[-s/2, s/2]` on every repeated
axis.

- `foldExact_` → the original single fold, one child evaluation, no regression.
- otherwise → the unclamped 8-neighbour minimum, mirroring `RepeatLimitedField::evaluate`
  (there is no tile count to clamp ids against here).

An invalid or infinite child `bounds()` counts as *not proven* and takes the safe path.

**`cellOverlaps` had to be widened to match.** The two evaluation paths read the child on
different sets, and the wrapper must ask about a superset of what it reads. The
conservativeness property test added by the 2026-08-21 batch ("Domain wrappers ask their
child about a superset of what they read") failed on the first build with
`read at (-0.9, 0.1, 0.1) is outside every box the child was asked about` — exactly the
regression it was written for, caught within one compile. The neighbour path now
enumerates per-tile-id boxes over `[round(min/s) − 1, round(max/s) + 1]`, with the same
`span > 64` single-spanning-box fallback `RepeatLimitedField` uses for very shallow cells.

## Cost, measured on the flagship path

Strut cells overhang their period by the capsule cap radius, so **strut lattices take the
neighbour path**. `dualc_field --expr "octet(wavelength=0.4,radius=0.05)"
--bounds -1,-1,-1,1,1,1 --depth 7`:

| | before | after |
| --- | --- | --- |
| wall clock | 39 s | **91 s (2.3×)** |
| faces | 1,433,724 | 1,433,724 (unchanged) |
| vertices moved | — | **678 of 4,301,172 (0.016 %)** |
| max deviation | — | **0.0167 units** — a third of the 0.05 strut radius |
| mean deviation | — | 1.1 × 10⁻⁶ units |

The mesh changing is the *point*: those 678 vertices were misplaced before, at the tile
seams, by a third of a strut radius. Topology is unchanged, so nothing catastrophic was
happening — but a lattice for a real part was not the lattice it claimed to be.

Landing the cost as measured and optimising separately was a deliberate call rather than
an oversight; the follow-up is
[#35](04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation).

`ctest` **246/246**.

## What this exposed in the test suite

`tests/test_strut_lattice.cpp` builds an independent oracle — the exact minimum over the
segments tiled across ±3 cells — and its comment states it *"certifies that the single
round-fold stays exact once struts carry the larger node caps."* **It does not.** The 678
moved vertices are error it passed clean over, because it samples pseudo-random points in
a box with an `Approx` tolerance and never lands hard on a seam. The oracle itself is
sound; the sampling strategy is what fails, and seam-targeted points would have caught it.

Left in place by decision and folded into
[#32](04-engineering-quality.md#32-test-coverage-ledger), so the comment currently
overstates what is established. Same shape as the dead `nFaces() == 0` warning
[#26](03-correctness-and-robustness/03-diagnostics-item.md#26-a-diagnostics-channel-for-the-silent-failure-surface)
found: an assurance that reads as coverage and is not.

## A note on method

Every claim in #34 was re-verified against `ebe929a` before any of it was scoped, because
#26 had just found that one of the audit's six claimed degradations
([05](05-diagnostics-channel.md)) **had never been true**. This time **all seven of #34's
claims held**, including one the audit only hinted at: `RevolveField::bounds` returns
`BBox{}` as its invalid-profile sentinel (`src/implicit/lift.cpp:50`), and `BBox{}` reports
`isValid() == true` — so the sentinel is unrecognisable by the very predicate meant to
catch it. That one is still open in #34. *(Closed the same day — `BBox::empty()`, batch 2,
[08](08-argument-validation.md); this sentence is left as written.)*

The audit is a dated snapshot, not a status board. Checking first cost minutes and changed
the scope of two consecutive items.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
