# Handoff — P05.1: the retiring pages' content re-homed under 11 and 12

Unit P05.1 of [`plan/14_P05_docs-migration.md`](../14_P05_docs-migration.md) § T1, the only (and
so the last) unit of P05; session of 2026-10-16, branch `feat/cli-consolidation` (pushed to
`origin`; `main` untouched). The record is
[roadmap 21/07](../../docs/roadmap/21-cli-consolidation/07-docs-migration.md) — every recipe
run, its result, what was found on the way; [#60](../../docs/roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)
stays **PLANNED** with "P05 T1 landed 2026-10-16, P06 next" on its status line. Nothing was
deleted.

## What changed, and why

Commits: `4b1f439` (docs, samples, ctest), `f0ea075` (the loss-audit triage and the facts it
surfaced), then this handoff.

- **Five new pages.** `11-dualc_field/11-mesh-in.md` (01: the flag-by-flag map, *Choosing a
  sign oracle*, sharp normals, `--no-manifold`, the input requirements, recipes G1–G9 with
  **the soup recipe** `dualc_field --expr 'winding(path="data/mesh-soup.obj")' -o sealed.stl`,
  and *Booleans of meshes* H1–H2 with `k` sizing — `dualc_boolean` stays (D-60), but its capability is in the framework too);
  `12-primitive-catalogue.md` (02: P1–P30 with flat order and defaults, D1–D7, O1–O6, F1–F3,
  C1–C7, the bounds / displacement advice); `13-composition-samples.md` (05: K1–K6 = the six
  sample files); `14-tpms-in-mesh.md` (06: the graph, the three mesh-derived defaults written
  out — wavelength = AABB diagonal / 10, centre = AABB centre, bounds = AABB + 5 % per axis —
  `--offset T` = `onion(…,thickness=T)`, `--normalize-thickness` = `normalize(…)` inside it,
  OBJ / STL / 3MF by `-o`, the λ-vs-T guide, the six formulas, Y1, T7–T14);
  `12-dualc_field_view/05-tpms-in-mesh-preview.md` (10: the preview graph and `dualc_raymarch`'s
  defaults, what replaces `↓/↑`, `PgDn/PgUp`, `t`, `n` — tab + nudge for the two scalars, edit the
  file and let it auto-reload for the two structural ones — the mm workflow, M1–M8).
- **Added to existing pages**: 11/09 table L1–L8 (04's eight recipes as graphs); 11/10 X6–X8
  (07's S2–S4) and `dualc_slice`'s defaults (plane at the centre, `<type>_slice`, the `-o`
  rule); 03 a pointer to the per-node `bake=` (D-60 kept the tool, as the plan's item 3 says);
  a one-line "graph form" pointer on 01, 02, 05, 06, 10; 11/README (four page rows, five recipe
  rows, the samples sentence); 12/README (page row); `command_reference/README.md` § *Where a
  feature is documented* rows re-pointed at the new homes (plus the lattice-family row);
  21/07 new, 21/01 #60's status line, 21/README (page line, row), `roadmap/README.md`
  (§ Current focus, Track 1 NEXT); `STRUCTURE.md` (samples row).
- **Six graph files** `examples/samples/{cube_minus_sphere,smooth_blend,displaced_sphere,mesh_shell,mesh_minus_sphere,twisted_mesh}.json`,
  added to `FIELD_GRAPH_SAMPLES` (copied next to `dualc_field` and `dualc_field_view`). JSON,
  not `.fld`: the canonical form, and the mesh paths need no quoting.
- **27 new ctest cases** (`examples/CMakeLists.txt`): per sample `cli_csg_sample_<s>`,
  `cli_field_sample_<s>`, `…_content`, `…_same` (`compare_files`); `cli_field_lattice` (cli_lattice's
  field as a graph), its content check, `cli_field_lattice_same`. No existing test edited. This is
  how "the six samples are files the gate builds or reads" holds.
- No `src/` or `docs/design/` edit → no `/docs-semantic-lint` obligation. No field node,
  `field_graph.cpp`, `field_glsl.cpp` or parity case moved → no GPU tier owed (the viewer was
  still run, for the M recipes).

## Every heading of 01–07 and 10, and its new home

| Old page § | New page § |
| --- | --- |
| 01 (H1 + flag table) | 11/11 § The input (the flag-by-flag table) |
| 01 § Recipes (D1–D10) | 11/11 § Recipes (G1–G9; D2 → G1, D10 → G9) |
| 01 § Choosing a sign oracle | 11/11 § Choosing a sign oracle |
| 01 § Sharp vs. smooth normals (`--sharp`) | 11/11 § Sharp vs. smooth normals (`normals=sharp`) |
| 02 (H1 + flag table) | 11/12 § How a primitive is written |
| 02 § The 30 analytic 3D primitives (Tier A / B / C) | 11/12 § The 30 analytic primitives (P1–P30, one table) |
| 02 § The 6 decorators | 11/12 § The decorators (one child) (D1–D7) |
| 02 § The 6 domain operators | 11/12 § The domain operators (one child) (O1–O6) |
| 02 § The 3 displace functions | 11/12 § The three displace functions (F1–F3) |
| 02 § Recipes | 11/12 § Recipes (C1–C7) |
| 03 (all) | **stays** (D-60): gains the `bake=` pointer, B8 corrected; also 11/11 § Booleans of meshes (H1–H2, `k` sizing) |
| 04 (H1 + flag table) | 11/09 § The two lifts |
| 04 § The 4 2D primitives | 11/09 § The four profiles |
| 04 § Recipes, § Lift 1, § Lift 2 | 11/09 § The four profiles (the L1–L8 table) and § Recipes |
| 05 (H1 + flag table) | 11/13 § The samples, the *A mesh of your own* paragraph |
| 05 § Recipes (R1–R6) | 11/13 § Recipes (K1–K6) |
| 06 (H1 + flag table + the composing callout) | 11/14 § The graph and its defaults |
| 06 § Wavelength vs. offset | 11/14 § Wavelength vs. offset |
| 06 § TPMS formulas, § The 6 TPMS types | 11/14 § The six TPMS (Y1–Y6) |
| 06 § Recipes (T7–T13) | 11/14 § Recipes (Y1, T7–T14) |
| 07 (H1 + flag table + plane rule) | 11/10 § Options, § What it writes, and the *`dualc_slice`'s defaults* note |
| 07 § Recipes (S1–S4) | 11/10 § Recipes (X4, X6–X8) |
| 10 (H1 + § CLI flags) | 12/05 § The graph and its defaults; the build flag is 12/README's |
| 10 § Live controls | 12/05 § What replaces the hotkeys; the shared keys and section planes 12/01 § Controls |
| 10 § Recipes (M1–M8) | 12/05 § Recipes |
| 10 § See also | 12/README (the viewer itself) |

## Measured, and under what

Linux, GCC 15, Ninja, Release, binaries in `build/examples/` (the viewer `build-gl/examples/`,
rebuilt first, GL renderer NV92); outputs compared with `cmp`. The full table is 21/07 § *Every
recipe, run once*; the headline:

- **Byte-identical to the old tool**: all six samples (164,268 / 58,300 / 135,132 / 272,184 /
  164,268 / 150,204 faces at depth 7); all 30 primitives bare; D1–D7, O1–O6, F2–F3, C1–C7;
  G1, G3, G4, G6–G9; Y1, T8, T9 on the cube, Y3 and a normalised shell on the off-centre
  `knot.obj` (T14's awk computes centre / wavelength / box exactly as `dualc_lattice`); L1–L8;
  X7 (PNG + SVG); H1, H2 against `dualc_boolean`.
- **Run, not compared**: T7, T12, T13, G9 as written (`sealed.stl`, 65,508 faces), K5's torus
  variant, X6 (12,338 segments, looked at: gyroid walls inside the bunny), X8 (17,474);
  M1–M8 and M4 at `thickness=0.002` as `--snapshot` PNGs, looked at.
- **Not run**: G5 (molde at depth 9), T10, T11, and the foot lattice at depth 7 (the old tool's
  OBJ alone was 371 MB; the twin was stopped by the 10-minute background limit).
- **Full gate** `python3 scripts/check.py` before `4b1f439`: 32 checks, 29 passed, 2 SKIP
  (configure; warnings — 0 TUs compiled, the change is CMake + JSON + docs), 0 failed; **ctest
  358, 0 failed** (331 + 27 new, 240 s, run while other jobs loaded the host). `--fast` after
  every later docs edit: 28 checks, 0 failed; `flag-table` 11 pages, 67 flags, 443 recipe flags,
  0 problems. `--docs --strict`: § Close.
- **New ctest alone** (`ctest -R "cli_field_sample|cli_csg_sample|cli_field_lattice"`): 28 tests
  (27 + the `cli_lattice` fixture), 0 failed, 21 s.

## What I got wrong on the way

- 11/12's first draft explained O1's twist as visible "because of the tube" — invented; the
  torus is invariant about y and the twist shows nothing. Replaced with page 02's statement
  before any commit.
- H2 first carved `cube.obj` by `sphere.obj`, page 03's B8: one placeholder triangle (the
  radius-1 sphere swallows the cube). Both now carve `genus2.obj` (155,736 faces; B8 baked
  44,776) — B8's command is corrected on 03, its ID kept.
- The first loss-audit run scoped only the eight pages; the script refuses (exit 2) unless every
  changed base docs file is in scope, so it ran with `--scope docs/` and the rows were filtered
  by source. A `pkill -f docs_loss_audit.py` killed my own shell (its command line matched).
- The awk one-liner's first form keyed initialisation on `NR==1`; replaced by `!(i in lo)` and
  forced numeric `+0` before it was compared against the Python AABB.

## Open — for Giacomo

1. **07's S2 and 10's M4** used a 1-unit wall on the 0.15-unit bunny (S2 also cut below it). X6
   is corrected (`y=0.11`, `thickness=0.002`); M4 is kept as written with a note on 12/05
   pointing at 0.002. Say if you want M4 changed too.
2. **New recipe IDs where tool 11 already used the letter** (G for 01's D, K for 05's R, X6–X8
   for 07's S2–S4), each with a *Was* column; P/D/O/F/C, Y/T, L, M kept. If you prefer the old
   letters kept, it is a rename on 11/11, 11/13, 11/10 and the 11/README rows.
3. **`dualc_raymarch --snapshot` renders wrongly on this host's NV92** (saturated colours, a
   black box on M8); `dualc_field_view` renders the same graphs correctly. Not pursued — the tool
   retires in P06 — but if anyone needs `dualc_raymarch` before then, it is broken here.
4. Still open from earlier units: P04's two (the refusal of `--depth` etc. with `--slice`; the
   region padding rule), P03.2's `vec2` binding / polygon ceiling, P02's list.

## Exit criteria of P05, one by one

- *The framework pages alone document every capability the eight tools had* — every heading
  maps (table above); every flag of the seven retiring tools has its graph or option equivalent
  in a mapping table (11/11, 11/12, 11/13, 11/14, 11/09, 11/10, 12/05); `dualc_boolean`'s
  booleans, `k` sizing, `--sharp` and baking are in 11/11 § Booleans of meshes and 11/01 §
  Sources although D-60 keeps the tool. Held by the loss audit (§ Loss audit): nothing on the
  eight pages is lost that has no home, and what is not carried is named, with its reason.
- T1's acceptance — the heading table (above); the audit (below); full gate green with the six
  samples built (copied by `FIELD_GRAPH_SAMPLES`) and run by 24 of the 27 new ctest cases.

## Loss audit

Against the **branch base** `2fcb6ee` (`git merge-base HEAD main`), head = a scratch commit
(local branch `scratch/p05-audit` in a `/tmp` worktree, never pushed, removed after): this
branch's final docs with the seven retiring pages deleted as P06 will (03 kept, D-60).
`python3 scripts/docs_loss_audit.py --base 2fcb6ee --head ee51325 --scope docs/ --untriaged`,
scratch triage (`/tmp`: audit key `2fcb6ee`, the script's thresholds, no items, no rules);
4 min 30 s per run. `--scope` over the eight pages alone exits 2 (the script requires every
changed base docs file in scope), so the run is tree-wide and the rows were filtered to the
eight pages by source. Tree-wide it reports thousands of untriaged rows, as for P03.2 / P04.1
(unchanged files, the raw audit dump re-parsed).

On the eight pages: **400 rows** on the first run, **396** after the patches (`f0ea075`) — 246
sentences, 116 identifiers, 23 headings, 11 evidence tokens; by page 02 141, 06 68, 10 57, 01 43,
04 32, 07 31, 05 23, 03 1. Every one triaged by hand into four classes, recorded in 21/07 §
*What P06's deletion would lose*: (1) old command lines, synopsis lines and flag-table rows —
superseded by the graph twin named in a *Was* column or mapping table; (2) the 23 headings —
the map above; (3) eight facts the first pass had dropped — now carried (`f0ea075`); (4) four
things not carried on purpose, each with its reason (raymarch's thin-wall hole note and
missing-mesh message, `dualc_demo`'s flag-override order, D9 / D10's never-existing files). The
one row on 03 is B8's old command, corrected. **Nothing untriaged that P06's deletion would
lose.** No docs file was deleted in this unit, so no `scripts/docs_loss_audit.json` entry was
written; P06 runs the audit for real before its deletion commit, and this triage is its
starting point.

## Remote run

`python3 plan/remote_run_check.py` on `f0ea075` (the code commit `4b1f439` plus the triage
docs): run [38866229941](https://github.com/Giacogiak/DualC/actions/runs/38866229941) —
completed, **success**; the script printed `OK branch feat/cli-consolidation HEAD f0ea075 green`
and exited 0. This handoff's own commit (docs + plan only) is pushed after it; the driver's
remote-run check reads that HEAD's run.

## Close

`python3 scripts/check.py --docs --strict` after the last docs edit: 28 checks, 27 passed, 1
INFO (index-staleness, report only, the same 12 pre-existing rows), 0 failed. Memory note
updated (outside the repo).

**Human check asked for**: the M recipes on 12/05 were judged on `--snapshot` frames only; the
interactive half of *What replaces the hotkeys* (`tab` to `wavelength` / `thickness`, then
`↓` / `↑`; editing the file and the auto-reload) was not exercised in a window. Worth one look
in `dualc_field_view` on a real GPU: M3 from a `.fld` file, `tab` + `↑`, then swap `schwarz_p`
for `gyroid` and save.

## What the next unit (P06, retire the tools) needs to know

- **ctest twins exist** for `cli_csg_demo` (six `cli_field_sample_*`) and `cli_lattice`
  (`cli_field_lattice`); before deleting the tools, give each `…_same` test a fixed expectation
  (or drop it and keep the content check), as P04 noted for `cli_field_slice_same_*` and
  `cli_field_lift_same`. `cli_csg_sample_*` run `dualc_csg_demo` and must go with it. Still no
  twin: `cli_demo_*` (dualc_demo), `cli_primitive*`, `cli_lift` has `cli_field_lift`.
- **Inbound links to the old pages' anchors** (`01-dualc_demo.md#choosing-a-sign-oracle`,
  `02-dualc_primitive.md` from 11/01 and 12/03, `#sharp-vs-smooth-normals---sharp` from 03, …)
  must be re-pointed at the new homes before the pages become tombstones; `grep -rn
  "0[124567]-dualc_\|10-dualc_raymarch" docs/ README.md STRUCTURE.md` lists them.
- 11/README § *Scope* still says `dualc_field` "generalises but does not replace" `dualc_lattice`
  / `dualc_csg_demo`, and its intro calls them shortcuts — true until P06, then stale.
  `command_reference/README.md` § *Where a feature is documented* already points at the new
  homes; its tool table, keyboard map, options matrix and inventory are P06's.
- Byte room: 11/README 15,205 / 15,360 (P06 must replace text, or move the recipe table to a
  child); `command_reference/README.md` ~15,100; 21/01 15,357 (#60's landing must go to 21/07 or
  a new 21/08); `roadmap/README.md` ~15,030.
- Next free IDs unchanged: **#62**, **D-63**.

---

← Back to the [plan](../README.md).
