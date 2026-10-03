# Decimation (`--decimate` / `--simplify`)

Part of [Tool 11: `dualc_field`](README.md). Post-contour QEM decimation: choosing the
knob, how far it can go (measured), which lighter-files lever to reach for, recipes,
limits.

Dual contouring emits **one vertex per cell**, so a dense part is tessellated at
the uniform grid resolution everywhere — even across the smooth-curved gyroid
walls whose curvature radius is far larger than the cell. That makes the file
heavier than the geometry needs. `--decimate` runs a global **QEM (quadric error
metric) mesh simplification** — via vendored [meshoptimizer](https://github.com/zeux/meshoptimizer)
(MIT, host-side only) — on the finished mesh, *before* export:

```bash
# depth-8 gyroid shell, then ~10x lighter at sub-print-resolution error:
dualc_field part.fld --depth 8 --decimate 0.1 -o part.stl
# or bound the geometric error instead of the ratio (0.05 mm here):
dualc_field part.fld --depth 8 --simplify 0.05 -o part.stl
```

This is the recommended **"depth 8 + decimate ~10×"** workflow from the
[contouring-quality inspection](../../roadmap/12-field-graph-and-app/08-quality-inspection-gyroid-shell/README.md): on
the reference sphere-clipped gyroid it yields ~0.5 M faces at ~0.04 mm p99 error
— *lighter and faster than* contouring at depth 9, and the QEM pass also smooths
the depth-8 roughness.

#### `--decimate` vs `--simplify` — choosing the knob

**They are the same geometric operation.** Both run one QEM (quadric error
metric) pass that repeatedly **collapses edges** — merging pairs of triangles,
preferentially where the surface is flat/low-curvature, so detail is kept where
it matters. There is no difference in *what* they do to the mesh. The only
difference is the **stopping rule**: which quantity you pin down, and which one
you let float.

| | `--decimate R` | `--simplify E` |
| --- | --- | --- |
| **You fix** | **output size** — keep fraction `R` ∈ (0,1) of the triangles | **accuracy** — max deviation `E`, in world units (mm) |
| **Floats freely** | the resulting error | the resulting triangle count |
| **In plain terms** | "make it *this light*, I'll accept whatever accuracy that costs" | "keep deviation *under E mm*, and be as light as that allows" |
| **Predictable** | file size / triangle count | geometric fidelity |
| **Varies per part** | accuracy | file size |

So it is one lever seen from two ends: `--decimate` targets a **count** and
reports the error it reached; `--simplify` targets an **error ceiling** and
reports the count it reached. They are mutually exclusive, and both print the
achieved values so you can read back the one that floated.

**Which to choose — pin down the variable you can't let float:**

- **`--decimate` — when your constraint is size.** A file-size / triangle / GPU
  budget, or you have empirically found "`0.1` is fine for my parts" and want the
  *same* ratio applied reproducibly. Risk: a fixed ratio ignores geometry — it can
  over-thin a part with fine features (past acceptable error) or leave a simple
  part heavier than needed.
- **`--simplify` — when your constraint is a tolerance.** A process resolution
  (FDM nozzle ~0.4 mm, SLA/DLP ~0.05 mm, a CNC tolerance), an FEA element size, or
  a dimensional-fit requirement. It **guarantees** the surface never moves more
  than `E` mm and gives the lightest mesh that respects that — light on smooth
  parts, heavier on detailed ones, automatically. This is the right engineering
  choice when *accuracy*, not size, is the real requirement.

**Rule of thumb:** prototyping / visualization / a known-good ratio → `--decimate`;
manufacturing to a spec → `--simplify`.

> **What `E` is measured against.** The `--simplify` error is the decimated mesh's
> deviation from the **contoured (pre-decimation) mesh**, not from the true
> analytic surface — contouring has its own error on top. So `--simplify 0.03`
> means "decimation adds ≤ 0.03 mm," not "the final mesh is within 0.03 mm of the
> ideal SDF." (The true-surface figures in the next table are stricter: they
> sample the analytic field directly at each output vertex.)

