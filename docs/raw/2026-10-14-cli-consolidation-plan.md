# DualC CLI consolidation plan — one framework, `dualc_field` + `dualc_field_view`

**Drafted 2026-10-14**, from the owner's answers to the
[CLI tools screening](2026-10-14-cli-tools-screening.md) of the same day. An input, not a
record: status lives in the roadmap block the plan opens (P01 T1), never here. Each task is
**one fresh session** driven by `/plan-run`, opened and closed with `/repo-docs-lifecycle`; a
task that does not fit a session is closed partial, with what was verified and what remains
written into its record page.

## 0. The goal, in the owner's words

> The focus of DualC must be to maintain a unified framework that includes all features and
> functions. The established framework is based on `dualc_field` and `dualc_field_view`, with
> the possibility to combine functions with expressions and serialize to JSON.

The screening found that five of the eleven live CLIs are byte-identical re-spellings of a graph,
that three hold one capability the graph lacks, and that `dualc_raymarch` is superseded by
`dualc_field_view`. Because the C ABI Boletus links (`capi/dualc_c.h`) is the graph and nothing
else, every capability that lives only in a side CLI is also out of Boletus's reach: closing the
gaps serves the ABI, not only the command line.

## 1. The owner's decisions

Taken 2026-10-14; recorded as decision rows by P01 T1; the ids are read from `docs/decisions/` at phase time
(the last on the drafting day is **D-54**, the last tracked item **#54**, the next roadmap
block **21**).

| Tool | Decision | Precondition the plan builds |
| --- | --- | --- |
| `dualc_csg_demo` | retire as-is | its six recipes ship as ready-to-run graph files (P05) |
| `dualc_primitive` | retire as-is | its primitive catalogue (P1–P30, decorators, domain operators) moves into the `dualc_field` docs (P05) |
| `dualc_raymarch` | retire as-is | its recipes move to `dualc_field_view` (P05) |
| `dualc_demo` | retire after a graph addition | `mesh(…, sign="gwn")` (P02 T1). Finding F2 (OBJ-only output) is **not fixed**: the tool goes; `dualc_field -o` already picks OBJ / STL / 3MF from the extension |
| `dualc_boolean` | retire after a graph addition | a `bake` parameter on the mesh nodes (P02 T2) |
| `dualc_lattice` | retire; its defaults become a documented recipe | the recipe (P05) |
| `dualc_lift` | move into the graph, then retire | 2D profile nodes + `revolve` / `extrude` (P03) — fires D-11's trigger "a profile-defined solid" for the 2D sub-grammar; `GridField` as an author node and the arbitrary-lambda `displaced` stay DEFERRED |
| `dualc_slice` | move into the graph, then retire | slicing of any graph (P04). The owner allowed either form; the plan takes **`dualc_field <graph> --slice AXIS=VALUE`**, because it keeps the user surface at the two framework CLIs, which is the stated goal. The owner confirms or overrides at the approve gate after P01 |
| `dualc_gen_demo`, `dualc_glsl_parity`, `dualc_io_stress` | keep | infrastructure, not modelling tools; F1 is fixed in `dualc_gen_demo` (P01 T2) |

Retired tool numbers become tombstone pages, as `08-dualc_view.md` is; a number is never reused.

## 2. Repo facts the plan relies on

All verified on the drafting day against `main` at `2fcb6ee`.

- **Equivalence evidence**: the screening's eight byte-identical tool-vs-graph runs. One more,
  for the owner's soup workflow: `dualc_demo --gwn-field --depth 7 mesh-soup.obj` and
  `dualc_field --expr 'winding(path="mesh-soup.obj")' --depth 7` write byte-identical OBJ
  (65,508 faces), and the same graph with `-o soup.stl` writes binary STL.
- **`sign="gwn"` is a guard, not a gap in the library.** `field_graph.cpp`'s `meshSignMethod`
  throws on `gwn`, citing the 12/01 note that "`MeshSource` has no GWN ctor". That note is
  stale: `MeshSource(mesh, geom, interpolateNormals, SignMethod)` takes
  `GENERALIZED_WINDING_NUMBER`, and `dualContourMesh` routes through it
  (`src/pipeline.cpp`, `src/sampler.cpp` `sampleMeshToHermiteOctree`) — which is how
  `dualc_demo --gwn` works today. The GPU path bakes a `mesh` node through
  `MeshSource::bakeToGrid` (`field_glsl.cpp` `emitMesh` → `emitBakedGrid`), so the sign oracle
  is inside the bake.
