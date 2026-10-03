# Recipes by node category (the full vocabulary)

Part of [Tool 12: `dualc_field_view`](README.md). One ready-to-run preview per node
family. All are `--expr` one-liners; on PowerShell put each on one line (the
[quoting rule](../../design/09-conventions.md#windows-powershell-51-quoting)).

## Recipes

| # | Node family | What it shows |
| --- | --- | --- |
| N1–N2 | analytic primitives | Grouped-key core primitives (`capsule`, `ellipsoid`) |
| N3–N6 | analytic primitives | Positional long-tail primitives (`octahedron`, `hexprism`, `cappedtorus`, `link`) |
| N7–N9 | analytic primitives | A bare `op()` — the registry default shape |
| N10–N11 | analytic primitives | Infinite primitives with the `--bounds` they require |
| N12 | analytic primitives | An open primitive (`quad`) given a wall by `onion` |
| N13–N17 | domain operators | `mirror`, `repeat` (needs `--bounds`), `repeat-limited`, `displace`, `round` |
| N18–N19 | `winding` | The GWN solid of a mesh; a gyroid shell clipped to it |

### Analytic primitives (mesh-free, instant)

All 30 primitives render. The seven core primitives take grouped named keys,
every other one its parameters positionally in the
[`dualc_primitive`](../02-dualc_primitive.md) order, and a bare `op()` uses the
registry defaults — [design/09 § Primitive parameters](../../design/09-conventions.md#primitive-parameters-grouped-keys-positional-fallback-one-registry).

```bash
# N1, N2 — grouped-key (core) primitives
dualc_field_view --expr "capsule(a=[-1,0,0],b=[1,0,0],radius=0.4)"
dualc_field_view --expr "ellipsoid(center=[0,0,0],radii=[1.2,0.7,0.5])"

# N3–N6 — positional (long-tail) primitives -- args follow the dualc_primitive page's order
dualc_field_view --expr "octahedron(0,0,0,1.2)"            # cx cy cz size
dualc_field_view --expr "hexprism(0,0,0,0.8,1.2)"          # cx cy cz radius halfLength
dualc_field_view --expr "cappedtorus(0,0,0,2.0,1.0,0.3)"   # cx cy cz angleRad major minor
dualc_field_view --expr "link(0,0,0,0.5,0.6,0.25)"         # cx cy cz halfLength major minor

# N7–N9 — bare op() == the registry default shape (always valid)
dualc_field_view --expr "deathstar()"
dualc_field_view --expr "pyramid()"
dualc_field_view --expr "roundedcylinder()"

# N10, N11 — infinite primitives REQUIRE --bounds (they have no finite bounds()); positional
dualc_field_view --expr "plane(0,1,0,0)" --bounds -2,-2,-2,2,2,2   # nx ny nz offset
dualc_field_view --expr "infinitecone(0,0,0,0.5)" --bounds -2,-2,-2,2,2,2   # cx cy cz angleRad

# N12 — open primitives (triangle/quad) are unsigned -- wrap in onion to give a wall
dualc_field_view --expr "onion(quad(),thickness=0.05)"
```

### Domain operators (warp / tile a child field)

```bash
# N13 — mirror -- reflect a child across a plane through the origin (normal is required;
# it is unit-normalised internally). Asymmetric child shows the duplicated half.
dualc_field_view --expr "mirror(sphere(center=[0.8,0,0],radius=0.5),normal=[1,0,0])"

# N14 — repeat -- INFINITE tiling, so it needs --bounds; a lattice of spheres
dualc_field_view --expr "repeat(sphere(radius=0.3),period=[1,1,1])" --bounds -3,-3,-3,3,3,3

# N15 — repeat-limited -- a FINITE 3x3x3 block (auto-bounds, no --bounds needed)
dualc_field_view --expr "repeat-limited(sphere(radius=0.3),period=[1,1,1],count=[3,3,3])"

# N16 — displace -- add a sine/gyroid/bumps ripple to a child's surface
#   (amplitude is the true peak displacement; default fn=sine amplitude=0.1 frequency=6)
dualc_field_view --expr "displace(sphere(radius=1),fn=gyroid,amplitude=0.15,frequency=6)"

# N17 — round -- the offset alias; round(f,r) = f - r (here: a rounded box == roundbox)
dualc_field_view --expr "round(box(min=[-1,-1,-1],max=[1,1,1]),r=0.3)"
```

`twist` / `bend` and these warps all carry a reduced `uStepScale` so the
sphere-trace stays safe through the non-metric warp — no flag needed.

### `winding` — generalized-winding-number mesh soups

`winding(path="…")` reconstructs a **solid** from a triangle mesh via the 0.5-GWN
isosurface — **robust to non-watertight input / soup** (where ray-parity `mesh`
would mis-sign). It bakes to a `sampler3D` for preview exactly like `mesh` (so
`--grid-res` controls its resolution); its bounds are the padded mesh AABB, so no
`--bounds` is needed. It benefits more than `mesh` from a higher `--grid-res`: the
GWN field is nearly a step function, so a large model shows visible voxel terracing
at `96³` that `--grid-res 256` removes. Run from the repo root so `data/…` resolves.

```bash
# N18 — the solid GWN reconstruction of a mesh
dualc_field_view --expr 'winding(path="data/bunny.obj")'

# N19 — mix a soup volume with an analytic lattice: a gyroid shell clipped to the GWN solid
dualc_field_view --grid-res 128 --expr 'intersection(onion(normalize(gyroid(wavelength=0.08)),thickness=0.01),winding(path="data/bunny.obj"))'
```

The same two in PowerShell 5.1 form (path quotes `\"`):

```powershell
.\dualc_field_view.exe --expr 'winding(path=\"data/bunny.obj\")'
.\dualc_field_view.exe --grid-res 128 --expr 'intersection(onion(normalize(gyroid(wavelength=0.08)),thickness=0.01),winding(path=\"data/bunny.obj\"))'
```

> `mesh` vs `winding`: use **`mesh`** for clean watertight, oriented input (faster,
> exact narrow-band bake); use **`winding`** for soup / open / self-intersecting
> input. `mesh(sign=gwn)` is intentionally rejected — route GWN through `winding`.

### DAG dedup is automatic

If a graph names the **same subexpression twice** — e.g. the same `mesh`/`winding`
as both a clip and a skin, or one lattice feeding two booleans — the codegen emits
that subtree **once**: a single GPU function, a single shared uniform set, and (for
a mesh/winding source) a **single `sampler3D` bake** rather than two. You write the
natural tree; nothing special is needed. A shared scalar therefore moves *all* its
uses together when you nudge it (`tab` + `↓`/`↑`).

---

← Back to the [Tool 12 overview](README.md) · the [Command Reference index](../README.md).
