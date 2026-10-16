# Slicing a graph (`--slice`)

Part of [Tool 11: `dualc_field`](README.md). `--slice AXIS=VALUE` samples the built field on
an axis-aligned plane instead of contouring it, and writes what the plane shows: a **PNG
heatmap** and an **SVG of the zero-contour**. Cost is `O(res²)`, independent of how dense the
field is, so it inspects parts far too dense to mesh. Any graph slices — a lattice, a mesh
source, a boolean, a lift. It replaced [`dualc_slice`](../07-dualc_slice.md), retired 2026-10-16: the
same core (`examples/field_slice.{h,cpp}`), and the same field wrote the same bytes
([roadmap 21/06](../../roadmap/21-cli-consolidation/06-slice-any-graph.md)).

## Synopsis

```
dualc_field <graph> --slice AXIS=VALUE [--res N] [--bounds x0,y0,z0,x1,y1,z1] [-o BASE]
dualc_field --expr "<shorthand>" --slice AXIS=VALUE [...]
```

The graph comes in any of the usual forms (file, `--expr`, stdin `-`); it is built, never
contoured.

## Options

| Option | Default | Meaning |
| --- | --- | --- |
| `--slice AXIS=VALUE` | (required) | The cutting plane, e.g. `z=0`, `x=1.5`; AXIS ∈ `x` / `y` / `z` (either case), VALUE an **absolute world coordinate**. |
| `--res N` | `512` | Grid resolution per side; clamped to `2 … 4096`. Only with `--slice` — alone it is refused. |
| `--bounds x0,y0,z0,x1,y1,z1` | the graph's bounds + 5 % | The sampling region: the plane's footprint in it is sampled. Without it, the graph's bounds padded by 5 % of their **largest** extent on every side — the box a contour of the same graph auto-fits. An unbounded graph needs it. |
| `-o PATH` | `field_slice` | Output base: `BASE.png` **and** `BASE.svg` are written. A `.png` / `.svg` extension on `PATH` is stripped (`-o cut.png` writes `cut.png` + `cut.svg`); any other path is the base as given. |

**The mesh flags are refused.** `--depth`, `--collapse`, `--no-manifold`, `--tile-depth`,
`--mem`, `--weld`, `--decimate` and `--simplify` with `--slice` stop the tool before anything
is read: *`--slice writes no mesh; it cannot be combined with --depth`* (the first one given),
exit 2. The contour-only three are refused rather than ignored with a warning because a slice
has no octree for them to act on, and a flag that silently does nothing invites the reading
that it did something — a `--depth 9` slice looks no different from a `--depth 5` one.
`--dump-json` with `--slice` is refused too (each is a different run).

## What it writes

- **The PNG**: a diverging map — blue = inside, white = surface, red = outside — over a
  symmetric range set at the 99th percentile of `|value|` (one huge outside value of a mesh
  source cannot flatten the band), with the zero-contour burned in black.
