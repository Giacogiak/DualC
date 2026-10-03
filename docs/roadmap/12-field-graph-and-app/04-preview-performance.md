# Viewer performance (§ D.2): adaptive preview, metric fast path, GPU selection, startup

Part of [12 — Field-graph & standalone raymarch app](README.md); continues
[03](03-raymarch-app.md) (§ D). The interactive-performance record and the one deferred
item.

## D.2 Viewer interactive performance

Three entries, each dated on its first line.

### Adaptive-resolution preview + metric SDF fast-path
**DONE (2026-07-09).** Triggered by a concrete report: a **tapered `octet`** strut lattice (Phase 4,
[05 §17b](../05-tpms-lattices/02-strut-enhancements.md#17b-strut-lattice-enhancements-phases-3-5))
orbited unusably — even froze on open — on an **Intel HD 630** iGPU. Root cause:
the raymarch sphere-traces the SDF *per pixel every frame*, and a tapered octet is
**72 SDF primitives** (36 segments × 2 round cones) per march step; worse, the
trace framework computed a **6-sample finite-difference gradient every step** to
size the step — a ~7× multiplier that is pure waste on an exact SDF. Three changes,
all confined to `examples/dualc_field_view.cpp` + the viewer-only trace framework
in `examples/field_glsl.cpp` (no `libdualc`, no codegen, no parity/export impact —
`dualc_glsl_parity` unchanged at 68/68; field/codegen tests green) *(2026-09-18: 68 is
the table at `65d10b5`, after Phase 4 (`e7c653c`) and before Phase 5's `mix` case took it
to 69 (`aa0d577`) the same day — [05/03](../05-tpms-lattices/03-crystal-blending.md))*:

- **Metric SDF fast-path (the big win).** New `uMetricSDF` uniform in
  `kTraceFramework`. When the whole graph is a true Lipschitz-1 SDF — detected via
  the existing `nodeStepScale(root) == 1.0` (no TPMS / smooth boolean / warp) — the
  loop sphere-traces by the distance directly and **skips the per-step gradient**,
  ~**7×** fewer SDF evaluations. Safe because such fields are exact distances (and
  strut folds are oracle-proven exact); non-metric fields keep the
  gradient-normalized conservative step unchanged. Set once per frame from
  `scene.stepScale`.
  > **Corrected 2026-08-25** — baked `mesh`/`winding` sources are NOT exact SDFs and are now excluded from this fast path: [12/07 §H1](07-mesh-preview-sweep.md#h1-the-metric-fast-path-traced-a-field-that-is-not-a-distance-field).
- **Adaptive render-scale.** New **`--preview-scale S`** flag (default and range:
  [command_reference/12](../../command_reference/12-dualc_field_view/README.md)). While the camera/param/section is *moving* (tracked
  by `OrbitState.dragging` + a `lastInteract` timestamp stamped in the input
  callbacks, 0.18 s settle) the raymarch renders into an offscreen **RGBA8
  renderbuffer FBO** at `S×` the window and is `glBlitFramebuffer`-upscaled
  (`GL_LINEAR`); when the view **settles** it re-renders full-res straight to the
  window (crisp still). `glBlitFramebuffer` (not a blit shader) keeps it a pure
  framebuffer copy — no sampler, so `--es` is unaffected.
- **Opens in low-res + idle throttle.** `lastInteract` is stamped at startup so the
  first frames are low-res (the window shows immediately instead of blocking on a
  heavy full-res frame — the specific "stuck on open" symptom). Idle frames drop
  from vsync to `glfwWaitEventsTimeout(0.1)` so a parked window isn't re-raymarched
  at 60 Hz (the file-watch poll still runs).

Net: dense strut lattices orbit smoothly (low-res + 7× fast-path); `bcc` is
comfortable full-res, `octet` is best navigated at `--preview-scale 0.35` and left
to settle. Flag + behaviour documented in
[command_reference/12 › Adaptive
resolution](../../command_reference/12-dualc_field_view/README.md#adaptive-resolution-smooth-orbit-on-heavy-scenes).
No CTest (windowed); validated by headless `--snapshot` (sphere/bcc/octet render
correct) + the interactive report.

### Discrete-GPU auto-selection (hybrid-graphics laptops)
**DONE (2026-07-09).** Triggered by a report that `dualc_field_view` rendered on the weak **Intel HD 630**
iGPU of an Optimus laptop that also has an **NVIDIA GTX 1050 Ti** — the OS hands a new
GL context to the *integrated* GPU by default and GLFW/GL exposes no portable "prefer
the fast GPU" hint. Fix is the vendor-standard, exe-name-independent one: a new
`examples/gpu_preference.h` exports the two magic globals the drivers read from the
launching `.exe` at startup — `NvOptimusEnablement = 1` (NVIDIA Optimus) and
`AmdPowerXpressRequestHighPerformance = 1` (AMD PowerXpress) — guarded `#ifdef _WIN32`
(no-op elsewhere), included from exactly one TU of each GL executable
(`dualc_field_view`, `dualc_raymarch`, `dualc_glsl_parity`). It **must** live in the
`.exe`, not the shared `dualc_examples_gl` static lib — the linker can drop an
unreferenced exported global from an archive, and the driver only reads exports from
the launching module. **Link-only:** no `libdualc`, codegen, export, or parity impact
(those paths are untouched). **Verified (NVIDIA/Optimus, on the reporting machine):**
the startup `[dualc_gl] GL renderer:` line flipped from `Intel(R) HD Graphics 630` to
`GeForce GTX 1050 Ti/PCIe/SSE2`, a heavy tapered `octet` came up full-res in ~2 s, and
all three exes link clean (an export table is now emitted). The AMD symbol is the
documented symmetric mechanism, **not tested here**. **Client relevance** — the
Boletus side-car re-vendors this exe and gets discrete-GPU selection for free; caveat
for a client building its own GL exe, see
[15-boletus-handoff.md › Discrete-GPU
selection](../15-boletus-handoff.md#discrete-gpu-selection-side-car-binary).

### Viewer startup / shader-compile time
**DEFERRED** — a future plan; analysis 2026-07-09, trigger at the end. The above fixed *steady-state* orbit, not the **initial load**: a tapered `octet`
still takes several seconds to first appear on the HD 630, plus a one-off "starting
lag" on the first full-res settle frame. Analysis and a ranked fix menu, collected
so a future plan can act without re-deriving:

*Where the time goes (two distinct costs, don't conflate):*
1. **Shader compile + link (the true "loading", before any frame).** The generated
   fragment shader is `kLibrary` (~400 lines, **all 36 `sd*` helpers always emitted**
   regardless of use) + `sceneSDF` (**72 inlined `sdRoundCone` calls** for a tapered
   octet) + `kTraceFramework` (~116 lines). Intel's GLSL compiler is slow optimizing
   a large single function; `glCompileShader`/`glLinkProgram` is almost certainly the
   multi-second load. **Independent of resolution/throughput** — `--preview-scale`
   and the fast-path do not touch it.
2. **First full-res settle frame ("starting lag").** 0.18 s after open the viewer
   settles and renders one heavy full-res octet frame, freezing briefly before the
   first orbit drops it to low-res.

*Candidate fixes (ranked):*

| # | Fix | Targets | Effort/Risk | Notes |
| --- | --- | --- | --- | --- |
| 1 | **Data-driven strut loop** — a `for` loop over a uniform array `uSegA[36]/uSegB[36]` instead of 72 unrolled `sdRoundCone` calls | Compile time (big), shader size | Med | Shrinks `sceneSDF` from 72 calls to ~1 loop → far faster compile, likely faster runtime (smaller shader). Bonus: segment count becomes data → live crystal switching. |
| 2 | **Program-binary cache** (`GL_ARB_get_program_binary`) keyed by source hash | Compile time on **re-launch + every live reload** | Med | **High value for the Boletus side-car**: the file-watch rewrites + recompiles on *every* Grasshopper solve; a disk cache makes repeat compiles ~free. |
| 3 | **Emit only the `sd*` helpers the graph uses** (prune `kLibrary`) | Compile time (moderate) | Low | Stop shipping 36 helpers when the graph uses 1–2. |
| 4 | **Progressive refinement on settle** — ramp 0.35→0.5→0.75→1.0 over successive idle frames instead of one full-res jump | "Starting lag" | Low | No single giant frame; sharpens gradually. |
| 5 | **Async/parallel compile** (`GL_KHR_parallel_shader_compile`) with a low-res proxy while compiling | Perceived load time | Med | Show *something* immediately instead of a frozen window. |
| 6 | **Adaptive `MAX_STEPS`** (fewer steps while moving; lower cap than 384) | Full-res + moving frames | Low/Med | Risk: holes on thin grazing struts — needs care. |

*Recommended first step: measure before optimizing.* Add `glfwGetTime()` timing
around `glCompileShader`/`glLinkProgram`/first frame in `buildProgram` + the loop
and print the split — confirm compile (#1–3) vs. settle frame (#4) dominates on the
target GPU before investing.

*Benchmarking confounders (all hit during the 2026-07-09 investigation):*
- **Intel driver teardown hang** — the process prints `(ok)` then hangs ~2 min in
  `glfwTerminate()` on exit; only affects `--snapshot`-and-exit, **not** the live
  window. Measure via internal timestamps, never wall-clock-to-exit.
- **Thermal throttling** — repeated heavy octet renders cook the iGPU; later runs
  (even a sphere) crawl. Cool between runs; log GPU clock if possible.
- **Kill the right PID** — stuck viewer processes pin the GL context;
  `taskkill /F /IM dualc_field_view.exe` between runs.

*Honest framing:* #1 (loop) + #2 (binary cache) attack load time directly and are
highest-leverage; #4 smooths the first-frame lag. None change the fundamental truth
that a 72-primitive full-res raymarch is slow on an HD 630 — the viewer's sweet spot
on integrated-only hardware is live editing at `--preview-scale 0.35`. (On a hybrid
laptop the viewer now **auto-selects the discrete GPU** for full-res comfort — see the
Discrete-GPU auto-selection sub-entry above.) **Trigger to revisit:** a workflow that
opens dense strut/analytic graphs
often enough that the multi-second compile hurts, or a Boletus side-car session that
recompiles on every solve (→ prioritize #2).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
