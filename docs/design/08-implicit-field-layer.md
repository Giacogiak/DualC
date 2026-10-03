# 10. The implicit-field layer ("SDF + DC")

The input side of the pipeline is any **real-valued implicit field**, of which a mesh is one
kind. The `HermiteOctree` contract ([§ 2](README.md#2-the-data-contract-hermiteoctree)) and
the entire contourer ([§ 4](03-contourer-recursion.md)) are field-agnostic — the field layer
is a sampler-side interface plus a library of fields that implement it. `dualContourMesh`
and `sampleMeshToHermiteOctree` are the field path with a `MeshSource` built for you, in one
place ([§ 1](README.md#1-pipeline-at-a-glance)); the data flow is
[§ 10.2](README.md#102-data-flow-on-the-field-path).

## 10.1 `ImplicitField`

Defined in [`include/dualc/implicit.h`](../../include/dualc/implicit.h). It is **3 mandatory
+ 4 overridable** virtuals (plus the virtual destructor):

```cpp
class ImplicitField {
  virtual double  valueAt(const Vector3& p)    const = 0;  // SDF: < 0 inside
  virtual Vector3 gradientAt(const Vector3& p) const = 0;
  virtual BBox    bounds() const = 0;
  // derived, overridable for closed-form fast paths:
  virtual bool isInside(const Vector3&) const;             // default valueAt < 0
  virtual bool edgeHit(...) const;                         // default: Illinois regula falsi
  virtual bool cellOverlaps(const BBox&) const;            // default: corner signs + Lipschitz-1
  virtual bool closestSurfacePoint(...) const;             // default: one Newton step
};
```

Only `valueAt` / `gradientAt` / `bounds` are mandatory. Fields compose through `FieldPtr`
(`std::shared_ptr<ImplicitField>`) into an immutable expression tree — immutable by
convention (no setters, no `mutable`, no caching), not by the type system. Every method is
called concurrently by the sampler, so `const` must mean genuinely immutable; the header's
`THREAD SAFETY` block names the `mutable` memoisation cache as the trap and `bakeToGrid` /
`GridField` as the supported way to amortise an expensive field
([§ 3.2](01-sampler.md#32-parallel-octree-build)).

The derived set is not speculative: it is **exactly the sampler's call graph** reified.
`isInside` supplies corner signs, `cellOverlaps` the refine/prune decision, `edgeHit` the
Hermite data, `closestSurfacePoint` the miss fallback
([§ 3.3](01-sampler.md#33-refinement-and-edge-crossing-capture)). Each default is correct but
slow; each override is equivalent but faster or better-conditioned. `MeshSource` overrides
all four, because it has a BVH.

Three defaults deserve calling out:

- **`edgeHit` is Illinois-modified regula falsi with a 6-step budget, not bisection**
  (§ 3.3). Its last line is the load-bearing one for the whole sharp-feature claim: the
  Hermite normal comes from `gradientAt(outP)`.
- **`cellOverlaps` assumes the field is Lipschitz-1.** After the 8 corner-sign test it falls
  back to `|valueAt(center)| <= 0.5 * |extent|`. This is a *correctness* assumption, not a
  performance heuristic: a field whose gradient magnitude exceeds 1 can have surface passing
  through a cell that the test misses, and that geometry is silently dropped. This is why
  all six TPMS primitives override `cellOverlaps` to `return true` — a fully dense octree
  inside the root box, paid deliberately. It is also why the boolean combinators and the
  offset-like decorators (`offsetOf`, `roundedOf`, `onionOf`, `gradedOnionOf`,
  `gradedOffsetOf`, `normalizedOf`) forward `cellOverlaps` to their children — a high-frequency child reporting always-overlap
  must not have that answer thrown away — and why every wrapper that *warps* the domain (the
  six in `domain_ops.cpp` plus `TransformField`, `ScaleField` and `ElongateField`) forwards
  on a box that contains the child's image of the cell. The limitation this leaves, and its
  fix, are [§ 9](07-limitations.md).
- **`closestSurfacePoint` is one Newton step** — `q - g·(f/|g|)`, then a gradient
  re-evaluation at the projected point. Exact for a true SDF, arbitrarily wrong for a
  non-metric field. It returns `false` only when the gradient is degenerate. `MeshSource`
  overrides it with a direct BVH closest-point query.

## 10.3 Sharp features

This is the headline benefit over "SDF + marching cubes". Hard booleans (`min` / `max`) make
the compound field C0 along the seam; the combinators override `gradientAt` to return the
**active operand's** gradient — the one attaining the min/max — never an average. This is
not a hack: `min(a, b)` is differentiable everywhere except the seam, and the active
operand's gradient is the correct a.e. derivative.

Because the default `edgeHit` takes its crossing normal from `gradientAt`, the QEF receives
the un-blended normal. Along a seam, adjacent edges of one cell then carry two *different*
un-blended normals, giving two independent plane constraints, and the QEF's minimiser is
their intersection — the vertex lands exactly on the ridge. With marching cubes the vertex is
pinned to the edge by linear interpolation of the value, so a ridge crossing a cell
diagonally is unrepresentable at any resolution.

Smooth booleans (Quilez `smin` / `smax`) produce a smooth field that DC reconstructs
smoothly; the polynomial `smin` is used specifically because its gradient is exactly the
`h`-lerp of the operand gradients, with the `∇h` term cancelling identically — so no extra
evaluations are needed. Same pipeline; the field algebra alone decides sharp vs. smooth.

## 10.4 The shape of the library

The full enumeration — every primitive, decorator, operator and boolean, with its node name
— is the command reference's
[inventory appendix](../command_reference/README.md#appendix--full-inventory-count); this
page states only how the library is built.

- **Sources** — `MeshSource` (signed distance to a mesh: magnitude from a BVH closest-point
  query, sign from the configured `SignMethod`), `WindingNumberField` (`0.5 - w(p)`, a
  sign-correct pseudo-SDF over arbitrary triangle soup), `GridField` (a sampled lattice with
  trilinear interpolation and an analytic gradient), plus `bakeToGrid` /
  `MeshSource::bakeToGrid` (narrow-band bake) to produce one.
- **Combinators** (`src/implicit/combinators.cpp`) — the hard and smooth booleans and
  `mixOf`. Hard booleans keep the seam sharp by the active-operand rule of § 10.3.
- **Decorators** (`decorators.cpp`) — offset-like operations on the level set (`offsetOf`,
  `roundedOf`, `onionOf`, `gradedOnionOf`, `gradedOffsetOf`, `normalizedOf`) and the
  similarity transforms (`elongated`, `transformed`, `scaled`).
- **Primitives** — the analytic Quilez distance-function catalogue in
  [`primitives.h`](../../include/dualc/primitives.h) (`primitives_tier{A,B,C}.cpp`) and the
  TPMS surfaces (`primitives_tpms.cpp`). Primitives are public classes constructed directly
  (`std::make_shared<SphereField>(c, r)`); combinators, decorators and domain operators are
  hidden behind free factory functions returning `FieldPtr`. Two primitives are **open
  surfaces**: `TriangleField` and `QuadField` are unsigned distances with no inside, so they
  contour to nothing on their own and are meshed only through `onionOf`, which gives them a
  wall.
- **Domain operators** (`domain_ops.cpp`) — isometries (`mirrored`, `repeated`,
  `repeatedLimited`: the gradient comes from the active copy, so tile and mirror seams stay
  sharp) and distortions (`twisted`, `bent`, `displaced`: the gradient is a central finite
  difference of the warped field, 6 `valueAt` calls per query, which multiply when nested).
- **A 2D layer** ([`implicit2d.h`](../../include/dualc/implicit2d.h), `field2d.cpp`,
  `lift.cpp`) — `ImplicitField2D` with `Circle2D` / `Box2D` / `Segment2D` / `Polygon2D`,
  lifted to 3D by `RevolveField` / `ExtrudeField`.

## 10.5 Unbounded fields

`PlaneField`, `InfiniteCylinderField`, `InfiniteConeField`, every TPMS, and `repeated` with
any positive period have no finite extent — their `bounds()` returns the `BBox::infinite()`
sentinel. `sampleFieldToHermiteOctree` throws `std::invalid_argument` for such a field unless
`SamplerParams::rootBounds` is set explicitly. Note the asymmetry with the *invalid* box
(min > max, e.g. a disjoint intersection): that one degrades to `BBox::unit()` and reports
itself through `Diagnostics::boundsFallback` rather than throwing, because for an empty
intersection an empty result is the correct answer
([§ 3.1](01-sampler.md#31-root-bounds)). The infinite case throws because no finite answer
exists without the caller's box.

---

← Back to the [design index](README.md) · the [docs index](../README.md)