- **`dualc_boolean --bake N`** (`examples/dualc_boolean.cpp`): region = the two meshes'
  combined AABB padded by `0.10 · extent + k`; band = `k` (smooth ops only) + 3 cells; both
  meshes go through `MeshSource::bakeToGrid(region, N³, band)`. A per-node `bake` cannot see the
  parent's `k` or the sibling's box, so it will not be byte-identical to the tool; P02 T2 states
  the per-node rule and tests it against the exact field.
- **2D layer** (`include/dualc/implicit2d.h`): `Circle2D`, `Box2D`, `Segment2D`, `Polygon2D`
  (`ImplicitField2D`, `Field2DPtr`), `RevolveField(profile, axisOffset)` around y,
  `ExtrudeField(profile, halfHeight)` along z. No 2D booleans exist; none are in scope. The graph
  builder returns 3D `FieldPtr` only; `field_glsl.cpp` has no 2D emitter. `tests/test_lift.cpp`
  covers the library classes.
- **`dualc_slice`** (`examples/dualc_slice.cpp`, 491 lines): axis-plane parse, sampling grid,
  symmetric-range colormap PNG through the vendored `stb_image_write`, marching-squares SVG
  contour; the field is hard-wired to `intersection(tpms, MeshSource)`.
- **Inside-out demo meshes (F1)**: signed volume negative for `torus`, `knot`, `cylinder`,
  `bracket`, `hexbore`, `uvsphere` (generated by `examples/demo_meshes.cpp`); positive for
  `sphere`, `cube`, `genus2`, and for every non-generated mesh in `data/` (`bunny`, `foot`,
  `molde`, `opA`, `opB`, `mesh-soup`). Parity sign ignores orientation, which is why no test
  noticed; `--gwn`, `--gwn-field` and `winding(…)` on `torus` / `knot` give the empty placeholder,
  and `--pseudonormal bracket.obj` gives 55,832 faces against 34,984.
  `tests/test_demo_meshes.cpp` exists and checks no orientation.
- **What references the retiring tools**: 19–46 files each outside `build/` and `docs/raw/` —
  `examples/CMakeLists.txt` (targets; `cli_*` tests at lines ~310–480 and the content list fed
  to `examples/check_cli_output.cmake`), `CMakeLists.txt` (`DUALC_BUILD_RAYMARCH_VIEWER`, the
  GLFW condition), `scripts/check.py` (`check_flag_table` special-cases `dualc_primitive`'s
  registry flags), `scripts/docs_loss_audit.json`, `README.md`, `STRUCTURE.md`, `AGENTS.md`,
  `docs/design/09-conventions.md`, every `docs/command_reference/` page and many roadmap records
  (records are history: they are **not** rewritten). Boletus names them only inside its vendored
  copy of DualC; it depends on the ABI, not on any CLI.
- **Shared code**: `raymarch_gl.{h,cpp}` is used by `dualc_field_view` and `dualc_glsl_parity`
  too and stays. `example_common`'s primitive / post-op / boolean registries are the graph's
  vocabulary and stay; `csgRecipes` / `buildRecipe` serve only `dualc_csg_demo`.
