# Handoff — P03.2: the true-gradient path, the Newton step, one FD helper

Unit P03.2 of [`plan/12_P03_gradient-contract.md`](../12_P03_gradient-contract.md) § T2, the
last unit of P03; session of 2026-10-20, branch `feat/hygiene-and-editor` (nothing pushed,
no remote touched, `main` untouched). The record is
[17/19 § The true-gradient path](../../docs/roadmap/17-code-audit-and-hardening/19-gradient-contract.md#the-true-gradient-path)
(override table with file:line, the Newton numbers, the tests, the in-run choices, the
gates). This handoff points at it and adds what P04 needs.

## What changed, and why

1. **The virtual** (step 1): `virtual Vector3 ImplicitField::trueGradientAt(const Vector3&) const`
   in `include/dualc/implicit.h`, documented there. Default
   (`src/implicit/implicit_field.cpp`): central FD of `valueAt`, e=1e-4, magnitude kept —
   the code that was `NormalizedField::trueGradMag`. Analytic overrides: `SphereField`
   (unit radial, zero at the centre), `PlaneField` (its normal), `GridField` (the trilinear
   gradient before normalisation — `gradientAt` now normalises `trueGradientAt`, the same
   floating-point ops as before). Forwarded, chain rule applied: offset, onion (`sign(f)`),
   transform (rotation), scale (`sign(s)`), and — beyond the plan's "decorators" — mirror,
   repeat, repeatLimited and the four hard booleans (in-run choice below).
2. **`NormalizedField`** (step 2): `valueAt` divides by `|child->trueGradientAt(p)|`;
   `trueGradMag` deleted. `test_tpms.cpp`'s regression ("divides by the true gradient
   magnitude") untouched and green.
3. **Newton step** (step 3): `closestSurfacePoint` steps `q - G·f/|G|²`, `G =
   trueGradientAt(q)`, normal at the result from `gradientAt`; returns false when `G`
   vanishes or `f`/`G` are not finite. Numbers below.
4. **`WindingNumberField`** (step 4): its straddle-aware FD is now `trueGradientAt`;
   `gradientAt` is its unit direction, `(0,0,1)` below 1e-12. Every tree is on the contract
   now.
5. **One FD helper** (step 5): `src/internal/fd_gradient.h` (header-only, new STRUCTURE.md
   row): `centralDifferences` (undivided), `fdGradientDirection` (the merged `fdGradient`
   copies of `domain_ops.cpp` and `lift.cpp`, same ops, so bit-identical) and
   `fdTrueGradient` (the default). `PrimitiveField::gradientAt` stays inline in the public
   header (it cannot include `src/internal/`); the 2D layer is untouched.
6. **Tests** (step 6): new `tests/test_gradient.cpp`, 10 cases tagged `[gradient]` — listed on
   17/19. The property case covers every catalogue primitive, TPMS, strut lattice, the mesh,
   winding, grid and lift sources, and every combinator/decorator/domain op over a gyroid
   and over a winding child (the handoff's ask): `|gradientAt| ≤ 1 + 1e-9` everywhere,
   `= 1 ± 1e-9` outside the smooth booleans, `mix` and `elongated`, at 343 points each.
7. **Docs:** design 08 § 10.1 (the virtual in the code block, "5 overridable", a
   `trueGradientAt` paragraph replacing the sentences P03.1 flagged as going false, the
   Newton bullet), design 10's central-difference row (now names `fd_gradient.h`), 17/19
   (census rows for winding/normalize/the FD copies annotated, totals since P03.2, the new
   section, gates), 17/04 § #34 (the contract bullet DONE 2026-10-20, the `GridField` bullet
   "next, P04", status line date), 17 README (index row, #34 status cell gains
   `10-20 (gradient, [19])`).

## Measured, and under what

Linux, GCC 15, Ninja, Release, i7-920.

| What | Result |
| --- | --- |
| Newton, gyroid λ=1, 53 lattice points with \|f\| ≤ 0.3 | mean \|f\| after the step **9.6e-4** new vs **0.953** old; new < old at every point |
| Newton, winding cap, open tetrahedron edge 4 (\|∇w\| ≈ 0.25), 24 points | **3.2e-3** vs **0.054**; new < old at every point |
| Newton, same, edge 0.25 (\|∇w\| ≈ 4) | **3.2e-3** vs **0.190**; new < old at every point |
| Newton, same, edge 1 (\|∇w\| ≈ 1.013) — measured once, not a test | 3.2e-3 vs **1.7e-3**: old wins at 10 of 24 points |
| Red proof | old Newton step + raw winding `gradientAt` restored → 4 of 10 new cases fail (both Newton cases, the winding-unit case, the property); restored, all pass |
| Existing suite after the change, before the new tests | ctest 350 / 350 |
| Full gate `python3 scripts/check.py` | 32 checks: 30 passed, 0 failed, 1 skipped (configure); final run after touching every changed source: 47 TUs compiled, 0 DualC-origin warnings; **ctest 360 / 360**; exit 0 |
| GPU tier `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict --build-dir build-gl` | **parity 82 / 82**, exit 0 |
| `python3 scripts/check.py --docs --strict` | 28 checks, 27 OK + `index-staleness` INFO (report-only, pre-existing list), exit 0 |

## Exit criteria of P03, one by one

- **"04 records #34's two gradient bullets: the contract DONE; the `GridField` normal next,
  in P04."** — 17/04 § #34: the `gradientAt` bullet is struck through, "**DONE 2026-10-20**…
  record: [19]"; the `GridField` bullet ends "— next, P04 of the run." Checked by reading the
  page; `--docs --strict` green.
- **"Nothing is pushed."** — `git branch -vv`: `feat/hygiene-and-editor` has no upstream;
  no `git push` run, no remote added or changed.
- T1's criteria still hold (census page indexed, header + design state the contract).
  T2's: `trueGradientAt` exists with its tests; `NormalizedField` and `closestSurfacePoint`
  use it; full gate and `--gpu` green with the counts above, also on 17/19's Gates.

## Loss audit

Run before the commit, P03.1's recipe: `scripts/docs_loss_audit.py --base 1f20a24 --head
<scratch commit-tree of the staged index> --scope docs/ --untriaged --triage /tmp/…` (scratch
triage under key `1f20a24` with the `3d73220` thresholds; the repo's triage untouched), one
`--source` run per touched page. Whole tree: 10,056 untriaged, the baseline noise P01–P03.1
saw (9,979 / ~9,975 / 10,035), mostly `rewritten` 0.000/1.000 rows on short lines that did not
change. On the five touched pages, every `partial`/`gone` row is **intentional**:

| Row | Page:line | What went, and where the fact is now |
| --- | --- | --- |
| `58375ffe6e4d`, `e12c822700d3` partial | design 08:29 | "a caller that needs \|∇f\| cannot get it here…", "`WindingNumberField` breaks it" — false since this unit; replaced by the `trueGradientAt` paragraph; the old state is on 17/19's census |
| `9650e31516c1`, `d80de0737d8f`, `fa5bc5071e6c` gone | design 08:76 | the old Newton bullet (`q - g·(f/\|g\|)`, "step length is `f`", "false only when the gradient is degenerate") — replaced in place; the old step is described on 17/19 and in `tests/test_gradient.cpp`'s `oldStep` |
| `03746bc5cfe9` partial | design 10:176 | the central-difference row's list of FD sites — rewritten to name `fd_gradient.h` |
| `94bfbe58ee5e` partial | 17 README:47 | 19's index entry, tightened for cap room |
| `bccece3f08f2` partial | 17 README:155 | #34 status cell: "unknown keys" dropped for cap room — verbatim in the same README's 2026-09-11 milestone line |
| `60a7aeebcb35` partial | 17/04:163 | the `GridField` bullet's "callers assuming a true gradient magnitude (such as the Newton step) silently rely on" — no longer true; reworded to "relied on until … `trueGradientAt` (2026-10-20)"; the audit's original wording is in `docs/raw/study/` (the § #34 *Source:* line) |
| `cce3e6075513` gone | 17/19:68 | "So the contract holds for every tree that has no winding node…" — rewritten as the `db8ffef` totals plus "since P03.2" |

(The first pass printed only the last 25 lines per page and dropped rows; the four pages it cut
were re-run in full — the table is from the full runs.)

## What I got wrong on the way

- First winding Newton test used the unit-size tetrahedron: its cap has |∇w| ≈ 1.013, and
  the old step won at 10 of 24 points. Unit size is near-metric there by accident. w is
  scale-invariant, so the test now runs at edge 4 and 0.25. The edge-1 case is recorded,
  not hidden.
- The gyroid Newton test first sampled a 5³ lattice and found only 16 near-surface points
  (< the 20 asserted); now 7³ → 53.
- The first draft of design 08's Newton bullet claimed "one to three orders of magnitude
  closer". The winding cases are 17–60×, and at |∇w| ≈ 1 the old step can win. Reworded to
  the measured numbers.
- A table cell on 17/19 again carried an unescaped `|∇f|` (P03.1's mistake); escaped.

## Left open — for P04 and P08

- **P04** (the `GridField` normal): `GridField::trueGradientAt` is the raw trilinear
  gradient, discontinuous across cell faces exactly like `gradientAt`. The Newton step no
  longer reads `gradientAt`'s magnitude (only the normal at the projected point), so P04's
  continuous normal is now only about the normal's direction. The 17/04 bullet was reworded
  to say so.
- **Chosen in-run (pending owner review)**, also on 17/19: (a) hard booleans, mirror and the
  repeats forward `trueGradientAt` too; the graded decorators do not (their `gradientAt`
  drops ∇t); (b) sphere's `trueGradientAt` is zero at the centre; (c) the winding Newton
  test at edge 4 / 0.25, not 1; (d) `MeshSource` keeps the FD default (forwarding its unit
  `gradientAt` would be exact off the surface and 6 BVH queries cheaper; not in the plan).
- **`MeshSource` / `normalizedOf(mesh)` cost** is unchanged (6 BVH queries per `valueAt`), as
  before. It is a candidate for (d).
- **17 README "Latest verification"** still quotes ctest 302 (CI, 2026-10-07). The counts
  since (350 at P02, 360 now) are local and on the record pages. CI's count after the push is
  Giacomo's, and P08's harvest is where it belongs.
- **Cap room now:** 17 README **15,356** / 15,360 (4 bytes; the #34 status cell needed
  "unknown keys" dropped, which is verbatim in that README's 2026-09-11 milestone line);
  design 10 **15,357** (3 bytes); 17/04 15,279; design 08 10,924; 17/19 ≈12.3 KB.
  **P04's edit to the #34 status cell (DONE) has no room**: it must trim the cell as it
  rewrites it (the batch list can shrink to the DONE date + record links).
- **P08's semantic lint**: this session edited both `src/` and `docs/design/` (08, 10).
- Next free IDs unchanged: **D-67, #62, block 22**; block 17's next page is **20**.
