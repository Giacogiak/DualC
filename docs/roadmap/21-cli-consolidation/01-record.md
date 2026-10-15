# CLI consolidation — the tracked items and the owner's decisions

Part of [CLI consolidation](README.md) (block 21). Opened 2026-10-14 by the plan-run unit
P01.1 on branch `feat/cli-consolidation`, from the
[consolidation plan](../../raw/2026-10-14-cli-consolidation-plan.md) (an immutable input) and
its evidence, the [CLI tools screening](../../raw/2026-10-14-cli-tools-screening.md) of the
same day: five of the eleven live CLIs re-spell a graph byte for byte, three hold one
capability the graph lacks, and `dualc_raymarch` is superseded by `dualc_field_view`. One item
per precondition the plan builds; each opens with its acceptance line, quoted from the plan's
phase, and takes dated appends as its phase lands.

## The owner's decisions

**DONE — decided 2026-10-14** (the owner's answers to the screening, the plan's § 1). The goal,
in the owner's words: *"The focus of DualC must be to maintain a unified framework that
includes all features and functions. The established framework is based on `dualc_field` and
`dualc_field_view`, with the possibility to combine functions with expressions and serialize
to JSON."* Because the C ABI Boletus links is the graph and nothing else, a capability that
lives only in a side CLI is also out of Boletus's reach: closing the gaps serves the ABI too.

