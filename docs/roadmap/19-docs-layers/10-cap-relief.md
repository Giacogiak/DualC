# Cap relief — the status snapshot and the gate's record

Part of the [docs layers record](README.md). At the close of Phase 8
([07](07-record-phase-8.md)) two pages sat at the byte cap of `scripts/check_data.json`
`size_caps`: [`roadmap/README.md`](../README.md), the status snapshot, at 15,359 of 15,360
bytes, and [17/09](../17-code-audit-and-hardening/09-local-checks-gate/README.md), the gate's
record, at 15,352 — each recorded there as "takes no append". The snapshot is an
`index_roots` entry and the one status surface, so its relief is a session of its own: this
one, 2026-09-21. Bytes here are content bytes — `len(text.encode("utf-8"))` after a
universal-newline read, the count `sizes` makes; `size_baseline` is empty and stays so.

## The status snapshot

**What was over.** The page is a rewritable snapshot, not a record — its own intro says
every claim is one line plus a link and a count is stated only in its home — and it had
stopped obeying that rule: a five-bullet "shipped keystone" list restating block 12's
sequence, a Track 2 "shipped baseline" paragraph restating 12 § B–§ F, per-item
parentheticals in both tracks restating what their decisions rows and record entries own,
and a glossary remark the glossary already makes. The cost was real: block 15's index row
could not be refreshed on 2026-09-21 (`c6d0c7a`), so 15 and four other direct children
showed under `index-staleness` until this page was touched.

**What moved where** (nothing was lost — every target already held the fact):