- **The SVG**: the zero-contour as line segments (marching squares; a saddle cell is
  resolved by its four corners' average), in world units — the authoritative cross-section
  geometry. The marching squares is kept separable as the kernel of a future per-layer
  print slicer.
- Both share the world-up orientation (PNG row 0 = the largest `v`; the SVG flips `y`), so the
  SVG overlays the PNG exactly. In-plane axes: `z` → (u = x, v = y), `x` → (u = y, v = z),
  `y` → (u = x, v = z).

**The plane must cross the region.** A plane outside it samples empty space; the tool warns
*`plane z=0 is outside the sampling bounds [<min>, <max>]`*, then *`the field has a single
sign on this plane -- empty cross-section.`* — and still writes both files (a blank map, an
SVG with no segments). Read the printed `plane …, res …, pixel …` line for the pixel size.

**Errors**: a malformed `--slice` value — *`--slice expects AXIS=VALUE with AXIS x, y or z`*,
exit 2; an unbounded graph without `--bounds` — *`this field is unbounded -- pass --bounds …`*,
exit 1 (as its contour); a graph error is reported as for any build, with its location.

## Recipes

| # | What it shows |
| --- | --- |
| X1 | A TPMS cross-section — a bare gyroid is unbounded, so `--bounds` gives the region |
| X2 | A mesh-shell wall check: the hex-bored part hollowed to a 0.05 shell, cut through its bore |
| X3 | A lattice-in-mesh layer: one print layer of a gyroid infill inside the bracket, at 1024² |
| X4 | `dualc_slice`'s S1 as a graph — byte-identical PNG and SVG |
| X5 | The shipped sample `mesh_lattice.json`, cut at `x=0` |
| X6 | `dualc_slice`'s S2 corrected: a thin gyroid shell in the bunny, cut through its middle at 1024² |
| X7 | `dualc_slice`'s S3: a Schwarz-P shell in the cube on `x=0` |
| X8 | `dualc_slice`'s S4: millimetre scale — 5 mm gyroid cells, 2 mm walls, through the foot's centre |

```bash
# X1 — the gyroid's z=0 section over a 4-unit box → gyroid_z.png + gyroid_z.svg:
dualc_field --expr "gyroid(wavelength=1)" --slice z=0 --bounds -2,-2,-2,2,2,2 -o gyroid_z

# X2 — is the shell's wall continuous round the bore? (region: the part + 5 %):
dualc_field --expr "onion(mesh(path=\"hexbore.obj\"),thickness=0.05)" --slice y=0 -o hexbore_wall

# X3 — the z=0.1 layer of a gyroid infill clipped to the bracket:
dualc_field --expr "intersection(mesh(path=\"bracket.obj\"),onion(normalize(gyroid(wavelength=0.25)),thickness=0.03))" --slice z=0.1 --res 1024 -o bracket_layer.png

# X4 — dualc_slice cube.obj --type gyroid --wavelength 0.5 --offset 0.1 --plane z=0 --res 128,
#      with the TPMS centred where dualc_slice centres it (cube.obj's AABB centre) and its box
#      (the AABB padded 5 % per axis):
dualc_field --expr "intersection(onion(gyroid(center=[0,0,0],wavelength=0.5),thickness=0.1),mesh(path=\"cube.obj\"))" --slice z=0 --res 128 --bounds -0.55,-0.55,-0.55,0.55,0.55,0.55 -o gyroid_slice

# X5 — a graph file:
dualc_field mesh_lattice.json --slice x=0 -o mesh_lattice_x

# X6–X8 — the lattice-in-mesh graph of 14, with dualc_slice's defaults written in
# (C = the mesh's AABB centre, W = its diagonal / 10, the plane = the region's centre):
dualc_field --expr 'intersection(onion(normalize(gyroid(center=[-0.0168,0.1102,-0.0015],wavelength=0.025)),thickness=0.002),mesh(path="data/bunny.obj"))' --slice y=0.11 --res 1024 -o bunny_y
dualc_field --expr 'intersection(onion(schwarz_p(wavelength=0.4),thickness=0.05),mesh(path="cube.obj"))' --slice x=0 -o cube_x
dualc_field --expr 'intersection(onion(normalize(gyroid(center=[0,0,44.5],wavelength=5)),thickness=1),mesh(path="data/foot.obj"))' --slice z=44.5 -o foot
```

- **`dualc_slice`'s defaults.** Its plane defaulted to `z` at the region's centre; `--slice`
  has no default — read the centre off the mesh's box (the foot spans z 0 … 89, so `z=44.5`).
  Its lattice was always the graph of [14](14-tpms-in-mesh.md#the-graph-and-its-defaults),
  with that page's defaults (`wavelength` = the AABB diagonal / 10, `center` = the AABB
  centre); its `-o` replaced any extension, and defaulted to `<type>_slice`.
- **X6 corrects S2**, which cut `y=0` — below the bunny, whose box starts at `y ≈ 0.025`
  (the tool warned *plane y=0 is outside the sampling bounds*) — and with a 1-unit wall, which
  is thicker than the bunny (≈ 0.15 across) and fills every cell
  ([12/05](../12-dualc_field_view/05-tpms-in-mesh-preview.md#recipes) M4 shows it).

- **X4 and the region.** `dualc_slice` padded the mesh's box by 5 % **per axis**; `--slice`
  without `--bounds` pads by 5 % of the **largest** extent. On a cube they agree (X4 without
  `--bounds` writes the same bytes); on any other shape pass `dualc_slice`'s box as
  `--bounds` to reproduce it. `dualc_slice`'s `--normalize-thickness` was
  `onion(normalize(tpms), …)` in the graph — normalise first, then the shell.
- The meshes are the demo meshes next to the binary; the quoting of a `path` inside `--expr`
  on Windows PowerShell 5.1 is
  [11/01's](01-op-vocabulary.md#--expr-and-shell-quoting).

---

← Back to [Tool 11: `dualc_field`](README.md) · the [Command Reference index](../README.md)
