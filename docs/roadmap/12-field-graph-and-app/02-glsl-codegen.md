# Field→GLSL codegen contract (§ B)

Part of [12 — Field-graph & standalone raymarch app](README.md). How a field-graph
compiles to a `sceneSDF()` and what the parity gate holds. Later GPU correctness work:
[07](07-mesh-preview-sweep.md).

## B. Field→GLSL codegen contract
**DONE (2026-06-14).**


The compiler turns a graph into **one generated `float sceneSDF(vec3 p)` + a
binding table**; a single fixed, generic sphere-trace loop renders any graph. Each
node emits a pure `float fN(vec3 p)` (point flows down, distance up — the only shape
that handles domain warps). A fixed GLSL prelude carries the primitive/TPMS/op
library, **bit-faithful to the C++ formulas** (enforced by a preview==export parity
test per analytic node). **Structural edits recompile; parameter edits update a
uniform (no recompile)** — so slider-dragging is instant. Mesh-backed sources bake
(`MeshSource::bakeToGrid`, narrow-band 128³–256³, band ≥ any touching smooth-blend
radius) to a `sampler3D` — the only approximate part of preview; export stays exact.
*(Corrected 2026-08-25: the bake is `--grid-res`³, default 96³, over the trace box, and
its far field is an over-estimate — [16
§H](07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep).)*
Non-metric fields (TPMS/GWN/smooth/warps) step by a graph-level `uStepScale` = the
min safe factor over the nodes present (analytic = 1; `normalize` lifts it back
toward 1, so it both fixes smooth-boolean metric correctness *and* speeds the trace);
normals are finite-differenced from `sceneSDF`. (The current
[raymarch viewer](../08-raymarch.md) already does the single-mesh, hand-written case;
this generalizes it to arbitrary graphs.)

**Implemented:** `examples/field_glsl.{h,cpp}` (lib `dualc_examples_fieldglsl`,
no GL dependency) — `compileToGlsl(GraphNode, MeshResolver, BBox) -> GlslScene`
{generated source, `UniformBinding` table, `MeshTexture` bakes, `stepScale`,
bounds}. The walk mirrors `FieldGraph::build` op-for-op; the fixed prelude +
trace/value frameworks are embedded strings, authored to the WebGL2 subset with a
per-target version header (desktop `330 core` / `--es` `300 es` — the body is
identical). **Acceptance gate** `examples/dualc_glsl_parity.cpp` (opt-in
`-DDUALC_BUILD_GLSL_PARITY=ON`, no CTest — needs a GL context; *2026-10-06: run by CI's
required `gpu` job,
[17/13](../17-code-audit-and-hardening/13-parity-gate-binding.md)*): 28 cases render
`sceneSDF` over a 16³ lattice to an R32F FBO and compare to C++ `valueAt` —
**all pass**, analytic nodes to ~1e-7 (float32 ε), TPMS ~1e-4 (GPU trig), the FD
nodes (`normalize`/`twist`/`bend`) to ~1e-3 on a looser tier. **v1 slice** (per the
per-node criterion, incremental): 6 TPMS,
sphere/box/roundbox/torus (A) + cone (B) + triangle (C), all 7 booleans,
offset/onion/scale/elongate/normalize/transform/translate/rotate, twist/bend,
`mesh`→`sampler3D`. **Vocabulary complete + value-verified (2026-06-24):** the
codegen now accepts the same node vocabulary as the contour path — every registry
primitives, every decorator/domain-op, `winding`, and DAG-deduped emission — and
`dualc_glsl_parity` passes **62/62** on GPU (every new SDF port at tight tolerance).
DAG-dedup is value-preserving by construction and `winding` is value-verified
GL-free.

**DAG-dedup emission (2026-06-24).** The codegen walk (`emitNode`,
`examples/field_glsl.cpp`) was a pure tree walk: every descent re-emitted a node's
`float fN(vec3 p)` + uniforms, and `emitMesh` re-baked + uploaded a fresh
`sampler3D`, even for a repeated subtree. It is now a **deduped DAG**: nodes are
keyed by structural equality (`nodeKey` = op + full-precision params + recursive
child keys) and a `Ctx::memo` returns the already-emitted index, so a duplicated
subtree compiles to **one** `fN` / one shared uniform set (so a tier-2 slider edit
moves every reference coherently), and a duplicated `mesh` **bakes once** into a
single texture. The change is value-preserving — `sceneSDF` is byte-identical to
the un-deduped walk (the memo only skips re-emitting; topological
children-before-parents order is preserved because the function is appended before
its memo entry is set), so the existing parity cases stay green and one
`union(gyroid,gyroid)` case was added (matches a single gyroid, tight tier). New
GL-free CTests (`tests/test_field_glsl.cpp`) assert the dedup, the mesh
bake-once, and the no-over-collapse guard (distinct params / sign modes stay
separate). Codegen-only — the viewers and the export path are untouched. This is
emission dedup over today's tree wire-format (duplicate subtrees); the DAG-ref
*serialization* superset (`id`/`ref`) remains the separate deferred item (§A).

