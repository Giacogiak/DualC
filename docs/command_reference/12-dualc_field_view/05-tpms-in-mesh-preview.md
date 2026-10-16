# Previewing a TPMS lattice inside a mesh (density-independent)

Part of [Tool 12: `dualc_field_view`](README.md). The single most common preview — one TPMS
lattice, optionally shelled and normalised, clipped to an input mesh — written as a graph. It
is what [`dualc_raymarch`](../10-dualc_raymarch.md), retired 2026-10-16, hard-coded; here it is one `--expr` (or
`.fld` file), and the same graph exports with [`dualc_field`](../11-dualc_field/README.md).
The cost is per screen pixel, not per cell: no mesh is built and RAM is bounded by the window,
so a `wavelength` that would run the contourer out of memory still renders. Only the mesh
clip is baked (a `--grid-res`³ narrow-band texture); the lattice stays analytic in the shader.

## The graph and its defaults

```
intersection(onion(normalize(gyroid(center=C,wavelength=W)),thickness=T),mesh(path="part.obj"))
```

| `dualc_raymarch` | In the graph | `dualc_raymarch`'s default, to write in by hand |
| --- | --- | --- |
| `<input.obj>` | the `mesh(path=…)` operand of the `intersection` | none: without a mesh it showed the bare lattice in the unit cube — drop the `intersection` and pass `--bounds -0.5,-0.5,-0.5,0.5,0.5,0.5` |
| `--type NAME` | the TPMS op: `gyroid`, `schwarz_p`, `diamond`, `fischer_koch`, `lidinoid`, `neovius` | `gyroid` |
| `--wavelength W` | `wavelength=W` on the TPMS | the mesh AABB diagonal / 10 (no mesh: `0.25`); the graph's own default is `1` |
| *(the lattice phase)* | `center=C` on the TPMS | the mesh AABB centre (no mesh: the box centre); the graph's own default is the origin |
| `--offset T` | the `onion(…, thickness=T)` wrapper; `T = 0` is no `onion` | `0` |
| `--normalize-thickness` | the `normalize(…)` wrapper, **inside** the `onion` | off |
| `--bounds …` | `--bounds …` (the same flag) | the mesh AABB + 5 % per axis — `dualc_field_view`'s own default for a mesh-clipped graph |
| `--grid-res N` · `--snapshot PATH` | the same flags | `96` (max `256`) · off |

`--bounds` is the raymarch box **and** the bake region of the mesh clip, so a box tighter
than the mesh concentrates the same grid on the part you inspect (M8); a box inside the mesh
is fine. Mesh paths resolve from the current directory, not the binary's: the demo meshes are
copied next to it, `bunny.obj` and `foot.obj` are not — `data/foot.obj` from the repo root.

**Millimetre workflow.** With 1 world unit = 1 mm
([design/09 § Units](../../design/09-conventions.md#units-and-frames)), `wavelength` is the
cell size in mm and `onion(normalize(…), thickness=T)` a wall of about `2·T` mm (`T` per side).
That holds only if the mesh is modelled in mm — rescale it host-side first.

## What replaces the hotkeys

The orbit, zoom, reset and section-plane keys are the same
([01 § Controls](01-controls.md#controls)). The four lattice keys become:

| `dualc_raymarch` | `dualc_field_view` |
| --- | --- |
| `↓` / `↑` (or `[` / `]`) — wavelength ×0.8 / ×1.25 | `tab` to `wavelength`, then `↓` / `↑` — the same factors, no recompile |
| `PgDn` / `PgUp` (or `-` / `=`) — offset −/+ 0.05 × wavelength, floored at 0 | `tab` to `thickness`, then `↓` / `↑` — ×0.8 / ×1.25, so it never reaches 0; going to or from no shell is adding or removing the `onion` |
| `t` — cycle the TPMS family | edit the op token in the file (`gyroid` → `schwarz_p`) and save: the viewer reloads on its own |
| `n` — toggle thickness normalisation | add or remove the `normalize(…)` wrapper in the file and save |

The last two are **structural edits**: they recompile the shader, which needs a graph *file*
(`--expr` and stdin have nothing to watch) — why the two tiers exist is
[01 § Why two tiers](01-controls.md#why-two-tiers). Write the graph to a `.fld` file, open it,
and keep the editor beside the window.

## Recipes

Each was `dualc_raymarch`'s recipe of the same number; `C` and `W` are the mesh's AABB centre
and diagonal / 10, rounded.

| # | What it shows |
| --- | --- |
| M1 | A bare gyroid in the unit cube — a smoke test, no mesh |
| M2 | A density that would run the contourer out of memory — still renders |
| M3 | A Schwarz-P shell clipped to the cube |
| M4 | A gyroid clipped to the bunny, normalised, with `thickness=1` |
| M5 | Millimetre scale: 5 mm gyroid cells and 2 mm walls in the foot, a 128³ clip |
| M6 | Headless: one PNG of a gyroid shell in the cube, and exit |
| M7 | Section a solid lattice: press `z`, then `→` / `←`, `f` to flip, `0` to clear |
| M8 | The bake zoomed into the ankle: the same 96³ grid on 28 mm instead of 150 mm (0.3 mm cells) |

```bash
dualc_field_view --expr "gyroid(wavelength=0.2)" --bounds -0.5,-0.5,-0.5,0.5,0.5,0.5     # M1
dualc_field_view --expr "gyroid(wavelength=0.02)" --bounds -0.5,-0.5,-0.5,0.5,0.5,0.5    # M2
dualc_field_view --expr 'intersection(onion(schwarz_p(wavelength=0.5),thickness=0.05),mesh(path="cube.obj"))'   # M3
dualc_field_view --expr 'intersection(onion(normalize(gyroid(center=[-0.0168,0.1102,-0.0015],wavelength=0.025)),thickness=1),mesh(path="data/bunny.obj"))'   # M4
dualc_field_view --expr 'intersection(onion(normalize(gyroid(center=[0,0,44.5],wavelength=5)),thickness=1),mesh(path="data/foot.obj"))' --grid-res 128   # M5
dualc_field_view --expr 'intersection(onion(gyroid(wavelength=0.1732),thickness=0.1),mesh(path="cube.obj"))' --snapshot cube.png   # M6
dualc_field_view --expr "onion(gyroid(wavelength=0.15),thickness=0.04)" --bounds -0.5,-0.5,-0.5,0.5,0.5,0.5    # M7
dualc_field_view --expr 'intersection(onion(normalize(gyroid(center=[0,0,44.5],wavelength=14)),thickness=2),mesh(path="data/foot.obj"))' --bounds -14,-34,41,14,-6,69   # M8
```

- **M4's wall fills the bunny.** The bunny is about 0.15 units across, so a 1-unit wall
  covers every cell and the preview is the solid bunny; for a lattice try `thickness=0.002`.
- On PowerShell 5.1 escape each inner quote
  ([11/01 § `--expr` and shell quoting](../11-dualc_field/01-op-vocabulary.md#--expr-and-shell-quoting)).
- A thin un-normalised shell (M6) shows dark specks on grazing silhouettes on one renderer —
  a preview artefact, tracked as #61
  ([17/17](../../roadmap/17-code-audit-and-hardening/17-viewer-silhouette-specks.md)).

---

← Back to the [Tool 12 overview](README.md) · the [Command Reference index](../README.md).
