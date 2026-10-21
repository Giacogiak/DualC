# P04 — The field and the graph node

§ 3 *The node, as designed* and § 3 P04 of
[the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md); the defaults from the block's
`01-study.md`. Branch `feat/porous-shell`. Never push.

### T1. PorousField and the porous graph token

1. In `src/implicit/porous.cpp`, write `PorousField : ImplicitField`. Its value is the packed
   query from P03. It also has:
   - a gradient (analytic for the bisector and primitive distances where it is cheap, otherwise
     a central difference), following `implicit.h`'s gradient contract as the hygiene run left it;
   - `bounds()` = the volume's box;
   - the default `cellOverlaps`, with a comment giving the Lipschitz-1 argument.

   The volume is read at build time only.
2. In `examples/field_graph.cpp`, the `porous` token takes child 0 = the volume, an optional
   child 1 = the control, and every key of the raw plan's signature with its default. It covers
   build, shorthand, the `dumpJson` round trip, `rejectUnreadParams` and a `--list` group entry in
   `examples/dualc_field.cpp`. Thread the cancel token into the build if the graph build sees one;
   if it does not, record that.
3. Tests (`test_field_graph.cpp`, `test_porous.cpp`): the three properties of the raw plan § 3
   P04 T1, the round trip, a mesh volume, a missing `spacing` error, and the domain-wrapper
   property test in `test_domain_ops.cpp` if the node qualifies.

   *Corrected by P01.1:* the raw plan's "`smooth` lowers the value by at most `k/4`" holds only
   where two primitives are within `k` of the minimum. A fold of three (a generic cell corner)
   lowers it by 0.39k, of four by 0.48k, and never by `k` or more. Test `k/4` at two-primitive
   points and `< k` everywhere, by [23/01's defaults table](../docs/roadmap/23-porous-shell/01-study.md#the-defaults-table).
   Name every new Catch2 case `porous: …`; `-R 'field_graph'` matches no test (the existing
   names read `field-graph …`).

#### Acceptance criteria

- `ctest --test-dir build -R 'porous|field_graph|domain'` is green, and `dualc_field --list`
  shows the node.

### T2. Contour the sponge end to end

1. Run the raw plan's four recipes through `build/examples/dualc_field … -o out.stl` on a sphere
   and on a demo mesh (`build/data/`), for `cells=voronoi|delaunay` and `mode=walls|struts`. Pick
   depths that keep each run under about a minute.
2. Add a test that contours a small `walls` sponge and asserts it is watertight
   (`Diagnostics`) with **more than one connected component**. Write the component counter in
   the test (a union-find over faces).
3. Commit two or three `examples/samples/porous-*.json` graphs, with their ready-to-run lines.
4. Record for each case: seeds, tets, faces, components, depth and wall time, in the block's
   record page.

#### Acceptance criteria

- The component test passes. The samples are committed, and their measurements are in the block.

## Exit criteria

- `dualc_field` produces a watertight, multi-cavity sponge from any volume, with tests.

← Back to the [plan](README.md).
