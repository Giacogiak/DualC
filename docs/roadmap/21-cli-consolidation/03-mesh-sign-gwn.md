# CLI consolidation — `mesh(sign="gwn")`, measured

Part of [CLI consolidation](README.md) (block 21): the evidence behind
[#56](01-record.md#56-the-mesh-node-takes-signgwn), landed 2026-10-15 by the plan-run unit
P02.1 on branch `feat/cli-consolidation`. The item's record holds the summary; this page holds
the numbers, so the record stays under the cap.

## What changed

`meshSignMethod` (`examples/field_graph.cpp`) maps `gwn` to
`SignMethod::GENERALIZED_WINDING_NUMBER` instead of throwing; `emitMesh`
(`examples/field_glsl.cpp`) maps it the same way, so the GPU preview bakes the node through
`MeshSource::bakeToGrid` with the GWN oracle inside the bake. An unknown value now reads
`mesh sign must be 'parity', 'pseudonormal' or 'gwn'`, located on the `mesh` node. The GLSL
dedup memo already keyed on every parameter (`nodeKey`), so two `mesh` nodes that differ only
in `sign` bake two textures — now a unit test. `--list` names the three values.

## The equivalence, byte-identical

Linux, GCC 15, Ninja, Release, `build/examples/`, `--depth 6`, the demo meshes as generated
after #55 (wound outward). `cmp` on the written OBJ; one SHA-256 for all four torus files.

| Run | Faces | Peak RSS | Wall |
| --- | --- | --- | --- |
| `dualc_demo --gwn --depth 6 torus.obj` | 40,032 | 111 MB | 1.32 s |
| `dualc_field --expr 'mesh(path="torus.obj",sign="gwn")' --depth 6` | 40,032 | 112 MB | 1.35 s |
| the same graph through `--dump-json` → JSON file → `dualc_field` | 40,032 | — | — |

All three are byte-identical (`57d29ef4…`) — and identical to parity's output by either tool,
as they must be on a closed, outward mesh: GWN and parity agree at every corner the sampler
asks about. So that the equality is not parity in disguise, the same pair on
`data/mesh-soup.obj` (31,718 input faces, a soup; local, not in CI), where the oracles disagree:

| Sign | `dualc_demo` faces | `dualc_field` faces | `cmp` |
| --- | --- | --- | --- |
| `gwn` | 30,120 | 30,120 | identical |
| `parity` | 33,610 | 33,610 | identical |

## Tests and the parity case

- *mesh sign=gwn is the GWN sign oracle on a metric field* (`tests/test_field_graph.cpp`,
  `[gwn]`): on a closed box, `sign=gwn` and `sign=parity` contour to the same face count and
  the same vertex positions at depth 5; on the box without its `+z` face, `mesh(sign=gwn)`
  reads −1 at the centre (the distance to the walls) where `winding` reads ≈ −1/3
  (0.5 − 5/6, the solid angle the shell subtends) — the sign oracle, not the sealing field.
  Far outside (5, 0, 0) `mesh` reads the distance 4, `winding` 0.5025 (the hierarchical GWN's
  far-field error, hence a 0.05 margin).
- The old assertion *gwn on `mesh` throws* in *field-graph reports located errors* was the
  guard this item removes; it is replaced by an unknown value (`bogus`) under a `union`, which
  must name all three values and carry the pointer `/in/1`.
- `tests/test_field_glsl.cpp`: *same mesh path, parity vs gwn bakes two textures*.
- Parity case `mesh-gwn` (`examples/dualc_glsl_parity.cpp`): the straddle region of
  `mesh-straddle` with `sign="gwn"`, sign-only against the CPU field.
  `parity_expected_cases` 73 → 74. On the closed cube GWN and parity agree, so the case
  gates that the codegen accepts the value and carries it into the bake, not that the two
  oracles differ.

## The ABI version

No move: `dualc 0.5.1` stays. The rule block 14 applied to the last vocabulary change
([14/04, 2026-09-11](../14-c-abi/04-abi-0-4-0.md#unknown-parameter-keys-are-rejected-behaviour-change-no-abi-break)):
the create calls build through the same `buildField`, and only the graph's vocabulary moved —
signatures, structs and codes did not. Recorded in
[14/04](../14-c-abi/04-abi-0-4-0.md#meshsigngwn-is-accepted-vocabulary-widening-no-abi-version-move).

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
