# Inspection report — gyroid-shell contouring quality & performance

Part of [12 — Field-graph & standalone raymarch app](../README.md); the evidence behind
[§ G Post-contour QEM decimation](../06-decimation.md#g-post-contour-qem-decimation---decimate----simplify).
Inspection complete 2026-07-04: investigation, measured results, recommended workflow and
a proposed engine enhancement — which shipped on 2026-07-05 as `dualc_field --decimate` /
`--simplify` (Approach A, monolithic; the per-tile Approach B stays deferred, row D-26 of
the [decisions index](../../../decisions/README.md)); usage in
[`command_reference/11-dualc_field/`](../../../command_reference/11-dualc_field/05-decimation.md#decimation---decimate----simplify).
All numbers were measured on this repo's CLIs (16 GB / 8-core Win10 dev box); the
reproduction commands are in the [Methods appendix](#appendix--methods--reproduction).

*(Moved on 2026-09-18 from `docs/report-quality-inspection-contouring.md`, unchanged apart
from this intro, two retargeted paths and the split into three pages —
[19 Phase 5](../../19-docs-layers/README.md).)*

## The pages of this report

| Page | Sections |
| --- | --- |
| this README | the test case · executive summary · root-cause diagnosis · recommended workflow · verification · methods |
| [Measurements](01-measurements.md) | Steps 1–4b, the ≈2× wall rule, workflow optimization: structural limits and the feasible win (QEM decimation) |
| [Decimation approaches](02-decimation-approaches.md) | the proposed `--decimate` / `--simplify` enhancement; Approach A (monolithic) vs B (per-tile streaming) |

## The test case

A part built as a **sphere (r = 25 mm)** boundary filled with a **gyroid TPMS
(wavelength 4.3)** wrapped in an **onion shell (thickness 0.9 mm)**, `normalize`d —
i.e. the shipped `examples/samples/gyroid_box.json` idiom with a sphere clip:

```
intersection( sphere(radius=25), onion( normalize(gyroid(wavelength=4.3)), thickness=0.9 ) )
```

Reported symptoms:

- **Depth 6:** the result is *completely messed up*.
- **Depth 8:** the file is **~230 MB** and slow to write, and the gyroid boundary
  curves are covered in **small saw-tooth dents**.

Constraints confirmed with the requester: goal is a **printable/manufacturing**
mesh; the field **does** use `normalize`; **λ = 4.3 / thickness = 0.9 must stay
exact** (density cannot be loosened). The live GPU preview (`dualc_field_view`,
which sphere-traces the SDF directly) looks **perfect** — confirming the field is
correct and the defect is purely in mesh extraction.

---

## Executive summary

- **Root cause is geometric, not a bug.** The part is a **78%-solid block** whose
  binding feature is a **0.41 mm void** between gyroid sheets (the walls, ≈1.8 mm,
  are thick and well-resolved). Dual contouring needs ~2–3 cells across that void,
  i.e. cell ≤ 0.16 mm → **depth 9** on the 52 mm domain. Depth 6 (0.8 mm cell) puts
  the void *sub-cell* → sheets fuse → "messed up." Depth 8 (0.2 mm cell) is 2
  cells/void → marginal → dents. Depth 9 (0.1 mm cell) is clean.
- **The 230 MB is expected**, not a defect: the octree refines uniformly to max
  depth across the whole space-filling shell (5.2 M faces at depth 8).
- **The saw-tooth on the "boundary curves" is the sphere rim** (a curved
  feature-curve the per-cell QEF cannot place a vertex along) — measured 30× more
  sharp edges at the rim than the interior. **But its amplitude is small**
  (~0.15 mm at depth 8, ~0.07 mm at depth 9) and **not tunable** via the exposed
  QEF knobs; it is a structural DC limitation, largely cosmetic for FDM.
- **Recommended workflow: depth 8 + QEM mesh decimation (~10×)** →
  ~0.5 M faces, ~10–15 MB, ~0.04 mm error, ~4 min — **lighter *and* faster than
  depth 9**. Octree `--collapse` is structurally dead here (6.5%); decimation is the
  real "adaptively lighter files" lever (demonstrated in [Measurements](01-measurements.md#the-feasible-win--qem-mesh-decimation)).
- **Proposed engine enhancement:** a `dualc_field --decimate RATIO | --simplify ERR`
  flag backed by `meshoptimizer` (MIT) in `examples/third_party/`.

---

## Root-cause diagnosis (from code)

Three **coupled** failures. All three scale with cell size, which is why depth 6 is
garbage, depth 8 is dented-but-heavy, and no single depth is clean-and-cheap.

### 1. The void gap, not the wall, is the binding feature — and it is under-resolved

Dual contouring must resolve **both** the solid wall **and** the empty gap between
adjacent gyroid sheets; the binding feature is `min(wall, gap)`.

- `onion(f, t)` = `|f| − t`, so the solid **wall ≈ 2·thickness ≈ 1.8 mm**
  (`src/implicit/decorators.cpp:48`; the "≈2·thickness" rule in
  [`command_reference/11-dualc_field/`](../../../command_reference/11-dualc_field/README.md)). With `normalize` on, this 1.8 mm is
  genuinely metric.
- Adjacent gyroid sheets sit ≈ λ/2 ≈ 2.15 mm apart, so a 1.8 mm wall leaves only a
  **hairline void gap** (measured at ≈0.41 mm — see [Step 1](01-measurements.md#step-1--field-geometry-analytic-onionnormalizegyroid-43-t09)). That gap is the
  smallest feature in the part.
- The sampler records **at most one crossing per octree edge, detected only by a
  corner-sign change** (`src/sampler.cpp:45-49`, `if (sa == sb) continue`;
  one-crossing `HermiteEdge` in `include/dualc/hermite_octree.h:13-24`). An edge
  passing *through a sub-cell gap* (out→in→out) has equal end signs → **no crossing
  recorded** → the two sheets **fuse**. There is no edge sub-sampling and no
  min-feature guard.
- **Depth 6** (cell ≈ 0.82 mm ≫ gap): gaps collapse → "completely messed up."
  **Depth 8** (cell ≈ 0.20 mm ≈ half the gap): gaps *barely* resolve → dents at the
  resolution limit. A 1.8 mm wall at depth 6 is ~2.3 cells/wall (marginally viable)
  — so the wall is **not** the problem; the gap is.

### 2. Saw-tooth on the sphere-clipped rim — ill-conditioned per-cell QEF on a curve

- The rim is a plain hard `max` (`IntersectionField`, `src/implicit/combinators.cpp:74-100`);
  the contourer has **no feature-curve handling**. A rim cell's QEF holds
  gyroid-plane + sphere-plane constraints whose intersection is a **line** → a ~1-D
  null space along the rim.
- The QEF SVD truncates that direction (`qefRegularization = 0.1`,
  `include/dualc/contourer.h:19`; `src/contourer.cpp:30-32`) and falls back to the
  grid-dependent **mass point**; `clampVertexToCell` (`contourer.h:21-22`) then
  quantises it toward a cell face (`src/contourer.cpp:119-133`).
- `IntersectionField::gradientAt` returns the *active operand's* gradient
  (`combinators.cpp:82-85`), so across the intersection kink neighbouring cells feed
  **categorically different normals** (gyroid-tangent vs sphere-radial) → vertices
  lurch. ([Step 4](01-measurements.md#step-4--quantify-the-rim-contribution-which-knob-if-any) refines *which* of these mechanisms actually dominates — it is the
  null-space/mass-point, not the clamp.)

### 3. 230 MB / slow = dense uniform grid (known RAM wall)

- `GyroidField::cellOverlaps` is hard-wired **`true`** (`src/implicit/primitives_tpms.cpp:34,54`)
  and every wrapper forwards it, so **every cell inside the sphere refines to
  `maxDepth`** — a dense uniform grid.
- The onion has **two walls** (~doubling the contoured area) and **cell collapse is
  off by default** (`simplificationError = 0.0`, `contourer.h:20`). A depth-8 doubled
  shell over a 50 mm sphere is ~5 M triangles ≈ 250 MB — expected, per the density
  wall in [roadmap 11](../../11-dense-lattice-deliverable/README.md).

---

## Recommended workflow

**Primary — depth 8 + QEM decimate ~10×** (lighter *and* faster than depth 9):

1. Contour at depth 8 (2 cells/void; watertight, F/V 2.010).
2. QEM-decimate to ~10% (target ~0.04 mm error, below FDM resolution).
3. Result: ~0.5 M faces, ~10–15 MB, ~4 min total — decimation also smooths depth-8's
   roughness and the rim jitter.

**When depth-8 roughness (0.12 mm interior) is visible / for high-res processes
(SLA/DLP/CNC):** use **depth 9, streamed**, then optionally per-tile decimate:

```
dualc_field <graph> --depth 9 --tile-depth 7 -o part.3mf        # bounded RAM, clean
dualc_field <graph> --depth 9 --tile-depth 7 --weld -o part.3mf # + topology (FEA / re-boolean)
```

`--tile-depth (depth−2)` holds peak RAM to one tile; `.3mf` is ~⅓ the STL size with
`1 unit = 1 mm`. Geometry-identical to the monolithic mesh.

**Interactive inspection:** preview the same graph in `dualc_field_view` (GPU
raymarch) — no mesh, no dents, no file-size wall — and reserve the tiled contour for
the final export.

**Do not** reach for octree `--collapse` (dead here) or expect a rim fix from
`--qef-reg`/`--no-clamp` tuning (Step 4 refutes it).

---

## Verification

- **Closure:** each export's **F/V → ~2.0** (watertight) — the certification metric
  from [05 #20](../../05-tpms-lattices/README.md). F/V well below 2 = holes/fragments.
- **Visual:** the depth-9 tiled `.3mf` (or a decimated depth-8 mesh) opened in a
  slicer / `dualc_field_view` shows the void gaps open (no fused blob) and smooth
  walls; residual rim teeth quantified by Steps 2/4b.
- **Memory:** the tiled run completes with peak RAM ≈ one tile (not tens of GB) —
  proof the streaming path broke the wall the 230 MB/depth-8 run was hitting.
- **Decimation error:** sample the field at output vertices (Step-1 method); p99
  ≤ ~0.05 mm confirms sub-print-resolution fidelity.

---

## Appendix — methods & reproduction

All CLIs from `build/examples/Release/`. Field constant throughout:
`intersection(sphere(center=[0,0,0],radius=R),onion(normalize(gyroid(wavelength=4.3)),thickness=0.9))`.

- **Step 1 gap measurement** — `dualc_slice <cube.obj> --type gyroid --wavelength 4.3
  --offset 0.9 --normalize-thickness --res 2048 --bounds …` for the cross-section
  PNG/SVG; the wall/void chord widths and 78% solid fraction were cross-checked by an
  analytic scan of `|normalize(gyroid)| < 0.9` across 5 z-planes (`k = 2π/4.3`,
  central-difference `|∇f|`).
- **Steps 2 / 4 / 4b roughness & amplitude** — contour with `dualc_field --expr … --depth
  D --bounds …`, export `.obj`, then compute per-edge dihedral angle (sharp = >60°
  normal jump) and per-vertex neighbor-average deviation (mm), binned by radius
  (rim band vs interior). Box-clip used `box(min=…,max=…)` strictly inside the sphere
  at identical `--bounds`.
- **Step 3 sweep** — depths 6–8 monolithic (`-o .obj`/`.stl`), depth 9
  `--tile-depth 7 --weld -o .3mf`; V/F from the tool's report / a `.3mf`
  vertex+triangle count; F/V = F÷V.
- **Step 4 knob study** — throwaway `--no-clamp` (`clampVertexToCell=false`) and
  `--qef-reg V` (`qefRegularization`) flags were added to `examples/dualc_field.cpp`
  for the run and **reverted** (working tree left clean).
- **Decimation demo** — `fast_simplification.simplify(V, F, target_reduction=1−keep)`
  on the clean r=8 depth-7 mesh; error = `|max(‖p‖−8, |normalize(gyroid(p))|−0.9)|`
  at each decimated vertex, reported as p99/max in mm.

Key code references: `src/sampler.cpp:45-49` (single-crossing edge gate),
`src/contourer.cpp:78-138` (QEF solve + clamp), `include/dualc/contourer.h:18-29`
(`ContourerParams` defaults), `src/implicit/decorators.cpp:41-75` (`onion`),
`src/implicit/combinators.cpp:74-100` (`intersection`),
`src/implicit/primitives_tpms.cpp:34-54` (`gyroid`, `cellOverlaps=true`).

---

← Back to the [topic README](../README.md) · the [Roadmap index](../../README.md).
