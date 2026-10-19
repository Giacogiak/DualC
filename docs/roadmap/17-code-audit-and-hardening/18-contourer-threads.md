# #34 — the contourer's thread count, through the C ABI, `dualc_field` and the bakes

The delivery record of the thread bullet of
[04 #34](04-engineering-quality.md#34-api-hygiene-batch), unit P02 of the
[hygiene + editor run](../../raw/2026-10-19-hygiene-and-editor-plan.md), on branch
`feat/hygiene-and-editor`, 2026-10-19. Every claim was checked against `e19ac50` before it
was scoped.

## What was wrong

- `ContourerParams` had no thread count: the QEF pre-solve in `contourHermiteOctree` called
  `resolveThreadCount(0)`, so a caller restricting `SamplerParams::numThreads` still got
  every hardware thread in the contourer.
- The two bakes were hardcoded the same way: `bakeToGrid` (one z-slab loop) and
  `MeshSource::bakeToGrid` (the band-detection loop and the band-SDF loop).
- The C ABI's `DualcContourParams.numThreads` reached the sampler only: `applyParams`
  never set the `ContourerParams` it was handed.
- `dualc_field` had no flag for any of it.

## What shipped

- **`ContourerParams::numThreads`**, default 0 = all hardware threads, worded as the
  sampler's field; both pre-solve call sites read it, and the #34 comment is gone.
- **The bakes** take a trailing `unsigned numThreads = 0`: `bakeToGrid(src, region,
  resolution, numThreads)` (both overloads) and `MeshSource::bakeToGrid(region, resolution,
  bandWidth, numThreads)`. Source-compatible, no global state.
- **The host layer** passes a count down: `FieldGraph::build(root, meshes, bakeThreads = 0)`
  to every `bake=N` node, `compileToGlsl(…, gridRes, bakeThreads = 0)` to the viewer's
  texture bakes.
- **The C ABI** copies `numThreads` into `ContourerParams` too; the header comment says so,
  and the version is **0.5.2** — a patch, because the field already existed and is now
  honoured (raw plan § 3). Record: [14/04](../14-c-abi/04-abi-0-4-0.md).
- **`dualc_field --threads N`** (default 0; a whole number up to 1024, parsed by
  `dce::parseThreadCount`) feeds the sampler, the contourer and the per-node bakes; with
  `--slice` it feeds the bakes, the only parallel part of that path.
  **`dualc_field_view --threads N`** feeds its bakes. Pages:
  [11](../../command_reference/11-dualc_field/README.md#options),
  [12](../../command_reference/12-dualc_field_view/README.md#cli-flags).

**Chosen in-run (pending owner review):** the CLI cap of 1024 (a typo such as `-1` is
refused instead of spawning one thread per work item); and the C ABI's create calls still
bake a graph's `bake=N` nodes on all hardware threads — they take no parameters, and
`DualcContourParams` arrives only at contour time. Honouring it there needs a new create
twin, an ABI minor rather than this patch, so it is left for a host that asks.

## Evidence

- **Tests**, all in the default gate except the last: `The contourer is bit-identical
  regardless of its numThreads` (one octree, contoured at 1, 2, 0 and 64 threads, extends
  [#30](04-engineering-quality.md#30-threading-determinism-test)); `ContourerParams::numThreads = 1 runs the leaf
  solves serially` — the proof the count arrives: the serial path polls after every
  `max(1, n/64)` leaves, the threaded one on a 100 ms timer, so the Contour stage's reports
  are the exact 63-multiple sequence only at one thread; `bakeToGrid and
  MeshSource::bakeToGrid are bit-identical regardless of their thread count`;
  `cli_field_threads` + `cli_field_threads_same` (the baked boolean at `--threads 1`, the
  same bytes as `cli_field_boolean_bake` at all threads) and `cli_field_threads_bad`;
  `cli_c_abi_threads` (needs `-DDUALC_BUILD_C_ABI=ON`).
- **Proven red:** with the pre-solve put back on `resolveThreadCount(0)` the serial-path
  case fails; with the `applyParams` line removed `cli_c_abi_threads` fails (0 of 63
  serial-cadence polls). Both green again once restored.
- **`cli_c_abi_threads`**, Linux GCC Release: 262,144 leaves, 63 serial-cadence polls of 63,
  0 other; 1, 2 and 0 threads give 101,476 vertices and 163,740 triangles, byte-identical.
- **Timing**, i7-920 (4 cores, 8 threads), Linux GCC 15 Release, `build/examples/`:
  `dualc_field gyroid_box.json --depth 7 -o x.obj` takes **41.7 s at `--threads 1`** and
  **22.6 s at the default** (two runs each, within 0.2 s), peak RSS 3.39 GB both, the OBJ
  byte-identical; a baked graph, `smooth-union(mesh(path="torus.obj",bake=192),
  winding(path="cube.obj",bake=128),k=0.2)` at depth 7, **45.1 s vs 11.2 s** (one run
  each), byte-identical.
- **Viewer:** `dualc_field_view --snapshot` of a graph with a mesh and a baked winding node
  wrote the same PNG bytes at `--threads 1` and the default; GPU parity 82/82
  (`check.py --gpu --strict --build-dir build-gl`).

---

← Back to the [block 17 index](README.md) · the ledger entry:
[04 #34](04-engineering-quality.md#34-api-hygiene-batch)
