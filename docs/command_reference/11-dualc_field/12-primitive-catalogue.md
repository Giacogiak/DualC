# The primitive catalogue (P1–P30), decorators and domain operators

Part of [Tool 11: `dualc_field`](README.md). Every analytic primitive with its parameters and
defaults, then the decorators and domain operators that wrap one, in graph syntax. It is the
graph form of [`dualc_primitive`](../02-dualc_primitive.md), whose IDs it keeps: every row
here, run as a bare graph, writes the same bytes as the tool's default command
([roadmap 21/07](../../roadmap/21-cli-consolidation/07-docs-migration.md)). The node tables of
[01 § Op vocabulary](01-op-vocabulary.md#op-vocabulary) give the keys; this page gives the
shapes.

## How a primitive is written

- **Bare** — `sphere` or `sphere()` is the registry default of the table below.
- **Positional** — `torus(0,0,0,3,0.5)`: bare numbers fill the flat order of the table
  (`params=[…]` in JSON); omitted trailing values keep their defaults.
- **Grouped keys** (the core seven, [01](01-op-vocabulary.md#primitive-parameters-grouped-keys--flat-fallback)) —
  `torus(center=[0,0,0],major=3,minor=0.5)`.
- `-o` picks OBJ / STL / 3MF by extension (default `field.obj`); `--depth` (7) and
  `--collapse` (off) are `dualc_field`'s options.
- `dualc_field --list` prints the whole vocabulary, as `dualc_primitive --list` printed the
  primitives and their parameter names.

## The 30 analytic primitives

Defaults in the flat order. **∞** = unbounded, needs `--bounds` (the box of the old default
command is given); **open** = an unsigned-distance surface with no inside, needs an `onion` for a wall.

| # | op | Flat order = defaults | Needs |
| --- | --- | --- | --- |
| P1 | `sphere` | cx cy cz radius = 0 0 0 1 | |
| P2 | `box` | minx miny minz maxx maxy maxz = -1 -1 -1 1 1 1 | |
| P3 | `roundbox` | min, max, radius = -1 -1 -1 1 1 1 0.3 | |
| P4 | `capsule` | ax ay az bx by bz radius = -1 0 0 1 0 0 0.5 | |
| P5 | `cappedcylinder` | ax ay az bx by bz radius = 0 -1 0 0 1 0 0.5 | |
| P6 | `torus` | cx cy cz major minor = 0 0 0 1 0.3 | |
| P7 | `ellipsoid` | cx cy cz rx ry rz = 0 0 0 1 0.6 0.4 | |
| P8 | `plane` | nx ny nz offset = 0 1 0 0 | ∞ `--bounds -2,-2,-2,2,2,2` |
| P9 | `boxframe` | min, max, edge = -1 -1 -1 1 1 1 0.1 | |
| P10 | `cone` | cx cy cz angleRad height = 0 1 0 0.5 2 | |
| P11 | `cappedcone` | cx cy cz height radiusLow radiusHigh = 0 0 0 1 1 0.5 | |
| P12 | `roundcone` | ax ay az bx by bz radiusA radiusB = 0 -1 0 0 1 0 0.6 0.3 | |
| P13 | `hexprism` | cx cy cz radius halfLength = 0 0 0 1 1 | |
| P14 | `triprism` | cx cy cz radius halfLength = 0 0 0 1 1 | |
| P15 | `octahedron` | cx cy cz size = 0 0 0 1 | |
| P16 | `pyramid` | cx cy cz height = 0 0 0 1.5 | |
| P17 | `solidangle` | cx cy cz angleRad radius = 0 0 0 0.7 1.5 | |
| P18 | `infinitecylinder` | px py pz dx dy dz radius = 0 0 0 0 1 0 0.5 | ∞ `--bounds -2,-3,-2,2,3,2` |
| P19 | `cappedtorus` | cx cy cz angleRad major minor = 0 0 0 1.0 1 0.3 | |
| P20 | `link` | cx cy cz halfLength major minor = 0 0 0 0.5 1 0.3 | |
| P21 | `cutsphere` | cx cy cz radius cutHeight = 0 0 0 1 0.3 | |
| P22 | `cuthollowsphere` | cx cy cz radius cutHeight thickness = 0 0 0 1 -0.2 0.1 | |
| P23 | `deathstar` | cx cy cz radiusMain radiusBite distance = 0 0 0 1 0.7 0.9 | |
| P24 | `vesica` | ax ay az bx by bz width = 0 -1 0 0 1 0 0.6 | |
| P25 | `rhombus` | cx cy cz la lb height cornerRadius = 0 0 0 1 0.6 0.3 0 | |
| P26 | `verticalcapsule` | cx cy cz height radius = 0 0 0 1.5 0.4 | |
| P27 | `roundedcylinder` | cx cy cz radius roundRadius halfHeight = 0 0 0 1 0.2 1 | |
| P28 | `infinitecone` | cx cy cz angleRad = 0 0 0 0.5 | ∞ `--bounds -3,-3,-3,3,1,3` |
| P29 | `triangle` | ax ay az bx by bz cx cy cz = 0 0 0 1 0 0 0 1 0 | open: `onion(triangle,thickness=0.05)` |
| P30 | `quad` | ax … dz (12 values) | open: `onion(quad,thickness=0.05)` |

- An unbounded graph contoured without `--bounds` exits with *`[dualc] hint: this field is
  unbounded -- pass --bounds x0,y0,z0,x1,y1,z1 …`*. A `repeat` makes any child unbounded.
- **Keep `--bounds` tight**: a box much larger than the shape spends the octree on empty
  space. A `displace` does not grow the bounds, so a bump that pushes past the base shape
  needs a snug box (C4–C6).

## The decorators (one child)

`dualc_primitive`'s post-op flags applied left to right are the nesting order, innermost
first: `--onion 0.1 --twist 1.5,1` is `twist(onion(…))` (C7).

| # | Graph | Was | Effect |
| --- | --- | --- | --- |
| D1 | `offset(sphere,r=0.3)` | `--offset 0.3` | Grow the surface outward by `r` |
| D2 | `round(sphere,r=0.3)` | `--round 0.3` | The same decorator under its second name |
| D3 | `onion(box,thickness=0.1)` | `--onion 0.1` | Hollow shell of wall `thickness` |
| D4 | `scale(sphere,s=2)` | `--scale 2` | Uniform scale about the origin |
| D5 | `elongate(sphere,h=[1,0,0])` | `--elongate 1,0,0` | Stretch by a slab per axis |
| D6 | `translate(box,by=[3,0,0])` | `--translate 3,0,0` | Rigid translation |
| D7 | `rotate(box,axis=[0,0,1],degrees=45)` | `--rotate 0,0,1,45` | Rigid rotation: axis + degrees |

The graph adds `normalize` and `transform` (a 4×4 matrix), which had no flag
([01 § Decorators](01-op-vocabulary.md#decorators--placement-one-child)).

## The domain operators (one child)

| # | Graph | Was | Effect |
| --- | --- | --- | --- |
| O1 | `twist(torus,radiansPerUnit=1.5,axis=y)` | `--twist 1.5,1` | Twist about an axis (the flag's `0`/`1`/`2` is `x`/`y`/`z`) |
| O2 | `bend(box,curvature=0.5,axis=x)` | `--bend 0.5,0` | Bend about an axis |
| O3 | `mirror(sphere(0.6,0,0,0.6),normal=[1,0,0])` | `--mirror 1,0,0` | Mirror across a plane through the origin |
| O4 | `repeat(sphere,period=[3,0,0])` + `--bounds -7,-2,-2,7,2,2` | `--repeat 3,0,0` | Infinite tiling (∞) |
| O5 | `repeat-limited(sphere,period=[3,0,0],count=[4,1,1])` | `--repeat-limited 3,0,0,4,1,1` | Finite tiling: period + counts |
| O6 | `displace(box,fn=sine,amplitude=0.1,frequency=6)` | `--displace sine,0.1,6` | Analytic surface bump |

**Twist / bend axis.** `twist` / `bend` rotate the plane keyed to the chosen axis. Twisting a body of revolution about *its own* axis of symmetry shows
nothing — O1's `twist(torus,…,axis=y)` among them, a torus being invariant under rotation
about y; twist such a shape about another axis, or twist a non-symmetric one (a box twists
visibly about any).

## The three displace functions

| # | `fn` | Graph |
| --- | --- | --- |
| F1 | `sine` | `displace(box,fn=sine,amplitude=0.1,frequency=6)` |
| F2 | `gyroid` | `displace(box,fn=gyroid,amplitude=0.1,frequency=6)` |
| F3 | `bumps` | `displace(box,fn=bumps,amplitude=0.1,frequency=6)` |

`amplitude` is the true peak surface displacement for all three (each function is divided by
its own peak), so `bumps` and `sine` at `0.15` deform the surface by the same ±0.15. The
defaults differ from the old flag's: the node's are `amplitude=0.1`, `frequency=6`; a bare
`--displace sine` meant `0.1, 4`.

Strong displacement adds fine detail the octree must resolve: keep `--bounds` tight and pick
`--depth` so a cell is a few times smaller than the bump
([the resolution rule](../../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist)).
If the output has holes, raise `--depth` and watch the trend: holes that **shrink** were
under-resolution; holes that **persist or grow** (a boundary-edge count stable or rising) mean the bumped surface truly
self-intersects — a property of `SDF + bump`, not of the mesher; lower `amplitude`. `gyroid`
grows handles and tunnels, so it self-intersects at a lower amplitude than the other two.

## Recipes

| # | What it builds |
| --- | --- |
| C1 | Positional override: a radius-2 sphere |
| C2 | Grouped keys: torus of major 3, minor 0.5 |
| C3 | A 4-unit box |
| C4–C6 | Strong sine / bumps / gyroid displacement on the unit sphere (±15 %, ±15 %, ±12 %), bounds snug round the bumps |
| C7 | Composed decorators: a hollow torus, then twisted |

```bash
dualc_field --expr "sphere(0,0,0,2)" -o c1.obj                                    # C1
dualc_field --expr "torus(center=[0,0,0],major=3,minor=0.5)" -o c2.obj            # C2
dualc_field --expr "box(min=[-2,-2,-2],max=[2,2,2])" -o c3.obj                    # C3
dualc_field --expr "displace(sphere,fn=sine,amplitude=0.15,frequency=6)" --bounds -1.4,-1.4,-1.4,1.4,1.4,1.4 -o c4.obj    # C4
dualc_field --expr "displace(sphere,fn=bumps,amplitude=0.15,frequency=6)" --bounds -1.4,-1.4,-1.4,1.4,1.4,1.4 -o c5.obj   # C5
dualc_field --expr "displace(sphere,fn=gyroid,amplitude=0.12,frequency=6)" --bounds -1.4,-1.4,-1.4,1.4,1.4,1.4 -o c6.obj  # C6
dualc_field --expr "twist(onion(torus(0,0,0,2,0.5),thickness=0.1),radiansPerUnit=1.5,axis=y)" -o fancy_torus.obj          # C7
```

The same graphs preview live in [`dualc_field_view`](../12-dualc_field_view/03-recipes-by-node.md#recipes);
on PowerShell put each on one line.

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
