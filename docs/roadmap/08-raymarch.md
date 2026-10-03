# Analytic raymarch viewer — `dualc_raymarch`

Part of the dense-lattice viz/manufacturing deliverable (see
[11-dense-lattice-deliverable/](11-dense-lattice-deliverable/README.md) for the
rationale and the other steps). Per-command catalogue:
[../command_reference/10-dualc_raymarch.md](../command_reference/10-dualc_raymarch.md).

## Step 3. Analytic raymarch viewer (`dualc_raymarch`) — see dense parts. [DONE — 2026-06-03]

The slice plane (step 1) is an inspection probe, not a "see the whole object"
view. This is the real visualization fix — but the *how* changed once the
constraints were pinned down:

- **Rhino can't host this.** The headline downstream is a Rhino/Grasshopper
  plugin (#19), but Rhino owns its viewport and draws *native geometry*
  (meshes/curves/points) via `DisplayPipeline`. McNeel's GhGL can run GLSL in
  the viewport, but raymarching a mesh-clipped TPMS there needs the mesh SDF as
  a `sampler3D` (not a documented GhGL feature) and re-implementing the field in
  GLSL — high coupling, reuses none of the C++ tree. So GPU implicit viz must
  live in a viewer **we** own (the same reason nTopology is a standalone app).
  The Rhino-side viz is therefore a *mesh* — see step 4.
- **Polyscope's `VolumeGrid` was rejected.** Its isosurface marches a *baked
  grid*, and by Nyquist a baked grid aliases a dense lattice into a featureless
  blob (λ=5 mm in a 300 mm cube = 60 periods/axis → 128³≈2 samples/period,
  256³≈4, 512³≈134 M nodes back at the RAM wall). It cannot carry sub-cell
  detail and largely duplicates `dualc_slice` — it fails the exact regime this
  step exists for.
- **You never needed to bake the lattice to raymarch it.** The TPMS is analytic
  and cheap (~20 flops); **sphere-trace it directly per pixel — no mesh to
  build, so RAM is bounded by the window, not the lattice.** It renders
  densities that OOM the contourer; cost is per-pixel rather than per-cell
  (finer λ costs somewhat more per frame as the conservative step shrinks).
  Baking is only for the *mesh clip* (a low-frequency field → a small 3D texture).

**Delivered:** new standalone `examples/dualc_raymarch.cpp` — a custom-GL app
(GLFW + glad, **not** Polyscope) that sphere-traces the analytic field in an
embedded GLSL fragment shader (the six TPMS formulas kept bit-faithful to
`primitives_tpms.cpp`; `|F|−T` offset and a `1/|grad F|` step normalisation
done in-shader, the GLSL analogue of `normalizedOf`). The only baked thing is
the mesh clip: `MeshSource::bakeToGrid` narrow-band SDF → `GL_R32F` 3D texture,
combined as `max(lattice, meshSDF)` (**corrected 2026-08-25** — that bake mis-signed
its far field whenever `--bounds` sat inside the mesh, dissolving interior material;
fixed in the library, so this viewer got it without an edit:
[16 §
H2](12-field-graph-and-app/07-mesh-preview-sweep.md#h2-the-bakes-far-field-sign-flood-assumed-a-padded-region));
new read-only `GridField::values()` /
`resolution()` accessors hand the grid to the GPU (and unblock the deferred
NRRD/VDB export). Orbit camera + keyboard live-tuning (wavelength/offset/type/
normalise are shader uniforms → instant, no CPU re-work). Opt-in build
`DUALC_BUILD_RAYMARCH_VIEWER` reuses the GLFW + glad the sibling Polyscope
checkout already vendors; **libdualc stays dependency-free.** A hidden
`--snapshot out.png` mode renders one frame headless (glReadPixels → stb PNG)
for scripted/visual verification. **Verified:** clean MSVC build; full suite
156/156 (incl. `dualc_view`, after fixing a pre-existing `stb`-vs-polyscope
link clash from `e996cda`); arg-validation exit codes; and the **rendered
images were inspected** via `--snapshot` — bare gyroid, dense λ=0.02 (renders
fine where the contourer would OOM), gyroid `|F|−T` clipped to a cube (with and
without `--normalize-thickness`: normalised walls are visibly more uniform),
plus diamond and neovius — all come out correct (form, shading, mesh-clip
boundary, the offset/normalise paths, the six-formula switch). Interactive
orbit/keyboard tuning is the only manual
piece (needs a display, no CTest). Docs in `README.md` (Raymarch viewer) and
[`../command_reference/10-dualc_raymarch.md`](../command_reference/10-dualc_raymarch.md).

## Generalized to arbitrary field-graphs — `dualc_field_view`. [DONE — 2026-06-14]

This hand-written single-field viewer is now the reference case for the
**general** field→GLSL codegen
([12 §B/§D](12-field-graph-and-app/02-glsl-codegen.md#b-fieldglsl-codegen-contract)):
`dualc_field_view` compiles *any* composed field-graph to a generated
`sceneSDF()` and sphere-traces it on the same GL stack. The GL plumbing
(window/compile/snapshot/orbit camera) was factored into the shared
`examples/raymarch_gl.{h,cpp}`, which `dualc_raymarch` now also uses (behavior
unchanged). The general shader is authored to the **WebGL2 (`#version 300 es`)
subset** (a `--es` flag selects the header; desktop gets `330 core`), so the same
engine later serves a Web build and the Rhino side-car. `dualc_raymarch` stays as
the tuned, dependency-light single-TPMS view; the formulas it pioneered are now
shared library functions in the codegen prelude, held to the C++ originals by the
`dualc_glsl_parity` gate.

---

← Back to the [Roadmap index](README.md).
