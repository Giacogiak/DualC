# Record — Phase 4 (the decisions index)

The dated close-of-session entry of [roadmap 19](README.md) Phase 4: `docs/decisions/`
seeded from the roadmap by the plan's grep, the triggers written for the trigger-less
DEFERREDs, and the `decisions-index` gate check flipped from SKIP to OK. The plan is
[§ 4 Phase 4](../../raw/2026-09-17-docs-restructuring-plan.md#phase-4--decisions-index-a4--session);
the worklist is S1's "DEFERRED without trigger" paragraph
([S1 § 4](../../raw/2026-09-17-docs-screening-roadmap.md)).

**DONE (2026-09-18).** [`docs/decisions/`](../../decisions/README.md) exists, with 50 rows
over two files, and `python scripts/check.py --fast` reports `decisions-index` OK — 10
gate-mandated entries, 27 distinct anchored rows, 0 problems. #39 and #43 flip to DONE (the third of
A8's three migration sources — grep — is now used, after intros in Phase 3 and the audit
checklist in Phase 2). What was delivered and where it deviates from the plan:

- **Seed.** The plan's `rg -n 'DROPPED|DEFERRED|decision|rejected|instead of' docs/roadmap`
  gives 137 lines (block 19 excluded); reading every hit in place, and the ten entries the
  check itself enumerates (a heading whose first body line opens `**DEFERRED**` /
  `**DROPPED**`, or whose text carries the word: `05 #18`, `09 #7`, `10 #8/#9/#11`,
  `11 § 6`, `17/03 #24/#25`, `17/04 #27/#31`), yields 50 decisions: 28 DEFERRED, 6 DROPPED,
  16 DONE by choice. **No DROPPED entry is gate-mandated** — every one sits in a body line
  that opens with another word (`13/README` § Status opens `**DONE**`) or in a bullet, so the
  six DROPPED rows are held by hand, not by the check. Ten of the fifty are tracked items
  and keep their `#N`; the other forty are `D-01`–`D-40`, numbered by decision date so each
  table reads chronologically (the number is an ID: a future row takes the next free one
  wherever its date falls).
- **Deviation — two tables, not one (decided by Giacomo, 2026-09-18).** The single table
  A4 asked for came to 16.8 KB against the 15,360-byte cap: an anchored "Where recorded"
  link to a roadmap heading costs ~120 bytes and a row ~340, so even a table pruned to the
  rows the plan's estimate counted left one row of headroom, and the index is a page that
  grows. Principle 5 (a page never outgrows the cap; it splits) won over the literal
  "one table": the README holds every **DEFERRED** and **DROPPED** row — the class the
  check reads and the class an agent would re-propose — at 13.1 KB, and
  [`01-settled.md`](../../decisions/01-settled.md) holds the 16 **DONE**-by-choice rows at
  5.5 KB. Same columns, one ID sequence. `decisions-index` reads the README only, which is
  the point: the gate-relevant class is on the gate-read page. The plan's own estimate
  ("≈ 40–60 rows") did not price the anchors; recorded here so Phase 5 does not repeat
  the sizing.
