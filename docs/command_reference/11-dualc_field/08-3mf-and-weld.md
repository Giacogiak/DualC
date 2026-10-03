# The 3MF path and `--weld`

Part of [Tool 11: `dualc_field`](README.md). Streaming to a slicer-native 3MF (per-tile
objects, `1 unit = 1 mm`), `--weld` for a single globally-manifold object, and the
tiled/welded recipes.

## The 3MF path (`-o .3mf`)

`--tile-depth` with a `.3mf` output streams the **same tiled geometry** as the STL
path (identical ghost-ring + ownership, so the triangle set matches the monolithic
`.3mf` exactly), but writes it in the compact, slicer-native container instead of a
raw triangle soup:

| | Tiled STL (`-o .stl`) | Tiled 3MF (`-o .3mf`) |
| --- | --- | --- |
| **On disk** | uncompressed, 3 verts/triangle repeated | **deflated** + vertices **shared within each tile** → typically **~⅓ the size** |
| **Units** | none (slicers assume mm) | **`1 unit = 1 mm`** declared on `<model>` |
| **Structure** | one triangle soup | **one `<mesh>` object per non-empty tile**, all placed by a single `<build>` |
| **Peak RAM** | one tile | one tile (model part streamed to a temp file, then packed with incremental deflate) |
| **Seams** | duplicated (soup) | duplicated **across tiles**; shared **within** a tile |

```bash
# depth-8 lattice, streamed to a ~⅓-size slicer-ready 3MF at one-tile RAM:
dualc_field part.fld --depth 8 --tile-depth 5 -o part.3mf
```

**How it stays bounded.** The tiler emits each non-empty tile as its own 3MF
`<object>` (vertices deduped only within that tile) into a `3D/3dmodel.model` part
that is written to a temporary file as tiles finish, then packed into the `.3mf`
ZIP with miniz's disk-streaming deflate. Peak RAM is one tile; the uncompressed
model only ever lives on disk transiently before it is compressed to the final
(~⅓-size) archive.

