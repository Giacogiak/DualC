# Record — Phase 3 (the design layer)

The dated close-of-session entries of [roadmap 19](README.md) Phase 3: session 3a (the split
of `docs/ARCHITECTURE.md` into `docs/design/`) and session 3b (the three knowhow children).
Each entry lists what was delivered, where it deviated from the plan and why, and the gate
evidence at close. The plan is
[§ 4 Phase 3](../../raw/2026-09-17-docs-restructuring-plan.md); the cut list is
[S3 § 2.4](../../raw/2026-09-17-docs-screening-architecture.md).

**Session 3a — DONE (2026-09-17).** `docs/ARCHITECTURE.md` (1,265 lines, 66 KB, 4.3× the
byte cap) is deleted — no redirect stub, decision 3 of the plan — and its content is nine
pages under [`docs/design/`](../../design/README.md), every one under the cap (the largest,
`04`, at 12.7 KB against 15.4 KB). The section → page mapping, which this entry is the
record of:

| Design page | From `ARCHITECTURE.md` | Labels kept |
| --- | --- | --- |
| `README.md` | intro, § 1 (minus the signMethod-drop parenthetical), § 2, § 10.2, plus the page index | § 1, § 2, § 10.2 |
| `01-sampler.md` | § 3 intro, § 3.1–3.3, the miss ladder, "why purely-geometric" | § 3, § 3.1–3.3 |
| `02-sign-oracles.md` | § 3.4 | § 3.4 |
| `03-contourer-recursion.md` | § 4 intro, § 4.1–4.3 | § 4, § 4.1–4.3 |
| `04-qef-manifold-collapse.md` | § 4.4–4.6 | § 4.4–4.6 |
| `05-conventions-and-tables.md` | § 5, § 6 | § 5, § 6 |
| `06-parameters-and-vendoring.md` | § 7, § 8 | § 7, § 8 |
| `07-limitations.md` | § 9: the four live entries, the two behavioural notes; the four "Fixed since" callouts as one line each | § 9 |
| `08-implicit-field-layer.md` | § 10 intro, § 10.1, 10.3, 10.4 (as the library's shape), 10.5 | § 10, § 10.1, 10.3–10.5 |

The status banner (lines 3–11, a revision note) was dropped: it was history. Section labels
survive in headings, so the study pack's "§ 4.6"-style citations still name a place; the
anchors changed with the split (`#46-adaptive-cell-collapse` is on `04`, not on one file),
which is Phase 7's errata table to state for the immutable study files.

- **The compiled-layer rule, applied.** Every dated change record became the present-tense
  fact plus a link to the roadmap entry that changed it: the ray-parity epsilon (§ 3.4 →
  17 #23), the pseudonormal watertightness diagnostics (§ 3.4 → 17/05), `parallelFor`
  exception propagation (§ 9 → 17 #22, restated as a property of § 3.2), the deleted dead
  parameters (§ 9 → 17 #28, restated as "no field is read nowhere" in § 7), the diagnostics
  channel (§ 9 → 17/05), the signMethod drop (§ 1 and § 9 → 01 § 4.9), the pinv/energy pair
  (§ 4.6 and § 9 → 01 § 4.9 and `THIRD_PARTY.md`), the nine wrappers (§ 9 and § 10.1 →
  01 § 4.9). `design-no-history` is live and reports 0 markers over 9 pages; its `now` and
  `since` patterns also caught the purely logical uses ("since the input is `AᵀA`"), which
  became "because".
- **The § 9 / § 10.5 bounds contradiction, resolved from `src/sampler.cpp`**
  (`sampleFieldToHermiteOctree`): an infinite `field.bounds()` throws; an *invalid* one
  becomes `BBox::unit()` **and** sets `Diagnostics::boundsFallback` when a `Diagnostics*`
  was passed, silently otherwise. Stated once in `01` § 3.1; `07` and `08` § 10.5 link to
  it. The empty-contour placeholder is the same shape (`src/contourer.cpp`,
  `Diagnostics::emptyContour`), stated once in `03` § 4.3.
- **Two present-tense claims that were false, corrected against the code.** § 3.3 said
  "`minDepth > maxDepth` is not an error, `maxDepth` simply wins. Neither parameter is
  validated" — `sampleFieldToHermiteOctree` throws `std::invalid_argument` on a negative
  depth and on `minDepth > maxDepth` (17 #34, [17/08](../17-code-audit-and-hardening/08-argument-validation.md));
  `01` § 3.1 and the `minDepth` row of `06` § 7 say so. § 3.2 called `parallel.h` "a
  51-line header" — it is 74 lines; the number is gone, not updated.
- **Deviation from a verbatim move: no source line numbers.** `ARCHITECTURE.md` pinned
  `src/contourer.cpp:529`, `:546`, `:552`, `:575`, `:272`, `:461`, `:654-665` and
  `cube_components.cpp:47`; every one had drifted (`cellProc` is at 564, `tryCollapse` at
  307, `kQuadCCWPlus` at 496). The design pages name the file and the symbol, never the
  line — a freely rewritten layer that pins lines is a drift generator, and the repo's own
  search recipe (`docs/README.md` § How to find things) is symbol-first.
- **Dedupe (S3 § 2.2), each fact left with one home.** The molde/cube vertex and face
  counts (two copies in § 4.5/§ 4.6) → links to the quality-bar table
  [01 § 5](../01-core-dual-contouring/01-engineering-record.md#5-current-quality-bars-depth-7-unless-stated);
  the `kQuadCCWPlus` Y-row reversal is explained once, in `03` § 4.3, and `05` § 6 links
  there; the library inventory of § 10.4 (with its "30 + 6" count) → the command-reference
  inventory appendix, `08` keeping only the library's shape; § 8's vendoring text keeps the
  design-relevant part (the one local change and the eigenvalue/singular-value note) and
  links `THIRD_PARTY.md` for attribution and the measurement; the § 7 parameter tables stay
  (they are the header comments' home in prose) with the CLI flags linked, not restated.
- **`05` keeps § 6.** The plan's 3b bullet gives `09-conventions.md` the "corner/edge/child
  index tables (ARCH § 6)"; S3 § 2.4 gives them to `05`. Following both would recreate the
  copy this phase removes. Decision: the DC tables are engine-internal and stay in `05`;
  `09` covers project-wide knowhow (units, PowerShell quoting, keyboard-layout independence,
  export-format dispatch, parameter style) and links `05` for the tables.
- **Inbound links.** The four live markdown links are retargeted: `docs/README.md` (two
  rows), `roadmap/README.md`, `01/01`, `17/README` (with its `#9-…` anchor). `AGENTS.md`
  and `STRUCTURE.md` lose their "until Phase 3" clauses; `docs/README.md` links `design/`.
  Fourteen backticked mentions in record files stay: the five `*Source:*` provenance
  citations of `17/03` and `17/04` (retargeting them would falsify what the audit read),
  `02:160`, `17/04:45`, `17/10` (three, plus a dated superseded note appended to its
  "living document with a ceiling" sentence), `17/README:60` (its "opening note" evidence
  is gone with the banner, so it took a dated pointer here), `19/01:49` and the #36 row —
  each a historical literal, which is the class `stale-paths` deliberately ignores. The 13
  study and raw files that name it are immutable; Phase 7's errata table retargets them.
- **Gate wiring** (`scripts/check_data.json`): the `docs/ARCHITECTURE.md` baseline entry
  deleted and the `_comment`'s "living design document" decision replaced by one sentence on
  the split; `docs/design` joined `index_roots`, `docs/design/` joined `footer_roots` (so the
  plan's footer criterion is enforced, not just met) and `structure_summarized_prefixes`
  (one line in `STRUCTURE.md`, as for every other docs folder); the roadmap README baseline
  lowered 16,125 → 16,104 bytes (the shorter link). `scripts/check.py` changed in one
  docstring (`strip_code` named `ARCHITECTURE.md` as its motivating case). No check logic
  changed; no fixture changed.
- **Left for session 3b.** `09`–`11` and the README's "Things we do not do" section (the
  README's page table gets three rows); the `05`/`09` boundary above; the seven multi-copy
  facts of S2 § 4 (#40).

*Verified:* `python scripts/check.py --fast` — 26 checks, 23 OK, 0 failed, 1 SKIP
(`decisions-index`, Phase 4), 2 INFO; **`design-no-history` OK, 9 pages, 0 markers** (SKIP
before this session); 924 links, 0 broken; 192 cross-file + 19 same-file anchors, 0
unresolved; `indexes` 14 roots (was 13), 94 siblings, 0 unlinked; `footer` 73 pages (the
eight design children and this record joined); `sizes` 88 files, 4 baselined (was 5), 0
over; `structure` 129 tracked files, 0 undocumented; `directional` 5 → 4 (the fifth was
`ARCHITECTURE.md` § 9's "everything below"). `--docs --strict` fails on exactly that
`directional` INFO — the four remaining sites are three in `command_reference/11` and
`12` (Phase 6's list) and one in `roadmap/10` (Phase 5's), untouched here by the
one-thread-per-phase rule — and on nothing else. `ctest -N` **256**, measured on the existing build — nothing under `src/`,
`include/`, `tests/`, `examples/` or `capi/` changed (diff stat), and `scripts/check.py`'s
only change is a docstring, so `check_selftest` is untouched; 256 is the Phase 2 note (a)
of [01](01-record-phases-0-2.md).

**Session 3b — DONE (2026-09-18).** The three knowhow children and the README section the
plan's 3b bullet names, harvested from the sources it names — the roadmap (grep for the
invariant, unit and tolerance statements: `bit-identical`, `1 unit = 1 mm`, `eps`, `1e-`,
the kernel/host rule of block 09, the build/pillar vocabulary of block 12), the S2 § 4
duplicates list, `src/` and `examples/` for every present-tense claim — and the design
README grown by three page rows and a "Things we do not do" section. #36 and #40 flip to
DONE; the phase closes. What each page holds and where it deviates from the plan:

- **`09-conventions.md`** — units and frames (the library is unit-less; the 3MF header is
  where 1 world unit becomes 1 mm; meshes arrive in one global frame and I/O is the host's,
  block 09's rule), export-format dispatch with its *why* (`dispatchAndLog`; why streaming
  is STL/3MF only), the Windows PowerShell 5.1 quoting rule (the rule and the one working
  form; the alternates and the worked recipes stay on `11/01`), keyboard-layout independence
  as a rule for every GL viewer (`glfwGetKeyName` for letters, arrows and Page keys for
  values, US-position keys as alternates only), and primitive parameters (the seven ops in
  `primitiveLayouts()`, the `params:[…]` fallback, the one registry). **Deviation, decided in
  3a:** the corner/edge/child tables stay in `05` § 6; `09` links them.
- **`10-invariants-and-tolerances.md`** — the output invariants (watertight + χ, pinned by
  `topologyOf` in `tests/test_demo_meshes.cpp`; bit-identical across thread counts; mesh
  path = field path; tiled = monolithic, `tests/test_streaming_export.cpp`; preview = export;
  fallbacks reported), the ≥ 2–3 cells rule as the one home, `mix` blends values as the one
  home (with the `cellOverlaps = true` reason from `MixField`), the `BBox` sentinel table
  (`infinite()` / `empty()` / `unit()` / the `BBox{}` trap), the `--tile-depth` range, and a
  table of every numeric constant with the file and the page that explains it. **The
  `--tile-depth` conflict, settled from `forEachOwnedTile` and `pickTileDepthForBudget`
  (`examples/example_common.cpp`):** the only hard bound is `D ≥ 2` (export-time error);
  there is no upper check; tiling happens while `2^D − 2 < 2^depth`, so `D = depth` still
  tiles (eight near-full tiles) and the single streamed pass begins at `D = depth + 1`; the
  useful range `2 ≤ D ≤ depth − 1` is `--mem`'s search range. `11/README`'s "`D < depth`"
  states the useful range, `11/06` the legal one, `11/07`'s clamp is `--mem`'s — none is
  wrong, and Phase 6 item 1 rewrites them against this page. One code nit found on the way,
  not fixed here (docs-only phase): the single-pass console line says `tile-depth >= depth`
  where the condition is `D ≥ depth + 1`.
- **`11-glossary.md`** — one line per term from the study pack's eleven "Key terms" tables,
  rewritten present-tense, cut to the terms with a DualC referent (no multipole expansion,
  chamfer distance, SAH, CMake vocabulary) and pointing at the design page that owns the
  concept rather than explaining it twice; plus the **ID vocabulary**: `#N` = roadmap tracked
  item (one sequence across blocks), "Tier 1 item N" = block 01's list (never `#N`),
  "§ A–G" = block 12's sections, "build #1–#3" = block 12's delivered steps and
  "pillar #1–#4" = its four value pillars (both always with the word attached), `NN-slug`
  and letter IDs. The rule "prose uses the qualified form" is stated there.
- **`README.md`** — three page rows (Labels `—`: these pages carry no `ARCHITECTURE.md`
  section number, and inventing one would create a false citation target) and "Things we
  do not do": the sibling-checkout rule, no mesh I/O in the library, no line numbers in this
  layer, and one-liners linking `docs/README.md` § Conventions for the docs-process rules
  (no renumbering, no restated counts, no search index, no reflow of `raw/`) rather than a
  second copy of that block.
- **The seven multi-copy facts of S2 § 4 (#40)** each have a home: export dispatch,
  layout independence, PowerShell quoting, grouped-vs-positional → `09`; the thin-feature
  rule, `mix`, preview == export → `10`. **The copies are untouched** — the plan's 3b
  wording ("the rule moves here, the cmdref page keeps the two-line reminder + link") is
  the end state, and the pass that replaces each copy by one sentence + link is Phase 6
  items 3–4 for `command_reference/` and Phase 5 item 11 for the roadmap intros. Until
  then the rule exists twice, in a compiled page and in a usage page; the semantic lint of
  Phase 8 is what keeps the two agreeing.
- **Not done, and why.** `command_reference/` is not edited (one thread per phase; the
  `directional` INFO stays at its four sites, none in `design/`). The plan's verify line
  "`rg -c 'ARCHITECTURE.md' docs` = 0 outside `raw/` and the errata table" is met for
  markdown *links* (none remain) and deliberately not for the backticked historical
  literals in the record files, per the 3a entry above.
- **Small fixes on the way.** `docs/README.md` § Conventions names `design/` among the
  footer trees (3a wired the check, not the sentence); the roadmap README's block-19 row
  reads "Phases 0–3 DONE; 4–8 PLANNED" with the dates left to this folder, and its byte
  baseline ratchets 16,104 → 16,091. One gate nit seen, not fixed (no script change this
  phase): `sizes` reports `1 frozen` with an empty `frozen` list — `check.py` prints
  `len(frozen) + 1`; an off-by-one in the summary line only, for Phase 8's script pass.

*Verified:* `python scripts/check.py --fast` — 26 checks, 23 OK, 0 failed, 1 SKIP
(`decisions-index`), 2 INFO (`index-staleness`, `directional` 4 sites, the same four as
3a); **`design-no-history` OK, 12 pages, 0 markers** (its `used to` pattern caught one
logical use, rewritten); 1,011 links, 0 broken; 244 cross-file + 19 same-file anchors, 0
unresolved; `indexes` 14 roots, 97 siblings, 0 unlinked; `footer` 76 pages; `sizes` 91
files, 0 over — the three pages are 8.2 KB, 11.4 KB and 13.1 KB (the glossary, the
largest, has 2.3 KB of headroom, so a new term goes in as a row and a new *section* means a
split). `--docs --strict` fails on `directional` only. Nothing under `src/`, `include/`,
`tests/`, `examples/` or `capi/` changed and `scripts/check.py` is untouched, so `ctest -N`
stays **256** (3a's measurement on the same build).

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
