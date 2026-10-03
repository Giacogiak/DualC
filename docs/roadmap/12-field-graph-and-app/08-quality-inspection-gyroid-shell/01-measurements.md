# Measurements

Part of the [gyroid-shell inspection report](README.md) (roadmap 12/08): the measured
steps behind its executive summary, and the workflow optimization they support.

## Measured results

### Step 1 — field geometry (analytic `onion(normalize(gyroid))`, λ=4.3, t=0.9)

- **Solid volume fraction ≈ 78%** — a near-solid block, *not* a lightweight lattice.
- **Binding feature = the void: min ≈ 0.41 mm** (median 0.87 mm). Min solid neck
  ≈ 1.26 mm. Confirmed on a `dualc_slice` z=0 cross-section (isolated triangular void
  pockets in a solid matrix) and by analytic chord scans across 5 z-planes.
- **Required resolution:** cell ≤ void/2.5 ≈ 0.16 mm. On the full sphere (extent
  ≈ 52 mm): **depth 9** (cell 0.10 mm ≈ 4 cells/void). This explains the symptom
  exactly — **depth 8** = 0.20 mm cell = ~2 cells/void (marginal → dents); **depth 6**
  = 0.82 mm cell ⇒ void *sub-cell* → collapses → "completely messed up."

### Step 2 — box-clip vs sphere-clip at matched grid

(r=8 domain, depth 7, cell 0.131 mm ≈ 3 cells/void; both close watertight,
F/V = 2.004 / 2.005.) Per-edge dihedral roughness (fraction of edges with a >60°
normal jump):

| Region | Sharp-edge % |
| --- | --- |
| Sphere-clip **interior** walls (r<6.8) | **0.09%** (smooth) |
| Sphere-clip **rim** band (r≈8) | **2.72%** (**30× rougher**) |
| Box-clip interior (flat, grid-aligned) | **0.09%** (smooth) |

**Verdict:** once the void is resolved (≥3 cells), interior walls are smooth in
*both* clips — §1 is a pure resolution problem. The saw-tooth is **the sphere rim**
(§2), 30× elevated and absent under a flat box clip. "The boundaries of the gyroid
curves full of dents" = the curved sphere∩gyroid intersection curve grid-aliasing.

### Step 3 — depth sweep on the full r=25 sphere

(Fixed 52 mm dyadic domain, so cell = 52/2^depth. d8/d9 measured this run.)

| Depth | Cell | Cells/void | Verts | Faces | F/V | Output | Time |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 6 | 0.81 mm | 0.5 (sub-cell) | 134,580 | 289,476 | **2.151** | 78 MB obj | s |
| 7 | 0.41 mm | 1.0 | 621,384 | 1,269,972 | **2.044** | 349 MB obj | s |
| 8 | 0.20 mm | 2.0 | 2,581,092 | 5,189,100 | **2.010** | **247 MB stl** | 3m28s |
| 9 | 0.10 mm | 4.0 | 10,437,942 | 20,902,092 | **2.0025** | 279 MB **welded 3mf** | 11m35s |

**F/V converges monotonically to 2.0 as the void is resolved** (2.151 → 2.044 →
2.010 → 2.0025), confirming the void is the closure driver. **Depth 8 reproduces the
reported ~230 MB** (247 MB STL) and is *nearly* closed (F/V 2.010) — its dents are
geometric roughness, not topological holes. **Depth 9 is the clean production
result:** a single globally-manifold welded object (10.4 M V / 20.9 M F), streamed
in **125 tiles (64 non-empty) at one-tile peak RAM** — depth-9 monolithic would be
~20 M faces and OOM. Cost: ~12 min, 279 MB.

### Step 4 — quantify the rim contribution (which knob, if any?)

Re-ran the r=8 sphere-clip (cell 0.131 mm, clean interior) varying
`clampVertexToCell` and `qefRegularization` (throwaway `--no-clamp` / `--qef-reg`
flags, added then reverted). Rim-band vs interior sharp-edge %:

| Variant | RIM sharp% | INT sharp% | maxR |
| --- | --- | --- | --- |
| baseline (clamp on, qef 0.1) | 2.72% | 0.09% | 8.03 |
| `--no-clamp` | 2.94% | 0.13% | 8.01 |
| `--qef-reg 0.01` | 5.58% | 6.37% | 8.03 |
| `--qef-reg 0.001` | 9.34% | 16.01% | 8.07 |

**Findings:**
- **The cell-clamp is NOT the cause.** Disabling it *slightly worsens* the rim
  (2.72→2.94%) and does not spike vertices (maxR ≈ 8.0). The clamp is
  benign/stabilising.
- **The pinv truncation (`qefRegularization=0.1`) is load-bearing, not too
  aggressive.** Loosening it explodes roughness *everywhere* (interior
  0.09%→6.4%→16%), because a gyroid's near-flat/thin cells are near-singular
  everywhere; the 0.1 default is correctly chosen.
- **Conclusion: the rim saw-tooth is not tunable via the exposed knobs.** It is a
  structural DC limitation — a per-cell QEF cannot place a vertex along a
  feature-curve tangent it does not constrain. A real fix must be structural
  (feature-curve-aware placement), not a parameter tweak.

### Step 4b — rim tooth AMPLITUDE (mm), not just the sharp-edge fraction

The 30× rim sharp-edge *fraction* is not 30× tooth *height*. Neighbor-average
deviation (jitter proxy, mm) on the r=8 ablation (cell 0.131 mm):