- **The gate**: `python3 scripts/check.py` (docs + configure + build + warnings + serial ctest);
  `--gpu` runs the parity harness and asserts `parity_expected_cases` (73, in
  `scripts/check_data.json`) — every new GLSL node adds cases and bumps that number in the same
  commit. The GL build tree is `build-gl/` (`DUALC_BUILD_FIELD_VIEW`, `_GLSL_PARITY`,
  `_RAYMARCH_VIEWER` ON); run `python3 scripts/check.py --gpu --build-dir build-gl` after
  `cmake --build build-gl -j`. On the drafting day `build-gl` was **stale** (linked during
  #31's deliberate `opXor` break, F3) — the first `--gpu` of the run rebuilds it.
- **Branch rule**: `main` is protected, PR only. The whole plan works on one branch,
  **`feat/cli-consolidation`**, pushed (never `main`, never force) so hosted CI runs every
  phase; the owner opens and merges the PR after P07. No `gh` CLI on the host: runs are read on
  the public Actions pages / API.

## 3. Phases

Dependency order: P01 opens the record and fixes the data every later test reads; P02 and P03
add graph vocabulary (each touches CPU builder, GLSL codegen, parity, C ABI docs); P04 adds the
slice output; P05 migrates every page's content to the framework pages while the old tools still
exist to check against; P06 deletes; P07 closes.

### P01 — Open the record, fix the demo meshes

- **T1. Record.** Roadmap block **21 — CLI consolidation**, a folder from the start
  (`21-cli-consolidation/README.md` + a record child), with one tracked item per
  precondition — demo-mesh orientation, `mesh` GWN sign, `mesh` bake, 2D + lift nodes, graph
  slice, tool retirement — and the decision rows of § 1 (one row per retired tool, or one row
  per group, by the decisions index's own style), the D-11 row's trigger marked fired for the
  2D sub-grammar only, the screening linked as evidence. `roadmap/README.md` § Current focus /
  § Next up. A dated note in 21 that 12/01's "`MeshSource` has no GWN ctor" is stale (12/01 is
  a record and is not rewritten).
- **T2. F1.** Fix the triangle winding of the six inside-out shapes in `demo_meshes.cpp`. Add to
  `tests/test_demo_meshes.cpp`: every generated shape has positive signed volume, and the
  generalized winding number at an interior point is ≈ 1. Verify `dualc_demo --gwn-field
  torus.obj` and `knot.obj` now produce geometry and that `--pseudonormal bracket.obj` matches
  parity's face count. Expect the output `vn` of `cli_demo_torus` and friends to flip outward
  (smooth Hermite normals come from the input); check `check_cli_output.cmake` and every
  recorded count that might move. Page 09 states the outward-orientation guarantee.

### P02 — Mesh-node parameters: `sign="gwn"` and `bake`

- **T1. `sign="gwn"`.** Remove the guard in `meshSignMethod`, map `gwn` →
  `SignMethod::GENERALIZED_WINDING_NUMBER`; the `--list` text, the `--expr` shorthand and
  `--dump-json` round-trip carry it. Unit test: `mesh(sign="gwn")` on a closed mesh equals
  parity's output, and on an open shell differs from `winding` (it is the sign oracle, not the
  sealing field). Equivalence: `dualc_demo --gwn torus.obj` ≡
  `dualc_field --expr 'mesh(path="torus.obj",sign="gwn")'`, byte-identical. GLSL: a parity case
  `mesh-gwn` (count +1). Docs: 11/01 § Sources, `design/02-sign-oracles.md` if it names the
  graph's sign set, the C ABI page if it lists node params.
- **T2. `bake`.** `mesh(…, bake=N[, band=W])` and `winding(…, bake=N)`: the node bakes itself
  through `bakeToGrid` over its own AABB padded by 10 % of its extent (`dualc_boolean`'s rule
  without the sibling and without `k`); `band` defaults to 3 cells, and the docs say to widen it
  to the parent's `k` for a smooth op. `N` validated (≥ 2, a ceiling with a clear message). The
  GLSL emitter feeds a node's own `GridField` straight to `emitBakedGrid`, so preview == export
  for a baked node (and `--grid-res` no longer applies to it). Tests: the baked field is within
  one bake cell of the exact one near the surface; `dump-json` round-trip; parity case
  `mesh-bake` (count +1). Equivalence with `dualc_boolean --bake N` is **not** byte-level (§ 2):
  record face counts of both on the screening's `difference sphere cube` case, side by side.

### P03 — 2D profiles and `revolve` / `extrude` in the graph

- **T1. CPU.** Node tokens `circle`, `rect` or `box2` (the session picks a token that does not
  clash with the 3D `box`, recording the choice), `segment`, `polygon` (`points=[[x,y],…]`, and
  the shorthand form), and lifts `revolve(profile, offset=O)` / `extrude(profile, height=H)`.
  2D nodes are legal **only** as a lift's child: anywhere else a located `GraphError`; a lift
  with a 3D child likewise. `--list` gets a "2D profiles / lifts" group. Tests in
  `test_field_graph.cpp`. Equivalence, byte-identical: `dualc_lift revolve circle 0 0 0.5
  --offset 2` and `dualc_lift extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` against their graphs.