**Caveat — object count.** Because each tile is a separate `<object>` + `<build>`
item, a dense part produces **many** objects (`nt³` non-empty tiles — thousands at
`--tile-depth 5`, depth 9). This is valid 3MF and every slicer imports it as one
print job, but some slicers render a several-thousand-item object tree slowly.
Mitigations: raise `--tile-depth` (fewer, larger tiles), or add
[`--weld`](#--weld--a-single-globally-manifold-object), which collapses the whole
part into a single welded object. If a *single* object without welding is required,
export monolithically (`-o .3mf` without `--tile-depth`) when
the part fits in RAM.

### `--weld` — a single globally-manifold object

Add `--weld` (with `--tile-depth` and `.3mf`) to fuse the per-tile objects into
**one** `<object>` with a **global shared-vertex pool**, welding the vertices that
adjacent tiles both emit at their shared seam:

```bash
dualc_field part.fld --depth 8 --tile-depth 5 --weld -o part.3mf
```

The result is **topology-identical to the monolithic mesh** — same vertices, same
triangles, same connectivity — so it is what you want for any consumer that reads
**adjacency**, not just geometry: re-running a CSG boolean on the export, FEA
volume meshing, edge-collapse decimation/LOD, or smooth cross-seam normals. (For
*printing* you don't need it — a slicer re-welds a soup on import — so the default
stays off.)

- **How the weld stays correct.** Each tile's interior is already connected; only
  the vertices on the tile-boundary planes are duplicated. The sink hashes only
  those seam-plane vertices (interior vertices get a fresh id and never enter the
  hash), keyed by position quantised to `cs·1e-6` with a 3×3×3 neighbour probe so
  a coincident pair whose two tiles computed it to slightly different low bits (the
  normal case on non-integer `--bounds`) still welds — verified to reproduce the
  monolithic vertex count, triangle set and edge structure exactly, including on
  non-dyadic boxes.
- **RAM.** Unlike the plain tiled paths, `--weld` is **not** one-tile-bounded: it
  holds one live tile **plus** the seam-vertex hash, which grows as `O(part seam
  area)` (a 2D quantity — far below the full mesh, but it does grow with the part).
  For the target part sizes this is ample; a strict one-tile bound (moving-front
  hash eviction) is not implemented
  ([11/04](../../roadmap/11-dense-lattice-deliverable/04-streaming-3mf-record.md)).
- **Constraint.** `--weld` requires `.3mf` output (it is inherently indexed); with
  `.stl` it is an error: `[dualc_field] --weld requires .3mf output (got 'part.stl')`. Without `--tile-depth`/`--mem` it is silently ignored (there is nothing to weld).

## Recipes

| # | What it does |
| --- | --- |
| Z1 | Dense gyroid lattice in a box → slicer-ready 3MF at one-tile RAM |
| Z2 | The same part welded — one globally-manifold object |
| Z3 | Welded on non-integer bounds |
| Z4 | Two tile depths for the same graph — the RAM/overhead trade |

All runnable as-is (swap the shape for your own). They use the `--expr` shorthand;
a `.fld`/`.json` file works identically.

```bash
# Z1 — Dense gyroid lattice clipped to a box -> slicer-ready 3MF, bounded RAM.
#    depth 8 would OOM monolithically; --tile-depth 5 keeps peak RAM to one tile,
#    and .3mf is ~1/3 the STL size with 1 unit = 1 mm baked in for the slicer.
dualc_field --expr "intersection(box(min=[-20,-20,-20],max=[20,20,20]),onion(normalize(gyroid(wavelength=4)),thickness=1.0))" \
            --depth 8 --tile-depth 5 -o lattice.3mf

# Z2 — Same part, but WELDED -> one globally-manifold object for a downstream that
#    reads connectivity (FEA volume meshing, edge-collapse LOD, re-running a CSG
#    boolean on the export). Topology-identical to the monolithic mesh.
dualc_field --expr "intersection(box(min=[-20,-20,-20],max=[20,20,20]),onion(normalize(gyroid(wavelength=4)),thickness=1.0))" \
            --depth 8 --tile-depth 5 --weld -o lattice_fea.3mf

# Z3 — Non-integer bounds are fine -- the weld key handles the tile-local rounding
#    divergence (this is the case an exact-vertex weld would crack).
dualc_field --expr "intersection(box(min=[-1.3,-1.3,-1.3],max=[2.7,2.7,2.7]),onion(normalize(gyroid(wavelength=1)),thickness=0.3))" \
            --depth 8 --bounds -1.3,-1.3,-1.3,2.7,2.7,2.7 --tile-depth 4 --weld -o part.3mf

# Z4 — Pick tile-depth from a RAM budget: start at depth-2, drop to -3/-4 if it
#    still doesn't fit (each -1 is ~4-8x less RAM/tile, more re-contour overhead).
dualc_field bracket_infill.fld --depth 9 --tile-depth 6 -o out.3mf   # ~440 MB/tile band
dualc_field bracket_infill.fld --depth 9 --tile-depth 5 -o out.3mf   # ~62 MB/tile
```

**Inspecting the output.** A `.3mf` is a ZIP; the mesh is `3D/3dmodel.model`
(XML). Quick checks without a slicer:

```bash
# Objects (1 with --weld; nt^3 non-empty tiles without) and the unit metadata:
unzip -p lattice.3mf 3D/3dmodel.model | grep -c "<object"
unzip -p lattice.3mf 3D/3dmodel.model | grep -o 'unit="[a-z]*"' | head -1   # unit="millimeter"

# Size win vs STL of the same part:
dualc_field ... --tile-depth 5 -o part.stl   # then compare file sizes; .3mf ~= 1/3
```

**Which variant?** Printing → plain `-o .3mf` (or `.stl`); the slicer re-welds a
soup on import, so `--weld` buys nothing and costs the seam hash. Any tool that
must **re-mesh, re-cut, or analyse adjacency** → `--weld`. A single object but the
part fits in RAM → monolithic `-o .3mf` (no `--tile-depth`).

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
