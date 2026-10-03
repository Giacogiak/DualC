# Tool 6: `dualc_lattice` (TPMS infill bounded by a mesh)

TPMS lattice infill bounded by an input mesh.

```
dualc_lattice <input.obj> [--type NAME] [--wavelength W] [--offset T]
              [--normalize-thickness] [--depth N] [--collapse E]
              [--bounds x0,y0,z0,x1,y1,z1] [-o output.obj|.stl|.3mf]
```

Takes an input mesh defining a closed volume, fills it with a periodic
triply-periodic-minimal-surface (TPMS) lattice of the requested type, and
dual-contours the result as a closed manifold solid. `--offset 0` (default)
emits the solid phase of the lattice clipped to the mesh; `--offset T > 0`
emits the closed double-sheet thick-wall shell `|F|−T` clipped to the mesh.
`--normalize-thickness` first-order SDF-normalises the field so `T` is
metrically close to the true wall thickness (raw TPMS values are
trigonometric, not metric distances).

| Flag | Default | Meaning |
| --- | --- | --- |
| `<input.obj>` | (required) | Closed mesh whose interior clips the lattice. |
| `--type NAME` | `gyroid` | TPMS family: `gyroid`, `schwarz-p`, `diamond`, `fischer-koch`, `lidinoid`, `neovius` (T1–T6). |
| `--wavelength W` | mesh AABB diagonal / 10 | Unit-cell side, world units (the default gives ~10 cells across). |
| `--offset T` | `0` | `0`: the solid phase; `T > 0`: the closed double-sheet shell `\|F\|−T`. Must be `≥ 0` (`[dualc_lattice] --offset must be >= 0`). |
| `--normalize-thickness` | off | Normalise the TPMS by `1/\|grad F\|` before the offset, so `T` is close to the metric wall thickness. |
| `--depth N` | `7` | Octree max depth. |
| `--collapse E` | `0` (off) | Adaptive cell-collapse threshold — [common options](README.md#common-options--which-tool-takes-which). |
| `--bounds x0,y0,z0,x1,y1,z1` | input AABB + 5 % | Explicit sampling region; the intersection clips to the mesh, so the box only needs to contain it. |
| `-o PATH` | `<type>_lattice.obj` | Output; `.obj` / `.stl` / `.3mf` by extension ([export formats](README.md#export-formats--o-extension-dispatch)). |

> **Composing beyond a single clipped solid.** `dualc_lattice` always emits one
> closed solid (lattice clipped to the mesh). To **isolate and inspect the open
> lattice surface alone** (no welded volume skin), or to compose the lattice with
> a thickened **skin** and further booleans as one field-level contour, use the
> general [`dualc_field`](11-dualc_field/README.md) workflow — see
> [isolate → thicken → skin → union a TPMS lattice](11-dualc_field/04-workflow-open-surface.md#workflow-isolate--thicken--skin--union-a-tpms-lattice)
> and the meshless preview in
> [`dualc_field_view`](12-dualc_field_view/04-open-surface-preview.md#isolating-a-tpms-surface-inside-a-volume-meshless-preview).

## Wavelength vs. offset — conceptual guide

`--wavelength` and `--offset` are **orthogonal knobs**: one cannot be reproduced
by tuning the other. With **λ ≡ `--wavelength`** and **T ≡ `--offset`**, λ is the
*period* — the side of one unit cell; a bigger λ rescales the same surface into
fewer, larger cells at the same volume fraction (~50 % for the bare lattice, which
is the boundary of one phase of the medial surface). T changes the *topology*: the
bare lattice contours `F = 0`, the offset version `|F| − T = 0`, the pair of level
sets `F = ±T` glued into a tube — thin-walled and wireframe-like for small T,
back to a solid block once adjacent walls meet.

| Mode | Field | 0-isosurface | Volume fraction |
| --- | --- | --- | --- |
| Bare (T = 0)            | `F`         | `F = 0`                 | ~50% |
| Offset (T > 0)          | `\|F\| − T` | `F = +T` and `F = -T`   | ~`2·T / λ` for small `T/λ` (thin-wall regime) |
| Offset (T → ∞)          | `\|F\| − T` | covers everything       | → 100% (solid block) |

The `2·T / λ` estimate holds when T is a metric distance — use
`--normalize-thickness`, which replaces the raw trig field by
`F / max(|grad F|, ε)` so T becomes ~true wall thickness in millimetres; without
it, T is in raw trig units and the wall varies by up to ~30 % across the cell.

**When to reach for each.** Tune **λ** for the cell count across the part
(mechanical / aesthetic feel); tune **T** for the wall thickness, which dictates
printability, stiffness and flow. For engineering parts set both: λ = the cell
pitch (say 5–10 mm), T = the minimum manufacturable wall (1 mm for FDM, 0.4 mm for
SLA), and `2·T / λ` is the volume fraction. The single-sheet Quilez shift `F − r`
(a third axis, not a double wall) is not wired into this CLI; the field graph
reaches it as `offset(gyroid(wavelength=…),r=…)`
([11-dualc_field/](11-dualc_field/README.md)). Why λ alone cannot unlock the volume
fraction, and how the CLI composes the field (`normalizedOf` → `onionOf` →
`intersectionOf`): [roadmap 05 § 16](../roadmap/05-tpms-lattices/README.md#16-tpms-lattices-on-input-meshes).

## TPMS formulas

With `kx = 2π(x − cx) / λ`, etc. (the `c` shift comes from the lattice
being re-centred on the input-mesh AABB centre; pass your own via the C++
constructor if you need a specific phase). Formulas follow Schoen (1970,
NASA TN D-5541) and Gandy et al. (2001); conventions cross-checked against
PicoGK (Apache-2.0, LEAP 71 — reference only, no code copied).

| `--type` | Field `F(x, y, z)` |
| --- | --- |
| `gyroid`       | `sin(kx)·cos(ky) + sin(ky)·cos(kz) + sin(kz)·cos(kx)` |
| `schwarz-p`    | `cos(kx) + cos(ky) + cos(kz)` |
| `diamond`      | `sin(kx)·sin(ky)·sin(kz) + sin(kx)·cos(ky)·cos(kz) + cos(kx)·sin(ky)·cos(kz) + cos(kx)·cos(ky)·sin(kz)` |
| `fischer-koch` | `cos(2kx)·sin(ky)·cos(kz) + cos(kx)·cos(2ky)·sin(kz) + sin(kx)·cos(ky)·cos(2kz)` |
| `lidinoid`     | `½·[sin(2kx)·cos(ky)·sin(kz) + sin(2ky)·cos(kz)·sin(kx) + sin(2kz)·cos(kx)·sin(ky)] − ½·[cos(2kx)·cos(2ky) + cos(2ky)·cos(2kz) + cos(2kz)·cos(2kx)] + 0.15` |
| `neovius`      | `3·(cos(kx) + cos(ky) + cos(kz)) + 4·cos(kx)·cos(ky)·cos(kz)` |

The `+0.15` constant in the Lidinoid formula shifts the 0-isosurface to
the Lidinoid proper (the minimal-surface convention used by Schoen / Lidin
and matched by PicoGK).

## The 6 TPMS types — one by one

The `--type` values, Y1–Y6 (the recipes keep their T numbers):

| # | Type | Default command | What it builds |
| --- | --- | --- | --- |
| Y1 | `gyroid` | `dualc_lattice cube.obj` | Schoen G lattice (default) clipped to the cube. |
| Y2 | `schwarz-p` | `dualc_lattice cube.obj --type schwarz-p` | Schwarz P (primitive cubic) lattice clipped to the cube. |
| Y3 | `diamond` | `dualc_lattice cube.obj --type diamond` | Schwarz D (Diamond) lattice clipped to the cube. |
| Y4 | `fischer-koch` | `dualc_lattice cube.obj --type fischer-koch` | Fischer-Koch S lattice clipped to the cube. |
| Y5 | `lidinoid` | `dualc_lattice cube.obj --type lidinoid` | Lidinoid (Lidin's surface) lattice clipped to the cube. |
| Y6 | `neovius` | `dualc_lattice cube.obj --type neovius` | Neovius (Im-3m symmetry) lattice clipped to the cube. |

## Recipes

Output-mode recipes:

| # | Command | What it builds |
| --- | --- | --- |
| T7  | `dualc_lattice cube.obj --type gyroid --wavelength 0.5` | Solid phase of a 4-cells-across gyroid lattice. |
| T8  | `dualc_lattice cube.obj --type gyroid --wavelength 0.5 --offset 0.1` | Thick-wall double-sheet shell of width `\|F\|−0.1` clipped to the cube. |
| T9  | `dualc_lattice cube.obj --type schwarz-p --wavelength 0.4 --offset 0.05 --normalize-thickness` | Schwarz-P shell with `T` ≈ metric wall thickness via first-order SDF normalisation. |
| T10 | `dualc_lattice molde.obj --type gyroid --wavelength 8 --offset 1 --depth 8 -o gyroid_molde.obj` | Real-mesh infill: gyroid solid lattice inside `molde.obj` with 8 cells across, 1 mm walls. |
| T11 | `dualc_lattice cube.obj --type neovius --depth 8 --collapse 1` | High-resolution Neovius solid lattice with mild adaptive collapse for smaller output. |
| T12 | `dualc_lattice cube.obj --type gyroid --wavelength 0.5 --offset 0.1 -o part.stl` | Same shell, written as a binary STL — printable directly. |
| T13 | `dualc_lattice cube.obj --type gyroid --wavelength 0.5 --offset 0.1 -o part.3mf` | Same shell as 3MF-mesh (`unit="millimeter"`; [export formats](README.md#export-formats--o-extension-dispatch)). |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
