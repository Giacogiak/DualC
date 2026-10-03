# Mesh-preview correctness sweep (§ H)

Part of [12 — Field-graph & standalone raymarch app](README.md). The 2026-08-25 sweep of
the `mesh` → `sampler3D` preview path: two independent defects, a dead flag and a hole in
the parity gate. Usage lives in
[`11-dualc_field/`](../../command_reference/11-dualc_field/README.md) and
[`12-dualc_field_view/`](../../command_reference/12-dualc_field_view/README.md); § H also
changes [`dualc_raymarch`](../08-raymarch.md) (same library bake —
[`10-dualc_raymarch.md`](../../command_reference/10-dualc_raymarch.md)) and
`dualc_boolean --bake` ([`03-dualc_boolean.md`](../../command_reference/03-dualc_boolean.md)).

*(Moved here verbatim on 2026-09-18 from `16-field-graph-and-app-continued.md`, the
continuation file opened while 12 was frozen; 16 is retired —
[19 Phase 5](../19-docs-layers/README.md).)*

## H. Mesh-preview correctness sweep

**DONE (2026-08-25).** Reported as "curvy series of holes, like stretched tissue" on

```
dualc_field_view --grid-res 160 --expr 'intersection(mesh(path="foot.obj"),
  graded-onion(normalize(gyroid(wavelength=14)),plane(0,1,0,0),t1=4.0,t2=0.8,d0=-60,d1=50))'
```

— GPU only (the exported mesh was always correct), and only with a **loaded mesh**,
never with a purely procedural field. It turned out to be **two independent defects
plus a dead flag and a hole in the gate**, all in the `mesh` → `sampler3D` path.

### H1. The metric fast-path traced a field that is not a distance field

`nodeStepScale` returned 1.0 for `mesh`/`winding`, so `uMetricSDF = 1` and the march
stepped by the raw sampled value. Outside the 3-voxel band a baked grid carries
`chamferGrow`'s **deliberate over-estimate** (`src/internal/distance_grid.h` — written
for octree pruning, where over-estimating is the safe direction), and `winding` stores
a unitless `0.5 − w`. Over-estimated step + a crossing-only hit test with no near-miss
fallback = tunnelling. **Fix:** `mesh` → `stepScale 0.6`, `winding` → `0.5`.
*Measured on the reported scene, single-variable — same `--bounds`, same camera, same
96³ bake, only the march changed. Metric: over the 800×600 `--snapshot`, count pixels
darker than luminance 60 (background) whose 5×5 neighbourhood is >60 % lit (>110) —
i.e. a hole punched through lit material, not a silhouette edge. **127 → 16.** In the
same pair, 624 pixels are solid under the conservative march and empty under the fast
path, against 115 the other way: the fast path loses material 5.4× more often than it
gains it, which is what tunnelling predicts.* Analytic and strut scenes are byte-identical (same
snapshot md5, `stepScale 1` unchanged), and the `--es` (WebGL2-subset) shader renders
a mesh scene identically.

*Is a shorter step enough on its own?* No — tested. Forcing `uMetricSDF = 1` while
keeping `stepScale 0.6` (short step, still 1 eval/step) leaves **86** pinholes vs 16
for the gradient-normalised step. The bake's error is not a bounded scale factor on
the distance, so only renormalising by the local gradient is safe.

*Cost: none measured — it is faster.* 800×600 `--snapshot`, GTX 1050 Ti, wall clock
for the whole process (startup + bake + one frame), so an end-to-end figure rather than
an isolated per-frame one; both columns use the same 96³ bake, so that term is common
and the difference is the march:

| scene | pre-fix (fast path) | shipped (conservative) |
| --- | --- | --- |
| `mesh(path="foot.obj")` | 12.7 s | **10.5 s** |
| `intersection(mesh, box)` (interior box) | 10.3 s | **6.4 s** |
| the reported graded-gyroid scene | 10.3–12.7 s | **10.3–10.9 s** |

