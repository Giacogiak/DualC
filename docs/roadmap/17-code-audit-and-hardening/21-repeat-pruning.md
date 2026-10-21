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
| (c) a symmetry flag restoring the single fold | **not taken**: (a)+(b) leave 1.23×, under the plan's 1.3× bar. It could not be taken anyway, since the fold's mesh is not byte-identical (above). |

**Chosen in-run (pending owner review):** lever (b) needs a property no field had, so it is
a new public virtual, opt-in, defaulting to the safe answer. The alternatives were an
internal type check on known primitives in `domain_ops.cpp` (fragile across wrappers), or
no lever (b) at all (≈ 2.9× left).

## Measured after

Same host, command and thread count as the baseline:

| build | runs (s) | vs fold | output |
| --- | --- | --- | --- |
| skips 1–2 | 67.0 · 67.5 · 68.1 | 1.46× | identical to the octet reference |
| skips 1–3 (committed) | 55.6 · 56.9 · 57.4 | **1.23×** | identical to the octet reference |

All five other reference meshes come out byte-identical (`cmp`). Ctest's new
`boundsBoundValue claims hold outside the box` (`tests/test_primitives.cpp`) checks every
claim against its definition over 4,000 points around the box, and the cases that withhold
it. The field-level pruned-vs-unpruned test and the closing table are P06 T2's.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
