# 3.4 Sign determination — the three oracles

`SignMethod` ([`include/dualc/types.h`](../../include/dualc/types.h)) has three values and
**all three are really implemented**. `internal::SignOracle::isInside`
([`src/internal/sign_oracle.cpp`](../../src/internal/sign_oracle.cpp)) is a three-way
dispatch on the stored enum. The sampler consumes the answer as a corner sign
([§ 3.3](01-sampler.md#33-refinement-and-edge-crossing-capture)); which oracle a mesh gets is
`SamplerParams::signMethod` ([§ 7](06-parameters-and-vendoring.md#7-configuration-parameters)).

## `WINDING_NUMBER` (default) — 3-ray majority parity

```cpp
int votes = 0;
for (const Vector3& d : kProbeDirs) {
  if ((bvh_.countRayHits(p, d) & 1) != 0) ++votes;
}
return votes >= 2;
```

The three probe directions are built once in a lambda-initialised `const std::array` at
namespace scope: normalised permutations of `{1, 0.7320508…, 0.5773502…}` — no axis-aligned
components (so an axis-aligned CAD face can never be hit edge-on), no two parallel. Cost:
three full BVH ray traversals, the most expensive of the three methods. **The name is a
misnomer** and the header says so: *"NOT a winding number despite the name — kept for
backwards compatibility."* Failure modes: requires closed, oriented input; the three rays
share an origin and all lie in the positive octant, so the majority vote hardens against a
*single* grazing edge but not against anything systematic — the rays fail together, not
independently.

`countRayHits` is a **single all-hits BVH walk with no advance epsilon and no hit cap**; hits
landing on an edge shared by two triangles are merged at 4 ULP, the float resolution
floor. There is
deliberately no distance-relative advance past each hit: an advance proportional to hit
distance rather than to feature size eats any crossing inside it, a lost crossing flips the
parity, and because all three probes would share the epsilon they would flip together. The
measurement and the fix are
[17 #23](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#23-ray-parity-advance-epsilon-is-relative-to-hit-distance-not-feature-size).

## `PSEUDONORMAL` — Bærentzen–Aanæs angle-weighted pseudonormal

```cpp
Vector3 cp, n;
int     tri;
if (!bvh_.closestPointWithPseudoNormal(p, cp, n, tri)) return false;
return dot(p - cp, n) < 0.0;
```

`MeshBVH` builds the supporting lookup tables at construction: per-vertex pseudonormals as
the interior-angle-weighted sum of incident face normals (the angle from
`atan2(|a×b|, a·b)`, robust for slivers), and per-edge pseudonormals as the mean of the two
adjacent face normals — but only where the incident face count is exactly 2; boundary and
non-manifold edges are left as a zero sentinel and the query falls back to the face normal.
Correctness rests on the theorem that the angle-weighted pseudonormal is the unique normal
for which `dot(p - cp, n) < 0` is exact for *any* query point; the naive face normal gives
the wrong answer in the Voronoi wedge outside a convex edge, which is why the tables exist.
**Cost: one closest-point traversal plus an O(1) lookup — by a wide margin the cheapest
method.** Failure modes: requires watertight, consistently oriented input; on an open shell
the pseudonormal degrades to the raw face normal. That degradation is *observable* rather
than silent: the topology build knows the mesh is not watertight and says so through
`Diagnostics::inputBoundaryEdges` / `inputNonManifoldEdges` (the counts fall out of the same
per-edge book the tables need — [17/05](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md)).
The oracle itself still degrades silently at the point of use. The tables are also built
unconditionally, regardless of the selected method.

## `GENERALIZED_WINDING_NUMBER` — Jacobson GWN, hierarchically accelerated

```cpp
return bvh_.windingNumberFast(p) > 0.5;
```

`w(p)` is ≈1 inside and ≈0 outside for watertight oriented input, so 0.5 is the
maximally-robust threshold; for non-watertight input `w` degrades *continuously* and passes
smoothly through 0.5 near an open boundary, so the 0.5 level set is a well-defined closed
surface that seals holes with a smooth cap. The same 0.5 convention is used consistently by
`WindingNumberField::isInside` and by its `valueAt`, which returns `0.5 - w(p)` — a
**sign-correct pseudo-SDF whose magnitude is not a metric distance**. Cost: O(log T)
average. The multipole tree is built only when the `MeshSource` was constructed with this
method; if it was not, `windingNumberFast` returns `0.0` — "everything is outside" — with no
error. Both in-tree callers construct with the method they query, so that path is not
reachable from the library's own entry points.

---

← Back to the [design index](README.md) · the [docs index](../README.md)
