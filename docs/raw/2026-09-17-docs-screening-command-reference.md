# Screening — `docs/command_reference/`

Read-only screening, 2026-09-17.

Scope: 25 files (README + 10 tool pages + `11-dualc_field/` [README + 8] + `12-dualc_field_view/`
[README + 4]) read in full, plus root `README.md` and `CLAUDE.md`. All relative links and `#anchors`
resolved programmatically: **0 broken links, 0 broken anchors**. `CR/` = `docs/command_reference/`.
Line numbers refer to commit `3d73220`.

## 1. Inventory

| File | Lines | KB | Words | Title line | Flag table | Recipes section | Footer | Roadmap links |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `README.md` | 206 | 15.2 | 2115 | `# DualC — Complete Command Reference` | matrix only | n/a | n/a | 4 |
| `01-dualc_demo.md` | 99 | 6.1 | 925 | `# Tool 1: dualc_demo (mesh in → mesh out)` | Y | Y (`1.1`–`1.10`, numeric) | Y | **0** |
| `02-dualc_primitive.md` | 158 | 9.4 | 1659 | `# Tool 2: dualc_primitive` (no role) | Y | **N** (P/D/O/F columns) | Y | **0** |
| `03-dualc_boolean.md` | 66 | 4.3 | 626 | `# Tool 3: dualc_boolean (…)` | **N** (bullets 14–52) | N (B1–B7, no heading) | Y | 1 |
| `04-dualc_lift.md` | 51 | 2.2 | 421 | `# Tool 4: dualc_lift (…)` | Y (39–47, after recipes) | N (L1–L8) | Y | **0** |
| `05-dualc_csg_demo.md` | 31 | 1.7 | 279 | `# Tool 5: dualc_csg_demo (…)` | Y | N (R1–R6) | Y | **0** |
| `06-dualc_lattice.md` | 161 | 9.4 | 1399 | `# Tool 6: dualc_lattice (…)` | **N** (prose 20–24; no defaults for `--depth`/`--collapse`) | "Output mode recipes" T7–T13 | Y | **0** |
| `07-dualc_slice.md` | 60 | 3.8 | 616 | `# Tool 7: dualc_slice (…)` | Y | Y (S1–S4) | Y | **0** |
| `08-dualc_view.md` | 176 | 11.7 | 1850 | `# Tool 8: dualc_view (…)` | Y | Y (V1–V18) | Y | **0** |
| `09-dualc_gen_demo.md` | 51 | 3.1 | 530 | `# Tool 9: dualc_gen_demo (…)` | Y | Y (G1–G5) | Y | **0** |
| `10-dualc_raymarch.md` | 120 | 8.0 | 1256 | `# Tool 10: dualc_raymarch (…)` | Y | Y (M1–M8) | Y | 1 |
| `11-dualc_field/README.md` | 172 | 11.9 | 1620 | `# Tool 11 — dualc_field` (no role) | Y | **N** ("## Examples") | Y | 5 |
| `11/01-op-vocabulary.md` | 213 | 11.2 | 1507 | `# Input forms, op vocabulary & text shorthand` | n/a | N (199–209 unlabelled) | Y | 3 |
| `11/02-strut-lattices.md` | 233 | 14.5 | 1841 | `# Strut lattices (sc · bcc · fcc · octet)` | n/a | "#### Examples & recipes", ~20 unlabelled | Y | 1 |
| `11/03-graded-and-morph.md` | 199 | 12.6 | 1775 | `# Graded shell, graded inflation & spatial morph` | n/a | N | Y | 1 |
| `11/04-workflow-open-surface.md` | 111 | 6.1 | 707 | `# Workflow: isolate → thicken → skin → union …` | n/a | steps (a)–(d) | Y | **0** |
| `11/05-decimation.md` | 205 | 11.9 | 1867 | `# Decimation (--decimate / --simplify)` | n/a | "### Recipes — decimation" `# 1.`–`# 5.` | Y | 0 (2 to report) |
| `11/06-streaming-tiled-export.md` | 173 | 10.8 | 1680 | `# Streaming / tiled export (--tile-depth)` | n/a | N | Y | 3 |
| `11/07-mem-budget.md` | 107 | 5.6 | 889 | `# Auto-budget (--mem BUDGET)` | n/a | "**Recipes.**" `# 1.`–`# 5.` | Y | **0** |
| `11/08-3mf-and-weld.md` | 125 | 6.9 | 1015 | `# The 3MF path and --weld` | n/a | "#### Recipes …" `# 1.`–`# 4.` | Y | **0** |
| `12-dualc_field_view/README.md` | 131 | 8.4 | 1223 | `# Tool 12: dualc_field_view (…)` | Y | N | Y | 3 |
| `12/01-controls.md` | 164 | 9.5 | 1533 | `# Controls: keys, live parameter editing, …` | n/a (key table) | N | Y | 4 |
| `12/02-nodes-and-examples.md` | 144 | 8.7 | 1043 | `# Supported nodes and worked examples` | n/a | "## Examples" unlabelled | Y | 1 |
| `12/03-recipes-by-node.md` | 111 | 5.6 | 658 | `# Recipes by node category (…)` | n/a | Y (unlabelled) | Y | **0** |
| `12/04-open-surface-preview.md` | 80 | 4.1 | 496 | `# Isolating a TPMS surface inside a volume (…)` | n/a | N | Y | 3 |

