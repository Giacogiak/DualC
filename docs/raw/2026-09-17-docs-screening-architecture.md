# Screening — `ARCHITECTURE.md`, `study/`, quality report, root files

Read-only screening, 2026-09-17.

Every file in scope read in full; `roadmap/README.md` and `command_reference/README.md` skimmed for
references only. Caps: ~300 lines / ~2,500 words / ~15 KB. Line numbers refer to commit `3d73220`.

## 1. Inventory

| File | Lines | KB | Words | `##` sections | Footer | Inbound (roadmap README / cmdref README / CLAUDE.md) |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `docs/ARCHITECTURE.md` | 1,265 | 64.6 | 8,915 | 1 Pipeline · 2 HermiteOctree · 3 Sampler · 4 Contourer · 5 GC integration · 6 Conventions · 7 Parameters · 8 Vendored · 9 Limitations · 10 Implicit layer | **N** | 1 / 0 / 1 (+README.md, STRUCTURE.md, 14 refs from roadmap children) |
| `docs/report-quality-inspection-contouring.md` | 499 | 26.1 | 4,025 | Test case · Exec summary · Root-cause · Measured results · Workflow optimization · Recommended workflow · Proposed enhancement · Monolithic vs per-tile · Verification · Appendix | **N** | 0 / 0 / 1 (+STRUCTURE.md; 12/README:105, 12/06:12; cmdref 11/05:24,79; study A3:216) |
| `docs/study/README.md` | 225 | 15.0 | 2,327 | What is in here · Before you start · Three-day schedule · Seven things · Six things to volunteer · Notes | N (index) | 1 / 0 / 1 (+README.md, STRUCTURE.md, 17/README:5, 17/05:5, 17/06:3) |
| `docs/study/A1-why-dual-contouring.md` | 481 | 29.1 | 4,624 | 11 unnumbered `##` | **N** | never cited by any roadmap file |
| `docs/study/A2-sampling-octree-oracles.md` | 456 | 57.5 | 8,697 | §1–§11 + 3 fixed | **N** | 17/03:49,187; 17/04:34,245 |
| `docs/study/A3-the-qef.md` | 310 | 35.3 | 5,575 | §1–§11 + 3 fixed | **N** | 17/04:118 |
| `docs/study/A4-contouring-recursion.md` | 431 | 45.9 | 6,950 | §1–§10 + 3 fixed | **N** | 17/03:187; 17/04:57 |
| `docs/study/A5-implicit-field-algebra.md` | 459 | 56.0 | 8,598 | §1–§12 + 3 fixed | **N** | 17/03:141; 17/04:243 |
| `docs/study/B1-architecture.md` | 187 | 28.0 | 4,172 | §1–§7 + 3 fixed | **N** | never cited by any roadmap file |
| `docs/study/B2-api-and-type-design.md` | 249 | 39.7 | 5,894 | §1–§10 + 3 fixed | **N** | 17/03:164; 17/04:242 |
| `docs/study/B3-memory-ownership-performance.md` | 333 | 44.7 | 6,692 | §1–§10 + 3 fixed | **N** | 17/03:186; 17/04:33,244 |
| `docs/study/B4-concurrency.md` | 231 | 33.1 | 5,122 | §1–§10 + 3 fixed | **N** | 17/03:38; 17/04:72,88,244 |
| `docs/study/B5-build-dependencies-licensing.md` | 261 | 38.0 | 5,149 | §1–§11 + 3 fixed | **N** | 17/04:103,189 |
| `docs/study/T-unit-testing.md` | 587 | 81.3 | 11,790 | §1–§8 + 3 fixed | **N** | 17/04:88,118,190; 17/06 |
| `README.md` | 270 | 11.8 | 1,587 | Dependency layout · Build · Consume · Minimal usage · TPMS · Export formats · Slice · Viewer · Raymarch (+ field_view) · Demo meshes · Documentation · License | N | 0 / 0 / 1 |
| `CLAUDE.md` | 130 | 7.0 | 879 | Architecture · Build & test · Conventions · CLI tools · Where to look | N | STRUCTURE.md:16 only |
| `STRUCTURE.md` | 271 | 24.4 | 2,105 | (tree) · Build targets at a glance | N | 0 / 0 / 2 |

