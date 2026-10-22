# Handoff — P05.2: parity and the viewer

Unit P05.2 of [`plan/14_P05_glsl-and-viewer.md`](../14_P05_glsl-and-viewer.md) § T2, the last
unit of P05. Session of 2026-10-22, branch `feat/porous-shell` in `~/Documents/DualC-porous`.
Nothing was pushed and no remote was touched. `main`, `feat/hygiene-and-editor` and
`~/Documents/DualC` were left alone.

**Commits:**

- `b49f148`: the parity cases with the count bump (82 → 87) in the same commit, the viewer's
  queried limits, and the GL-free rebuild test.
- The commit that adds the record and this handoff.

**The record:**

- [23/02 #66](../../docs/roadmap/23-porous-shell/02-record.md#66-the-porous-node-in-glsl-and-dualc_field_view)
  is now **DONE** 2026-10-22, with a T2 append. The NV92 run is listed for the owner's review.
- The numbers are in the new child page
  [23/08](../../docs/roadmap/23-porous-shell/08-parity-and-viewer.md).
- The status changed in the block README and in the three places of the roadmap index
  (`#63–#66 DONE, #67–#68 PLANNED`).

## ⚠ The host's display is still hung

The host had not rebooted after P05.1's NV92 hang:

- at 10:18 it was up 2 days 18 h;
- `xdpyinfo` timed out;
- the last nouveau trace in the kernel log was at 10:07.

I did not touch the NV92. GLFW needs X, and Xvfb is not installed. So every GL run in this
unit went through **Mesa llvmpipe via EGL surfaceless**, with the committed harness and viewer
code unchanged and only the context swapped (below). CI's `gpu` job uses the same renderer
(llvmpipe, under Xvfb).

## What changed, and why

| File | What |
| --- | --- |
| `examples/dualc_glsl_parity.cpp` | `porous-voronoi-walls`, `porous-delaunay-walls`, `porous-struts`, `porous-smooth` (Delaunay struts, smooth 0.05), `porous-sponge` (the sample recipe); all at `A, R` = 2e-3 over `box(-1.2, 1.2)` |
| `scripts/check_data.json` | `parity_expected_cases` 82 → 87 |
| `examples/dualc_field_view.cpp` | after `initWindow`, it queries `GL_MAX_TEXTURE_SIZE` and `GL_MAX_TEXTURE_IMAGE_UNITS`, and recompiles when a baked fallback may now fit or a smaller limit is passed; reloads use the queried limits. This closes P05.1's open "the viewer cannot pass the queried limits" |
| `tests/test_field_glsl.cpp` | `porous: field_glsl a structural key changes the textures, smooth only its uniform` |
| docs | 23/08 (new); 23/02 #66; the block README; the roadmap index |

**The viewer's structural rebuild** already existed. The file watch (or `l`) re-runs
`compile`, and `applyScene` deletes the old textures and uploads the new data textures. What
had to hold is in the codegen, and the new test checks it:

- a `smooth` change leaves the source and the textures equal and moves only `u_n0_smooth`;
- `spacing`, `seed`, `dist`, `relax`, `cells`, `mode`, `jitter` and the control each change
  the source or a texture;
- in the sponge recipe, `t1`, `t2`, `k` and `smooth` are one binding each.

## Measured, and under what

The conditions: this host, GCC 15, Release, one process at a time. The renderer was Mesa
26.0.8 llvmpipe (LLVM 21.1.8), GL 4.5 core, run with
`LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless`, under `timeout` and
`prlimit --as=8000000000`.

- **The parity harness: 87/87**, 8.9 s, 152 MB peak.
  - The porous worst errors: 1.64e-7 (Voronoi walls), 2.32e-7 (Delaunay walls),
    1.77e-7 (struts), 2.19e-7 (smooth), 2.60e-7 (sponge). The tolerance is 2e-3.
  - The earlier 82 cases all passed: worst 2.75e-5 on the finite-difference tier, mesh signs
    0 wrong.
- **The GPU tier, through the gate's wrapper:**
  `python3 scripts/check.py --gpu --strict --build-dir /tmp/p052/gl` gave
  **`[OK] parity 87/87 passed`** in 1.75 s. The scratch binary sat at
  `/tmp/p052/gl/examples/dualc_glsl_parity`.
- **The snapshots,** through the viewer's `--snapshot` path, 800 × 600:
  - `build/porous-snapshots/porous-sponge.png`, 86 KB: `examples/samples/porous-sponge.json`,
    walls; 5.3 s, 150 MB.
  - `build/porous-snapshots/porous-sponge-struts.png`, 143 KB:
    `build/porous-snapshots/porous-sponge-struts.json`, which is
    `smooth-intersection(sphere(radius=1), offset(porous(sphere(radius=1), mode=struts, spacing=0.25), r=0.04), k=0.1)`;
    20.4 s, 157 MB.

  I looked at both. The walls show a sphere of cells, open at the rim with cavities behind.
  The struts show a ball of round struts, with stubs where the sphere cuts them (06's
  finding 5).
- **The ctests:**
  - `ctest -R "porous: field_glsl"`: 4/4, 0.25 s.
  - `ctest --test-dir build -R 'porous|field_graph|field_glsl|GLSL codegen|field-graph|domain'`,
    sequential: **60/60**, 23.6 s, 0.40 GB peak. This covers the driver's smoke filter.
- **The scratch tool,** not committed and gone after a reboot.
  - `/tmp/p052/egl_init.cpp` defines `dce::gl::initWindow`. It opens EGL surfaceless (3.3
    core), loads glad, and exits unless the renderer is llvmpipe. It binds a 1600 × 1200 FBO
    in place of the default framebuffer and returns a dummy `GLFWwindow*`; GLFW's other calls
    are no-ops on an uninitialised library.
  - It is linked first, with `-Wl,--allow-multiple-definition`, against build-gl's own
    `dualc_glsl_parity.cpp.o` / `dualc_field_view.cpp.o` and the target's static libs (the
    link lines are from `ninja -C build-gl -t commands <target>`). 23/08 has the details.

