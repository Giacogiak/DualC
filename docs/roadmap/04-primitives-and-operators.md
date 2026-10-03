# Primitives & operators

The analytic primitive library (`dualc_primitive`), the decorators and domain
operators that edit them, and the 2D field layer with revolve/extrude lifts
(`dualc_lift`). Built on the [implicit/SDF foundation](02-implicit-sdf-foundation.md);
full per-command catalogue in
[../command_reference/02-dualc_primitive.md](../command_reference/02-dualc_primitive.md)
and [../command_reference/04-dualc_lift.md](../command_reference/04-dualc_lift.md).

> **Decorators** (`offset` / `round` / `onion` / `elongate` / `transform` / `scale`)
> were delivered in v2 Phase 2 alongside the booleans — see
> [03-booleans-csg.md](03-booleans-csg.md#phase-2--combinators--non-domain-decorators--done).

## Phase 3 — Analytic primitive library — DONE

Delivered:
- `BBox::infinite()` / `BBox::isInfinite()` in `types.h` — the sentinel for
  unbounded fields. `sampleFieldToHermiteOctree` throws `std::invalid_argument`
  when a root field reports infinite bounds and no `rootBounds` is supplied;
  `bboxUnion` propagates the sentinel.
- `include/dualc/primitives.h` — `PrimitiveField` base (central-difference
  gradient default) plus all 30 primitive classes, public and constructed
  via `std::make_shared<XxxField>(...)`.
- `src/implicit/primitives_tierA.cpp` — Tier A core 8: `SphereField`,
  `BoxField`, `RoundBoxField`, `PlaneField`, `CapsuleField`,
  `CappedCylinderField`, `TorusField`, `EllipsoidField`.
- `src/implicit/primitives_tierB.cpp` — Tier B common 10: `BoxFrameField`,
  `ConeField`, `CappedConeField`, `RoundConeField`, `InfiniteCylinderField`,
  `HexPrismField`, `TriPrismField`, `OctahedronField`, `PyramidField`,
  `SolidAngleField`.
- `src/implicit/primitives_tierC.cpp` — Tier C long tail 12: `CappedTorusField`,
  `LinkField`, `CutSphereField`, `CutHollowSphereField`, `DeathStarField`,
  `VesicaSegmentField`, `RhombusField`, `VerticalCapsuleField`,
  `RoundedCylinderField`, `TriangleField`, `QuadField`, `InfiniteConeField`.

Each primitive provides a closed-form `valueAt` (faithful to the Quilez
distance-functions article) and `bounds`; `gradientAt` is the `PrimitiveField`
central difference except `SphereField`/`PlaneField`, which override it with
the exact form. `isInside`/`edgeHit`/`cellOverlaps` inherit the defaults.

Notes carried through:
- Infinite-bounds primitives (`PlaneField`, `InfiniteCylinderField`,
  `InfiniteConeField`) return the `BBox::infinite()` sentinel.
- Open-surface primitives (`TriangleField`, `QuadField`) are unsigned-distance
  surfaces — usable only through `onionOf` / a closing op, documented in the
  header.
- The unit-cube `BoxField` dual-contours to a valid closed mesh; it is *not*
  byte-identical to the mesh-path `cube_dc.obj` (the analytic box feeds the
  QEF central-difference gradients, the mesh path feeds interpolated input
  normals — different Hermite data, same shape).

Verified: 65 tests pass (14 original + 51 implicit/combinator/primitive);
the molde mesh regression is byte-identical to Phase 2.

## Phase 4 — Domain operators — DONE

Delivered:
- `Vector3i` in `types.h` — per-axis integer tile counts.
- `src/implicit/domain_ops.cpp` — six operators, hidden classes behind public
  `FieldPtr` builders:
  - `mirrored` — reflect across a plane through the origin. Piecewise
    isometry; the mirror seam stays sharp (the reflected gradient is exact,
    not finite-differenced).
  - `repeated` — infinite tiling; `bounds()` returns the `BBox::infinite()`
    sentinel, so sampling needs an explicit `rootBounds`.
  - `repeatedLimited` — finite tiling via Quilez's 8-neighbour clamped
    repetition; tiles at integer ids `[0, count)`; `bounds()` is finite.
  - `twisted` / `bent` — distorted fields; `gradientAt` is a central
    difference of the warped field (the plan's accepted fallback). `bounds()`
    exploits that both warps preserve a radius (twist: distance from the
    axis; bend: distance from the origin in the bend plane).
  - `displaced` — adds `bump(p)` to the field value; `bounds()` is the
    child's (documented caveat: an outward bump needs an explicit rootBounds).

Verified: 72 tests pass (65 prior + 7 domain-op); molde mesh regression still
byte-identical to Phase 2.

## Phase 5 — 2D fields + revolution / extrusion — DONE

Delivered:
- `include/dualc/implicit2d.h` — `Vector2` alias, `BBox2D`, the
  `ImplicitField2D` interface (`valueAt`/`gradientAt`/`bounds` over `Vector2`),
  `Field2DPtr`, and a `PrimitiveField2D` base with a central-difference
  gradient.
- `src/implicit/field2d.cpp` — four 2D primitives: `Circle2D`, `Box2D`,
  `Segment2D` (2D capsule), `Polygon2D` (Quilez `sdPolygon`, sign from a
  crossing count).
- `src/implicit/lift.cpp` — `RevolveField` (spins a profile around the y-axis,
  local x -> radius - axisOffset) and `ExtrudeField` (sweeps an xy profile
  along z). Both are 3D `ImplicitField`s with finite-difference gradients and
  closed-form bounds derived from the 2D `BBox2D`.

Verified: 79 tests pass (72 prior + 7 lift). The two identity checks hold —
`RevolveField(Circle2D)` matches `TorusField` and `ExtrudeField(Box2D)`
matches `BoxField` to 1e-9 at sampled points; molde regression byte-identical
to Phase 2.

---

← Back to the [Roadmap index](README.md).
