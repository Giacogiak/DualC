# 9. Limitations

The standing limitations of the engine, each with the fix it calls for. This page states the
limitation; **status, priority and the trigger to revisit each one live in
[roadmap 17](../roadmap/17-code-audit-and-hardening/README.md)** (items #22–#35) and in the
topic blocks linked from each entry — that ledger tracks the work.

## Standing limitations

- **The default `cellOverlaps` assumes a Lipschitz-1 field.** When all 8 corner signs agree
  it falls back to `|valueAt(center)| <= halfDiagonal`. Any field with gradient magnitude
  greater than 1 can hide a surface bubble inside a cell that touches no corner and does not
  register at the centre, and the cell is silently dropped. The TPMS primitives work around
  this by overriding `cellOverlaps` to `return true`; there is no machine-checkable way for
  the interface to know. A conservative Lipschitz constant declared per field, or a cheap
  interval/range bound, is the real fix
  ([17 #25](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#25-lipschitzbound--the-silent-correctness-opt-out-behind-celloverlaps);
  the per-TPMS closed-form bound is the separate performance item
  [05 #18](../roadmap/05-tpms-lattices/README.md#18-tight-lipschitz-celloverlaps-for-tpms-primitives)).
- **A wrapper that forgets `cellOverlaps` silently under-refines.** Every domain wrapper in
  the tree overrides it (the six in `src/implicit/domain_ops.cpp`, plus `TransformField`,
  `ScaleField` and `ElongateField` in `src/implicit/decorators.cpp`), each forwarding to its
  child on a box that contains the child's image of the cell; the shell decorators
  (`OnionField`, `GradedOnionField`, `GradedOffsetField`) forward on the cell grown by their
  thickness, `OffsetField` (`offsetOf`, `roundedOf`) on the cell grown by `|r|` — the level
  set `f = r` lies within `|r|` of the child's surface on either side, so an inward shift
  grows by as much as an outward one — and `NormalizedField` forwards the cell unchanged.
  What makes it a limitation is that it is unenforceable: the next
  wrapper someone adds inherits a default that is a correctness opt-out, and forgetting to
  override is not a compile error. The conservativeness contract is pinned by a property
  test (`tests/test_domain_ops.cpp`, "Domain wrappers ask their child about a superset of
  what they read"), which a new wrapper must be added to by hand; it pins that a wrapper
  forwards, not how far, so a wrapper whose growth carries the correctness (`offsetOf`) has
  its own direct case beside it. A declared per-field
  Lipschitz constant would make the whole class machine-checkable; that is the same
  [17 #25](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#25-lipschitzbound--the-silent-correctness-opt-out-behind-celloverlaps).
  The batch that fixed the nine wrappers which had forgotten is
  [01 § 4.9](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch);
  the tenth, `OffsetField`, is
  [17 #46](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#46-offsetfield-does-not-forward-celloverlaps).
- **The octree pays a heap allocation per node.** Each internal node is 8 separate
  `make_unique<HermiteNode>` calls plus one `HermiteLeafData` per surface leaf (~808 bytes
  and two malloc headers per populated leaf, ~128 bytes per internal node), all
  pointer-chased on traversal. At `maxDepth = 9` that is millions of allocations. A
  pool/arena allocator over a flat node array is the standard fix and would help both memory
  and traversal locality
  ([17 #27](../roadmap/17-code-audit-and-hardening/04-engineering-quality.md#27-allocation-and-traversal-hot-spots)).
- **A float/double seam at the QEF, with no re-centring before it.** Everything outside the
  QEF is `double`; `svd::SMat3` / `Mat3` / `Vec3` are entirely `float`. `solveOneComponent`
  narrows Hermite positions and normals on the way in and widens the solution on the way
  out. The solver *does* re-centre on the mass point — but only at solve time,
  algebraically, after `Aᵀb` and `bᵀb` have already been accumulated as `n·p` in **world**
  float coordinates. For a millimetre-scale part placed far from the origin, that
  accumulation is where precision is lost; re-centring the samples on the cell (or the mass
  point) *before* narrowing would fix it, as would templating the solver on the scalar type
  ([17 #24](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/02-precision-and-celloverlaps.md#24-far-from-origin-precision--re-centre-before-the-float-seam)).

## Behavioural notes worth knowing

- **Fallbacks on bad input are reported, not refused.** An invalid `field.bounds()` (empty
  mesh, disjoint intersection) becomes `BBox::unit()`
  ([§ 3.1](01-sampler.md#31-root-bounds)), and an empty contour result becomes a single
  placeholder triangle ([§ 4.3](03-contourer-recursion.md#43-emission)): each is the
  *correct* answer in the intended case. Both are visible through `dualc::Diagnostics` —
  pass a `Diagnostics*` to any driver and read `boundsFallback` / `emptyContour`; the CLIs
  print them as `[dualc] warning:` lines
  ([17/05](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md)). Without a
  `Diagnostics*` they are silent.
- **`interpolateNormals` defaults to `true` on the mesh path**, which softens the very sharp
  features the implicit path preserves automatically. That is the right default for
  scanned/organic input and the wrong one for CAD; a caller remeshing a bracket must pass
  `false` ([§ 7](06-parameters-and-vendoring.md#7-configuration-parameters)).

## Closed, and recorded in the roadmap

Four defects the audit listed here are fixed; the design pages state the resulting
behaviour, and the record of each fix is:

- `dualContourMesh` dropping `SamplerParams::signMethod` — the single `MeshSource`
  construction site of [§ 1](README.md#1-pipeline-at-a-glance);
  [01 § 4.9](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch).
- The vendored `pinv` truncating *large* eigenvalues, and `tryCollapse` thresholding the
  normal-equation residual instead of the geometric QEF energy —
  [§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse) and
  [§ 8](06-parameters-and-vendoring.md#8-vendored-third-party-code); the same
  [01 § 4.9](../roadmap/01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch).
- `parallelFor` terminating the process on a throwing body —
  [§ 3.2](01-sampler.md#32-parallel-octree-build);
  [17 #22](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/01-parallelfor-and-ray-parity.md#22-parallelfor-exception-propagation).
- `ContourerParams::weldEdges` and `SamplerParams::seed`, declared but read nowhere —
  deleted, [§ 7](06-parameters-and-vendoring.md#7-configuration-parameters);
  [17 #28](../roadmap/17-code-audit-and-hardening/04-engineering-quality.md#28-delete-the-two-dead-public-parameters).

---

← Back to the [design index](README.md) · the [docs index](../README.md)
