# Tool 10: `dualc_raymarch` (GPU raymarch viewer)

Standalone GPU raymarch viewer — density-independent (opt-in build).

```
dualc_raymarch [<input.obj>] [--type NAME] [--wavelength W] [--offset T]
               [--normalize-thickness] [--bounds x0,y0,z0,x1,y1,z1]
               [--grid-res N] [--snapshot PATH]
```

Opens an interactive 3D window that **sphere-traces the analytic TPMS field once
per screen pixel** — there is no mesh to build, so RAM is bounded by the window,
not the lattice, and it renders parts at a `--wavelength` that would OOM
`dualc_lattice`'s contoured mesh. Cost is per-pixel rather than per-cell (finer
lattices cost somewhat more per frame as the conservative march step shrinks; a
real GPU is interactive where a software GL fallback is seconds-per-frame).
The lattice (TPMS + optional `|F|−T` offset + optional
`1/|grad F|` normalisation) is evaluated directly in a GLSL fragment shader and
needs no grid; only the *low-frequency mesh clip* is baked — a narrow-band SDF
of the input mesh uploaded as a small `GL_R32F` 3D texture and combined as
`max(lattice, meshSDF)`. With no input mesh the bare lattice is shown in
`--bounds`. There is **no `--depth`** (nothing is contoured) and **no `-o`**
(interactive only).

