# The seven tools retired (#60, P06)

The evidence page of the second half of [#60](01-record.md#60-retire-the-eight-tools), plan
P06, on branch `feat/cli-consolidation`. T1 — the code and the tests — and T2 — the
tombstones, the indexes, the loss audit — landed 2026-10-16. Seven tools, not eight:
`dualc_boolean` stays (D-60).

## T1 — the code and the tests

- **Deleted**: `examples/dualc_{demo,primitive,lift,csg_demo,lattice,slice,raymarch}.cpp` and
  their targets; the option `DUALC_BUILD_RAYMARCH_VIEWER` and its arm of the GLFW condition
  (root `CMakeLists.txt`) and of the GL-plumbing condition (`examples/CMakeLists.txt`).
  `raymarch_gl.{h,cpp}` stays: `dualc_field_view` and `dualc_glsl_parity` link it.
  `dualc_examples_slice` stays: `dualc_field --slice` is now its one caller.
- **Code only they used**, out of `example_common.{h,cpp}`: `RecipeEntry`, `csgRecipes`,
  `findRecipe`, `buildRecipe` (`dualc_csg_demo`); `isPostOpFlag`, `parsePostOp` (the
  `--onion`-style flags of `dualc_primitive`); `parseVec3`, `parseVec2`, `isNumberToken`,
  `paramNames` (no caller left — `paramNames` served the viewer retired in #50). The registries
  stay — primitives, `PostOp` / `applyPostOps`, `BoolOp`: the graph's vocabulary. `parseDoubles`
  stays, under `parseBounds`. 184 lines.
- **Comments** that named a deleted tool as a live caller now say *retired*
  (`field_glsl.cpp`, `field_graph.{h,cpp}`, `field_slice.{h,cpp}`, `raymarch_gl.h`,
  `dualc_field{,_view}.cpp`); no code line of them moved.
- **`scripts/check.py`**: `check_flag_table`'s `dualc_primitive` case (the post-op flags read out
  of `example_common.cpp`) is gone, and with it the last `"--onion"`-style literals there. A
  page whose `examples/<tool>.cpp` is gone is skipped, so the flag-table check went from
  11 pages, 67 flags, 443 recipe flags to 4, 27 and 304 — pages 01, 02, 04–07 and their
  recipes are no longer read against a parser; T2 turns them into tombstones.
- **CI**: no change. No job of `.github/workflows/gate.yml` ever configured the option, and
  `check.py` configures only `_TESTS` and `_EXAMPLES`. `build-gl` was re-configured without it
  (`cmake -U DUALC_BUILD_RAYMARCH_VIEWER`) and its stale tool binaries removed.

## The tests: every retired path has a `dualc_field` twin

Each twin was run against its old tool before the deletion, Linux, GCC 15, Ninja, Release,
`build/examples/`, compared with `cmp`:

| Path | Retired test | Twin | Faces | Against the old tool |
| --- | --- | --- | --- | --- |
| Mesh in, smooth | `cli_demo_sphere`, `cli_demo_torus` | `cli_field_mesh_sphere`, `cli_field_mesh_torus` | 31,644 / 40,032 | byte-identical |
| Mesh in, sharp | `cli_demo_bracket_sharp` | `cli_field_mesh_sharp` (`normals=sharp`) | 34,984 | byte-identical |
| Mesh in, GWN sign | — | `cli_field_mesh_gwn` (`sign=gwn`) | 31,644 | = `dualc_demo --gwn` |
| A soup through `winding` | — | `cli_field_winding` | 20,804 | = `dualc_demo --gwn-field` |
| Primitive + post-ops | `cli_primitive` | `cli_field_primitive` (`twist(onion(torus))`) | 25,584 | byte-identical |
| Infinite primitive, `--bounds` | `cli_primitive_infinite` | `cli_field_infinite` (`plane`) | 450 | byte-identical |
| Mesh boolean | `cli_boolean` (kept) | `cli_field_boolean` + `cli_field_boolean_same` | 5,292 | byte-identical, held by ctest |
| Mesh boolean, baked | — | `cli_field_boolean_bake` (`bake=64` per node) | 2,028 | = `dualc_boolean --bake 64` |
| Revolve | `cli_lift` | `cli_field_lift` (already there) | 9,520 | byte-identical |
| Extrude | — | `cli_field_extrude` (11/09's R2 prism) | 10,092 | = `dualc_lift extrude polygon` |
| Lattice → OBJ / STL / 3MF | `cli_lattice{,_stl,_3mf}` | `cli_field_lattice{,_stl,_3mf}` | 17,316 | OBJ, STL byte-identical; 3MF same entries (below) |
| Slice | `cli_slice` | `cli_field_slice` (already there) | — | byte-identical (P04) |
| The six recipes | `cli_csg_demo`, `cli_csg_sample_*` | `cli_field_sample_*` (already there) | 10,092 / 3,644 / 8,460 / 16,440 / 10,092 / 9,548 | byte-identical (P05) |

**The `_same` tests went with the tools** they compared against (`cli_field_lift_same`,
`cli_field_slice_same_{png,svg}`, six `cli_field_sample_*_same`, `cli_field_lattice_same`).
A file of expected bytes or exact counts was not put in their place: CI builds on Linux,
Windows and macOS, and a contour's exact face count is not promised across compilers. Instead
each twin's content check carries **`MIN_FACES` at about half the count above**, so an empty
contour (the one-triangle placeholder an inside-out mesh gives) fails on every platform; the
six samples' checks gained the same floor. The one equivalence that stays is
`cli_field_boolean_same`: `dualc_boolean` is kept, so it can be compared for good.

**The soup** is written by CMake into the build tree (`examples/cli_soup.obj`: two overlapping
boxes, each without its +y face, 20 triangles — `*.obj` is gitignored, and `mesh-soup.obj` is
not in the repository). `cli_field_winding` fails on a *not closed* warning: the same soup
through `mesh(…)` prints one, through `winding(…)` none.

**ctest: 358 → 344.** 39 cases out (19 tool runs, 10 content checks, 10 `_same`), 25 in (12
`dualc_field` runs, 12 content checks, `cli_field_boolean_same`); the `cli_*` cases 81 → 67,
the unit tests unchanged.

**The gate**, 2026-10-16, on the code commit's tree: a fresh build directory
(`python3 scripts/check.py --build-dir <new dir>` — fetch, configure, full compile; `--clean`
re-runs configure in place and so proves less) 32 checks, 31 passed, 0 failed: 251 TUs, 0
DualC-origin warnings, ctest 344 passed in 103 s. The GPU tier on the re-configured `build-gl`:
parity 82/82.

## Found on the way

- **The 3MF twin differs at byte 11** — the ZIP entries' modification time. Unpacked, the two
  archives' entries are identical (`diff -r`); the content check compares the triangle count.
- **The per-node bake matched the tool's bake byte for byte** here (both operands the same
  `cube.obj`, `k` 0.3, depth 5). The per-node region is not the tool's in general
  ([04](04-mesh-node-bake.md), D-62), so no test holds the two to it.

## T2 — the tombstones, the indexes, the loss audit

- **Tombstones**: pages 01, 02, 04, 05, 06, 07 and 10 are rewritten in 08's form — the H1
  kept (an anchor), the retirement date and decision (D-55 … D-58), this page as the record,
  and a *Was / Now* table sending each old section to its home under 11 or 12 (P05's heading
  map). The numbers are never reused. The two links into old anchors were re-pointed: 03's
  *Sharp vs. smooth normals* to 11/11, the keyboard map's *Live controls* to 12/01 and 12/05.
- **The command-reference index**: the tool index (seven rows say *Retired 2026-10-16*), the
  build prelude's opt-in list (`DUALC_BUILD_FIELD_VIEW`, `_GLSL_PARITY`), the keyboard map
  (one viewer: the `dualc_raymarch` column and its four lattice rows out), the common-options
  matrix (four columns — the `view` column #50 left behind went too), the export list, the
  inventory (4 tools, the composition samples, `rect`, the decorators by node name).
- **Other live pages**: 00's warnings list; 08's *Now* cell; 09's G2, G3, G5 as `dualc_field`
  graphs (run: 40,032, 34,984 and 60,180 faces — G2 and G3 equal to the T1 twins); the 11 and 12
  pages that called themselves "the graph form of" a tool now say it *replaced* it, and 11's
  § Scope names the two framework CLIs as the user surface; `README.md`'s tool list,
  `STRUCTURE.md`, `AGENTS.md`'s opt-in list, `THIRD_PARTY.md`, design/09 (`t`, `n`, the Page
  keys and `-` / `=` were `dualc_raymarch`'s, so the layout rule now lists `r`, `l`, the arrows
  and `[` / `]`).
- **Decisions**: #18's trigger reads "a TPMS lattice in a mesh" instead of the tool; **D-14**
  (`dualc_lattice --tile-depth`) moved to the dropped table — its trigger, a `dualc_lattice`
  user, can no longer fire — with a dated line on 11/03 § 5a.
- **Left as they are**: `scripts/docs_loss_audit.json` names the pages only as base-tree
  sources of the 3d73220 audit, and no `record` there cites them; the roadmap records (history);
  `tests/test_field_graph.cpp`'s SECTION name (a passing test).

### The loss audit

Before the rewrites were committed, `scripts/docs_loss_audit.py --base 2fcb6ee --scope docs/`
ran against a scratch commit of the working tree (never a branch, never pushed), with a scratch
triage (the script's thresholds, a ceiling above the tree-wide noise); then again on the
committed docs. As in P05 the tree-wide count is mostly rows of unchanged files, so the audited
set is the rows T2 can cause: every row on the eight pages, and every row not already residual
on this branch before T2 (the same run on 8b154e1). Each class below is a scratch-triage rule
or item the script verifies, with its citation:

- **Class A — superseded by its P05 home**: a row of the eight pages whose content lives under
  11 or 12, the tombstone's table naming where; P05's triage of the same rows is 21/07.
- **Class B — a recorded drop**: what P05 did not carry on purpose (21/07), still not carried.
- **Class C — rewritten in place**: a sentence of a live page that named a retired tool as
  present, now in the past tense or pointing at the graph; a table row of a retired tool's
  column or row; the D-14 row, moved to the dropped table; the #55 milestone row, out of the
  rolling five-row window (D-43) with #60's arrival.
- **Class D — noise of quotation**: an identifier on an unchanged quotation-only page (the
  2026-09-20 audit dump, roadmap 19's phase-6 record) that was "kept" only through a retired
  page or a deleted source file.

**The numbers**, 2026-10-16: tree-wide 9,207 residual rows against the pre-T2 run's 8,797 —
the noise of unchanged files P05 met too; the audited set **437 rows**, 391 of them on the
eight pages (P05's run found 396 there; the tombstones keep a few of their words): **393 class
A, 6 B, 36 C, 2 D — 0 untriaged**, every citation resolved, exit 0. Four minutes a run. The
report — the audited set row by row, its counts, the scratch triage file — follows in
`docs/raw/`, from the same run on this commit.

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