- **Triggers written (nine, marked † in the index)** for the DEFERREDs S1 listed without
  one, each checked against the code or the record it depends on before being written:
  `D-11` the three field-graph omissions and `D-12` DAG-refs (12 § A); `D-13` the direct
  slicer (11 § 5 — not on S1's list, but trigger-less and in the roadmap README's "all
  trigger-gated" claim, so it gets one); `D-16` the clip mask (12/05); `D-17` the section-plane
  sliders (12/03); `D-19` `smooth=true` (13/02); `D-21` 3MF / `--mem` behind the ABI (14 § 9,
  10 § Phase 1 — `capi/dualc_c.h` confirms `dualc_field_export_tiled_stl` is the only tiled
  entry point, so "STL-only" is true of the ABI and false of the CLI since 2026-07-03, which
  the pointer line says); `D-23` per-tile-object collapse and `D-24` moving-front eviction
  (11/03, 11/04 — `pickTileDepthForBudget` in `examples/example_common.cpp` charges bytes per
  surface face at the busiest tile and never the seam hash, which is the trigger's premise).
  The plan's rule is followed to the letter: **the trigger lives in the index row; the
  roadmap entry gets one dated pointer line** (ten pointers plus the superseded note below —
  eleven appends across nine files, each `*(2026-09-18: … row D-NN of the decisions index.)*`), and no roadmap
  sentence is rewritten. The remaining 19 DEFERRED rows copy the trigger their entry already
  states (`#7`, `#8`, `#9`, `#11`, `#18`, `#24`, `#25`, `#27`, `#31`, `#33`, `11 § 6`,
  `11 § 5a.3`, `12 § D` IPC, `12 § D.2` startup, `12 § G` Approach B, `16 § H` × 2,
  `17/05` per-probe clamp, FTS5 from plan § 6).
- **S1 item 8's second half.** `02:163` "deferred to v3" carries a dated superseded note
  pointing at `12 § C` and row `D-03` — the only roadmap append that is not a trigger
  pointer.
- **Entry points.** `AGENTS.md` reading-order item 3 loses its "until Phase 4" clause (still
  72 lines, `root-entry` OK); `docs/README.md`'s folder row links the folder and names the
  two tables, and its `rg` recipe for decisions becomes a by-ID lookup (`rg -n 'D-24|#25'
  docs/decisions docs/roadmap`) — the old `DEFERRED|DROPPED` grep is what the index
  replaces; `docs/design/README.md` § Things we do not do links the settled table (no date,
  `design-no-history` unchanged at 0 markers). `STRUCTURE.md` is untouched: `docs/` is one
  line there since Phase 1.
- **Gate wiring** (`scripts/check_data.json` only; `scripts/check.py` untouched):
  `docs/decisions/` joins `footer_roots`, `index_roots` (the README must name
  `01-settled.md`) and `structure_summarized_prefixes`; the `decisions_index._comment`'s
  "SKIPs while … Phase 4 writes it" becomes the statement that the check reads the README
  only. `decisions-index` was already in the fast tier (`tier="fast"` at its decorator) —
  "add it to `--fast`" needed no code, only the file whose absence made it SKIP.
- **Not done, and why.** `docs/decisions/README.md` is *not* added to `status_tables`:
  `status-sync` would demand that the first body line under every anchored heading contain
  the row's first legend word, and the DONE-by-choice rows link headings whose body opens
  with an argument, not a status word; `status-vocab` on the two tables is a Phase 8 script
  item if wanted. The roadmap README's Track 2 items 3–4 ("DEFERRED / optional", "deferred
  to the public release") and 12/README's table cells (F, D.1, A) are not appended — one
  pointer per decision, in the topic file, is the rule applied; the README is byte-baselined
  and its block-19 row edit is byte-neutral (`0–3`/`4–8` → `0–4`/`5–8`, baseline 16,091
  unchanged). The one-line `heading-status` catch on the way — the settled page's H1 first
  read "— DONE by choice", a status word in a heading — was renamed before commit, so no
  baseline entry exists for it.
- **Two latent facts about the check, for Phase 5 and Phase 8.** (a) `check_decisions_index`
  keys its rows by `(file, anchor)` in a plain dict, so rows that cite the same heading
  collapse to the last one: `D-29`/`D-30`/`D-31`/`D-32` all cite `16 § H`, `D-14`/`D-23`
  cite `11/03 § 5a`, `D-17`/`D-25` cite `12/03 § D`, `D-33`/`D-34` cite `17/05 § Not
  reported`. Harmless today — none of those headings is gate-mandated — but if Phase 5's
  consolidation promotes one of those bullets to a heading whose body opens `**DEFERRED**`,
  the check will read only the last row and pass while pointing at the wrong decision; the
  fix then is a sub-heading per decision, or the check keyed on the row's ID. (b) `D-28`'s
  decision cell contains `a \|\| b` — correct markdown, but a parser that splits cells on
  a bare `|` sees seven cells; Phase 8's semantic lint must honour the escape.

*Verified:* `python scripts/check.py --fast` — 26 checks, 24 OK, 0 failed, 0 SKIP, 2 INFO
(`index-staleness` 2 files, `directional` the same 4 sites as Phase 3); **`decisions-index`
OK, 10 entries, 27 rows, 0 problems** (SKIP before this session); 1,093 links, 0 broken;
297 cross-file + 19 same-file anchors, 0 unresolved (53 new, every row's anchor resolves to
the heading the check keys on); `indexes` 15 roots (was 14), 99 siblings, 0 unlinked;
`footer` 78 pages; `sizes` 94 files, 0 over — the README at 13,073 bytes has room for
about six more rows before the split rule fires again, at which point the DROPPED rows are
the natural next child; `heading-status` 112 files, 0 over baseline; `root-entry` OK.
`--docs --strict` fails on `directional` only, as before. Nothing under `src/`, `include/`,
`tests/`, `examples/`, `capi/` or `scripts/check.py` changed (diff stat), so `ctest -N`
stays **256**, measured on the existing build.

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
