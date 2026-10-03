# Record — Phase 7 (`raw/` and the study pack)

The dated close-of-session entry of [roadmap 19](README.md) Phase 7, done as one
session: the plan is
[§ 4 Phase 7](../../raw/2026-09-17-docs-restructuring-plan.md#phase-7--raw-and-the-study-pack-a6-a7-1-session)
(items 1–6, advice A6 and A7), the worklist is
[S3](../../raw/2026-09-17-docs-screening-architecture.md) § 3 and § 7. S3's line
numbers are pinned to commit `3d73220` and its passages were found by content — except
its refs *into* the pack, which are exact: the A/B/T files are byte-identical to
`f03ef3d`, the commit that landed them, and the 88-column reflow of `3d73220` excluded
the pack (`git diff 3d73220~1 3d73220 --stat -- docs/study` is empty — settled, not
re-verified here), so every `docs/study/X.md:NN` citation in roadmap 17 is still valid
under its new path.

**DONE (2026-09-20, commits `a81c7bf`, `abdcc11` and this record).**

- **Item 1 — the move, its own commit (`a81c7bf`).** `git mv docs/study
  docs/raw/study`; nothing inside the pack changed except its README's one outbound
  link, which gained a `../` (the `links` check reads `raw/`, so the pre-commit hook
  would have refused the commit otherwise). `git log --follow` on every A/B/T file
  is a clean rename to `f03ef3d`. The plan's "≈ 33 citations, one `sed`" was **45
  in 18 files, in two shapes**: relative markdown links (`../../study/README.md`
  on 17/01, 17/02, 17/05, 17/06 and 17/README; `../study/README.md` on
  roadmap/README) and bare prose `docs/study/X.md:NN` citations, 9 on 17/03 and
  16 on 17/04, retargeted as text. Also in that commit: docs/README's folder table
  lost its `study/` row (the pack is no longer a `docs/` folder) and its size-cap
  clause names `raw/` alone; `STRUCTURE.md`'s `study/` line moved under `raw/`;
  `check_data.json` dropped `docs/study/` from `frozen_prefixes` (`docs/raw/`
  covers it) and from `structure_summarized_prefixes` (same reason — 19/01 had
  expected a rename, a drop is the smaller change) and renamed the `index_roots`
  entry; `check.py`'s `strip_code` docstring names the new path. **Left as
  written:** the backticked historical mentions in 19/01, 19/04, 19/05, 17/09 and
  decision D-39 — records are append-only (Phase 5b's precedent). **One judgment
  call:** `01/02-bug-catalogue.md:67` ("audit kept under `docs/study/`") is a
  location pointer inside a record, retargeted as text rather than left false.
  Proof at close: `rg -n 'study/' --glob '!docs/raw/**'` outside backticks finds
  only `STRUCTURE.md`'s tree line.
- **The one design decision — `indexes` against an immutable README.** `indexes`
  requires every `.md` sibling and every sub-folder with a README to be linked
  from its folder's README, and `ERRATA.md` and `figures/README.md` are new
  siblings inside a pack whose files are never edited. The alternative — drop the
  pack from `index_roots` and let `raw/README.md` index the three navigational
  files, README byte-identical — fell the moment item 1 forced the `../` edit:
  the README was no longer untouched, and it was the only file of the pack ever
  amended (`015353b`, `40bfb9b`, `30725a0`). So the plan's step 2 is the single
  sanctioned edit: the two status notes (lines 3–29, 2026-08-21 and 2026-08-31)
  moved verbatim to `ERRATA.md` and a five-line pointer callout naming `ERRATA.md`
  and `figures/README.md` took their place. The README is 13.7 KB (it was 15,339 B,
  339 over the cap it is exempt from anyway); the pack stays in `index_roots`, and
  `docs/raw` joined it — principle 2 had no check behind it for that folder. This
  is recorded in `raw/README.md` as the second import-normalization note, under
  the rule the first one set: say exactly what was touched, and that nothing else was.
- **Item 2 — `docs/raw/study/ERRATA.md`**, the pack's one writable file. Four
  tables/sections, every row `file:line · what it says · where it is now`:
  (a) **folder paths** — nine of S3 § 7's twelve sites are in the pack (the other
  three were the report's and the root README's, fixed by Phases 5 and 1) plus one
  S3 did not list, `A3:216`'s `report-quality-inspection-contouring.md` Step 4 →
  12/08/01; each resolved by reading the cited line: `01-core-dual-contouring.md`
  §§ 3.2/3.3 → 01/01, §§ 4.3/4.4/4.9 → 01/02, `:16` → 01/01 § 2's vendoring row
  (line 16 of that file too), the "≤ 80-LOC decider" → 01/README Tier 2 item 4;
  (b) **line refs into `ARCHITECTURE.md`** — the pack cites the *pre-rewrite* file,
  so 19/02's section map (built on the post-rewrite one) would have mistargeted
  them; instead each was checked against `git show d6b2808:docs/ARCHITECTURE.md`
  (the last commit before `f03ef3d`) and all six are exact there: `:274`
  "default: bisection" and `:263-281` the 3 + 3 sketch → design/08 § 10.1,
  `:216`/`:248` the PSEUDONORMAL no-op → design/02, `:236` the LGPL octree →
  design/06 § 8 + `THIRD_PARTY.md`; the 31 "ARCHITECTURE.md is stale" mentions
  across eight files are named as history of the `d6b2808` file; (c) **counts** —
  README:20's "13 … #22–#34" was correct when written (#35 was born on 2026-09-10
  from #34's fix, 17/README says so) and is 14, #22–#35 today; the 182/201/224
  test counts point at 17/README's one home and restate nothing; (d) the **status
  notes** verbatim. S3's observation that A1 and B1 are cited nowhere outside the
  pack's README is left as it is — they are background reading, and adding
  citations to the ledger for the sake of it would be restating.
- **Item 3 — `figures/README.md`**: 24 rows, file → bytes → used by, generated
  from a grep of the documents (none orphaned; the explorer draws its own);
  `fig-tpms.svg` at 654,768 B is named as the asset — 4,400 `<line>` and 1,291
  `<rect>` elements, not a raster — and the reason the pack is 1.7 MB on disk
  against 0.6 MB of text.
- **Item 4 — `raw/README.md`**: rows for the study pack (its three navigational
  files linked, `git mv` and `--follow` named) and for the parity run; the
  never-edited rule stated once, with the ERRATA row as the study pack's form of a
  correction; and a "reading the screenings today" paragraph so the four things
  S1–S3 and the plan cite that have moved since `3d73220` (16 → 12/07, 18 → 14/04,
  the report → 12/08/, `ARCHITECTURE.md` → `design/`) are resolvable from the index
  without touching the immutable files — that was Phase 6's hand-off to this
  phase. That paragraph is deliberate navigation, not a second home: the
  mappings are owned by the tombstones and 19/02's table, which it links, and
  it exists only because the files it serves can carry no pointer themselves. **`parity.txt` earned its import:** it is the `dualc_glsl_parity` run of
  2026-06-24 (62 cases, Intel HD 630, all passed) and 12/02 cites exactly that
  "62/62" for that date. It arrived as UTF-16LE with CRLF and a BOM from a
  PowerShell redirect; re-encoded UTF-8/LF as `docs/raw/2026-06-24-parity-run.txt`.
  The dated name does not match `.gitignore`'s `parity*.txt`, so no negation was
  added; the root copy is gone.
- **Item 5 — the search layer — was already done.** docs/README § How to find
  things has been the whole search layer since Phase 1 (Phase 6 added the
  `MINOR-OPEN` grep) and D-40 has held the FTS5 deferral since 2026-09-17. What
  this phase added to D-40 is the plan's second trigger clause ("three sessions
  in a row where `rg` failed to find a fact that was in the repo") and the
  reading of the first: **50 *text* files or 1 MB of *text* — SVG and HTML assets
  do not count**, or the move would have fired the trigger it was written to
  guard. Measured at close: `docs/raw/` is 46 tracked files, of which 24 SVG
  figures and 1 HTML, so 21 text files at 637 KB (1.80 MB with the assets).
  #41's done-when ("trigger written") is met by D-40; no new D-NN, no new heading.
- **Item 6 — the gate.** `footer_roots` already held `design` and `decisions`
  (Phases 3 and 4) — nothing to do. `size_scope` stays `["docs/"]` with
  `frozen_prefixes` as the exemption, now `docs/raw/` alone, which is the plan's
  "every markdown outside `docs/raw/`" end state exactly; `sizes` prints 20
  exempt (12 pack pages + ERRATA + figures/README + 6 `raw/` files). 17/09's
  "all of `docs/study/` is a verbatim snapshot — those are exempt" is answered by
  a dated two-line append pointing at `raw/README.md`, never a rewrite; 17/09 is
  14,918 B after it. `heading-status` reads `raw/` too, so ERRATA's H1 lost the
  date it was born with before the first commit (the body carries it).

## What Phase 7 leaves, by owner

- **Phase 8's script pass:** `decisions-index` keyed on (file, anchor);
  `status-vocab` not reading the decisions tables; the `\|` escape in row D-28
  (the `sizes` off-by-one is fixed since Phase 6).
- **Phase 8's semantic lint, item (c):** `02-implicit-sdf-foundation.md`'s two
  remaining v2 gotchas; any roadmap intro still describing the present.
- **A code session, not 8's:** the `tile-depth >= depth` console line, a
  MINOR-OPEN on roadmap 11/03 § 5.
- **`index-staleness` INFO entries, all benign:** 11/03 (as after Phase 6),
  17/09 (this phase's append; its 17/README row is unchanged and correct) and
  01/02 (this phase's path retarget; its 01/README row likewise); 19/05 left the
  list when this record's commit touched 19/README.

*Verified at close* (measured after this record and the README rows landed):
`python scripts/check.py --fast` and `--docs --strict` — 26 checks, 25 OK, 1 INFO
(`index-staleness`, the three above), **0 failed, `--strict` still fully green**:
the three baselines (`size`, `heading_hierarchy`, `stale_paths`) empty; 1,497
links, 0 broken; 529 cross-file + 22 same-file anchors, 0 unresolved; `indexes`
18 roots, 120 siblings, 0 unlinked; 106 files under the cap, 20 exempt;
`check.py --selftest` 24 fixture runs, 0 wrong; `ctest -N` 256. Nothing under
`src/`, `include/`, `tests/`, `examples/` or `capi/` changed.

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
