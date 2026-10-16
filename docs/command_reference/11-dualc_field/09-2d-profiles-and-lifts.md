# 2D profiles and lifts (`revolve` · `extrude`)

Part of [Tool 11: `dualc_field`](README.md). A **profile** is a 2D field drawn in a plane
(`circle`, `rect`, `segment`, `polygon` — the library's `include/dualc/implicit2d.h`); a
**lift** turns it into a 3D node. They replaced
[`dualc_lift`](../04-dualc_lift.md), retired 2026-10-16: the same profiles, the same defaults,
and the same recipe wrote a byte-identical mesh
([roadmap 21/05](../../roadmap/21-cli-consolidation/05-2d-profiles-and-lifts.md)).

`dualc_field_view` previews the same graphs: the profile's scalars (`radius`) and the lift's
(`offset`, `height`) are live uniforms (`tab` + `↓`/`↑`), its points are `vec2` uniforms, and
a `polygon`'s vertices are compiled in — the preview takes **at most 64 vertices** and refuses
more with a located error (the export path has no ceiling). Recipes N20–N21 in
[12/03](../12-dualc_field_view/03-recipes-by-node.md#2d-profiles-under-the-lifts).

## The two lifts

Each takes **exactly one child, and that child must be a profile**.

| op | params | builds |
| --- | --- | --- |
| `revolve` | `offset`=1 | Spins the profile around the **y** axis: profile `x` = distance from the axis − `offset`, profile `y` = world `y`. A circle at the origin with `offset=2` is a torus of major radius 2. |
| `extrude` | `height`=0.5 | Sweeps the profile (in the xy plane) along **z** by ± `height` — `height` is the **half**-height, as `dualc_lift --height` was. Must be > 0. |

## The four profiles

Named keys, or the flat `params=[…]` order (also the shorthand's bare positionals). The
defaults are the retired `dualc_lift`'s.

| op | named keys (defaults) | flat order |
| --- | --- | --- |
| `circle` | `center`:[x,y]=[0,0], `radius`=0.5 | `cx cy radius` |
| `rect` | `center`:[x,y]=[0,0], `half`:[x,y]=[1,0.5] (half extents) | `cx cy halfx halfy` |
| `segment` | `a`:[x,y]=[-1,0], `b`:[x,y]=[1,0], `radius`=0.3 (a 2D capsule) | `ax ay bx by radius` |
| `polygon` | `points`:[[x,y],…] — **required**, ≥ 3 vertices | `x1 y1 x2 y2 x3 y3 …` |

- **`rect`, not `box`:** `box` is the 3D primitive's token; a profile token never shadows a
  3D one, so a misplaced profile is always a typing error and never a silently different
  shape ([design/12](../../design/12-field-graph.md#the-2d-layer)).
- A flat list **shorter** than the profile's arity overrides from the front (the rest keep
  their defaults), as on a 3D primitive; a **longer** one is refused.
- `polygon` takes `points` as rows (`[[0,0],[1,0],[1,1]]`) or flat (`[0,0,1,0,1,1]`), or
  `params=[…]` — one of them, not both. Vertices in order, either winding; concave is fine
  (the sign comes from a crossing count), self-intersecting is not. `--dump-json` writes
  `points` back as rows.
- Radii and half extents must be > 0, every value finite.

**`dualc_lift`'s recipes, one by one.** Each graph wrote the same bytes as the tool's line
(defaults on both sides: `offset` 1, `height` 0.5, the profile defaults above); `-o` replaces
the tool's `revolved.obj` / `extruded.obj` default, `--depth` / `--collapse` are the options.

| # | `dualc_lift` | Graph |
| --- | --- | --- |
| L1 | `revolve circle --offset 2` | `revolve(circle,offset=2)` — a torus |
| L2 | `revolve box --offset 2` | `revolve(rect,offset=2)` — a box-section ring |
| L3 | `revolve segment --offset 2` | `revolve(segment,offset=2)` — a capsule-section ring |
| L4 | `revolve polygon 0,0 1,0 1,1 0,1 --offset 2` | `revolve(polygon(0,0,1,0,1,1,0,1),offset=2)` |
| L5 | `extrude circle --height 0.5` | `extrude(circle,height=0.5)` — a cylinder |
| L6 | `extrude box --height 0.5` | `extrude(rect,height=0.5)` — a box |
| L7 | `extrude segment --height 0.5` | `extrude(segment,height=0.5)` — a capsule slab |
| L8 | `extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` | `extrude(polygon(0,0,1,0,1,1,0,1),height=0.5)` — a prism |

## Typing errors

Located on the offending node, like every graph error:

```
dualc_field --expr "union(sphere,circle(radius=1))"
[dualc_field] field-graph error at 13: 'circle' is a 2D profile: it is legal only as the one child of 'revolve' or 'extrude'
dualc_field --expr "revolve(sphere)"
[dualc_field] field-graph error at 8: 'revolve' takes one 2D profile (circle, rect, segment, polygon), not 'sphere'
```

A lift is a 3D node like any other: it goes under booleans, decorators and domain ops.

## Recipes

| # | What it builds |
| --- | --- |
| R1 | `dualc_lift revolve circle 0 0 0.5 --offset 2` as a graph — a torus, byte-identical |
| R2 | `dualc_lift extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` as a graph — byte-identical |
| R3 | An L-shaped (concave) prism, the JSON form |
| R4 | A revolved ring with a slot cut through it — a lift under a boolean |

```bash
# R1 — the torus of dualc_lift's first recipe:
dualc_field --expr "revolve(circle(center=[0,0],radius=0.5),offset=2)" -o torus.obj

# R2 — the unit-square prism (rows, or flat: polygon(0,0,1,0,1,1,0,1)):
dualc_field --expr "extrude(polygon(points=[[0,0],[1,0],[1,1],[0,1]]),height=0.5)" -o prism.obj

# R3 — a concave profile in JSON, piped in on stdin:
echo '{"op":"extrude","height":0.25,"in":[{"op":"polygon",
  "points":[[0,0],[1,0],[1,0.4],[0.4,0.4],[0.4,1],[0,1]]}]}' | dualc_field - -o ell.obj

# R4 — a rectangular-section ring, slotted by a box:
dualc_field --expr "difference(revolve(rect(half=[0.3,0.5]),offset=1.5),box(min=[1,-1,-0.2],max=[2,1,0.2]))" -o slotted.obj
```

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
