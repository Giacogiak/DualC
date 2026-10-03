# Tool 4: `dualc_lift` (2D primitives + the 2 lifts)

Revolve or extrude a 2D profile into 3D.

```
dualc_lift revolve <profile> [params...] --offset O [options]
dualc_lift extrude <profile> [params...] --height H [options]
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `revolve` \| `extrude` | (required) | The lift. |
| `<2d-primitive> [params...]` | (required) | `circle`, `box`, `segment` or `polygon` (points as `x,y` pairs). |
| `--offset O` | `1` | Revolve only: profile `x` maps to `radius − O` (the ring radius — distance from the profile to the axis). |
| `--height H` | `0.5` | Extrude only: half-height along `z`. |
| `-o PATH` | `revolved.obj` / `extruded.obj` | Output; `.obj` / `.stl` / `.3mf` by extension ([export formats](README.md#export-formats--o-extension-dispatch)). |
| `--depth N` | `7` | Octree max depth. |
| `--collapse E` | `0` (off) | Adaptive cell-collapse threshold. |

## The 4 2D primitives (profiles)

| # | 2D Primitive | Parameters (defaults) |
| --- | --- | --- |
| Q1 | `circle` | cx cy radius = 0 0 0.5 |
| Q2 | `box` | cx cy halfx halfy = 0 0 1 0.5 |
| Q3 | `segment` | ax ay bx by radius = -1 0 1 0 0.3 |
| Q4 | `polygon` | x1 y1 x2 y2 ... = unit triangle (≥ 3 vertices, even-length list) |

## Recipes

### Lift 1 — `revolve` (the 4 profiles, one by one)

| # | Command | What it builds |
| --- | --- | --- |
| L1 | `dualc_lift revolve circle --offset 2` | Revolved circle → torus. |
| L2 | `dualc_lift revolve box --offset 2` | Revolved box ring. |
| L3 | `dualc_lift revolve segment --offset 2` | Revolved capsule profile. |
| L4 | `dualc_lift revolve polygon 0,0 1,0 1,1 0,1 --offset 2` | Revolved polygon. |

### Lift 2 — `extrude` (the 4 profiles, one by one)

| # | Command | What it builds |
| --- | --- | --- |
| L5 | `dualc_lift extrude circle --height 0.5` | Extruded circle → cylinder. |
| L6 | `dualc_lift extrude box --height 0.5` | Extruded box → box. |
| L7 | `dualc_lift extrude segment --height 0.5` | Extruded capsule slab. |
| L8 | `dualc_lift extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` | Extruded polygon prism. |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
