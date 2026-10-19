# Semantic lint — after the CLI consolidation plan

Run on 2026-10-16, after the CLI consolidation plan ([21](../../21-cli-consolidation/README.md),
#55–#60): the `mesh` node took `sign="gwn"`, per-node `bake`, 2D profiles with
`revolve` / `extrude` and `--slice` moved into the graph, and seven side CLIs retired. It
edited `design/02` and `design/09`. It is a **delta run** over
[06](06-2026-10-07-after-ci-plan.md), taken at the branch base `2fcb6ee`. For (a) and (b) it
reads every design page that names a file changed since then, and greps all of `design/`
for the retired tools and the deleted symbols. Classes (c), (d) and (e) are swept whole.
Line numbers are as read at `f5dcf0f`, before this run's fixes. The gate was green before
the read (`check.py --docs`, 28 checks, 27 OK, 1 INFO).

**Inputs.**

- **Last code change:** `b1a2b9f` (2026-10-16, #60, seven CLIs retired).
- **Last design change:** `0efb96e` (2026-10-16, `design/09`); `design/02` changed on
  2026-10-14.
- **`doc-lag`:** 0 commits.
- **Code changed since `2fcb6ee`:**
  - `examples/`: `field_graph.{h,cpp}`, `field_glsl.{h,cpp}`, `field_slice.{h,cpp}` (new,
    from `dualc_slice.cpp`), `dualc_field.cpp`, `dualc_field_view.cpp`,
    `dualc_glsl_parity.cpp`, `example_common.{h,cpp}`, `demo_meshes.cpp`, `raymarch_gl.h`,
    `CMakeLists.txt`, six sample graphs;
  - seven tools deleted (`dualc_demo`, `dualc_primitive`, `dualc_lift`, `dualc_csg_demo`,
    `dualc_lattice`, `dualc_slice`, `dualc_raymarch`), along with `example_common`'s
    recipe and post-op parsers;
  - `tests/test_{field_graph,field_glsl,demo_meshes}.cpp`.
- **No change to `src/`, `include/` or `capi/`.**

**Pages read for (a) and (b).**

- `design/09`, all of it, against `examples/example_common.{h,cpp}`,
  `examples/field_graph.cpp:720-745`, `examples/dualc_field.cpp`,
  `examples/field_slice.{h,cpp}`, `examples/dualc_field_view.cpp:340-400` and
  `examples/CMakeLists.txt:410-420`.
- `design/10:8-50` and `:149-185` against `examples/field_glsl.{h,cpp}`,
  `examples/field_graph.h`, `examples/dualc_glsl_parity.cpp:230-240`,
  `examples/example_common.cpp:1635-1690,1856-1862` and `tests/test_demo_meshes.cpp`.
- `design/02` against `src/internal/sign_oracle.cpp`, `src/internal/mesh_bvh.cpp:475-530`
  and `include/dualc/types.h:18-30`.
- `design/08:97-119` (bakes, the 2D layer) and `design/11:58-67` (the graph rows).

## Findings

| # | Class | Finding | Docs `file:line` | Code `file:line` | Disposition |
| --- | --- | --- | --- | --- | --- |
| 1 | (a) | "The output format is chosen by the `-o` extension … and by nothing else … any other extension is an error". In `dualc_field`, `--slice` writes `BASE.png` + `BASE.svg` whatever `-o` ends in: `-o` becomes a base (default `field_slice`), a `.png` / `.svg` suffix is stripped, and any other suffix is kept as part of the name. The slice's default region reuses the sampler's `padFraction`, but `design/10`'s row names only `padBBox` | `design/09-conventions.md:39-46`; `design/10-invariants-and-tolerances.md:173` | `examples/dualc_field.cpp:437,457-480`; `examples/field_slice.cpp:255-262` | fixed in this session's commit: `design/09` scopes the rule to mesh exports and states `--slice`'s output; the `padFraction` row names `--slice` |
| 2 | (a) | "A destination file only ever appears complete. Every export writes `path + ".part"`" and "Every writer targets `path + ".part"` … never a truncated output". `--slice`'s writers open the destination directly, with no `.part`, no rename and no cleanup on failure. `00-shared-behaviour` scopes the same rule to the shared mesh writer and is right | `design/10-invariants-and-tolerances.md:45-50`; `design/09-conventions.md:58,77-78` | `examples/field_slice.cpp:126,201` | fixed in this session's commit: both pages scope `.part` to the mesh writers and say the slice writes in place; routing it through `AtomicOutput` is not taken (21/09 § Open) |
| 3 | (a) | `design/02` now says, rightly, that parity needs closed input, **not** oriented input (21/02 measured it). Two code comments still say "closed, oriented". 21/09 lists only the header's copy | `design/02-sign-oracles.md:25-27` | `include/dualc/types.h:22-23`; `src/internal/sign_oracle.cpp:49` | handed to [21/09 § Open](../../21-cli-consolidation/09-plan-run.md#the-consolidation-plans-run--p01-to-p07-harvested): a comment edit in `src/` + `include/`, not made in the close unit |
| 4 | (b) | *Preview equals export* names only `field_graph.{h,cpp}`. Four facts are missing. A node with its own `bake=N` uploads the builder's grid bit for bit. An unbaked `mesh` / `winding` node is still re-baked over the trace box at `--grid-res`, so for it the preview is a filtered approximation. A `polygon` over `kMaxGlslPolygonVertices` = 64 is refused by the preview only, with no ceiling on export. 2D points bind as a new `vec2` uniform type that hosts must upload | `design/10-invariants-and-tolerances.md:36-41` | `examples/field_glsl.cpp:1264-1276,1169-1173`; `examples/field_glsl.h:92-96`; `examples/dualc_field_view.cpp:116` | fixed in this session's commit: `design/10` names `field_glsl` and links the exceptions on the new `design/12` |
| 5 | (c) | The per-node bake rule (D-62) is stated in the present tense only in the record and on the usage page. A node samples its own bounds plus 10 % of their largest extent plus max(`band`, margin). Its band defaults to 3 cells and is at least margin + 3 cells. The margin comes from `childBakeMargin`'s table. The ceiling is 512. `design/08` describes `bakeToGrid` only as a library call | `roadmap/21-cli-consolidation/04-mesh-node-bake.md:11-23,41-60`; `command_reference/11-dualc_field/01-op-vocabulary.md:106-121` | `examples/field_graph.h:152-170`; `examples/field_graph.cpp:1135-1159,1230-1251,1260-1281` | fixed in this session's commit: new `design/12` § Per-node bake; 11/01 links it |
| 6 | (c) | The graph's 2D typing rule is stated only in the record and on the usage page. A profile is legal only as a lift's one child, and a lift refuses a 3D child. `rect`, not `box`, so a misplaced profile is always a typing error. One reader, `readProfile`, serves builder and codegen alike. `design/08` and `design/11` describe the library's 2D layer, not the graph rule | `roadmap/21-cli-consolidation/05-2d-profiles-and-lifts.md:18-22,79-84`; `command_reference/11-dualc_field/09-2d-profiles-and-lifts.md:18,37-39` | `examples/field_graph.h:202-231`; `examples/field_graph.cpp:1195-1203,1224-1228` | fixed in this session's commit: `design/12` § The 2D layer; 11/09 links it |
| 7 | (e) | D-40's trigger is met: `docs/raw/` holds 29 text files and 1,149,176 bytes of text (SVG and HTML excluded), over 1 MB. This plan's three raw files crossed it: the 2026-10-14 plan and screening, and the P06 loss audit | `decisions/README.md:53` | `docs/raw/2026-10-16-loss-audit-2fcb6ee-p06.txt` (132,475 B) | recorded on D-40's row (met, 29 files, 1.15 MB); build or re-defer is the owner's call |
| 8 | (e) | D-33's trigger is met, though no outside caller reported it: P02.2 measured a wrong result traced to baked-region reads. A baked node under a `smooth-union` read the grid's clamp, giving 3× the faces and a max deviation of 1.56. `gridBoundsExceeded` did not fire, because the grid was nested and the root box auto-fitted. D-62 answered it by preventing the read, not by counting it, and D-33's row does not say so | `decisions/README.md:46`; `roadmap/21-cli-consolidation/04-mesh-node-bake.md:29-35` | `src/sampler.cpp:208-222`; `examples/field_graph.cpp:1267-1276` | recorded on D-33's row (met, answered by D-62, counting not taken); the owner's call |

**(a) — 3 findings, above.** Read and true:

- **Retired names:** no design page names a retired tool or a deleted symbol in the
  present tense. `design/09:119` links the keyboard map by its kept heading anchor, which
  is not rewritten (principle 3).
- **`design/09`:** the unit commitment (`example_common.cpp:1083-1086`), `lowerExt`,
  `dispatchWrite`, `AtomicOutput`, `OutputFile` and `lastError` hold. So do the keyboard
  rules: `x`/`y`/`z`/`f` matched by `glfwGetKeyName`, `r`/`l` by keycode, nudge and slide
  on the arrows, `[`/`]` as alternates. The primitive registry (`PrimEntry`,
  `primitiveLayouts`, `primitiveParams`), the `.obj`-only `dualc_gen_demo` and
  `cli_gen_demo_out/` hold too.
- **`design/02:25-27`:** `countRayHits` takes nanort's default trace options (no back-face
  cull), so parity is orientation-blind. `:78-81` still holds with the graph's `gwn`: the
  node builds `MeshSource` with the method its oracle queries.
- **`design/10:149-185`:** the `--tile-depth` bounds, `pickTileDepthForBudget`'s range and
  the parity tolerances (`A = R = 2e-3`, `FA = FR = 3e-2`) are unchanged.

**(b) — 1 finding, above.** `design/11`'s *Lift*, *Field graph*, *GLSL codegen* and
*Preview == export* rows stay true at glossary depth once finding 4 lands in `design/10`.
`design/08:117-119` (the library 2D layer) is unchanged.

**(c) — 2 findings, above.** Other hits:

- The seed grep adds only `roadmap/21-cli-consolidation/01-record.md:125` ("how
  `dualc_demo --gwn` works today"), inside the dated 2026-10-14 entry. That is history.
- 21/02–21/08 describe what landed, each dated by its page.
- The slice region is a flag default on 11/10:27, which is that page's job. Its design
  home is finding 1's `design/10:173` row.
- The polygon ceiling on 11/09:10-13 and 12/03:109-111 is usage. Its design home is
  finding 4.

Notes, not findings — outside the five classes, both fixed in this session's commit:

- `command_reference/11-dualc_field/01-op-vocabulary.md:94` explains `gwn` as
  "`dualc_demo --gwn`", a present-tense pointer to a retired flag.
- `command_reference/00-shared-behaviour.md:65` reads "the six above", but `:39-40` above it
  name two tools (`dualc_field`, `dualc_boolean`).

**(d) — 0 findings.** No page outside `docs/raw/` lacks an inbound link from outside its
folder README. Newborn pages checked:

- 11/09–11/14, 12/05;
- 21/01–21/08, and 21/09 (staged, linked from `roadmap/README.md`);
- 17/17, 14/06 and 20/04.

[06](06-2026-10-07-after-ci-plan.md) is linked from `roadmap/09-io-formats.md` and from
this run.

**(e) — 30 DEFERRED rows read, 2 triggers met.**

- **D-11:** its remaining parts are unmet. `mesh(bake=N)` bakes a mesh, which is not a
  grid asset as input, and no lambda displacement exists.
- **D-12:** one mesh under two margins is two textures, but no Grasshopper graph has
  reported paying for it.
- **D-13:** `--slice` is now in the graph, but no print pipeline takes layers.
- **D-53:** `dualc_field_view.cpp` and `raymarch_gl.h` changed only comments, one
  `glUniform2f` arm and the bake report. No window or snapshot code changed.
- **#61:** the tracer's hit and miss test is untouched; only comments changed.
- **#25:** `revolve` / `extrude` wire existing library classes, so no wrapper or decorator
  was added.
- **D-44:** no CLI progress display.
- **#18:** its retired-tool trigger was re-anchored to 11/14. D-14, whose trigger named
  `dualc_lattice`, moved to dropped (21/09 flags that a unit, not the owner, made that call).
- **D-52:** CI is reported green (21/09).
- The rest are unchanged.

## Counts

(a) 3 · (b) 1 · (c) 2 · (d) 0 · (e) 2 — **8 findings**. Five come from one cause: the
graph gained three mechanisms — per-node bake, 2D typing and the slice output — and
`design/` has no field-graph page to hold them. The bake, the typing rule and the polygon
ceiling went to the record and the usage pages. The slice output went nowhere, so
`design/09` and `design/10` still describe `dualc_field`'s outputs as mesh exports only.

---

← Back to the [semantic-lint runs](README.md) · the [Docs layers index](../README.md).