**Primitive long tail (2026-06-24).** The remaining ~23 registry primitives *(2026-09-18:
24 are named below; the registry, `primitiveCatalogue()` in `examples/example_common.cpp`,
has 30 — the v1 slice had emitted six)* now
emit — tier-A (`plane`/`capsule`/`cappedcylinder`/`ellipsoid`), tier-B
(`boxframe`/`cappedcone`/`roundcone`/`infinitecylinder`/`hexprism`/`triprism`/
`octahedron`/`pyramid`/`solidangle`) and tier-C (`cappedtorus`/`link`/`cutsphere`/
`cuthollowsphere`/`deathstar`/`vesica`/`rhombus`/`verticalcapsule`/
`roundedcylinder`/`quad`/`infinitecone`) — each a bit-faithful prelude port of its
`valueAt` (`src/implicit/primitives_tier{A,B,C}.cpp`) plus an `emitPrimitive`
branch, with the grouped-key layouts for `capsule`/`cappedcylinder`/`ellipsoid`
mirrored from `field_graph.cpp`. Trig primitives take the raw angle and compute
sin/cos in-shader (like `sdCone`); `octahedron` ports the exact branch (the
registry default). Open (`quad`) and infinite (`plane`/`infinitecylinder`/
`infinitecone`) primitives emit too — onion/bounds stay caller concerns. A GL-free
test compiles **every** registry primitive (catches a missing branch / bad slice),
and 24 parity cases value-verify them on GPU — **all pass at tight tolerance** (the
heaviest, `pyramid`/`deathstar`/`vesica`/`cappedcylinder`/`roundcone`, land at
~1e-7). Running the full gate also surfaced and fixed a pre-existing use-after-free
in the harness (it bound a reference into a temporary `FieldGraph`, which crashed on
`displace`'s `std::function`).

**Domain ops `mirror`/`repeat`/`repeat-limited`/`displace` + `round` (2026-06-24).**
The remaining domain operators now emit. `round` is the `offset` alias (one line).
`mirror`, `repeat`, `repeat-limited` are point-warps like `twist`/`bend`: `mirror`
folds across a (re-normalised) plane; `repeat` tiles per-axis with
`v - s·round(v/s)`; `repeat-limited` is Quilez's home-tile + 7-neighbour clamped
`min`. `displace` is additive (`child(p) + amp/div · pattern(freq·p)`), with the
sine/gyroid/bumps patterns + per-pattern divisors copied bit-faithfully from
`example_common.cpp` `namedBump`. A shared prelude helper `dcRoundTA` rounds
**ties away from zero** to match C++ `std::round` (GLSL `round()` is ties-to-even),
so the tile index matches the field. Step-scale: `displace` keeps its 0.5 (already
wired); the rest are distance-preserving (1.0). Compile-gated GL-free in
`tests/test_field_glsl.cpp`; seven parity cases added to `dualc_glsl_parity.cpp`
(`examples/field_glsl.cpp` only).

**`winding` node (2026-06-24).** The `winding` (generalized winding number,
mesh-soup) source previously threw in the codegen. It now bakes to a `sampler3D`
for preview exactly like `mesh`, but via the **generic** free function
`bakeToGrid(const ImplicitField&, region, res)` (full-lattice sampling) rather than
`MeshSource::bakeToGrid`'s narrow-band path — GWN is not a metric SDF and has no
band. `WindingNumberField`'s sign convention (negative inside) matches
`MeshSource`, so the same texture()-as-SDF emission needs no flip. The baked-grid
tail (`MeshTexture` push + decls + the `texture()` `fN`) is now factored into one
`emitBakedGrid` helper shared by `emitMesh`/`emitWinding`. Path-based only in the
preview (id-based in-memory sources stay a C-ABI/contour concern). Gated GL-free by
`tests/test_field_glsl.cpp` (the baked texture is bit-identical to a fresh generic
bake of the same field); `examples/field_glsl.cpp` only.

**Fix — `normalize` FD step (2026-06-23).** The emitted `normalize` node computed
its finite-difference `|grad f|` with a hard-coded `1e-4` step. That is fine in the
C++ `NormalizedField` (double precision), but in the shader's **float32** a step
that small loses the gradient to catastrophic cancellation → noisy `|grad f|` → a
visibly *"sandy"/layered* surface in `dualc_field_view` (and, on large volumes like
`foot.obj`, a cloud of in-air speckles) on any graph containing a `normalize` —
e.g. `onion(normalize(gyroid), …)`. `dualc_raymarch` never showed it because its
`tpmsGradMag` differentiates with the marcher's feature-scaled `uHmem` step. The
codegen now bakes a **feature-relative** step, `min(diag*0.002, feat*0.003)` (the
same quantity the marcher feeds `uHmem`, where `feat` is the child subtree's
characteristic length via `nodeFeatureScale`), so the gradient is sampled over a
float32-safe baseline that scales with the lattice. `dualc_field_view` now renders
the shell **byte-identically to `dualc_raymarch`** (MSE 0.001, matching
Laplacian-variance) and the `foot.obj`-scale lattice is clean. The C++ field is
left untouched (`1e-4`, double) — the cancellation is float32-only, so export keeps
its accurate step; the preview-vs-export divergence stays FD-tier and is gated by
`dualc_glsl_parity` (`normalize` maxErr 7.06e-4, all pass). Viewer-only change
(`examples/field_glsl.cpp`); no mesh-output change.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
