# A TPMS lattice inside a mesh

Part of [Tool 11: `dualc_field`](README.md). Fill a closed mesh with a triply-periodic
minimal-surface (TPMS) lattice and contour the result as one closed solid — the graph form of
[`dualc_lattice`](../06-dualc_lattice.md). The tool composed one fixed graph and computed three
defaults from the mesh; written out, the same graph and the same numbers write the same bytes
([roadmap 21/07](../../roadmap/21-cli-consolidation/07-docs-migration.md)). Previewing it live:
[12/05](../12-dualc_field_view/05-tpms-in-mesh-preview.md).

## The graph and its defaults

```
intersection(onion(normalize(TPMS(center=C,wavelength=W)),thickness=T),mesh(path="part.obj"))
```

| `dualc_lattice` | In the graph | The tool's default, to write in by hand |
| --- | --- | --- |
| `<input.obj>` | the `mesh(path=…)` operand — the **second** child | (required) |
| `--type NAME` | the TPMS op: `gyroid`, `schwarz-p`, `diamond`, `fischer-koch`, `lidinoid`, `neovius` (in `--expr`, `_` for `-`) | `gyroid` |
| `--wavelength W` | `wavelength=W` | the mesh AABB **diagonal / 10** (about 10 cells across); the node's own default is `1` |
| *(the lattice phase)* | `center=C` | the mesh **AABB centre**; the node's own default is the origin |
| `--offset T` | the `onion(…, thickness=T)` wrapper: the double-sheet wall `\|F\| − T` | `0` — no `onion`: the solid phase `F ≤ 0` |
| `--normalize-thickness` | the `normalize(…)` wrapper, directly round the TPMS (inside any `onion`) | off |
| `--bounds …` | `--bounds …` — the intersection clips to the mesh, so the box need only contain it | the mesh **AABB + 5 % per axis**; `dualc_field`'s auto-fit pads 5 % of the *largest* extent, which is the same box only on a cube |
| `-o PATH` | `-o PATH` — `.obj` / `.stl` / `.3mf` by extension | `<type>_lattice.obj`; here `field.obj` |
| `--depth N` · `--collapse E` | the same options | `7` · `0` (off) |