## Checks run

- `cmake --build build -j` and `cmake --build build-gl -j`: no warnings.
- The ctests above.
- The GPU tier above, on llvmpipe.
- `python3 scripts/check.py --docs`: 27 OK + 1 INFO (`index-staleness`, the same 15 entries),
  PASS. The pre-commit hook passed on `b49f148`.
- No docs file was deleted or moved, so no loss audit was due.
- **Not run:**
  - the GPU tier on the NV92 (display hung);
  - the full gate and `--docs --strict` (P08's).

## What I got wrong on the way

- **The first gate-wrapper run failed: `expected 87 cases, harness announced 80`.** The gate
  runs the harness from the repo root, and this worktree had no `data/`, so the seven mesh
  cases skipped themselves. I ran `build/examples/dualc_gen_demo all --dir data` (data/ is
  gitignored). **P08 needs that `data/` too.** It is there now.
- **I first wrote "the earlier cases within 3.7e-7"** from the tail of the output. The full
  run's worst was 2.75e-5, on the finite-difference tier. I corrected it before committing.
- **I first told the owner to nudge `smooth` on the sponge sample.** The sample has no
  `smooth`, and a nudge is multiplicative, so a zero stays zero. The review steps now add
  `"smooth": 0.02` first.
- **My rebuild test first listed `spacing2` without a control,** which the builder rejects.
  It now goes with the control case.

## The in-run choices

Each is **Chosen in-run (pending owner review)**, listed in 23/08 § The in-run choices:

1. Five parity cases, not four: the sponge is added, and the Delaunay struts are the smooth
   case.
2. The GPU tier ran on llvmpipe through a scratch EGL context, because X was hung. A committed
   headless `--egl` mode for the harness is not built; it is the owner's call.
3. The snapshots stay under `build/porous-snapshots/`, not the block folder. `docs/` has no
   binaries, 86–143 KB is five to nine times the page byte cap, and these are llvmpipe
   renders.
4. The viewer recompiles only when the queried limits change the scene.
5. The NV92 was not used.

## Exit criteria of P05, one by one

- **"The viewer shows the porous node live, matching the CPU field within the parity
  tolerances."**
  - Matching within tolerance: the harness, with the same codegen and the same uploads as the
    viewer, gives 87/87, and the porous cases are within 2.6e-7 against 2e-3. Run on llvmpipe,
    and also through `check.py --gpu --strict`.
  - The viewer shows the node: its own `--snapshot` path rendered the sponge (walls) and the
    struts recipe on llvmpipe, and both look right.
  - **Live, in a window, and on the NV92: not verified.** The display is hung. This is the
    owner's review (below) and the reason `needsHumanCheck` is raised.
- **T1's criteria** (P05.1): `ctest -R field_glsl` is green (now 4 tests), and build-gl builds.
- **T2's criteria:** the parity cases exist and pass (on llvmpipe; the missing NV92 context
  is recorded); the count is bumped in the same commit; the snapshots are listed above.

## For the owner's review

**needsHumanCheck is raised.** The steps are also in 23/08 § For the owner's review.

1. **Reboot** (the display is still hung from P05.1), then run `glxinfo -B | head -5`.
2. **One porous graph on the NV92 first.** The harness runs all 87 cases in one process, so a
   trap there would hang the GPU again.

   ```bash
   journalctl -kf | grep -i -E "trap|pgraph" &    # stop at the first line
   cd ~/Documents/DualC-porous && cmake --build build-gl -j
   timeout 60 build-gl/examples/dualc_field_view examples/samples/porous-sponge.json \
     --snapshot /tmp/sponge-nv92.png
   ```

   Compare the result with `build/porous-snapshots/porous-sponge.png`. If the log stays
   silent, run `python3 scripts/check.py --gpu --strict --build-dir build-gl`; 87/87 is
   expected.
3. **Live:** `build-gl/examples/dualc_field_view examples/samples/porous-sponge.json`.
   - Nudge `t2`: the walls thicken, with no recompile.
   - Add `"smooth": 0.02` to the porous node and save: it reloads. Then nudge `smooth`: the
     corners round off.
   - Change `spacing` 0.4 → 0.3 and save: the cells shrink.
4. **Look at the two PNGs** under `build/porous-snapshots/`.

**Verified headless:** the parity and the gate wrapper (llvmpipe), both snapshots, and the
ctests.

## What the next unit needs to know

- **Sizes:**
  - `docs/roadmap/23-porous-shell/02-record.md` is at **15,357 / 15,360 bytes**. P06's #67
    status change needs a split of 02 into a `02-record/` folder (numbers are IDs; run
    `scripts/docs_loss_audit.py` before committing the move), or a trim.
  - `docs/roadmap/README.md` is at 15,306.
- **For P06's `12-dualc_field_view` page:**
  - the viewer prints `recompiling for this context's limits (R rows, U units)` when it
    recompiles;
  - the nudge caveats in 23/08 § What a nudge does not check: a zero `smooth` stays zero, and
    a struts `smooth` nudged to B or past it fills the shell.
- **For P08:**
  - `data/` exists in this worktree now; the GPU tier needs it.
  - If the display is still hung, the scratch EGL route in 23/08 rebuilds in a minute.
- `build/porous-snapshots/` holds the struts recipe JSON beside the PNGs.
