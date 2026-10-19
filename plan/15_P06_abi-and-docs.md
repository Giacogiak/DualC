# P06 — C ABI and the documentation

§ 3 P06 of [the raw plan](../docs/raw/2026-10-19-porous-shell-plan.md). Branch
`feat/porous-shell`. Never push.

### T1. The ABI test and entry, the command reference, the design page

1. **ABI.**
   - Add a `cli_c_abi_porous` ctest in `capi/CMakeLists.txt` that drives `capi/dualc_c_demo.c`
     with `porous(mesh(id=…))` from an in-memory mesh, plus the expression form, and checks
     faces > 0 and watertightness. Extend the demo program minimally if it cannot express the
     case.
   - Add a cancel case if the demo supports one.
   - Add a 14/04 "vocabulary widening" entry, applying block 14's version rule as written and
     recording which rule applied.
   - Update `capi/README.md` if it lists the vocabulary.
2. **Command reference.**
   - A new child page of `docs/command_reference/11-dualc_field/` (the next free number): the
     parameter table with the real defaults (the gate's `flag-table` check compares it), the
     four recipes ready to run from `build/examples/`, and a cost guide (seeds ↔ spacing ↔
     depth ↔ time, from P04's measurements).
   - Rows in its README and in `01-op-vocabulary.md`.
   - The node in `12-dualc_field_view/02-nodes-and-examples.md`.
   - The feature map in `command_reference/README.md`.
3. **Design.** The node's invariants (Lipschitz-1, volume at build time only, determinism, the
   bin bound, the texture layout) go in `docs/design/12-field-graph.md`, or in a new design page
   if the size cap requires it.
4. `STRUCTURE.md` for every new file of the run; `docs/README.md` if a new folder appeared.

#### Acceptance criteria

- `ctest --test-dir build -R c_abi` is green with the new case.
- `python3 scripts/check.py --docs --strict` is green.

## Exit criteria

- A Boletus developer can use the node through the ABI from the docs alone.

← Back to the [plan](README.md).