**Opt-in build** (`-DDUALC_BUILD_RAYMARCH_VIEWER=ON`; GLFW is fetched at its pinned
release and glad is vendored — the [build prelude](README.md#build-prelude));
`libdualc` is unaffected.

## CLI flags

| Flag | Default | Meaning |
| --- | --- | --- |
| `<input.obj>` | (none) | With a mesh, the lattice is clipped to its interior; without, the bare TPMS is shown in `--bounds`. |
| `--type NAME` | `gyroid` | TPMS family (same six as `dualc_lattice`). |
| `--wavelength W` | mesh-diag / 10, or `0.25` with no mesh | Unit-cell side (world units). |
| `--offset T` | `0` | Thick-wall offset (`\|F\|−T`), `T ≥ 0`. |
| `--normalize-thickness` | off | Normalise the TPMS by `1/\|grad F\|` before the offset. |
| `--bounds x0,y0,z0,x1,y1,z1` | input AABB + 5%, or unit cube | Raymarch box. It is **also** the mesh-SDF bake region, so a box tighter than the mesh concentrates `--grid-res` on the part you are inspecting. A box that sits *inside* the mesh is fine (the bake's sign flood no longer assumes a padded region — [roadmap 12/07 § H2](../roadmap/12-field-graph-and-app/07-mesh-preview-sweep.md#h2-the-bakes-far-field-sign-flood-assumed-a-padded-region)). |
| `--grid-res N` | `96` (max `256`) | Mesh-SDF bake resolution per axis. Only the low-frequency clip is baked; the lattice stays analytic. |
| `--snapshot PATH` | (off) | Render one frame headless to a PNG and exit — no window. For scripted / visual verification. |

## Live controls

| Input | Action |
| --- | --- |
| Left-drag | Orbit |
| Scroll | Dolly (zoom) |
| `↓` / `↑` (or `[` / `]`) | Wavelength ×0.8 / ×1.25 (finer / coarser cells) |
| `PgDn` / `PgUp` (or `-` / `=`) | Offset down / up (thick-wall `\|F\|−T`) |
| `t` | Cycle TPMS family |
| `n` | Toggle thickness normalisation |
| `x` / `y` / `z` | Toggle the **section plane** on that axis (and make it active) |
| `←` / `→` | Slide the active section plane − / + (arrow keys, auto-repeat) |
| `f` | Flip the active section plane's kept side |
| `0` | Clear all section planes |
| `r` | Reset view |
| `Esc` | Quit |

**Section planes** cut the lattice with up to three axis-aligned planes so you can
inspect the interior — `x`/`y`/`z` switch them on, `←`/`→` slide the active one,
`f` flips the kept half, `0` clears. The slice face is a solid warm-tinted
cross-section (a true capped section), sliding is instant, and it is
**visualization only** — a section never affects any exported geometry
([12/03 § Section planes](../roadmap/12-field-graph-and-app/03-raymarch-app.md#section-planes-viewport-inspection)).
Every key is layout-independent — values on the arrows and Page keys, letters
matched by the printed character, `[`/`]` and `-`/`=` kept only as US alternates
([design/09](../design/09-conventions.md#keyboard-layout-independence)).

Changing wavelength / offset / type / normalisation is instant — they are shader
uniforms, so no CPU re-work happens (the baked mesh texture is independent of the
lattice parameters). Note: a very thin `--offset` shell (walls ≪ `--wavelength`)
can show small holes — the sphere-trace step is capped at a fraction of the
wavelength and may step over a sub-wavelength wall; raise `--offset` or shrink
`--wavelength` if you see them (it is a march-tuning artifact, not a field error).

## Recipes

| # | Command | What it shows |
| --- | --- | --- |
| M1 | `dualc_raymarch --type gyroid --wavelength 0.2` | Bare gyroid in the unit cube — quick smoke test, no mesh needed. |
| M2 | `dualc_raymarch --type gyroid --wavelength 0.02` | A density that would OOM the mesher — still renders (no mesh built; per-pixel cost). |
| M3 | `dualc_raymarch cube.obj --type schwarz-p --wavelength 0.5 --offset 0.05` | Schwarz-P thick-wall shell clipped to the cube. |
| M4 | `dualc_raymarch data/bunny.obj --type gyroid --offset 1.0 --normalize-thickness` | 1-unit-wall gyroid lattice clipped to the bunny; orbit to inspect. (`bunny.obj` is not copied next to the exe — pass a path.) |
| M5 | `dualc_raymarch data/foot.obj --wavelength 5 --offset 1 --normalize-thickness --grid-res 128` | **Real-world mm-scale** (1 unit = 1 mm): 5 mm gyroid cells, 2 mm walls (1 mm/side). `--type` omitted ⇒ gyroid; `--grid-res 128` keeps the foot boundary crisp. |
| M6 | `dualc_raymarch cube.obj --type gyroid --offset 0.1 --snapshot cube.png` | Headless: write one PNG and exit (no window) for scripted checks. |
| M7 | `dualc_raymarch --type gyroid --wavelength 0.15 --offset 0.04` then press `z`, then `→` / `←` | **Section a solid lattice:** the Z plane caps the gyroid with a tan cross-section; slide it to sweep through the cells, `f` to flip the kept half, `x`/`y` to add more cuts, `0` to clear. Visualization only. |
| M8 | `dualc_raymarch data/foot.obj --wavelength 14 --offset 2 --normalize-thickness --bounds -14,-34,41,14,-6,69` | **Zoom the bake into a sub-region:** `--bounds` entirely inside the ankle, so the same 96³ grid covers 28 mm instead of 150 mm (0.3 mm cells). |

**Mesh paths are relative to the current directory, not the executable.** The
`dualc_gen_demo` palette (`cube.obj`, `sphere.obj`, `torus.obj`, …) is copied next
to the exe, so those bare names resolve when you run from the build output folder.
Other meshes — including `bunny.obj` and `foot.obj` — are **not** copied; pass a
path that resolves from where you launch, such as `data/foot.obj` from the repo
root or an absolute path. A missing file exits with
`[dualc_raymarch] cannot open mesh 'data/foot.obj': no such file (paths are relative to the current directory, not the executable)`
before the window opens.

**Millimetre workflow.** With 1 world unit = 1 mm
([design/09 § Units](../design/09-conventions.md#units-and-frames)), `--wavelength` is the cell size in mm
and `--offset T --normalize-thickness` gives a wall of `2·T` mm (`T` mm per side).
This only holds if the input mesh is modelled in mm — a part exported in metres or
inches must be rescaled first, host-side. `--type` defaults to `gyroid`; pass
`--type schwarz-p|diamond|…` (or press `t` in the viewer) for another family.

## See also

`dualc_raymarch` is the tuned single-TPMS view. For an **arbitrary composed
field-graph** — any primitives/booleans/decorators/lattices/mesh sources, the
same graph the exporter contours — use its general successor
[`12-dualc_field_view/`](12-dualc_field_view/README.md), which compiles the graph to
GLSL on the shared GL stack. Export the same field with
[`11-dualc_field/`](11-dualc_field/README.md).

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
