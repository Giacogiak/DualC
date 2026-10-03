# § 5b — implementation record: streaming 3MF + seam-welding

Part of [11 — Dense-lattice deliverable](README.md); the full record behind step 5's 3MF
and `--weld` deliveries (2026-07-03).

## 5b. Implementation record — streaming 3MF + seam-welding
**2026-07-03.**


The full engineering detail behind 5a.1 and 5a.4, so the design is recoverable
in-repo. Everything lives in **`examples/`** (the library-scope rule keeps all file
I/O out of `src/`/`include/dualc/`).

**Architecture — one driver, three sinks.** The tile loop that was inlined in
`writeFieldTiledStl` is extracted into a **format-agnostic template**
`forEachOwnedTile(field, path, sp, cp, tileDepth, label, Sink&)`
(`examples/example_common.cpp`). It owns everything format-independent — global-box
resolution (replicating the sampler's auto-fit padding so the tiled grid matches
the monolithic one), the cell grid (`nGlobal = 2^depth`, `tileCells = 2^tileDepth`,
`ownedCells = tileCells − 2`, `nt` tiles/axis), the **one-cell ghost ring**
(`tb.min` grown by −1 cell, span `tileCells` cells), the per-tile contour, the
empty-tile skips (`field.cellOverlaps(tb)` pre-check + `isEmptyPlaceholder`
post-check), the **centroid-ownership predicate** (quantise each face centroid to
the global cell grid; keep iff in this tile's owned range, with first/last tiles
clamping to the domain — so every boundary quad is emitted exactly once), the
single-pass fallback when `tileDepth ≥ depth`, and progress/success logging. A
`Sink` supplies only the on-disk format via `setGrid` / `open` / `onTile(c, keep)`
/ `finish` / `count`. Three sinks:

- **`TiledStlSink`** — the pre-existing behaviour, now a thin wrapper over
  `StreamingStl`; `onTile` fan-triangulates kept faces straight to independent
  50-byte STL records. `writeFieldTiledStl` is byte-for-byte unchanged in output
  (its acceptance test still passes untouched).
- **`Tiled3mfSink`** — one deflated 3MF `<object>` per non-empty tile. `onTile`
  builds a **within-tile** vertex remap over the kept faces only (dedup by original
  vertex index), writes the tile's `<mesh>` (shared vertices + fan-tri triangles)
  to a temp `*.model.tmp` file, and records the object id for a single closing
  `<build>`. `finish` closes the model part and packs the `.3mf` ZIP: the two tiny
  fixed OPC parts (`[Content_Types].xml`, `_rels/.rels`, now file-scope constants
  shared with the buffered `write3mf`) via `mz_zip_writer_add_mem`, and the model
  part via **`mz_zip_writer_add_file`** (miniz's disk-streaming, incremental
  deflate). Peak RAM = one tile; the uncompressed model only touches disk.
- **`Welded3mfSink`** — a single globally shared-vertex `<object>` (5a.4). Details
  below.

**3MF container.** `<model unit="millimeter">` (declared via the shared
`kModelHeader3mf`) gives the slicer-native `1 unit = 1 mm`. Per-tile objects are
each referenced by a `<build><item objectid=…>` with identity transform — valid
3MF, imported as one print job. `write3mf`'s content-type/rels blobs were hoisted
to file scope (`kContentTypes3mf` / `kRels3mf`) so the buffered and both streaming
writers can never drift.

**Welding algorithm (`Welded3mfSink`).** A single `<object>` needs all
`<vertices>` before all `<triangles>`, but both are discovered as tiles stream, so
they go to **two temp files** (`*.verts.tmp`, `*.tris.tmp`) that `finish`
concatenates (header + verts + `</vertices><triangles>` + tris + footer) into the
model part before the same ZIP pack as `Tiled3mfSink`. Per tile, a `local2global`
map dedups vertices *within* the tile; the global id comes from `globalIdFor`:
- **Interior vertex → fresh id.** A vertex more than a few cells from every
  internal seam plane is produced by exactly one tile, so it never needs
  cross-tile dedup and never enters the hash.
- **Seam vertex → hashed.** `onSeam(p)` tests **distance to the seam plane**
  (`box.min + k·ownedCells·cs`, k ∈ [1, nt−1]) within a `band = 2.0` cell window —
  a cs-scale test, so ULP noise can never flip a shared vertex between the hashed
  and un-hashed sets. The key is the position quantised to `eps = cs·1e-6` per
  axis, looked up with a **3×3×3 neighbour probe**: a coincident pair that two
  tiles computed to slightly different low bits (the normal case on non-integer
  `--bounds`, where each tile subdivides from a different origin) can straddle a
  bucket boundary and land on *adjacent* keys — the probe still welds them, while
  genuinely distinct vertices (~1e6 buckets apart) can never false-merge.

**Why the weld key is not exact-double (the subtle part).** Cell-corner world
positions are computed **tile-locally** (`cornerPosition(node.bounds,…)` descends
per-tile via `childBounds` halving, `src/sampler.cpp`). For **dyadic** bounds the
two chains coincide at double precision; for **non-dyadic** bounds the same seam
vertex differs in low mantissa bits between tiles, so an exact-double key would
false-split and crack the seam. The snap-to-grid key + neighbour probe absorbs
that; the divergence (~`depth·|coord|·2⁻⁵²`) is far below `eps` at every realistic
depth, and shrinking `eps` with depth is why the probe (not just a wide `eps`) is
the robust fix.

**RAM.** STL and per-tile-3MF are **one-tile-bounded**. Welding is **one tile + a
seam hash** that holds only the seam-band vertices — `O(nt·nGlobal²)`, a 2D subset
far below the 3D mesh, but it grows with the part and never shrinks. The
moving-front eviction that would restore a strict one-tile bound is deferred. *(2026-09-18:
trigger written as row D-24 of the [decisions index](../../decisions/README.md).)*

**CLI surface (`dualc_field`).** `--tile-depth D` dispatches on the `-o` extension
(`.stl` → `writeFieldTiledStl`, `.3mf` → `writeFieldTiled3mf`, `.obj` → error);
`--weld` (3MF only) selects `Welded3mfSink`. Both reject `--collapse` (per-tile
cell-collapse would crack seams). Public entry points in `example_common.h`:
`writeFieldTiledStl`, `writeFieldTiled3mf(…, bool weld = false)`.

**Verification (`tests/test_streaming_export.cpp`, 6 cases).** STL bit-identity
(interior + on-the-bounds) unchanged. New: tiled-3MF world-triangle set equals the
monolithic `write3mf` (via a miniz-unzip + model-XML parse), `unit="millimeter"`,
one object per non-empty tile, and the reject guards. Welding is tested on a
**non-dyadic** box (`[-1.3, 2.7]³`) — the hard case — asserting a single object,
triangle set == monolithic, welded vertex count **==** monolithic (two-sided: a
crack would add a vertex, an over-merge would drop one), welded vertex count `<`
the per-tile duplicated count, and the **edge-incidence distribution == monolithic
baseline** with no crack (count-1) edges (matching, not exceeding, the DC output's
intrinsic higher-valence edges). A manual depth-9 / 695 k-vertex non-dyadic run
confirmed exact reproduction at 16× smaller `eps`. All 188 CTest cases pass.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
