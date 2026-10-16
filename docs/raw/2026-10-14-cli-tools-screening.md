# CLI tools screening — which tool is still an asset, which is a duplicate of the field graph

Screened 2026-10-14 at commit `2fcb6ee`, read-only. The question: the framework is `dualc_field`
(compose any op as a JSON or `--expr` graph, contour once) and `dualc_field_view` (the same graph
compiled to GLSL and raymarched), and the C ABI Boletus links (`capi/dualc_c.h`) exposes only that
graph — `dualc_field_create_from_json` / `_from_expr`, then contour or export. Which of the other
CLIs add something the graph cannot express, and which only re-spell it? Status, decisions and
any follow-up live in the roadmap and the decisions index, never here.

## Method

- Every tool's `--help` and `dualc_field --list`, from `build/examples/` (Release, Ninja) and
  `build-gl/examples/` for the GL tools.
- The sources: `examples/dualc_*.cpp`, `examples/field_graph.cpp` (the node builder),
  `examples/example_common.cpp` (`buildRecipe`, the displace functions).
- Equivalence runs: the same shape built by the tool and by `dualc_field`, the two OBJ files
  compared with `cmp`. Byte-identical means the same field, the same octree, the same mesh.

| Tool command | `dualc_field` graph | Result |
| --- | --- | --- |
| `dualc_demo --depth 6 torus.obj` | `mesh(path="torus.obj")` | identical (40,032 faces) |
| `dualc_demo --depth 6 --sharp bracket.obj` | `mesh(path="bracket.obj",normals="sharp")` | identical (34,984) |
| `dualc_primitive torus 0 0 0 2 0.5 --onion 0.1 --twist 1.5,1 --depth 6` | `twist(onion(torus(…),thickness=0.1),radiansPerUnit=1.5,axis="y")` | identical (39,680) |
| `dualc_boolean difference sphere.obj cube.obj --depth 6` | `difference(mesh(path="sphere.obj"),mesh(path="cube.obj"))` | identical (41,736) |
| `dualc_csg_demo --recipe=cube-minus-sphere` | `difference(box(min=[-1,-1,-1],max=[1,1,1]),sphere(center=[1,1,1],radius=1.2))` | identical (164,268) |
| `dualc_csg_demo --recipe=smooth-blend` | `smooth_union(sphere(center=[-0.6,0,0],radius=0.8),sphere(center=[0.6,0,0],radius=0.8),k=0.5)` | identical (58,300) |
| `dualc_csg_demo --recipe=displaced-sphere` | `displace(sphere(radius=1),fn="sine",amplitude=0.08,frequency=8)` | identical (135,132) |
| `dualc_lattice cube.obj --wavelength 0.5 --offset 0.05 --depth 6` | `intersection(onion(gyroid(wavelength=0.5),thickness=0.05),mesh(path="cube.obj"))` | identical (69,684) |

The lattice row is identical because `cube.obj` is centred on the origin: `dualc_lattice` centres
the TPMS on the mesh AABB centre, a graph writes `center=[…]` itself.

## Inventory — 13 binaries in `examples/`, 12 numbered tool pages

