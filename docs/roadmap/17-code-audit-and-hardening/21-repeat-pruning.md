# #35 — pruning `repeat`'s neighbour set

The record of [04 #35](04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation),
unit P06 of the [hygiene + editor run](../../raw/2026-10-19-hygiene-and-editor-plan.md), on
branch `feat/hygiene-and-editor`, 2026-10-21. It follows the cost
[07](07-repeat-tiling-fix.md#cost-measured-on-the-flagship-path) recorded: a child that
overhangs its period reads 8 copies per evaluation instead of 1. The owner's rule (raw plan
§ 1): implemented ahead of its trigger, measured, and **byte-identical**.

## The baseline, before any change

Linux, Ubuntu, GCC 15.2, i7-920 (4 c / 8 t), Release with the gate's options (C ABI on, so
PIC). `dualc_field --expr "octet(wavelength=0.4,radius=0.05)" --bounds -1,-1,-1,1,1,1
--depth 7 --threads 4 -o …obj`, wall clock by `/usr/bin/time`, three runs each:

| build | runs (s) | faces |
| --- | --- | --- |
| 8-neighbour (`db50b2e`, the shipped code) | 129.1 · 132.3 · 133.4 | 1,433,724 |
| single fold (`db50b2e` with `foldExact_` forced true, scratch only) | 46.6 · 46.2 · 45.8 | 1,433,724 |

