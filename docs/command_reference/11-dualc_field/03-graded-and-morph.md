# Graded shell, graded inflation & spatial morph

Part of [Tool 11: `dualc_field`](README.md). The two-child `graded-onion` /
`graded-offset` decorators, the three-child `mix` morph, the critical callout on
interpolating disjoint fields, and grafting two crystals into one body.

## Graded shell & graded inflation (two children: base, control)

| op | value | params | grows the solid? |
| --- | --- | --- | --- |
| `graded-onion` | `\|base\| − t(control)` | `t1` (req), `t2` (req), `d1` (req), `d0`=0 | no — hollow shell (tube) |
| `graded-offset` | `base − t(control)` | `t1` (req), `t2` (req), `d1` (req), `d0`=0 | **yes — solid inflation** |

Both take **two children** — `in[0]` = **base** (the field being graded),
`in[1]` = **control** (an arbitrary field whose *value* drives the amount `t`).
They share one ramp function `t(control)` and differ only in how they apply it
(hollow vs. inflate). The four scalar params define the ramp:

| param | meaning | default |
| --- | --- | --- |
| `t1` | amount `t` where `control ≤ d0` (the "near" plateau) | required |
| `t2` | amount `t` where `control ≥ d1` (the "far" plateau) | required |
| `d0` | control value at which the ramp **starts** | `0` |
| `d1` | control value at which the ramp **ends** | required |

The ramp is *constant → linear → constant* in **control-field units**:
`t = t1 + (t2 − t1) · clamp((control − d0) / (d1 − d0), 0, 1)`.

The control child is any field, so the grading criterion is whatever you plug in:

| Criterion | control child | value at `p` |
| --- | --- | --- |
| Distance from a point | `sphere(center=[x,y,z], radius=0)` | `\|p − center\|` |
| Linear gradient along an axis | `plane(nx,ny,nz,offset)` | signed distance to the plane |
| Distance from a surface / target volume | `mesh(path=…)` | signed (negative inside); often the clip volume itself |

To put the falloff on the other side, **swap `t1`/`t2`** (or flip the control).
A graded node **dual-contours and exports like a uniform one** — same octree
depth, since grading the amount does not change the lattice period. All four scalars are live
uniforms in [`dualc_field_view`](../12-dualc_field_view/README.md). Graded *wavelength* is
intentionally **not** offered — see [roadmap 13](../../roadmap/13-graded-tpms/README.md).

**`graded-onion` — hollow shell of varying wall thickness.** `|base| − t`
carves a double-sided shell around the base's 0-isosurface whose wall gets
thicker/thinner with the control. Keep `min(t1,t2) > 0` to avoid a zero-thickness
(non-manifold) pinch; the `≈ 2·thickness` metric rule applies to both ends.

**`graded-offset` — solid grown outward (graded strut radius).** `base − t`
level-set-shifts the base **outward** by `t`: a positive `t` inflates the solid,
thickening every strut/wall *without hollowing it*. This is the field-level way
to grade a **solid** lattice — dense (fat struts) near a load path, sparse (thin)
away from it. It composes with any solid field, not only struts.

- **Local radius.** For a strut base of `radius = r₀`, the graded strut half-
  thickness is `≈ r₀ + t(control)`. Example: `bcc(radius=0.02)` wrapped in
  `graded-offset(…, t1=0.0, t2=0.06, …)` runs from `0.02` at the near plateau to
  `0.08` at the far plateau (4× thicker). Set `t1=0` to keep the base radius
  unchanged there; set `t1>0` to inflate everywhere.
- **`t1 == t2 == T`** collapses exactly to a uniform `offset(base, r=T)` — a
  handy sanity check (identical mesh, byte-for-byte).
