# DualC — Complete Command Reference

The exhaustive catalogue of every command DualC exposes — every tool, primitive,
decorator, operator, boolean, lift, displace function and baked recipe (counts in
the [inventory](#appendix--full-inventory-count)); each entry is a ready-to-run
command. One page per tool; this page holds the build prelude, the tool index,
where each feature is documented, the shared options and the export-format
dispatch. What every tool does the same way — rejected parameters, the runtime
warnings, the `.part` temp every export renames on success — is
[00-shared-behaviour.md](00-shared-behaviour.md).

## Build prelude

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DDUALC_BUILD_EXAMPLES=ON   # Windows
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDUALC_BUILD_EXAMPLES=ON   # POSIX
cmake --build build --config Release -j
```

The first configure fetches geometry-central at its pinned commit (v1.1.0) into `build/_deps/`
unless `-DDUALC_GC_DIR=/path` names a local tree; geometry-central then fetches Eigen unless one
is found. No other flag is needed for the tools.

The GL viewers are opt-in, each `OFF` by default and none a dependency of
`libdualc`: `-DDUALC_BUILD_RAYMARCH_VIEWER=ON` (tool 10), `-DDUALC_BUILD_FIELD_VIEW=ON` (12)
and `-DDUALC_BUILD_GLSL_PARITY=ON` (the field→GLSL parity harness). All three fetch GLFW at
its pinned release (or take a local tree via `-DDUALC_GLFW_DIR=/path`) and link the vendored
glad loader; on Linux GLFW's X11 backend needs the X11 development headers (`README.md`
§ Dependencies). Built CLIs land in `build/examples/` (Ninja, Makefiles) or
`build/examples/Release/` (Visual Studio); run every recipe from there (append `.exe` on
Windows, prefix `./` on POSIX). The build generates the
demo meshes into `build/data/` and copies them next to every tool; to run recipes from the repo
root instead, populate `data/` with `dualc_gen_demo all --dir data` ([09](09-dualc_gen_demo.md)).

## The 12 tools (binaries)

| # | Tool | Purpose | Page |
| --- | --- | --- | --- |
| 1 | `dualc_demo` | Re-mesh a triangle mesh through the dual contourer. | [01-dualc_demo.md](01-dualc_demo.md) |
| 2 | `dualc_primitive` | Analytic primitives + decorators + domain operators. | [02-dualc_primitive.md](02-dualc_primitive.md) |
| 3 | `dualc_boolean` | Booleans of two meshes. | [03-dualc_boolean.md](03-dualc_boolean.md) |
| 4 | `dualc_lift` | Revolve or extrude a 2D profile into 3D. | [04-dualc_lift.md](04-dualc_lift.md) |
| 5 | `dualc_csg_demo` | Baked field-tree composition recipes. | [05-dualc_csg_demo.md](05-dualc_csg_demo.md) |
| 6 | `dualc_lattice` | TPMS lattice infill bounded by an input mesh. | [06-dualc_lattice.md](06-dualc_lattice.md) |
| 7 | `dualc_slice` | Sample a field on a cutting plane → PNG heatmap + SVG contour. | [07-dualc_slice.md](07-dualc_slice.md) |
| 8 | `dualc_view` | Retired 2026-10-03 with the Polyscope dependency; the number is never reused. | [08-dualc_view.md](08-dualc_view.md) |
| 9 | `dualc_gen_demo` | Generate the procedural demo meshes (sphere, torus, knot, …). | [09-dualc_gen_demo.md](09-dualc_gen_demo.md) |
| 10 | `dualc_raymarch` | Standalone GPU raymarch viewer — density-independent (opt-in build). | [10-dualc_raymarch.md](10-dualc_raymarch.md) |
| 11 | `dualc_field` | General field-graph: compose any ops (JSON) and contour once. | [11-dualc_field/](11-dualc_field/README.md) |
| 12 | `dualc_field_view` | Field-graph GPU raymarch viewer — any field-graph compiled to GLSL and sphere-traced (opt-in build). | [12-dualc_field_view/](12-dualc_field_view/README.md) |

Pages 11 and 12 are same-numbered folders; the size contract (`scripts/check.py`)
applies to every page.

### Lattices have no separate tool page — they are field-graph *nodes*

There is intentionally **no `dualc_strut`/dedicated strut-lattice page**. Strut
lattices (and TPMS surfaces) ship as **field-graph source nodes**, not standalone
CLIs, so they compose with every boolean / decorator / mesh-clip and preview ==
export for free. They are documented inside the `dualc_field` pages:

| Lattice family | Nodes | Documented in |
| --- | --- | --- |
| **Strut lattices** (wireframe crystals) | `sc` · `bcc` · `fcc` · `octet` | [11-dualc_field/ § Strut lattices](11-dualc_field/02-strut-lattices.md#strut-lattices-wireframe-crystals) (params, clipping, recipes S1–S17); live preview in [12-dualc_field_view/](12-dualc_field_view/README.md) |
| **Graded strut radius** (spatially-varying thickness) | `graded-offset` | [11-dualc_field/ § Graded shell & graded inflation](11-dualc_field/03-graded-and-morph.md#graded-shell--graded-inflation-two-children-base-control) |
| **TPMS surfaces** | `gyroid` · `schwarz-p` · `diamond` · `fischer-koch` · `lidinoid` · `neovius` | [11-dualc_field/](11-dualc_field/README.md); mesh-bounded infill also has the standalone [06-dualc_lattice.md](06-dualc_lattice.md) |
| **Graded shell thickness** | `graded-onion` | [11-dualc_field/ § Graded shell & graded inflation](11-dualc_field/03-graded-and-morph.md#graded-shell--graded-inflation-two-children-base-control) |
| **Crystal morph / spatial blend** (three children) | `mix` | [11-dualc_field/ § Spatial morph](11-dualc_field/03-graded-and-morph.md#spatial-morph-three-children-a-b-control) |

Design rationale and status: [roadmap 05 §17 / §17b](../roadmap/05-tpms-lattices/README.md).

## Where a feature is documented

| Feature | Flags / nodes | Page |
| --- | --- | --- |
| Seal an open shell, soup or self-intersecting mesh into one watertight solid | `--gwn-field` (`dualc_demo`); the `winding` node (`dualc_field`) | [01 § Choosing a sign oracle](01-dualc_demo.md#choosing-a-sign-oracle); [11/01 § Sources](11-dualc_field/01-op-vocabulary.md#sources) |
| Sign oracles for a clean or almost-clean mesh | `--pseudonormal`, `--gwn`; `mesh(sign=…)` | [01](01-dualc_demo.md#choosing-a-sign-oracle); [11/01](11-dualc_field/01-op-vocabulary.md#sources) |
| Hollow a mesh into a shell | `--recipe=mesh-shell`; `--onion T`; `onion(mesh(…))` | [05](05-dualc_csg_demo.md#recipes) R4; [02](02-dualc_primitive.md#the-6-decorators--one-by-one) D3; [11/04](11-dualc_field/04-workflow-open-surface.md#recipes) W5 |
| Accelerate a huge mesh boolean by baking | `--bake N` | [03](03-dualc_boolean.md#the-options-in-detail) |
| Text shorthand, underscore aliases, positional params | `--expr` | [11/01 § Text shorthand](11-dualc_field/01-op-vocabulary.md#text-shorthand) |
| Graph input forms: JSON file, `.fld` shorthand file, stdin `-`, canonicalise, list the vocabulary | `--dump-json`, `--list` | [11/README § Synopsis](11-dualc_field/README.md#synopsis); [11/01](11-dualc_field/01-op-vocabulary.md#choosing-an-input-form-json-file-vs---expr-vs-stdin) |
| Quote a mesh path inside `--expr` on Windows PowerShell 5.1 | `mesh(path=\"…\")` | [11/01 § `--expr` and shell quoting](11-dualc_field/01-op-vocabulary.md#--expr-and-shell-quoting); the rule: [design/09](../design/09-conventions.md#windows-powershell-51-quoting) |
| Rigid placement by matrix | the `transform` decorator | [11/01 § Decorators](11-dualc_field/01-op-vocabulary.md#decorators--placement-one-child) |
| Post-contour decimation | `--decimate` / `--simplify` | [11/05](11-dualc_field/05-decimation.md) |
| Streaming / tiled export at one-tile RAM | `--tile-depth` | [11/06](11-dualc_field/06-streaming-tiled-export.md) |
| Pick the tile depth from a RAM budget | `--mem` | [11/07](11-dualc_field/07-mem-budget.md) |
| Slicer-native 3MF and a globally welded object | `-o .3mf`, `--weld` | [11/08](11-dualc_field/08-3mf-and-weld.md) |
| Headless PNG of a field graph; smooth orbit on a weak GPU; the WebGL2 shader | `--snapshot`, `--preview-scale`, `--es` | [12/README](12-dualc_field_view/README.md#cli-flags) |
| Inspect a lattice membrane without the volume's skin | `intersection(onion(normalize(tpms)), volume)` | [12/04](12-dualc_field_view/04-open-surface-preview.md); export it: [11/04](11-dualc_field/04-workflow-open-surface.md) |
| Cross-section of a field on a plane | `--plane`, `--res` | [07](07-dualc_slice.md) |

## GPU viewer keyboard map (`dualc_raymarch` & `dualc_field_view`)

Both opt-in GPU viewers share an orbit camera and the **section-plane** controls
(viewport-only cuts; never affect exported geometry). Keys are layout-independent —
letters matched by the printed character, values on the arrows and Page keys, the
US-position keys kept as alternates
([design/09](../design/09-conventions.md#keyboard-layout-independence)).

| Action | `dualc_field_view` | `dualc_raymarch` |
| --- | --- | --- |
| Orbit | left-drag | left-drag |
| Zoom (dolly) | scroll | scroll |
| Reset view | `r` | `r` |
| Quit | `Esc` | `Esc` |
| **Section** — toggle plane X / Y / Z | `x` / `y` / `z` | `x` / `y` / `z` |
| Section — slide active plane − / + | `←` / `→` | `←` / `→` |
| Section — flip kept side | `f` | `f` |
| Section — clear all planes | `0` | `0` |
| Select parameter to edit | `tab` | — |
| Nudge selected parameter − / + | `↓` / `↑` (or `[` / `]`) | — |
| Wavelength finer / coarser | — | `↓` / `↑` (or `[` / `]`) |
| Offset (thick-wall shell) − / + | — | `PgDn` / `PgUp` (or `-` / `=`) |
| Cycle TPMS family | — | `t` |
| Toggle thickness normalization | — | `n` |
| Reload graph from file (also automatic on file change) | `l` | — |

Per-tool detail (and the section-plane recipe):
[10-dualc_raymarch.md](10-dualc_raymarch.md#live-controls)
and [12-dualc_field_view/](12-dualc_field_view/01-controls.md#controls).

## Common options — which tool takes which

Every tool accepts `--help` (`-h`). The shared contouring options are **not**
uniform across the tools — this matrix is the truth, the per-tool pages carry the
defaults:

| Option | Meaning | `demo` | `primitive` | `boolean` | `lift` | `csg_demo` | `lattice` | `slice` | `view` | `gen_demo` | `raymarch` | `field` | `field_view` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `-o PATH` | Output; format by extension ([below](#export-formats--o-extension-dispatch)). `dualc_demo` takes its output as the second positional; `dualc_gen_demo` and `dualc_slice` write fixed formats (OBJ; PNG + SVG). | positional | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | — | ✓ | — |
| `--depth N` | Octree max depth; cell size = root extent / `2^N`. | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | `genus2` only | — | ✓ | — |
| `--collapse E` | Adaptive cell-collapse threshold (QEF energy, length²); how it works: [11/05](11-dualc_field/05-decimation.md#which-lighter-files-lever---collapse-vs---decimate-vs---tile-depth). | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | — | — | ✓ | — |
| `--bounds x0,…,z1` | Explicit root / sampling box for unbounded fields. | — | ✓ | — | — | — | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ |
| `--expr` / `-` (stdin) | Field-graph shorthand instead of a JSON file. | — | — | — | — | — | — | — | — | — | — | ✓ | ✓ |

## Resolution & thin features

Dual contouring records one surface crossing per octree cell edge, so **the
thinnest wall or gap must span ≥ ~2–3 cells** (cell ≈ root extent / `2^depth`);
if a shelled or repeated result comes out fragmented, raise `--depth` or thicken
the wall — a `--onion 0.1` shell on the default torus needs about `--depth 7`,
`--onion 0.35` is clean at `--depth 6`. The rule and why:
[design/10 § The resolution rule](../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist).

## Export formats (`-o` extension dispatch)

Every field-driven tool (`dualc_lattice`, `dualc_primitive`, `dualc_boolean`,
`dualc_lift`, `dualc_csg_demo`, `dualc_field`) chooses its output format from the
`-o` file extension, in a single contour pass:

| Extension | Format | Notes |
| --- | --- | --- |
| `.obj` | Wavefront OBJ | Smooth-shaded per-vertex normals (default). |
| `.stl` | Binary STL | Instantly printable; accepted by every slicer. |
| `.3mf` | 3MF-mesh | Deflate-compressed, ~⅓ the STL size; **1 world unit = 1 mm**. |

A *surface* TPMS is carried as a triangle mesh (3MF's beam-lattice extension is
for strut lattices only). Why the extension decides, what each format carries and
where the millimetre is fixed: [design/09 § Export-format dispatch](../design/09-conventions.md#export-format-dispatch)
and [§ Units and frames](../design/09-conventions.md#units-and-frames).

---

## Appendix — Full inventory count

| Category | Count | Items |
| --- | --- | --- |
| Tools (binaries) | 11 | `dualc_demo`, `dualc_primitive`, `dualc_boolean`, `dualc_lift`, `dualc_csg_demo`, `dualc_lattice`, `dualc_slice`, `dualc_gen_demo`, `dualc_raymarch`, `dualc_field`, `dualc_field_view` (tool 8, `dualc_view`, retired) |
| Demo meshes (`data/`) | 9 generated + 6 not | Generated by `dualc_gen_demo`: `cube`, `sphere`, `uvsphere`, `torus`, `knot`, `genus2`, `cylinder`, `bracket`, `hexbore`; not: `molde`, `foot`, `mesh-soup`, `opA`, `opB`, `bunny` ([09](09-dualc_gen_demo.md)) |
| 3D primitives | 30 | P1–P30 (see [02-dualc_primitive.md](02-dualc_primitive.md)) |
| TPMS primitives | 6 | `gyroid`, `schwarz-p`, `diamond`, `fischer-koch`, `lidinoid`, `neovius` |
| Strut lattices | 4 | `sc`, `bcc`, `fcc`, `octet` (field-graph nodes; see [11-dualc_field/ § Strut lattices](11-dualc_field/02-strut-lattices.md#strut-lattices-wireframe-crystals)) |
| 2D primitives | 4 | `circle`, `box`, `segment`, `polygon` |
| Decorators, `dualc_primitive` post-op flags | 6 (7 flags) | `--offset` ≡ `--round`, `--onion`, `--scale`, `--elongate`, `--translate`, `--rotate` |
| Decorators, field-graph only | 2 + 2 graded | `normalize`, `transform`; two-child `graded-onion` (hollow shell), `graded-offset` (solid inflation / graded strut radius) |
| Domain operators | 6 | `twist`, `bend`, `mirror`, `repeat`, `repeat-limited`, `displace` |
| Displace functions | 3 | `sine`, `gyroid`, `bumps` |
| Boolean operations | 7 | `union`, `intersection`, `difference`, `xor`, `smooth-union`, `smooth-intersection`, `smooth-difference` |
| Three-child blend | 1 | `mix` |
| 2D lifts | 2 | `revolve`, `extrude` |
| Baked recipes | 6 | `cube-minus-sphere`, `smooth-blend`, `displaced-sphere`, `mesh-shell`, `mesh-minus-sphere`, `twisted-mesh` |
