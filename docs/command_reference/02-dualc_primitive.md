# Tool 2: `dualc_primitive`

Analytic primitives + decorators + domain operators.

```
dualc_primitive <name> [params...] [post-ops...] [options]
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `<name> [params...]` | (required) | Primitive token and its positional parameters (tables below; omitted trailing params keep their defaults). |
| `-o PATH` | `<name>.obj` | Output; `.obj` / `.stl` / `.3mf` by extension ([export formats](README.md#export-formats--o-extension-dispatch)). |
| `--depth N` | `7` | Octree max depth. |
| `--collapse E` | `0` (off) | Adaptive cell-collapse threshold. |
| `--bounds x0,y0,z0,x1,y1,z1` | primitive AABB | Explicit root box — required for the unbounded `plane` / `infinitecone`. |
| `--list` | — | Print every primitive and its parameters, then exit. |
| post-op flags | — | Decorators and domain operators, applied left to right (sections below). |

## The 30 analytic 3D primitives — one by one

### Tier A — core 8

| # | Primitive | Default command | Default parameters |
| --- | --- | --- | --- |
| P1 | `sphere` | `dualc_primitive sphere` | cx cy cz radius = 0 0 0 1 |
| P2 | `box` | `dualc_primitive box` | minx miny minz maxx maxy maxz = -1 -1 -1 1 1 1 |
| P3 | `roundbox` | `dualc_primitive roundbox` | min/max + radius = -1 -1 -1 1 1 1 0.3 |
| P4 | `capsule` | `dualc_primitive capsule` | ax ay az bx by bz radius = -1 0 0 1 0 0 0.5 |
| P5 | `cappedcylinder` | `dualc_primitive cappedcylinder` | ax ay az bx by bz radius = 0 -1 0 0 1 0 0.5 |
| P6 | `torus` | `dualc_primitive torus` | cx cy cz major minor = 0 0 0 1 0.3 |
| P7 | `ellipsoid` | `dualc_primitive ellipsoid` | cx cy cz rx ry rz = 0 0 0 1 0.6 0.4 |
| P8 | `plane` | `dualc_primitive plane --bounds -2,-2,-2,2,2,2` | nx ny nz offset = 0 1 0 0 (infinite — needs `--bounds`) |

### Tier B — common 10

| # | Primitive | Default command | Default parameters |
| --- | --- | --- | --- |
| P9 | `boxframe` | `dualc_primitive boxframe` | min/max + edge = -1 -1 -1 1 1 1 0.1 |
| P10 | `cone` | `dualc_primitive cone` | cx cy cz angleRad height = 0 1 0 0.5 2 |
| P11 | `cappedcone` | `dualc_primitive cappedcone` | cx cy cz height radiusLow radiusHigh = 0 0 0 1 1 0.5 |
| P12 | `roundcone` | `dualc_primitive roundcone` | ax ay az bx by bz radiusA radiusB = 0 -1 0 0 1 0 0.6 0.3 |
| P13 | `hexprism` | `dualc_primitive hexprism` | cx cy cz radius halfLength = 0 0 0 1 1 |
| P14 | `triprism` | `dualc_primitive triprism` | cx cy cz radius halfLength = 0 0 0 1 1 |
| P15 | `octahedron` | `dualc_primitive octahedron` | cx cy cz size = 0 0 0 1 |
| P16 | `pyramid` | `dualc_primitive pyramid` | cx cy cz height = 0 0 0 1.5 |
| P17 | `solidangle` | `dualc_primitive solidangle` | cx cy cz angleRad radius = 0 0 0 0.7 1.5 |
| P18 | `infinitecylinder` | `dualc_primitive infinitecylinder --bounds -2,-3,-2,2,3,2` | px py pz dx dy dz radius = 0 0 0 0 1 0 0.5 (infinite — needs `--bounds`) |

### Tier C — long tail 12

| # | Primitive | Default command | Default parameters |
| --- | --- | --- | --- |
| P19 | `cappedtorus` | `dualc_primitive cappedtorus` | cx cy cz angleRad major minor = 0 0 0 1.0 1 0.3 |
| P20 | `link` | `dualc_primitive link` | cx cy cz halfLength major minor = 0 0 0 0.5 1 0.3 |
| P21 | `cutsphere` | `dualc_primitive cutsphere` | cx cy cz radius cutHeight = 0 0 0 1 0.3 |
| P22 | `cuthollowsphere` | `dualc_primitive cuthollowsphere` | cx cy cz radius cutHeight thickness = 0 0 0 1 -0.2 0.1 |
| P23 | `deathstar` | `dualc_primitive deathstar` | cx cy cz radiusMain radiusBite distance = 0 0 0 1 0.7 0.9 |
| P24 | `vesica` | `dualc_primitive vesica` | ax ay az bx by bz width = 0 -1 0 0 1 0 0.6 |
| P25 | `rhombus` | `dualc_primitive rhombus` | cx cy cz la lb height cornerRadius = 0 0 0 1 0.6 0.3 0 |
| P26 | `verticalcapsule` | `dualc_primitive verticalcapsule` | cx cy cz height radius = 0 0 0 1.5 0.4 |
| P27 | `roundedcylinder` | `dualc_primitive roundedcylinder` | cx cy cz radius roundRadius halfHeight = 0 0 0 1 0.2 1 |
| P28 | `infinitecone` | `dualc_primitive infinitecone --bounds -3,-3,-3,3,1,3` | cx cy cz angleRad = 0 0 0 0.5 (infinite — needs `--bounds`) |
| P29 | `triangle` | `dualc_primitive triangle --onion 0.05` | ax ay az bx by bz cx cy cz = 0 0 0 1 0 0 0 1 0 (open surface — needs `--onion`) |
| P30 | `quad` | `dualc_primitive quad --onion 0.05` | ax..dz (12 values) (open surface — needs `--onion`) |

Notes:
- `plane`, `infinitecylinder`, `infinitecone` are **unbounded** — they require an explicit `--bounds`. Any `--repeat` post-op makes the field infinite too. `--bounds` takes six doubles `x0,y0,z0,x1,y1,z1`; omitting it on an unbounded field exits with `[dualc] error: …` followed by `[dualc] hint: this field is unbounded -- pass --bounds x0,y0,z0,x1,y1,z1 to give the sampler an explicit finite region.`
- `triangle`, `quad` are **open surfaces** (unsigned distance, no inside) — they require `--onion T` to gain thickness.
- **Keep `--bounds` tight.** A root box much larger than the geometry wastes resolution on empty space and coarsens the cells. `--displace` does not enlarge `bounds()`, so a box is needed once a bump pushes past the base shape — make it snug, not huge.

Overriding parameters — supply positional doubles after the name (single tokens or
`comma,groups`, negatives allowed): recipes C1–C3 in [Recipes](#recipes).

## The 6 decorators — one by one

Appended as post-op flags after the primitive; applied left to right.

| # | Decorator | Default command | Effect |
| --- | --- | --- | --- |
| D1 | `--offset O` | `dualc_primitive sphere --offset 0.3` | Grow the surface outward by `O`. |
| D2 | `--round O` | `dualc_primitive sphere --round 0.3` | Alias of `--offset`. |
| D3 | `--onion T` | `dualc_primitive box --onion 0.1` | Hollow shell, wall thickness `T`. |
| D4 | `--scale S` | `dualc_primitive sphere --scale 2` | Uniform scale about the origin. |
| D5 | `--elongate x,y,z` | `dualc_primitive sphere --elongate 1,0,0` | Stretch by a slab per axis. |
| D6 | `--translate x,y,z` | `dualc_primitive box --translate 3,0,0` | Rigid translation. |
| D7 | `--rotate x,y,z,deg` | `dualc_primitive box --rotate 0,0,1,45` | Rigid rotation: axis + degrees. |

(`--offset` and `--round` are the same decorator under two names — 6 distinct decorators.)

## The 6 domain operators — one by one

| # | Operator | Default command | Effect |
| --- | --- | --- | --- |
| O1 | `--twist k,axis` | `dualc_primitive torus --twist 1.5,1` | Twist about an axis (0=x,1=y,2=z). |
| O2 | `--bend k,axis` | `dualc_primitive box --bend 0.5,0` | Bend about an axis. |
| O3 | `--mirror x,y,z` | `dualc_primitive sphere 0.6,0,0 0.6 --mirror 1,0,0` | Mirror across a plane normal. |
| O4 | `--repeat x,y,z` | `dualc_primitive sphere --repeat 3,0,0 --bounds -7,-2,-2,7,2,2` | Infinite tiling (needs `--bounds`). |
| O5 | `--repeat-limited x,y,z,nx,ny,nz` | `dualc_primitive sphere --repeat-limited 3,0,0,4,1,1` | Finite tiling: period + counts. |
| O6 | `--displace name,amp,freq` | `dualc_primitive box --displace sine,0.1,6` | Analytic surface bump. |

**Twist/bend axis.** `--twist`/`--bend` rotate the plane keyed to the chosen
axis. Twisting a body of revolution about *its own axis of symmetry* has no
visible effect (e.g. `torus --twist k,1` — a torus is invariant under rotation
about y). Twist such shapes about a different axis, or twist a non-symmetric
shape (a box twists visibly about any axis).

## The 3 displace functions (argument to `--displace`)

| # | Function | Default command |
| --- | --- | --- |
| F1 | `sine` | `dualc_primitive box --displace sine,0.1,6` |
| F2 | `gyroid` | `dualc_primitive box --displace gyroid,0.1,6` |
| F3 | `bumps` | `dualc_primitive box --displace bumps,0.1,6` |

Each is parsed as `name,amplitude,frequency`. `amplitude` is the true peak
surface displacement for all three — the functions are internally normalized,
so `--displace bumps,0.15` and `--displace sine,0.15` deform the surface by the
same ±0.15.

Strong displacement is fully supported, but the displaced surface gains fine
detail the octree must resolve — keep `--bounds` tight around the shape and set
`--depth` so the cell is a few times smaller than the displacement detail (the
[resolution rule](../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist)). If the output has holes, raise `--depth` and re-check —
the trend is the diagnostic:

- holes **shrink or vanish** → it was under-resolution; keep the higher depth
  (or tighten `--bounds`).
- holes **persist or grow** (boundary-edge count stable or rising) → the
  displacement is strong enough that the surface genuinely self-intersects. That
  is a real property of `SDF + bump`, not a meshing artifact — reduce `amplitude`.

`gyroid` is the gyroid minimal-surface pattern; blended strongly onto a shape it
genuinely grows handles and tunnels, so it self-intersects at a lower amplitude
than `sine` / `bumps` — give it a gentler `amplitude`.

Clean strong examples (unit sphere, ±15 % / ±12 % displacement) are C4–C6.

## Recipes

| # | Command | What it builds |
| --- | --- | --- |
| C1 | `dualc_primitive sphere 0 0 0 2` | Positional override: a radius-2 sphere. |
| C2 | `dualc_primitive torus 0 0 0 3 0.5` | Positional override: major 3, minor 0.5. |
| C3 | `dualc_primitive box -2,-2,-2 2,2,2` | Comma groups (negatives allowed): a 4-unit box. |
| C4 | `dualc_primitive sphere --displace sine,0.15,6 --bounds -1.4,-1.4,-1.4,1.4,1.4,1.4` | Strong sine displacement, bounds snug around the bumps. |
| C5 | `dualc_primitive sphere --displace bumps,0.15,6 --bounds -1.4,-1.4,-1.4,1.4,1.4,1.4` | Same amplitude with `bumps`. |
| C6 | `dualc_primitive sphere --displace gyroid,0.12,6 --bounds -1.4,-1.4,-1.4,1.4,1.4,1.4` | `gyroid` at the gentler amplitude it needs. |
| C7 | `dualc_primitive torus 0 0 0 2 0.5 --onion 0.1 --twist 1.5,1 -o fancy_torus.obj` | Composed post-ops, applied left to right: hollow, then twisted. |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
