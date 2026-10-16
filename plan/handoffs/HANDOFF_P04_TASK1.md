# Handoff — P04.1: a field-agnostic slice module and `dualc_field --slice AXIS=VALUE` (#59)

Unit P04.1 of [`plan/13_P04_graph-slice.md`](../13_P04_graph-slice.md) § T1, the only (and
so the last) unit of P04; session of 2026-10-16, branch `feat/cli-consolidation` (pushed to
`origin`; `main` untouched). The record is
[roadmap 21 #59](../../docs/roadmap/21-cli-consolidation/01-record.md#59-slice-any-graph), now
**DONE**, with its evidence on a new page,
[21/06](../../docs/roadmap/21-cli-consolidation/06-slice-any-graph.md); the usage contract is
the new [11/10](../../docs/command_reference/11-dualc_field/10-slice.md). The form is D-58's,
confirmed by Giacomo at the P01 gate (2026-10-15).

## What changed, and why

Commits: `d201539` (code, tests, docs), then this handoff.

- **`examples/field_slice.{h,cpp}`**, new lib `dualc_examples_slice` (links
  `dualc_examples_common` + `dualc_examples_img`): `dualc_slice`'s core moved verbatim —
  `sampleSlice`, `symRange`, `colormap`, `writeHeatmapPng`, `interp`,
  `marchingSquaresZero`, `writeContourSvg` (file-local) — behind one public call,
  `dce::writeFieldSlice(field, region, axis, pos, res, pngPath, svgPath, tag)`, plus
  `parseSlicePlane`, `clampSliceRes` / `kSliceDefaultRes` 512 / `kSliceMinRes` 2 /
  `kSliceMaxRes` 4096, `withExt` (dualc_slice's `-o` rule) and `stripSliceExt` (dualc_field's).
  The only changes to the moved code: the log prefix is the `tag` argument, and the
  plane-outside warning + `plane …, res …` line moved into `writeFieldSlice`. This lib is
  now the only CLI code that links the PNG writer.
- **`dualc_slice.cpp`** (491 → ~210 lines): composes its field as before, prints its
  `type = …` line, calls `writeFieldSlice`. Its usage text printed a literal `5%%` (a
  `printf` escape in an `std::cerr` stream); now `5%`.
- **`dualc_field.cpp`**: `--slice AXIS=VALUE`, `--res N` (clamped like `dualc_slice`).
  The graph is built, never contoured. Region: `--bounds`, else `field.bounds()` padded by
  `padFraction` (5 %) of the **largest** extent — the sampler's own auto-fit rule
  (`src/sampler.cpp` `padBBox`). Unbounded → *[dualc_field] error: this field is unbounded
  -- pass --bounds … to give the slice an explicit finite region.*, exit 1 (the contour
  path's code for the same case); an invalid (empty) bounds box gets the same message with
  "empty". `-o` default `field_slice`; trailing `.png` / `.svg` (any case) stripped, any other
  path used as the base.
- **Refusals** (all before the input is read, exit 2): every one of the eight mesh flags —
  the plan's five plus `--depth`, `--collapse`, `--no-manifold` — naming the first given
  (*--slice writes no mesh; it cannot be combined with --depth*); `--res` without `--slice`;
  `--slice` with `--dump-json`.
- **The plan's open choice — contour-only flags refused, not warned-and-ignored.** Why
  (recorded in 21/06 § *Why the contour-only flags are refused* and on 11/10): a slice has no
  octree; an accepted flag reads as having acted (a `--depth 9` slice is the same bytes as
  `--depth 5`) and a warning scrolls past in scripts; one rule for all eight is one sentence.
  Cost: a contour command line cannot become a slice by appending `--slice` alone.
- **CMake / ctest** (`examples/CMakeLists.txt`): `cli_field_slice` (the S1 graph,
  `-o cli_field_slice.png` so the stripping is exercised), `cli_field_slice_content` (PNG
  128×128) and `cli_field_slice_svg_content` (`<line `) — exactly `cli_slice`'s two checks —
  `cli_field_slice_same_png` / `_svg` (`cmake -E compare_files` vs `cli_slice`'s outputs),
  and `cli_field_slice_refuse` (`PASS_REGULAR_EXPRESSION`; the `;` of the message is a `.`
  in the regex because a `;` would split the CMake list into two alternatives). No existing
  test edited.
- **Docs**: new 11/10 (flag table with real defaults, the refusals, outputs, errors, recipes
  X1–X5); 11/README (synopsis line, `--slice` / `--res` rows, `-o` row note, page row, recipe
  row X1–X5); `command_reference/README.md` feature-map row; 07 (one pointer to 11/10); new
  21/06; 21/01 #59 DONE (status line rewritten + a one-line landing: 15,337 / 15,360 bytes);
  21/README page line + row; `roadmap/README.md` § Current focus, Track 1 NEXT (#60, P05/P06),
  block-21 row, milestone row for #59 (the 2026-10-07 **#52** row left the five-row window,
  D-43; it lives in [17/16](../../docs/roadmap/17-code-audit-and-hardening/16-windows-rename-race/README.md));
  D-58's row in `decisions/01-settled.md` notes P04's landing; `STRUCTURE.md` (field_slice
  row, dualc_slice / dualc_field / stb rows, the libs list); `THIRD_PARTY.md` (stb's
  consumers). No `src/` or `docs/design/` edit → no `/docs-semantic-lint` obligation. No
  field node, `field_graph.cpp`, `field_glsl.cpp` or parity case moved → no GPU tier owed.

## Measured, and under what

Linux, GCC 15, Ninja, Release; binaries in `build/examples/`; outputs compared with `cmp`.

- **The plan's equivalence**: `dualc_slice cube.obj --type gyroid --wavelength 0.5 --offset
  0.1 --plane z=0 --res 128` vs `dualc_field --expr
  "intersection(onion(gyroid(center=[0,0,0],wavelength=0.5),thickness=0.1),mesh(path=\"cube.obj\"))"
  --slice z=0 --res 128 --bounds -0.55,-0.55,-0.55,0.55,0.55,0.55` — **PNG and SVG
  byte-identical**, SHA-256 PNG `743c40a39583499b7188dc183a88c3fa3903e3d9520ea908deb6dea06db0a982`,
  SVG `619150915e8660d0ab66827cf323b5a26aa62ed8684334746357b986a38efbf2`, 1432 segments.
  `cube.obj` spans [-0.5, 0.5]³ (AABB centre 0; `-0.5 - 0.05 == -0.55` in double, checked).
  Without `--bounds` the graph's region on the cube coincides → same bytes too.
- **Two extra equivalences**: the normalised Schwarz-P on `x=0.1` at 200² (2464 segments),
  and the empty `y=0.7` plane at 64² with the default wavelength √3/10 (0 segments, both
  warnings) — byte-identical.
- **`dualc_slice` before vs after the move**, three runs (the two above + the S1): all six
  files byte-identical to the pre-refactor binary's.
- **Recipes X1–X5**, each run once: segment counts 6127 / 5188 / 14306 (1024²) / the S1
  bytes / 5792. X2 (the hexbore shell) and X3 (the bracket layer)
  PNGs looked at: walls closed round hex and bore; the lattice only inside the L. A torus
  (`revolve(circle(0,0,0.5),offset=2)`, `y=0`) slice looked at too: the annulus, inside blue.
- **Full gate** `python3 scripts/check.py`, the three changed TUs touched first: 32 checks,
  30 passed, 1 SKIP (configure), 0 failed; **3 TUs compiled, 0 DualC-origin warnings;
  ctest 331, 0 failed** (85.9 s; 325 + 6 new). `--docs --strict` green (28 checks).
- **CI**: § Remote run. **Loss audit**: § Loss audit.

## What I got wrong on the way

- The first extra equivalence wrote the graph as `normalize(onion(schwarz_p))` and differed;
  `dualc_slice --normalize-thickness` is `onion(normalize(tpms))`. Corrected; 11/10 says so.
- The first refusal-test regex carried the message's `;`, which CMake reads as a list
  separator (two alternatives, either passing) — replaced with `.` before any run.
- An early loss-audit loop ran the whole audit once per file (> 5 min); stopped, re-run once.

## Open — for Giacomo

1. **The refusal of `--depth` / `--collapse` / `--no-manifold`** with `--slice` is my call
   (the plan allowed either). If you prefer warn-and-ignore, it is a 5-line change in
   `dualc_field.cpp` (split `meshFlags` in two) plus 11/10's paragraph and 21/06's section.
2. **Region padding differs from `dualc_slice` off-cube**: `--slice` pads 5 % of the largest
   extent (the contour's rule), `dualc_slice` 5 % per axis. Chosen so a slice covers what a
   contour samples; documented on 11/10 with the `--bounds` workaround.
3. Still open from earlier units: P03.2's `vec2` binding (DualCEditor's uniform loop), the
   polygon ceiling, nested arrays off `points`; P02's list.

## Exit criteria of P04, one by one

- *Any graph can be sliced from `dualc_field`* — `--slice` works on the built `FieldPtr`, so
  every node the builder accepts slices: run on a bare TPMS with `--bounds` (X1), a mesh
  under `onion` (X2), mesh ∩ lattice (X3, X4), a JSON file (X5), a lift (the torus), a
  sphere; ctest `cli_field_slice` + content checks.
- *`dualc_slice` is reproduced byte-identically* — the plan's S1 pair `cmp`-identical, hashes
  above, held by ctest `cli_field_slice_same_png` / `_svg`; plus two more pairs.
- T1's acceptance: full gate green with `flag-table` holding `--slice` and `--res` to the
  pages (11 pages, 67 flags, 383 recipe flags, 0 problems); `remote_run_check.py` — § Remote
  run; item DONE in block 21 (21/01, 21/README, roadmap README).

## Remote run

`python3 plan/remote_run_check.py` on the code commit `d201539`: run
[38859390269](https://github.com/Giacogiak/DualC/actions/runs/38859390269) — completed,
**success**; the script printed `OK branch feat/cli-consolidation HEAD d201539 green` and exited
0. This handoff's own commit (docs only) is pushed after it; the driver's remote-run check
reads that HEAD's run.

## Loss audit

`python3 scripts/docs_loss_audit.py --base 8063fd4 --head d201539 --scope docs/ --untriaged`
with a scratch triage (`/tmp`: audit key `8063fd4`, the script's thresholds, no items, no
rules), one run (4 min). Tree-wide it reports thousands of untriaged rows (unchanged files,
and the 2026-09-20 raw audit dump re-parsed), as for P03.2. Filtered to the nine docs files
the commit changed, **5 rows gone / partial, all intended**: #59's "the form … awaits the
owner's confirmation" (superseded — D-58's row records the confirmation, and #59's status
line now says so); the #59 status cell in 21/README (PLANNED → DONE); `roadmap/README.md`
§ Current focus's "#55–#58 are DONE; NEXT is #59" and Track 1's "NEXT: #59" (→ #59 DONE,
NEXT #60); the 2026-10-07 **#52** milestone row, which left the rolling five-row window
(D-43) and lives in 17/16. No docs file was deleted.

## What the next unit (P05, docs migration) needs to know

- Page **07 `dualc_slice`** is already covered by 11/10 (P05 T1 item 7 is "check"): every
  flag of 07 maps — `--plane` → `--slice`, `--type/--wavelength/--offset/--normalize-thickness`
  → the graph (`gyroid(center=AABB centre,wavelength=…)`, `onion(…,thickness=T)`,
  `onion(normalize(…))`), `--bounds` / `--res` / `-o` identical (the `-o` stripping differs:
  `dualc_slice` replaces any extension). What 07 still holds and 11/10 does not spell out:
  its default plane (bounds centre — `--slice` has no default), its default wavelength
  (AABB diagonal / 10) and its S2 / S4 recipes on `bunny` / `foot`.
- **P06** must give `cli_field_slice_same_png` / `_svg` a fixed expectation (they read
  `cli_slice.png` / `.svg`) before deleting `dualc_slice`, as for `cli_field_lift_same`;
  `field_slice.cpp`'s header comment and `CMakeLists.txt`'s stb comments name `dualc_slice`.
- Next free IDs: **#62**, **D-63** (#61 was opened 2026-10-16 for the viewer's specks).
- Byte room: 21/01 at 15,337 / 15,360 — #60's landing must replace text or go to a new
  child (21/07); `roadmap/README.md` 14,999; `command_reference/README.md` 14,591;
  11/README 13,942.

---

← Back to the [plan](../README.md).