- **T2. GPU.** GLSL for the four 2D distances and the two lifts (Quilez forms; the polygon as a
  loop over a constant array, its vertex count bounded and documented); parity cases for each
  (count bumped); `dualc_field_view` parameter nudging works on them. `--gpu` green.

### P04 — Slice any graph: `dualc_field --slice`

- **T1.** Move the slice core out of `dualc_slice.cpp` into a shared module
  (`examples/field_slice.{h,cpp}`), field-agnostic. `dualc_field <graph> --slice AXIS=VALUE
  [--res N] -o base(.png)` writes `base.png` + `base.svg` and contours nothing; the region is
  `--bounds` or the graph's bounds (unbounded → the existing error); `--slice` refuses
  `--tile-depth`, `--mem`, `--weld`, `--decimate`, `--simplify`. Equivalence: `dualc_slice
  cube.obj --type gyroid --wavelength 0.5 --offset 0.1 --plane z=0 --res 128` against the
  equivalent graph (TPMS centred where `dualc_slice` centres it) — byte-identical PNG and SVG.
  A `cli_field_slice` ctest with content checks; a new 11 child page (flag table, recipes).

### P05 — Migrate every page's content into the framework docs

- **T1.** Before anything is deleted, everything a retiring page teaches gets a home under
  `11-dualc_field/` or `12-dualc_field_view/`: the primitive catalogue P1–P30 with parameters
  and the decorator / domain-operator walk-through (02); booleans incl. `--k` sizing and
  baking (03); lifts (04); the six recipes, each as a committed graph file in
  `examples/samples/` with a ready-to-run line (05); the lattice recipe with its defaults
  written out — wavelength = mesh AABB diagonal / 10, centre = AABB centre, bounds = AABB + 5 %
  — and `--normalize-thickness` = `normalize(…)` (06); slicing (07, done in P04); the mesh-in
  workflow and *Choosing a sign oracle*, including **the soup recipe**
  `dualc_field --expr 'winding(path="soup.obj")' -o sealed.stl` (01); density-independent
  preview of a TPMS-in-mesh (10 → 12). Every migrated recipe is run once and its result
  recorded. `command_reference/README.md`'s feature map points at the new homes. The old pages
  stay until P06.

### P06 — Retire the eight tools

- **T1. Code and tests.** Delete the eight `examples/dualc_*.cpp`, their CMake targets,
  `DUALC_BUILD_RAYMARCH_VIEWER` (and its arm of the GLFW condition), `csgRecipes` /
  `buildRecipe` and any other code only they used. Every retired `cli_*` test gets a
  `dualc_field` twin covering the same path (mesh-in smooth / sharp, primitive + post-ops,
  infinite primitive with `--bounds`, boolean, lift, lattice OBJ / STL / 3MF, slice), so ctest
  coverage of each path survives; the content checks follow. `check.py`'s flag-table special
  case for `dualc_primitive` goes. `build-gl` re-configured without the removed option. Full
  gate and `--gpu` green.
- **T2. Docs.** Pages 01–07 and 10 become tombstones in `08-dualc_view.md`'s form (what it was,
  when retired, where its content lives now); the tool index, the inventory, the common-options
  matrix and the keyboard map in `command_reference/README.md`; `README.md`, `STRUCTURE.md`,
  `AGENTS.md` (option list), `docs/design/09-conventions.md`; `scripts/docs_loss_audit.json` if
  it names them. **Run `scripts/docs_loss_audit.py` against the branch base before committing
  the deletions** and triage every residual into its new home. Roadmap records keep naming the
  tools: they are history.

### P07 — Close

- **T1.** Harvest the handoffs into block 21's record (measurements, rejected approaches,
  deviations), tracked items → DONE with evidence, decision rows final, `roadmap/README.md`
  snapshot and milestones row, `/docs-semantic-lint` (this is a src + design run), delete
  `plan/` per D-51, final `check.py` + `--gpu` + `--docs --strict`, push the branch, and hand the
  owner the compare link for the PR.

## 4. Out of scope

2D booleans and 2D domain operators; `GridField` as an author node (a baked grid file as input);
an arbitrary-lambda `displace`; a slice on a non-axis plane; a `--format` flag (the `-o`
extension already decides OBJ / STL / 3MF); changes to Boletus (the ABI gains vocabulary, no
symbol). The ABI version moves by the rule block 14 applied to the last vocabulary change;
the session that adds the first new node records which.