#### How far can you decimate? (measured)

A gyroid wall's curvature radius is far larger than the tessellation, so QEM has
enormous slack to merge before it deforms the surface. **`0.1` is the sweet spot**:
10× lighter with the true-surface p99 error still below FDM nozzle+layer
resolution, and a *lower* max error than the undecimated mesh (it smooths the worst
jitter); on the full depth-8 reference part it is watertight at ~0.05 mm p99. The
measured table (faces, p99 and max at `0.25` / `0.1` / `0.05`) is in the
[inspection report](../../roadmap/12-field-graph-and-app/08-quality-inspection-gyroid-shell/01-measurements.md#the-feasible-win--qem-mesh-decimation).

#### Which "lighter files" lever? `--collapse` vs `--decimate` vs `--tile-depth`

Three different knobs touch mesh weight; they are **not** interchangeable:

| Lever | Acts on | Wins on | Doesn't help |
| --- | --- | --- | --- |
| `--collapse E` | the **octree**, pre-mesh (merges QEF-flat cells) | large flat/planar regions | **dense TPMS** — a gyroid is high-frequency everywhere, so collapse finds ~6.5% and no time saving |
| **`--decimate` / `--simplify`** | the **finished mesh** (QEM triangle merge) | **smooth-curved** walls whose radius ≫ cell — exactly a TPMS shell | doesn't lower peak RAM (it runs *after* the full mesh exists) |
| `--tile-depth D` (or [`--mem BUDGET`](07-mem-budget.md#auto-budget---mem-budget)) | **peak RAM** (streams tiles) | parts too big to hold in RAM at all | doesn't shrink the *file* — same face count, seam verts duplicated |

`--decimate` and `--tile-depth` optimize **different walls** (file size vs. RAM)
and cannot be combined; see the limitation below.
(`--mem` is just the auto-picker for `--tile-depth`, so it inherits the same
RAM-vs-file distinction and the same no-`--decimate` rule.)

**How `--collapse` works, and when to use it.** `--collapse E` is an
**octree-level** simplification that runs *during* contouring, before the triangle
mesh exists. As the contourer walks the octree bottom-up, wherever a block of 8
sibling cells is flat enough that a **single** QEF vertex represents them within
error `E`, it **merges those cells** — emitting one
coarse vertex instead of eight fine ones (adaptive Ju/Schaefer/Warren DC). Because
it acts on the octree, it also **lowers peak RAM and contour time**, not just the
face count — the one thing `--decimate` can't do.

> **What `E` measures.** `E` is compared against the merged QEF's *geometric
> energy*: the summed squared distance from the candidate merged vertex to the
> planes its Hermite samples define. It is a **sum**, so it grows with the
> number of merged samples — a value tuned at one `--depth` does not transfer
> unchanged to another, and a deeper octree tolerates the same shape at a
> smaller `E`. Sweep it: raise `E` until the error shows, then back off. (The
> quantity compared changed once, with the measured effect, in
> [01/02 § 4.9](../../roadmap/01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch).)

- **Recommended for parts with genuine flat / low-curvature regions:** mechanical
  brackets, housings, plates, boolean results with planar faces — anywhere large
  areas are near-planar, a few merged cells replace thousands of coplanar triangles
  at essentially zero geometric cost. Start around `E = 0.01`–`0.05` (world units)
  and raise it until you see error you don't want.
- **Not worth it on dense high-frequency fields (TPMS / gyroid / any space-filling
  lattice):** the surface curves on every cell, so almost no 8-cell block is ever
  flat — measured ~6.5% here, no time saved. Use `--decimate`/`--simplify` instead
  (they exploit *smooth curvature*, which collapse cannot).
- **They compose.** `--collapse` (flats, during contour) and `--decimate`/`--simplify`
  (curves, after) are complementary and may be combined in the monolithic path —
  collapse coarsens the flat regions cheaply, then QEM thins the curved ones. Only
  `--collapse` + `--tile-depth` is rejected (per-tile collapse would crack seams).
- Default is `E = 0` (off). It is **topology-safe** — it only merges where the
  error budget allows, so it never opens holes.

## Recipes

| # | What it does |
| --- | --- |
| Q1 | The primary FDM workflow: depth 8, `--decimate 0.1`, watertight STL |
| Q2 | Tolerance-driven (`--simplify 0.03`) |
| Q3 | Decimated slicer-native 3MF |
| Q4 | Decimated `.obj` for rendering (normals recomputed) |
| Q5 | Size/quality sweep over three ratios |

```bash
# Q1 — The primary FDM workflow: dense shell, depth 8 + ~10x lighter, watertight
#    STL. ~0.5 M faces / ~10-15 MB / p99 ~0.05 mm -- lighter AND faster than depth 9.
dualc_field part.fld --depth 8 --decimate 0.1 -o part.stl

# Q2 — Tolerance-driven (SLA/DLP/CNC, or an FEA element budget): cap the geometric
#    error instead of the ratio -- the mesh is as light as 0.03 mm allows.
dualc_field part.fld --depth 8 --simplify 0.03 -o part.stl

# Q3 — Slicer-native 3MF, decimated. Good default hand-off to a printer.
dualc_field part.fld --depth 8 --decimate 0.1 -o part.3mf

# Q4 — Decimated .obj for rendering / inspection -- the OBJ writer recomputes
#    angle-weighted vertex normals so smooth shading survives the QEM pass.
dualc_field part.fld --depth 8 --decimate 0.1 -o part.obj

# Q5 — Quick size/quality sweep before committing: try a few ratios (each prints
#    the face count + achieved error).
for r in 0.25 0.1 0.05; do
  dualc_field part.fld --depth 8 --decimate $r -o "part_$r.stl"
done
```

#### Notes and limits

- **Watertight in/out.** The contoured mesh is a closed 2-manifold and QEM
  preserves closure (no new holes). A small number of **non-manifold edges** can
  appear on aggressive ratios of high-genus surfaces (≈200 of ~780 k at the r=25
  `0.1` gate); the mesh stays hole-free and every slicer repairs these
  automatically. Raise `R` (e.g. `0.15`) or use `--simplify` with a tighter
  ceiling if a downstream needs strict 2-manifoldness.
- **F/V rises above 2.0** after decimation on a TPMS shell — that is expected,
  not a defect: a gyroid is **high genus**, and `F = 2V − 4 + 4g`, so shrinking
  `V` while preserving the handles raises the ratio. Closure is verified by
  **zero boundary edges**, not by F/V ≈ 2 (which only holds near genus 0).
- The `.obj` writer emits recomputed angle-weighted vertex normals for the
  decimated mesh; `.stl`/`.3mf` carry per-face normals.
- **Monolithic only.** The whole mesh is built in RAM, then
  decimated, so this covers parts that fit at contour time (~depth 8 for a dense
  shell; depth-9 monolithic OOMs *before* decimation runs). It **cannot** combine
  with `--tile-depth` — the CLI rejects the pair:
  `[dualc_field] --decimate/--simplify cannot be combined with --tile-depth (per-tile locked-border decimation is not yet implemented)`.
  Per-tile decimation with locked seam vertices (for depth 9+ at bounded RAM) is
  not implemented — the two paths are compared in
  [12/08 § Decimation approaches](../../roadmap/12-field-graph-and-app/08-quality-inspection-gyroid-shell/02-decimation-approaches.md#decimation-monolithic-vs-per-tile-streaming--the-two-implementation-paths);
  until then, for depth-9 fidelity use `--tile-depth` *without* decimation, or
  preview live in `dualc_field_view`.

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
