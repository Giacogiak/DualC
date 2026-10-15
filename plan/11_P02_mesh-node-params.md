# P02 — Mesh-node parameters: `sign="gwn"` and `bake`

§ 2 (the `sign` and `--bake` facts) and § 3 P02 of
[the raw plan](../docs/raw/2026-10-14-cli-consolidation-plan.md). Branch
`feat/cli-consolidation`. Both tasks touch a field node: `--gpu` is part of the gate.

### T1. `mesh(…, sign="gwn")`

1. `examples/field_graph.cpp` `meshSignMethod`: map `gwn` →
   `dualc::SignMethod::GENERALIZED_WINDING_NUMBER` instead of throwing; the error for an unknown
   value lists all three. `--list` text, `--expr` shorthand and `--dump-json` carry it.
2. GLSL: confirm `emitMesh`'s bake goes through `MeshSource` with the node's sign (the dedup memo
   must key on `sign` so two mesh nodes differing only in sign do not share a texture). Add
   parity case `mesh-gwn`; bump `parity_expected_cases` in `scripts/check_data.json`.
3. Tests (`tests/test_field_graph.cpp`): `sign="gwn"` on a closed mesh gives parity's output;
   the bad-value error is located.
4. Equivalence, byte-identical, recorded in block 21:
   `dualc_demo --gwn --depth 6 torus.obj` ≡
   `dualc_field --expr 'mesh(path="torus.obj",sign="gwn")' --depth 6`.
5. Docs: 11/01 § Sources (the three signs and when to use `winding` instead — the sealing field,
   not a sign oracle); `docs/design/02-sign-oracles.md` if it names the graph's sign set; the C
   ABI pages if they list node parameters. Decide and record the ABI version move by the rule
   block 14 applied to the last vocabulary change.

#### Acceptance criteria

- Full gate and `python3 scripts/check.py --gpu --build-dir build-gl` green (count bumped).
- The equivalence above is byte-identical; the item is DONE in block 21.

### T2. Per-node `bake` on `mesh` and `winding`

1. `mesh(…, bake=N[, band=W])`, `winding(…, bake=N)`: the node returns
   `bakeToGrid(region, N³, band)` where region = the node's own AABB padded by 10 % of its
   largest extent; `band` defaults to 3 cells of the bake (mesh) and is ignored for `winding`
   (full-lattice bake, as `field_glsl.cpp` already does). `N` validated: ≥ 2 and a documented
   ceiling, with a located error.
   *Corrected by P02.2 (2026-10-15), on a measurement:* an explicit `band` also widens the
   region (pad = 10 % + `band`). Without it a smooth parent with a large `k` makes material
   out of the grid's clamped far field (smooth-union torus cube, k = 1: 3× the faces, max
   deviation 1.56); `dualc_boolean` pads by `k` for the same reason. A `band` on `winding` is
   rejected as an unknown key (the unread-key rule), not ignored. Evidence: roadmap 21/04.
2. GLSL: a baked node's own `GridField` goes straight to `emitBakedGrid`, so preview == export;
   `--grid-res` does not re-bake it. Parity case `mesh-bake`; count bumped.
3. Tests: near the surface the baked field is within one bake cell of the exact one; the
   round-trip through `--dump-json`; the validation errors.
4. Measure, do not equate — **these numbers decide whether `dualc_boolean` retires** (an open
   decision Giacomo takes at the approve gate after this unit). For each case run three ways:
   `dualc_boolean <op> A B --bake N`, the graph with `mesh(…,bake=N)` on both children, and the
   exact graph (no bake). Cases: `difference sphere.obj cube.obj` at `--depth 6`, `--bake 64`;
   `smooth-union` with a `k` near the model size; and the large pair `data/opA.obj` /
   `data/opB.obj` (local, not in CI) at the depth and `--bake` page 03 recommends. Report per
   run: wall time, peak RSS (`/usr/bin/time -v`), face count, and the deviation from the exact
   result (max and mean distance of output vertices to the exact field, `|valueAt|`). Write a
   table into block 21 and the handoff, with a one-line reading: does per-node `bake` keep the
   speed advantage, and how much accuracy does it lose against the tool's combined-region bake.
   Do not delete or deprecate `dualc_boolean` here.
5. Docs: 11/01 § Sources (the speed / exactness trade, `band` ≥ the parent's `k` for smooth
   ops, why `dualc_boolean`'s combined region is not reproduced); 12's `--grid-res` text.

#### Acceptance criteria

- Full gate and `--gpu` green; the measurement table recorded in block 21 and in the handoff,
  which ends by asking Giacomo to decide `dualc_boolean`'s retirement.

## Exit criteria

- Both parameters shipped CPU + GPU with parity cases; `parity_expected_cases` matches.
- `python3 scripts/check.py` and `--gpu --build-dir build-gl` green; `python3
  plan/remote_run_check.py` exits 0.
