# P06 — Retire the eight tools

§ 2 (what references them) and § 3 P06 of
[the raw plan](../docs/raw/2026-10-14-cli-consolidation-plan.md). The tools:
`dualc_demo`, `dualc_primitive`, `dualc_boolean`, `dualc_lift`, `dualc_csg_demo`,
`dualc_lattice`, `dualc_slice`, `dualc_raymarch`. Branch `feat/cli-consolidation`.

### T1. Delete the code, keep every tested path tested

1. Delete the eight `examples/dualc_*.cpp` and their targets in `examples/CMakeLists.txt`;
   remove `DUALC_BUILD_RAYMARCH_VIEWER` from `CMakeLists.txt` and its arm of the GLFW
   condition; `raymarch_gl.{h,cpp}` stays (field_view and the parity harness use it).
2. Remove code only they used — `csgRecipes` / `buildRecipe` in `example_common`, anything
   else a build without them reports unused. The primitive / post-op / boolean registries stay:
   they are the graph's vocabulary.
3. Every retired `cli_*` test gets a `dualc_field` twin on the same path — mesh-in smooth,
   mesh-in sharp, mesh-in GWN sign, the soup through `winding`, primitive + post-ops, an infinite
   primitive with `--bounds`, mesh boolean, mesh boolean with `bake`, revolve, extrude, lattice
   to OBJ / STL / 3MF — and the content list fed to `check_cli_output.cmake` follows. Report the
   ctest count before and after.
4. `scripts/check.py`: drop the flag-table special case for `dualc_primitive`'s registry flags
   (the field page must already cover them); any other reference.
5. CI: `.github/workflows/*.yml` and `check.py` must not configure the removed option.
   Re-configure `build-gl` without it.

#### Acceptance criteria

- `python3 scripts/check.py --clean` green (a clean configure proves no stale option),
  `--gpu --build-dir build-gl` green; `python3 plan/remote_run_check.py` exits 0.

### T2. Tombstones, indexes, the loss audit

1. Pages 01–07 and 10 become tombstones in `08-dualc_view.md`'s form: what the tool was, the
   date and decision that retired it, where its content lives now. The numbers are never reused.
2. `docs/command_reference/README.md`: the tool index, the build prelude's opt-in list, the
   keyboard map (one viewer now), the common-options matrix, the export-format list, the
   appendix inventory. `README.md`, `STRUCTURE.md`, `AGENTS.md` (the opt-in option list),
   `docs/design/09-conventions.md` and any other `design/` mention; `scripts/docs_loss_audit.json`
   if it names the pages. Roadmap records keep naming the tools — history is not rewritten.
3. **Before committing the page rewrites, run `scripts/docs_loss_audit.py` against the branch
   base** (scratch triage, `--scope docs/`) and triage every residual into its P05 home or a
   recorded drop. Its report goes to `docs/raw/` as a dated file, indexed.

#### Acceptance criteria

- `python3 scripts/check.py --docs --strict` and the full gate green; zero untriaged lines in the
  loss audit; the retirement item DONE in block 21 with the ctest before/after.

## Exit criteria

- No retired binary, target, option or live doc reference remains; every path they covered is
  tested through `dualc_field`; remote run green.
