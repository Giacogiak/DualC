# P04 — Slice any graph: `dualc_field --slice`

§ 2 (`dualc_slice`) and § 3 P04 of
[the raw plan](../docs/raw/2026-10-14-cli-consolidation-plan.md). The form is the plan's
choice under the owner's "either"; if the owner overrode it at the P01 approve gate (see the
P01 handoff and the decision row), implement the recorded form instead. Branch
`feat/cli-consolidation`.

### T1. A field-agnostic slice module and `dualc_field --slice AXIS=VALUE`

1. Move the slice core of `examples/dualc_slice.cpp` (plane parse, sampling grid, symmetric
   colormap PNG via `stb_image_write`, marching-squares SVG) into
   `examples/field_slice.{h,cpp}`, taking any `dualc::ImplicitField` and a region. `dualc_slice`
   calls it until P06 deletes it, so both outputs can be compared.
2. `dualc_field <graph> --slice AXIS=VALUE [--res N] [-o base]`: writes `base.png` + `base.svg`
   (a `.png` / `.svg` extension on `-o` is stripped to the base), builds the graph, contours
   nothing. Region: `--bounds`, else the graph's bounds (unbounded graph → the existing error).
   Default `--res` = `dualc_slice`'s (512, max 4096). Refuse with a clear message when combined
   with `--tile-depth`, `--mem`, `--weld`, `--decimate`, `--simplify`. The contour-only flags
   (`--depth`, `--collapse`, `--no-manifold`) are refused too, or ignored with a warning —
   decide, record why, and list the behaviour on the page.
3. Equivalence, byte-identical PNG and SVG, recorded in block 21: `dualc_slice cube.obj --type
   gyroid --wavelength 0.5 --offset 0.1 --plane z=0 --res 128` against the graph
   `intersection(onion(gyroid(center=<the cube's AABB centre>,wavelength=0.5),thickness=0.1),mesh(path="cube.obj"))`
   with the same bounds (`dualc_slice` pads the AABB by 5 %; pass that box as `--bounds`).
4. ctest `cli_field_slice` with the content checks `cli_slice` has.
5. Docs: a new child page under `docs/command_reference/11-dualc_field/` (flag table with real
   defaults, recipes — a TPMS cross-section, a mesh-shell wall check, a lattice-in-mesh layer),
   its row in the 11 README and in `command_reference/README.md`'s feature map; `STRUCTURE.md`
   row for `field_slice`.

#### Acceptance criteria

- Full gate green (the flag-table check holds `--slice` and `--res` to the page); the
  equivalence byte-identical; `python3 plan/remote_run_check.py` exits 0; item DONE in block 21.

## Exit criteria

- Any graph can be sliced from `dualc_field`; `dualc_slice` is reproduced byte-identically.