- **D-55** — `dualc_csg_demo`, `dualc_primitive` and `dualc_raymarch` retire as-is, and
  `dualc_lattice` with its defaults written out as a recipe, once their content lives on the
  framework pages (#60); a retired tool's number becomes a tombstone page, as
  `08-dualc_view.md` is, and is never reused.
- **D-56** — `dualc_demo` retires once the `mesh` node takes `sign="gwn"` (#56). The
  screening's finding F2 (the tool writes OBJ only) is **not fixed**: the tool goes, and
  `dualc_field -o` already picks OBJ / STL / 3MF from the extension.
- **D-57** — `dualc_lift` moves into the graph — 2D profile nodes and `revolve` / `extrude`
  (#58) — then retires. This fires [D-11](../../decisions/README.md)'s trigger "a
  profile-defined solid" for the 2D + lift sub-grammar **only**: `GridField` as an author node
  and the arbitrary-lambda `displaced` stay DEFERRED on D-11's row.
- **D-58** — `dualc_slice` moves into the graph — slicing of any graph (#59) — then retires.
  The owner allowed either form; the plan takes **`dualc_field <graph> --slice AXIS=VALUE`**,
  because it keeps the user surface at the two framework CLIs, which is the stated goal. The
  owner confirms or overrides the form at the approve gate after P01; an override is a new
  decisions row.
- **D-59** — `dualc_gen_demo`, `dualc_glsl_parity` and `dualc_io_stress` stay:
  infrastructure, not modelling tools. F1 is fixed in `dualc_gen_demo` (#55).
- **D-60** — `dualc_boolean` is the one retirement not decided:
  [§ The dualc_boolean decision](#the-dualc_boolean-decision).

## The dualc_boolean decision

**DEFERRED — 2026-10-14.** Trigger: P02 T2's measurements — the face counts of a per-node
`mesh(…, bake=N)` (#57) beside `dualc_boolean --bake N` on the screening's
`difference sphere cube` case — read by the owner at the approve gate after P02. The owner's
words, 2026-10-14: *"I wait P02 measures before to decide whether to remove dualc_boolean."*
The plan's own row says "retire after a graph addition", a per-node `bake`; it cannot be
byte-identical to the tool, whose bake region is the two meshes' combined box padded by
`0.10 · extent + k` and whose band is `k` + 3 cells (plan § 2), which a node cannot see. Until
D-60's row says retire, no unit removes, deprecates or migrates away `dualc_boolean`.

## Opening the record at the cap

**2026-10-14.** Both pages that had to take block 21 sat at the byte cap of
`scripts/check_data.json` `size_caps` (15,360): [`roadmap/README.md`](../README.md) at 15,355
and [`decisions/README.md`](../../decisions/README.md) at 15,322. Nothing was baselined; each
was trimmed to what its own rule allows, and every removed clause already lived in the page it
links (`scripts/docs_loss_audit.py --base d6cc19d`, scratch triage: the 18 residual rows on
the two pages are the clauses below, list lines renumbered or moved, and the reworded intro):

| Page | Removed | Its home |
| --- | --- | --- |
| `roadmap/README.md` § Current focus | the 19 merge detail, the 2026-06-12 re-sequencing's "finished", the hosted-CI plan's per-item list (#51, #31, #33, #32), #49 / #50, #48's date, and "`main` unpushed" (false since 2026-10-07: `origin/main` is `2fcb6ee`) | [19/09](../19-docs-layers/09-pre-merge-loss-audit.md), [12 § The new sequence](../12-field-graph-and-app/README.md#the-new-sequence-value-first-then-the-c-abi), the milestones rows, [20/04](../20-public-delivery/04-hosted-ci-plan-run.md), the block rows of 20 and 07, [14/05](../14-c-abi/05-progress-and-cancel.md) |
| `decisions/README.md` intro | "(architecture, scope, process)" of the settled table | [01-settled.md](../../decisions/01-settled.md)'s own intro |
| `decisions/README.md` #31, #33 status cells | how each fired and what #33 left (`clang-tidy`, coverage → #32) | [17/13](../17-code-audit-and-hardening/13-parity-gate-binding.md), [17/14](../17-code-audit-and-hardening/14-build-hardening-ci.md) |
| `decisions/README.md` D-21, D-29 | "the CLI shipped both, 2026-07-03/06"; "ranked fix menu recorded" | [14 § 9](../14-c-abi/README.md#9-deferred-explicitly-not-in-this-version), [12/04 § Startup](../12-field-graph-and-app/04-preview-performance.md#viewer-startup--shader-compile-time) |

After the edits the snapshot is at 15,182 bytes and the decisions index at 15,317 — 43 bytes
of room, less than D-60's status cell will need when the owner decides it. The index needs a
relief of its own before then, as [19/10](../19-docs-layers/10-cap-relief.md) gave the
snapshot; how is the owner's call, since the index's two-table design is the owner's.

**2026-10-15, at the P01 approve gate.** Giacomo chose to split the DROPPED rows into
[`decisions/02-dropped.md`](../../decisions/02-dropped.md) (D-61): six rows moved unchanged,
`decisions-index` reads both pages (`check_data.json` `decisions_index.index` is a list), and
the index went from 15,317 to about 13,700 bytes. He also confirmed D-58's form,
`dualc_field --slice`.

## #55 Generated demo meshes wound outward

**DONE** 2026-10-14 — opened 2026-10-14; plan P01 T2. The screening's finding F1: six of the nine
`dualc_gen_demo` shapes have negative signed volume (`torus`, `knot`, `cylinder`, `bracket`,
`hexbore`, `uvsphere`); the default parity oracle ignores orientation, so no test noticed, but
`--gwn`, `--gwn-field` and `winding(…)` give the empty placeholder on `torus` / `knot` and
`--pseudonormal bracket.obj` disagrees with parity.

*Acceptance:* the six shapes' triangle order reversed in `examples/demo_meshes.cpp`;
`tests/test_demo_meshes.cpp` asserts positive signed volume for every generated shape and a
generalized winding number ≈ 1 at an interior point (inside the tube for `torus` and `knot`),
shown failing on the old winding and passing on the new; `dualc_demo --gwn-field` and `--gwn`
on `torus.obj` / `knot.obj` produce geometry; `dualc_demo --pseudonormal bracket.obj` matches
parity's face count at `--depth 6`; every recorded count or normal that moves (output `vn` flips
outward) is updated; page 09 states that every generated mesh is closed and wound outward;
before/after numbers recorded here.

*Landed 2026-10-14 (P01.2):* five shapes reversed whole; the **bracket was mixed** — caps out,
walls in — so only its walls flipped. The new `[orientation]` case failed 14 of 18 checks on the
old winding and passes on the new. At `--depth 6`, `--gwn` / `--gwn-field` on `torus` / `knot`
went from the placeholder to 40,032 / 18,320 and 30,116 / 15,316 faces; `--pseudonormal
bracket.obj` from 55,832 to parity's 34,984. Parity's output keeps its faces; its `vn` flip
outward, positions move ≤ 0.0056; no recorded count moved. A side fix: the mesh copies next to
the CLIs went stale on regeneration (`POST_BUILD` only) and are now `LINK_DEPENDS`. Every number,
per shape and oracle: [02](02-demo-mesh-winding.md).

## #56 The mesh node takes sign="gwn"

**DONE** 2026-10-15 — opened 2026-10-14; plan P02 T1. The precondition of `dualc_demo`'s retirement
(D-56): `dualc_demo --gwn` is the one thing it has that the graph refuses.

*Acceptance:* the guard in `field_graph.cpp`'s `meshSignMethod` removed and `gwn` mapped to
`SignMethod::GENERALIZED_WINDING_NUMBER`; `--list`, the `--expr` shorthand and the
`--dump-json` round-trip carry it; a unit test shows `mesh(sign="gwn")` on a closed mesh equal
to parity's output and, on an open shell, different from `winding` (the sign oracle, not the
sealing field); `dualc_demo --gwn torus.obj` and
`dualc_field --expr 'mesh(path="torus.obj",sign="gwn")'` write byte-identical OBJ; a parity
case `mesh-gwn` (the count +1); 11/01 § Sources, `design/02-sign-oracles.md` and the C ABI page
updated where they name the graph's sign set.

**2026-10-14 — 12/01's "`MeshSource` has no GWN ctor" no longer holds.** The note in
[12/01 § A](../12-field-graph-and-app/01-field-graph.md#a-the-field-graph-keystone) and its
twin in [12's README](../12-field-graph-and-app/README.md) are history and are not rewritten;
read at `d6cc19d`, the code says otherwise: `MeshSource(mesh, geometry, interpolateNormals,
SignMethod)` (`include/dualc/implicit.h`) takes `SignMethod::GENERALIZED_WINDING_NUMBER`, and
its constructor (`src/implicit/mesh_source.cpp`) builds the GWN oracle from it;
`dualContourMesh` reaches it through `sampleMeshToHermiteOctree` (`src/pipeline.cpp`,
`src/sampler.cpp`), which is how `dualc_demo --gwn` works today. The guard in
`meshSignMethod` is the only thing between the `mesh` node and GWN. The GPU path bakes a `mesh`
node through `MeshSource::bakeToGrid` (`field_glsl.cpp` `emitMesh` → `emitBakedGrid`), so the
sign oracle is inside the bake.

*Landed 2026-10-15 (P02.1):* `gwn` mapped on the CPU and GPU paths; at `--depth 6` the two
torus commands write byte-identical OBJ (40,032 faces, = parity's), and on `mesh-soup.obj`,
where the oracles differ, so do both tools' `gwn` runs (30,120 faces vs parity's 33,610);
parity case `mesh-gwn` (74); no ABI version move. Numbers: [03](03-mesh-sign-gwn.md).

## #57 Per-node bake on the mesh nodes

**NEXT** — opened 2026-10-14; plan P02 T2. The graph addition `dualc_boolean`'s retirement
would need (D-60): its `--bake N`, the narrow-band SDF-grid bake of each mesh, is a speed /
quality trade no node exposes.

*Acceptance:* `mesh(…, bake=N[, band=W])` and `winding(…, bake=N)`: the node bakes itself
through `bakeToGrid` over its own AABB padded by 10 % of its extent, `band` defaulting to 3
cells, and the docs say to widen it to the parent's `k` for a smooth op; `N` validated (≥ 2, a
ceiling with a clear message); the GLSL emitter feeds the node's own `GridField` straight to
`emitBakedGrid`, so preview == export for a baked node and `--grid-res` no longer applies to it;
tests: the baked field within one bake cell of the exact one near the surface, the `dump-json`
round-trip, a parity case `mesh-bake` (the count +1); the face counts of both on the
screening's `difference sphere cube` case recorded side by side — equivalence with
`dualc_boolean --bake N` is not byte-level.

## #58 2D profiles and revolve / extrude in the graph

**PLANNED** — opened 2026-10-14; plan P03 T1–T2. The precondition of `dualc_lift`'s retirement
(D-57); fires D-11 for this sub-grammar only. The library layer exists
(`include/dualc/implicit2d.h`: `Circle2D`, `Box2D`, `Segment2D`, `Polygon2D`, `RevolveField`,
`ExtrudeField`); the graph and `field_glsl.cpp` have no 2D node.

*Acceptance:* node tokens `circle`, `rect` or `box2` (a token that does not clash with the 3D
`box`, the choice recorded here), `segment`, `polygon` (`points=[[x,y],…]` and the shorthand
form), and the lifts `revolve(profile, offset=O)` / `extrude(profile, height=H)`; a 2D node is
legal only as a lift's child, anywhere else a located `GraphError`, and a lift with a 3D child
likewise; a "2D profiles / lifts" group in `--list`; tests in `test_field_graph.cpp`;
`dualc_lift revolve circle 0 0 0.5 --offset 2` and
`dualc_lift extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` byte-identical with their graphs;
GLSL for the four 2D distances and the two lifts (the polygon as a loop over a bounded,
documented constant array), a parity case for each (the count bumped), parameter nudging in
`dualc_field_view` working on them, `--gpu` green.

## #59 Slice any graph

**PLANNED** — opened 2026-10-14; plan P04 T1. The precondition of `dualc_slice`'s retirement
(D-58); the form `dualc_field <graph> --slice AXIS=VALUE` awaits the owner's confirmation.

*Acceptance:* the slice core moved out of `dualc_slice.cpp` into a field-agnostic shared module
(`examples/field_slice.{h,cpp}`); `dualc_field <graph> --slice AXIS=VALUE [--res N] -o
base(.png)` writes `base.png` + `base.svg` and contours nothing; the region is `--bounds` or the
graph's bounds (unbounded → the existing error); `--slice` refuses `--tile-depth`, `--mem`,
`--weld`, `--decimate`, `--simplify`; `dualc_slice cube.obj --type gyroid --wavelength 0.5
--offset 0.1 --plane z=0 --res 128` and the equivalent graph (the TPMS centred where
`dualc_slice` centres it) write byte-identical PNG and SVG; a `cli_field_slice` ctest with
content checks; a new 11 child page with its flag table and recipes.

## #60 Retire the eight tools

**PLANNED** — opened 2026-10-14; plan P05 T1, P06 T1–T2. The eight: `dualc_csg_demo`,
`dualc_primitive`, `dualc_raymarch`, `dualc_lattice` (D-55), `dualc_demo` (D-56), `dualc_lift`
(D-57), `dualc_slice` (D-58) — and `dualc_boolean` only if D-60 says retire.

*Acceptance:* before anything is deleted, everything a retiring page teaches has a home under
`11-dualc_field/` or `12-dualc_field_view/` — the primitive catalogue and the decorator /
domain-operator walk-through, booleans with `--k` sizing and baking, lifts, the six recipes as
committed graph files in `examples/samples/`, the lattice recipe with its defaults written out,
slicing, the mesh-in workflow and *Choosing a sign oracle* with the soup recipe, the
density-independent TPMS-in-mesh preview — each migrated recipe run once and its result
recorded; then the tools' sources, CMake targets, `DUALC_BUILD_RAYMARCH_VIEWER` and the code
only they used are deleted, every retired `cli_*` test has a `dualc_field` twin on the same path,
`check.py`'s flag-table special case for `dualc_primitive` goes, pages 01–07 and 10 become
tombstones in `08-dualc_view.md`'s form, `scripts/docs_loss_audit.py` runs against the branch
base before the deletions are committed and every residual is triaged; the full gate and
`--gpu` green. Roadmap records keep naming the tools: they are history.

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
