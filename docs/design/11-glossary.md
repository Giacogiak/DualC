# Glossary

One line per term as the code and the docs use it, with the page that explains it where one
exists; a term whose meaning is standard and not DualC-specific gets its standard meaning
only. The second half is the **ID vocabulary** — what `#N`, "Tier 1 item N", "§ A–G" and
"build #N" refer to — because those look alike and are not.

## Terms

| Term | Meaning |
| --- | --- |
| **Implicit field** | Any `f: ℝ³ → ℝ`; the solid is `{f < 0}`, the surface `{f = 0}`. The library's input type, `ImplicitField` ([§ 10.1](08-implicit-field-layer.md#101-implicitfield)). |
| **SDF** | Signed distance function: `\|f\|` is the Euclidean distance to the surface, so `\|∇f\| = 1` almost everywhere. Primitives and `MeshSource` are SDFs; booleans of SDFs are bounding distance functions. |
| **Bounding distance function** | Sign-correct, `\|f\| ≤` the true distance, Lipschitz-1 — still safe for the prune test. |
| **Non-metric field** | Sign-correct but the magnitude carries no distance: a TPMS, `0.5 − w(p)`. `normalizedOf` makes one approximately metric. |
| **Lipschitz-1** | `\|f(p) − f(q)\| ≤ \|p − q\|`, i.e. `\|∇f\| ≤ 1`; the precondition of the default `cellOverlaps` ([§ 9](07-limitations.md#standing-limitations)). |
| **Isosurface / zero level set** | `{x : f(x) = 0}` — what the sampler looks for and the contourer meshes. |
| **Hermite data** | Value *and* derivative: per cell, the eight corner signs plus, for each sign-changing edge, the crossing point and the surface normal there. The sole contract between sampler and contourer ([§ 2](README.md#2-the-data-contract-hermiteoctree)). |
| **`HermiteOctree` / `HermiteNode` / `HermiteLeafData`** | The value type carrying Hermite data, its node, and the per-leaf payload. |
| **Sampler** | Stage 1: the adaptive octree walk over a field, producing Hermite data ([§ 3](01-sampler.md)). |
| **Contourer** | Stage 2: Hermite octree → triangle mesh, one QEF vertex per cell or per surface component ([§ 4](03-contourer-recursion.md)). |
| **Sign oracle** | The inside/outside test a `MeshSource` uses: ray parity, angle-weighted pseudonormal, or generalized winding number, chosen by `SignMethod` ([§ 3.4](02-sign-oracles.md)). |
| **Ray parity** | Count a ray's crossings of the mesh; odd means inside. DualC takes a majority of three rays. |
| **Pseudonormal** | The Bærentzen–Aanæs angle-weighted normal at a vertex or edge, which makes the sign of `(p − cp) · n` exact for the closest point `cp`. |
| **GWN** | Generalized winding number: the total signed solid angle a mesh subtends at a point, over 4π. Continuous, so it survives open shells and soup; `0.5 − w(p)` is `WindingNumberField`. |
| **BVH** | Bounding volume hierarchy over the triangles of a `MeshSource`; every closest-point and ray query goes through it (`src/internal/mesh_bvh.cpp`). |
| **Sign-changing (minimal) edge** | A cell edge whose endpoints differ in sign: the finest such edge is shared by four cells and emits one quad ([§ 4.3](03-contourer-recursion.md#43-emission)). |
| **Edge crossing** | The point on a sign-changing edge where `f = 0`, found by `edgeHit`; the normal there is `gradientAt` of that point. |
| **Illinois regula falsi** | The bracketed root finder the default `edgeHit` runs, with a halving correction that keeps convergence two-sided. |
| **Miss fallback ladder** | What the sampler does when `edgeHit` finds no crossing on an edge whose signs differ ([§ 3.3](01-sampler.md#33-refinement-and-edge-crossing-capture)). |
| **`cellOverlaps`** | The refine-or-prune oracle: may the surface pass through this cell? A wrong `false` drops geometry silently ([§ 10.1](08-implicit-field-layer.md#101-implicitfield)). |
| **Correctness opt-out** | Overriding `cellOverlaps` to `true` to disable the Lipschitz-1 prune test (every TPMS, `MixField`); the same virtual is also the performance fast path. |
| **QEF** | Quadratic error function `E(x) = Σ (nᵢ · (x − pᵢ))²`, the sum of squared distances from `x` to the sample planes; minimised to place a cell's vertex ([§ 4.4](04-qef-manifold-collapse.md#44-the-per-cell-qef)). |
| **Normal equations** | `AᵀA x = Aᵀb`; the QEF is accumulated as these ten numbers, never as the sample list. |
| **Mass point** | The centroid of a cell's crossing points; the origin of the solve and the answer along every truncated direction. |
| **Truncated pseudo-inverse** | Invert only the eigen-directions of `AᵀA` above `qefRegularization`; the rest stay at the mass point. |
| **Soft clamp** | Clamp the minimiser into its cell, unless it drifted more than `clampToleranceCells` widths, in which case the mass point replaces it. |
| **Geometric energy vs residual** | `E(x)` at the solved vertex (what the collapse threshold compares) versus `‖Aᵀb − AᵀA x‖²` (what the solver's return value is). Not the same number ([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)). |
| **`cellProc` / `faceProc` / `edgeProc`** | The Ju–Losasso–Schaefer–Warren recursion: cells spawn faces and edges, faces spawn faces and edges, edges spawn edges; only `edgeProc` emits ([§ 4.1](03-contourer-recursion.md#41-cellproc--faceproc--edgeproc)). |
| **Descent table** | The hand-derived cube combinatorics telling the recursion which children to pass down ([§ 6](05-conventions-and-tables.md#the-dc-descent-tables)). |
| **`childOrSelf`** | A leaf "descends to itself", so a coarse cell stands in for the children it does not have ([§ 4.2](03-contourer-recursion.md#42-the-two-mixed-depth-devices)). |
| **Finest-cell rule** | At the terminal case the deepest of the four cells supplies the edge's sign data — only its edge *is* the minimal edge. |
| **Saddle face** | A cube face whose corners alternate in sign; two crossings pairings are valid. DualC joins the two crossings that share an inside corner (the inside-pair rule); Schaefer's asymptotic decider needs corner *values*, which a leaf does not store. |
| **Manifold DC (MDC)** | One QEF vertex per *surface component* in a cell rather than one per cell, so two sheets in a cell do not share a vertex ([§ 4.5](04-qef-manifold-collapse.md#45-manifold-dual-contouring)). |
| **Pseudo-leaf** | An internal node turned into a leaf by the collapse pass: merged `HermiteLeafData`, no children. |
| **Adaptive collapse** | `simplifyHermiteOctree`: merge eight sibling leaves into their parent when the merged QEF's energy is under `simplificationError` and the topology gates pass ([§ 4.6](04-qef-manifold-collapse.md#46-adaptive-cell-collapse)). |
| **`kQuadCCWPlus`** | The four-cell order around an edge that makes an emitted quad counter-clockwise seen from outside ([§ 4.3](03-contourer-recursion.md#43-emission)). |
| **Watertight** | Zero boundary edges and zero non-manifold edges ([10](10-invariants-and-tolerances.md#output-invariants)). |
| **Non-manifold edge** | An edge shared by more than two faces; breaks booleans, FEA and some slicers. |
| **Euler characteristic (χ)** | `V − E + F`; `2 − 2g` for a closed surface, so it is the genus check in the tests. |
| **Active operand** | In a hard boolean, the child attaining the `min` or `max` at `p`; its gradient is returned un-blended, which is what keeps seams sharp ([§ 10.3](08-implicit-field-layer.md#103-sharp-features)). |
| **Smooth boolean** | Quilez polynomial `smin` / `smax` with blend radius `k` in world units; the gradient is the exact `h`-lerp of the operands' gradients. |
| **Decorator** | A field wrapper that moves the level set without warping the domain: `offset`, `round`, `onion`, `graded-onion`, `graded-offset`, `normalize`, and the similarity transforms ([§ 10.4](08-implicit-field-layer.md#104-the-shape-of-the-library)). |
| **Domain operator** | A field wrapper that warps the query point: the isometries `mirror`, `repeat`, `repeat-limited` (seams stay sharp) and the distortions `twist`, `bend`, `displace` (gradient by finite differences). |
| **Onion** | `\|f\| − t`: a hollow shell of wall thickness `2t` centred on the surface. |
| **`normalizedOf`** | `f / max(\|∇f\|, ε)`: makes level-set spacing approximately metric without moving the surface, so `onion` thickness on a TPMS is in world units. |
| **TPMS** | Triply-periodic minimal surface (gyroid, Schwarz P/D, …): zero mean curvature, periodic on all three axes; `wavelength` is the unit-cell side, `k = 2π/λ`. |
| **Strut lattice** | `sc` / `bcc` / `fcc` / `octet`: the union of `radius`-thick capsules over one unit cell's strut segments, tiled infinitely (`makeStrutLattice`, `examples/example_common`); infinite bounds, like a TPMS. |
| **Lift** | A 2D profile (`ImplicitField2D`) turned into a 3D field by `RevolveField` or `ExtrudeField`. |
| **Field graph** | The runtime tree of `FieldPtr` nodes built from JSON, an `.fld` file or `--expr`; the one representation the CLIs, the GPU viewer and the C ABI consume (`examples/field_graph.{h,cpp}`). |
| **`--expr` / shorthand** | The one-line text form of a field graph; it parses to the same node tree as the JSON. |
| **GLSL codegen** | Compiling a field graph to a straight-line `sceneSDF` for the GPU viewer; the parity harness holds it to the C++ formulas ([10](10-invariants-and-tolerances.md#the-numeric-constants)). |
| **`bakeToGrid` / `GridField`** | Sample any subtree once into a lattice; the result's `valueAt` is one trilinear lookup — the supported way to amortise an expensive field. |
| **Narrow band** | The shell of grid cells near the surface where a bake computes an exact distance; the rest is filled by propagation. |
| **Tile / owned cells / ghost ring** | In streaming export, a tile is a `2^D`-cell octree of which the outer one-cell ring is ghost cells sampled for continuity and the inner `2^D − 2` are owned and emitted ([10](10-invariants-and-tolerances.md#--tile-depth-the-legal-range-and-the-useful-one)). |
| **Weld** | Identifying across-tile seam vertices by a quantised position key so the 3MF becomes one manifold object. |
| **Preview == export** | The invariant that the GPU viewer and the exporter render one parsed graph ([10](10-invariants-and-tolerances.md#output-invariants)). |
| **`Diagnostics`** | The optional out-parameter every driver fills with what it had to fall back on and what the output's topology is ([17/05](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md)). |
| **`CancelToken`** | A host-set sticky atomic flag every driver polls at its checkpoints; requested from any thread, it makes the call throw `Cancelled` ([10 § Hooks](10-invariants-and-tolerances.md#hooks-cancellation-and-progress)). At the ABI, `DualcCancelToken`. |
| **`ProgressSink` / `Stage`** | The optional progress receiver, `report(Stage, done, total)`, invoked on the calling thread only; stages `Sample`, `Contour` (engine), `Write`, `Tile` (writers). At the ABI, `DualcProgressFn` + `DUALC_STAGE_*`. |
| **`.part` file** | `<path>.part`, the temp every export writes and renames over `<path>` on success; the only thing an interrupted run can leave behind ([09 § Export-format dispatch](09-conventions.md#export-format-dispatch)). |
| **`BBox::empty()` / `infinite()` / `unit()`** | The three sentinel boxes; `BBox{}` is none of them ([10](10-invariants-and-tolerances.md#bounding-box-sentinels)). |
| **Sibling checkout** | A dependency cloned next to the repository and added by path. DualC uses none: geometry-central and GLFW are fetched at a pinned commit unless `-DDUALC_GC_DIR` / `-DDUALC_GLFW_DIR` names a local tree ([§ 5](05-conventions-and-tables.md#5-geometry-central-integration)); glad and the other small deps are vendored ([`THIRD_PARTY.md`](../../THIRD_PARTY.md)). |
| **Vendored** | Copied into the tree: the public-domain QEF/SVD pair in `src/internal/`, the MIT nanort header in `src/internal/third_party/` and the writers' helpers under `examples/third_party/` ([§ 8](06-parameters-and-vendoring.md#8-vendored-third-party-code)). |
| **ULP** | Unit in the last place: the gap between adjacent representable floats at a magnitude. The unit of the ray-parity duplicate-hit merge. |
| **Bit-identical / byte-identical** | The same bytes out, not merely the same geometry; the standard the thread-count and tiled-export invariants are held to. |
| **Gate** | `scripts/check.py`, the one command that defines green, run locally and by hosted CI (`.github/workflows/gate.yml`) ([17/09](../roadmap/17-code-audit-and-hardening/09-local-checks-gate/README.md), [20 #51](../roadmap/20-public-delivery/03-hosted-ci.md#51-hosted-ci--the-gate-as-a-github-actions-job)). |

## The ID vocabulary

Four numbering schemes coexist in the docs and are written differently on purpose; prose
always uses the qualified form.

- **`#N` — a roadmap tracked item.** One sequence across every roadmap block: `#18` is the
  tight-Lipschitz item in block 05, `#22`–`#35` are block 17's, `#36`–`#45` block 19's.
  `rg -n '#34' docs/` finds every citation of an item. A `#N` is never renumbered and a
  retired number is never reused.
- **"Tier 1 item N" — block 01's prioritised list** (`docs/roadmap/01-core-dual-contouring/README.md`).
  A different list from the tracked items, never written `#N`.
- **"§ A–G" — the lettered sections of block 12** (`docs/roadmap/12-field-graph-and-app/`),
  cited as `12 § F` with the section letter, never by number.
- **"build #1–#3" and "pillar #1–#4" — block 12's keystone.** A *build* is one of the three
  delivered steps of its sequence (field graph, GLSL preview, streaming export); a *pillar*
  is one of the four value pillars the re-evaluation named. The two are numbered
  independently and are cited with the word attached — "build #3", "pillar #3" — because a
  bare `#3` is a tracked item.
- **`NN-slug` — a file or folder number** in every docs tree, and **letter IDs** for recipes
  on a command-reference page. Both are assigned at birth and never renumbered
  ([`docs/README.md` § Conventions](../README.md#conventions)).

---

← Back to the [design index](README.md) · the [docs index](../README.md)