The ~7×-more-evaluations arithmetic does not carry over to a *baked* field: the
gradient-normalised step is `0.6·0.8·f/|∇f|`, and a chamfer/trilinear grid is flatter
than a true SDF away from the surface (`|∇f| < 1`), so the steps get **longer**; and the
fast path was burning its 384-step budget chasing surfaces it had tunnelled through.
(Mesh scenes are seconds-per-frame at full res either way on this GPU — pre-existing,
and why the file-watch reload feels sluggish on them: the mtime poll runs once per
frame.)
Latent since the codegen was written; **activated** by the 2026-07-09 fast path
(`65d10b5`) — which is why it looked like a regression of the 2026-06-23 `normalize`
FD fix (`96ab252`) and not a new bug. Before `65d10b5` the per-step gradient
normalisation silently absorbed the baked field's non-metricity.

### H2. The bake's far-field sign flood assumed a padded region
 `floodFarSign` seeded
the six faces of the bake region as *outside* — documented precondition: "the bake
region is padded, so those are outside". But `emitMesh` bakes over the **trace box**,
and graph bounds are the *intersection* of child bounds, so `intersection(mesh, smaller)`
or any `--bounds` zoom hands it a box **inside** the mesh. Interior voxels were then
labelled outside and material dissolved in stair-stepped patches.
**Fix (library-wide, so `dualc_raymarch`'s identical latent bug goes with it):**
`floodFarSign` now labels the 6-connected components of non-band voxels and resolves
each one's sign with a real `SignOracle::isInside` probe (3 probes per component;
typically 2–6 components, negligible next to the exact band queries). The band is a
closed shell ≥ 1 voxel thick, so the sign is constant per component — the old flood
already relied on that; only its *seeding* was unsound. Region-agnostic now: padded,
flush with the AABB, partly overlapping, or strictly interior.
*Proof: `intersection(mesh(foot.obj), box(min=[-14,-34,41],max=[14,-6,69]))` — a box
inside the ankle. `dualc_field` contours the full 28³ box; the GPU drew a serrated
shard covering 44 % of it. Now 100 %. `dualc_raymarch` with the same interior
`--bounds` recovered from 71 045 to 173 638 lit pixels — verified, not inferred.*

**Second behaviour change, same fix: enclosed cavities.** The old flood also inferred
"a component the face-flood never reached must be inside", which **filled** the cavity
of a hollow mesh. The sign query gets it right (ray parity crosses two shells → outside).
This one is on the **CPU export** path too (`dualc_boolean --bake`), not just preview,
and is pinned by a new `[grid]` case (a hollow cube: cavity centre must read positive,
shell negative). It is correct given `MeshSource`'s standing watertight precondition;
an open shell has no well-defined inside either way (route soup through `winding`).
One consequence: a region strictly inside the solid has an empty band, so the grid is
constant and its gradient is zero — the trace framework now falls back to `-rd` for the
shading normal instead of `normalize(vec3(0))`.

### H3. `--grid-res` was dead
 Parsed, clamped, never read; the bake was hardcoded 96³,
so renders at 64/96/160/256 were **bit-identical**. Now plumbed through `compileToGlsl`
(defaulted, so no other call site moved), with `kDefaultMeshGridRes`/`kMaxMeshGridRes`
as the single source of truth, and the compile line prints the resolution **and the
world cell size** so a silently-ignored flag cannot recur. The payoff is largest for
`winding`, whose near-step-function field terraces visibly at 96³ and is clean at 256³.

### H4. Nothing gated any of it
 `dualc_glsl_parity` was 69/69 with **zero** mesh or
winding cases — the only approximate node in the codegen was the only ungated one — and
the CPU band tests only ever baked a cube inside a generously padded box. Added:

- `tests/test_grid_field.cpp` — three region/mesh relationships (strictly interior,
  partly containing, flush with the AABB) plus the hollow-cube cavity. Three of the four
  **fail on the pre-fix library** (the cube centre read `+0.69`, the cavity filled in);
  the flush case and the pre-existing padded case stay bit-identical.
