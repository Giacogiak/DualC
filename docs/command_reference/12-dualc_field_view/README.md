# Tool 12: `dualc_field_view` (field-graph GPU raymarch viewer)

Standalone GPU viewer that **compiles an arbitrary field-graph to GLSL and
sphere-traces it** — the general successor to `dualc_raymarch` (opt-in build).

```
dualc_field_view <graph.fld|graph.json|-> [--bounds x0,y0,z0,x1,y1,z1]
                 [--grid-res N] [--preview-scale S] [--es] [--snapshot PATH]
dualc_field_view --expr "onion(gyroid(wavelength=0.3),thickness=0.05)" [...]
```

Where `dualc_raymarch` hand-codes one TPMS-in-mesh field, this takes the **same
field-graph** `dualc_field` contours for export (`.fld` shorthand or `.json`, or
`--expr`/stdin) and lowers it to a generated `float sceneSDF(vec3 p)` via the
field→GLSL codegen, then sphere-traces that. So **the field you preview is the
field you export** — one source of truth, no mesh round-trip. RAM is bounded by
the window, not the lattice, so it renders densities that OOM the contourer.

The compiler emits one pure `float fN(vec3 p)` per graph node (point flows down
through domain warps, distance up) calling a fixed GLSL prelude that is
bit-faithful (within float32 + GPU-trig tolerance) to the C++ field formulas —
enforced by the `dualc_glsl_parity` gate. Mesh sources are the only approximate
part: they bake to a narrow-band `GL_R32F` 3D texture (`sampler3D`) for preview;
**export stays exact**.

