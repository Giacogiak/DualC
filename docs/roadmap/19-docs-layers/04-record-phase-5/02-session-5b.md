# Phase 5, session 5b — conflicts, chronology, duplicates, residue

Part of the [Phase 5 record](README.md) (roadmap 19): plan items 5–11 and the navigation re-run.

**Session 5b — DONE (2026-09-18).** Items 5–11, same rules: a conflict is settled by
reading code or git, never by preferring one document; a record entry takes a dated
one-line append; a duplicate loses its copy, not its home; index READMEs are rewritten.
The phase closes; the `roadmap/README.md` row 19 and the phase table read Phases 0–5 DONE.

- **Item 5 — the 16 conflicts, all settled from evidence; zero markers owed.** The
  plan's wording invites a reader to expect `Reconstructed` / `DRIFT-PENDING` lines;
  none was needed, because every historical value re-derived from git, and the record
  says so here so the absence is not read as an omission. What each turned out to be:
  (1) **entry points** — `capi/dualc_c.h` at `3287754` (0.2.0, 2026-06-17) exports **9**,
  at `e345bf3` (0.3.0, 2026-06-19) **11**, at `ebe929a` (0.4.0) **13**; `10 § Phase 1`,
  `14/02 § 3` and `14/03 § 8` all said "11 at 0.2.0" — a 2026-09-11 annotation, corrected
  in place, with a dated clause citing the three commits, and the stale 0.3.0
  per-function table in `14/02 § 3.2` removed in favour of the header and
  `capi/README.md` (the `§ 3.3` mapping table stays: checked against `applyParams`, still
  exact — the plan's "both tables" over-reached). (2) **frozen** — resolved by the 5a
  tombstones. (3) **"every oversize file split"** — the README row is rewritten. (4)
  **171 vs 157** — two metrics, not a conflict: `ctest -N` at `2530acf` (2026-06-14) is
  171 (153 `TEST_CASE` + 18 `add_test`), and 157 is the `TEST_CASE` count at `51b5efa`
  (2026-06-17), where ctest is 176; dated clause on `13/README`. (5)/(6) **`#3` and
  `#N` collisions** — `01/01` and `01/02` say "Tier-1 item 3" / "Tier-2 item 6" and link
  them, which item 6 below made possible. (7) **primitive count** — `primitiveCatalogue()`
  has 30; `12/02`'s "~23 remaining" names 24 (the v1 slice had emitted six); dated clause.
  (8) **study count** — 11 documents is right (`ls docs/study/`); `STRUCTURE.md`'s 13
  went with Phase 1's one-line summary. (9) **231 on 2026-09-01** — true of the tree
  between #28 (`1492647`) and #23 (`71b8018`), both that day; the static
  `TEST_CASE + add_test` count reads one high from `71b8018` on because #23 added the
  hidden `[.][golden]` case that `ctest -N` does not list — which is why 235, 245, 253,
  255 and 256 in the record are all right. (10) **68 vs 69** — the parity table at
  `65d10b5` (the perf entry) is 68 and at `aa0d577` (Phase 5, later the same day) 69;
  the 73 of today is 69 table cases + 4 pushed at runtime (`runList`, the mesh/grid
  cases), which is what `parity_expected_cases` asserts. (11) **export bug** — item 6.
  (12) **wall thickness "pending"** — applied since: cmdref `06`'s offset table, `12 § F`
  and `design/10` agree on wall ≈ 2·thickness; dated clause on `05/README #20`. (13)
  **opt-in build options** — `AGENTS.md` lists four since Phase 1. (14)
  **`user_guidance.md`** — the reference is gone (`07`). (15) **four binaries / five
  smoke tests** — both true: `add_test` at `7758093` is five, `cli_primitive` and
  `cli_primitive_infinite` both drive `dualc_primitive`; dated clause. (16) **NEXT** —
  5a.
- **Item 6 — chronology.** `03`'s blockquote: the "still open" sentence was written at
  `e84dbae` (2026-06-14), two days after the fix (`6ae7dc9`) — stale when written, said
  in a dated clause after the 2026-09-11 resolution line. `13/README`'s banner reads
  DONE first and names the superseded "design only" wording; its two "above" pointers
  link `13/01 § Why … a PDE`. `11/02`'s "table above" links `11/01`. `02:163` and
  `14/README:48-50` were Phase 4's ([03](../03-record-phase-4.md)), cited, not redone.
  **The bold pseudo-headings became headings**, which is what made the `#N` fix
  linkable: `01/README`'s nine Tier items (`#### N. Title`, status on the body line),
  `12/03`'s section planes / file-watch / parameter push, `12/04`'s three entries under
  a new `## D.2`, `17/09`'s "semantic half". No heading *text* changed — the status
  and date moved out of the bold line onto the body line, per the contract — so no
  existing anchor moved; seventeen new headings exist (9 + 3 + 4 + 1). Two of the new headings open
  `**DEFERRED**` and are gate-mandated now: `decisions-index` reads 12 entries (was
  10), and rows `D-17`, `D-25`, `D-29`, `D-05` land on their own headings instead of
  sharing `§ D` / `Tier 2` — hazard (a) of [03](../03-record-phase-4.md) closed for those
  four. The split-residue heading levels of `01/01`, `01/02`, `05/03`, `11/03`, `11/04`
  were lifted one step and `11/02`'s H1 renamed (it repeated its section heading), so
  `heading_hierarchy_baseline` carries no roadmap entry any more (10 left, all
  `command_reference/`, Phase 6's).
- **Item 7 — duplicates.** `10`'s "why a C ABI" paragraph (a verbatim copy of
  `14/01 § 1`), its dead API sketch (never shipped; deleted, the deletion dated) and its
  eleven-name entry-point list are links into 14; `12/README` has one Boletus pointer
  (the banner), the sequence's fifth step is a plain list item and `§ E` is three lines;
  `15`'s file-watch and discrete-GPU paragraphs keep the Boletus-side facts and link the
  mechanism in `12/03` / `12/04`; `12/05`'s Hermite-tag paragraph links
  `01 Tier-4 item 14` (the item is the home) and its section-planes note links `12/03`;
  `17/README`'s five `Update` paragraphs are dated one-liners linking the record pages
  (every per-batch ctest count survives on those pages; the 228 → 229 correction is
  recorded once, in `12/07`); `17/10`'s R1 lists the seven checks and links
  `17/09 § The semantic half` for what each holds; `17/08 § Still open in #34` was 5a.
  **Left in place, and why:** `12/01`'s "Implemented" vs "Original design" (design then
  delivery — the record's shape, not a duplicate); the boolean-over-lattice figure in
  `03` and `12/README` (one sentence with the benchmark linked, the allowed form); the
  decorator list in `02`/`03`/`04` (`04` already points at `03`); one "Moved to Boletus"
  blockquote per file in `10` and `11/README` (each file needs its pointer).
- **Item 8 — usage tables.** `14/02 § 3.2` (removed), `06`'s `--plane`/`--res` defaults,
  `12/04`'s `--preview-scale` default and range, `11/03`'s `--mem` suffix grammar and
  `12/03`'s section-plane keys are links to the `command_reference/` page, on the
  `12/01 § Node tables` model. **Kept:** `17/05`'s `Diagnostics` field table (the
  delivered shape of a struct, not usage — and `usage-in-record`, which reads `Flag` /
  `Default` / `Option` headers, agrees), `07`'s mode and control lists and
  `05/README #16`'s flag semantics (prose records of what shipped, not tables of
  defaults).