- **Don't confuse with `onion`.** `graded-onion`/`onion` *hollow* a strut into a
  tube (recipe S8 on [the strut page](02-strut-lattices.md#recipes)); `graded-
  offset`/`offset` keep it *solid* and grow it. Use `graded-offset` for a graded
  **solid** lattice.

The graded strut-radius recipes are S9–S12 on [the strut page](02-strut-lattices.md#recipes).

## Spatial morph (three children: A, B, control)

| op | value | params |
| --- | --- | --- |
| `mix` | `lerp(A, B, w)`, `w = clamp((control − lo)/(hi − lo), 0, 1)` | `lo`=0, `hi` (req) |

`mix` **linearly blends the two fields' *values* across space**: `in[0]` = **A**,
`in[1]` = **B**, `in[2]` = **control**. Where `control ≤ lo` the field is pure
**A**; where `control ≥ hi` it is pure **B**; in the band between it is
`lerp(A, B, w)`. The `control` child follows the same convention as the graded ops
(a `plane` for a linear gradient along an axis, `sphere(radius=0)` for distance
from a point, a `mesh` for distance from a surface). Both `lo`/`hi` are live
uniforms in [`dualc_field_view`](../12-dualc_field_view/README.md).

> **⚠ `mix` blends values, not shapes.** Two solids that coincide (one family at
> two radii) blend into one body — a graded radius, for which
> [`graded-offset`](#graded-shell--graded-inflation-two-children-base-control) is
> the cheaper equivalent. Two *different* crystals (`bcc` ↔ `fcc`, …) taper to
> nothing around `w ≈ 0.5` and come out as **two bodies with a gap slab** —
> inherent to interpolating disjoint distance fields, not a bug, and no `--depth`
> fixes it ([design/10 § `mix`](../../design/10-invariants-and-tolerances.md#mix-blends-values-not-shapes)). To bond two
> different crystals, [graft them](#grafting-two-different-crystals-continuous-body);
> use `mix` there only for a deliberately faded, gapped transition.

- **Why.** The [hard-seam recipe](02-strut-lattices.md#examples--recipes) switches crystals at a
  discontinuous plane, and the [compound cell](02-strut-lattices.md#examples--recipes) piles both on
  top of each other. `mix` is the third option — a smooth **density/radius
  gradient within one family**, e.g. a dense load-bearing `bcc` thinning toward a
  free surface, or a dense `octet` core fading to an open `octet` shell.
- **`mix` vs `graded-offset` for a density gradient.** To grade the **radius of a
  single lattice**, prefer [`graded-offset`](#graded-shell--graded-inflation-two-children-base-control):
  it inflates one exact-SDF field, so it stays metric and **prunes empty
  space efficiently**. Reach for `mix` when the two endpoints are genuinely
  *different fields* that no single `graded-offset` can express (between two
  different crystals: a gap — [graft](#grafting-two-different-crystals-continuous-body)
  for a continuous one). (`mix(bcc(r1), bcc(r2), …)` also grades radius and is handy
  for a quick two-endpoint blend, but it refines densely; `graded-offset` is the
  cheaper tool for that job.)
- **Not an SDF.** A lerp of two distance fields is not itself a distance, but it
  is a valid implicit for dual contouring (its sign change bounds the surface).
  Like every blend, the QEF normal drops the slowly-varying `∇w` term. If either
  child is non-metric (a raw TPMS / `winding`), `normalize` it first, exactly as
  for the smooth booleans.
- **How to pick `control` / `lo` / `hi`.** `control` is *any* field; its **value**
  (not its surface) drives the blend, so choose it for the transition shape you
  want, then set `lo`/`hi` to the control values that bracket the band:
  - *Linear gradient along an axis* — `plane(1,0,0,0)` gives
    `control = x`; `lo`/`hi` are the two x-planes the morph runs between.
  - *Radial (core → shell)* — `sphere(center=[…],radius=0)` gives `control = |p −
    center|`; `lo`/`hi` are the two radii. A dense core fading to an open shell.
  - *Distance from a surface* — `mesh(path=…)` gives signed distance (negative
    inside); morph by proximity to another part or to the clip volume itself.

  Where `control ≤ lo` you get pure **A**, so put the crystal you want in the
  "near" region as **A**. Swap A/B (or flip `lo`/`hi`) to reverse the direction. A
  **narrow** `[lo,hi]` gives a fast, crisp transition (and — between *different*
  crystals — a thinner gap slab); a **wide** band gives a long, gradual blend.
  `--depth` follows the *finer* of the two lattices, exactly as for a plain strut
  node.
- **Performance — `mix` refines conservatively.** The blended surface floats free
  of both operands, so `mix` refines **every** cell inside its bounds to `--depth`,
  like a raw TPMS (why: [design/10 § `mix`](../../design/10-invariants-and-tolerances.md#mix-blends-values-not-shapes)).
  Always **clip it** — `intersection(box(…), mix(…))` or `intersection(mesh(…), mix(…))` —
  so the dense work is confined to the part; an unclipped `mix` over a large
  `--bounds` is slow.

The morph recipes are S5–S7 on [the strut page](02-strut-lattices.md#recipes).

### Grafting two different crystals (continuous body)

`mix` **cannot** bond two *different* crystals into one solid (it leaves a gap and
two bodies — [design/10 § `mix`](../../design/10-invariants-and-tolerances.md#mix-blends-values-not-shapes)). The reliable way to get a **single
continuous body that transitions from one crystal to another** is to **graft**: clip
each crystal to an **overlapping** region and `union` them, so in the overlap band
*both* lattices are present and their struts physically cross and fuse. The seam is
a denser, mechanically bonded band rather than a hollow gap.

The knob is the **overlap width**: give each half-region an extra `±δ` past the
nominal seam plane. A larger `δ` = a wider, stronger bond band (and more material);
`δ ≈ one strut wavelength` is a good start so at least one full cell of each crystal
interpenetrates.

## Recipes

| # | What it builds |
| --- | --- |
| J1 | Graft: `bcc` (x < 0) fused to `fcc` (x > 0) across an overlap band — one continuous body |
| J2 | The same graft with `smooth-union` fillets at the bonds |

```bash
# J1 — GRAFT — bcc (x<0 region) fused to fcc (x>0 region), welded across an overlap
# band x in [-0.15, 0.15] (delta = 0.15). Both crystals exist in that band, so
# their struts cross and bond -> ONE continuous body, no gap. This is the correct
# way to transition between two DIFFERENT crystals:
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),union(intersection(box(min=[-1.2,-1.2,-1.2],max=[0.15,1.2,1.2]),bcc(wavelength=0.5,radius=0.06)),intersection(box(min=[-0.15,-1.2,-1.2],max=[1.2,1.2,1.2]),fcc(wavelength=0.5,radius=0.06))))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o graft_bcc_fcc.obj

# J2 — Same graft with rounded fillets at the bonds -- swap union -> smooth-union (k =
# blend radius, world units). Softer, print-friendlier junctions:
dualc_field --expr 'intersection(box(min=[-1,-1,-1],max=[1,1,1]),smooth-union(intersection(box(min=[-1.2,-1.2,-1.2],max=[0.15,1.2,1.2]),bcc(wavelength=0.5,radius=0.06)),intersection(box(min=[-0.15,-1.2,-1.2],max=[1.2,1.2,1.2]),fcc(wavelength=0.5,radius=0.06)),k=0.08))' \
            --bounds -1.1,-1.1,-1.1,1.1,1.1,1.1 --depth 7 -o graft_bcc_fcc_smooth.obj
```

> **Rule of thumb.** Continuous body across *different*
> crystals ⇒ **graft** (overlap + `union`/`smooth-union`). Smooth *density/radius*
> gradient within *one* crystal family ⇒ `mix` (or the cheaper `graded-offset`).
> Use `mix` between different crystals **only** when a faded, intentionally
> discontinuous transition is the desired look.

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
