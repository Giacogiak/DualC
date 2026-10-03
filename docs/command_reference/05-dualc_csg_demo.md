# Tool 5: `dualc_csg_demo` (the 6 baked recipes)

Baked field-tree composition recipes — hand-built trees that showcase
composition, mesh+primitive booleans, and the `displaced` operator (whose
function argument has no plain CLI form).

```
dualc_csg_demo --recipe=NAME [input.obj] [options]
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `--recipe=NAME` / `--recipe NAME` | (required) | One of the six recipes (R1–R6). |
| `input.obj` | — | Positional; required by the three mesh recipes. |
| `-o PATH` | `<recipe>.obj` | Output; `.obj` / `.stl` / `.3mf` by extension ([export formats](README.md#export-formats--o-extension-dispatch)). |
| `--depth N` | `7` | Octree max depth. |
| `--collapse E` | `0` (off) | Adaptive cell-collapse threshold. |
| `--list` | — | Print all recipes, then exit. |

## Recipes

| # | Recipe | Default command | Needs a mesh? | What it builds |
| --- | --- | --- | --- | --- |
| R1 | `cube-minus-sphere` | `dualc_csg_demo --recipe=cube-minus-sphere` | no | Hard difference of a box and a sphere. |
| R2 | `smooth-blend` | `dualc_csg_demo --recipe=smooth-blend` | no | Smooth union of two spheres (visible fillet). |
| R3 | `displaced-sphere` | `dualc_csg_demo --recipe=displaced-sphere` | no | A sphere perturbed by a sinusoidal bump. |
| R4 | `mesh-shell` | `dualc_csg_demo --recipe=mesh-shell cube.obj` | yes | `onionOf(mesh)` — hollow shell of the input. |
| R5 | `mesh-minus-sphere` | `dualc_csg_demo --recipe=mesh-minus-sphere cube.obj` | yes | `differenceOf(mesh, sphere)` — carve the input. |
| R6 | `twisted-mesh` | `dualc_csg_demo --recipe=twisted-mesh cube.obj` | yes | `twisted(mesh)` — a domain warp on the input. |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