**2.86× on this host**, against 17/07's 2.3× (39 s → 91 s, all threads, the old Windows
host). The two meshes differ on 3,452 of 6,456,310 OBJ lines, so the fold is not a
byte-identical answer for these cells (the 260 moved vertices of
[15](15-test-coverage-batches.md#the-strut-lattice-oracle-certified-what-it-claimed)).

**Reference meshes**, from the shipped 8-neighbour code (`db50b2e`), `--threads 4`, in the
scratchpad `/tmp/p06/ref/`; their SHA-256 is the durable record, the binary can be rebuilt
from `db50b2e`:

| case | expression, bounds, depth | faces | SHA-256 (first 16) |
| --- | --- | --- | --- |
| octet | as above | 1,433,724 | `3ea0229692680004` |
| BCC | `bcc(wavelength=0.4,radius=0.05)`, same box, 7 | 887,340 | `abd94e4f3ce7d58d` |
| tapered octet | `octet(…,nodeRadius=0.08)` (round cones), same box, 6 | 291,804 | `618f65cbd447dcf1` |
| sphere across a seam | `repeat(sphere(center=[0.4,0,0],radius=0.3),period=[1,0,0])`, `-1.5,-0.5,-0.5,1.5,0.5,0.5`, 7 | 92,140 | `f3fe9c003d68cdc4` |
| the same, two axes | `…center=[0.4,0.15,0]…,period=[1,1,0]`, `-1.5,-1.5,-0.5,1.5,1.5,0.5`, 7 | 128,624 | `c7c4646d71d6c85b` |
| limited repeat | `repeat-limited(sphere(center=[0.4,0,0],radius=0.3),period=[1,0,0],count=[3,1,1])`, `-1,-0.5,-0.5,3,0.5,0.5`, 7 | 83,508 | `beaab71f7a10cd7c` |

The sphere is `test_domain_ops.cpp`'s off-centre child (the 17/07 defect's case).

## What changed

Both repeats keep the smallest of the copies they read with a strict `<`, so a copy whose
value cannot come out below the best so far changes nothing — not the value, not the copy
the gradient is read from. Skipping such a copy is exact. Three skips, all in
`src/implicit/domain_ops.cpp`:

1. **A copy already read.** A neighbour on an axis whose offset is zero, whose period is 0,
   or (limited) whose id clamps back onto the home id is the same point again.
2. **A copy whose box is too far** (the plan's lever b). `ImplicitField::boundsBoundValue()`
   is new (`include/dualc/implicit.h`, default `false`): outside `bounds()`, `valueAt` is at
   least the distance to `bounds()`. Sphere, box, capsule, a round cone with
   `|r1 − r2| < |b − a|` and a hard union of claimants claim it. When the child claims it,
   a copy is skipped when the point lies outside that copy's box by at least `best + tol`,
   `tol = 1e-9 · max(1, |box|, |period|)`, which covers the rounding of the two numbers
   compared. Design: [§ 10.1](../../design/08-implicit-field-layer.md#101-implicitfield).
3. **`cellOverlaps` asks the home tile first.** The 8-neighbour path ORs the child's
   `cellOverlaps` over up to 27 translated boxes; the predicate is pure, so the order is
   free. The box of the tile the cell's centre falls in now goes first. The visit set and
   the answer are unchanged — still the full 8-neighbour superset, which the superset test
   (`test_domain_ops.cpp`) pins. Tightening it would change the octree, so it is not done.

Counted on the octet at depth 7, with skips 1–2 in (a scratch counting build): 33.4 M
`nearestCopy` calls read **1.94 copies each** (64.9 M), 200.9 M skipped by the box, 1.8 M as
repeats. `cellOverlaps`: 293,640 calls, 4.67 M child calls (≈ 16 a call), 250,376 true.

`RepeatLimitedField` takes the same three. The GLSL mirror (`examples/field_glsl.cpp`) still
reads all 8 copies: the values are the same, and parity is **82/82**
(`check.py --gpu --strict --build-dir build-gl`, rebuilt first).

## The levers, kept and not

| lever (plan § T1) | outcome |
| --- | --- |
| (a) neighbours only on axes the child overhangs | **not kept as written**: on an axis the child fits, the neighbour copy can still hold the smaller value off the surface (a sphere at `(0.4, 0.15, 0)`, `r = 0.3`, periods `(1, 1, 0)`: at `y = −0.45` the home copy reads 0.30, the `−y` copy 0.10), so the minimum would change. What (a) is exact for is what (b) skips anyway. |
| (b) skip a copy whose box lower bound ≥ the best | **kept**, behind the new claim. The plan's `lipschitzBound()` does not exist, and Lipschitz-1 is the wrong property: it bounds how fast the value changes, not how small it is far away. |
| — home tile first in `cellOverlaps`, repeats skipped | **kept**; found by counting, not in the plan. |
| (c) a symmetry flag restoring the single fold | **not taken**: (a)+(b) leave 1.23× (P06.1's timing) to 1.30× (T2's, [below](#the-speed-up)), at the plan's "about 1.3×" bar. It could not be taken anyway, since the fold's mesh is not byte-identical (above). |

**Chosen in-run (pending owner review):** lever (b) needs a property no field had, so it is
a new public virtual, opt-in, defaulting to the safe answer. The alternatives were an
internal type check on known primitives in `domain_ops.cpp` (fragile across wrappers), or
no lever (b) at all (≈ 2.9× left).

## The levers as 04 planned them

Moved here verbatim from [04 § #35](04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation)
on 2026-10-21 for its size cap; the table above is what became of each.

> Two candidate levers, neither yet tried:
> - **Per-axis neighbours.** Visit offsets only on axes where the child actually overhangs.
>   Cheap and exact, but no help for the case that motivated it: a strut cell has corner caps
>   and so overhangs on all three axes.
> - **Early-out on the home copy.** Skip a neighbour when the home copy's value is already
>   smaller than the closest the neighbour's translated box could possibly be. This is where
>   the win would come from, since most points are nowhere near a seam.

## Measured after

Same host, command and thread count as the baseline:

| build | runs (s) | vs fold | output |
| --- | --- | --- | --- |
| skips 1–2 | 67.0 · 67.5 · 68.1 | 1.46× | identical to the octet reference |
| skips 1–3 (committed) | 55.6 · 56.9 · 57.4 | **1.23×** | identical to the octet reference |

All five other reference meshes come out byte-identical (`cmp`). Ctest's new
`boundsBoundValue claims hold outside the box` (`tests/test_primitives.cpp`) checks every
claim against its definition over 4,000 points around the box, and the cases that withhold
it. The field-level pruned-vs-unpruned test and the closing table are P06 T2's (below).

## Byte-identity, proven

P06 T2, 2026-10-21, on the committed code (`6a231c7`):

- **The meshes.** All six reference cases were re-run from the committed binary and compared
  with `cmp` to the `db50b2e` files: **all six identical**, so their SHA-256 matches the table above (the octet, bcc, tapered octet, the two seam spheres and the limited repeat); the 8-neighbour binary re-run gave the octet reference again.
- **The field.** `Pruned repeats equal the 8-neighbour minimum bit for bit`
  (`tests/test_domain_ops.cpp`) builds each repeat twice over the same child, through a
  pass-through probe that forwards the claim and one that withholds it, and compares both
  with `==` to a hand-written copy of `db50b2e`'s 8-copy loop (no skip at all): `valueAt`,
  `gradientAt` and `trueGradientAt`, at 15,625 grid points on multiples of period / 8
  (tile centres and seams, where the repeat skip fires) plus 4,000 pseudo-random ones. Seven
  cases: the seam sphere on one and two axes, the octet cell in capsules and in round cones,
  a `RoundBoxField` (no claim: only the repeat skip), and `repeatedLimited` over the sphere
  and the octet cell. On every claimant the probe counts fewer child reads with the claim
  than without, so the box skip fired; without a claim the counts are equal.
- **The superset test** (`Domain wrappers ask their child about a superset of what they
  read`) is untouched and green.

## The speed-up

Same host, command and `--threads 4` as the baseline; the three builds re-timed back to back
in one session, three runs each (load average 2.1–2.7 before and after, from the desktop and other sessions; nothing else heavy):

| build | runs (s) | median | vs fold |
| --- | --- | --- | --- |
| 8-neighbour (`db50b2e`) | 114.7 · 125.2 · 129.5 | 125.2 | 2.73× |
| single fold (scratch, as above) | 46.0 · 45.8 · 45.8 | 45.8 | 1 |
| **pruned (`6a231c7`, committed)** | 59.7 · 60.0 · 60.2 | 60.0 | **1.31×** |
| fold, interleaved with the next row | 45.0 · 45.5 · 46.1 | 45.5 | 1 |
| pruned, interleaved | 59.4 · 59.4 · 59.2 | 59.4 | **1.30×** |

The pruned repeat is **2.09× faster** than the 8-neighbour one (125.2 s → 60.0 s) and costs **1.30× the single fold**, down from 2.73×; peak memory is the 8-neighbour code's 3.35 GB (the fold peaks at 3.56 GB). The 2026-10-21 P06.1 runs above
(132.3 / 46.2 / 56.9 s) give the same ratios within the host's drift: 2.86× and 1.23× then; the same pruned binary ran 5 % slower today against an unchanged fold, which is why the interleaved rows were taken.

**Chosen in-run (pending owner review):** T2's 1.30× sits on the plan's "more than about
1.3×" bar for lever (c), not clearly under it as P06.1's 1.23× did. Lever (c) is still not
taken: the fold's octet mesh differs from the 8-neighbour one (above), so it cannot meet the
run's byte-identity rule, whatever the timing. Taking it would mean accepting its 3,452
differing OBJ lines as the new reference — the owner's call.

## Gates

`python3 scripts/check.py` green with ctest **@@CTEST@@** (C ABI in); `--docs --strict` green.
The GPU tier is P06.1's, **82/82**: T2 changed no field node or codegen.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
