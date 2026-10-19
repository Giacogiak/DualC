# Mesh in → mesh out: re-meshing, sign oracles, sealing soup

Part of [Tool 11: `dualc_field`](README.md). A graph whose root is one `mesh` node re-meshes
that mesh through the dual contourer; the same node with another `sign`, or the `winding`
source in its place, decides how a clean, almost-clean or open input is read. It replaced
[`dualc_demo`](../01-dualc_demo.md), retired 2026-10-16: every one of its flags is a node parameter
or one of `dualc_field`'s own options, and the same input wrote the same bytes
([roadmap 21/07](../../roadmap/21-cli-consolidation/07-docs-migration.md)).

## The input

- **A triangle mesh file**, named by `path` — resolved from the current directory, not the
  binary's (the demo meshes sit next to it; `molde`, `foot`, `bunny`, `mesh-soup` are in the
  repo's `data/`, [09](../09-dualc_gen_demo.md)). The path goes to geometry-central's
  `readSurfaceMesh`; every recipe here reads an OBJ.
- **In one world frame**, modelled in the unit you export in (1 unit = 1 mm in a 3MF,
  [design/09 § Units](../../design/09-conventions.md#units-and-frames)): no transform is
  applied on the way in.
- **Wound outward**, if the sign is not `parity` (below). Every `dualc_gen_demo` mesh is.
- The output is any of `.obj` / `.stl` / `.3mf` by `-o`'s extension
  ([export formats](../README.md#export-formats--o-extension-dispatch)); `dualc_demo` wrote OBJ
  only.

| `dualc_demo` | `dualc_field` | Default |
| --- | --- | --- |
| `input.obj` (positional, default `cube.obj`) | `mesh(path="input.obj")` — `path` is required | — |
| `output.obj` (positional, default `cube_dc.obj`) | `-o PATH` | `field.obj` |
| `--depth N` · `--collapse E` · `--no-manifold` | the same options | `7` · `0` (off) · off |
| `--sharp` | `normals=sharp` on the node | `smooth` |
| `--pseudonormal` · `--gwn` | `sign=pseudonormal` · `sign=gwn` on the node | `parity` |
| `--gwn-field` | the `winding(path=…)` source instead of `mesh` | — |

`dualc_demo`'s three sign flags overrode one another (`--gwn-field` over `--gwn` over
`--pseudonormal`); in the graph each is its own spelling, so there is nothing to override.

## Choosing a sign oracle

The *sign oracle* is the inside/outside test run at every octree corner. The edge-crossing
**positions and normals** always come from the mesh triangles; the oracle only decides the
sign — except `winding`, which derives *everything* from the winding-number scalar. Picking
one is about how *clean* the input is; the algorithms and their failure modes are
[design/02 § Sign determination](../../design/02-sign-oracles.md).

| Your input | Use | Why |
| --- | --- | --- |
| Closed and consistently oriented | `mesh(path=…)` (`sign=parity`, 3 rays), or `sign=pseudonormal` for speed | both need watertight input, the pseudonormal an outward winding too; it is one closest-point query instead of three rays |
| Almost watertight — small gaps, self-intersections, non-manifold bits | `mesh(path=…,sign=gwn)` | robust where parity coin-flips; the crossings still come from the mesh, so a real hole stays a hole |
| Genuinely open — open shells, soup, holes you want sealed | `winding(path=…)` | contours the 0.5-isosurface of the winding number itself: a hole gets a smooth cap, disconnected pieces are summed, and the output is one watertight solid |

Only parity ignores the winding. On a mesh wound inside out, `sign=gwn` and `winding` find no
inside (the output is the placeholder triangle) and `sign=pseudonormal` returns the solid
turned inside out; a mixed winding gives a wrong surface.

## Sharp vs. smooth normals (`normals=sharp`)

`normals` picks the normal recorded at each Hermite edge-crossing: by default a
**barycentric blend of the mesh's per-vertex normals** (smooth shading); with `sharp`, the
hit triangle's geometric **face normal**. The recorded normals steer QEF vertex placement, so
this changes the output *geometry*, not its topology (the same triangle count).

Use `sharp` for **CAD-style, hard-edged input**: on the cube its corner vertices land at
exactly `±0.5` instead of being rounded slightly inward. **Leave it off for organic meshes** —
they come out faceted at the cell scale. It is orthogonal to `sign` and combines with any of
them. `--no-manifold` is the pre-Manifold-DC reference: one QEF vertex per cell instead of
one per surface component.

## Recipes

| # | Was | What it does |
| --- | --- | --- |
| G1 | `dualc_demo` (D1, D2) | Re-mesh `cube.obj` with all defaults |
| G3 | D3 `--sharp` | Face normals — the cube's 90° corners stay crisp |
| G4 | D4 `--collapse 1` | Adaptive cell collapse on `molde.obj` |
| G5 | D5 | High depth with aggressive collapse — a bounded file size |
| G6 | D6 `--no-manifold` | One QEF vertex per cell, the pre-MDC reference |
| G7 | D7 `--pseudonormal` | The fast oracle — watertight, outward-wound input only |
| G8 | D8 `--gwn` | The winding-number oracle, robust where parity coin-flips |
| G9 | D9, D10 `--gwn-field` | **The soup recipe**: seal a multi-object soup into one watertight solid, as STL |
| H1, H2 | `dualc_boolean` (kept, D-60) | Booleans of meshes, [below](#booleans-of-meshes) |

```bash
# G1 — re-mesh the cube (dualc_demo's cube_dc.obj):
dualc_field --expr 'mesh(path="cube.obj")' -o cube_dc.obj
# G3 — keep the corners square:
dualc_field --expr 'mesh(path="cube.obj",normals=sharp)' -o cube_sharp.obj
# G4, G5 — adaptive collapse; depth 9 with a strong collapse keeps the file bounded:
dualc_field --expr 'mesh(path="data/molde.obj")' --collapse 1 -o molde_dc.obj
dualc_field --expr 'mesh(path="data/molde.obj")' --depth 9 --collapse 100 -o molde_d9.obj
# G6 — the pre-Manifold-DC reference:
dualc_field --expr 'mesh(path="cube.obj")' --no-manifold -o cube_pre_mdc.obj
# G7, G8 — the other two oracles:
dualc_field --expr 'mesh(path="cube.obj",sign=pseudonormal)' -o cube_pn.obj
dualc_field --expr 'mesh(path="data/molde.obj",sign=gwn)' -o molde_gwn.obj
# G9 — the soup recipe: open shells, soup, self-intersections → one watertight solid:
dualc_field --expr 'winding(path="data/mesh-soup.obj")' -o sealed.stl
```

G2 and D10 of the old page were G1 and G9 with the paths spelled out; there is no G2 or G10.
The outer single quotes are bash's; on Windows PowerShell 5.1 escape each inner quote
([11/01 § `--expr` and shell quoting](01-op-vocabulary.md#--expr-and-shell-quoting)), or put
the graph in a JSON file.

## Booleans of meshes

Two `mesh` nodes under a boolean ([11/01 § Booleans](01-op-vocabulary.md#booleans-two-children))
are what [`dualc_boolean`](../03-dualc_boolean.md) computes, the same bytes for the same
operands: `dualc_boolean smooth-union a.obj b.obj --k K` is
`smooth-union(mesh(path="a.obj"),mesh(path="b.obj"),k=K)`, and its `--sharp` is
`normals=sharp` on **both** nodes. Size `k` to the model, not to a fixed small number: a fillet
narrower than one cell (≈ the model's extent / `2^depth`) is invisible — if a `smooth-union`
looks like a `union`, raise `k` or `--depth`. `dualc_boolean` warns when `k` is under 1 % of
the extent; the graph does not. For a large mesh, `bake=N` on a node trades exactness for speed
([11/01 § Sources](01-op-vocabulary.md#sources)).

```bash
# H1 — a filleted union of two meshes (dualc_boolean smooth-union torus.obj cube.obj --k 0.25):
dualc_field --expr 'smooth-union(mesh(path="torus.obj"),mesh(path="cube.obj"),k=0.25)' -o su.obj
# H2 — a square-seamed carve (dualc_boolean difference genus2.obj sphere.obj --sharp):
dualc_field --expr 'difference(mesh(path="genus2.obj",normals=sharp),mesh(path="sphere.obj",normals=sharp))' -o carved.obj
```

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