- **Item 9 — residue, paths, anchors.** Chat residue scrubbed at every S1 § 6 site
  still standing: "the advisor caught" → "caught in review" (`05/03`, `07`), the
  "AskUserQuestion" scope line → "decided by Giacomo" (`07`), "auto-memory note" (`05/03`)
  and `user_guidance.md` (`07`) gone, "the user commits manually" (`11/README`) and
  "the owner clarified" (`10 #12`) reworded; `17/05`'s was 5a. Stale prose paths: the
  bare link texts `11-dualc_field.md` / `12-dualc_field_view.md` in `12/01`, `12/03`,
  `12/05` and `05/03` are `command_reference/11` / `12`; `13/01` and `13/README` link
  `05` and the cmdref folders; `stale_paths_baseline` carries no roadmap entry (4 left,
  Phase 6's). **Kept:** the backticked `11-dualc_field.md` mentions in `05/02`, `05/03`
  and `13/README`'s docs list — historical literals naming where a page was written at
  the time, the class Phase 3 kept for `ARCHITECTURE.md` and the check ignores.
  Misdirected anchors: `15`'s discrete-GPU link lands on `12/04 § Discrete-GPU`,
  `12/README`'s "ranked fix menu" on `12/04 § Startup`, `12/07`'s "Unchanged from" on
  the same (`12/README:101` and `16:180` were 5a). **§ anchors on every folder-README
  page table** (`01`, `05`, `11`, `12`, `13`, `14`): each "Sections" cell links the
  headings it names, so an index → child → section path is two hops from the roadmap
  README — 407 cross-file anchors resolve (was 327 at 5a's close).
- **Item 2, remainder.** `17/09`'s "Today (2026-09-11)" table and its "253 today"
  carry a dated clause naming them as that day's reading (the `frozen-decl` 8 among them,
  S1 § 3's `17/09` vs `17/10` contradiction) with the live counters left to `check.py`
  and the one *Latest verification* line — a snapshot in a record is dated, not updated.
- **Item 10 — legend.** 5a used NEXT once and wrote the PARTIAL body-line form; nothing
  off-legend remains in the two `status_tables` (33 cells) or `12/README`'s table.
- **Item 11 — "the roadmap explains the present".** Applied where Phase 3 actually
  harvested the material: `02`'s infinite-bounds gotcha is one sentence linking
  `design/08 § 10.5`; `12/03`'s section-planes entry links `design/09 § Keyboard-layout
  independence` for the rule instead of restating it; `05/README #20`'s wall-thickness
  clause links `design/10`. The rest of the intros describe *what a block is*, not how
  the engine works today, and `02`'s other two gotchas (open-surface primitives,
  resolution-driven face counts) have no design home yet — both are item (c) of Phase
  8's semantic lint, as the plan says.

*Verified (5b):* `python scripts/check.py --fast` — 26 checks, 24 OK, 0 failed, 0 SKIP,
2 INFO (`index-staleness`; `directional` 3 sites, all `command_reference/`); 1,288 links,
0 broken; **408 cross-file + 19 same-file anchors, 0 unresolved**; `indexes` 17 roots
(this record's folder joined), 109 siblings; `sizes` 103 files, 0 baselined, 0 over (the
roadmap README at 14,655 bytes; this record split into a folder at 20.5 KB — principle 5
applies to the record too); **`status-sync` 15 anchored rows, 0 disagree**; `decisions-index` 12 entries,
29 rows, 0 problems; `drift-pending` 0 lines; `heading-hierarchy` 10 grandfathered
(was 15), `stale-paths` 4 (was 9), both lists now `command_reference/` only.
`--docs --strict` fails on `directional` only — the three `command_reference/` sites,
Phase 6's. Nothing under `src/`, `include/`, `tests/`, `examples/`, `capi/` or
`scripts/check.py` changed; `ctest -N` **256**. S1 § 5's fourteen navigation questions, re-run:
[README § Navigation](README.md#the-fourteen-navigation-questions).

---

← Back to the [phase record](README.md) · the [Docs layers index](../README.md).
