# The seven tools retired (#60, P06)

The evidence page of the second half of [#60](01-record.md#60-retire-the-eight-tools), plan
P06, on branch `feat/cli-consolidation`. T1 — the code and the tests — landed 2026-10-16;
T2 (the tombstones, the indexes, the loss audit) follows. Seven tools, not eight:
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

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
