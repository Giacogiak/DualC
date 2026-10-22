# The porous node in GLSL — #66, parity and the viewer

The numbers and rules behind [02 #66](02-record.md#66-the-porous-node-in-glsl-and-dualc_field_view)'s
second half (P05.2 T2, 2026-10-22). [07](07-glsl-node.md) is the first half: the emitter and
its data textures.

## What was built

| File | What |
| --- | --- |
| `examples/dualc_glsl_parity.cpp` | five `porous-*` cases at the analytic tier (next section) |
| `scripts/check_data.json` | `parity_expected_cases` 82 → 87, in the same commit |
| `examples/dualc_field_view.cpp` | after the window opens, the scene is recompiled against the context's `GL_MAX_TEXTURE_SIZE` and `GL_MAX_TEXTURE_IMAGE_UNITS` when that changes it; every reload uses those limits |
| `tests/test_field_glsl.cpp` | `porous: field_glsl a structural key changes the textures, smooth only its uniform` |

**The parity cases.** Each is one graph over `box(-1.2, 1.2)`, compared with the built
field's `valueAt` on the harness's 16³ lattice at `A, R` = 2e-3. The box reaches past the
sphere volume, so the hull side of the walls and the struts' clamp at B are compared too.

| Case | Graph | Worst error (llvmpipe) |
| --- | --- | --- |
| `porous-voronoi-walls` | `porous(sphere(radius=1),spacing=0.3)` | 1.64e-7 |
| `porous-delaunay-walls` | the same, `cells=delaunay` | 2.32e-7 |
| `porous-struts` | the same, `mode=struts` (Voronoi) | 1.77e-7 |
| `porous-smooth` | `cells=delaunay,mode=struts,smooth=0.05` | 2.19e-7 |
| `porous-sponge` | `examples/samples/porous-sponge.json` as shorthand | 2.60e-7 |

`porous-smooth` is also the Delaunay struts case, so the four complexes are all covered.
`porous-sponge` puts the node under `graded-onion` and `smooth-intersection`.

**The viewer.** A structural edit already rebuilds the textures:

- the file watch (or `l`) re-runs `compile` on the new graph;
- `applyScene` deletes the previous textures and uploads the new scene's mesh grids and data
  textures;
- a nudge only rewrites the selected uniform.

So what has to hold is in the codegen, and the new GL-free case checks it:

- `smooth` 0.04 → 0.08 leaves the source and every texture equal, and moves only
  `u_n0_smooth`;
- each of `spacing`, `seed`, `dist`, `relax`, `cells`, `mode`, `jitter` and the control
  changes the source or a texture;
- in the sponge recipe, `t1`, `t2`, `k` and `smooth` are one binding each, so they nudge.

**The queried limits.** The first compile runs before the window exists, so a bad graph never
opens one. It uses `GlslLimits{}`, the guaranteed 2048 rows and 16 units. Once the context
answers, its limits replace those. The scene is recompiled in two cases:

- it took the baked fallback and the context's limits are wider;
- one of its textures, or its unit count, passes a smaller limit.

Otherwise it is kept as it is. The recompile prints
`recompiling for this context's limits (R rows, U units)` on stderr. This host reports 8192
rows (01 § d), so a fallback that 8192 rows would hold now traces exact.

## The GPU tier, run on llvmpipe

**No display.** The NV92 hang of [07](07-glsl-node.md#a-first-draft-hung-this-hosts-gpu)
was still on at 10:18:

- the host had not rebooted (up 2 days 18 h);
- `xdpyinfo` timed out;
- the kernel log had stayed silent since its 10:07 nouveau trace.

GLFW needs X, and Xvfb is not installed. So `dualc_glsl_parity` and `dualc_field_view` could
not open their window, and the NV92 was not touched again.

**What ran instead** was the committed harness and viewer, unchanged, with only the context
replaced. A scratch `initWindow` (`/tmp/p052/egl_init.cpp`, not committed) does four things:

- it opens EGL surfaceless (`dlopen("libEGL.so.1")`, a 3.3 core context, the attributes of
  07 § Measured);
- it loads glad through `eglGetProcAddress`;
- it exits unless `GL_RENDERER` names llvmpipe;
- it binds a 1600 × 1200 RGBA8 FBO in place of the default framebuffer. The snapshot path
  never rebinds 0.

The binaries link it first, before the build-gl objects, with
`-Wl,--allow-multiple-definition`. GLFW's other calls are no-ops on an uninitialised
library. Every run had `LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless`, under `timeout`
and `prlimit --as=8000000000`.

CI's required `gpu` job runs the same renderer, Mesa llvmpipe, under Xvfb (check.py's
`check_parity` docstring). So this is the CI's tier on this host's Mesa, not the NV92's.

## Measured

This host, GCC 15, Release, one process at a time, Mesa 26.0.8, llvmpipe (LLVM 21.1.8,
128 bits), GL 4.5 core.

- **The harness:** 87/87 passed in 8.9 s, 152 MB peak. The porous cases are in the table
  above. The 82 earlier cases all passed: the worst was 2.75e-5, on the finite-difference tier, and the mesh signs had 0 wrong.
- **The gate's wrapper:** `python3 scripts/check.py --gpu --strict --build-dir /tmp/p052/gl`,
  with the scratch binary at `/tmp/p052/gl/examples/`, gave `[OK] parity 87/87 passed` in
  1.75 s.
  - Its first run failed with `expected 87 cases, harness announced 80`. The gate runs the
    harness from the repo root, and this worktree had no `data/`, so the seven mesh cases
    skipped themselves. That is the trap the wrapper exists for.
  - `build/examples/dualc_gen_demo all --dir data` (gitignored) fixed it. P08's GPU tier
    needs the same `data/` in this worktree.
- **The snapshots:** the viewer's `--snapshot` path, 800 × 600 (below).
- **The ctests:**
  - `ctest -R "porous: field_glsl"`: 4/4, 0.25 s;
  - `ctest --test-dir build -R 'porous|field_graph|field_glsl|GLSL codegen|field-graph|domain'`,
    sequential: 60/60, 23.6 s, 0.40 GB peak.

## The snapshots

Under `build/porous-snapshots/` in this worktree. They are gitignored, not in the block's
folder (in-run choice 3).

| File | Graph | Compile, render |
| --- | --- | --- |
| `porous-sponge.png` (86 KB) | `examples/samples/porous-sponge.json`: walls, closed cells | 5 nodes, 10 params, step scale 0.6; 5.3 s, 150 MB |
| `porous-sponge-struts.png` (143 KB) | `porous-sponge-struts.json` beside it: `smooth-intersection(sphere(radius=1), offset(porous(sphere(radius=1), mode=struts, spacing=0.25), r=0.04), k=0.1)`, the sphere's bone recipe of [06](06-sponge-end-to-end.md#the-recipes-as-run) with the sponge's rounded rim | 4 nodes, 5 params, step scale 0.6; 20.4 s, 157 MB |

They show:

- **The walls:** a sphere of cells, open at the rim with cavities behind, walls thickening
  outwards.
- **The struts:** a ball of round struts, with stubs where the sphere cuts them (06's
  finding 5).

To render them again, run from the repo root with the scratch binary, or with
`build-gl/examples/dualc_field_view` on a working display:

```bash
dualc_field_view examples/samples/porous-sponge.json --snapshot build/porous-snapshots/porous-sponge.png
dualc_field_view build/porous-snapshots/porous-sponge-struts.json --snapshot build/porous-snapshots/porous-sponge-struts.png
```

## What a nudge does not check

These are the viewer's behaviour, not the node's. They are recorded here, not fixed:

- **A zero `smooth` stays zero.** A nudge multiplies the value, as it does for every scalar
  binding (`k=0` behaves the same).
- **A nudge can pass the struts' bound.** The builder rejects `smooth ≥ B` for struts. A nudge
  is not validated, so the GLSL ceiling `B − smooth` can reach zero or below, and the shell
  then fills solid. Reloading the file restores it.

## The in-run choices

Each is **Chosen in-run (pending owner review)**:

1. **Five parity cases, not four.** The four named ones, with the Delaunay struts as the
   `smooth` case, plus the sponge sample: the node under the graded shell is what the viewer
   shows.
2. **The GPU tier ran on llvmpipe through a scratch EGL context**, because X was hung. The
   committed code is unchanged. A headless `--egl` mode for the harness would let an
   unattended run do this without scratch code; that is the owner's call, and not built.
3. **The snapshots stay under `build/`.** `docs/` holds no binary file, a PNG of 86–143 KB is
   five to nine times the page byte cap, and these are llvmpipe renders, not the owner's GPU.
4. **The viewer recompiles only when the limits change the scene.** It does not recompile
   always, because a large complex takes its build time again.
5. **The NV92 was not used.** 07 asked for a reboot first, and none had happened.

## For the owner's review

**needsHumanCheck is raised.** Headless, I verified the parity on llvmpipe (87/87), the
gate's wrapper, both snapshots through the viewer's own path, and the GL-free rebuild test.

1. **Reboot** (07 § For the owner's review), then check that the GPU answers:
   `glxinfo -B | head -5`.
2. **The node on the NV92, one graph first.** The harness runs all 87 cases in one process,
   so a trap there would hang the GPU again. Start with one porous viewer snapshot under a
   watch:

   ```bash
   journalctl -kf | grep -i -E "trap|pgraph" &    # stop at the first line
   cd ~/Documents/DualC-porous && cmake --build build-gl -j
   timeout 60 build-gl/examples/dualc_field_view examples/samples/porous-sponge.json \
     --snapshot /tmp/sponge-nv92.png
   ```

   If it is silent and the image matches `build/porous-snapshots/porous-sponge.png`, run the
   tier: `python3 scripts/check.py --gpu --strict --build-dir build-gl` (87/87 expected;
   `data/` is already generated).
3. **Live, in the window:** `build-gl/examples/dualc_field_view examples/samples/porous-sponge.json`.
   - Tab to `t2` and nudge it with up / down: the outer walls thicken, with no recompile.
   - Add `"smooth": 0.02` to the porous node in the JSON and save: `reloaded` is printed. The
     sample has no `smooth`, and a nudge cannot move a zero. Then tab to `smooth` and nudge
     it: the cell corners round off.
   - Edit `spacing` 0.4 → 0.3 and save: the cells shrink, and nothing leaks.

---

← Back to [the block](README.md).
