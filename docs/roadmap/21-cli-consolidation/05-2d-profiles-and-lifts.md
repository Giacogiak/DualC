# 2D profiles and the two lifts in the graph — the CPU half (#58)

The evidence page of [#58](01-record.md#58-2d-profiles-and-revolve--extrude-in-the-graph),
plan P03 T1, landed 2026-10-15 on branch `feat/cli-consolidation`; the GPU half (P03 T2)
landed the same day, [§ The GPU half](#the-gpu-half-p03-t2). The usage contract is
[11/09](../../command_reference/11-dualc_field/09-2d-profiles-and-lifts.md).

## What landed

- **Six nodes** in `examples/field_graph.{h,cpp}`: the profiles `circle`, `rect`, `segment`,
  `polygon` (`Circle2D`, `Box2D`, `Segment2D`, `Polygon2D`) and the lifts
  `revolve(profile, offset=1)` (`RevolveField`, around y) and `extrude(profile, height=0.5)`
  (`ExtrudeField`, ± `height` along z). The library is untouched; the graph only wires it.
- **One parser, a second builder entry point.** Both readers parse profiles like any node;
  `buildField` dispatches the lifts and refuses a profile, and the lift calls
  `buildProfile(child, lift)`, which returns a `Field2DPtr` and refuses anything that is not
  a profile. Both run the unread-key rule and locate library errors on the node.
- **Typing errors**, located: a profile anywhere but as a lift's one child — *'circle' is a
  2D profile: it is legal only as the one child of 'revolve' or 'extrude'*, on the profile;
  a lift over a 3D (or unknown) node — *'revolve' takes one 2D profile (circle, rect,
  segment, polygon), not 'sphere'*, on the child; arity through the usual
  `requireChildren`; a profile with children is refused.
- **Parameters:** named keys (`center=[x,y]`, `radius`, `half=[x,y]`, `a`, `b`,
  `points=[[x,y],…]`) and the flat `params=[…]` / shorthand positionals, with
  `dualc_lift`'s defaults and its rule that a flat list longer than the arity is refused; a
  `polygon` needs ≥ 3 vertices and takes `points` or `params`, not both. Radii, half extents
  and `extrude`'s `height` must be > 0, every value finite — `dualc_lift` checks none of
  this, so the graph refuses inputs the tool would contour into nothing or a degenerate shape.
- **Arrays of arrays.** `ParamValue` gained `rowSize`: both readers accept one level of
  nested, equal-length number arrays, flattened row by row, and `dumpJson` writes the rows
  back, so `points=[[0,0],[1,0],…]` round-trips as written. Every other reader sees the flat
  vector, so a nested value elsewhere reads as its flattening (`center=[[0,0,0]]` is
  `[0,0,0]`); ragged rows and deeper nesting are parse errors.
- **`--list`** gained a "2D profiles / lifts" group, printed from `profileKinds()`.
- **`field_glsl.cpp` untouched:** a lift reaches its *unknown op* branch, so
  `dualc_field_view` refused the graph (*GLSL codegen does not support op 'revolve'*) until T2.

## Why `rect`

The plan asked for a 2D box token that cannot be confused with the 3D `box`. `rect` is the
word a sketcher uses, reads as 2D without a suffix, and collides with no token in the
registry (`box2` was the alternative the record named; a digit suffix reads as a version).
The same rule keeps `circle`, `segment` and `polygon` safe: none is a 3D primitive's token,
so a profile placed where a 3D node belongs is always a typing error, never a different
shape. `dualc_lift`'s own word stays `box`; its migration (P05) is to map `box` → `rect`.

## Equivalence with `dualc_lift`

Linux, GCC 15, Ninja, Release, `build/examples/`, default depth 7, auto-fit root box (both
tools hand the field to the same `writeField`); `cmp` on the written OBJ.

| `dualc_lift` | Graph | Faces | Result |
| --- | --- | ---: | --- |
| `revolve circle 0 0 0.5 --offset 2` | `revolve(circle(0,0,0.5),offset=2)`, and `circle(center=[0,0],radius=0.5)` | 153,888 | byte-identical |
| `extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` | `extrude(polygon(points=[[0,0],[1,0],[1,1],[0,1]]),height=0.5)`, and `polygon(0,0,1,0,1,1,0,1)` | 164,268 | byte-identical |
| `extrude box 0 0 1 0.5 --height 0.75` | `extrude(rect(half=[1,0.5]),height=0.75)` | — | byte-identical |
| `extrude segment` (defaults) | `extrude(segment)` | — | byte-identical |

SHA-256 of the two required pairs: revolve `3a0d846f…0aa336bee`, extrude
`a46c983c…80330d80d7`. The first equivalence is now a ctest at depth 5:
`cli_field_lift` writes the graph, `cli_field_lift_same` holds it byte-identical to
`cli_lift`'s output (`cmake -E compare_files`), so the retirement of `dualc_lift` (P06) must
move that test to a fixed expectation.

## Tests

`tests/test_field_graph.cpp`, tag `[lift]`: *2D profiles under revolve and extrude build the
library lifts* (each profile by named keys under both lifts, equal to the library field at
140 points and in bounds; the flat and positional forms; `dualc_lift`'s defaults; a lift
under a boolean contours); *a 2D profile outside a lift, or a 3D child under one, is located*
(profile under a boolean, at the root, under a decorator, under a nested `translate`; 3D,
lift and unknown children; arity both ways); *2D profiles and lifts reject bad values with a
located error* (15 cases, plus ragged and doubly nested arrays in both readers); *2D
profiles and lifts round-trip through dumpJson + parseJson* (rows come back as rows; the
hand-written JSON reads to the shorthand's tree; fields equal).

## The GPU half (P03 T2)

- **One reader for both paths.** `field_graph` exports `readProfile(profile, lift)` — the
  profile's values resolved, validated and unread-key-checked, the typing error included —
  and `checkLiftParam` / `liftParamKey` / `throwProfileOutsideLift`; the builder and
  `field_glsl.cpp` both call them (the `checkNodeBake` pattern), so the preview cannot read
  a profile, or fail on one, differently from the export. A ctest holds 11 bad graphs to the
  same pointer and message on both paths.
- **Emission.** A profile is a `float fN(vec2 p)` (`sd2Circle`, `sd2Rect`, `sd2Segment` in
  the prelude, ported from `src/implicit/field2d.cpp`); `revolve` calls it on
  `dcRevolveQ(p, offset)`, `extrude` wraps it in `dcExtrude(d, p.z, height)`
  (`src/implicit/lift.cpp`). Profiles memoize under their own key space, so one profile
  under two lifts emits once. `stepScale` stays 1: the lifts of exact 2D distances are exact.
- **The polygon** is a constant `vec2[N]` array and a loop of exactly `N` — `Polygon2D`'s
  edge distance and crossing-count sign, line for line. The ceiling is
  `kMaxGlslPolygonVertices` = **64** (`field_glsl.h`): above it the codegen refuses with
  *'polygon' has 65 vertices; the GLSL preview takes at most 64 (the export path has no
  ceiling)*, on the polygon. Its vertices are compiled in (an edit recompiles).
- **Bindings.** Scalars are float uniforms — the profiles' `radius`, `revolve`'s `offset`,
  `extrude`'s `height` — so `tab` + `↓`/`↑` nudge them; 2D points (`center`, `half`, `a`,
  `b`) are a new binding type, `vec2`, which `dualc_field_view` and `dualc_glsl_parity`
  upload (a host that ignores an unknown type leaves those uniforms at zero).

**Parity** (`dualc_glsl_parity`, 16³ samples over `[-2,2]³`, the analytic tier `A = R =
2e-3`; Linux, GL renderer NV92, `build-gl` Release): 82/82, the six new cases' max error

| Case | Graph | Max error |
| --- | --- | ---: |
| `extrude-circle` | `extrude(circle(center=[0.2,-0.1],radius=0.7),height=0.6)` | 3.51e-7 |
| `extrude-rect` | `extrude(rect(center=[0,0.1],half=[0.8,0.5]),height=0.4)` | 3.09e-7 |
| `extrude-segment` | `extrude(segment(a=[-1,-0.3],b=[0.8,0.5],radius=0.25),height=0.5)` | 4.86e-7 |
| `extrude-polygon` | a concave L, 6 vertices, `height=0.5` | 3.20e-7 |
| `revolve-circle` | `revolve(circle(radius=0.4),offset=1.2)` | 4.60e-7 |
| `revolve-polygon` | a 5-vertex profile notched at the top, `offset=0.5` | 3.75e-7 |

**Snapshots looked at** (`--snapshot`, 800×600): `revolve-polygon` — a ring with a conical
groove in its top; `extrude-polygon` — the L prism; N21 — the L with its corner carved.
`revolve(circle(radius=0.4),offset=1.2)` renders the same as the `torus(major=1.2,minor=0.4)`
primitive to 1/255 per channel, including ~128 dark pixels where the ring's inner silhouette
grazes; the 3D `torus` shows them too, so they come from the trace on this renderer, not
from the lifts.

---

← Back to the [block README](README.md) · the [record](01-record.md).
