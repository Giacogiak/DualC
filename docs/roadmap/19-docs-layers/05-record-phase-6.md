# Record — Phase 6 (the command reference cleanup)

The dated close-of-session entry of [roadmap 19](README.md) Phase 6, done as two
sessions on one day — 6a (plan items 1–2) and 6b (items 3–7, the `flag-table`
extension the plan's verify line asks for, this record). The plan is
[§ 4 Phase 6](../../raw/2026-09-17-docs-restructuring-plan.md#phase-6--command-reference-cleanup-12-sessions-s2-is-the-worklist);
the worklist is [S2](../../raw/2026-09-17-docs-screening-command-reference.md). S2's
line numbers are pinned to commit `3d73220`; every passage was found by content,
since the 2026-09-11 split pass had moved most of them. Phase 5 had already edited
this tree, links only: `03`, `10`, `12/README` and `12/02` point at
[12/07](../12-field-graph-and-app/07-mesh-preview-sweep.md) (16 is a tombstone) and
`11/05` at [12/08](../12-field-graph-and-app/08-quality-inspection-gyroid-shell/README.md);
those links are kept as they were.

**Session 6a — DONE (2026-09-20, commit `3af866c`).** Items 1–2.

- **Item 1, each fixed from `examples/*.cpp`, never from the other page.**
  `dualc_lift --offset` defaults to `1` and is named `O` (`dualc_lift.cpp:116`,
  usage "Default 1."; the `0` / `R` table row was the wrong one and is gone).
  `11/06`'s "globally-welded mesh is a separate, deferred step" now says `--weld` on
  the 3MF path; its "≈ ⁶⁴⁄" reads "≈ 1⁄16 to 1⁄64 of the RAM". The `--tile-depth`
  range reads the same on `11/README`, `11/06` and `11/07`, all against
  [design/10](../../design/10-invariants-and-tolerances.md#--tile-depth-the-legal-range-and-the-useful-one):
  the only hard bound is `D ≥ 2`, `D = depth` still tiles, the useful range
  `2 ≤ D ≤ depth − 1` is `--mem`'s search range. `11/07`'s recipe 4 had a literal
  `\n` in the bash line. `12/02` said "press `l` to reload" where the viewer
  auto-reloads (`dualc_field_view.cpp:32-40`).
- **Item 2.** Real flag tables with source defaults and complete synopses on `03`
  (the seven ops named, `--bake`/`--sharp`/`--k` in the synopsis) and `06`
  (`--depth 7`, `--collapse 0`, `--collapse` in the synopsis). A `## Recipes` table
  with letter IDs on every page that lacked one: `01` (`1.1–1.10` → `D1–D10`, the
  prose reference updated), `02` (`C1–C7` for the scattered commands), `03`
  (`B1–B8`), `04` (`L1–L8`, the flag table moved above them), `05`, `06`,
  `11/README` (`F1–F3` plus an index of every child's IDs), `11/01` (`E`), `11/02`
  (`S1–S17`), `11/03` (`J`), `11/04` (`W1–W6`), `11/05` (`Q`), `11/06` (`T`),
  `11/07` (`U`), `11/08` (`Z`), `12/02` (`P1–P16`), `12/03` (`N1–N19`), `12/04`
  (`O1–O5`). **The `T` collision on `06`:** the recipes keep `T7–T13` (principle 3 —
  a recipe keeps its number) and the *types* table, which is not a recipe table,
  is re-lettered `Y1–Y6`. **Long `--expr` recipes** (`11/02`, `11/03`, `11/05`–`08`,
  `12/02`–`04`) keep their bash blocks — a 200-character command in a table cell is
  unusable and `11/02` had 849 bytes of headroom — with the ID in each recipe's
  comment header and a short `## Recipes` index table (ID · what it builds) above
  the block. The two Tool-12 commands in `11/02` moved to `12/02` (`P12`, `P13`);
  `11/04`'s step (a) is a legitimate cross-tool step of a four-step workflow and
  stays as `W1`, pointing at `12/04`'s `O1` instead of repeating the command.
  POSIX-safe paths on `07`, `08`, `10`. The exact messages quoted from source:
  `dualc_boolean`'s tiny-`--k` warning, `dualc_slice`'s two plane warnings,
  `dualc_raymarch`'s missing-mesh line, `dualc_field`'s `--mem` / `--tile-depth` /
  `--weld` / `--decimate` rejections, the unbounded-field hint, `dualc_view`'s
  "post-op not supported in the viewer preload" (which also settles S2 § 5.10: the
  viewer's stack has no `--translate`, `--rotate`, `--bend`, `--repeat-limited`, and
  `--round` preloads Offset). The six stale `11-dualc_field.md` link texts
  retargeted; `stale_paths_baseline` emptied in the same commit.

**Session 6b — DONE (2026-09-20).** Items 3–7 and the verify line.

- **Item 3 — rationale to its home — was link-not-move wherever the record already
  held it**, checked page by page before cutting: `11/06`'s "Why STL" argument is
  [11/03 § 5](../11-dense-lattice-deliverable/03-streaming-export.md#5-tiledstreaming-mesh--break-the-ram-wall-for-stl3mf-at-high-density)
  and § 5a; `08`'s coalescing and heavy-gate paragraphs are
  [07 § 10](../07-viewer-polyscope.md#10-polyscope-integration-in-the-demo-done--2026-05-31);
  `11/02`'s tapered-strut "why/how" is
  [05/02 § Phase 4](../05-tpms-lattices/02-strut-enhancements.md#phase-4----tapered-struts);
  `12/README`'s performance rationale is the three sections of
  [12/04](../12-field-graph-and-app/04-preview-performance.md); `12/01`'s side-car
  paragraphs are [12/03](../12-field-graph-and-app/03-raymarch-app.md) § Disk
  file-watch and § Real-time parameter push; `11/05`'s measured table is
  [12/08/01](../12-field-graph-and-app/08-quality-inspection-gyroid-shell/01-measurements.md#the-feasible-win--qem-mesh-decimation)
  and its collapse-energy history
  [01/02 § 4.9](../01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch);
  `01`'s oracle algorithms are [design/02](../../design/02-sign-oracles.md).
  Each copy became one to three sentences plus the link. **One verbatim move:**
  `06`'s derivation (50 % by symmetry, why λ alone cannot unlock the volume
  fraction, the Quilez third axis, the composition pipeline) is now a dated append
  under [05 § 16](../05-tpms-lattices/README.md#16-tpms-lattices-on-input-meshes)
  with the moved-from marker; the usage page keeps the volume-fraction table and
  the when-to-reach-for-each advice. **Deleted, not moved:** the video analogy of
  `11/06` and the ASCII ramp of `11/03` (the formula stays). `11/04`'s "514 553
  triangles, ~4 min" is not re-recorded: 12/05 dates the workflow's verification
  and the number was never a roadmap fact. The S2 § 8.3 residue (dates, "now",
  "since", "Phase 3", "Approach A/B", "Boletus", the "future side-car / eventual
  C ABI" claim, the parity counts of `12/02`) is gone from every usage page; what
  remains of those words is inside link anchors whose roadmap headings carry them.
- **Item 4 — one home + link for the six multi-copy facts.** PowerShell quoting →
  [design/09](../../design/09-conventions.md#windows-powershell-51-quoting) (the
  rule) and `11/01` (the worked forms); the copies on `11/02`, `11/04`, `12/02`,
  `12/03`, `12/04` are one line each. `mix` → [design/10](../../design/10-invariants-and-tolerances.md#mix-blends-values-not-shapes);
  `11/03`'s box is a five-line reminder with the graft pointer, `11/02`'s recipe
  comments and `12/02` one clause each. The thin-feature rule → design/10; the
  README, `02`, `11/02`, `11/04`, `11/06` link it. Export formats → the README table
  (user-facing) with design/09 for the why; every `-o` row says "by extension" and
  links the table instead of restating the millimetre. `--collapse` → `11/05`; the
  README matrix row and `11/README`'s cell link it. Keyboard layout → the README map
  with design/09 for the rule; `10` and `12/01` link it. Section planes → the key
  tables stay, the mechanism is 12/03 § Section planes.
- **Item 5 — the README, split before it grew.** At 14,934 bytes with the redirect
  rows, four matrix columns and the build prelude still to add, the two shared
  behaviour sections (rejected parameters, diagnostics warnings) moved verbatim,
  minus their dates, to a new page **`00-shared-behaviour.md`** — number 0 because
  it is not a tool; the `flag-table` check skips it, the index links it, three
  roadmap 17/05 links and one `11/README` link were retargeted to it. The README
  gained `## Build prelude` (Visual Studio and Ninja/POSIX lines, the four opt-in
  GL flags, the Polyscope sibling, where the binaries land), the
  `## Where a feature is documented` table (sealing, the oracles, hollowing,
  `--bake`, the input forms, quoting, `transform`, the three streaming rows that
  were one, the `dualc_view` modes, the viewer flags, the membrane workflow, the
  slice), the four missing matrix columns (`demo` = positional output, `view`,
  `gen_demo` = `--depth` for `genus2` only, `raymarch`), and lost its process
  history and dated lines. 13,970 bytes at close.
- **Item 6 — split residue.** Every `heading_hierarchy_baseline` entry cleared by
  the rule *delete the heading if its text equals the H1, promote its level if
  not* — slugs are live link targets and both moves preserve them (`11/04`,
  `11/05`, `11/06`, `11/07`, `12/03`, `12/04` deletions with the remaining tree
  promoted; `11/02`, `11/03`, `11/08` promotions); the list is empty and its
  comment says so. The three `directional` sites and the other "above/below"
  across files are now recipe-ID links. `12/README`'s mini-index has real summaries;
  `12/02`'s thirty-line paragraph is one paragraph plus the vocabulary link.
- **Item 7 — counts reconciled from source and stated once, in the README
  appendix.** Demo meshes: **9 generated** (`dualc_gen_demo.cpp`) **+ 6 not**
  (`data/` and `THIRD_PARTY.md`: `molde`, `foot`, `mesh-soup`, `opA`, `opB`,
  `bunny` — the README had dropped `mesh-soup`). Decorators differ because the
  surfaces differ, so the appendix names the scope: `dualc_primitive` exposes
  7 flags / 6 distinct decorators (`--round` ≡ `--offset`; `example_common.cpp`),
  the field graph adds `normalize` and `transform` plus the two `graded-*`
  (`field_graph.cpp`). `02`'s "The 6 decorators" heading is correct as scoped and
  is not rewritten; neither are the two H1s S2 called pattern-breaking (`02`,
  `11/README`) — principle 3.
- **The verify line — `flag-table` extended** (`scripts/check.py`): every `--flag`
  a recipe passes to `dualc_<tool>` — a command line inside a fence or a table
  cell, keyed off the binary the line names, not the page it sits on, with
  continuations joined and comments dropped — must head a table cell on that
  tool's page and be known to its parser (`dualc_primitive`'s post-op flags come
  from `example_common.cpp`; `dualc_view` may document them through page 02).
  380 recipe flags on the tree today, 0 problems. The tenth fixture pair,
  `scripts/check_fixtures/flag-table/`, proves both failure modes (a flag the
  parser knows but no table documents; a flag the parser does not know); dated
  entry on [17/09](../17-code-audit-and-hardening/09-local-checks-gate/README.md). Being
  in `check.py` anyway, the `sizes` summary's `len(frozen) + 1` (19/02's nit) now
  prints the exempt-file count.

## The twelve navigation questions

S2 § 6's questions, re-run at the close from `command_reference/README.md` with zero
context (the plan's verify line asked for 5, 6 and 12 to reach ≤ 2 hops):

| # | Question | Path now | Hops |
| --- | --- | --- | --- |
| 1 | Export a tiled 3MF? | § Where a feature → `11/08` | 1 (was 3) |
| 2 | What does `--mem` do? | § Where a feature → `11/07` | 1 (was 3) |
| 3 | Which tool previews a field graph on GPU? | tool table → `12/README` | 1 (was 1) |
| 4 | Viewer keyboard controls? | § GPU viewer keyboard map; `dualc_view`'s panel: tool table → `08` | 0 / 1 (was 0 / 1) |
| 5 | Seal an open/soup mesh? | § Where a feature → `01 § Choosing a sign oracle` | **1 (was N)** |
| 6 | Hollow a mesh (shell)? | § Where a feature → `05` R4 / `02` D3 / `11/04` W5 | **1 (was N)** |
| 7 | Which primitives need `--bounds`? | tool table → `02` notes | 1 (was 1) |
| 8 | PowerShell quoting? | § Where a feature → `11/01 § --expr and shell quoting` | 1 (was 1) |
| 9 | Headless PNG of a field graph? | § Where a feature → `12/README § CLI flags` | 1 (was 1) |
| 10 | Revolve a 2D profile? | tool table → `04` | 1 (was 1) |
| 11 | What does `--collapse` do? | § Common options (0); how it works → `11/05` | 0 / 1 (was 0 / 1) |
| 12 | Build the GPU viewers (CMake flag)? | § Build prelude | **0 (was N)** |

Every question is ≤ 1 hop; the plan's three (5, 6, 12) are 1, 1 and 0.

## What Phase 6 leaves, by owner

- **Phase 7's errata table:** `docs/study/` and `docs/raw/` citations of moved
  files (16 → 12/07, 18 → 14/04, the quality report → 12/08, `ARCHITECTURE.md` →
  `design/`), untouched here because those trees are immutable.
- **Phase 8's semantic lint, item (c):** `02-implicit-sdf-foundation.md`'s two
  remaining v2 gotchas and any roadmap intro that still describes the present; the
  cmdref tree now links `design/` for every present-tense fact it used to restate,
  and the lint is what keeps the two agreeing.
- **Phase 8's script pass:** the gate nits of 19/02–19/04 that are not one-liners
  — `decisions-index` keyed on (file, anchor), `status-vocab` not reading the
  decisions tables, the `\|` escape in row D-28. The `sizes` off-by-one is fixed
  above.
- **Kept on purpose, recorded so no later pass "fixes" it:** the tool H1s and
  section headings S2 wanted reworded (principle 3); `T7–T13` on `06`; the bash
  blocks with ID'd comment headers on the long-`--expr` pages; the `~⅓` figure in
  two places (the README's export table and `11/08`'s 3MF table, the two
  user-facing homes); the `(2026-05-31)` and `phase-4` fragments inside link
  anchors.

*Verified at close* (measured after this record and the README rows landed):
`python scripts/check.py --fast` and `--docs --strict` — 26 checks, 25 OK, 1 INFO
(`index-staleness`: `17/05`, whose three links this phase retargeted, and `17/09`,
listed since Phase 5 — both under `17/README`, whose rows for them are unchanged
and correct), **0 failed for the first time under
`--strict`**: `directional` 0 sites, `heading-hierarchy` 0 grandfathered,
`stale-paths` 0 grandfathered, both baselines empty; `flag-table` 12 pages, 78
flags, 380 recipe flags, 0 problems; 1,426 links, 0 broken; 512 cross-file + 22
same-file anchors, 0 unresolved; 106 files under the cap, `size_baseline` still
empty (`command_reference/README.md` 13,970 bytes, `11/02` 14,080, `roadmap
05/README` 12,605 after the append, `17/09` 14,709). `check.py --selftest` 24
fixture runs, 0 wrong; `ctest -R check_selftest` passes; `ctest -N` 256. Nothing
under `src/`, `include/`, `tests/`, `examples/` or `capi/` changed; the `tile-depth
>= depth` console-line nit of 19/02 stays a code nit for a code session.

*(2026-09-20, found by the pre-merge loss audit, `19/09`):* item 2's uniform `## Recipes`
heading renamed three recipe headings that already existed — `06`'s "Output mode recipes",
`11/05`'s "Recipes — decimation" and `11/08`'s "Recipes — tiled & welded 3MF" — an exception
to principle 3 this record did not state; no link targeted the old anchors, so the renamed
headings stand.

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
