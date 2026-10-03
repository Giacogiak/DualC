# Tool 1: `dualc_demo` (mesh in → mesh out)

Re-mesh a triangle mesh through the dual contourer.

```
dualc_demo [options] [input.obj] [output.obj]
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `input.obj` | `cube.obj` | First positional: the mesh to re-contour (OBJ). |
| `output.obj` | `cube_dc.obj` | Second positional: OBJ output (this tool writes OBJ only). |
| `--depth N` / `--depth=N` | `7` | Octree max depth. |
| `--collapse E` / `--collapse=E` | `0` (off) | Adaptive cell-collapse threshold (QEF energy, length²). |
| `--sharp` | off | Face normals instead of smooth vertex normals — keeps corners crisp. |
| `--no-manifold` | off | One QEF vertex per cell (pre-Manifold-DC reference). |
| `--pseudonormal` | off | Pseudonormal sign oracle ([choosing one](#choosing-a-sign-oracle)). |
| `--gwn` | off | Generalized-winding-number sign oracle ([choosing one](#choosing-a-sign-oracle)). |
| `--gwn-field` | off | Contour the GWN 0.5-isosurface; overrides `--pseudonormal` and `--gwn`. |

## Recipes

| # | Command | What it does |
| --- | --- | --- |
| D1 | `dualc_demo` | Re-mesh `cube.obj` → `cube_dc.obj` with all defaults. |
| D2 | `dualc_demo --depth 7 cube.obj cube_dc.obj` | Explicit depth + paths. |
| D3 | `dualc_demo --sharp cube.obj cube_sharp.obj` | Face normals — keeps 90° corners crisp. |
| D4 | `dualc_demo --collapse 1 molde.obj molde_dc.obj` | Adaptive cell collapse. |
| D5 | `dualc_demo --depth 9 --collapse 100 molde.obj molde_d9.obj` | High depth + aggressive collapse; bounded file size. |
| D6 | `dualc_demo --no-manifold cube.obj cube_pre_mdc.obj` | Disable manifold DC (one QEF vertex per cell — pre-MDC reference). |
| D7 | `dualc_demo --pseudonormal cube.obj cube_pn.obj` | Pseudonormal sign oracle (fast; watertight + consistently oriented input only). |
| D8 | `dualc_demo --gwn molde.obj molde_gwn.obj` | Generalized-winding-number sign oracle (robust where 3-ray parity coin-flips; edge crossings still from the mesh). |
| D9 | `dualc_demo --gwn-field open_shell.obj sealed.obj` | Contour the GWN 0.5-isosurface — seals open shells / soup / self-intersecting input into a single watertight solid. |
| D10 | `dualc_demo --gwn-field --depth 7 scene.obj sealed.obj` | Same, on a multi-object OBJ — the disconnected components are summed by GWN; output is one watertight solid (the multi-mesh "soup" workflow). |

The three sign-oracle flags
(`--pseudonormal`, `--gwn`, `--gwn-field`) are mutually exclusive in
effect: `--gwn-field` overrides both, and `--gwn` overrides
`--pseudonormal`. Default is the 3-ray parity oracle (watertight input).

## Choosing a sign oracle

The *sign oracle* is the inside/outside test the sampler runs at every octree
corner. The edge-crossing **positions and normals** always come from the mesh
triangles; the oracle only decides the sign — except `--gwn-field`, which derives
*everything* from the winding-number scalar. Picking one is about how *clean* the
input is; the algorithms and their failure modes are
[design/02 § Sign determination](../design/02-sign-oracles.md).

| Your input | Use | Why |
| --- | --- | --- |
| Closed & consistently oriented | default (3-ray parity), or `--pseudonormal` for speed | both require watertight, oriented input; the pseudonormal is one closest-point query instead of three rays |
| Almost watertight — small gaps, self-intersections, non-manifold bits | `--gwn` | robust where parity coin-flips; edge crossings still come from the mesh, so a real hole stays a hole |
| Genuinely open — open shells, soup, holes you want sealed | `--gwn-field` | contours the 0.5-isosurface of the winding number itself: the hole gets a smooth cap and the output is one watertight solid (D9 / D10) |

## Sharp vs. smooth normals (`--sharp`)

`--sharp` toggles the normal recorded at each Hermite edge-crossing. By default
the sampler records a **barycentric blend of the mesh's per-vertex normals**
(smooth shading); with `--sharp` it records the hit triangle's geometric **face
normal** instead. Because the recorded normals steer QEF vertex placement, this
changes the output *geometry*, not its topology (the triangle count is the same).

Use `--sharp` for **CAD-style, hard-edged input**: on the cube its corner
vertices land at exactly `±0.5` instead of being rounded slightly inward by the
smooth-normal QEF. **Leave it off for organic meshes** — with `--sharp` they come
out faceted at the cell scale, which is rarely what you want. The flag is
orthogonal to the sign-oracle choice and combines with any of them.

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
