# Handoff — P05.1: the porous GLSL emitter

Unit P05.1 of [`plan/14_P05_glsl-and-viewer.md`](../14_P05_glsl-and-viewer.md) § T1. Session
of 2026-10-22, branch `feat/porous-shell` in `~/Documents/DualC-porous`. Nothing was pushed
and no remote was touched. `main`, `feat/hygiene-and-editor` and `~/Documents/DualC` were
left alone.

**Commits:**

- `94139e7`: the code, the tests, the record and the plan correction.
- The commit that adds this handoff.

**The record:**

- [23/02 #66](../../docs/roadmap/23-porous-shell/02-record.md#66-the-porous-node-in-glsl-and-dualc_field_view)
  is now **PARTIAL** (T1 done, T2 is P05.2), with a T1 append.
- The numbers and rules are in the new child page
  [23/07](../../docs/roadmap/23-porous-shell/07-glsl-node.md).
- The status moved in the block README and in the three places of the roadmap index
  (`#66 PARTIAL, #67–#68 PLANNED`).

## ⚠ First: this host's GPU is hung (my doing)

A first draft of the GLSL overflowed the control-flow stack of nouveau's NV92 and hung the
GPU around 10:00:

- The kernel logged `TRAP_MP … [STACK_LIMIT]` at 09:58:58.
- From then on it repeated `PGRAPH TLB flush idle timeout fail` / `PGRAPH_STATUS … BUSY`.
- At 10:06 `xdpyinfo` timed out: **the X server stopped answering, so the owner's display is
  probably frozen.**

I could not recover it (no sudo), and I did not touch the GPU again. **P05.2's parity run and
GPU tier need a reboot first.** Details are in 23/07 § A first draft hung this host's GPU and
§ For the owner's review.

## What changed, and why

| File | What |
| --- | --- |
| `include/dualc/porous.h`, `src/implicit/porous.cpp` | `PorousField::packed()`: read-only views of the packed arrays, the bin grid and `B` |
| `examples/field_glsl.h` | `DataTexture` (RGBA32F / R32I / RGBA32I), `dataTextureChannels`, `kDataTextureWidth` = 2048, `GlslLimits` (2048 rows, 16 units), `GlslScene::dataTextures` / `notes`, a new last argument of `compileToGlsl` (defaulted) |
| `examples/field_glsl.cpp` | the `dcPorous*` prelude; `emitPorous`; the dispatcher entry; `nodeStepScale` 1.0 and `nodeFeatureScale` = `min(spacing, spacing2)` for `porous` (children not traced); `Ctx::stepCap` (0.5 after a bake fallback); the ES precision lines for `sampler2D` / `isampler2D` |
| `examples/raymarch_gl.{h,cpp}` | `uploadData2D` (GL_NEAREST, its own unit) |
| `examples/dualc_glsl_parity.cpp`, `examples/dualc_field_view.cpp` | upload the data textures after the mesh grids; the viewer prints `notes` on stderr from its `compile` lambda |
| `tests/test_field_glsl.cpp` | three `porous: field_glsl …` cases (below) |
| docs | 23/07 (new); 23/02 #66; the block README; the roadmap index; `STRUCTURE.md`; the plan file's T1 criterion |

**How it works.**

- `emitPorous` reads the twelve keys through the GLSL file's `findParam`, so that file's
  `rejectUnreadParams` passes them.
- It runs `FieldGraph::build` on the porous node, so the seeds and the complex are the
  export's own, and the volume and the control emit nothing.
- It copies `packed()` into textures, declares `u_n<i>_smooth`, and emits one call
  `dcPorousVoronoiWalls` / `dcPorousDelaunayWalls` / `dcPorousStruts` with the bins (and
  `B`) as literals.
- The texture tables are in 23/07 § The textures.

## The tests (GL-free)

- **`porous: field_glsl compiles the four cases to data textures and a smooth uniform`.** For
  each case:
  - the names, the formats and the declarations of the samplers;
  - each texture equals the builder's `packed()` array, bit for bit, with a zero-padded last
    row, at 2048 wide and the right height;
  - the textures fit the default limits;
  - one binding `u_n0_smooth` = 0.04, and the call;
  - the bins literal;
  - no `sdSphere` in the source (the volume is not traced);
  - `stepScale` 1, `featureScale` 0.3;
  - the ES precision lines.
- **`porous: field_glsl reads every key, rejects an unknown one, keeps children untraced`:**
  - all twelve keys compile;
  - `smoth=` is an error located at offset `0:`;
  - a gyroid control leaves `stepScale` 1 and `featureScale` = the spacing;
  - a repeated node uploads once (4 textures, 1 smooth binding).
- **`porous: field_glsl bakes the node past the texture limits`:**
  - `maxTextureUnits` 3 → one sampler3D, a note, `stepScale` 0.5, and values equal to
    `bakeToGrid` of the built field;
  - `maxTextureSize` 1 → baked;
  - the defaults → 4 textures.

## Measured, and under what

The conditions: this host, GCC 15, Release, one process at a time. Full detail is in 23/07
§ Measured.

- **The GLSL on llvmpipe** (Mesa 26.0.8's LLVM 21.1.8), through EGL surfaceless with
  `LIBGL_ALWAYS_SOFTWARE=1`, so neither X nor the GPU was involved. It used the committed
  value shader, `uploadData2D` / `uploadGrid3D`, and the harness's 16³ lattice against
  `valueAt`.
  - **20 graphs passed at the analytic tier 2e-3, with a worst error of 2.4e-7.** They cover
    the four cases × smooth 0 / 0.05 / 0.12, graded density, the sponge recipe, an offset
    strut lattice, a jittered box, a `bracket.obj` volume, and baked meshes sharing units
    with a node.
  - The ES and 330 trace shaders compile.
  - 17 graphs took 4.8 s and 119 MB.
- **A C++ float transliteration of the GLSL**, line for line: 12 runs with a worst error of
  1.4e-7 against `valueAt`.
- **The scratch tools** were `/tmp/pemu/emu.cpp` and `/tmp/pemu/egl_parity.cpp`, not
  committed, and gone after a reboot. The EGL one needs only `dlopen("libEGL.so.1")`:
  - `eglGetDisplay(0)` with `EGL_PLATFORM=surfaceless`;
  - `eglBindAPI(0x30A2)`;
  - a 3.3 core context, `eglCreateContext(dpy, nullptr, nullptr, {0x3098,3, 0x30FB,3, 0x30FD,1, 0x3038})`;
  - `eglMakeCurrent` with no surfaces;
  - `gladLoadGLLoader(eglGetProcAddress)`.

  It is linked like `dualc_glsl_parity` in `build-gl` and is worth rebuilding if the GPU stays
  unavailable.

## Checks run

- `cmake --build build -j` and `cmake --build build-gl -j`: no warnings. A GCC `-Wnonnull`
  false positive from a `std::vector` initialiser-list assignment was rewritten away.
- `ctest -R "porous: field_glsl"`: 3/3, 0.17 s.
- `ctest --test-dir build -R 'porous|field_graph|field_glsl|GLSL codegen|field-graph|domain'`,
  sequential: **59/59**, 23.6 s, 0.40 GB peak. This covers the driver's smoke filter.
- `ctest -R cli_field`: 66/66.
- `python3 scripts/check.py --docs`: 27 OK + 1 INFO (`index-staleness`, the same 15 entries
  as before), PASS. The pre-commit hook passed on `94139e7`.
- No docs file was deleted or moved, so no loss audit was due.
- **Not run:**
  - the GPU parity harness on real hardware with the committed GLSL (the GPU is hung);
  - the full gate, `--docs --strict`, the GPU tier. These belong to P05.2 / P08.

## What I got wrong on the way

- **I ran an untested, deeply nested shader on a 2008-era GPU** (nouveau NV92) with no
  timeout guard against a GPU hang. A wrong value would have been cheap. The hang costs the
  owner a reboot.
  - The kernel's `STACK_LIMIT` traps were already in the log after the first run. I should
    have read `journalctl -k` before the second one.
  - **Next time:** check the GLSL on llvmpipe (EGL surfaceless) first. Then run on the NV92
    one case at a time, and stop at the first trap.
- The first draft's logic was right: the transliteration matched to 1.4e-7. I spent a run
  suspecting multi-row textures before reading the kernel log.
- I first counted 21 llvmpipe graphs; there were 20 (corrected in 23/07).

## Plan corrections

T1's criterion `ctest -R field_glsl` matched **no** test before this unit: the codegen cases
are named `GLSL codegen …`, as the study's defaults table records. So it was green vacuously.
The new cases are named `porous: field_glsl …` and match both filters. The note is in the
plan file under T1's criteria.

## T1 acceptance

- **`ctest --test-dir build -R field_glsl` is green, and no longer vacuous:** 3/3.
- **`build-gl` builds:** yes, with the harness and the viewer uploading the textures.
- **The exact path fits the unit,** so `emitBakedGrid` is only the fallback past the limits
  (step 3 of the plan does not apply). It is tested.

## The in-run choices

Each is **Chosen in-run (pending owner review)**, listed in 23/07 § The in-run choices:

1. Integer textures are R32I / RGBA32I through `isampler2D`. The study said R32UI /
   `usampler2D`; the signed formats keep the packed −1.
2. The arrays reach the codegen through a public `PorousField::packed()`: a library API
   addition. P06 owns the design section that should name it.
3. `GlslLimits` defaults to the guarantees (2048 rows, 16 units), not this host's 8192.
4. A baked fallback lowers the scene's step scale to 0.5.
5. The fold is a cursor rescan, exact up to 64 primitives within k. The GLSL avoids arrays,
   `continue` and returns inside loops, because of the NV92 incident.
6. The upload is already wired into the harness and the viewer, and the viewer prints the
   notes. T2 still owns the parity cases and the rebuild check.

## For the owner's review

**needsHumanCheck is raised.**

1. **The display and the GPU.** If the desktop is frozen, reboot. Then:

   ```bash
   journalctl -k -b -1 | grep -c -i nouveau   # the hang, from the previous boot
   glxinfo -B | head -5                       # the GPU answers again
   ```

2. **Whether the committed GLSL runs on the NV92.** Run it with care, one case at a time,
   watching the kernel log:

   ```bash
   journalctl -kf | grep -i -E "trap|pgraph" &   # stop at the first line
   cd ~/Documents/DualC-porous && cmake --build build-gl -j
   timeout 60 build-gl/examples/dualc_field_view examples/samples/porous-sponge.json \
     --snapshot /tmp/sponge.png
   ```

   The snapshot command is T2's to verify. If the NV92 traps again, it is your decision
   whether to bake on that GPU (pass `GlslLimits{…, 0}` units) or to flatten the GLSL
   further.

**Verified headless:** the ctests, the transliteration, and llvmpipe (20 graphs within
2.4e-7).

## What P05.2 needs to know

- **Reboot first,** or do the parity work on llvmpipe: `LIBGL_ALWAYS_SOFTWARE=1` needs X for
  GLFW, and X is hung now. Read `journalctl -k` after every GPU run.
- **The parity cases** can reuse 23/07's graphs. The four named in the raw plan § 3 P05 T2,
  at the analytic tier `A, R` = 2e-3, should pass with room to spare (llvmpipe: ≤ 2.4e-7).
  Bump `parity_expected_cases` from 82 in the same commit.
- **The viewer compiles before it creates the window,** so it cannot pass the queried
  `GL_MAX_TEXTURE_SIZE` / `GL_MAX_TEXTURE_IMAGE_UNITS` yet. It uses the defaults. Moving the
  compile after `initWindow`, or recompiling, is T2's choice.
- **The structural rebuild already re-uploads the data textures,** through `applyScene` and
  the file-watch path at `dualc_field_view.cpp` ~line 594; T2 verifies it. The only node
  uniform is `smooth`. The thickness keys live on `onion` / `graded-*`, already uniforms.
- **Sizes:** `docs/roadmap/README.md` is at 15,345 / 15,360 bytes, and 23/02 at 15,179. The
  next status change in either needs a split or a trim.
