# Step 5 — tiled / streaming export, and its deferred enhancements (§ 5a)

Part of [11 — Dense-lattice deliverable](README.md). What shipped (STL 2026-06-15, 3MF +
seam-welding 2026-07-03, `--mem` 2026-07-06) and the § 5a ledger of what was deferred
and why. Usage: [command_reference/11 §
Streaming](../../command_reference/11-dualc_field/06-streaming-tiled-export.md).

## 5. Tiled/streaming mesh — break the RAM wall for STL/3MF at high density
**DONE** — STL 2026-06-15; 3MF + seam-welding 2026-07-03; `--mem` auto-budget
2026-07-06. Direct slicer DEFERRED (step 6).


[Step 2](../09-io-formats.md) makes parts printable but still materializes the whole
mesh, so it hits the RAM wall at high density. **Shipped (`dualc_field
--tile-depth D`, STL):** `writeFieldTiledStl` in `examples/example_common.cpp`
contours the field in **uniform, grid-aligned cubic tiles** of `2^D` cells each
and streams triangles to a binary STL via an incremental writer (header +
placeholder count, append 50-B records, `seekp(80)` count-patch on finish), so
peak RAM is one tile. **Measured: a depth-7 gyroid shell that peaks at ~3.08 GB
monolithically exports at ~62 MB peak with `--tile-depth 5` — a ~49× reduction,
output bit-identical** (`tests/test_streaming_export.cpp`). Seams: adjacent tiles
overlap by one ghost cell and an ownership rule (centroid quantised to the global
cell grid) emits every boundary quad exactly once — no crack, no dup. Tiling is
**uniform** (single-`maxDepth` octree halves every axis equally, so only a
uniform split reproduces the global grid; slabs/per-axis would change the cell
aspect ratio and the mesh). STL stores independent triangles, so seam vertices
are duplicated (geometrically watertight for slicers, not topologically welded —
true welding needs a global vertex hash = unbounded RAM). Empty tiles are skipped
(`cellOverlaps` pre-check + empty-placeholder guard). Four enhancements are
**deferred** — detailed with their gains in
[5a](#5a-deferred-enhancements-gains-vs-the-shipped-stl-version) below.

Separately, eventually promote [step 1](../06-slice.md)'s `marchingSquaresZero`
into a full **direct per-layer slicer** (B.1): a Z-stack of 2D contours is
O(res²)/layer and memory-independent of density — the manufacturing-native escape
— valuable when targeting a print pipeline we control (DLP/industrial), since
desktop slicers still want a mesh. *(2026-09-18: trigger written as row D-13 of the
[decisions index](../../decisions/README.md).)*

*(2026-09-20: **MINOR-OPEN** — the single-pass console line in `forEachOwnedTile`
(`examples/example_common.cpp`, `"[dualc] tile-depth >= depth: no tiling, single
streamed pass"`) names the wrong condition: the fallback fires at `ownedCells >=
nGlobal`, i.e. `D ≥ depth + 1`, and `D = depth` still tiles. Found by
[19 Phase 3](../19-docs-layers/02-record-phase-3.md) while settling the range for
[design/10](../../design/10-invariants-and-tolerances.md#--tile-depth-the-legal-range-and-the-useful-one);
left by the docs-only phases 3–6. Owed by the next code session that touches the
tiler: reword the line and drop the parenthetical from design/10 in the same commit.)*

### 5a. Deferred enhancements (gains vs. the shipped STL version)

The v1 ships the load-bearing capability — **bounded-RAM export of dense parts to
STL** — and validates the whole manufacturing workflow. These four follow-ons
each close a specific gap; all build on the same `writeFieldTiledStl` /
`StreamingStl` plumbing, so none requires rework.

1. **3MF streaming. — DONE (2026-07-03).** *Was:* STL only; STL writes every
   triangle as three independent 32-bit vertices, so a dense part is **huge** on
   disk — a 155 M-face lattice is ≈ 7.7 GB of STL. *Gain:* 3MF (a) **deflates** and
   (b) shares vertices, so it is typically **~⅓ the size** *and* carries the
   `1 unit = 1 mm` metadata desktop slicers expect — the print-ready interchange.
   *Shipped:* `dualc_field --tile-depth D -o part.3mf` (`writeFieldTiled3mf` in
   `example_common.cpp`). The tiling driver is now format-agnostic
   (`forEachOwnedTile` + a `Sink`): the STL path is the unchanged soup sink; the
   3MF sink writes **per-tile mesh objects** (vertices shared *within* each tile)
   to a temp model part streamed as tiles arrive, then packs it into the `.3mf`
   ZIP via miniz's disk-streaming `mz_zip_writer_add_file` (incremental deflate) —
   so peak RAM stays **one tile**, the uncompressed model only ever touches disk.
   Across-tile seam vertices stay duplicated (as in the STL soup); a single
   globally-welded object is item 4 below. *Verified:* `tests/test_streaming_export.cpp`
   — the tiled 3MF's world-triangle set equals the monolithic `write3mf`, `unit=
   "millimeter"` present, one `<object>` per non-empty tile. *Deferred follow-on:*
   the per-tile-object count scales with non-empty tiles (thousands at high
   density) — some slicers render that object tree slowly; collapse to components
   under one build item, or land item 4 (welding) which yields a single object.
   *(2026-09-18: trigger written as row D-23 of the [decisions index](../../decisions/README.md).)*

2. **`--mem BUDGET` auto-budget. — DONE (2026-07-06).** *Was:* the user picks
   `--tile-depth D` manually and must reason about RAM-per-tile (4–8× per level).
   *Shipped:* `dualc_field --mem BUDGET` (the suffix grammar:
   [command_reference/11 § --mem](../../command_reference/11-dualc_field/07-mem-budget.md))
   **estimates the busiest tile and picks the largest `D` that fits** — both
   ergonomic ("export this part within my RAM", no tuning) and *optimal*: the
   largest fitting `D` minimises the tile count and the ghost-ring re-contour
   overhead, so it is the **fastest** export within the budget. `--mem` is a thin
   **tile-depth picker** in front of the already-verified `forEachOwnedTile`: it
   computes one integer `D` and hands it to `writeFieldTiled{Stl,3mf}`, so the mesh
   is byte-identical to choosing that `D` by hand (locked by
   `tests/test_streaming_export.cpp`). *Estimator (`pickTileDepthForBudget` in
   `example_common.cpp`):* one cheap coarse whole-field contour (probe depth
   `min(depth−1, 5)`, bumped if a non-empty part reads empty) supplies the spatial
   face distribution; bucket its face centroids into each candidate `D`'s owned-tile
   grid and take the **busiest bucket** (peak RAM is bounded by the densest tile,
   never the average — the interior tiles of a boolean-carved lattice run well above
   the mean); extrapolate to `--depth` with a fixed **8×/level** growth (the
   space-filling bound; over-estimates a thin surface → safe) and a **3 KiB/face**
   peak constant — the transient contour peak (octree + mesh-under-construction +
   QEF/SVD scratch) is ~10× the final per-face mesh cost — *calibrated by execution*
   against the Phase-0 anchor: the measured depth-7 gyroid `D=5` ≈ 62 MB point
   reports ~99 MB (~1.6×), so a budget that admits `D=5` sits comfortably above the
   true requirement. (An initial 1 KiB/face under-reported it at ~33 MB — it would
   have silently OOM'd a tight budget — which is exactly why the constant is
   anchored to a run, not reasoned.) Every rounding is **conservative** — over-estimating
   only picks a smaller `D` (more tiles, slower, safe); under-estimating would OOM,
   the one failure `--mem` exists to prevent — and the probe/chosen-`D`/estimate are
   logged so the user can override with an explicit `--tile-depth` (which wins).
   Same `.stl`/`.3mf`-only + no-`--decimate`/`--collapse` guards as `--tile-depth`.
   *Verified — new behaviour:* two `tests/test_streaming_export.cpp` cases (tag
   `[mem]`) — `parseMemBudget` suffix parsing (`4G`/`512M`/`2048K`/`1.5G`/`256MiB`/
   bytes + the `0`/`-4G`/`4Z`/`bogus` rejections) and the picker itself (clamping to
   `[2, depth−1]`, monotonicity in the budget, delegation byte-identical to the
   explicit `--tile-depth`, unbounded-field → −1). Plus a **live calibration run**:
   the Phase-0 anchor (`intersection(box±40, onion(normalize(gyroid λ=5),2))`,
   `--depth 7 --mem 100M`) picks `D=5` at est. ~99 MB (> the 62 MB measured peak →
   confirmed conservative), and `--mem 50M` correctly steps down to `D=4` rather than
   pick a `D=5` that would OOM a 50 MB budget — the pre-fix `1 KiB` constant would
   have picked `D=5` there. CLI-checked: `--mem` + `.obj`/`--decimate`/`--collapse`
   rejected, explicit `--tile-depth` override, `--mem --weld -o .3mf`, empty-probe
   fallback (surface outside `--bounds`).
   *Verified — no regression:* the only behaviour-changing edit to existing code is
   extracting the tiled driver's box-resolution into `resolveGlobalSamplingBox`
   (`forEachOwnedTile` now calls it); its output is locked **byte-identical** by the
   pre-existing streaming cases (161–166: tiled STL/3MF/welded == monolithic), all
   still green. All other changes are additive (new `--mem` flag skipped entirely
   when absent; new functions with no existing caller). Pre-existing `dualc_field`
   paths re-smoke-tested (monolithic `.obj`/`.stl`, `--tile-depth`, `--decimate`,
   `--dump-json`, `--list`). **Full suite: 190/190 pass; clean `/W4` build** (the only
   warnings are pre-existing geometry-central ones in the unrelated `dualc_view`).

3. **`dualc_lattice` wiring.** *Current:* only `dualc_field` exposes
   `--tile-depth`; the **`dualc_lattice` shortcut — the exact tool whose gyroid
   λ=5 mm run defined the RAM wall in the Phase 0 benchmark — still OOMs at high
   density**. *Gain:* the streaming writer is already shared in `example_common`,
   so wiring it into `dualc_lattice` is a **few lines** and lets the dense-lattice
   command export at high density directly, without re-expressing the lattice as a
   field-graph. *Net:* **convenience parity only — no new capability** over
   `dualc_field --tile-depth` (any lattice is expressible as a field-graph and
   already streams). Good to mention, not a focus; deferred behind the
   capability-adding items (5a.1 3MF streaming, 5a.4 seam-welding).
   **DEFERRED** (status added 2026-09-11; 5a.1/5a.4 shipped 2026-07-03 without triggering it) — *trigger:* a `dualc_lattice` user who cannot switch to `dualc_field`.

4. **Topological seam-welding. — DONE (2026-07-03).** *Was:* the streamed output
   never built a global *connected* mesh — each tile emitted its own patch, so
   vertices shared across a seam were written **twice** (per-tile-3MF) or as an
   independent soup (STL). Fine for a slicer (it re-welds on import), but wrong for
   anything that consumes **topology** (adjacency): re-running a CSG boolean on the
   export, FEA volume meshing, edge-collapse decimation/LOD. *Shipped:* `dualc_field
   --tile-depth D --weld -o part.3mf` (`Welded3mfSink` in `example_common.cpp`)
   emits a **single** 3MF `<object>` with a global shared-vertex pool. Each tile's
   interior is already correctly connected; the only duplicates lie on the
   tile-boundary planes (a 2D subset), so only vertices within a few cells of an
   internal seam plane enter a hash — interior vertices get a fresh id and pass
   straight through. *Weld key:* the vertex position quantised to `eps = cs*1e-6`,
   with a **3×3×3 neighbour probe** so a coincident pair whose two tile-local
   computations straddle a bucket boundary still welds (distinct vertices are
   ~1e6 buckets apart, so a ±1 hit is always the coincident one — no false merge).
   Classification is by distance to the seam plane (a cs-scale band), not a cell
   floor, so ULP noise can't flip it. *Verified:* on a **non-dyadic** box
   (`tests/test_streaming_export.cpp`, and a depth-9 / 695 k-vertex check) the
   welded mesh reproduces the monolithic mesh **exactly** — same vertex count, same
   triangle set, same edge-incidence distribution, zero crack (count-1) edges.
   *RAM caveat:* one live tile **plus** the seam hash, which is `O(part seam area)
   ~ O(nt·nGlobal²)` — a 2D quantity far below the full mesh, but it does grow with
   the part (the hash never shrinks). A strict one-tile bound needs a **moving-front
   eviction** (drop a seam plane's entries once both neighbouring tiles are done) —
   *deferred* as an optimisation; the current 2D bound is ample for the target sizes.
   *(2026-09-18: trigger written as row D-24 of the [decisions index](../../decisions/README.md).)*

*(2026-09-22 — #48.)* The tiled driver gained the cancel token and the progress sink
(`Stage::Tile` per tile), writes `<path>.part` and renames on success, and its two
error exits that used to `finish()` a truncated file now abort; the sinks gained an
`abort()` rung. Record and evidence: [14/05](../14-c-abi/05-progress-and-cancel.md).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
