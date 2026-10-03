# Tool 7: `dualc_slice` (field cross-section sampler)

Sample a field on a cutting plane → PNG heatmap + SVG contour.

```
dualc_slice <input.obj> [--type NAME] [--wavelength W] [--offset T]
            [--normalize-thickness] [--plane AXIS=VALUE] [--res N]
            [--bounds x0,y0,z0,x1,y1,z1] [-o base.png]
```

Composes the **identical** lattice-in-mesh field as `dualc_lattice`, then —
instead of contouring — samples the implicit field directly on an axis-aligned
cutting plane and writes a **PNG heatmap** plus an **SVG** of the zero-contour.
Cost is `O(res²)`, independent of lattice density, so it inspects (and previews
the cross-section of) parts far too dense to mesh. The marching-squares contour
is kept separable as the kernel of future per-layer print-slicing.

There is **no `--depth`**: the tool samples the field, it does not dual-contour.

| Flag | Default | Meaning |
| --- | --- | --- |
| `<input.obj>` | (required) | Mesh whose interior clips the lattice. |
| `--type NAME` | `gyroid` | TPMS family (same six as `dualc_lattice`). |
| `--wavelength W` | mesh-diag / 10 | Unit-cell side (world units). |
| `--offset T` | `0` | Thick-wall offset (`\|F\|−T`); `T ≥ 0`. |
| `--normalize-thickness` | off | Normalise the TPMS by `1/\|grad F\|` before offset. |
| `--plane AXIS=VALUE` | `z` at bounds centre | Cutting plane, e.g. `z=0`, `x=1.5` (AXIS ∈ x/y/z). |
| `--res N` | `512` | Grid resolution per side (max `4096`). |
| `--bounds x0,y0,z0,x1,y1,z1` | input AABB + 5% | Sampling region. |
| `-o PATH` | `<type>_slice.png` | Output base; `.png` **and** `.svg` are written (default is named after the TPMS type, not the input). |

The PNG is a diverging map (blue = inside, white = surface, red = outside;
symmetric range from the 99th percentile of `\|value\|` so one huge
MeshSource-outside value can't flatten the band) with the zero-isocontour
burned in. PNG row-0 = max-v and the SVG Y-flip keep both outputs in the same
world-up orientation, so the SVG overlays the PNG exactly. In-plane axis
mapping: z → (u=x, v=y), x → (u=y, v=z), y → (u=x, v=z).

**The plane must lie inside the mesh.** `--plane AXIS=VALUE` is an **absolute
world coordinate**, and real meshes are rarely centred on the origin (a foot or
a scan can sit at, say, z ≈ 60). A plane outside the sampling bounds samples only
empty space → the field has a single sign → an **empty slice** (blank PNG, no SVG
segments), and the tool warns `[dualc_slice] warning: plane z=0 is outside the
sampling bounds [<min>, <max>]` and then `[dualc_slice] warning: the field has a
single sign on this plane -- empty cross-section.` **Omit `--plane` to cut the
bounds *centre*** (always inside), or read the printed `plane <axis>=<value>`
line and pick an interior value. `--plane z=0` is correct for an origin-centred
mesh like `cube.obj`, but not for an off-origin one.

## Recipes

| # | Command | What it builds |
| --- | --- | --- |
| S1 | `dualc_slice cube.obj --type gyroid --wavelength 0.5 --plane z=0` | Mid-plane cross-section of a gyroid-in-cube → `gyroid_slice.png` + `.svg`. |
| S2 | `dualc_slice data/bunny.obj --offset 1.0 --normalize-thickness --plane y=0 --res 1024 -o bunny_y.png` | High-res `y=0` slice of a 1 mm-wall gyroid shell inside the bunny. (`bunny.obj` is not copied next to the exe — pass a path.) |
| S3 | `dualc_slice cube.obj --type schwarz-p --wavelength 0.4 --offset 0.05 --plane x=0 -o cube_x.png` | Schwarz-P shell cross-section on the `x=0` plane. |
| S4 | `dualc_slice data/foot.obj --wavelength 5 --offset 1 --normalize-thickness -o foot.png` | Real-world mm-scale part (1 unit = 1 mm): 5 mm gyroid cells, 2 mm walls. `foot.obj` is not copied next to the exe — pass a path. **`--plane` omitted** so it cuts the centre — an off-origin foot has no data at `z=0`. |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