- `dualc_glsl_parity` — now **73** cases. Two `Ref::CpuSignOnly` mesh cases (sign must
  match the exact `MeshSource`, skipping points within one voxel of the surface):
  `mesh-inside` fails 4096/4096 pre-fix, `mesh-straddle` 216/216. Two `Ref::BakedGrid`
  cases compare the GPU against the scene's own texture data, gating the upload
  plumbing (`texMin`/`texScale`/`texDim`, the half-texel offset, `GL_LINEAR`, the R32F
  round-trip) that had never been exercised. The upload itself moved into
  `dce::gl::uploadGrid3D` so the viewer and the gate share one implementation.
  Each sign case prints how many points it actually compared **and fails if that count
  (or the interior count) is zero** — the first draft of these cases printed the counters
  without gating on them, and a `winding` case over a region wholly inside the mesh
  passed on a constant field until the counters were made binding.
  `examples/CMakeLists.txt` now also copies the demo meshes next to `dualc_glsl_parity`:
  it shares an output directory with the always-on tools, so the meshes were there only
  as a side effect of building one of *them*, and a clean single-target build skipped
  every baked-source case.
- `tests/test_field_glsl.cpp` — "GLSL codegen honours the requested bake resolution":
  `compileToGlsl(…, 48)` must produce a 48³ texture, `129` a 129³ one, the default the
  documented 96³, and an out-of-range value must clamp rather than reach `GridField`'s
  throw. `--grid-res` was a *parameter-passing* bug, so a GL-free unit test is its exact
  gate. (It cannot be run against the pre-fix code — that signature took no resolution
  at all — but it fails the moment the parameter stops reaching the bake, which is the
  shape the bug had.)

A GPU snapshot script was written for this sweep and then dropped: its interior-probe
check duplicated the `mesh-inside` parity case, its `--grid-res` check is the unit test
above, and it needed `foot.obj` — which `dualc_gen_demo` does not generate and CMake
does not copy next to the binaries. A gate that cannot run on a clean checkout is not a
gate.

Two smaller things fixed in passing: the viewer **released no mesh texture on reload**
(a full bake leaked per structural reload — 3.5 MB at 96³, but 64 MB once `--grid-res
256` actually worked, and the file-watch reload is what the Boletus side-car drives on
every solve), and `uploadGrid3D` now returns the texture id so both the viewer and the
gate own theirs.

**Verified.** `ctest` **229/229** (the four new `[grid]` cases included; three of them
fail against the pre-fix library). `dualc_glsl_parity` **73/73**, up from 69 — the two
sign cases fail 4096/4096 and 216/216 pre-fix.
An analytic strut scene snapshots **byte-identical** (md5 `e640f940…`) with `stepScale`
still 1, and the `--es` (WebGL2-subset) shader renders a mesh scene to the same lit-pixel
count (148 781) as desktop `330 core`.

**Correction (2026-09-01).** The ctest figure above originally read `228/228` and was off
by one. Re-measured on a worktree at this commit (`0cdcc88`) with the same options
(`DUALC_BUILD_TESTS`/`EXAMPLES`/`C_ABI` on): **229** entries — 206 Catch2 cases plus 23 CLI
tests. The count is unchanged by the re-measurement; only the recorded number was wrong.
See [17](../17-code-audit-and-hardening/README.md).

**Deferred follow-ups.**

- **DEFERRED — mesh-aware `nodeFeatureScale`.** It ignores mesh nodes, so `uStepMin` /
  `uStepMax` / `uHmem` never relate to the bake voxel size (they are pure fractions of
  the bounds diagonal). Wiring it moves three coupled uniforms at once, so it needs its
  own before/after snapshot set; suggested `feat = 4 · maxCell`. *Trigger:* a preview
  that still shows step-size artifacts (banding or missed thin walls) on a mesh scene
  after this sweep, or the first complaint about a small mesh inside large `--bounds`.
- **DEFERRED — conservative baked far field.** The proper fix for [§ H1](#h1-the-metric-fast-path-traced-a-field-that-is-not-a-distance-field):
  clamp the non-band magnitude to the distance-to-nearest-band-voxel lower bound, or
  redistance the `winding` bake from its own zero crossing, so a baked grid becomes a
  true SDF and can take the metric fast path again. *Trigger:* mesh-scene preview
  performance becoming a real complaint (it is currently *faster* than before the fix —
  see the table above), or `winding` needing metric behaviour for a smooth boolean.
- **DEFERRED — shader-compile startup.** Unchanged from [12 § D.2 › Startup](04-preview-performance.md#viewer-startup--shader-compile-time);
  a `--grid-res 256` bake now adds visibly to open time on a large mesh. *Trigger:* the
  Boletus side-car's per-solve reload feeling slow in real Grasshopper use.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