| # | Binary | Verdict | Reachable from the graph? | What only this tool has |
| --- | --- | --- | --- | --- |
| 1 | `dualc_demo` | **Duplicate**; also the one CLI over the `dualContourMesh` entry point | yes — `mesh(…)`, `winding(…)`, `normals=`, `sign=`, `--no-manifold` | `--gwn` (GWN sign with mesh-triangle crossings): the `mesh` node rejects `sign="gwn"`. Output is OBJ only, see finding F2. |
| 2 | `dualc_primitive` | **Duplicate** | yes — all 30 primitives, the 6 decorator flags, the 6 domain operators, the 3 displace functions are nodes | positional-parameter syntax only (`params=[…]` covers it in the graph) |
| 3 | `dualc_boolean` | **Duplicate, minus one flag** | yes — the 7 booleans with `mesh(…)` children | `--bake N`: the narrow-band SDF-grid bake of each mesh, a speed/quality trade for huge meshes. No node exposes it (`GridField` as an author node is D-11, DEFERRED) |
| 4 | `dualc_lift` | **Unique** | **no** | the 2D profiles `circle`, `box`, `segment`, `polygon` and `revolve` / `extrude` (`include/dualc/implicit2d.h`). D-11 deferred them out of the graph, trigger "a profile-defined solid"; `--list` confirms: no 2D node, no lift node |
| 5 | `dualc_csg_demo` | **Duplicate** | yes — each of the 6 recipes is a one-line graph; `mesh-shell` = `onion(mesh)`, `mesh-minus-sphere` = `difference(mesh,sphere)`, `twisted-mesh` = `twist(mesh)` | nothing; its recipes predate the graph (D-03: the expression parser it stood in for is `--expr`) |
| 6 | `dualc_lattice` | **Duplicate, minus defaults** | yes — `intersection(onion(<tpms>),mesh)`; `--normalize-thickness` = `normalize(…)` | the automatic defaults: wavelength = mesh AABB diagonal / 10, TPMS centred on the AABB, bounds = AABB + 5 % |
| 7 | `dualc_slice` | **Unique output, narrow input** | **no** | the only 2D cross-section export (PNG heatmap + SVG iso-contour on an axis plane, `--plane`, `--res`), but its field is hard-wired to TPMS ∩ mesh: it cannot slice an arbitrary graph |
| 8 | `dualc_view` | retired 2026-10-03 (#50) | — | — |
| 9 | `dualc_gen_demo` | **Infrastructure, keep** | n/a | generates the demo meshes every recipe and test reads; not a modelling tool. Finding F1 |
| 10 | `dualc_raymarch` | **Superseded** by 12 | yes — TPMS ∩ mesh is a graph `dualc_field_view` raymarches | live hotkeys: `t` cycles the TPMS family, `n` toggles normalization, wavelength / offset on dedicated keys (12 nudges any parameter with `tab` + arrows, but cannot swap a node's op) |
| 11 | `dualc_field` | **The framework** (export) | — | — |
| 12 | `dualc_field_view` | **The framework** (preview) | — | — |
| — | `dualc_glsl_parity` | **Infrastructure** — the CPU/GPU gate harness (`check.py --gpu`, #31) | n/a | not a user tool |
| — | `dualc_io_stress` | **Experiment** — the #52 harness (`check.py --io-stress`) | n/a | not a user tool |

## What the two framework CLIs cannot do today

Against `dualc_field`:

1. **2D profiles and lifts** — `revolve`, `extrude` over `circle` / `box` / `segment` / `polygon`.
   Only `dualc_lift`. Deferred by D-11.
2. **Mesh baking for speed** — `dualc_boolean --bake N`. Deferred with the `GridField` author node
   (D-11). The graph's mesh nodes are always exact.
3. **GWN sign with mesh crossings** — `dualc_demo --gwn`. The `mesh` node refuses `sign="gwn"` and
   points to `winding`, which is a different field (the 0.5 iso-surface of the winding number,
   the hole capped), not the same oracle.
4. **Cross-section output** — `dualc_slice`'s PNG + SVG. No graph can be sliced; only the TPMS ∩ mesh
   hard-wired case can.
5. **Defaults derived from an input mesh** — `dualc_lattice`'s wavelength, centre and bounds. A
   convenience, not a capability: the graph states them explicitly.

Against `dualc_field_view`, beyond the above: it does not export (by design, a viewer); mesh sources
are baked to a `--grid-res` texture for the GPU, so a mesh preview approximates what `dualc_field`
exports; it cannot change a node's op live (raymarch's `t`).

Because the C ABI is the graph, items 1–4 are also out of Boletus's reach.

## Findings outside the question

- **F1 — six of the nine generated demo meshes are wound inside-out.** Signed volume of the
  `dualc_gen_demo` output (from `build/examples/`): `sphere` 4.153, `cube` 1.0, `genus2` 7.712 —
  positive; `torus` −2.399, `knot` −12.118, `cylinder` −6.265, `bracket` −0.128, `hexbore` −2.363,
  `uvsphere` −4.159 — negative. The default 3-ray parity oracle ignores orientation, so every test
  and recipe on the default path passes; the orientation-dependent oracles do not:
  `dualc_demo --gwn-field torus.obj` and `knot.obj` give the empty-placeholder triangle (so does
  `winding(path="torus.obj")`, and `--gwn`), and `dualc_demo --pseudonormal bracket.obj` gives
  55,832 faces against 34,984 with parity. Not recorded anywhere in `docs/` on this date.
- **F2 — `dualc_demo` writes OBJ whatever the output extension.** `dualc_demo sphere.obj x.stl` writes
  OBJ text (`# Mesh exported from GeometryCentral`) into `x.stl`; it calls `WavefrontOBJ::write`
  and does not use the `-o` extension dispatch every other tool shares.
- **F3 — `build-gl/` is stale.** Its `dualc_glsl_parity` was linked 2026-10-06 13:42, during #31's
  deliberate `opXor` break (`239000a`); the source was reverted at 13:45 (`a25a94c`). Run today it
  reports `xor` FAIL, 72/73; the source is correct and CI's `gpu` job builds fresh. Rebuilding
  `build-gl` clears it.