| Passage | Owner it now links |
| --- | --- |
| the shipped-keystone list (#1, #2, #3, #19, the Boletus move) | [12 § The new sequence](../12-field-graph-and-app/README.md#the-new-sequence-value-first-then-the-c-abi), [15](../15-boletus-handoff.md) |
| "Tier-1 items are never `#N`" | [design 11](../../design/11-glossary.md), the ID vocabulary |
| Track 2's "shipped baseline" paragraph | [12](../12-field-graph-and-app/README.md) § B–§ F |
| the #46 aside on Track 1 item 1, the 3MF / `--mem` / QEM clause on item 3, the "#18 prerequisite" clause on item 5, the wire-format clause on item 7 | the milestones row of #46; [11 § 5a](../11-dense-lattice-deliverable/03-streaming-export.md#5a-deferred-enhancements-gains-vs-the-shipped-stl-version) and 12 § G; the decisions rows [#11](../../decisions/README.md) and [D-12](../../decisions/README.md) |
| the merge / loss-audit detail of § Current focus | [19/09](09-pre-merge-loss-audit.md) |
| the WebGL2 "cheap shell-swap" explanation on Track 2 item 5 | [08](../08-raymarch.md) |

**The rule** — [D-43](../../decisions/01-settled.md): *Recent milestones* is a rolling
window of the last five rows (its lead-in says so; older rows live in the block records the
table links), a shipped keystone or baseline is one line plus a link to its block, and no
count or version is restated on the page. The table held four rows at the trim; the fifth
is this session's.

**Bytes.** 15,359 → 13,343 (169 → 147 lines, 1,833 → 1,568 words) after the trim —
13,526 once the fifth milestones row and 17/09's folder path landed — with
every heading, every Status cell, every `#N` and D-NN link and every Principal-blocks row
kept — `heading-status` and `status-sync` read the page and both stayed at zero. The
session's brief had aimed at ≤ ~12.5 KB; reaching it meant rewording the "What it covers"
cells of the Principal-blocks table, the one table `status-sync` reads, and that was
declined — the trim stops where the page's own rule stops, 1.8 KB under the cap.

**The index rows owed.** Block 15's row now says its links were re-pointed on 2026-09-21
after Boletus's docs restructuring. The other four children that were newer than the page
— [10](../10-infrastructure-and-integration.md) (`33732c9`),
[12](../12-field-graph-and-app/README.md) (`c6d0c7a`), [19](README.md) (`c41a6a7`),
[20](../20-public-delivery/README.md) (`33732c9`, `b53f3f3`, `98d41ab`) — were re-read against
those commits: dated pointers and verification lines only, no status cell to change.

## The gate's record

**What was over.** 17/09 is a record page — dated appends only — and sat 8 bytes under the
cap while owing the `semantic-lint-runs` row of the 27th check ([07](07-record-phase-8.md)).
The remedy is the contract's: a same-numbered folder, the R5 precedent of
[17/10](../17-code-audit-and-hardening/10-docs-system-screening.md#the-split-pass-r5)
(11, 13, 14 and 17/03 were split the same way).

**What moved where** — `git mv` to
[`09-local-checks-gate/README.md`](../17-code-audit-and-hardening/09-local-checks-gate/README.md),
then the sections into numbered children, verbatim, headings and dated notes unchanged:

| File | Sections (old lines) | Bytes |
| --- | --- | --- |
| `README.md` | the intro and § What CI actually is, split in two (L1–28) + the section index | 3,043 |
| [`01-the-docs-checks.md`](../17-code-audit-and-hardening/09-local-checks-gate/01-the-docs-checks.md) | § The docs checks lead, and that is the point + § The semantic half (L29–97) + the dated append for the 27th check | 6,103 |
| [`02-three-decisions.md`](../17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md) | § Three decisions worth recording (L98–146) | 3,796 |
| [`03-proven-to-fail-and-scope.md`](../17-code-audit-and-hardening/09-local-checks-gate/03-proven-to-fail-and-scope.md) | § Every check was proven to fail + § Fixed on the way past + § Enabling the hook + § What this does and does not move, with the two dated one-liners (L147–230) | 5,493 |

One split residue was retargeted: the `sizes` row of the first check table said "ratcheted
(below)" of a section now in 02, and links it. The folder joined `index_roots`
(20 → 21 roots). `STRUCTURE.md` needed nothing — `docs/roadmap/` is a summarized prefix.

**The owed fact** landed as a dated append on 01 § The semantic half: the 27th check,
`semantic-lint-runs` (2026-09-21, ported from Boletus with its fixture pair; `--selftest`
28 runs). The check tables stay the dated 2026-09-11 reading, as their notes say.

**Inbound links retargeted** — thirteen path links to the folder README (`AGENTS.md`,
`docs/README.md`, `roadmap/README.md`, 17/README, 17/04 ×3, 17/12, 19/01 ×2, 19/05, 19/07,
this page, design 11 — the last not on the session's list, found by grep) and the two
anchor links to the child that holds the heading (17/10 `#the-semantic-half` → 01, D-36
`#three-decisions-worth-recording` → 02). `20 #47`'s "17/09" is prose, not a path; the
bare old name in `docs/raw/` is outside `stale-paths`' scope (`in_scope` excludes
`frozen_prefixes`), so `stale_paths_baseline` stays empty.

## Gate evidence

Three commits — the snapshot (`4d7807b`), the folder (`cb84978`), this record —
`python scripts/check.py --docs --strict` PASS before and after each. After the folder:
`links` 1688, 0 broken; `anchors` 588 cross-file + 22 same-file, 0 unresolved; `indexes`
21 roots, 135 siblings, 0 unlinked; `sizes` 121 files, 0 over; `stale-paths` 17 folders,
0 new; `heading-hierarchy` 0 new; `status-sync` 16 anchored rows, 0 disagree;
`python scripts/check.py --selftest` 28 fixture runs, 0 wrong; `ctest -R check_selftest`
1/1 in the existing `build/`.

`index-staleness` stays INFO under `--strict` — it is not declared `report_only`
(`check.py`, the check's decorator), so its list is read by hand at every close, as here.
At this close, nine INFO lines, each benign: 14/03, 11/03,
command_reference 11/01, 01/01, 01/02 and 17/03/02 are the pre-session entries — count
syncs and dated pointers whose index rows were re-read on earlier closes; `raw/study/ERRATA`
is immutable; `decisions/01-settled.md` is newer than `decisions/README.md` because D-43
was appended, and `design/11-glossary.md` newer than `design/README.md` because its 17/09
link was retargeted — each README indexes the file, not its rows. The five direct children of
`roadmap/README.md` that this relief was for (10, 12, 15, 19, 20) and 17/09 are off the
list: the snapshot and 17/README were committed with or after them.

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
