# Handoff — P06.1: the baseline, the reference meshes, the pruned neighbour set

Unit P06.1 of [`plan/15_P06_repeat-pruning.md`](../15_P06_repeat-pruning.md) § T1. Session of
2026-10-21, branch `feat/hygiene-and-editor`, start commit `db50b2e`, code commit `6a231c7`.
Nothing pushed, no remote touched, `main` untouched. The record is the new
[17/21](../../docs/roadmap/17-code-audit-and-hardening/21-repeat-pruning.md): baseline,
reference meshes with their hashes, what changed, the levers kept and not, the numbers after.
This handoff points at it and adds what T2 and P08 need.

## Outcome in one paragraph

Both repeats now skip the copies that provably cannot win their strict `<` minimum, so the
output is **bit for bit the 8-neighbour minimum**: octet at depth 7 went from **132 s to
57 s** (`--threads 4`), **1.23× the single fold's 46 s**, down from 2.86×. All six reference
meshes come out byte-identical (`cmp`). The gate is green (ctest **368 / 368**). The GPU tier
is green too (parity **82 / 82**). Lever (c) was not needed, and could not have been taken
anyway: the fold's mesh is not byte-identical.

## What changed, and why

1. **`ImplicitField::boundsBoundValue()`** (`include/dualc/implicit.h`), a new public
   virtual, default `false`. It claims that outside `bounds()`, `valueAt` ≥ the distance to
   `bounds()`. These fields claim it:
   - `SphereField`;
   - `BoxField` (when its half-extents are ≥ 0);
   - `CapsuleField` (when `r ≥ 0`);
   - `RoundConeField` (when `r1, r2 ≥ 0` and `|r1−r2| < |b−a|`, or `a == b`);
   - the hard `UnionField`, when both operands claim it on valid boxes.

   This is lever (b)'s certificate. The plan's `lipschitzBound()` does not exist, and
   Lipschitz-1 is the wrong property anyway: it bounds the slope, not how small the value is.
2. **`src/implicit/domain_ops.cpp`**:
   - `CopyPruner` caches the child's box and claim. It skips a copy when the point is outside
     that copy's box by ≥ `best + tol`, with `tol = 1e-9·max(1, |box|, |period|)`.
   - Both `RepeatField::nearestCopy` and `RepeatLimitedField::evaluate` also skip a copy
     already read: a zero offset, a period-0 axis, or (limited) an id that clamps back home.
   - Both `cellOverlaps` now go through `anyTileOverlaps`, which asks the home tile's box
     first. The answer is the same OR over the same boxes, so the octree does not change.
3. **Test**: `boundsBoundValue claims hold outside the box` (`tests/test_primitives.cpp`)
   checks each claim against its definition over 4,000 pseudo-random points, plus the cases
   that must not claim it. This is a safety pin for (b). It is not T2's pruned-vs-unpruned
   test.
4. **Docs**:
   - design 08 § 10.1 has the new virtual: "3 + 6 overridable", its paragraph, and § 10.4's
     repeat bullet.
   - cmd-ref 11/01's `repeat` row: "~8×" became "~1.2× the single fold".
   - 17/21 is new and indexed in the 17 README; the 07 entry was condensed to make room.

## Measured, and under what

Linux, Ubuntu, GCC 15.2, i7-920 (4 c / 8 t), Release with the gate's options (C ABI on, PIC).
`dualc_field --expr "octet(wavelength=0.4,radius=0.05)" --bounds -1,-1,-1,1,1,1 --depth 7
--threads 4 -o …obj`, `/usr/bin/time`, three runs each, nothing else heavy running:

| build | runs (s) |
| --- | --- |
| 8-neighbour, `db50b2e` | 129.14 · 132.25 · 133.43 |
| fold forced, scratch `/tmp/p06/fold-build` | 46.64 · 46.20 · 45.79 |
| skips 1–2 only | 67.03 · 67.46 · 68.12 |
| **committed (skips 1–3)** | **55.57 · 56.94 · 57.44** |

A scratch counting build ran with skips 1–2:
- `nearestCopy` reads 1.94 copies a call (64.9 M over 33.4 M calls).
- The box skip removed 200.9 M copies, and repeats 1.8 M more.
- `cellOverlaps`: 293,640 calls made ≈ 16 child calls each (4.67 M), and 250,376 calls
  answered true.

Those counts are why the home-tile-first order went in. Reference hashes are in 17/21; the
files are in `/tmp/p06/ref/`, and the shipped binary is copied at
`/tmp/p06/ref-bin/dualc_field`. Both are scratch: if `/tmp` is gone, rebuild `db50b2e`, and
the hashes still decide.

