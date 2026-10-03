# Implicit / SDF foundation — the v2 "SDF + DC" layer

The base layer that generalises DualC's input from "a mesh" to "any implicit
field": the `ImplicitField` model, `MeshSource`, automatic sharp-feature
preservation, and the phase-by-phase v2 development log. The booleans/CSG and the
primitives/operators built on top of this layer have their own pages
([03-booleans-csg.md](03-booleans-csg.md),
[04-primitives-and-operators.md](04-primitives-and-operators.md)).

## The v2 SDF + DC layer (handoff §9)

v2 generalises the input side from "a mesh" to "any implicit field". The
`HermiteOctree` contract and the whole contourer are **unchanged**; v2 is a
sampler-side extension plus a large field library. The mesh path stays
byte-identical (`dualContourMesh` / `sampleMeshToHermiteOctree` are now thin
wrappers that build a `MeshSource`). The phase-by-phase log is in the
[v2 phase log](#v2-phase-log) below.

### The model

`ImplicitField` — three mandatory virtuals, four overridable ones, the `FieldPtr` expression
tree and its thread-safety contract — is described where it lives today:
[design/08 § 10.1](../design/08-implicit-field-layer.md#101-implicitfield).

### Sharp features come for free

Why hard booleans give a sharp seam and smooth booleans a smooth field, with one code path
and no toggle: [design/08 § 10.3](../design/08-implicit-field-layer.md#103-sharp-features).

### The library

What the library holds — sources, combinators, decorators, primitives, domain operators, the
2D layer — is [design/08 § 10.4](../design/08-implicit-field-layer.md#104-the-shape-of-the-library);
the full node inventory with counts is the command reference's
[inventory appendix](../command_reference/README.md#appendix--full-inventory-count).

### CLI tools

The four v2-era binaries — `dualc_primitive`, `dualc_boolean`, `dualc_lift`,
`dualc_csg_demo` — each have a page in the [command reference](../command_reference/README.md);
the tools that came after them are indexed there too.

### Gotchas specific to v2

- **Infinite-bounds fields** return the `BBox::infinite()` sentinel and the sampler
  throws without an explicit `rootBounds` — the present-tense rule, and its asymmetry
  with the *invalid* box, is [design/08 § 10.5](../design/08-implicit-field-layer.md#105-unbounded-fields).
- **Open-surface primitives** (`TriangleField` / `QuadField`) have no inside and are
  meshed only through `onionOf`: [design/08 § 10.4](../design/08-implicit-field-layer.md#104-the-shape-of-the-library).
- **DC face count is resolution-driven**, not shape-driven — compare geometry, not counts:
  [design/10 § The resolution rule](../design/10-invariants-and-tolerances.md#the-resolution-rule-a-feature-is--23-cells-or-it-does-not-exist).

---

## v2 phase log

Living roadmap for the v2 work: turning DualC into an "SDF + dual contouring"
pipeline. Updated as phases land. Companion to the [roadmap index](README.md)
(general backlog) and [01-core-dual-contouring/](01-core-dual-contouring/README.md)
(architecture handoff).

### Final objective

Make DualC the **"SDF + DC" pipeline**: a real-valued implicit field is
sampled on an adaptive octree, then contoured by the existing dual contourer.

Compared to the familiar "SDF + marching cubes" workflow, DualC's headline
benefit is **automatic sharp-feature preservation** at field kinks — the QEF
plus un-blended gradients reconstruct hard-boolean seams crisply, where MC
rounds them. Everything else SDFs are good at (smooth blends, offsetting,
morphology, transforms, distortions, repetition, 2D→3D lifts) is supported on
par with the SDF+MC world.

The committed feature inventory is Inigo Quilez's distance-functions article
(https://iquilezles.org/articles/distfunctions/): ~30 analytic primitives,
hard + smooth booleans, positioning, alterations, domain operators, and
2D-to-3D lifts.

#### Foundation (locked design decisions)

1. **Real-valued field.** The interface `ImplicitField` exposes
   `double valueAt(p)` (SDF convention: negative inside) and
   `Vector3 gradientAt(p)`. `isInside`, `edgeHit`, `cellOverlaps` are derived
   with correct defaults and overridden only for closed-form fast paths.
2. **Sharp features for free.** Hard booleans (`min`/`max`) give the field a
   C0 ridge; combinators override `gradientAt` to return the *active
   operand's* true gradient (never an average), so the QEF lands the cell
   vertex on the ridge. Smooth booleans give a C-infinity field reconstructed
   smoothly. One code path, no toggles.
3. **Expression tree.** Fields compose through `FieldPtr`
   (`std::shared_ptr<ImplicitField>`) into an immutable tree, sampled once.
4. **Backward compatible.** `dualContourMesh` / `sampleMeshToHermiteOctree`
   keep their signatures as thin `MeshSource` wrappers; the mesh path stays
   byte-identical to v1.x.

### Phase status

| Phase | Scope | Status | Detail |
| --- | --- | --- | --- |
| 1 | `ImplicitField` foundation + `MeshSource` | **Done** (commit 5c16aea) | this page |
| 2 | Combinators + non-domain decorators | **Done** (committed) | [03-booleans-csg.md](03-booleans-csg.md) (+ decorators in [04](04-primitives-and-operators.md)) |
| 3 | Analytic primitive library (~30 Quilez SDFs) | **Done** | [04-primitives-and-operators.md](04-primitives-and-operators.md) |
| 4 | Domain operators (mirror, repetition, distortions) | **Done** | [04-primitives-and-operators.md](04-primitives-and-operators.md) |
| 5 | 2D fields + revolution / extrusion | **Done** | [04-primitives-and-operators.md](04-primitives-and-operators.md) |
| 6 | CLI / demo exposure | **Done** | this page |

*(2026-09-20: the commits, all 2026-05-15 — Phase 1 `5c16aea`, 2 `378ebf6`, 3 `661f539`,
4 `ee3d78e`, 5 `d501f31`, 6 `7758093`; Catch2 cases 31 → 65 → 72 → 79 over Phases 2–5
and 79 + 5 CLI smoke tests at Phase 6, counted from `tests/` and `add_test` at each tree —
which is the "31 tests" of [03](03-booleans-csg.md) and the "84 tests" of Phase 6 below.)*

### Phase 1 — `ImplicitField` foundation + `MeshSource` — DONE

Delivered:
- `include/dualc/implicit.h` — the `ImplicitField` abstract base and
  `MeshSource`.
- `src/implicit/implicit_field.cpp` — default `edgeHit` (bisection root-find
  on `valueAt`), default `cellOverlaps` (corner-sign + Lipschitz centre test).
- `src/implicit/mesh_source.cpp` — `MeshSource`: signed distance to a mesh
  (sign from 3-ray parity, magnitude from a closest-point query);
  `isInside`/`edgeHit`/`cellOverlaps` overridden to the existing BVH paths.
- `MeshBVH::closestPoint` — branch-and-bound DFS over the nanort BVH.
- Sampler retargeted to `const ImplicitField&`; new
  `sampleFieldToHermiteOctree`; `sampleMeshToHermiteOctree` is now a wrapper.
- Pipeline: new `dualContourField`; `dualContourMesh` is now a wrapper.

Verified: 14 original tests + 5 new `MeshSource` tests pass; the molde
regression output is byte-identical to v1.x.

### Phase 6 — CLI / demo exposure — DONE

Delivered four example executables under `examples/`, sharing one helper:
- `examples/example_common.{h,cpp}` (static lib `dualc_examples_common`) —
  `writeFieldToObj`, argument-parsing helpers, and the `namedBump` library
  backing `--displace`.
- `dualc_primitive` — meshes any of the 30 analytic primitives, with a
  decorator/domain-operator post-op chain (`--offset/--onion/--twist/...`)
  that covers all 12 decorators and domain operators on any primitive.
- `dualc_boolean` — hard and smooth booleans of two meshes.
- `dualc_lift` — revolution / extrusion of a 2D profile (circle/box/segment/
  polygon).
- `dualc_csg_demo` — baked composition recipes (mesh+primitive booleans,
  `displaced`, twisted mesh).

`examples/CMakeLists.txt` registers five exit-code CLI smoke tests with ctest
*(2026-09-18: over the four binaries — `cli_primitive` and `cli_primitive_infinite` both
exercise `dualc_primitive`; `add_test` at `7758093`)*.
Docs refreshed: [01-core-dual-contouring/](01-core-dual-contouring/README.md),
`docs/ARCHITECTURE.md`, the [roadmap index](README.md).

Verified: 84 tests pass (79 unit + 5 CLI smoke); molde mesh regression
byte-identical to Phase 2. A real CSG expression-language parser is deferred
to v3. *(2026-09-18: superseded — `dualc_field --expr` shipped 2026-06-14,
[12 § C](12-field-graph-and-app/01-field-graph.md#c-dualc_field-cli); row D-03 of the
[settled decisions](../decisions/01-settled.md).)*

### v2 complete

All six phases are delivered. DualC is now the "SDF + DC" pipeline described
under the final objective above: analytic primitives, hard and smooth
booleans, decorators, domain operators and 2D lifts, all dual-contoured with
automatic sharp-feature preservation, and all reachable from the command line.

### Cross-phase verification

Every phase: existing tests pass with no regressions; new tests pass; the
cube + molde mesh regressions stay byte-identical to v1.x (the field layer is
additive — the mesh path is never altered).

Two techniques used throughout:
- **Field self-consistency:** `gradientAt(p)` matches a central
  finite-difference of `valueAt` away from ridges; on the zero set,
  `|valueAt|` is below tolerance at sampled surface points.
- **Sharp-vs-smooth discriminator:** dual-contour the result, find the seam,
  fit local planes on both sides, measure the dihedral angle. Hard booleans
  must show a crease above a threshold; smooth booleans must not.

### Known risks (carried forward)

1. **One crossing per edge.** DC stores a single `HermiteEdge` per cube edge;
   sub-cell detail is lost at a given `maxDepth`. Manifold DC is the real fix
   (separate work item); raising depth mitigates.
2. **Mesh signed-distance cost.** `MeshSource::valueAt` runs a closest-point
   traversal. `isInside` keeps its cheap parity fast-path; the distance path
   only runs where the field is genuinely queried as a real number.
3. **Non-Lipschitz composed fields.** `min`/`max`/`smin` and domain warps do
   not preserve unit gradient. Fine here — the field is only sampled, never
   sphere-traced; the only soft spot is the `cellOverlaps` centre test, and
   the corner-sign test plus `minDepth` floor cover it.
4. **`shared_ptr` lifetime.** `MeshSource` holds non-owning references into
   the caller's mesh/geometry; they must outlive the field tree.

---

← Back to the [Roadmap index](README.md).
