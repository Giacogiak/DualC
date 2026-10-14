# P01 — Open the record, fix the inside-out demo meshes

Sections 1–2 and § 3 P01 of [the raw plan](../docs/raw/2026-10-14-cli-consolidation-plan.md);
evidence in [the screening](../docs/raw/2026-10-14-cli-tools-screening.md). Branch
`feat/cli-consolidation` (created from `main` with this plan on it); push it, never `main`.

### T1. Open roadmap block 21 with its tracked items and the owner's decisions

1. Work on `feat/cli-consolidation`. Read `docs/decisions/README.md` and `01-settled.md` for the last `D-NN`
   and the roadmap for the last `#N` (on 2026-10-14: D-54, #54, next block 21) — never trust
   the plan's numbers over the files.
2. `docs/roadmap/21-cli-consolidation/README.md` (folder from the start, index + status table)
   and a record child. One tracked item per precondition: demo-mesh orientation (F1), `mesh`
   GWN sign, mesh-node `bake`, 2D profile + lift nodes, graph slice, retirement of the eight
   tools (docs migration + removal). Each with its acceptance line from the raw plan.
3. Decision rows for the owner's 2026-10-14 decisions (raw plan § 1), in the index's own style,
   citing block 21: the retirements and their preconditions — **except `dualc_boolean`, whose
   retirement is OPEN**: Giacomo decides it at the approve gate after P02, on P02 T2's
   measurements (his words, 2026-10-14: "I wait P02 measures before to decide whether to remove
   dualc_boolean"); record it as an open decision row naming that gate; F2 not fixed because
   `dualc_demo` retires and `dualc_field -o` already chooses the format; the slice form
   `dualc_field --slice` (the owner allowed either form; flag it in the handoff for
   confirmation at the approve gate). The D-11 row: trigger fired for the 2D + lift sub-grammar
   only; `GridField` author node and lambda `displace` stay DEFERRED.
4. A dated note in block 21 that roadmap 12/01's "`MeshSource` has no GWN ctor" no longer holds
   (12/01 is history and is not rewritten).
5. `docs/roadmap/README.md` § Current focus / § Next up name block 21; the block index row;
   `docs/raw/README.md` already indexes the plan and the screening — check it.

#### Acceptance criteria

- Block 21 exists with its items; the decisions index has the new rows and the edited D-11 row.
- `python3 scripts/check.py --docs --strict` green; the branch is pushed.
- The handoff lists every new ID and asks the owner to confirm the `--slice` form.

### T2. Fix the winding of the six inside-out generated meshes (F1)

1. `examples/demo_meshes.cpp`: reverse the triangle order of `torus`, `knot`, `cylinder`,
   `bracket`, `hexbore`, `uvsphere` so every shape winds outward (positive signed volume).
2. `tests/test_demo_meshes.cpp`: every generated shape has positive signed volume, and the
   generalized winding number at an interior point is ≈ 1 (a point inside the tube for the torus
   and knot).
3. Rebuild, regenerate `build/data/`; verify on the CLI: `dualc_demo --gwn-field torus.obj` and
   `knot.obj` produce geometry (they gave the placeholder triangle), `dualc_demo --gwn torus.obj`
   too, and `dualc_demo --pseudonormal bracket.obj` matches parity's face count (34,984 at
   `--depth 6`). Record before/after numbers.
4. Smooth Hermite normals come from the input, so output `vn` on these shapes flips outward:
   check `examples/check_cli_output.cmake` and every count or normal a test or a page records.
5. Page 09 (`dualc_gen_demo`) states that every generated mesh is closed and wound outward;
   the GWN / pseudonormal sections of page 01 and 11/01 if they warned around it.

#### Acceptance criteria

- The new test fails on the old winding and passes on the new (show both).
- `python3 scripts/check.py` green; the branch pushed and `python3 plan/remote_run_check.py`
  exits 0.
- The F1 item in block 21 is DONE with the measured before/after.

## Exit criteria

- Block 21, its items and the decisions are on the branch, `--docs --strict` green.
- F1 fixed and tested; full gate green; remote run green.