| Region | dev p99 | dev max | per cell |
| --- | --- | --- | --- |
| Interior (intrinsic DC roughness floor) | 0.078 mm | 0.104 mm | 0.59 cell |
| Rim | 0.096 mm | 0.190 mm | 0.73 cell |

The rim jitter is only **~1.2–1.8× the intrinsic DC roughness present on every
wall**. Because the QEF vertex is **clamped to its cell**, tooth amplitude scales
with cell size (≈0.73·cell at p99):

- **depth 8** (the reported 230 MB export): rim teeth ~**0.15 mm** p99.
- **depth 9** (production): rim teeth ~**0.07 mm** p99 — at/below FDM nozzle+layer
  resolution and barely above the floor.

Direct measurement of the **depth-8 regime interior** (2 cells/void, cell 0.20 mm,
r=8 clip; F/V 2.010):

| Region (cell 0.20 mm) | Sharp % | dev p99 | dev max |
| --- | --- | --- | --- |
| Interior walls | 0.88% | 0.12 mm | 0.15 mm |
| Rim | 4.64% | 0.16 mm | 0.28 mm |

So depth-8 is watertight but ~1.5× rougher than depth-9 (0.12 vs 0.078 mm interior)
— usable for FDM, not pristine.

### Confirm-intent data (the ≈2× wall rule)

`thickness` sweep at λ=4.3:

| thickness | wall ≈ 2t | solid % | min void | min solid |
| --- | --- | --- | --- | --- |
| **0.9 (current)** | 1.8 mm | **78%** | **0.41 mm** | 1.26 mm |
| 0.6 | 1.2 mm | 65% | 0.42 mm | 0.98 mm |
| **0.45** | **0.9 mm** | **54%** | **0.71 mm** | 0.80 mm |
| 0.3 | 0.6 mm | 40% | 0.75 mm | 0.57 mm |

`onion` makes a shell of half-width = thickness *on each side* of the membrane, so
the solid wall ≈ 2·thickness. `thickness = 0.9` ⇒ a **1.8 mm** wall / 78% solid. If
a **0.9 mm wall** was intended, `thickness = 0.45` nearly doubles the binding void
(0.41→0.71 mm, resolvable ~one depth level lower) and gives a real 54%-solid
lattice. Left exact per the requester's instruction, but this is the single cheapest
quality+memory win if 1.8 mm walls were unintended.

---

## Workflow optimization — faster/lighter vs. cleaner-at-lower-depth

Goal: depth 9 is slow (11.6 min) and heavy (279 MB / 20.9 M faces). Two asks — **(a)**
faster + adaptively lighter files, or **(b)** a clean result at depth 7/8 — assessed
with the structural limits flagged.

### Objective structural limits

- **(b) truly-clean depth 7 is structurally blocked.** The binding void is 0.41 mm;
  at depth 7 the cell is 0.41 mm ≈ **1 void-width**, so the two walls fuse
  (single-crossing-per-edge can't separate them) — Nyquist, not a QEF issue. **Depth
  8 (2 cells/void) is the practical floor:** watertight (F/V 2.010) but interior
  roughness ~0.12 mm p99 / rim ~0.16–0.28 mm. Depth 9 (4 cells) is pristine
  (~0.078 / 0.07 mm).
- **The feature-curve fix does not help the interior** — it only touches the rim
  (~0.02–0.09 mm cosmetic excess). **Edge sub-sampling doesn't help faceting**
  either: walls are ~9 cells thick at depth 8 (well-resolved); edge sub-sampling
  fixes *detection* of sub-cell features (the depth-6 fusion), not tessellation
  quality — wrong lever for this part.
- **Faster contouring is near the floor:** sampler *and* contourer are already fully
  multi-threaded (`internal/parallelFor`, QEF pre-solved across all cores). The only
  speed lever is depth 8 vs 9, or GPU (deferred, roadmap #15).
- **Octree `--collapse` is structurally dead on dense TPMS:** measured **6.5%** face
  reduction at threshold 0.01 (482k→451k faces), no time saving — a gyroid is
  high-frequency everywhere, so the topology-safe collapse finds no flat region.

### The feasible win — QEM mesh decimation

Unlike octree collapse, post-hoc QEM merges triangles on **smooth-curved** regions
(a gyroid wall's curvature radius ~0.7 mm ≫ the 0.10 mm tessellation). Demonstrated
(`fast_simplification`, an MIT QEM decimator), error measured vs the **true analytic
surface** (`|max(sphere_sdf, onion)|` at each output vertex, in mm) on the clean
r=8 depth-7 mesh:

| keep | faces | vs orig | surf-err p99 | max |
| --- | --- | --- | --- | --- |
| 100% | 482,388 | 1× | 0.019 mm | 0.128 mm |
| 25% | 120,596 | 4× | 0.026 mm | 0.128 mm |
| **10%** | **48,238** | **10×** | **0.035 mm** | **0.077 mm** |
| 5% | 24,118 | 20× | 0.056 mm | 0.096 mm |

**10× lighter at 0.035 mm p99 error (below FDM resolution), and it *reduces* max
error (0.128→0.077) — decimation also smooths the worst jitter, mildly helping the
rim.** It does **not** conflict with streaming: decimate depth-8 **monolithically**
(it fits — ~3 GB, contoured in 3m28s), or per-tile with seam-plane vertices locked
(bounded RAM, crack-free).

---

---

← Back to the [report README](README.md) · the [topic README](../README.md).
