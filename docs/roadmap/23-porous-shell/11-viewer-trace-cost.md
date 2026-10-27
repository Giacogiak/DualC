# The viewer's trace cost on a sponge — #70, measured and fixed

The numbers behind [02 #70](02-record.md#70-smooth-booleans-keep-the-viewers-metric-step)
(2026-10-27). The owner orbited a porous sponge in `dualc_field_view` on a GTX 1050 machine
and found it "extremely laggy":

```bash
dualc_field_view --expr "smooth-intersection(sphere(radius=1),onion(porous(sphere(radius=1),spacing=0.3,smooth=0.02),thickness=0.04),k=0.1)"
```

## The cause

The trace (`kTraceFramework` in `examples/field_glsl.cpp`) takes the metric fast path only
when `nodeStepScale(root)` is 1.0. On that path it steps by the value directly. Otherwise
each step also samples a 6-point gradient to normalise the step, which makes 7
evaluations a step. `nodeStepScale` gave every smooth boolean 0.6. So every sponge recipe
(the canonical one is a `smooth-intersection`) paid the gradient on every step, and every
evaluation walks the porous complex's data textures.

The 0.6 was not needed. The polynomial smooth-min that both the C++
(`smoothMinValue`, `src/implicit/combinators.cpp`) and the GLSL (`opSmoothUnion` /
`opSmoothInter` / `opSmoothDiff`) use is `lerp(b, a, h) − k·h·(1 − h)`. Its gradient is
exactly `h∇a + (1 − h)∇b`: the derivative of the `k·h·(1 − h)` term cancels the lerp's
`∂h` terms. So a smooth boolean of 1-Lipschitz operands is 1-Lipschitz, and stepping by its
value stays safe.

The viewer has no `--depth`: nothing is contoured. Cell size did not drive the lag. Bigger
cells (spacing 0.8) cost the same per pixel.

## Measured

A scratch CPU copy of the trace loop, not committed, counted the `sceneSDF` evaluations
per pixel. It used the viewer's own uniforms (`compileToGlsl`'s step and feature scales,
the 5 % bounds pad, the default orbit camera, the 1100 × 800 aspect) at 160 px wide, with
the library's `valueAt` in place of the GLSL.

A reference trace, a metric step at scale 0.25 with a 20,000-step cap, gave the true hits.
It is safe for any 1-Lipschitz field. "Differ" counts the pixels whose hit or miss differs
from it.

The table gives the evaluations per pixel: mean / 95th percentile / worst. The worst pixel
matters on a GPU, where a warp runs as long as its slowest pixel.

| Graph | View | Shipped (0.6, gradient) | Metric, scale 1 | Differ: shipped → metric |
| --- | --- | --- | --- | --- |
| The owner's expression | default | 44 / 217 / 1190 | 7.8 / 41 / 157 | 0 → 0 |
| The owner's expression | zoomed (0.35 × distance) | 193 / 350 / 1652 | 41 / 58 / 231 | 2 → 2 |
| `porous-sponge.json` | default | 41 / 203 / 1344 | 7.2 / 39 / 137 | 0 → 0 |
| `porous-sponge.json` | zoomed | 184 / 315 / 966 | 39 / 53 / 132 | 0 → 0 |
| Spacing 0.8, thickness 0.03 | zoomed | 210 / 371 / 756 | 39 / 57 / 102 | 5 → 3 |
| A 4 × 4 × 4 box, spacing 0.25 | zoomed | 212 / 399 / 1428 | 41 / 62 / 200 | 0 → 0 |

The metric step at the old 0.6 scale fell between the two: 48–52 per pixel zoomed in. The
shipped trace evaluated more than the fine reference did (77–90 zoomed in), because its
steps cost 7 evaluations.

## The fix and its checks

`nodeStepScale` now gives a boolean, smooth or not, the minimum of its operands' scales.
A TPMS operand still makes it 0.5, a warp 0.5, a baked `mesh` 0.6 and a `winding` 0.5. So
a graph of primitives, `porous`, onions and smooth booleans takes the fast path, and a TPMS
or a baked mesh in it still does not. `normalize` was already 1.0, so a smooth boolean of a
normalised TPMS now takes the fast path too, as a hard boolean of one already did.

- **Test:** the porous GLSL case in `tests/test_field_glsl.cpp` asserts the owner's
  expression compiles at step scale 1.0, and `smooth-union(sphere, gyroid)` at 0.5.
- **Snapshots:** `--snapshot` on llvmpipe (Mesa 26.0.8), 800 × 600, before and after the
  change.
  - `porous-sponge.json` is identical pixel for pixel. The owner's expression differs in 3
    of 480,000 pixels by more than 24 of 255.
  - Wall time per snapshot, startup and shader compile included, fell from 8.8 s to 3.9 s
    for the owner's expression and from 5.7 s to 3.4 s for the sample.
- **Gate:** `check.py` and the GPU tier (`--gpu --strict --build-dir build-gl`, llvmpipe);
  the results are in [02 #70](02-record.md#70-smooth-booleans-keep-the-viewers-metric-step).

**Not measured:** frame times on the GTX 1050. The owner's next orbit is the check.

**Left as it was:** `graded-onion` and `graded-offset` pass their operand's scale through,
although a thickness that varies with the control makes them `1 + |dt/dc|·|∇c|`-Lipschitz
(1.04 for `porous-sponge.json`). The trace's 0.8 factor covers a slope up to 0.25. A steeper
ramp would need its own scale, and none was asked for.

---

← Back to [the block](README.md).