Gates:
- `python3 scripts/check.py`: 32 checks, 29 passed, 0 failed, 2 skipped (configure, already
  configured; warnings, incremental). The ninja output of every build in this unit was
  grepped for `warning:` and `error`, and there were none.
- `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict --build-dir build-gl`:
  parity 82 / 82.
- `--docs --strict`: 28 checks, PASS.

## Chosen in-run (pending owner review) — for P08

- **Lever (b) is a new public virtual**, opt-in with a safe default. The alternatives were:
  - an internal type check on known primitives inside `domain_ops.cpp`, which is fragile
    across wrappers and invisible to users' own fields;
  - no lever (b), which leaves ≈ 2.9×.
- **Lever (a) not kept as written.** On an axis the child fits, a neighbour copy can still
  hold the smaller value off the surface. 17/21 has the numbers: a sphere at (0.4, 0.15, 0),
  r 0.3, periods (1, 1, 0), at y = −0.45 reads 0.30 at home and 0.10 next door. So (a)
  would break value identity. Its exact part is what (b) skips anyway.
- **An unplanned exact lever was kept: home tile first in `cellOverlaps`.** It took
  67.5 s → 56.9 s.
- **More reference cases than the plan's three.** The plan asks for octet, BCC and the
  seam sphere; I added the seam sphere on two axes, a `repeat-limited` and a tapered octet
  (round cones), so every new code path has a mesh.

## What I got wrong on the way

- My first test used a braced list of mixed pointer types in a range-for, and it did not
  compile. It became a `std::vector<const ImplicitField*>`.
- My first `fold-build` configure lacked `-DDUALC_BUILD_C_ABI=ON`, so it was built without
  PIC, unlike the gate's `build/`. It was re-configured before any timing run, so all
  timings share the same flags.
- Before the counts I expected the box skip alone to land near 1.2×. It landed at 1.46×,
  because the neighbour path's `cellOverlaps` asked ≈ 16 boxes a cell. The home-first order
  fixed that.

## Loss audit

The audit ran per touched page, each in a foreground call of about 5 min:
`--base db50b2e --head <scratch commit-tree of the staged tree> --scope docs/ --source <page>
--untriaged --triage /tmp/p06/triage.json` (keyed `db50b2e`, the `3d73220` thresholds).
Tree-wide untriaged is **10,054**, the known noise.

Rows on lines this unit edited, all intentional:
- 17 README :49, partial + gone: the condensed 07 entry. The full text is in 07 itself, and
  "both strut-lattice claims corrected in [15]" is kept.
- cmd-ref 01 :166, rewritten: "~8×" became the measured cost. The 8× history is in 17/07.
- design 08 :14, rewritten: "5 overridable" became "6".

The other flagged rows sit on untouched lines (17 README :46, :76, :84, :142, :147; design 08
:22, :57; cmd-ref 01 :33, :36, :116, …). Every `^-` line of `git diff -U0 -- docs/` was also
checked by hand. They are the cmd-ref row, the "5 overridable" line, the reflowed "sharp)
and distortions" line (its content is kept) and the 07 index entry.

## Left open — for T2, P08 and the owner

- **T2's field-level test** needs an unpruned twin. Wrap the claimant in a test-local
  pass-through `ImplicitField`: it forwards `valueAt` / `gradientAt` / `trueGradientAt` /
  `bounds` / `cellOverlaps` but not the claim. Then compare `repeated(claimant)` with
  `repeated(wrapper)` using `==` on `valueAt` and `gradientAt`. The duplicate skip (1) is
  active in both, but it is exact by construction: the same point and the same value.
  To cover it too, compare against a hand-written 8-copy loop.
- **T2's byte-identity check**: re-run `/tmp/p06/refs.sh <bin> <dir>` and `bench.sh`, or
  rebuild from the commands in 17/21, then `cmp` / `sha256sum` against 17/21's hashes. The
  committed code already matched all six in this unit.
- **Cap room.** 04 is at 15,312 / 15,360 bytes, so T2's "dated pointer in 04 § #35" must
  replace text there. The 17 README is at 15,353 / 15,360. 17/21 is ≈ 7 KB, so the speed-up
  table fits there.
- **#35's status is still PLANNED** in 04, in the 17 README table and in the roadmap README.
  T2 flips it to DONE with the evidence.
- **P08's semantic lint**: this unit edited both `src/` and `docs/design/` (08 § 10.1 and
  § 10.4). That calls for a `/docs-semantic-lint` delta on 08.
- The GLSL mirror still reads 8 copies (the same values, so parity holds). Pruning it is
  not planned.
- Next free IDs are unchanged: **D-67, #63, block 22**. Block 17's next page is **22**.