**Opt-in build** (`-DDUALC_BUILD_FIELD_VIEW=ON`, the
[build prelude](../README.md#build-prelude)): a custom-GL app on GLFW (fetched at its
pinned release, or `-DDUALC_GLFW_DIR`) and the vendored glad loader; `libdualc` is unaffected. It needs a GL window,
so it has no CTest — `--snapshot` renders one frame headless for scripted checks.

## CLI flags

| Flag | Default | Meaning |
| --- | --- | --- |
| `<graph>` | (none) | A `.fld` (text shorthand) or `.json` (canonical) field-graph, or `-` for stdin. The same files `dualc_field` accepts. |
| `--expr STR` | — | Inline text shorthand instead of a file. |
| `--bounds x0,y0,z0,x1,y1,z1` | the field's own `bounds()` + 5% | Raymarch box. An **infinite** field (`plane`, `repeat`, …) has no finite bounds and **requires** this. |
| `--grid-res N` | `96` (max 256) | Mesh-source bake resolution per axis (the `sampler3D` clip). The bake region **is** the raymarch box, so this composes with `--bounds`: zooming in concentrates the same grid on the region you are inspecting instead of spreading it over the whole mesh AABB. `256³` R32F is 64 MB per unique mesh node. The compile line reports the resolution *and* the resulting world cell size. |
| `--preview-scale S` | `0.5` (range `0.1`–`1.0`) | **Adaptive resolution** while the view is moving (see below). `1.0` disables it (always full-res). |
| `--es` | off | Emit a `#version 300 es` shader (the WebGL2-portable subset) instead of desktop `330 core`. A portability check; the shader body is identical, only the version/precision header differs. |
| `--snapshot PATH` | — | Render one frame headless to a PNG and exit (no window). |
| `--help`, `-h` | — | Usage. |

There is **no `--depth`** and **no `-o`** (nothing is contoured — use
`dualc_field` for export). Sampling/contour settings are not graph nodes.

### Adaptive resolution (smooth orbit on heavy scenes)

Tracing is per pixel per frame, so a dense or tapered lattice can drop to
single-digit FPS on an integrated GPU while you orbit. `--preview-scale S` renders
at `S ×` the window resolution while the camera or a parameter is moving and
re-renders at full resolution once the view settles (~0.2 s), so the still image
is always crisp; idle frames drop to ~10 Hz. Default `0.5`; lower it (`0.35`,
`0.25`) for a heavy graph on weak hardware, `1.0` disables it. Thin struts shimmer
during motion at `S < 1` — that is the motion preview. It never touches the field,
the codegen or the export, and `--snapshot` always renders at full resolution. The
measurements and the design:
[12/04 § Adaptive-resolution preview](../../roadmap/12-field-graph-and-app/04-preview-performance.md#adaptive-resolution-preview--metric-sdf-fast-path).

**Metric SDF fast-path (automatic, no flag).** When the whole graph is a true
distance field (struts, analytic primitives, hard booleans, metric decorators — no
TPMS, smooth boolean, `twist`/`bend`/`displace` or baked `mesh`/`winding`) the
tracer steps by the distance directly and skips the per-step gradient sampling —
about 7× fewer SDF evaluations, which is why strut lattices and analytic CSG
preview far faster than a normalized-TPMS shell. Baked sources keep the
conservative step and measurably do not pay for it
([12/07 § H1](../../roadmap/12-field-graph-and-app/07-mesh-preview-sweep.md#h1-the-metric-fast-path-traced-a-field-that-is-not-a-distance-field)).

**Heavy scenes and hybrid laptops.** A tapered `octet` on an integrated-only GPU
takes a few seconds to compile the shader on open — a known limitation,
[12/04 § Viewer startup](../../roadmap/12-field-graph-and-app/04-preview-performance.md#viewer-startup--shader-compile-time); navigate it at
`--preview-scale 0.35`. On a machine with both an integrated and a discrete GPU the
viewer runs on the discrete one with no Control-Panel setup — confirm from the
`[dualc_gl] GL renderer: …` line at startup
([12/04 § Discrete-GPU auto-selection](../../roadmap/12-field-graph-and-app/04-preview-performance.md#discrete-gpu-auto-selection-hybrid-graphics-laptops)).

## The pages of this tool

The reference for `dualc_field_view` is split by concern; this page holds the flags and
the resolution model.

| Page | What it covers |
| --- | --- |
| [Controls](01-controls.md) | Everything you press: the orbit and section-plane keys, how `tab` + nudge edits a parameter live (and why some edits recompile), reload vs reset, the viewport-only cuts. |
| [Supported nodes and worked examples](02-nodes-and-examples.md) | The node vocabulary (the same as `dualc_field`'s) and recipes P1–P16: composed CSG, mesh sources, graded and tapered lattices, dense previews, headless snapshots. |
| [Recipes by node category (the full vocabulary)](03-recipes-by-node.md) | One ready-to-run preview per node family, N1–N19: the primitive parameter forms, the domain operators, `winding` soups, DAG dedup. |
| [Isolating a TPMS surface inside a volume (meshless preview)](04-open-surface-preview.md) | The thin-shell recipe `intersection(onion(normalize(tpms)), volume)` that shows a lattice membrane without the volume's skin, O1–O5. |

## Verifying preview == export

The previewed field and the exported mesh are built from the **same** parsed
graph, so they agree by construction, and the GLSL prelude is held to the C++
formulas by the `dualc_glsl_parity` harness (`-DDUALC_BUILD_GLSL_PARITY=ON`, run by
hand — it needs a GL context; `python scripts/check.py --gpu` runs it) — the
invariant is [design/10](../../design/10-invariants-and-tolerances.md#output-invariants). To cross-check a specific graph,
contour it and compare:

```bash
dualc_field      mygraph.fld -o ref.stl     # exact, contoured
dualc_field_view mygraph.fld                 # raymarched preview of the same field
```

---

← Back to the [Command Reference index](../README.md) · see also
[`11-dualc_field/`](../11-dualc_field/README.md) (export) and
[`10-dualc_raymarch.md`](../10-dualc_raymarch.md) (the single-TPMS predecessor).
