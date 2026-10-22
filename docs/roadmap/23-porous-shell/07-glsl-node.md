# The porous node in GLSL — #66, the emitter

The numbers and rules behind [02 #66](02-record.md#66-the-porous-node-in-glsl-and-dualc_field_view)'s
first half (P05.1 T1, 2026-10-22). The packed complex of [04](04-complex-and-bins.md) is
uploaded as data textures, and the GLSL queries it as `porousDistance` does. The layout
follows [01 § (d)](01-study.md#d-the-gpu-layout).

## What was built

| File | What |
| --- | --- |
| `include/dualc/porous.h`, `src/implicit/porous.cpp` | `PorousField::packed()`: read-only views (`Array<T>`: pointer + size) of the packed arrays, the bin grid and `B`, valid for the field's lifetime |
| `examples/field_glsl.{h,cpp}` | `DataTexture` (RGBA32F / R32I / RGBA32I, 2048 wide), `GlslScene::dataTextures` and `notes`, `GlslLimits` (a new last argument of `compileToGlsl`), `emitPorous`, the `dcPorous*` prelude, `nodeStepScale` 1.0 and `nodeFeatureScale` = the smaller spacing for `porous`, the ES precision lines for `sampler2D` / `isampler2D` |
| `examples/raymarch_gl.{h,cpp}` | `uploadData2D`: an unfiltered 2D texture (GL_NEAREST, needed for an integer texture to be complete) on its own unit |
| `examples/dualc_glsl_parity.cpp`, `examples/dualc_field_view.cpp` | both upload the data textures after the mesh grids (units `meshes.size() + i`); the viewer prints `GlslScene::notes` on stderr |
| `tests/test_field_glsl.cpp` | three `porous: field_glsl …` cases |

**One builder.** `emitPorous` runs `FieldGraph::build` on the `porous` node itself, then
uploads that field's `packed()` arrays. So the seeds and the complex are the export's, bit
for bit, and the volume and the control emit no GLSL. Building the node alone gives the same
field as building the whole tree: the node hands its children their margins without reading
the one it is handed (`childBakeMargin`). The node reads its twelve keys through the GLSL
file's own `findParam`, so that file's `rejectUnreadParams` passes them and the builder
checks their values. `smooth` is the node's one uniform (`u_n<i>_smooth`): the arrays do not
depend on it. Every other key changes the complex, so it recompiles. The bin origin, size and
dims and `B` are GLSL literals.

## The textures

Item `i` sits at texel `ivec2(i % 2048, i / 2048)`. The last row is padded with zeros.

| Case | Textures (sampler suffix: format, texels) | GLSL |
| --- | --- | --- |
| Voronoi walls | `seeds`: RGBA32F, 1 per seed; `offsets`, `neighbours`, `start`: R32I | `dcPorousVoronoiWalls` |
| Delaunay walls | `planes`: RGBA32F, 4 per tet (face k at 4t + k); `tetNeighbours`: RGBA32I, 1 per tet; `start`: R32I | `dcPorousDelaunayWalls` |
| Struts (both cells) | `segments`: RGBA32F, 2 per segment (a at 2s, b at 2s + 1); `binOffsets`, `binItems`: R32I | `dcPorousStruts` |

**Limits and the fallback.** The node takes the baked path when one of its textures needs
more rows than `GlslLimits::maxTextureSize`, or when the scene's units (mesh grids plus data
textures, this node's included) pass `maxTextureUnits`. The baked path is the builder's field
through `bakeToGrid` over the trace box at `--grid-res`, then `emitBakedGrid`, as an unbaked
mesh is baked. A note goes into `GlslScene::notes`, and the scene's step scale drops to 0.5.

The defaults are the guaranteed values: 2048 rows and 16 units. At 2048 rows a texture holds
4.2M texels. By 04's per-seed sizes, that is about:

- 280k seeds for Voronoi walls (the neighbours, 15 per seed);
- 161k for Delaunay walls (the planes, 26);
- 168k for Voronoi struts (the segments, 24.9);
- 135k for Delaunay struts (the bin items, 31).

An app with a context can pass the values it queried. This host reports 8192 rows (01 § d),
which quadruples those counts.

## The GLSL

The prelude ports `porous_complex.cpp`'s query to float, part for part:

- the bin coordinate, including the clamp to the boundary bin;
- the greedy seed walk, ties broken by (d², index);
- the visibility walk, which takes the face p is farthest beyond (the first on a tie) and
  returns 0 once it would leave the hull;
- the bisector, plane and segment distances;
- `porousSmin`;
- the struts' 3 × 3 × 3 search, which visits the centre bin first and skips a bin that
  cannot hold anything nearer than the best so far;
- `min(·, B)`, the clamp at 0, and `PorousField`'s ceiling `B − smooth`.

The walks stop at 1,024 steps, as `kPorousMaxWalk` does.

**The fold is a cursor rescan.** The CPU folds the primitives within k of the minimum in
ascending (d, index) order, each index once. The GLSL first scans for the minimum, which
gives (m, the lowest index at m). Each later step scans again for the smallest (d, index)
pair after the cursor and below `m + k` (struts: below `min(m + k, B)`) and folds it. A
segment filed in two bins is the same pair twice, so it is taken once. The Delaunay walls
sort their four values with a sorting network: their indices are distinct, so only the
values enter the fold.

The rescan is exact for up to 64 primitives within k of the minimum; a generic cell corner
has 3. The CPU has no such cap. The code has no arrays, no `continue` and no return inside a
loop, and it uses selects where it can (next section).

## A first draft hung this host's GPU

The first draft had a different shape. It folded through a sorted eight-pair buffer, a struct
holding arrays passed `inout`. Its loops nested pass / bin / item with `continue`. The
Delaunay walls kept a local `float[4]` and indexed an `ivec4` dynamically.

The GPU was nouveau's NV92 (Mesa, GL 3.3), in a temporary copy of the parity harness that was
not committed. On it, Voronoi walls at k = 0 matched (3.6e-8). Every other case failed:

- Delaunay walls gave 0 inside tets;
- struts gave a constant 0.0806 at the box corners, where the CPU gives `B`;
- at k > 0, the errors were 0.04–0.32.

From the first run (09:58:58), the kernel logged `TRAP_MP … [STACK_LIMIT]` on all seven TPs,
plus trapped reads and writes. That is the shader overflowing the MP's control-flow stack. A
second run at spacing 0.6 never returned. From then on the log repeated
`PGRAPH TLB flush idle timeout fail` and `PGRAPH_STATUS … BUSY`. A later `glxinfo` blocked,
and at 10:06 `xdpyinfo` timed out: the X server had stopped answering. The GPU has not been
used since.

**The rewrite** (section The GLSL) removes every construct named above. **It has not run on
the NV92**, which was still hung when this unit ended. It ran on llvmpipe instead (next
section).

## Measured

All measurements: this host, GCC 15, Release, one process at a time.

- **A float transliteration of the GLSL** in C++, line for line, against
  `PorousField::valueAt` (a scratch program in `/tmp`, not committed).
  - Sphere volume, spacing 0.25, the four cases, smooth 0, 0.05 and 0.12; 16³ points over
    ±1.
  - Worst error 1.4e-7 (Delaunay walls); 0 of 4096 points over 2e-3 in each of the 12 runs.
  - It caught nothing that the GPU had not already shown. It did show that the first draft's
    logic was right as well, so the failures were the driver's.
- **The GLSL on Mesa's llvmpipe 21.1.8** (LLVM 21.1.8). The context came from EGL
  surfaceless with `LIBGL_ALWAYS_SOFTWARE=1`, so neither X nor the GPU was used; the program
  stopped unless `GL_RENDERER` named llvmpipe. The shaders were the committed value shader,
  the uploads `uploadData2D` and `uploadGrid3D`, and the comparison the harness's 16³ lattice
  against `valueAt`, at the analytic tier 2e-3. **20 graphs passed:**
  - the four cases at smooth 0, 0.05 and 0.12, the most error 1.4e-7;
  - graded (spacing 0.15 → 0.35, Voronoi walls and Delaunay struts): 2.1e-7;
  - the sponge recipe `smooth-intersection(sphere, graded-onion(porous(sphere, spacing=0.4), …), k=0.1)`:
    2.4e-7, step scale 0.6;
  - an offset Delaunay strut lattice: 2.4e-7;
  - a jittered, relaxed box: 1.4e-7;
  - a `bracket.obj` volume: 1.3e-7;
  - a `cube.obj` `bake=64` grid on unit 0 beside a node's four textures: 5.4e-8;
  - a `bracket.obj` `bake=96` intersected with a struts shell: 1.7e-8.

  The same two scenes with unbaked meshes failed, by the bake's own error (8.1e-3 on the
  cube; 0.16 in the bracket's over-estimated far field). The harness's mesh cases already
  compare those by sign or against the grid, so this is not the node's error.

  Both trace shaders compiled, the ES one (`#version 300 es`) and the 330 one. The run of the first 17
  graphs took 4.8 s and 119 MB.
- **The ctests:**
  - the three new cases take 0.17 s;
  - `ctest -R 'porous|field_graph|field_glsl|GLSL codegen|field-graph|domain'`: 59/59, 23.6 s,
    0.40 GB peak;
  - `ctest -R cli_field`: 66/66, 27.5 s.

## The in-run choices

Each is **Chosen in-run (pending owner review)**:

1. **Integer textures are R32I / RGBA32I through `isampler2D`**, where the study said R32UI /
   RGBA32UI through `usampler2D`. They are just as exact, and they keep the packed `−1` (a
   hull neighbour).
2. **The packed arrays reach the codegen through a public `PorousField::packed()`**, not the
   internal header: one view, owned by the field.
3. **`GlslLimits` defaults to the guarantees** (2048 rows, 16 units), not to this host's
   8192. P05.2 can pass the values the viewer queries.
4. **A baked fallback sets the step scale to 0.5.** A trilinear grid of a 1-Lipschitz field
   is at most √3 steep.
5. **The fold rescans** and is exact to 64 primitives within k. The GLSL avoids arrays,
   `continue` and returns inside loops, because of the NV92 incident.
6. **The upload is wired into the harness and the viewer now** (units after the meshes), and
   the viewer prints the notes. T2 adds the parity cases and checks the rebuild.

## For the owner's review

- **The display.** My first-draft shader hung the NV92 around 10:00 on 2026-10-22. The kernel
  logged `PGRAPH` timeouts, and X stopped answering. If the desktop is frozen, reboot. Then
  check `journalctl -k -b -1 | grep -c nouveau` (the hang) and `glxinfo -B` (the GPU is back)
  before P05.2's GPU tier.
- **Whether the rewritten GLSL runs on the NV92.** It needs a GPU, run with care: one case at
  a time, under `timeout`, with `journalctl -kf | grep -i trap` watched. If it still traps,
  that is the owner's decision: bake on this GPU (`GlslLimits` can force it), or flatten it
  further.

Verified headless: the ctests above, the transliteration and llvmpipe.

---

← Back to [the block](README.md).