`--offset` had to be ≥ 0; in the graph a negative `thickness` is simply not written. To
**isolate the open lattice surface** without the volume's skin, or add a skin and further
booleans in the same contour, see the [workflow](04-workflow-open-surface.md#workflow-isolate--thicken--skin--union-a-tpms-lattice).

## Wavelength vs. offset

`wavelength` (λ) and `thickness` (T) are **orthogonal**: one cannot be reproduced by tuning the
other. λ is the *period*, the side of one unit cell: a bigger λ rescales the same surface into
fewer, larger cells at the same volume fraction (~50 % for the bare lattice). T changes the
*topology*: the bare lattice contours `F = 0`, the shell `|F| − T = 0` — the level sets
`F = ±T` glued into a tube, thin-walled for small T, a solid block once adjacent walls meet.

| Mode | Field | 0-isosurface | Volume fraction |
| --- | --- | --- | --- |
| Bare (no `onion`) | `F` | `F = 0` | ~50 % |
| Shell | `\|F\| − T` | `F = +T` and `F = −T` | ~`2·T / λ` for small `T/λ` |
| Shell, T → ∞ | `\|F\| − T` | everything | → 100 % |

`2·T / λ` holds when T is a metric distance — wrap the TPMS in `normalize`, which divides by
`max(|grad F|, ε)`, so T is close to the true wall thickness (the wall is ~`2·T`); without it T is
in raw trig units and the wall varies by up to ~30 % across a cell. Tune λ for the cell count
across the part, T for the wall (printability, stiffness, flow): for a part in mm, λ = the cell
pitch (5–10 mm), T = the thinnest manufacturable wall (1 mm FDM, 0.4 mm SLA). The single-sheet
shift `F − r` is a third, separate knob: `offset(gyroid(…),r=…)`. Why λ alone cannot unlock the
volume fraction: [roadmap 05 § 16](../../roadmap/05-tpms-lattices/README.md#16-tpms-lattices-on-input-meshes).

## The six TPMS

With `kx = 2π(x − cx) / λ` and so on (`c` = the `center`). Formulas after Schoen (1970, NASA
TN D-5541) and Gandy et al. (2001), conventions cross-checked against PicoGK (Apache-2.0,
LEAP 71 — reference only, no code copied).

| # | op | Field `F(x, y, z)` |
| --- | --- | --- |
| Y1 | `gyroid` | `sin(kx)·cos(ky) + sin(ky)·cos(kz) + sin(kz)·cos(kx)` |
| Y2 | `schwarz-p` | `cos(kx) + cos(ky) + cos(kz)` |
| Y3 | `diamond` | `sin(kx)·sin(ky)·sin(kz) + sin(kx)·cos(ky)·cos(kz) + cos(kx)·sin(ky)·cos(kz) + cos(kx)·cos(ky)·sin(kz)` |
| Y4 | `fischer-koch` | `cos(2kx)·sin(ky)·cos(kz) + cos(kx)·cos(2ky)·sin(kz) + sin(kx)·cos(ky)·cos(2kz)` |
| Y5 | `lidinoid` | `½·[sin(2kx)·cos(ky)·sin(kz) + sin(2ky)·cos(kz)·sin(kx) + sin(2kz)·cos(kx)·sin(ky)] − ½·[cos(2kx)·cos(2ky) + cos(2ky)·cos(2kz) + cos(2kz)·cos(2kx)] + 0.15` |
| Y6 | `neovius` | `3·(cos(kx) + cos(ky) + cos(kz)) + 4·cos(kx)·cos(ky)·cos(kz)` |

The `+0.15` shifts the Lidinoid's 0-isosurface to the Lidinoid proper (Schoen / Lidin's
convention, matched by PicoGK). Gyroid is Schoen's G, Schwarz-P the primitive cubic,
diamond Schwarz D, Fischer-Koch its S surface, Neovius of Im-3m symmetry.

## Recipes

`cube.obj` spans [−0.5, 0.5]³: its AABB centre is the origin (the node's default `center`), its
diagonal / 10 is `0.17320508075688773`, and its padded box is what the auto-fit gives — so on
the cube no `center` and no `--bounds` are needed. Y1–Y6 are each type at every default; for
another type swap the op of Y1.

| # | What it builds |
| --- | --- |
| Y1 | `dualc_lattice cube.obj` — the solid gyroid phase clipped to the cube (Y2–Y6: `schwarz_p` … `neovius`) |
| T7 | The solid phase of a 4-cells-across gyroid |
| T8 | The double-sheet shell `\|F\| − 0.1` clipped to the cube |
| T9 | A Schwarz-P shell with T ≈ the metric wall, normalised |
| T10 | Real-mesh infill: a gyroid shell in `molde.obj`, 8-unit cells, 1-unit walls |
| T11 | A Neovius solid at depth 8 with a mild collapse |
| T12, T13 | T8 as binary STL and as 3MF (`unit="millimeter"`) |
| T14 | Any mesh, the tool's three defaults computed from the file — byte-identical to `dualc_lattice` |

```bash
dualc_field --expr 'intersection(gyroid(wavelength=0.17320508075688773),mesh(path="cube.obj"))' -o gyroid_lattice.obj   # Y1
dualc_field --expr 'intersection(gyroid(wavelength=0.5),mesh(path="cube.obj"))' -o t7.obj                               # T7
dualc_field --expr 'intersection(onion(gyroid(wavelength=0.5),thickness=0.1),mesh(path="cube.obj"))' -o t8.obj           # T8
dualc_field --expr 'intersection(onion(normalize(schwarz_p(wavelength=0.4)),thickness=0.05),mesh(path="cube.obj"))' -o t9.obj   # T9
dualc_field --expr 'intersection(onion(gyroid(center=[-85.13,-27.0,242.97],wavelength=8),thickness=1),mesh(path="data/molde.obj"))' --depth 8 -o gyroid_molde.obj   # T10
dualc_field --expr 'intersection(neovius(wavelength=0.17320508075688773),mesh(path="cube.obj"))' --depth 8 --collapse 1 -o t11.obj   # T11
dualc_field --expr 'intersection(onion(gyroid(wavelength=0.5),thickness=0.1),mesh(path="cube.obj"))' -o part.stl         # T12
dualc_field --expr 'intersection(onion(gyroid(wavelength=0.5),thickness=0.1),mesh(path="cube.obj"))' -o part.3mf         # T13

# T14 — C (AABB centre), W (diagonal / 10) and B (AABB + 5 % per axis) from an OBJ, in bash:
eval "$(awk '/^v /{for(i=1;i<=3;i++){x=$(i+1)+0;if(!(i in lo)||x<lo[i])lo[i]=x;if(!(i in hi)||x>hi[i])hi[i]=x}}
  END{for(i=1;i<=3;i++){e[i]=hi[i]-lo[i];c[i]=(lo[i]+hi[i])/2}
  printf "C=[%.17g,%.17g,%.17g] W=%.17g B=%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",c[1],c[2],c[3],
  sqrt(e[1]^2+e[2]^2+e[3]^2)/10,lo[1]-e[1]*0.05,lo[2]-e[2]*0.05,lo[3]-e[3]*0.05,
  hi[1]+e[1]*0.05,hi[2]+e[2]*0.05,hi[3]+e[3]*0.05}' knot.obj)"
dualc_field --expr "intersection(diamond(center=$C,wavelength=$W),mesh(path=\"knot.obj\"))" --bounds $B --depth 6 -o knot_lattice.obj
```

T10 rounds molde's AABB centre and leaves the box to the auto-fit (5 % of the largest
extent), so its mesh is close to the tool's but not the same bytes; T14's form reproduces the
tool exactly. The 3MF's millimetre and the
formats: [export formats](../README.md#export-formats--o-extension-dispatch).

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