No file exceeds 300 lines or 2,500 words. `README.md` is **15,202 bytes** (over a 15,000-byte cap);
`README.md:38` asserts "Every page is inside the limit". `11/02` at 14.5 KB / 233 lines is next.

## 2. Index completeness

**README tool table (18–31) vs disk:** 12 rows ↔ 12 pages/folders ✓. Appendix inventory (194) consistent ✓.
**Folder READMEs:** `11/README:83-92` 8 rows ↔ 8 children ✓. `12/README:107-112` 4 rows ↔ 4 children, but
three "What it covers" cells are verbatim page titles.

**Redirect-table gaps (README 40–57):** features documented inside pages the README never points at:
`--gwn`/`--gwn-field`/`--pseudonormal` sign oracles (`01:17-19,36-81`; sealing open meshes only in root
`README.md:246-247`); `--bake` for `dualc_boolean` (`03:36-52`); `--mem` row 57 lumps three flags and
links only `06` (not `07-mem-budget.md`; `--weld` not `08-3mf-and-weld.md`); `--preview-scale`, metric
fast-path, discrete-GPU selection (`12/README:41,49-100`); `--snapshot`/`--es`; `--dump-json`, `--list`,
`.fld`, stdin `-` (`11/README:22-26`); `transform` decorator (`11/01:124`) absent from inventory 200;
`dualc_view` modes `--prim/--op/--recipe` (`08:14-19`). Options matrix (96–102) has **no columns** for
`dualc_demo`, `dualc_view`, `dualc_raymarch`, `dualc_gen_demo` though they take `--depth/--collapse/--bounds`
(`08:51-53`, `10:40`, `09:37`). No phantom pages.

## 3. Contract violations per file

**`README.md`** — 33–38 "Legacy pages (frozen). None today. … explicitly requested split passes of
2026-09-11" (process history, chat residue → roadmap 17/10); 119 "Since 2026-09-10", 145 "Since
2026-09-09", 162–164 history; 98 implies `dualc_demo` takes `-o` (it is positional, `01:6,12`); build
prelude 9–12 VS-only, no Ninja/POSIX, no opt-in CMake flags (scattered across `08:29-34`, `10:25-29`,
`12/README:26-31`); inventory 200 counts 7 decorators incl. `normalize` vs `02:94` "6" vs field-graph 8;
195 "9 generated + 5 not" vs `09:16-19` 6 not-generated.

