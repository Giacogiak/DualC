# P03 — 2D profiles and `revolve` / `extrude` in the graph

§ 2 (the 2D layer) and § 3 P03 of
[the raw plan](../docs/raw/2026-10-14-cli-consolidation-plan.md). Fires D-11's trigger for the
2D sub-grammar only. Branch `feat/cli-consolidation`.

### T1. CPU: the 2D nodes and the two lifts

1. Profile tokens for `Circle2D`, `Box2D`, `Segment2D`, `Polygon2D`
   (`include/dualc/implicit2d.h`). The 2D box needs a token that cannot be confused with the 3D
   `box` (for example `rect`); choose, and record why in block 21. Parameters by name
   (`center=[x,y]`, `radius`, `half=[x,y]`, `a=[x,y]`, `b=[x,y]`, `points=[[x,y],…]`), plus the
   flat `params=[…]` form the 3D primitives accept.
2. Lifts `revolve(profile, offset=O)` (around y, `RevolveField`) and `extrude(profile, height=H)`
   (half-height along z, `ExtrudeField`), exactly one child each.
3. Typing: a 2D node anywhere but as a lift's child, or a lift whose child is 3D, is a located
   `GraphError`. The builder may grow a second entry point for 2D nodes; keep one parser.
4. `--list` gains a "2D profiles / lifts" group; `--expr` and `--dump-json` round-trip them.
5. Tests in `tests/test_field_graph.cpp` (each node, both typing errors, round-trip).
6. Equivalence, byte-identical, recorded in block 21:
   `dualc_lift revolve circle 0 0 0.5 --offset 2` and
   `dualc_lift extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` against their graphs.
7. Docs: an 11/01 section for the 2D profiles and lifts (`dualc_field_view` cannot show them
   until T2 — say so until then).

#### Acceptance criteria

- Full gate green; both equivalences byte-identical; the CPU half recorded in block 21.

### T2. GPU: GLSL for the 2D distances and the lifts

1. `examples/field_glsl.cpp`: the four 2D distance functions and the two lifts, in the
   formulas the C++ uses (the parity tolerance is the contract). The polygon as a constant array
   and a bounded loop; document the vertex ceiling and refuse above it with a located error.
2. Parity cases: each profile under `extrude`, at least one under `revolve`, the polygon at a
   concave shape; bump `parity_expected_cases`.
3. `dualc_field_view`: parameter nudging (`tab` + arrows) works on the new scalar params.
4. Docs: 12's recipes by node gain a lift recipe; remove T1's "not in the viewer yet" note.

#### Acceptance criteria

- `python3 scripts/check.py --gpu --build-dir build-gl` green with the new count; full gate
  green; a `--snapshot` PNG of one revolve and one extrude looked at and named in the handoff.

## Exit criteria

- 2D profiles and lifts in the graph, CPU and GPU, parity green, `dualc_lift`'s two recipes
  reproduced byte-identically; `python3 plan/remote_run_check.py` exits 0.