Not staged: `docs/study/dualc-explorer.html` (60 KB; listed in study README:57, STRUCTURE.md:35) and
`docs/study/figures/` (24 SVGs, one 655 KB; each referenced exactly once from the A/B/T files; no README
lists them by name). **None** of the 17 files above has a back-link footer; ARCHITECTURE.md ends mid-prose.

## 2. `docs/ARCHITECTURE.md`

### 2.1 Section map

| § | Lines | Len | Nature |
| --- | --- | ---: | --- |
| status banner | 3–11 | 9 | revision note ("This revision brings the document back in line…") — residue |
| intro | 13–23 | 11 | architectural |
| 1 Pipeline at a glance | 27–83 | 57 | architectural; 76–81 bug-history parenthetical (signMethod drop) |
| 2 HermiteOctree contract | 85–136 | 52 | architectural (core) |
| 3 Sampler: 3.1 bounds 157, 3.2 parallel 184, 3.3 refinement 228, miss ladder 286, why geometric 330, 3.4 sign 345 | 138–428 | 291 | architectural; 373–382 dated change record ("Since 2026-09-01 … 4 ULP … ~84× band") = 17 #23; 405–410 "Since 2026-09-09" = 17 #26 |
| 4 Contourer: 4.1 recursion 462, 4.2 mixed depth 495, 4.3 emission 536, 4.4 QEF 623, 4.5 MDC 667, 4.6 collapse 742 | 430–845 | 416 | architectural with measurement records (737–740, 841–843 molde numbers) and defect history (584–587, 608–617, 806–815) |
| 5 GC integration | 847–870 | 24 | architectural |
| 6 Conventions (corner/edge/child indexing, descent tables, four-cell order) | 872–961 | 90 | reference tables; 939–959 duplicates §4.3 551–566 (kQuadCCWPlus stated twice) |
| 7 Configuration parameters | 963–990 | 28 | usage/reference (param tables = header comments) |
| 8 Vendored third-party | 992–1018 | 27 | policy; duplicates `THIRD_PARTY.md` + CLAUDE.md 79–81 |
| 9 Limitations & future work | 1020–1113 | 94 | 2 live limitations (1034–1055, 1064–1079) + 4 "Fixed since this list was written" callouts (1026–1033, 1057–1062, 1080–1088, 1089–1094) + 2 notes; ≈45 of 94 lines are history |
| 10 Implicit layer: 10.1 interface 1124, 10.2 flow 1179, 10.3 sharp 1204, 10.4 library 1227, 10.5 unbounded 1257 | 1115–1265 | 151 | architectural; 10.4 is an inventory |

Totals (approx.): architectural ≈ 850 lines, historical/changelog ≈ 190, usage/inventory/reference ≈ 225.

### 2.2 Duplication with roadmap / command_reference

1. Molde numbers: ARCH 841–843 (34,328 V / 68,652 F, χ = 2) ⇔ `01/01-engineering-record.md:77-80`; ARCH
   737–740 ⇔ same table :80; third copy study A4:268,340.
