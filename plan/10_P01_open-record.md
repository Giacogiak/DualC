# P01 — Open the record

§ 1 and § 3 P01 of [the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md). Branch
`feat/hygiene-and-editor`. Never push.

### T1. Ledger entries, decision rows and the stale #53 cell

1. Check that you are on `feat/hygiene-and-editor`. Read `docs/decisions/README.md` and
   `01-settled.md` for the last `D-NN`, and the roadmap for the last `#N` and block number.
   On 2026-10-19 these were D-64, #61 and block 21. Never trust the plan's numbers over the files.
2. Append a dated entry (the run date) in
   `docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md`, under § #34 and under
   § #35. It says that this run takes up #34's three open bullets and implements #35 ahead of
   its trigger, by the owner's decision (raw plan § 1), and points to the raw plan. Append only;
   do not rewrite existing text.
3. Add two decision rows to `01-settled.md`, in the index's own style, citing 17 § #34 and the raw plan:
   - **Gradient contract.** `gradientAt` is a unit outward direction: shorter inside smooth
     blends, (0,0,1) at degenerate points. The true magnitude comes from a separate virtual.
     Rejected: having every override return ∇f (raw plan § 1). DONE by decision.
   - **Track 2 UI owned by DualCEditor.** The boundary is the one in raw plan § 1. DONE by
     decision. P07 opens the block that records it: cite "P07" in the row and leave the link
     for P07 to fill.
4. Fix the stale `#53 PLANNED` in the block-14 row of `docs/roadmap/README.md` § Principal
   blocks, so it matches `docs/roadmap/14-c-abi/README.md` (PARTIAL).
5. Check that `docs/raw/README.md` indexes the raw plan (it should already).

#### Acceptance criteria

- The two decision rows exist with new IDs, and the 04 page has its dated entries.
- `python3 scripts/check.py --docs --strict` is green, everything is committed, and the handoff lists every new ID.

## Exit criteria

- The record is open on the branch, `--docs --strict` is green, and nothing is pushed.
