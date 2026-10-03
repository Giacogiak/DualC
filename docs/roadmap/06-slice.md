# Field-on-plane slicing — `dualc_slice`

Part of the dense-lattice viz/manufacturing deliverable (see
[11-dense-lattice-deliverable/](11-dense-lattice-deliverable/README.md) for the
rationale and the other steps). Per-command catalogue:
[../command_reference/07-dualc_slice.md](../command_reference/07-dualc_slice.md).

## Step 1. Field-on-plane sampler — the shared core of A.1 viz and B.1 slicing. [DONE — 2026-06-02]

New standalone CLI `examples/dualc_slice.cpp`. Mirrors `dualc_lattice`'s
front-end (`--type --wavelength --offset --normalize-thickness --bounds`) so it
slices the identical lattice-in-mesh field that would be manufactured; adds
`--plane AXIS=VALUE` (axis-aligned) and `--res N` (defaults and limits:
[command_reference/07](../command_reference/07-dualc_slice.md)). Evaluates `field.valueAt` on a res×res corner
grid over the cutting plane — cost O(res²), independent of lattice density — and
writes two outputs: a **PNG heatmap** (diverging blue=inside / white=surface /
red=outside, symmetric range from the 99th percentile of |value| so one huge
MeshSource-outside value can't flatten the band, zero-isocontour burned in) via
vendored `stb_image_write`, and an **SVG** of that contour via `marchingSquares­Zero`
— a pure 16-case marching-squares pass (saddles resolved by the 4-corner average
sign) kept deliberately separable as the **seed of step 4's per-layer slicing**.
PNG row-0 = max-v and the SVG Y-flip keep the two outputs in the same world-up
orientation. **Verified:** valid 256×256 PNG; well-formed SVG with segment count
matching the contour; bunny + cube runs; `cli_slice` ctest green.

---

← Back to the [Roadmap index](README.md).
