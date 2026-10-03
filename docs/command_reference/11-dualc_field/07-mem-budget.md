# Auto-budget (`--mem BUDGET`)

Part of [Tool 11: `dualc_field`](README.md). Let the tool pick the largest
`--tile-depth` that fits a RAM budget instead of choosing `D` by hand: syntax, the
estimate, precedence, recipes.

Choosing `D` by hand means reasoning about RAM-per-tile (which grows ~4–8× per
level — the worked table in [Choosing `D`](06-streaming-tiled-export.md#choosing-d)). `--mem` does that reasoning for you: **give a RAM
budget instead of a tile depth, and the tool picks the largest `D` whose estimated
peak per-tile RAM fits.** The largest fitting `D` is also the *fastest* export
within the budget (fewer tiles ⇒ less ghost-ring re-contour overhead), so `--mem`
picks the speed/RAM sweet spot automatically. It is otherwise a drop-in for
`--tile-depth`: it takes the identical tiled path, so the mesh is byte-identical to
picking that `D` by hand.

```bash
# "export this dense part, staying under ~4 GB" -- no manual tile tuning:
dualc_field part.fld --depth 8 --mem 4G -o part.3mf
```

**`BUDGET` syntax** — a positive magnitude with an optional binary suffix
(case-insensitive; the `iB` spelling is accepted too):

| Form | Means | Notes |
| --- | --- | --- |
| `4G`, `4GiB` | 4 × 1024³ bytes | gibibytes |
| `512M`, `512MiB` | 512 × 1024² bytes | mebibytes |
| `2048K`, `2048KiB` | 2048 × 1024 bytes | kibibytes |
| `1.5G` | fractional magnitude | any suffix |
| `268435456`, `…B` | raw bytes | no suffix / `B` |

Malformed or non-positive values (`0`, `-4G`, `4Z`, `bogus`) are rejected:
`[dualc_field] --mem expects a positive size like 4G, 512M, or a byte count`.

**Reading the log.** `--mem` prints exactly what it decided, so you can sanity-check
or override it:

```text
[dualc] --mem: budget 100 MiB; probe at depth 5 -> 32124 faces
[dualc] --mem: chose --tile-depth 5 (est. peak ~99 MiB/tile)
[dualc] tiled streaming STL: depth 7, tile-depth 5 => 5^3 = 125 tiles ...
```

- *probe at depth D → N faces* — the cheap coarse contour used to estimate density.
- *chose --tile-depth D (est. peak ~X MiB/tile)* — the pick and its (conservative)
  RAM estimate. If `X` is comfortably under your budget, you can try forcing `D+1`
  by hand for a faster run; if it is near the budget, trust the pick.

## Recipes

| # | What it does |
| --- | --- |
| U1 | Dense TPMS on a workstation — most of 16 GB |
| U2 | The same graph on a laptop — a smaller `D`, completes instead of OOM-ing |
| U3 | Welded single-object 3MF under a budget |
| U4 | Lattice infill of a mesh → 3MF under 4 GB |
| U5 | An unbounded graph needs `--bounds`, as with `--tile-depth` |

```bash
# U1 — Dense TPMS on a workstation -- let it use most of 16 GB:
dualc_field lattice.fld --depth 9 --mem 12G -o lattice.3mf

# U2 — Same graph on a laptop with far less headroom -- it just picks a smaller D
#    (more, smaller tiles; slower but it completes instead of OOM-ing):
dualc_field lattice.fld --depth 9 --mem 2G -o lattice.3mf

# U3 — Welded, single-object 3MF (FEA / re-boolean consumer) under a budget:
dualc_field part.fld --depth 8 --mem 4G --weld -o part.3mf

# U4 — Lattice infill of a mesh -> slicer-ready 3MF, under 4 GB, no tile tuning
#    (the gyroid-in-bracket graph from the overview page):
dualc_field --expr 'intersection(mesh(path="bracket.obj"),onion(normalize(gyroid(wavelength=0.5)),thickness=0.1))' \
  --depth 8 --mem 4G -o bracket_infill.3mf

# U5 — Unbounded graph (bare gyroid / infinite repeat) -- give it a region, as with
#    --tile-depth:
dualc_field --expr "onion(normalize(gyroid(wavelength=5)),thickness=1.5)" \
  --bounds -40,-40,-40,40,40,40 --depth 8 --mem 4G -o gyroid.stl
```

**When to use `--mem` vs. `--tile-depth`:**

| Use… | When |
| --- | --- |
| **`--mem BUDGET`** | You know your **RAM ceiling** ("keep it under 8 GB") but not the right `D`; you run the **same graph across machines** with different RAM; you want the fastest safe export without tuning. This is the ergonomic default for dense parts. |
| **`--tile-depth D`** | You already know the `D` that works and want it **reproducibly**; you want to **override** the conservative auto-pick and push `D` one higher; you are scripting a fixed pipeline where the geometry (hence `D`) never changes. |
| **neither (monolithic)** | The part fits in RAM comfortably — tiling only adds re-contour overhead. Use `--decimate`/`--simplify` there to shrink the *file*, which `--mem`/`--tile-depth` do not. |

**How the estimate works (and why it's safe).** One cheap coarse whole-field
contour supplies the spatial face distribution; the tool buckets those faces into
each candidate tile grid, takes the **busiest** tile (peak RAM is bounded by the
densest tile, never the average — the interior tiles of a boolean-carved lattice
run well above the mean), and extrapolates it to `--depth` with a conservative
fixed growth model + a per-face byte constant *calibrated by measurement* (the
depth-7 gyroid `D=5` ≈ 62 MB reference reports ~99 MB, i.e. ~1.6×). It therefore
**errs toward over-estimating** — if in doubt it picks a *smaller* `D` (more tiles,
a little slower, but it will **not** OOM, which is the one failure `--mem` exists to
prevent). The estimate is a safety heuristic, **not** a precise RAM predictor; the
printed figure is a conservative upper bound, so your real peak is typically lower.

**Constraints & precedence.**
- An explicit `--tile-depth D` **wins** — `[dualc_field] note: --mem ignored -- explicit --tile-depth 5 takes precedence` — so you
  can always override the auto-pick.
- `.stl` or `.3mf` output only (as with `--tile-depth`): `[dualc_field] --mem requires .stl or .3mf output (got 'part.obj')`.
- Cannot combine with `--decimate`/`--simplify` (`[dualc_field] --mem (streaming) cannot be combined with --decimate/--simplify`) or `--collapse`
  (`[dualc_field] --mem (streaming) cannot be combined with --collapse (per-tile collapse would crack seams)`) — same rejections as `--tile-depth`.
- If the field is **unbounded**, pass `--bounds` (same requirement as `--tile-depth`).
- The chosen `D` is searched in `[2, --depth − 1]` — the useful range of
  `--tile-depth` ([design/10](../../design/10-invariants-and-tolerances.md#--tile-depth-the-legal-range-and-the-useful-one)).
  If nothing fits even at `D=2`, it uses `D=2` and warns that the estimate is
  still over budget (it cannot tile finer).

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
