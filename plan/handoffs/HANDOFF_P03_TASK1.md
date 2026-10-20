# Handoff — P03.1: the `gradientAt` contract text and the census

Unit P03.1 of [`plan/12_P03_gradient-contract.md`](../12_P03_gradient-contract.md) § T1; session
of 2026-10-20, branch `feat/hygiene-and-editor` (nothing pushed, no remote touched, `main`
untouched). Comments and docs only — no behaviour changed. The record is
[17/19](../../docs/roadmap/17-code-audit-and-hardening/19-gradient-contract.md) (the contract,
the census table with file:line, the callers, the gaps in the raw plan's census); this
handoff points at it and adds what P03.2 needs.

## What changed, and why

1. **Census** (step 1): every `gradientAt` override under `include/` and `src/` grepped at
   `db8ffef` — **30 in 3D** (+ 2 in the 2D layer); none in `examples/`, `capi/`, `tests/`.
   Classes: unit 11 (`PrimitiveField` FD for 34 primitives, sphere, plane, `MeshSource`,
   `GridField`, the 2 lifts, twist/bend/displace, `NormalizedField`), pass-through 13,
   blend-shortened 5 (3 smooth booleans, `mix`, `elongated`), raw FD 1 (`WindingNumberField`).
   Page [17/19](../../docs/roadmap/17-code-audit-and-hardening/19-gradient-contract.md), indexed
   in 17's README, dated pointer appended at the end of 17/04 § #34.
2. **Header** (step 2): `include/dualc/implicit.h` base `gradientAt` comment rewritten (unit
   outward direction; shorter only in smooth blends, `mix`, `elongated`'s flat band; (0,0,1)
   where undefined; not |∇f|, use `trueGradientAt()`), and `GridField`'s class comment
   likewise.
3. **Design** (step 3): [design 08 § 10.1](../../docs/design/08-implicit-field-layer.md#101-implicitfield)
   gains the contract paragraph; the `closestSurfacePoint` bullet says the step length is `f`
   (unit `g`), hence exact only for |∇f| = 1.
4. **Cap room:** 17's README was at 15,318 / 15,360 bytes; the #52 index entry (`16-…/`) was
   tightened (its enumeration of contents is in 16's own README page table) to fit the new
   row. Loss-audited (below).

**Chosen in-run (pending owner review)** — also on 17/19: the plan names "smooth blends and
`mix`" as the only places the length drops below 1; the census showed `ElongateField`
zeroes components of a unit vector too, so the header and design page name `elongated` as well.

## Measured, and under what

Linux, GCC 15, Ninja, Release, i7-920.

| What | Result |
| --- | --- |
| Full gate `python3 scripts/check.py` | 32 checks: 30 passed, 0 failed, 1 skipped (configure: already configured); 46 TUs compiled (the header touch), 0 DualC-origin warnings; **ctest 350 / 350**; exit 0 |
| `python3 scripts/check.py --docs --strict` | 28 checks, 27 OK + `index-staleness` INFO (report-only, pre-existing list), exit 0 |
| GPU tier | not run: no field node, `field_graph.cpp`, `field_glsl.cpp` or parity case moved |
| Loss audit | 1 intentional `partial` row on the touched pages (below) |

## Loss audit

Before the commit (17's README lost text): `scripts/docs_loss_audit.py --base HEAD --head
<scratch commit-tree of the staged index> --scope docs/ --untriaged --triage /tmp/…` with a
scratch triage (the repo's `scripts/docs_loss_audit.json` untouched). Whole tree: 10,035
untriaged rows — the baseline noise P01/P02 also saw (9,979 / ~9,975), on pages this unit
did not touch. On the three pages it did touch: **one `partial` row, intentional** — 17
README:40, the #52 index entry, tightened for cap room (the dropped enumeration — "the rc 2
sites ranked, the alternative hypotheses, … the decision criteria" — is verbatim in
16/README's page table). Every other row from those pages is `rewritten` at 0.000/1.000,
short unchanged lines (the same noise class as the baseline). Design 08 and 17/04 lost
nothing: their edits are additions plus one bullet extended in place.

## What I got wrong on the way

- Census table first carried `|p-c|` inside backticks in a table cell — an unescaped pipe
  splits the cell; reworded.
- First loss-audit runs: an empty `{"audits":{}}` triage is rejected (the tool needs the
  `db8ffef` audit key with its thresholds), and `--scope` must be `docs/` whole (narrow
  scopes fail with "base docs files neither in --scope nor an R100 rename"). The working
  recipe: copy the thresholds of the repo's `3d73220` audit into a scratch triage under
  key `db8ffef` with empty items, `--scope docs/ --source <page>`. Each source run takes
  minutes.
- The design paragraph first said `WindingNumberField` "still" returns raw FD — a history
  marker in the present-tense layer; reworded to "breaks it".

## Left open — for P03.2

- **The header names `trueGradientAt()` which does not exist yet** — P03.2 adds it (step 1).
  Until then the comment is a forward reference; the gate does not check it.
- **Design 08 § 10.1 must be updated by P03.2** (T2 does not list it): the contract
  paragraph says `NormalizedField` takes |∇f| by its own FD and the Newton step assumes 1,
  and that `WindingNumberField` breaks the contract; the `closestSurfacePoint` bullet gives
  the old step. All four sentences go false with T2. Also the census row of
  `WindingNumberField` and `NormalizedField` on 17/19, and 17/19's opening line ("P03.2 adds
  the true-gradient path and records it here").
- **Pass-through nodes forward the winding field's raw FD** — once winding is unit, every
  tree is on the contract; the property test (T2 step 6) should include a winding child
  under a pass-through node.
- The 2D `gradientAt` overrides are never called by the library (lifts FD their own
  `valueAt`); T2's "one FD helper" could leave them alone.
- **Cap room now:** 17 README **15,315** (45 bytes left — the #34 status cell edit at P03.2/P04
  needs a trim elsewhere first), 17/04 **15,087** (≈270 left), 17/19 ≈7.3 KB, design 08 ≈9.8 KB.
- Next free IDs unchanged: **D-67, #62, block 22**; block 17's next page is **20**.
