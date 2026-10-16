# Tool 11 — `dualc_field`

The general **field-graph** CLI: parse a composable field description, build the
`dualc::ImplicitField` it denotes, dual-contour it **once**, and export. Where
`dualc_lattice`/`dualc_boolean`/`dualc_csg_demo` are fixed-shape shortcuts,
`dualc_field` lets you compose *any* of the registry operations into one tree —
and crucially it composes them at the **field** level, so a boolean over a TPMS
lattice is a single contour pass, not a mesh round-trip.

> **Why this matters.** Composing as a field instead of as meshes turns a
> minutes-long lattice → mesh-boolean round-trip into one analytic contour — the
> measurements are the
> [Phase 0 benchmark](../../roadmap/11-dense-lattice-deliverable/02-phase-0-benchmark.md#phase-0-benchmark);
> the field-graph keystone is [roadmap/12](../../roadmap/12-field-graph-and-app/README.md).

## Synopsis

```bash
dualc_field <graph.json> [options]      # graph from a JSON (or .fld shorthand) file
dualc_field -            [options]      # graph from stdin (for pipes / Grasshopper)
dualc_field --expr "<shorthand>" [options]   # graph from an inline shorthand string
dualc_field --list                     # print the op vocabulary and exit
dualc_field <graph> --dump-json        # canonicalise any input to JSON and exit
dualc_field <graph> --slice z=0        # sample on a plane → PNG + SVG, no mesh (page 10)
```

The graph can be given two ways — **JSON** (the canonical form, in a file or on
stdin) or the **[text shorthand](01-op-vocabulary.md#text-shorthand)** (one line, via `--expr` or a
`.fld` file). They denote the *same* tree; see
[**Choosing an input
form**](01-op-vocabulary.md#choosing-an-input-form-json-file-vs---expr-vs-stdin)
for when to use which (and the shell-quoting caveats). The parser/builder
lives in `examples/field_graph.{h,cpp}`, a separate translation unit that
`dualc_field_view` and the C ABI share. The op vocabulary
**is** the `example_common` registry vocabulary used by the other CLIs.

## Options

| Option | Default | Meaning |
| --- | --- | --- |
| `-o PATH` | `field.obj` | Output mesh; `.obj` / `.stl` / `.3mf` by extension ([export formats](../README.md#export-formats--o-extension-dispatch)). With `--slice`, the PNG + SVG base (default `field_slice`). |
| `--depth N` | `7` | Octree max depth; cell size ≈ root extent / `2^N`. |
| `--collapse E` | `0` | Adaptive octree cell-collapse *during* contouring (QEF energy, length²); helps flat regions, ~nil on dense TPMS. How it works and [which lighter-files lever](05-decimation.md#which-lighter-files-lever---collapse-vs---decimate-vs---tile-depth) to reach for: [05](05-decimation.md). |
| `--decimate R` | off | **Post-contour QEM decimation** (monolithic only): keep fraction `R` ∈ (0,1) of the triangles (e.g. `0.1` ≈ 10× lighter) via a global [meshoptimizer](https://github.com/zeux/meshoptimizer) simplify on the finished mesh, before export. Global (quality-optimal), watertight in/out. **Monolithic only** — cannot combine with `--tile-depth`. See [Decimation](05-decimation.md#decimation---decimate----simplify). |
| `--simplify E` | off | Like `--decimate` but targets an **absolute geometric error** of `E` world units (mm) instead of a keep-ratio; the triangle count falls as far as that error allows. Mutually exclusive with `--decimate`; monolithic only. |
| `--bounds x0,y0,z0,x1,y1,z1` | auto | Explicit root box. Required when the graph is **unbounded** (a bare `plane`, an infinite source, or a `repeat`); otherwise the sampler auto-fits to the field's bounds (padded ~5 %). |
| `--tile-depth D` | off | **Streaming / tiled export** for dense parts that would OOM the monolithic contourer. Contour in bounded-memory tiles of `2^D` cells each and stream triangles to disk; peak RAM is one tile. `.stl` or `.3mf` output ([the 3MF path](08-3mf-and-weld.md#the-3mf-path--o-3mf)); useful range `2 ≤ D ≤ depth − 1`. See [Streaming / tiled export](06-streaming-tiled-export.md#streaming--tiled-export---tile-depth). |
| `--weld` | off | With `--tile-depth` and `.3mf` output, weld across-tile seam vertices into a **single globally-manifold, shared-vertex** object (topology-identical to the monolithic mesh) — for consumers that need connectivity (re-booleans, FEA, decimation). Bounded to a 2D seam-vertex hash, not one tile. See [The 3MF path](08-3mf-and-weld.md#the-3mf-path--o-3mf). |
| `--mem BUDGET` | off | **Auto-pick `--tile-depth`** from a RAM budget (`4G`, `512M`, `2048K`, or a byte count) instead of choosing `D` by hand: a cheap coarse probe estimates the busiest tile and selects the **largest `D` that fits**. `.stl` or `.3mf` output. Conservative (errs to a smaller `D`, never OOMs); an explicit `--tile-depth` overrides it. See [Auto-budget](07-mem-budget.md#auto-budget---mem-budget). |
| `--no-manifold` | off | Disable Manifold Dual Contouring (one vertex per cell instead of per surface component). |
| `--slice AXIS=VALUE` | off | Sample the field on an axis plane instead of contouring: `BASE.png` heatmap + `BASE.svg` contour; refuses the mesh flags. See [Slicing a graph](10-slice.md#options). |
| `--res N` | `512` | With `--slice` only: grid resolution per side, clamped to `2 … 4096`. |
| `--expr S` | — | Build from a terse [text shorthand](01-op-vocabulary.md#text-shorthand) string instead of a file/stdin. |
| `--dump-json` | — | Print the canonical JSON for the parsed graph and exit (no build/contour) — round-trip / Grasshopper aid. |
| `--list` | — | Print the op vocabulary (grouped) and exit. |
| `--help`, `-h` | — | Usage. |

**Input format.** A file or stdin is read as **JSON or shorthand**: a `.fld`
file is shorthand; otherwise the first non-whitespace character decides (`{` ⇒
JSON, anything else ⇒ shorthand). `--expr` is always shorthand.

`--depth`/`--collapse`/`--bounds`/`--no-manifold` are properties of the
*invocation*, not the graph — the graph is a pure field description. Per-`mesh`
sign/normals modes live **in** the graph ([Document & node shape](#document--node-shape)), not as global flags.

## Document & node shape

- **Document:** `{ "version": 1, "units": "mm", "root": <node> }`. A bare
  `<node>` is also accepted (the `root`/header are optional for quick use).
- **Node:** `{ "op": <token>, <params…>, "in": [<children>] }`.
  - Leaf **sources** (TPMS, primitives, `mesh`, `winding`) have no `in`.
  - **Decorators / domain operators** take exactly **one** child (except
    `graded-onion` / `graded-offset`, two-child decorators — base + control
    field).
  - **Booleans** take exactly **two** children.
- **Defaults:** every parameter defaults to its registry value, so partial nodes
  are valid — `{"op":"sphere"}` is the unit sphere at the origin.

## The pages of this tool

The reference for `dualc_field` is split by concern; every page is ready-to-run on its
own and links back here for the options table.

| Page | What it covers |
| --- | --- |
| [Input forms, op vocabulary & text shorthand](01-op-vocabulary.md) | How to hand the tool a graph (JSON file, `--expr`, stdin — and shell quoting), then every node `dualc_field` (and `dualc_field_view`) accepts — sources, booleans, decorators, domain operators, the primitive parameter forms — and the `--expr` text grammar. Strut and graded lattices, 2D profiles and lifts have their own pages ([02](02-strut-lattices.md), [03](03-graded-and-morph.md), [09](09-2d-profiles-and-lifts.md)). |
| [Strut lattices (`sc` · `bcc` · `fcc` · `octet`)](02-strut-lattices.md) | The wireframe-crystal source nodes: parameters, tapered struts (`nodeRadius`), clipping to a mesh, and the recipe block (infill, spatial blend, hollow, graded and tapered struts). |
| [Graded shell, graded inflation & spatial morph](03-graded-and-morph.md) | The two-child `graded-onion` / `graded-offset` decorators, the three-child `mix` morph, the critical callout on interpolating disjoint fields, and grafting two crystals into one body. |
| [Workflow: isolate → thicken → skin → union a TPMS lattice](04-workflow-open-surface.md) | The four-step manufacturing workflow that turns a TPMS lattice and its enclosing part into one printable solid, with the RAM-bounded and preview variants. |
| [Decimation (`--decimate` / `--simplify`)](05-decimation.md) | Post-contour QEM decimation: choosing the knob, how far it can go (measured), which lighter-files lever to reach for, recipes, limits. |
| [Streaming / tiled export (`--tile-depth`)](06-streaming-tiled-export.md) | Contour in grid-aligned tiles and stream the mesh at one-tile RAM: how it works, choosing `D`, the constraints and the watertightness contract, and why the stream is STL. |
| [Auto-budget (`--mem BUDGET`)](07-mem-budget.md) | Let the tool pick the largest `--tile-depth` that fits a RAM budget instead of choosing `D` by hand: syntax, the estimate, precedence, recipes. |
| [The 3MF path and `--weld`](08-3mf-and-weld.md) | Streaming to a slicer-native 3MF (per-tile objects, `1 unit = 1 mm`), `--weld` for a single globally-manifold object, and the tiled/welded recipes. |
| [2D profiles and lifts (`revolve` · `extrude`)](09-2d-profiles-and-lifts.md) | The profiles `circle`, `rect`, `segment`, `polygon` and the two lifts that make them 3D — the graph form of `dualc_lift`: keys and defaults, the typing rule, recipes. |
| [Slicing a graph (`--slice`)](10-slice.md) | Any graph sampled on an axis plane → PNG heatmap + SVG contour, no mesh — the graph form of `dualc_slice`: options and defaults, the refused mesh flags, the region, recipes. |
| [Mesh in → mesh out](11-mesh-in.md) | Re-meshing a mesh, choosing a sign oracle, sharp normals, sealing soup with `winding` — the graph form of `dualc_demo`, flag by flag. |
| [The primitive catalogue](12-primitive-catalogue.md) | P1–P30 with their defaults, the decorators D1–D7, the domain operators O1–O6 and the displace functions — `dualc_primitive`'s catalogue in graph syntax. |
| [Six composition samples](13-composition-samples.md) | The six `examples/samples/` graphs that were `dualc_csg_demo`'s recipes, each byte-identical. |
| [A TPMS lattice inside a mesh](14-tpms-in-mesh.md) | The graph `dualc_lattice` composed, its three mesh-derived defaults written out, wavelength vs. thickness, the six TPMS formulas. |

## Diagnostics

Errors are reported with the offending node's **JSON pointer**:

```
[dualc_field] field-graph error at /root/in/0/in/1: unknown op 'frobnicate'
```

Caught cases: invalid JSON, unknown op, wrong boolean arity, missing required
param, bad `mesh` sign/normals (the message lists the accepted values), a `bake` outside
[2, 512] or a `band` without `bake` on `mesh`/`winding`, a 2D profile outside a lift or a
3D child under one ([09](09-2d-profiles-and-lifts.md#typing-errors)), mesh-not-found.
An **unbounded** graph contoured without `--bounds` prints the sampler's
`--bounds` hint and exits non-zero.

Runtime **`[dualc] warning:` lines** (empty contour, bounds fallback, sampling
past a baked grid, a non-closed output) are documented once for every tool in
[Shared behaviour § Diagnostics warnings](../00-shared-behaviour.md#diagnostics-warnings-every-exporting-tool).

## Examples

A sphere carved out of a 2 mm-walled gyroid lattice inside a box (all analytic,
auto-fit bounds), exported straight to print-ready 3MF:

```json
{ "version": 1, "units": "mm",
  "root": { "op": "difference", "in": [
    { "op": "intersection", "in": [
      { "op": "box", "min": [-40,-40,-40], "max": [40,40,40] },
      { "op": "onion", "thickness": 2, "in": [
        { "op": "normalize", "in": [
          { "op": "gyroid", "wavelength": 5 } ] } ] } ] },
    { "op": "sphere", "center": [0,0,0], "radius": 20 } ] } }
```

```bash
# F1 — the graph above, exported print-ready
dualc_field gyroid_box.json --depth 7 -o part.3mf
```

A TPMS lattice clipped to an input volume — the field-level equivalent of
`dualc_lattice`, and freely composable with further booleans:

```json
{ "op": "intersection", "in": [
  { "op": "mesh", "path": "bracket.obj" },
  { "op": "onion", "thickness": 0.1, "in": [
    { "op": "normalize", "in": [
      { "op": "gyroid", "wavelength": 0.5 } ] } ] } ] }
```

```bash
# F2 — a mesh-clipped lattice, exported as STL
dualc_field mesh_lattice.json --depth 7 -o bracket_infill.stl
# F3 — or stream a graph in from another tool / Grasshopper:
cat graph.json | dualc_field - -o out.obj
```

Eight ready-to-run samples ship next to the binary (sources in `examples/samples/`):
`gyroid_box.json`, `mesh_lattice.json` and the six of [13](13-composition-samples.md).

## Recipes

| # | What it does | Page |
| --- | --- | --- |
| F1 | Export the gyroid-box graph print-ready (`.3mf`) | above |
| F2 | Export a mesh-clipped lattice (`.stl`) | above |
| F3 | Stream a graph in on stdin | above |
| E1–E3 | Shorthand one-liners: the gyroid box as `--expr`, a smooth union, canonicalise a `.fld` | [01](01-op-vocabulary.md#recipes) |
| S1–S17 | Strut lattices: infill, blends, hollow, graded and tapered struts | [02](02-strut-lattices.md#recipes) |
| J1–J2 | Graft two different crystals into one body | [03](03-graded-and-morph.md#recipes) |
| W1–W6 | Isolate → thicken → skin → union a TPMS lattice | [04](04-workflow-open-surface.md#recipes) |
| Q1–Q5 | Decimation workflows | [05](05-decimation.md#recipes) |
| T1–T2 | Tiled STL / 3MF at one-tile RAM | [06](06-streaming-tiled-export.md#recipes) |
| U1–U5 | `--mem` budgets | [07](07-mem-budget.md#recipes) |
| Z1–Z4 | Tiled and welded 3MF | [08](08-3mf-and-weld.md#recipes) |
| R1–R4, L1–L8 | 2D profiles revolved and extruded, `dualc_lift`'s recipes as graphs | [09](09-2d-profiles-and-lifts.md#recipes) |
| X1–X8 | Slices: a TPMS section, a shell's wall, a lattice layer, `dualc_slice`'s S1–S4 as graphs | [10](10-slice.md#recipes) |
| G1–G9, H1–H2 | Re-mesh, sharp normals, collapse, the three oracles, the soup sealed to STL; mesh booleans | [11](11-mesh-in.md#recipes) |
| C1–C7 | Primitives overridden, displaced, composed | [12](12-primitive-catalogue.md#recipes) |
| K1–K6 | The six composition samples | [13](13-composition-samples.md#recipes) |
| Y1, T7–T14 | Lattices in a mesh: the six types, shells, real-mesh infill, the defaults computed | [14](14-tpms-in-mesh.md#recipes) |

## Scope

`dualc_field` **generalises but does not replace** `dualc_lattice`,
`dualc_boolean`, or `dualc_csg_demo` — those remain convenience shortcuts and
the registries stay the single source of truth. The DAG-ref syntax (`id`/`ref`,
`let … in …`) of the serialization superset is not implemented — row D-12 of the
[decisions index](../../decisions/README.md).

## Live GPU preview

The **same graph** this tool contours previews live on the GPU in
[`dualc_field_view`](../12-dualc_field_view/README.md), which accepts the identical
input — preview equals export by construction
([design/10](../../design/10-invariants-and-tolerances.md#output-invariants)).

---

← Back to the [Command Reference index](../README.md) · Live preview:
[12-dualc_field_view/](../12-dualc_field_view/README.md) · Design:
[roadmap/12](../../roadmap/12-field-graph-and-app/README.md)
