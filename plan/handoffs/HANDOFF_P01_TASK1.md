# Handoff — P01.1: ledger entries, decision rows and the stale #53 cell

Unit P01.1 of [`plan/10_P01_open-record.md`](../10_P01_open-record.md) § T1; session of
2026-10-19, branch `feat/hygiene-and-editor` (nothing pushed, no remote touched, `main`
untouched). Docs only — no source, test, CMake or gate-configuration file changed. The record
is roadmap 17's ledger ([04](../../docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md))
and the [settled decisions](../../docs/decisions/01-settled.md); this handoff points at them and
adds what the next units need.

## New IDs

| ID | Where | What |
| --- | --- | --- |
| **D-65** | `docs/decisions/01-settled.md` | `gradientAt` is a unit outward direction (shorter inside smooth blends, (0,0,1) at degenerate points); the true magnitude comes from a separate virtual; rejected: every override returning ∇f. DONE (decided; P03 executes). Cites 17/04 #34 + raw plan § 1. |
| **D-66** | `docs/decisions/01-settled.md` | All UI is DualCEditor's (Track 2), the boundary of raw plan § 1. DONE (decided; P07 records it). Cites raw plan § 1 and "P07's block" — **the link is P07's to fill**. |

No `#N` and no roadmap block was opened. IDs read from the files at the start: last `D-64`
(`01-settled.md`), last `#61` (17 #61), last block `21` — as the plan said. **Next free after
this unit: D-67, #62, block 22.**

## What changed, and why

1. **Branch checked:** `feat/hygiene-and-editor`.
2. **17/04 dated entries (append only).** `*2026-10-19:*` lines at the end of § #34 (the run takes
   up the three open bullets by the owner's decision, the contract stays a unit direction,
   D-65) and of § #35 (implemented ahead of its trigger by the owner's decision, measured,
   levers in the plan's order, byte-identical). Both link the raw plan.
3. **Cap relief on 17/04, first.** The page stood at **15,355 / 15,360** content bytes: no
   room for the entries. The #33 entry's "full original scope" paragraph (15 lines) moved
   **verbatim** into [12 § #33](../../docs/roadmap/17-code-audit-and-hardening/12-engineering-quality-records.md#33--build-hardening-three-batches);
   04's #33 lead sentence now says where it went. **Chosen in-run (pending owner review):**
   this move instead of D-37's folder split of 04 — written also in 12's note. Why: I did the
   split first (04 → `04-engineering-quality/` with README + three children, every inbound
   link rewritten) and the gate refused it twice: three anchored links in the immutable
   `docs/raw/2026-10-05-hosted-ci-plan.md` (lines 26–28) went broken, and the audit README
   rose to 15,501 bytes from the longer link paths. The split was reverted entirely; the move
   follows the 2026-10-07 precedent (#33 text moved into 12 when #32's close did not fit 04).
4. **Decision rows** D-65, D-66 appended to `01-settled.md` in its style. The page had 896
   bytes free; the rows were tightened three times to land at **15,358**.
5. **Stale #53 cell:** `docs/roadmap/README.md` § Principal blocks, block-14 row, `#53 PLANNED`
   → `#53 PARTIAL`, matching `14-c-abi/README.md` ("PARTIAL: … the CI proof owed").
6. **`docs/raw/README.md`** already indexes the raw plan (row 27) — checked, unchanged.

## Measured, and under what

Content bytes against the 15,360 cap (`scripts/check_data.json` `size_caps`), at commit time:

| Page | Before | After |
| --- | --- | --- |
| `17/04-engineering-quality.md` | 15,355 | 14,741 |
| `17/12-engineering-quality-records.md` | 6,884 | 8,744 |
| `decisions/01-settled.md` | 14,464 | 15,358 |
| `roadmap/README.md` | 15,123 | 15,123 (same length cell) |

`python3 scripts/check.py --docs --strict`: 28 checks, 27 OK + 1 INFO (`index-staleness`, 18
pre-existing entries from earlier sessions, report-only), exit 0. Full `python3 scripts/check.py`
on the same docs content (finished just after the commit `95fcc94`): 32 checks, 29 passed, 0
failed, 2 skipped (configure: already configured; warnings: incremental build, 0 TUs
compiled), **ctest 344 / 344**, exit 0 — Linux, GCC, Ninja, Release, `build/`.

Loss audit: `scripts/docs_loss_audit.py --base HEAD --head ece6830 --scope docs/` (`ece6830` a
scratch commit of the tree, never on the branch) was started before the commit but had **not
finished** (over 3 minutes on all of `docs/`) when the session was closed. In its place, a
mechanical check: the 15 lines removed from 04 are byte-identical to the 15 lines added to
12 (`diff` of the two hunks, empty). The only other removed text is 04's one lead sentence
("What follows is the full original scope."), rewritten to point at 12, and the old
`#53 PLANNED` cell. **Owner: re-run the audit at P08** (it covers this unit's base anyway).

## What I got wrong on the way

- Split 04 into a folder before checking who links to it from `docs/raw/` — the links check
  covers raw files, and raw files cannot be edited. Reverted.
- First D-65/D-66 wording was 92 bytes over the settled page's cap.

## Exit criteria (plan/10_P01 § Exit criteria)

- **The record is open on the branch** — D-65, D-66 in `01-settled.md`; dated entries in
  17/04 § #34 and § #35; the #53 cell fixed; all committed on `feat/hygiene-and-editor`.
- **`--docs --strict` is green** — exit 0, 27 OK + 1 INFO (above), on the committed tree.
- **Nothing is pushed** — no `git push` run; `git remote -v` unchanged.

## Left open — for the next units

- **17/04 has ~600 bytes of room.** P02–P06 each owe #34 / #35 a status line there. Write each
  delivery to its own record page in block 17 (next free page number 18) with one line in 04,
  as 07 / 08 / 13–15 did; if 04 fills again, the next relief is the same kind of verbatim move
  (#32's "Added 2026-09-10" paragraph to 15), **not** a folder split while
  `docs/raw/2026-10-05-hosted-ci-plan.md` links 04's #31–#33 anchors.
- **`01-settled.md` has 2 bytes of room.** P07 T2 adds decision rows: it must relieve that page
  first (a same-numbered split, or rows moved to a child as D-61 did for the DROPPED rows) —
  and fill D-66's "P07's block" with the link.
- `docs/roadmap/README.md` § Current focus still says "the rest of #34, then #35 on its
  trigger"; left for P07 to rewrite once, as the raw plan's § 3 orders.
- `plan/10_P01_open-record.md` is correct as written; nothing changed in it.
