# Record — Phase 5 (the roadmap consolidation)

The dated close-of-session entries of [roadmap 19](../README.md) Phase 5: session 5a (the
structural moves, plan items 1–4) and session 5b (items 5–11: conflicts, chronology,
duplicates, usage tables, chat residue, the legend, "the roadmap explains the present").
The plan is [§ 4 Phase 5](../../../raw/2026-09-17-docs-restructuring-plan.md#phase-5--roadmap-consolidation-12-sessions-s1-is-the-worklist);
the worklist is [S1](../../../raw/2026-09-17-docs-screening-roadmap.md). Its line numbers
are pinned to commit `3d73220`; every passage below was found by content, since Phases
0–4 split or moved several of the files in between.

**Session 5a — DONE (2026-09-18).** Items 1–4: `16` § H folded into `12/07` and `18` into
`14/04` (tombstones, numbers retired); one status ledger for #22–#35 with a *Latest
verification* line as the counts' one home; `roadmap/README.md`, `17/04` and `17/05`
under the cap (`17/11`, `17/12` and `08 § FieldPtr` hold what moved out) and
`size_baseline` empty; the quality report split into `12/08`. Evidence and deviations:
[01-session-5a.md](01-session-5a.md).

**Session 5b — DONE (2026-09-18).** Items 5–11: the 16 conflicts settled from git and the
code with zero `Reconstructed` / `DRIFT-PENDING` markers owed; the chronology repairs; the
bold pseudo-headings of `01/README`, `12/03`, `12/04`, `17/09` promoted to headings
(four decisions rows land on their own heading); duplicates and usage tables replaced by
links; chat residue and stale paths gone; § anchors on every folder-README page table;
the roadmap-side `heading_hierarchy_baseline` and `stale_paths_baseline` entries
removed. Evidence, what was deliberately kept, and the verified line:
[02-session-5b.md](02-session-5b.md). The phase closes with `--fast` green, `--docs
--strict` failing on the three `command_reference/` directional sites only (Phase 6's),
0 broken links, 0 unresolved anchors, `ctest -N` 256.

**Session 5c — DONE (2026-09-20).** The two S1 findings 5b had left unnamed — "DONE
without evidence" and "status tables doing record duty" — closed from git:
[03-session-5c.md](03-session-5c.md).

## The pages of this record

| Page | Holds |
| --- | --- |
| [Session 5a](01-session-5a.md) | items 1–4 — the four moves, the deviations, the verified line |
| [Session 5b](02-session-5b.md) | items 5–11 — the sixteen conflicts one by one, what was left in place and why, the verified line |
| [Session 5c](03-session-5c.md) | the evidence pass (2026-09-20) — every undated DONE gets its commit; `12/README`'s status cells become status + link |

## The fourteen navigation questions

S1 § 5's questions, re-run at the close of 5b from `roadmap/README.md` with zero context
(the plan's verify line asked for 3, 11 and 12 to reach ≤ 2 hops):

| # | Question | Path now | Hops |
| --- | --- | --- | --- |
| 1 | Status of #17? | README row 05 / Track 1 item 2 say DONE → `05/README` → `05/01` | 2 (was 2) |
| 2 | Streaming export record? | README keystone #3 → `11/03 § 5` | 1 (was 2) |
| 3 | What is NEXT? | README § Current focus: **NEXT** — the rest of #34, then #32 → `17 § Tracked items` for what "the rest" is | 1 (was "partial") |
| 4 | What remains in #34? | README Track 1 item 1 → `17 § Tracked items` → `04 #34` | 2 hops to one list (was 1 hop to three differing copies) |
| 5 | Why was graded wavelength dropped? | README row 13 → `13/README § Decision` → `13/01 § Why … a PDE` | 2 (was 2) |
| 6 | ABI version / entry-point count? | README keystone #19 names `capi/README.md`; `10`, `14/02`, `14/03` agree since item 5 | 1 (was "conflicting") |
| 7 | What does `check.py` enforce? | README milestones row → `17/09` | 1 (was "via CLAUDE only") |
| 8 | Was `--normalize-thickness` broken? | README row 05 → `05/README #20` | 1 (was 1) |
| 9 | Can `dualc_lattice` stream? | README Track 1 item 3: no, `D-14` DEFERRED → `11/03 § 5a` | 1 (was 1) |
| 10 | Rhino side-car status? | README keystone bullet → `15` hand-off table, PLANNED | 1 (was 1) |
| 11 | Section planes record? | README Track 2 item 4 → `12/03 § Section planes` | 1 (was 3) |
| 12 | Item #12 (demo data)? | README row 10 names #12 → `10 #12` | 1 (was "marginal") |
| 13 | Latest ctest/parity? | README Track 1 item 1 → `17 § Tracked items › Latest verification` | 1 (was 1, no home) |
| 14 | Open-surface clip mask built? | README Track 2 item 3: no, `D-16` → `12/05 § Deferred` | 1 (was 1) |

Every question is ≤ 2 hops; the plan's three (3, 11, 12) are 1, 1 and 1.

---

← Back to the [Docs layers index](../README.md) · the [Roadmap index](../../README.md).
