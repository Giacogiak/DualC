# Streaming / tiled export (`--tile-depth`)

Part of [Tool 11: `dualc_field`](README.md). Contour in grid-aligned tiles and stream
the mesh at one-tile RAM: how it works, choosing `D`, the constraints and the
watertightness contract, and why the stream is STL.

The monolithic path builds the **whole** octree and mesh in RAM before writing,
so a dense part hits a memory wall: a space-filling gyroid OOMs around depth 8 /
tens of GB ([Phase 0
benchmark](../../roadmap/11-dense-lattice-deliverable/02-phase-0-benchmark.md#phase-0-benchmark)).
`--tile-depth D` breaks that wall: it contours the field in **uniform,
grid-aligned cubic tiles** of `2^D` octree cells each and **streams** the
triangles to a binary STL (or a deflated 3MF) as each tile finishes, so peak RAM
is bounded by one tile — not the whole part.

## Recipes

| # | What it does |
| --- | --- |
| T1 | A depth-7 lattice that needs ~3 GB monolithically, exported in ~62 MB of RAM |
| T2 | The same as a ~3×-smaller slicer-native 3MF |

```bash
# T1 — depth-7 lattice that needs ~3 GB monolithically, exported in ~62 MB of RAM:
dualc_field part.fld --depth 7 --tile-depth 5 -o part.stl
# T2 — same, but a ~3x-smaller slicer-native 3MF (1 unit = 1 mm):
dualc_field part.fld --depth 7 --tile-depth 5 -o part.3mf
```

The tiled and welded 3MF recipes (Z1–Z4) are on [the 3MF page](08-3mf-and-weld.md#recipes),
the budget-driven ones (U1–U5) on [the `--mem` page](07-mem-budget.md#recipes).

#### Same file, different *how*

The output is **one STL file, identical** to the monolithic one (bit-identical on
a dyadic grid; otherwise the same triangles to within micron rounding —
indistinguishable to any slicer). `--tile-depth` changes only *how* it is
produced, i.e. the peak RAM along the way:

| | Monolithic (default) | Tiled (`--tile-depth`) |
| --- | --- | --- |
| **Process** | Build the *whole* octree + *whole* mesh in RAM, then write it all | One tile at a time: build a small tile octree → contour → append its triangles → free it → next tile |
| **Peak RAM** | the whole part (e.g. ~3 GB) | one tile (e.g. ~62 MB) |
| **Part too big to fit** | **OOM → no file at all** | still completes |
| **CPU time** | baseline | slightly more (ghost-ring re-contour, see below) |
| **Resulting `.stl`** | identical | identical |

It is genuinely *one* file, not fragments to stitch: binary STL is `header → facet
count → records`, so the streamer appends records per tile and patches the count
at the end.

#### Choosing `D`

Think of the two depths as **resolution vs. working-set**:

- **`--depth`** fixes the *part's* resolution — the global cell size
  `cs ≈ extent / 2^depth`. You pick it from the wall thickness you need (the
  [resolution rule](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist)); it does **not** change with tiling, and tiled output at a
  given `--depth` is the *same mesh* you'd get monolithically.
- **`--tile-depth D`** fixes how much of that grid is held in RAM at once: each
  tile is a `2^D`-cell cube. It is purely a **memory/throughput knob** — it never
  changes the result, only the peak RAM and the wall-clock.

`D` trades two costs that move in **opposite** directions, so there is a sweet
spot rather than "smaller is always better":

| As `D` decreases | Effect |
| --- | --- |
| **Peak RAM per tile** ↓ | One tile holds ≈ `2^D` cells/axis. RAM/tile scales ≈ **4× per +1 of `D`** for a thin surface, up to **≈8×** for a space-filling lattice (the measured gyroid below was ~7×). This is the win. |
| **Tile count** ↑ | Derived: `nt = ceil(2^depth / (2^D − 2))` per axis, so `nt³` total — it grows ≈ 8× for every −1 of `D`. More tiles = more sampler/contour setup overhead. |
| **Re-contour overhead** ↑ | Each tile re-samples a 1-cell **ghost ring** (the overlap that welds seams). The redundant work is a fraction `(2^D / (2^D − 2))³ − 1` of the useful work: **~10 % at `D=6`, ~21 % at `D=5`, ~49 % at `D=4`** — it explodes as `D→2`. This is CPU time, not RAM. |

Worked example — a depth-7 space-filling gyroid (anchored on the measured
`D=5` point; other rows are ≈ extrapolations at ~7×/level):

| `--tile-depth` | tiles (`nt³`) | peak RAM | re-contour overhead |
| --- | --- | --- | --- |
| 7 (= depth) | 2³ = 8 near-full tiles (legal, no benefit) | ~3.1 GB | ~0 % |
| 6 | 3³ = 27 | ~440 MB | ~10 % |
| **5** | **5³ = 125** | **62 MB (measured)** | ~21 % |
| 4 | 10³ = 1000 | ~10 MB | ~49 % |
| 3 | 22³ ≈ 10 600 | ~2 MB | ~140 % |

**Practical rule.** Start at **`tile-depth = depth − 2`** (≈ 1⁄16 to 1⁄64 of the
RAM, ~10–20 % overhead). If it still doesn't fit, drop to `− 3`, then `− 4`. Avoid
`D < 4`: the ghost-ring overhead dominates and the tile count balloons for little
extra RAM saving. The only hard bound is `D ≥ 2` (`--tile-depth must be >= 2`
otherwise); the useful range is `2 ≤ D ≤ depth − 1` — `D = depth` is legal but
runs 8 near-full tiles for nothing, and from `D ≥ depth + 1` the writer falls back
to one streamed pass ([design/10 § `--tile-depth`](../../design/10-invariants-and-tolerances.md#--tile-depth-the-legal-range-and-the-useful-one)).
To skip the hand-tuning entirely, let
[`--mem BUDGET`](07-mem-budget.md#auto-budget---mem-budget) pick the largest fitting `D` for you.

**Constraints.**
- **`.stl` or `.3mf` output.** `.obj` with `--tile-depth` is an error — `[dualc_field] --tile-depth requires .stl or .3mf output (got 'part.obj')` — it would be
  huge and unshared. The `.3mf` path streams one deflated **mesh object per tile**
  ([the 3MF page](08-3mf-and-weld.md#the-3mf-path--o-3mf)) — at the cost of one
  `<object>` per non-empty tile (thousands at high density; some slicers render
  that object tree slowly). Add
  [`--weld`](08-3mf-and-weld.md#--weld--a-single-globally-manifold-object) to collapse those into a
  single globally-manifold object.
- **Cannot combine with `--collapse`** — `[dualc_field] --tile-depth cannot be combined with --collapse (per-tile collapse would crack seams)`.
- Tiling is **uniform** on all axes (not slabs / per-axis): DualC's octree halves
  every axis by the same factor, so only a uniform split reproduces the global
  cell grid exactly. A non-uniform split would change the cell aspect ratio and
  give a *different* mesh than the untiled part.

**Watertightness contract.** Adjacent tiles overlap by one ghost cell and an
ownership rule (face centroid quantised to the global cell grid) emits every
boundary quad **exactly once** — no cracks, no duplicate faces. On a
dyadic-aligned grid (integer-mm box + power-of-2 depth) the tiled output is
**bit-identical** to the monolithic mesh — including the manufacturing case where
the surface lies *on* the `--bounds` faces (e.g. `gyroid ∩ box`: the box-face caps
and outer walls are reproduced faithfully, no crack or doubled cap). Both the
interior-surface and on-the-bounds cases are locked by
`tests/test_streaming_export.cpp`.

STL stores independent triangles **by design** (no shared vertices), so the
streamed file is a triangle *soup* — exactly like a monolithic STL, and in fact
bit-identical to one (above). A slicer welds coincident vertices on import, so
**tiling causes no manifold regression**: the tiled `.stl` is as watertight and as
weldable as the untiled one. A *globally-welded, shared-vertex* object — for a
downstream that needs connectivity (re-booleans, FEA, decimation) — is
[`--weld`](08-3mf-and-weld.md#--weld--a-single-globally-manifold-object) on the
`.3mf` path, which hashes only the seam-plane vertices so the weld stays bounded.
On a *non*-dyadic box (arbitrary float `--bounds`) seam vertices are coincident to
within rounding (microns) rather than bit-identical; the ownership rule is
integer-grid so it stays crack-free regardless.

#### Why STL (and not a shared-vertex format like PLY)

Two reinforcing reasons, recorded with the design in
[11/03 § 5](../../roadmap/11-dense-lattice-deliverable/03-streaming-export.md#5-tiledstreaming-mesh--break-the-ram-wall-for-stl3mf-at-high-density):
STL is what manufacturing consumes (a slicer re-welds any mesh as a triangle soup
on import, so shared vertices buy it nothing, and most slicers do not read PLY),
and STL is the format that makes bounded-RAM streaming possible at all (no global
vertex table, so one triangle at a time with O(1) state). The size cost of
repeated vertices is answered inside the manufacturing ecosystem by
[3MF](08-3mf-and-weld.md#the-3mf-path--o-3mf) — shared vertices per tile plus
deflate — and the connectivity a downstream tool may need by
[`--weld`](08-3mf-and-weld.md#--weld--a-single-globally-manifold-object), which
hashes only the seam-plane vertices; how both escape the whole-part vertex table
is [11/03 § 5a](../../roadmap/11-dense-lattice-deliverable/03-streaming-export.md#5a-deferred-enhancements-gains-vs-the-shipped-stl-version).

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