**`01`** — recipe IDs numeric; 0 roadmap links; 41–81 (41 lines) oracle *algorithms* (3-ray vote, GWN,
pseudonormal) half rationale; no exact messages.
**`02`** — no role in title; no `## Recipes` (commands scattered 74–78, 144–148, 152–154); heading 80
"The 6 decorators" vs table D1–D7; 67 "prints a clear error" unquoted; 0 roadmap links.
**`03`** — no flag table (bullets 14–52); usage 6 omits `--bake/--sharp/--k`; 43–52 "Changed 2026-08-25 —
enclosed cavities … used to … now resolved … Verified by …" (10 lines → roadmap 16 § H2); 19–20 warning
unquoted; `--collapse` explained 25–30 (dup).
**`04`** — **default contradiction**: 28 "`--offset O` (default `1`)" vs table 43 "`--offset R` | `0`";
table after recipes; no caveats; 0 roadmap links.
**`05`** — no `## Recipes` heading; 0 roadmap links; recipe constants not named.
**`06`** — no flag table; `--depth`/`--collapse` defaults never stated; usage 6–8 omits `--collapse`;
35–94 (60 lines) conceptual/volume-fraction rationale and 117–130 pipeline internals → roadmap 05; TPMS
attribution stated twice (98–102, 155–157); `T` prefix shared by types T1–T6 and recipes T7–T13; 93 stale
link text `[11-dualc_field.md]`; 0 roadmap links.
**`07`** — compliant; warnings paraphrased with "…"; Windows separators (`data\bunny.obj`); 0 roadmap links.
**`08`** — implementation-log paragraphs 76–80 (`ImGui::IsItemDeactivatedAfterEdit`), 82–86 (`pendingState`),
104–111, 115–122 (GLFW/WGL) ≈ 20 lines → roadmap 07; post-op list 64 omits `translate/rotate/bend/repeat-limited`
that `02` documents (limitation or gap?); 0 roadmap links.
**`09`** — 23 "added 2026-09-11"; G2–G5 are `dualc_demo`/`dualc_view` commands; 43 path `../../../data`
assumes cwd; README says `--dir data`.
**`10`** — 40 "(it did dissolve into holes before 2026-08-25 …)"; 98–100 "**now** exits with …" (keep
message, drop before/now); otherwise the best-formed page.
**`11/README`** — title pattern broken; 10–17 "Why this matters" benchmark → roadmap 11/02; 34–37 "a
*future* raymarch side-car and an *eventual* C ABI" (both shipped); 155–158 "are **now** supported …
remains deferred"; 64→65 missing blank line; no `## Recipes`; 46 "Approach A" jargon.
**`11/01`** — recipes 199–209 unlabelled; otherwise compliant; shell-quoting 42–83 is the canonical copy.
**`11/02`** — H1 repeated as `###` at 7; `####` 32, 88; 11 "TPMS surfaces **above**" (in `01`); 130 "see
note in the Spatial morph section" (no link, in `03`); 204 plain-text "12-dualc_field_view.md"; 38–46
"Why you'd want it", 48–52 "How it works" (`RoundCone`) → roadmap 05/01; 188 "compose Phase 3 and Phase
4"; ~20 recipes in one 115-line bash block 95–210 with no IDs; 205, 209 are Tool-12 commands.
**`11/03`** — `###` 7, 77; `####` 163; 75 "recipes **below**", 161 "**above**" (in `02`); 123 "(Phase 3)";
191–195 "Rule of thumb for clients (e.g. Boletus)"; `mix`-gap callout ×3 in one page (91–114, 116–130,
165–170); 26–34 ASCII ramp + 44–50 QEF-normal remark (implementation rationale).
**`11/04`** — `##` 7 repeats H1; 62–71 "Measured: 514 553 triangles, ~4 min"; 104–107 boxed rationale;
steps unlabelled; 0 roadmap links (source is 12 § F).
**`11/05`** — `##` 7 repeats H1; 129–137 history ("Before 2026-08 … 70,170 to 68,698 faces") → 17 ledger;
75–95 (21 lines) copy the inspection report's tables (linked at 24, 78); 120–121 double blank; 195–201
"Approach A/B" undefined; 198 "rejects the pair" unquoted; 95 "the acceptance result"; `--collapse`
explained a third time 112–153.
**`11/06`** — `##` 7 repeats H1; 40–46 video analogy / "the crash that didn't happen"; 129–169 (41 lines)
"Why STL (and not PLY)" → roadmap 11/03; **contradiction** 116–124 "globally-welded mesh is a separate,
deferred step" vs 153–165 "`--weld` now shipped" and `08`; 80 garbled "≈ ⁶⁴⁄" (should be 1⁄64); 86 "`D =
depth` still runs 8 near-full tiles" vs `11/README:49` "`D` < `--depth`" vs `07:101` clamp `[2, depth−1]`;
90 error unquoted.
**`11/07`** — `###` 7 repeats H1; 10 "table **above**" (in `06`); **broken recipe 66**: literal `\n` inside
the bash line; 34–35 unquoted; 0 roadmap links.
**`11/08`** — `###` 7 repeats H1; recipes `# 1.`–`# 4.`; 75 caveat unquoted; 0 roadmap links.
**`12/README`** — usage 7–9 omits `--preview-scale` (table 41); 30–31 "**hidden** `--snapshot`" though in
table 43 and 4 recipes; 49–100 (52 lines) performance rationale ("7× fewer SDF evaluations", "Intel HD
630 … GTX 1050 Ti") → roadmap 12/04, keep one line each; mini-index title-only; no recipes.
**`12/01`** — 94–98, 110–114 Boletus side-car rationale ≈ 10 lines → roadmap 12 § D / 15; 88 "`l` is
**now** only a manual force"; 41, 139 "below" (other files); 104 leads with `[`/`]` after 73–79 said
prefer arrows.
**`12/02`** — 9–12 "As of 2026-06-24 … 69/69 … 73/73 since 2026-08-25" and 137–140 (dates + case counts
in a usage page; `check.py --gpu` asserts the count); 7–37 one 30-line paragraph; 68 "press `l` to
reload" contradicts `12/01:19,85-89` (auto-reload); 63 brackets vs arrows; 41 "the controls **above**".
**`12/03`** — `##` 5 repeats H1; 7–8 "Since 2026-06-24 … previously contour-only", 67 "these **new**
warps"; recipes unlabelled; 0 roadmap links.
**`12/04`** — `##` 6 repeats H1; 41–43 bare `dualc_field_view` after `02:42` said prefix; otherwise clean.

**Root `README.md`** — 224 stale path `docs/command_reference/12-dualc_field_view.md`; 3 "on triangle
meshes" (omits fields); 231–244 8 meshes + "All **seven** are generated" vs `09` 9 generated incl. `cube`;
60–66 "planned submodule conversion … Phase 2 of Tier 3 #8" contradicts `CLAUDE.md` ("no submodule");
213–219 dates and "73/73".
**`CLAUDE.md`** — opt-in list omits `-DDUALC_BUILD_FIELD_VIEW=ON` (`11/README:163`, `12/README:26`) and
`-DDUALC_BUILD_GLSL_PARITY=ON` (`12/README:119`).

## 4. Duplication (canonical spot → copies)

- **Export-format dispatch** (`.obj/.stl/.3mf`, "1 unit = 1 mm", "~⅓ the STL size"): `README.md:172-186`
  (table); root `README.md:97-114` near-verbatim; `02:12`; `03:9-10`; `04:45`; `05:15`; `06:23-24,153`;
  `11/README:43,49`; `11/05:166-167`; `11/06:21-22,90-93`; `11/08:14-20`. "~⅓" appears 9 times.
- **Keyboard maps**: `README.md:68-84` ∪ `10:46-59` ∪ `12/01:9-21` ∪ root `README.md:191-193,219-221`.
- **Layout-independence** (AZERTY, `[`/`]` physical-US, prefer arrows): `README.md:63-66`; `10:63-68`;
  `12/01:36-38,73-79,156-160` (three times in one file).
- **Section-plane semantics**: `README.md:63-64`; `10:61-71`; `12/01:116-160` (45 lines); `10` M7.
- **`--collapse` meaning**: `README.md:100`; `01:14`; `03:25-30`; `11/README:45`; `11/05:112-153` (42 lines).
- **Thin-feature "≥ 2–3 cells" rule**: `README.md:104-115`; `11/02:57-59,143-145,177-178`; `11/03:53-55`;
  `11/04:28,82-83`; `11/06:52-54`; `02:126-136`.
- **PowerShell 5.1 quoting**: canonical `11/01:42-83`; restated `11/02:90-93`; `11/04:73-78`;
  `12/02:46-56`; `12/03:8-11,89-94`; `12/04:35-52`.
- **`mix` blends values → gap**: `11/02:115-122,126-130`; `11/03:91-114,121-130,165-170,191-195`;
  `12/02:26-32`. Six statements.
- **"Preview == export / one source of truth"**: `11/README:160-166`; `12/README:12-17,114-120`;
  `10:111-116`; root `README.md:196-201`; `11/02:202`.
- **Isolate-TPMS-surface workflow**: `06:26-33`; `11/04:30-36`; `12/04` whole page; identical box
  expression in `11/04:35,42`, `12/01:136-139`, `12/04:32`.
- **Foot graded-onion recipe**: `11/04:68,77` ≡ `12/02:82,117`; radial box recipe `11/04:58` ≡ `12/02:86`;
  graded-offset bcc/octet `11/02:154,161` ≡ `12/02:92,95`; tapered bcc `11/02:179` ≡ `12/02:99`.
- **Measured streaming numbers** (~62 MB, ~440 MB, ~3 GB): `11/06:19,35,74-78`; `11/07:88`; `11/08:102-103`.
- **Grouped-key vs positional params**: `11/01:138-155` and `12/03:15-20`.
- **Node-vocabulary coverage sentence**: `12/02:9-37,135-140`; root `README.md:211-219`.
- **Tool descriptions**: `10:11-29` ≈ root `README.md:164-190`; `07:11-16` ≈ `:118-125`; `08:10-34` ≈
  `:134-144`; demo-mesh table `09:21-31` ≈ `:231-239`.
- **TPMS formula attribution**: `06:100-102` and `06:155-157`.

## 5. Internal inconsistencies

1. `dualc_lift --offset` default `1` (`04:28`) vs `0` (`04:43`); named `O` vs `R`.
2. Decorator count: README 7 (with `normalize`); `02:80,94` 6; `11/01:116-125` 8 (`transform`).
3. Not-generated meshes: README 195 = 5; `09:16-19` = 6; root README 241 "All seven" + 244 `cube` not
   generated vs `09:23` generated.
4. `--tile-depth D` upper bound: `11/README:49` `D < depth`; `11/06:83-86` `D = depth` accepted; `11/07:101`
   clamp `[2, depth−1]`.
5. Global weld: `11/06:116-124` deferred vs `11/06:153-165` + `08` shipped.
6. `l` reload: `12/02:68` vs `12/01:19,85-89`; README 84 lists `l` without auto-reload.
7. `--snapshot` "hidden" (`12/README:30`, root README 222) vs listed (`12/README:43`).
8. `dualc_demo -o` implied (`README.md:98`) vs positional (`01:6,12`).
9. `--preview-scale` in table not in usage (`12/README:7-9,41`).
10. `dualc_view` post-op stack (`08:64`) omits `translate/rotate/bend/repeat-limited`.
11. `03:6` synopsis lacks `--bake/--sharp/--k`. 12. `06:6-8` synopsis lacks `--collapse`.
13. Arrow vs bracket priority (`12/01:73-79` vs `12/01:104`, `12/02:63`, root README 191, 220).
14. Stale "future" claims (`11/README:35-36`; root README 61–66).
15. `README.md:38` "Every page is inside the limit" vs 15,202 bytes.
16. Stale link texts `[11-dualc_field.md]` (`06:93`, `10:116`, `12/02:56`, `12/03:10`, `12/04:36`),
    plain "12-dualc_field_view.md" (`11/02:204`).
17. `README.md:57` names three flags, links one page.
18. `11/07:66` literal `\n` — not runnable.
19. Recipe prefix collision (`06` T1–T6 types vs T7–T13 recipes); `01` uses `1.x`; `11/*`, `12/*` none.

## 6. Navigation test (from README, ≤ 2 hops)

| # | Question | ≤2? | Path |
| --- | --- | --- | --- |
| 1 | Export a tiled 3MF? | Y (2) | README 57 → `11/06` → recipes actually in `11/08` (3rd hop) |
| 2 | What does `--mem` do? | Y (2) | README 30 → `11/README:51` → `11/07`; redirect row 57 sends to `06` (3 hops) |
| 3 | Which tool previews a field graph on GPU? | Y (1) | README 31 → `12/README` |
| 4 | Viewer keyboard controls? | Y | README 61–84; `dualc_view` ImGui: README 27 → `08:55-69` |
| 5 | Seal an open/soup mesh? | **N** | README never mentions `--gwn-field`/`winding` sealing |
| 6 | Hollow a mesh (shell)? | **N** | split across `05` R4, `02` D3, `11/04` (c); no redirect |
| 7 | Which primitives need `--bounds`? | Y (1) | README 21 → `02:67` |
| 8 | PowerShell quoting? | Y (1) | README 55 → `11/01` |
| 9 | Headless PNG of a field graph? | Y (1) | README 31 → `12/README:43` |
| 10 | Revolve a 2D profile? | Y (1) | README 23 → `04` |
| 11 | What does `--collapse` do? | Y (0) | README 100; depth in `11/05` |
| 12 | Build the GPU viewers (CMake flag)? | **N** | prelude has no opt-in flags; three flags on `08`, `10`, `12/README` |

## 7. Structural observations

**`11-dualc_field/`** — split by concern, mostly clean (01 vocab, 02 struts, 03 graded/mix, 04 workflow,
05 decimation, 06 tiling, 07 mem, 08 3MF/weld). Problems: every child repeats its H1 as a second heading
at its old level; directional words survived ("above/below" → other files); 02 ↔ 03 overlap (mix-gap and
graded-strut recipes in 02, explanation in 03); 06 ↔ 07 ↔ 08 overlap (06 carries 3MF constraints 90–96,
weld rationale 150–169, pre-weld text 116–127 superseded by 08); mostly-rationale children: `06` (≈70 of
173 lines), `05` (≈70 of 205), `03` (≈40 of 199); `04` and `02` host Tool-12 commands; `11/README` still
hosts "Examples" 111–149 and "Scope"/"Live GPU preview" prose overlapping 01 and 12.

**`12-dualc_field_view/`** — 02 and 03 overlap in purpose; 02's examples duplicate `11/02`, `11/04`; 03
duplicates `11/01` param-form explanation; 02's "Supported nodes" 7–37 is a 30-line paragraph saying "same
vocabulary as `dualc_field`"; `12/README` keeps ~50 lines of performance rationale; mini-index title-only.

**Chat/engineering-log residue (verbatim):** `README.md:33-38`; `03:43-52`; `09:23`; `10:40,98`;
`11/README:35-36,156`; `11/02:188`; `11/03:123,191`; `11/04:65`; `11/05:95,129-137,195-201`;
`11/06:45-46,153`; `12/01:88,94-98,110-114`; `12/02:9-12,137`; `12/03:7-8,67`; `12/README:78,86-91`.

## 8. Top 10 fixes (screener's ranking)

1. Fix user-misleading contradictions: `04` `--offset`; `11/06:116-127` weld; `12/02:68` `l`;
   `--tile-depth` bound; `11/07:66` `\n`; `11/06:80` fraction.
2. Real flag tables with defaults: rewrite `03`, `06`; add `--preview-scale` to `12/README` usage.
3. Strip dates/"now"/"since"/phase jargon from usage pages (list in §3); parity counts not hard-coded.
4. Move rationale to roadmap, leave a link: `11/06:129-169,40-46`; `06:35-94,117-130`;
   `11/05:75-95,112-153`; `11/README:10-17`; `12/README:49-100`; `08:76-86,104-122`; `11/02:38-52`.
5. Kill split residue: duplicated second headings (02:7, 03:7, 04:7, 05:7, 06:7, 07:7, 08:7, 12/03:5,
   12/04:6); cross-file "above/below"; stale link texts (6 spots) and root README 224.
6. README redirect table: split row 57 into three; add rows for sealing, `--bake`, `--snapshot`/`--es`/
   `--preview-scale`, `--dump-json`/`--list`/`.fld`, `dualc_view` modes; add missing matrix columns; fix 98.
7. Deduplicate to one canonical spot + link: PowerShell quoting (`11/01`), `mix`-gap (`11/03` box),
   thin-feature (README §), export formats (README §), `--collapse` (`11/05`), keyboard layout (README);
   remove `06:155-157`.
8. Recipes contract: `## Recipes` table with letter IDs on `02`–`06`, `11/README`, every `11/*`, `12/*`
   child (~60 unlabelled commands); `01` `1.x` → `D1…`; resolve `T` collision; move Tool-12 recipes out
   of `11/02`, `11/04`; POSIX-safe paths in `07`.
9. Reconcile counts: decorators 6/7/8, not-generated 5/6, generated 7/8/9; root README 3, 60–66, 241–244.
10. Build prelude: Ninja/POSIX variant + opt-in flags (`POLYSCOPE_VIEWER`, `RAYMARCH_VIEWER`, `FIELD_VIEW`,
    `GLSL_PARITY`) in `README.md:9-12` and `CLAUDE.md`; real summaries in `12/README` mini-index; README
    under 15,000 bytes.
