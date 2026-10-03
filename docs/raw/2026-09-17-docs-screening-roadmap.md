# Screening — `docs/roadmap/`

Read-only screening, 2026-09-17.

Scope: all 49 `.md` files under `docs/roadmap/` read in full, plus `CLAUDE.md` and `STRUCTURE.md`.
Link/anchor resolution checked mechanically: all intra-roadmap links and `#anchors` resolve; 37 links
point outside the roadmap. Line numbers refer to the working tree at commit `3d73220`.

## 1. Inventory

Cap = 300 lines / 2,500 words / 15 KB. Footer: `Y` = contract footer is the last paragraph; `Y+` = footer
present but followed by extra links.

| File | Lines | KB | Words | First heading | Status claims found | Footer |
| --- | --- | --- | --- | --- | --- | --- |
| README.md | 191 | **16.1** | 1958 | DualC — Roadmap & development record | index: every block; #22–#35; milestones table | n/a |
| 01-core-dual-contouring/README.md | 131 | 8.5 | 1128 | Core dual contouring — the DC engine | Tier-1/2 items 1–6 `[DONE]` (no dates); #13/#14/#15 open, no status word | Y |
| 01-…/01-engineering-record.md | 106 | 8.5 | 1230 | Engineering record (end-of-v2 handoff) | none (record) | Y (double `---` 102–104) |
| 01-…/02-bug-catalogue.md | 162 | 12.3 | 1680 | Significant bugs and their fixes (§ 4) | § 4.9 dated 08-21 | Y |
| 02-implicit-sdf-foundation.md | 204 | 10.5 | 1343 | Implicit / SDF foundation | Phase 1–6 "Done"; "v2 complete"; CSG parser "deferred to v3" | Y |
| 03-booleans-csg.md | 57 | 3.1 | 339 | Booleans & CSG | Phase 2 DONE; open-items blockquote | Y |
| 04-primitives-and-operators.md | 100 | 5.3 | 575 | Primitives & operators | Phases 3/4/5 DONE (in headings) | Y |
| 05-tpms-lattices/README.md | 48 | 9.0 | 1234 | TPMS lattices | #16 DONE 05-26; #20 DONE 06-01; #18 DEFERRED 05-31 | Y |
| 05-…/01-strut-lattices.md | 79 | 4.7 | 609 | #17 Strut-based lattices | #17 DONE 07-09 | Y |
| 05-…/02-strut-enhancements.md | 169 | 10.4 | 1300 | #17b Strut-lattice enhancements | #17b DONE 07-09; Ph3 07-07; Ph4 07-09 | Y |
| 05-…/03-crystal-blending.md | 97 | 6.0 | 819 | #17b Phase 5 — crystal blending | Ph5 DONE 07-09 | Y |
| 06-slice.md | 28 | 1.7 | 204 | Field-on-plane slicing | Step 1 DONE 06-02 (heading) | Y |
| 07-viewer-polyscope.md | 96 | 7.5 | 950 | Interactive viewer — `dualc_view` | #10 DONE 05-31; #21 DONE 06-09 (headings) | Y |
| 08-raymarch.md | 83 | 5.3 | 684 | Analytic raymarch viewer | Step 3 DONE 06-03; generalisation DONE 06-14 (headings) | Y |
| 09-io-formats.md | 63 | 4.3 | 554 | Input & output formats | #7 DEFERRED 05-26; Step 2 DONE 06-02 (headings) | Y |
| 10-infrastructure-and-integration.md | 161 | 13.5 | 1710 | Infrastructure & integration | #19 Ph1 DONE 06-17; #8 Ph1 DONE/Ph2 DEFERRED; #9, #11 DEFERRED; #12 DONE (headings) | Y |
| 11-dense-lattice-deliverable/README.md | 91 | 5.0 | 688 | Deliverable: visualizing & manufacturing… | step 4 → Boletus; step 6 DEFERRED; table steps 1–6 | Y |
| 11-…/01-decision.md | 61 | 2.9 | 443 | The decision | dated 06-02 | Y (double `---`) |
| 11-…/02-phase-0-benchmark.md | 56 | 3.0 | 482 | Phase 0 benchmark | dated 06-12 | Y (double `---`) |
| 11-…/03-streaming-export.md | 157 | 11.5 | 1519 | Step 5 — tiled / streaming export | DONE (STL 06-15, 3MF 07-03, --mem 07-06); 5a.3 DEFERRED | Y |
| 11-…/04-streaming-3mf-record.md | 107 | 6.9 | 932 | § 5b — implementation record | dated 07-03 | Y |
| 12-field-graph-and-app/README.md | 120 | 8.9 | 1146 | Field-graph & standalone raymarch app | builds DONE; C ABI DONE; E → Boletus; table A–G | Y |
| 12-…/01-field-graph.md | 137 | 8.1 | 1002 | Field-graph (§ A) and the `dualc_field` CLI (§ C) | A DONE 06-14 (DAG-refs DEFERRED); C DONE | Y |
| 12-…/02-glsl-codegen.md | 137 | 9.5 | 1161 | Field→GLSL codegen contract (§ B) | B DONE 06-14 | Y |
| 12-…/03-raymarch-app.md | 109 | 7.4 | 978 | Standalone raymarch app (§ D) | D DONE 06-14; planes 06-16; file-watch 07-03; IPC DEFERRED | Y |
| 12-…/04-preview-performance.md | 128 | 9.5 | 1298 | Viewer performance (§ D.2) | adaptive DONE 07-09; dGPU DONE 07-09; startup DEFERRED | Y |
| 12-…/05-open-surface.md | 113 | 7.4 | 997 | Isolating the open lattice surface (§ F) | VALIDATED 06-16; clip mask DEFERRED | Y |
| 12-…/06-decimation.md | 50 | 2.9 | 344 | Post-contour QEM decimation (§ G) | A DONE 07-05; B DEFERRED | Y |
| 13-graded-tpms/README.md | 81 | 4.3 | 552 | Graded TPMS | header "design only" + "Superseded — DONE"; wavelength DROPPED | Y |
| 13-…/01-analysis.md | 133 | 7.4 | 1071 | Analysis: motivation… | none | Y |
| 13-…/02-design-and-plan.md | 168 | 9.8 | 1236 | Design and plan: `graded-onion` | none | Y |
| 14-c-abi/README.md | 59 | 3.6 | 420 | C ABI — native-consumer boundary | #19 DONE 06-17; § 9 bullets | Y+ (57–59 "related:") |
| 14-…/01-design.md | 60 | 3.3 | 468 | Why the C ABI exists (§ 1, § 2, § 5) | none | Y |
| 14-…/02-surface-and-contract.md | 101 | 7.9 | 1016 | The surface and the contract (§ 3, § 4) | none (reference tables) | Y |
| 14-…/03-implementation-and-verification.md | 98 | 7.1 | 851 | Implementation map, build & deployment | § 8 dated 06-17 | Y |
| 15-boletus-handoff.md | 178 | 12.1 | 1596 | Boletus hand-off | table DONE/IN PROGRESS/PLANNED; dGPU DONE 07-09 | Y |
| 16-field-graph-and-app-continued.md | 185 | 12.4 | 1784 | Field-graph & app — continued | § H DONE 08-25; 3 DEFERRED; correction 09-01 | Y |
| 17-code-audit-and-hardening/README.md | 160 | 13.0 | 1675 | Code audit & engine hardening | verification table; updates 08-31…09-11; table #22–#35 | Y |
| 17-…/01-findings-ledger-engine.md | 76 | 10.6 | 1565 | Findings ledger — engine | E1–E33 | Y |
| 17-…/02-findings-ledger-craft.md | 92 | 14.1 | 1917 | Findings ledger — craft | C1–C41 | Y |
| 17-…/03-correctness-and-robustness.md | 193 | 13.9 | 1956 | Tracked items — Correctness (#22–#26) | #22 DONE, #23 DONE, #24 DEF, #25 DEF, #26 DONE | Y |
| 17-…/04-engineering-quality.md | 268 | **18.7** | 2448 | Tracked items — Engineering quality (#27–#35) | #27 DEF, #28–#30 DONE, #31 DEF, #32 PLANNED, #33 PARTIAL, #34 PARTIAL, #35 PLANNED | Y |
| 17-…/05-diagnostics-channel.md | 273 | **17.0** | 2322 | The Diagnostics channel (#26) | DONE 09-09; per-probe clamp DEFERRED | Y |
| 17-…/06-findings-ledger-testing.md | 43 | 4.3 | 590 | Findings ledger — testing | C42–C54 | Y |
| 17-…/07-repeat-tiling-fix.md | 117 | 6.3 | 940 | `repeat` tiling — the single-fold defect (#34) | DONE 09-10 | Y |
| 17-…/08-argument-validation.md | 147 | 7.5 | 1086 | Argument validation and the `BBox{}` sentinel (#34) | 3× DONE 09-10 | Y |
| 17-…/09-local-checks-gate.md | 194 | 12.1 | 1867 | The local checks gate (#33) | DONE 09-10; follow-up 09-11 | Y |
| 17-…/10-docs-system-screening.md | 166 | 12.5 | 1806 | Docs-system screening | DONE 09-11; R5 DONE 09-11 | Y |
| 18-c-abi-continued.md | 36 | 1.8 | 218 | C ABI — continued | twins DONE 09-09; unknown keys DONE 09-11 | Y |

**Over the byte cap:** `17/04` (18.7 KB), `17/05` (17.0 KB), `README.md` (16.1 KB). Near cap: `17/02`
(14.1), `17/03` (13.9), `10` (13.5), `17/README` (13.0). Markers `Reconstructed (date) from commit` and
`DRIFT-PENDING:` are used **nowhere** in the roadmap.

## 2. Index completeness

**Roadmap README → blocks.** `README.md:151-170` lists every `NN-*` exactly once; no phantom rows, no
orphans. Defects: row order is `…16, 18, 17` (`:169-170`; `STRUCTURE.md:108-110` mirrors it); status cell
of row 12 (`:164`) contains `VALIDATED`, not a legend word.

**Folder READMEs → children:** 01 (`README:12-15`), 05 (`:22-26`), 11 (`:27-32`), 12 (`:71-78`), 13
(`:17-20`), 14 (`:27-31`) all complete. 17 (`:15-45`) lists all ten but in non-numeric order (06 after 02,
05 after 08).

**README section order:** purpose (1-7), legend (9-12), snapshot note (14-16), Current focus (18), Next up
(62), Principal blocks (149), How the blocks relate (179) ✓. Extra material inside Current focus
(milestones table 30-40, keystone list 42-60) and a "Legacy files (frozen)" paragraph (172-177) that is
internally contradictory.

**Linking granularity:** 21 links cite a § or item but land on a folder README without an anchor
(`README.md:48,51,90,98,111,123,132,138,141`; `03:24`; `15:25,28,39,42,92,115`; `16:180`). Folder README
page tables carry no § anchors, so reaching e.g. § D.1 from the index is 3 hops.

**Misdirected anchors (resolve, wrong page):** `15:149-150` "Discrete-GPU auto-selection" →
`12/03#d-…`; entry is at `12/04:50`. `12/README:101` "ranked fix menu in `[§ D](03-…)`"; menu is at
`12/04:73-124`. `16:180` "Unchanged from [12 § D]" — item lives in `12/04`.

## 3. Contract violations per file

**Size over cap** — `17/04`, `17/05`, `README.md`. `17/09:96-102` and `17/README:132-133` admit it;
`README.md:34` nevertheless claims "every oversize file split".

**Status/date in headings** (grandfathered): `02:125,143`; `03:29`; `04:14,55,78`; `06:8`; `07:7,44`;
`08:8,65`; `09:9,36`; `10:16,26,106,131,145,155`; `17/README:52`. Plus ~20 **bold pseudo-headings with
status** that are not linkable: `12/03:37,56,73`; `12/04:7,50,73`; `01/README:21,32,38,50,62,79`
(`[DONE]` inline); `15:145`.

**Footer** — none missing. `14/README:57-59` extends the footer. Doubled `---` at `01/01:102-104`,
`11/01:57-59`, `11/02:52-54`.

**Missing intro links** — `01/README:1-5` (no cmdref/related links); `02:1-8` (no cmdref link);
`13/README:1-11` (no cmdref link); `11/README` (no cmdref links). Heading hierarchy broken by splits:
`01/02:8` and `01/01:9` begin at `###`/`####`; also `11/03:8`, `11/04:6`, `05/03:7`.

**Non-chronological / contradictory in place:**
- `03:12-27` blockquote: updated 06-14 says export bug "still open", then "*Resolved 2026-06-12*" —
  resolution dated before the sentence asserting it is open.
- `13/README:8` "Status: design only — not yet scheduled." followed by "(Superseded — DONE 2026-06-17)";
  `13/README:27,77` "the chirp/PDE problems above" — now in `13/01`.
- `11/02:11` "The 2026-06-02 table above" — table is in `11/01`.
- `01/01:88` "(#3 below)" and `01/02:27` "see #6 below" — Tier-1 items now on `01/README`, written as
  `#N`, which `README.md:44-45` forbids (collides with block-12 `#3`).
- `02:163` "CSG expression-language parser is deferred to v3" — superseded by `dualc_field` (06-14),
  never annotated.
- `14/README:48-50` "streaming writer is STL-only today — see 11 § 5a" — 11 § 5a says 3MF + `--mem`
  shipped 07-03/07-06. No trigger.
- `16:4` "file 12 … is frozen"; `18:3-4` "Block 14 is frozen"; `README.md:168-169,172` "(frozen)" — vs
  `17/10:139-141` and `README.md:176` "No roadmap file is frozen today".
- `17/09:39-49` "Today (2026-09-11)" column: `frozen-decl` 8 files — same day `17/10:141` empties the
  list; 09 not annotated. `17/05:17-18` "The frozen `11-dualc_field.md` gets a one-line pointer" — now a
  folder.

**DONE without evidence:** `01/README:21-31` item 1 "[DONE]" but body is plan text; item 2 `:32-36`
likewise; `02:118-123` Phases 2–5 "Done (committed)" no hash/test count; `10:106-117` #8 Phase 1;
`12/README:100` D.1 section planes; `12/03:37-54` "bit-identical" asserted not measured; `12/README:96`
row A "DAG-refs **pending**" (off-legend); `05/03:80-88` closing evidence is "README index row
refreshed, auto-memory note updated".

**DEFERRED without trigger:** `12/05:84-87` clip mask; `README.md:137-140`; `12/README:100` / `12/03:52-54`
UI sliders; `12/01:22-25` `GridField`, 2D sub-grammar, `displaced`; `11/03:59-64` per-tile-object
collapse; `11/03:149-153`, `11/04:82-85` moving-front eviction; `13/02:85,164` `smooth=true`;
`14/README:48-52` 3MF/`--mem` behind ABI, DAG-ref serialization; `10:60` same.

**Numbers restated outside their home:** `README.md:26-27,70` "87 findings, 14 tracked items #22–#35";
`:84` "~12× at depth 7"; `:165` "parity 30/30"; `:166` "v0.3.0 … 0.4.0"; `:169` "0.4.0". `12/README:97`
"28/28", "62/62"; `:102` "11 flat entry points"; `:105` "5.19 M→519 k faces, p99 0.049 mm".
`11/README:81` "~49× peak-RAM cut". `15:38` "11 entry points at hand-off, 13 since 0.4.0" although
`15:14-15` says the count lives in `capi/README.md`. `05/02:38,45` "currently 66/66", "update it".
`STRUCTURE.md:211` "9 extern C entry points" (stale vs 13); `:75` "(DEFERRED)"; `:168,270` "2 opt-in
viewers" (four opt-in GL targets exist). `CLAUDE.md:61-64` names only `POLYSCOPE_VIEWER` and
`RAYMARCH_VIEWER`; `12/03:20` documents `-DDUALC_BUILD_FIELD_VIEW=ON`, `12/02:37` `-DDUALC_BUILD_GLSL_PARITY=ON`.

**Usage tables that belong in command_reference / capi:** `14/02:30-42` per-function ABI table and
`:49-56` param-mapping table (stale: no 0.4.0 twins); `17/05:72-84` `Diagnostics` field table; `06:11-14`
`--plane`, `--res N (default 512, max 4096)`; `12/04:28-29` `--preview-scale`; `11/03:66-67` `--mem`
suffixes; `12/03:39-41` key bindings; `07:9-11,50-53` control lists; `05/README:32` flag semantics.
`12/01:117-124` correctly replaced node tables with a pointer ✓ (model to follow).

**Duplicated content (both locations):**
1. Hermite-edge-tag idea for #14: `01/README:117-121` ≈ `12/05:105-109`.
2. Export-bug open item: `03:15-17,26-27` ≈ `11/02:46-50`; fix again at `09:44-47`.
3. "Why a C ABI rather than P/Invoke": `10:66-73` ≈ `14/01:21-27`.
4. #19 entry-point list/verification: `10:35-41,53-59` ≈ `14/02:16`, `14/03:74-92`, `14/README:35-43`,
   `14/03:18-38`, `12/README:102`.
5. Streaming-export status: `11/README:81`, `11/03:8-10`, `12/README:47-55`, `README.md:52-53,87-94`, `15:40`.
6. "Moved to Boletus" blockquote ×3 in `12/README:14-17,60-64,84-90`; plus `10:10-14`, `11/README:36-45`.
7. Discrete-GPU: `12/04:50-71` and `15:143-164`. 8. File-watch: `12/03:56-71` and `15:133-141`.
9. 228→229 correction: `16:160-164`, `17/README:88-93`, `17/05:43-44`.
10. #22–#35 status: `README.md:71-79`, `README.md:170`, `17/README:141-156`, bodies in `17/03-04`,
    `17/08:137-143` — five copies.
11. Boolean-over-lattice "10–49 min vs ~40 s": `11/02:34-44`, `12/README:35-38`, `03:18-22`.
12. Decorator list: `02:43`, `03:41-43`, `04:10-12`. 13. Section planes: `12/03:37-54`, `12/05:53-60`,
    `README.md:141-143`. 14. `12/01:45-67` "Implemented" vs `:69-82` "Original design" restate the same facts.
15. `05/02` Phase 3 and 4 each carry "shipped as specced" + the full spec. 16. `17/README:74-133`
    "Update" paragraphs duplicate `README.md:30-40` and per-item bodies. 17. 2026-09-11 seven checks:
    `17/09:54-73` ≈ `17/10:61-71`.

## 4. Factual conflicts

1. **C ABI entry points at 0.2.0**: `10:35` "11 (at 0.2.0; 13 since 0.4.0)", `14/03:87` "exactly the 11
   (at 0.2.0)" vs `14/README:12-13` "the original 9 + the v0.3.0 twins"; `10:53` "11 exports" dated
   06-17 while v0.3.0 landed 06-19 (`10:56`). `STRUCTURE.md:211` says 9.
2. **Frozen files**: `README.md:168,169,172`, `16:4`, `18:3-4`, `17/09:39-49` vs `README.md:176`, `17/10:139-141,157`.
3. **"Every oversize file split"** (`README.md:34`) vs three files over 15 KB.
4. **ctest 06-14 vs 06-17**: `12/03:29` "171/171" vs `13/README:57` "157/157" three days later.
5. **`#3`**: streaming export (`README.md:24`, `14/01:11`, `12/README:47`); "pillar #3" = kill boolean
   round-trip (`12/README:43`, `12/01:72`); multi-threading (`01/01:88`).
6. **`#1/#2` inside 12**: `12/README:42-47` "pillar #N" and "#N build" for different things.
7. **Primitive count**: `04:22`/`02:149` "30" vs `12/README:97` "~29" vs `12/02:70` "~23 remaining".
8. **Study document count**: `17/README:4` "11" vs `STRUCTURE.md:23` "13".
9. **Test-count date**: `17/09:110` "231 on 2026-09-01" vs `17/README:78,86` (231 after 08-31; 235 on 09-01).
10. **Parity 07-09**: `12/04:17` "68/68" vs `05/03:78` "69/69" same day, order unstated.
11. **Boolean export bug**: `03:25` "still open" vs `03:26-27`, `11/02:49`, `09:44-47` fixed 06-12.
12. **Open-surface wall thickness**: `05/README:38` "~2× off; pending" vs `12/05:38` "Wall ≈ 2·thickness".
13. **Opt-in build options**: `CLAUDE.md:61-64` two vs `12/03:20`, `12/02:37`, `STRUCTURE.md:168`.
14. **`user_guidance.md`**: `07:92` cites "§8" of a file that does not exist.
15. **Smoke-test count**: `02:52-56` "four binaries" vs `02:158` "registers five".
16. **NEXT** defined in legend (`README.md:9`), used nowhere (`:28` prose "Next candidates").

## 5. Navigation test (zero-context, ≤ 2 hops)

| # | Question | Path | ≤2? |
| --- | --- | --- | --- |
| 1 | Status of #17? | `README:157` → `05/README:24` → `05/01:9` | Yes (2) |
| 2 | Streaming export record? | `README:52-53` → `11/README:31` → `11/03` | Yes (2) |
| 3 | What is NEXT? | `README:28` prose only; no NEXT status anywhere | Partial |
| 4 | What remains in #34? | `README:74-76` → `17/04:192-240` (three copies differ) | Yes (1) |
| 5 | Why was graded wavelength dropped? | `README:165` → `13/README:26-28` → `13/01:83-129` | Yes (2) |
| 6 | ABI version / entry-point count? | `README:166` → `14/README:13` → `capi/README.md`; `10:35` contradicts | Yes, conflicting |
| 7 | What does `check.py` enforce? | `CLAUDE.md:59` → `17/09`; README never names the file | Via CLAUDE only |
| 8 | Was `--normalize-thickness` broken? | `README:157` → `05/README:34-38` | Yes (1) |
| 9 | Can `dualc_lattice` stream? | `README:87-94` → `11/03:117-127` | Yes (1) |
| 10 | Rhino side-car status? | `README:58-60` → `15:28` PLANNED | Yes (1) |
| 11 | Section planes record? | `README:141` → `12/README:100` (no link) → `:75` → `12/03:37` | No (3) |
| 12 | Item #12 (demo data)? | `README:162` row 10 omits #10/#12/#21; scan headings | Marginal |
| 13 | Latest ctest/parity? | README milestones give none → `17/README:133` | Yes (1) |
| 14 | Open-surface clip mask built? | `README:137-140` → `12/05:52` | Yes (1) |

## 6. Structural observations

- **12 vs 16, 14 vs 18.** Continuations exist because parents were frozen; parents are now split folders
  and nothing is frozen. `16` § H belongs as `12/07-…`; `18` as `14/04-…`. `17/10:157` itself says "never
  a continuation of a file that could simply be split". `18` is 36 lines; its first entry points at `17/05`.
- **11 vs 06/08/09 vs 12.** Block 11's delivered steps live in 06, 08, 09 (single-entry pages 28–83 lines)
  while `11/README:75-82` and `12/README:40-58` restate them.
- **11 vs 13 vs 05.** graded-onion (13) and graded-offset (`05/02`) are sibling control-field decorators in
  different blocks.
- **10 vs 14.** #19 has a 90-line "Phase 1 realized" section in `10:16-104` (dead API sketch `:74-94`)
  plus the full 14 folder; 10 holds the "why C ABI" argument verbatim.
- **17/03-04 vs 17/05,07,08.** `17/04` #34 (`:192-240`) is a 50-line record pushing the file over cap;
  `17/08:137-143` re-lists the open bullets. **17/09 vs 17/10** overlap on the 09-11 checks.
- **Item IDs in several files:** #19 (10, 11/README, 11/01, 12/README, 14/*, 15, 07:32, 08:15, README);
  #34 (17/README, 17/04, 17/07, 17/08, 17/10, README); #26 (17/README, 03, 05, 07, 08, 02-ledger, 18,
  README); #18 (05, 10:151, 11/README, 17/03:155-160, README:84); #13/#14 (01/README, 12/05, README:96-98).
- **Mostly-pointer files:** 18; 06; 03; `14/README` § 9; `11/README`; `12/README` § E.
- **Chat residue:** `05/03:4` "the advisor caught"; `07:55-57` "User-locked scope (3 AskUserQuestion
  answers)"; `07:81`; `05/03:86` "auto-memory note … updated"; `11/README:84` "the user commits
  manually"; `17/05:14,16` "the plan for this session", "doc_conventions §6"; `17/09:35,131` "this
  session"; `10:157` "the owner clarified"; `12/01:55`; `05/02:45,15`; `07:42`; `02:159-160`; `16:141-145`.
- **Stale file paths in prose:** `13/01:9` "`05-tpms-lattices.md`"; `05/02:43,63-64,128-130`,
  `05/03:16,62,85`, `13/README:67,71`, `17/05:17` name `11-dualc_field.md` / `12-dualc_field_view.md`;
  `12/01:59` link text.
- **Heading levels** start at `###`/`####` in split children (01/01, 01/02, 11/03, 11/04, 05/03).
- **Status tables doing record duty:** `12/README:97,101,102,105` cells are 3–6 sentences; `11/README:81`.
- `17/README:57-72` verification table (OPEN/FIXED/HALF) is the third place #22–#35 can be read.

## 7. Top 10 fixes (screener's ranking)

1. Resolve the "frozen" contradiction everywhere (`README.md:168-169,172-177`, `16:3-4`, `18:3-4`,
   `17/09:39-49`, `STRUCTURE.md:108-109`); fold `16` → `12/07`, `18` → `14/04`; retire 16/18 as
   "merged into" tombstones (never-reuse rule).
2. Fix entry-point/version history (`10:35`, `14/02:4,16`, `14/03:87`; `STRUCTURE.md:211`): 9 at 0.2.0,
   11 at 0.3.0, 13 at 0.4.0. Remove stale ABI tables `14/02:30-56` in favour of `capi/README.md`.
3. De-duplicate #22–#35: keep `17/README:141-156`; reduce `README.md:69-80,170` to one clause + link;
   drop restated numbers (`README.md:26-27,84,165,166,169`).
4. Bring `17/04`, `17/05`, `README.md` under 15 KB.
5. Fix `#N` collisions (`01/01:88`, `01/02:27`, `12/README:42-47`).
6. Correct misdirected anchors (`15:149-150`, `12/README:101`, `16:180`); add § anchors to folder-README
   page tables.
7. Repair chronology `03:12-27`, `13/README:8,27,77`; resolve 157 vs 171 with a `Reconstructed` marker.
8. Add triggers or reclassify trigger-less DEFERREDs; annotate `02:163` as superseded.
9. Collapse verbatim duplicates (Hermite-tag, "why a C ABI", three side-car blockquotes, `15:133-164`);
   delete dead API sketch `10:74-94`.
10. Scrub chat residue and stale paths; align `CLAUDE.md:61-64` / `STRUCTURE.md:168,270` with four opt-in
    GL targets; remove `user_guidance.md` reference at `07:92`.
