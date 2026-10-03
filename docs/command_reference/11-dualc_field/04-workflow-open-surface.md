# Workflow: isolate → thicken → skin → union a TPMS lattice

Part of [Tool 11: `dualc_field`](README.md). The four-step manufacturing workflow that
turns a TPMS lattice and its enclosing part into one printable solid, with the
RAM-bounded and preview variants.

A four-step manufacturing workflow that turns a TPMS lattice and its enclosing
volume into one printable part — composed at the **field** level, so each export
is a single contour pass with no intermediate lattice mesh.

The enclosing volume does double duty in every step: it **clips** the lattice
and (because a TPMS has infinite `bounds()`) **supplies the finite sampling
domain**. The examples below clip to the unit cube `[-0.5,0.5]³`; swap
`box(min=[-0.5,-0.5,-0.5],max=[0.5,0.5,0.5])` for `mesh(path="part.obj")` to clip
to an arbitrary volume (mind the [mesh-path quoting](01-op-vocabulary.md#--expr-and-shell-quoting)).

Facts worth keeping in mind:

- `onion(f, t)` = `|f| − t` is solid for `−t < f < +t`, so the **wall is
  `≈ 2·thickness`** (each sheet sits ±t from the membrane). For a 1 mm wall with
  `normalize`, use `thickness=0.5`.
- `normalize` makes `thickness` first-order metric; confirm a critical wall on a
  [`dualc_slice`](../07-dualc_slice.md) section or the exported STL.
- A volume-filling TPMS refines **everywhere**, so a high monolithic `--depth` is
  expensive — prefer [`--tile-depth`](06-streaming-tiled-export.md#streaming--tiled-export---tile-depth) to
  hold RAM down. The wall must be ≥ ~2–3 cells — the
  [resolution rule](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist).

## Recipes

| # | Step | What it does |
| --- | --- | --- |
| W1 | (a) | Inspect the isolated lattice surface, meshless — the `dualc_field_view` recipe O1 |
| W2 | (b) | Thicken → watertight lattice solid (STL) |
| W3 | (b′) | Graded wall thickness (`graded-onion`) |
| W4 | (b′) | Graded lattice in a real mesh (`foot.obj`), streamed with `--tile-depth` |
| W5 | (c) | Skin — thicken the enclosing volume's own surface |
| W6 | (d) | Union — lattice + skin in one field-graph, one contour |

**(a) W1 — Inspect the isolated lattice surface — meshless, not contoured.** Preview
the membrane alone (no welded volume skin) in `dualc_field_view` with the *same*
expression as W2: recipe O1 on
[Isolating a TPMS surface](../12-dualc_field_view/04-open-surface-preview.md#recipes).

**(b) W2 — Thicken → watertight lattice solid** — the *same field*, contoured and
exported:

```bash
dualc_field --expr "intersection(onion(normalize(gyroid(wavelength=0.3)),thickness=0.03),box(min=[-0.5,-0.5,-0.5],max=[0.5,0.5,0.5]))" -o cube_lattice.stl --depth 7
# high depth without the RAM spike: add --depth 8 --tile-depth 3
```

Because the shell is empty between its sheets, **no big box faces appear** — the
volume boundary shows only as thin strut-end cross-sections, which belong to the
lattice (the bonding surface to the skin), not a welded skin. (Drop the `onion`
and you get the skin artifact back: `intersection(gyroid(...),box(...))` fills a
whole channel and welds the box faces on.)

**(b′) W3 — Graded wall thickness** — swap `onion` for `graded-onion` to thin/thicken
the lattice across the part. Here the wall grows from 0.04 near the origin
(`|p| ≤ 2`) to 0.18 toward the rim (`|p| ≥ 7`), driven by distance from a point
(`sphere(radius=0)`); exports exactly like the uniform shell above:

```bash
dualc_field --expr "intersection(graded-onion(normalize(gyroid(wavelength=2)),sphere(radius=0),t1=0.04,t2=0.18,d0=2,d1=7),box(min=[-8,-8,-8],max=[8,8,8]))" -o graded.stl --depth 6 --bounds -9,-9,-9,9,9,9
# distance-from-surface instead: use mesh(path="part.obj") as the control child
```

**W4 — Graded lattice in a real mesh — quick test** (`data/foot.obj`; run from the
repo root). A control `sphere` placed in the instep makes the walls thick there
(`t1=4`, ≈8 mm) and thin out 45 mm beyond it (`t2=1.6`, ≈3.2 mm); `--tile-depth`
streams it in bounded RAM — a few minutes at `--depth 7` (the measurement is in
[12/05 § Validated thin-shell workflow](../../roadmap/12-field-graph-and-app/05-open-surface.md#validated-thin-shell-workflow-no-new-code)):

```bash
dualc_field --expr 'intersection(graded-onion(normalize(gyroid(wavelength=16)),sphere(center=[0,0,35],radius=15),t1=4.0,t2=1.6,d0=0,d1=45),mesh(path="data/foot.obj"))' --depth 7 --tile-depth 4 -o foot_graded.stl
# faster coarse sanity check (~30-60 s): drop to --depth 6 and remove --tile-depth
# preview it live (no meshing) with the same --expr in dualc_field_view
```

PowerShell 5.1 form of the same command (path quotes escaped `\"` — the
[quoting rule](../../design/09-conventions.md#windows-powershell-51-quoting)):

```powershell
.\dualc_field.exe --expr 'intersection(graded-onion(normalize(gyroid(wavelength=16)),sphere(center=[0,0,35],radius=15),t1=4.0,t2=1.6,d0=0,d1=45),mesh(path=\"data/foot.obj\"))' --depth 7 --tile-depth 4 -o foot_graded.stl
```

Orientation cheatsheet: control = `sphere` SDF (negative inside) → `t1` is the
thickness **at/inside** the sphere, `t2` far away; swap `t1`/`t2` to invert.
Lower `t2` ⇒ thinner walls ⇒ need higher `--depth` (the
[resolution rule](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist))
or the thin end fragments.

**(c) W5 — Skin — thicken the enclosing volume's own surface** into a hollow wall. A
`box`/`mesh` SDF is already metric, so no `normalize`:

```bash
dualc_field --expr "onion(box(min=[-0.5,-0.5,-0.5],max=[0.5,0.5,0.5]),thickness=0.03)" -o cube_skin.stl --depth 7
# mesh volume: onion(mesh(path="part.obj"),thickness=…)
```

**(d) W6 — Union — bond lattice + skin in one field-graph, one contour:**

```bash
dualc_field --expr "union(intersection(onion(normalize(gyroid(wavelength=0.3)),thickness=0.03),box(min=[-0.5,-0.5,-0.5],max=[0.5,0.5,0.5])),onion(box(min=[-0.5,-0.5,-0.5],max=[0.5,0.5,0.5]),thickness=0.03))" -o cube_part.stl --depth 7
```

The plain `union` bonds the lattice to the skin's **inner** wall through their
overlap. To seat the lattice strictly inside the inner wall, clip it to the inner
cavity first — `intersection(lattice-shell, offset(box, -t_skin))` — before the
union.

> All four are field-level compositions: nothing is meshed until the single
> `dualc_field` contour; the genuinely *open* surface of step (a) is the one thing a
> contourer cannot emit, which is why (a) is a raymarch preview —
> [12/05](../../roadmap/12-field-graph-and-app/05-open-surface.md).

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
