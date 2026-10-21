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
+ 5 overridable** virtuals (plus the virtual destructor):

```cpp
class ImplicitField {
  virtual double  valueAt(const Vector3& p)    const = 0;  // SDF: < 0 inside
  virtual Vector3 gradientAt(const Vector3& p) const = 0;
  virtual BBox    bounds() const = 0;
  virtual Vector3 trueGradientAt(const Vector3& p) const;  // ∇f, magnitude kept
  // derived, overridable for closed-form fast paths:
  virtual bool isInside(const Vector3&) const;             // default valueAt < 0
  virtual bool edgeHit(...) const;                         // default: Illinois regula falsi
  virtual bool cellOverlaps(const BBox&) const;            // default: corner signs + Lipschitz-1
  virtual bool closestSurfacePoint(...) const;             // default: one Newton step
};
```

**`gradientAt` is a unit outward direction, not ∇f.** On the surface it is the outward
normal, and that is all its callers use it for: `edgeHit` and `QefSolver::add` re-normalise
it anyway. Its length is 1, except where a node blends directions — the smooth booleans and
`mix` lerp their operands' unit vectors, `elongated` zeroes the components of its flat band —
and there it may be shorter, never longer. Where the direction is undefined (the gradient
vanishes: a sphere's centre, a flat FD stencil) it is `(0,0,1)`. It says nothing about the
magnitude, which differs from 1 for every non-metric field (a TPMS, a winding number, a
lerp). The census of the ~30 overrides against this contract is
[17/19](../roadmap/17-code-audit-and-hardening/19-gradient-contract.md).

**`trueGradientAt` is ∇f with its magnitude**, for the two callers that need |∇f|:
`NormalizedField::valueAt` divides by its child's, and the default `closestSurfacePoint`
steps by it. Its default is a central difference of `valueAt` (step 1e-4, 6 calls — the
stencil shared with the distorted operators, `src/internal/fd_gradient.h`). Sphere, plane
and `GridField` (the trilinear interpolant's own gradient, which jumps across cell faces;
its `gradientAt` is that gradient's direction, so the normal jumps there too — every
continuous node stencil measured cost the sharp edges accuracy,
[17/20](../roadmap/17-code-audit-and-hardening/20-gridfield-normal.md#p04b--a-limited-node-stencil-measured))
override it exactly;
`WindingNumberField`'s is its straddle-aware finite difference, whose unit direction is its
`gradientAt`. The nodes that pass a child's value through forward it, chain rule applied:
offset, onion, `transformed` (rotated), `scaled` (sign of `s`), mirror, both repeats, the
four hard booleans. Everything else — the smooth booleans, `mix`, the graded decorators
(whose `gradientAt` drops ∇t), `elongated`, `normalizedOf`, the distortions, the lifts,
`MeshSource` — takes the default. Where the field is flat it is zero, with no fallback
direction.

Only `valueAt` / `gradientAt` / `bounds` are mandatory. Fields compose through `FieldPtr`
(`std::shared_ptr<const ImplicitField>`) into an immutable expression tree — the `const`
in the alias is the type system enforcing it (every evaluation method is `const`; a
`shared_ptr<Derived>` from `make_shared` converts implicitly), and the class adds no
setters, no `mutable` and no caching on top. Every method is
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
- **`closestSurfacePoint` is one Newton step** — `q - G·f/|G|²` with `G =
  trueGradientAt(q)`, then the normal from `gradientAt` at the projected point. The step
  length is `f/|∇f|`, so it is first-order on any field, metric or not. A step of `f` along
  the unit direction is the same step only where |∇f| = 1; on a gyroid Newton lands ~1000×
  closer, on a winding field's cap 17–60× where |∇w| is 0.25 or 4, and near |∇w| = 1 the
  two nearly coincide ([17/19](../roadmap/17-code-audit-and-hardening/19-gradient-contract.md)).
  It returns `false` when `G` vanishes or `f`, `G` are not finite. `MeshSource` overrides it
  with a direct BVH closest-point query.

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
  trilinear interpolation, its analytic gradient as `trueGradientAt` and that gradient's
  direction as `gradientAt`), plus `bakeToGrid` /
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
