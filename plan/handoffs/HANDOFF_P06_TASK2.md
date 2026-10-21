# Handoff — P06.2: byte-identity proven, the speed-up recorded, #35 closed

Unit P06.2 of [`plan/15_P06_repeat-pruning.md`](../15_P06_repeat-pruning.md) § T2, the last
unit of P06. Session of 2026-10-21, branch `feat/hygiene-and-editor`, start commit
`5eaf534`. Nothing pushed, no remote touched, `main` untouched. No library code changed in
this unit: a test, the record and the status lines. The record is
[17/21](../../docs/roadmap/17-code-audit-and-hardening/21-repeat-pruning.md), now with
§ Byte-identity, proven, § The speed-up and § Gates. This handoff points at it and adds what
P08 needs.

## Outcome in one paragraph

All six reference meshes from the committed code (`6a231c7`) are byte-identical to the
`db50b2e` references (`cmp`; SHA-256 equal to 17/21's table). A new Catch2 case proves the
same thing at field level: pruned, unpruned and a hand-written copy of the old 8-copy loop
agree with `==` on `valueAt`, `gradientAt` and `trueGradientAt`. Two mutations of the
pruning turned it red. Re-timed today on the same host and counts, the pruned repeat is
**2.09× faster** than the 8-neighbour one (125.2 s → 60.0 s, medians) and costs **1.30×**
the single fold. P06.1 measured 1.23×. #35 is **DONE** in the 17 table, in 04 and in the
roadmap README. The gate is green with ctest **369 / 369**, and `--docs --strict` passes.

## What changed, and why

1. **Test** `Pruned repeats equal the 8-neighbour minimum bit for bit`
   (`tests/test_domain_ops.cpp`, `[domain][repeat]`). How it works:
   - `ClaimProbe` is a pass-through child. It forwards everything and claims
     `boundsBoundValue` only when told to. It counts child reads.
   - Each case builds `repeated` / `repeatedLimited` twice over the same child: once through
     a probe that forwards the claim (pruned), once through one that withholds it (box skip
     off).
   - Both are compared with `==` to `eightCopyMin`, a hand-written copy of `db50b2e`'s loop
     with no skip at all.
   - Points: a 25³ grid on exact multiples of period / 8, which lands on tile centres (where
     the repeat skip fires) and on seams, plus 4,000 splitmix64 points.
   - Seven cases: the seam sphere on one axis and on two, the octet cell as capsules and as
     round cones (built from `dce::strutCellSegments` the way `makeStrutLattice` builds it),
     a `RoundBoxField` that claims nothing, and `repeatedLimited` over the sphere and over
     the octet cell.
   - It also requires fewer reads with the claim than without on every claimant, so the skip
     really fired. On the non-claimant the counts must be equal.
2. **Shown to fail** (scratch edits, reverted with `git checkout`):
   - Box skip at `0.5·best`: red on 3 cases, the spheres.
   - Repeat skip forced on for x (`sameX = true`): red on 3 cases, including the round box.
   - The octet cases did not catch the `0.5·best` mutation. The sphere cases did.
3. **Docs**:
   - 17/21 gained § The levers as 04 planned them (moved verbatim from 04 for its cap),
     § Byte-identity, proven, § The speed-up and § Gates. The lever (c) row now cites both
     timings.
   - 04 § #35: **DONE** 2026-10-21 on the status line, the lever bullets replaced by a
     pointer to 21's verbatim copy, and a dated `2026-10-21` pointer at the end.
   - 17 README: the #35 row is DONE ("~1.3× the fold, exact", linked to 21). Latest
     verification now reads ctest **369** → [21 § Gates]. Two phrases on line 21–22 were
     shortened to fit the cap (the content is kept).
   - Roadmap README Track 1: "**NEXT:** #35 on its trigger" became "#35 DONE 2026-10-21;
     **NEXT:** **#53** …".
   - cmd-ref 11/01 `repeat` row: "~1.2×" became "~1.3× the single fold".

## Measured, and under what

The host was Linux (Ubuntu), GCC 15.2, i7-920 (4 c / 8 t), Release with the gate's options.
Each run was `dualc_field --expr "octet(wavelength=0.4,radius=0.05)" --bounds -1,-1,-1,1,1,1
--depth 7 --threads 4 -o …obj`, timed by `/usr/bin/time`. The three binaries were P06.1's:
`/tmp/p06/ref-bin/dualc_field` (`db50b2e`), `/tmp/p06/fold-build` (the fold forced) and
`build/examples/dualc_field` (`6a231c7`, `cmp`-equal to P06.1's `pruned-bin`). The load
average was 2.1–2.9 throughout, from the desktop and other sessions.

| build | runs (s) | median | vs fold |
| --- | --- | --- | --- |
| 8-neighbour | 114.7 · 125.2 · 129.5 | 125.2 | 2.73× |
| fold | 46.0 · 45.8 · 45.8 | 45.8 | 1 |
| pruned | 59.7 · 60.0 · 60.2 | 60.0 | 1.31× |
| fold, interleaved | 45.0 · 45.5 · 46.1 | 45.5 | 1 |
| pruned, interleaved | 59.4 · 59.4 · 59.2 | 59.4 | 1.30× |

Gates:
- `python3 scripts/check.py`: 32 checks, 30 passed, 0 failed, 1 skipped. ctest **369**,
  0 DualC-origin warnings.
- `--docs --strict`: PASS (28 checks).
- No GPU tier: no field node, `field_graph.cpp`, `field_glsl.cpp` or `parity_expected_cases`
  moved. P06.1's 82 / 82 stands.

## Exit criteria — one bullet at a time

- **"#35 closed in 17 with its measurements":** closed. The 17 README table row reads
  **DONE** 2026-10-21 → 21. 04's status line and dated line say the same, and so does the
  roadmap README. The measurements are 17/21 § The baseline, § Measured after (P06.1) and
  § The speed-up (this unit).
- **"nothing pushed":** `git push` was never run, and no remote was added or changed. The
  commits are local on `feat/hygiene-and-editor`.

T2's acceptance criteria:
- **Meshes byte-identical:** all six identical (`cmp`), and the field-level test is green.
- **Speed-up recorded:** 17/21 § The speed-up.
- **#35 DONE:** see the first exit bullet.
- **Gates green, committed, not pushed:** yes.
- **The superset test** (`Domain wrappers ask their child about a superset of what they
  read`) is untouched and green inside the 369.

## Chosen in-run (pending owner review) — for P08

- **The 1.3× bar.** The plan takes lever (c) "only if (a)+(b) still leave more than about
  1.3×". P06.1 measured 1.23×. Today's re-timing is 1.30–1.31×, which sits on the bar.
  Lever (c) is still not taken, because the fold's octet mesh differs from the 8-neighbour
  one (3,452 OBJ lines), so it cannot meet the run's byte-identity rule. Taking it would
  mean accepting a new reference, which is the owner's call. This is written in 17/21
  § The speed-up.
- **The lever text moved from 04 to 21 verbatim**, so 04 has room for the DONE pointer
  under its 15 KB cap. It follows the 2026-09-18 practice in 04 § #34.
- **The page.** The plan asks for "a new child page of block 17". P06.1 had already created
  17/21 for #35, so the speed-up table went there rather than into a 17/22.

## What I got wrong on the way

- **A wrong first interleaved set.** It ran with `/tmp` (tmpfs, 12 GB) nearly full of
  P06.1's scratch OBJs. Every write failed and left empty files, and the times were
  meaningless (27–56 s). I deleted `/tmp/p06/runs/*.obj`; the references in `/tmp/p06/ref`
  and `/tmp/p06/t2/out` were kept. Then I re-ran the set and gated it with a `cmp` against
  the reference. Only the second set is recorded.
- **Expecting today's pruned time to match P06.1's 56.9 s.** It ran at 60.0 s against an
  unchanged fold (45.8 vs 46.2 s). That ≈ 5 % drift on the same binary is why the
  interleaved rows were taken.

## Loss audit

The audit ran per touched page, each in a foreground call of about 5.5 min:
`--base 5eaf534 --head <scratch commit-tree of the staged tree> --scope docs/ --source <page>
--untriaged --triage /tmp/p06/t2/triage.json` (keyed `5eaf534`, the `3d73220` thresholds).
Tree-wide untriaged is **10,059**, the known noise.

Rows on lines this unit edited, all intentional:
- 04 :188, "Two candidate levers, neither yet tried:": matched in 21's verbatim quote.
- 17 README :21: the shortened finding-id sentence. Its content is kept.
- 17 README :155: the #35 row. "on 2026-09-10" was dropped there; the date stays in the
  README's :111 milestone list and in 04.
- cmd-ref 01 :166, token-only: "~1.2×" became "~1.3×".

The other flagged rows are on untouched lines: 17 README :46–:147; 21 :32, :43, :89;
roadmap README :21, :108, :119; cmd-ref 01's other rows. Every `^-` line of
`git diff -U0 -- docs/` was also checked by hand: the cmd-ref row, 04's status line and
lever bullets, 21's lever (c) row and closing sentence, the three 17 README lines and the
roadmap README line.

## Left open — for P08 and the owner

- **The lever (c) bar** (above).
- **P08's semantic lint:** P06.1 edited both `src/` and `docs/design/` (08 § 10.1, § 10.4).
  That still calls for a `/docs-semantic-lint` delta on 08. This unit touched neither.
- **Cap room is nearly gone:**
  - 17 README: 15,359 / 15,360 bytes. The next status edit there needs a split or a
    condensation.
  - Roadmap README: 15,319.
  - 04: 15,259.
- **The recent-milestones window** in the roadmap README has no rows for the hygiene run
  yet. That is P08's harvest.
- **The GLSL mirror** still reads all 8 copies. Parity holds; pruning it is not planned.
- Next free IDs are unchanged: **D-67, #63, block 22**. Block 17's next page is **22**.
- Scratch, safe to delete after P08: `/tmp/p06/` (references, binaries, scratch builds,
  audit outputs).
