# Slice any graph — `dualc_field --slice` (#59)

The evidence page of [#59](01-record.md#59-slice-any-graph), plan P04 T1, landed 2026-10-16
on branch `feat/cli-consolidation`. The form is D-58's, confirmed at the P01 gate. The usage
contract is [11/10](../../command_reference/11-dualc_field/10-slice.md).

## What landed

- **`examples/field_slice.{h,cpp}`** (lib `dualc_examples_slice`): `dualc_slice`'s core moved
  out unchanged in what it writes — the sampling grid, the 99th-percentile symmetric colormap,
  the PNG through `stb_image_write`, marching squares and the SVG — behind one call,
  `writeFieldSlice(field, region, axis, pos, res, png, svg, tag)`, which takes any
  `dualc::ImplicitField` and a box. Beside it the plane parser, the `--res` default and clamp
  (`kSliceDefaultRes` 512, 2 … 4096) and the two `-o` rules (`withExt` for `dualc_slice`,
  `stripSliceExt` for `dualc_field`). The slice lib is now the only CLI code linking the PNG
  writer; `dualc_slice` and `dualc_field` both link it.
- **`dualc_slice`** composes its TPMS-in-mesh field as before and calls `writeFieldSlice`; its
  usage line printed `5%%` (a `printf` escape in a stream) and now prints `5%`.
- **`dualc_field --slice AXIS=VALUE [--res N]`**: builds the graph, writes `BASE.png` +
  `BASE.svg`, contours nothing. Region: `--bounds`, else the graph's bounds padded by 5 % of
  their largest extent — `src/sampler.cpp`'s `padBBox`, so the slice covers the box a contour
  of the same graph auto-fits. Unbounded → *this field is unbounded -- pass --bounds …*,
  exit 1, the contour path's code; an empty-bounds graph is refused the same way.
  `-o` default `field_slice`; a trailing `.png` / `.svg` (any case) is stripped, any other
  path is the base as given.
- **Refusals**, before the graph is read, exit 2: every mesh flag — `--tile-depth`, `--mem`,
  `--weld`, `--decimate`, `--simplify` as the plan says, **and** `--depth`, `--collapse`,
  `--no-manifold` (the first given is named); `--res` without `--slice`; `--slice` with
  `--dump-json`.

## Why the contour-only flags are refused, not ignored

The plan left it open. Refused: a slice has no octree, so `--depth` / `--collapse` /
`--no-manifold` cannot act on it, and a flag accepted with a warning reads as having done
something — a `--depth 9` slice is the same bytes as a `--depth 5` one, and a warning scrolls
past in a script. One rule for all eight mesh flags is also one sentence on the page. The cost:
a command line built for a contour cannot be turned into a slice by appending `--slice` alone.

## The region, against `dualc_slice`'s

`dualc_slice` pads the mesh's AABB by 5 % **per axis**; `--slice` pads by 5 % of the
**largest** extent on every axis (the sampler's rule). On `cube.obj` they coincide — the S1
graph without `--bounds` writes the same bytes — and on any other shape the graph's region is
wider on the short axes. The equivalence passes `dualc_slice`'s box explicitly.

## Measured

Linux, GCC 15, Ninja, Release; `build/examples/`; compared with `cmp`.

- **The plan's equivalence** — `dualc_slice cube.obj --type gyroid --wavelength 0.5 --offset
  0.1 --plane z=0 --res 128` against `dualc_field --expr
  "intersection(onion(gyroid(center=[0,0,0],wavelength=0.5),thickness=0.1),mesh(path=\"cube.obj\"))"
  --slice z=0 --res 128 --bounds -0.55,-0.55,-0.55,0.55,0.55,0.55` (`cube.obj` spans
  [-0.5, 0.5]³: centre 0, box padded 0.05; `-0.5 - 0.05` is the double `-0.55`): **PNG and
  SVG byte-identical**, SHA-256 `743c40a3…db0a982` (PNG) and `61915091…a38efbf2` (SVG), 1432
  contour segments. Held by ctest `cli_field_slice_same_png` / `_svg`.
- **Two more, beyond the plan:** `--type schwarz-p --wavelength 0.4 --offset 0.05
  --normalize-thickness --plane x=0.1 --res 200` ≡
  `intersection(onion(normalize(schwarz_p(center=[0,0,0],wavelength=0.4)),thickness=0.05),mesh(…))`
  (2464 segments); and the empty plane `--plane y=0.7 --res 64` (default type and wavelength
  √3 / 10, outside the box) ≡ the bare gyroid at `wavelength=0.173205080756887719` — both
  byte-identical. The first one failed while the graph read `normalize(onion(…))`:
  `dualc_slice` normalises first.
- **`dualc_slice` unchanged by the move:** the three runs above, before and after the
  refactor, byte-identical PNG and SVG.
- **Recipes X1–X5** of 11/10, each run once (512² unless stated): X1 6127 segments, X2 5188,
  X3 (1024²) 14306, X4 the bytes above, X5 5792. X2 (the hexbore shell, `y=0`) and X3 (the
  bracket's gyroid layer) looked at: the shell's two walls closed round the hex and the bore;
  the lattice inside the L only.

## Tests

ctest `cli_field_slice` (the S1 graph, `-o cli_field_slice.png`, so the stripping is
exercised), its content checks `cli_field_slice_content` (PNG 128 × 128) and
`cli_field_slice_svg_content` (an SVG with `<line `) — `cli_slice`'s two checks —
`cli_field_slice_same_png` / `_svg` (`cmake -E compare_files` against `cli_slice`'s outputs),
and `cli_field_slice_refuse` (`--slice z=0 --depth 5` names `--depth`). P06 must give the two
`_same` tests a fixed expectation before it deletes `dualc_slice`, as for
`cli_field_lift_same`.

---

← Back to the [block README](README.md).