2. Ray-parity epsilon retirement: ARCH 373–382 ⇔ `17/03:40-101` (#23).
3. Dead parameters deleted: ARCH 1089–1094 ⇔ `17/04:36-49` (#28).
4. parallelFor exception fix: ARCH 1080–1088 ⇔ `17/03:16` (#22).
5. Diagnostics channel: ARCH 1098–1107 ⇔ `17/05` and `command_reference/README.md:143-170`.
6. signMethod-drop bug: ARCH 76–81 and again 1026–1033 ⇔ `01/02-bug-catalogue.md:71`.
7. Four-cell ordering / kQuadCCWPlus: ARCH 562–566 and 939–959 ⇔ `01/01:95`.
8. Library inventory: ARCH 1235–1255 ⇔ `command_reference/README.md:190-206`.
9. `--collapse` semantics: ARCH 982 ⇔ `command_reference/README.md:100`.
10. Vendoring policy: ARCH 994–1016 ⇔ CLAUDE.md 79–81 ⇔ `THIRD_PARTY.md`; B5:144 quotes verbatim.

### 2.3 Does "living doc, edited in place, byte ceiling" hold?

No. (a) 4.2× the line cap, 4.3× bytes, 3.6× words — the largest maintained file outside the frozen study.
(b) 2026-08-31 → 09-09 it received five dated patches (378, 405, 1080, 1089, 1101) — used as a changelog,
which is roadmap 17's job. (c) §9 contradicts itself and §10.5: 1098 "Fallbacks on bad bounds — **no
longer silent**" vs 1264–1265 "an *invalid* box … **silently degrades** to `BBox::unit()`" vs 166–170.
(d) `17/10:136-139` records the ceiling as "bumped in the same commit" — a ceiling that moves with every
edit is not a ceiling. (e) `17/09:77-79` still says "Eight files are declared frozen" while roadmap
README:176 and cmdref README:33 say none. A zero-context agent looking for parameter defaults (§7, 28
lines) must load 66 KB.

### 2.4 Proposed split (README + numbered children)

| Child | Source sections (lines) | Est. lines after dedupe |
| --- | --- | ---: |
| `README.md` | intro 13–23, §1 27–75 (drop 76–81), §2 85–136, §10.2 1179–1202, index | ~180 |
| `01-sampler.md` | §3.1–3.3 + miss ladder + why-geometric 157–344 | ~190 |
| `02-sign-oracles.md` | §3.4 345–428 minus 373–382 (→ link 17 #23) | ~75 |
| `03-contourer-recursion.md` | §4 intro + 4.1–4.3 430–621, drop second kQuadCCWPlus copy | ~180 |
| `04-qef-manifold-collapse.md` | §4.4–4.6 623–845, molde numbers → link `01/01:70-80` | ~200 |
| `05-conventions-and-tables.md` | §6 872–961 + §5 847–870 | ~110 |
| `06-parameters-and-vendoring.md` | §7 963–990 + §8 992–1018 | ~55 |
| `07-limitations.md` | §9 1034–1055, 1064–1079, 1108–1111 only; "Fixed since" → one line each linking 17 | ~55 |
| `08-implicit-field-layer.md` | §10.1, 10.3, 10.4, 10.5 1124–1177, 1204–1265; 10.4 inventory → link cmdref appendix | ~120 |

### 2.5 Staleness signals

Dates: 378 (09-01), 405 (09-09), 1080 (08-31), 1089 (09-01), 1101 (09-09). Relative/status words: 3
"This revision", 78 "It used to", 373 "used to be", 759 "used to leave", 761 "today", 1026/1057/1080/1089
"Fixed since this list was written", 1098 "no longer silent", 1104 "used to list". Counts: 215 "51-line
header", 1241 "30 analytic primitives plus 6 TPMS". Contradiction §9 1098–1107 vs §10.5 1263–1265.

## 3. `docs/study/`

**README structure**: two status callouts (3–29, added 08-21 and 08-31), audience framing (31–38 "You
commissioned this library; it was written by Claude Code. In three days you need to be able to defend…"),
document table (44–57, 11 docs + explorer), "Before you start" (68–93), a **three-day reading schedule**
(97–124), "seven things to have ready cold" (128–172), "six things to volunteer" (176–211), notes
(215–225). It is an interview-prep pack index, not a repo index.

**Lettering vs NN convention.** `A1–A5`, `B1–B5`, `T` conflicts with `NN-slug.md`. Because the pack is
declared verbatim/frozen and roadmap 17 cites it by **file:line** 30 times (`docs/study/B4-concurrency.md:132-143`
at 17/03:38; `A2-…:317` at 17/04:34; …), renaming or reflowing would silently break those citations.
Keep the letters; state the exception where the convention is declared; never reflow these files
(17/10:140–144 says the 88-column reflow excluded baselined files — confirm study/ was excluded).

**Size vs cap.** Every A/B/T file exceeds the byte cap 1.9×–5.4× and the word cap 1.7×–4.7×; exempt by
declaration (17/09:79–80 "all of `docs/study/` is a verbatim snapshot"). `study/README.md` at 15,339 B is
**over** 15,000 and has been amended twice.

**Referenced from**: CLAUDE.md:117, README.md:261, STRUCTURE.md:22–36, roadmap README:170, 17/README:4–5,
17/05:5, 17/06:3,9, 17/09:80. Roadmap children cite 9 of 11 documents; **A1 and B1 are cited nowhere**
outside the study README. command_reference never cites the study (correct).

**Assets.** Explorer indexed (README:57, STRUCTURE.md:35). Figures: all 24 referenced from documents;
STRUCTURE.md:36 says "24 SVG figures"; **no README names them** — a `figures/README.md` (file → used by)
or a table in the study README is needed. The 655 KB SVG is an asset outside the doc-size contract.

**Study content duplicating ARCHITECTURE.md**: Hermite structs ARCH 93–111 ⇔ A1:164–175, B3:20–26;
mesh adapter ARCH 143–150 ⇔ A1:255–259, A2:3, B1:31–38; miss ladder ARCH 294–318 ⇔ A2:171–180; sign
oracle snippets ARCH 354–360/386–390/414–415 ⇔ A2:225–231/255–258/270; `cellProc`, `childOrSelf`,
finest-cell rule, `kQuadCCWPlus`, degenerate-quad, `comp < 0`, inside-pair, soft clamp ARCH
464–475/500–506/515–525/554–560/573–582/594–599/711–716/643–653 ⇔ A4:57–66/102–108/116–128/155–161/
27–35/254–260/237–240 and A3:190–204; bit-identical four-step argument ARCH 204–211 ⇔ B4:97–100 ⇔ study
README:160–165; corner/edge tables ARCH 878–916 ⇔ A1:179–199; `ImplicitField` sketch ARCH 1129–1140 ⇔
B2:13–22; LGPL sentence ARCH 1013–1016 quoted at B5:144 as "`docs/ARCHITECTURE.md:236`" — the study
cites the *pre-rewrite* ARCHITECTURE at A2:142 ":274", A2:219 ":216 and :248", B2:24 ":263-281",
B5:144/232/258 ":236"; all dangling. Acceptable only because the study is frozen; each study file's
stale-ARCHITECTURE passages (A1:322–344, A1:405–416, A2:142, A2:219, A4:3, A4:41–45, B1:172–174, B3:84,
B3:318) are neutralised only by README:68–76, which a reader landing on A4 directly never sees.

## 4. `docs/report-quality-inspection-contouring.md`

A dated (2026-07-04/05) one-off investigation of a gyroid-shell contouring complaint — root cause, four
measurement steps, recommended workflow, a proposal (`--decimate`/`--simplify`) implemented the next day,
and an Approach-A/B comparison. It is a **development-record entry**: dated, status-bearing (3–13 "Status:
inspection complete… Update (2026-07-05): implemented"), outcome recorded inside (323–328), a deferred
item (Approach B) that roadmap README:90–91 tracks. 1.7× the line cap, 1.7× bytes, 1.6× words.

Indexed from: CLAUDE.md:118, STRUCTURE.md:21, 12/README:105, 12/06:12, cmdref 11/05:24,79, study A3:216.
**Not** in roadmap/README.md, not in command_reference/README.md, no docs-level index exists. Stale
internally: 75 `docs/command_reference/11-dualc_field.md` and 115 `docs/roadmap/11-dense-lattice-deliverable.md`
(folders since 09-11); 31 "Constraints confirmed with the requester", 238 "per the requester's
instruction", 25 "Reported symptoms"; 263 "GPU (deferred, roadmap #15)".

Recommendation: move into the roadmap as `12-field-graph-and-app/07-quality-inspection-gyroid-shell.md`
(the evidence for § G, which already links it) — or a new numbered topic folder with README (test case +
summary + workflow, ~120 lines), `01-measurements.md` (119–241), `02-decimation-approaches.md` (358–454).
Strip the status banner (status lives in 12 § G), fix the two paths, add the footer, repoint 6 inbound links.

## 5. `README.md` / `CLAUDE.md` / `STRUCTURE.md`

**README.md**: v1-era pitch (3 "on triangle meshes" — the field path, v2 layer and `dualc_field` absent
from 1–14), dependency layout, build, consume, minimal usage (mesh-only), then per-tool sections (TPMS 88,
Export 97, Slice 116, Viewer 132, Raymarch 162, field_view 196) duplicating command_reference pages,
demo meshes, Documentation table, licence. Duplication: Export 99–114 ⇔ cmdref README:172–186
near-verbatim; Build 32–46 ⇔ CLAUDE.md:40–48; controls 191–193 ⇔ cmdref README:61–88; demo-mesh table
231–239 ⇔ `09`. Numbers: 215 "all 30 analytic primitives", 217 "73/73" (study says 62/62, roadmap 13 says
30/30), 241 "All seven are generated" vs 8-row table vs cmdref "9 generated", 253 "Five entry points",
260 "(items #22–#35)", 268 "Two vendored source files". Staleness: 51 "today"; 60–63 "once the planned
submodule conversion lands" contradicts CLAUDE.md:82–83 "no FetchContent, no submodule"; 65 "Phase 2 of
Tier 3 #8"; 213, 218 dates; 224 `12-dualc_field_view.md` (folder); 261 "verbatim 2026-08-19 snapshot".

**CLAUDE.md** is *not* a pointer file. Pointer content: §Where to look (102–120) and current-focus
(122–130). Restated: §Architecture 7–32 (compressed ARCH §1/§10), §Build & test 34–67, §Conventions 69–85
(THIRD_PARTY + ARCH §8 + export rule), §CLI tools 87–100 (12-tool list = cmdref README:16–31). Numbers
despite 127–130's claim: 72 "the two vendored files", 85 "1 world unit = 1 mm", 117 "~70k words, verbatim
2026-08-19 snapshot". Contradiction: 99–100 "**future** raymarch side-car / C ABI" vs 122–124 "the C ABI
all shipped". ≈60 % restates facts with another home; the gate (17/10:68) only forbids counts/versions/
item ranges.

**STRUCTURE.md**: full tree plus one `##`; the `structure` check's oracle (17/09:92–95). Drift: 168 & 270
"9 always-on CLIs + **2** opt-in viewers" (four opt-in GL targets, listed 188–192); 211 "**9** extern C
entry points" vs B5:168 "eleven" vs 18 (0.4.0 twins); 23 "the **13** documents" vs 17/README:4 "11"; 108–109
"(frozen)" vs roadmap README:176; seven "split 2026-09-11" dates and others (70, 86, 87, 99, 111, 117, 121)
— tree annotations are a second status ledger. `dualc_field_view` and `dualc_glsl_parity` missing from
263–271 "Build targets at a glance".

Cross-file: README §Documentation (251–264) and CLAUDE §Where to look are two indexes of the same five
targets with different descriptions.

## 6. `docs/` root

No `docs/README.md`. Loose: `ARCHITECTURE.md` (66 KB) and `report-quality-inspection-contouring.md` (26 KB);
three folders, of which only `study/` lacks an NN scheme. A `docs/README.md` (≤60 lines) should hold: one
sentence per child (what class of fact lives there), the convention block (NN numbering, README-per-level,
footer, caps, the study exemption and why), the "one home per fact" table, nothing else. Then README.md
§Documentation and CLAUDE.md §Where to look shrink to one link each.

## 7. Chat residue and zero-context failures

- **Study README**: 31 "You commissioned this library; it was written by Claude Code. In three days…"; 64
  "reread these on the morning"; 97–124 three-day schedule; 101 "an interviewer is most likely to probe";
  176 "six things to volunteer before you are asked"; 211 "the first test you would write tomorrow";
  23–29 "this folder used to hold its own `ARCHITECTURE.md`… deleted"; 11 dangling
  `01-core-dual-contouring.md` §4.9; 20 "13 worth work are tracked items #22–#34" vs "14 … #22–#35".
- **Every A/B/T file**: header "**Read this after:** … **Time:** 45 min"; closing "## If they ask…" with
  rehearsed first-person answers (55 "I would/I'd"); coaching imperatives — A2:102, A2:188, A4:45, A5:270,
  B2:43, B3:118, B4:85, T:130, A1:340.
- **Dangling folder paths** (files promoted 09-11): A1:343,416; A3:214; A4:45,132,248,400; B5:144; study
  README:11; report:75, :115; README.md:224.
- **Dangling line refs into rewritten ARCHITECTURE**: A2:142, A2:219, B2:24, B5:144/232/258.
- **Report**: 31, 238 "the requester"; 25; 3–13 banner.
- **ARCHITECTURE**: 3–11; 1026/1057/1080/1089; 761 "today"; five absolute dates.
- **README.md**: 51 "today"; 62 submodule sentence; 213/218.
- **CLAUDE.md**: 99–100 "future".
- **STRUCTURE.md**: 108–109 "(frozen…)"; 15 absolute dates in annotations.
- **Roadmap-side**: `17/09:77-79` "Eight files are declared frozen" vs README:176 / cmdref README:33;
  roadmap README:168–169 "(frozen)".
- No `TODO`, `FIXME`, `DRIFT-PENDING`, `TBD` markers in any screened file.

## 8. Top 10 fixes (screener's ranking)

1. Split `ARCHITECTURE.md` per §2.4; strip "Fixed since" callouts and dated change records into one-line
   links to roadmap 17; resolve §9/§10.5 contradiction; drop duplicate kQuadCCWPlus and §1 bug parenthetical;
   retire the byte-ceiling exemption in `check_data.json`.
2. Create `docs/README.md` (≤60 lines); cut README.md §Documentation and CLAUDE.md §Where to look to one
   pointer each.
3. Move the quality report into the roadmap; delete banner; fix paths; footer; repoint 6 inbound links.
4. Fix every dangling folder path (12 sites) — for frozen study files via a "path errata" table in the
   study README so line numbers stay intact.
5. Add back-link footers to ARCHITECTURE (or children), the report, and one-line footers to each A/B/T file.
6. Make CLAUDE.md a pointer file; remove "future" at 99–100; remove "~70k words / 2026-08-19" at 117.
7. Rewrite README.md 1–14 and 48–66 for v2; delete the submodule sentence; fix 224; reconcile 241 with
   the table and cmdref; drop 217 "73/73" and 213/218 dates.
8. Index the study assets (`figures/README.md`, 24 rows); flag the 655 KB SVG; add missing A1/B1 citations
   or state they are background only.
9. Reconcile STRUCTURE.md annotations (168/270, 211, 23, 108–109; add `dualc_field_view`/`dualc_glsl_parity`
   to Build targets); strip the 15 dates from tree annotations.
10. Fix the contract docs themselves: `17/09:77-79`, roadmap README:168–169; decide whether the study
    README is frozen or maintained and stop appending status notes to it.
