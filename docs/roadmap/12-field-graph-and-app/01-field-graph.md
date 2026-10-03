# Field-graph (§ A) and the `dualc_field` CLI (§ C)

Part of [12 — Field-graph & standalone raymarch app](README.md). The keystone: the
composable field description, its locked serialization conventions and the CLI that
contours it. Usage:
[command_reference/11](../../command_reference/11-dualc_field/README.md).

## A. The field-graph (keystone)
**DONE (2026-06-14)** — JSON node set + text shorthand; the DAG-ref superset is DEFERRED
(Boletus-driven).


A composable, immutable description of a field — the single source of truth that
(a) dual-contours for export, (b) compiles to GLSL for live raymarch, (c) crosses
the IPC boundary to the Rhino side-car, (d) is later wrapped by the C ABI.

**Node set.** Core sources `MeshSource` (watertight volumes), `WindingNumberField`
(soups — mesh-soup mixing is a core requirement, not an edge case), TPMS (+ struts
later, [05](../05-tpms-lattices/README.md) #17), joined by the 7 booleans, with
`onion`/`normalize`/`transform`. The ~30 analytic primitives + remaining
decorators/domain-ops ride along because they are nearly free (analytic, already in
the `example_common` registry). Deferred: `GridField` as an author node (it stays
the internal bake target / raymarch texture for mesh inputs), the 2D-primitive +
revolve/extrude sub-grammar, and `displaced` with an arbitrary lambda (the graph
admits only the named sine/gyroid/bumps form). *(2026-09-18: the triggers for these three
omissions and for the DAG-ref superset are rows D-11 and D-12 of the
[decisions index](../../decisions/README.md).)*

**Semantics that matter.** Hard booleans need only the sign ⇒ always correct on the
non-metric inputs (`WindingNumberField`, raw TPMS) — the safe core for soup-mixing.
Smooth booleans assume metric distance ⇒ require a `normalize` wrap on those inputs.
Mesh inputs bake to a low-res 3D texture for *preview* only; export is always exact.

**Serialization: JSON canonical + terse text shorthand.** Op tokens == the
`example_common` registry tokens (one vocabulary). Text shorthand is uniform
prefix-call `op(child, …, key=value)` with underscore aliases for the hyphenated
tokens. The wire format is **tree-only** in v1, but the engine is **always a
deduped DAG** (a duplicated `mesh(...)` bakes once; a shared subexpression compiles
to one GLSL variable). DAG ref syntax (`id`/`ref`, `let … in …`) is a deferred,
no-rework superset added when Grasshopper (itself a node DAG) is wired up. Sampling
settings (`depth`/`collapse`/`bounds`) are invocation flags, **not** graph nodes.

## C. `dualc_field` CLI
**DONE (2026-06-14)** — JSON core + shorthand / `--dump-json` / `--list`.


**Implemented (JSON core):** `examples/field_graph.{h,cpp}` (lib
`dualc_examples_fieldgraph`, vendoring nlohmann/json MIT under `third_party/`)
parses the canonical JSON into a `GraphNode` tree and lowers it to a `FieldPtr`
purely by wiring the `example_common` registries (`buildPrimitive`/`makeTpmsField`/
`applyBoolOp`/`applyPostOps`) + `implicit.h` factories (`transformed`/
`normalizedOf`/`windingNumberField`) — no new field maths. A pluggable, caching
`MeshResolver` (path-based `FileMeshResolver` now; in-memory later for the C
ABI/GH) owns the loaded meshes for the field's lifetime. `dualc_field` takes a
JSON file or stdin (`-`), the shared `--depth/--collapse/--bounds/--no-manifold`
flags, and `-o` extension dispatch; node-located `GraphError`s carry a JSON
pointer. The doc-12 build-check is resolved: `MeshSource` has no GWN ctor, so
`mesh.sign` is `parity`/`pseudonormal` and `gwn` routes through the `winding`
node (enforced with a hint). Tested by `tests/test_field_graph.cpp` + three
`cli_field*` smoke tests; two sample graphs ship in `examples/samples/`. Full
page: [command_reference/11](../../command_reference/11-dualc_field/README.md).
**Fast-follow — DONE 2026-06-14:** the terse `--expr` text shorthand
(`parseShorthand` in `field_graph.cpp` — uniform prefix-call, underscore op
aliases, named/positional params, parses to the SAME `GraphNode` tree so the
builder is untouched), `--dump-json` (`dumpJson`, the canonical inverse — a
round-trip / Grasshopper aid), and `--list` (op vocabulary from the
`example_common` registries). Input is content-sniffed (`.fld` ⇒ shorthand, else
`{`⇒JSON). Only the DAG-ref syntax (`id`/`ref`, `let … in …`) of the
serialization superset remains deferred (lands with Grasshopper).

Original design (for reference): parse field-graph → build the `FieldPtr` tree →
dual-contour once → export. This is
what makes booleans-over-lattice **one field-level contour** (no mesh round-trip),
delivering pillar #3. Inputs: file (`.json`/`.fld`), `--expr`, or stdin (for GH/
pipes). Output via the existing `-o` extension dispatch. The parser/builder lives in
a new host-side `examples/field_graph.{h,cpp}` (it touches mesh I/O + registry
tokens, so it stays out of `libdualc`), with a **pluggable mesh-source resolver**
(path-based now, in-memory arrays later for the C ABI / GH) — so the side-car,
viewer, and C ABI reuse the same parser verbatim. Reuses the `example_common`
registries + `implicit.h` factories (no new field code, just wiring). Node-located
diagnostics (unknown op, missing required param, wrong boolean arity,
mesh-not-found, infinite-field-without-bounds, open-source-without-onion). Helpers:
`--dump-json` (shorthand→canonical), `--list`. Generalizes but does **not** replace
`dualc_lattice`/`dualc_boolean`/`dualc_csg_demo` (they remain shortcuts).

## Appendix — field-graph schema (locked reference)

The complete pinned schema (locked 2026-06-12), so the design is reproducible
in-repo without the working plan.

### The 7 serialization conventions

1. **Tokens.** JSON keeps the exact hyphenated `example_common` registry tokens
   (`schwarz-p`, `fischer-koch`, `smooth-union`); the text shorthand uses underscore
   aliases (`schwarz_p`, `smooth_union`) the parser maps back.
2. **Vectors** are grouped semantic keys (`center`/`radius`/`min`/`max`/`a`/`b`/
   `period`/`count`), not flat scalars.
3. **Placement:** friendly `translate`/`rotate`/`scale` nodes **and** a raw
   `transform` matrix node (for GH) — all lower to `transformed`/`scaled`.
4. **Units:** `rotate` takes `degrees`; `twist`/`bend` keep rate params
   (`radiansPerUnit`/`curvature`); `axis` is a `"x"|"y"|"z"` string.
5. **Mesh sign:** `mesh.sign` ∈ `parity`(def)/`pseudonormal`/`gwn`, `mesh.normals` ∈
   `smooth`(def)/`sharp`; `winding` takes only `path`.
6. **Defaults:** every param defaults to its registry value (partial nodes valid —
   `{"op":"sphere"}` = unit sphere at origin).
7. **Aliases:** `offset` and `round` are kept as aliases (both → `offsetOf`).

### Document & node shape

- **Header:** `{ "version": 1, "units": "mm", "root": <node> }`.
- **Node:** `{ "op": <token>, <params…>, "in": [<children>] }` — leaf sources have
  no `in`, decorators/domain-ops one child, booleans two.
- **Sampling/contour settings (`depth`, `collapse`, root `bounds`) are CLI flags on
  the invocation, NOT graph nodes** — the graph is pure field description.
- **Text shorthand** is uniform prefix-call `op(child1, child2, key=value, …)`
  (children positional, params `key=value`; positional scalars allowed in registry
  order). No infix symbols.

### Node tables (`param : default`)

The per-node parameter tables that stood here were a copy of the usage
reference and drifted from it; removed 2026-09-11. The single home is
[command_reference/11 § Op
vocabulary](../../command_reference/11-dualc_field/01-op-vocabulary.md#op-vocabulary)
(sources, booleans, decorators, graded, domain ops, primitive params) and, for
the GPU side, [command_reference/12](../../command_reference/12-dualc_field_view/README.md).

### GLSL codegen — fixed vs. generated

| Fixed (write once) | Generated per graph |
|---|---|
| Sphere-trace loop, camera, FD normals, shading | `float sceneSDF(vec3 p)` (per-node funcs + root) |
| GLSL prelude (primitive/TPMS/op library) | uniform declarations + binding table |
| Texture-sampling helper | one `sampler3D` per unique mesh node |
| `uStepScale` plumbing | the graph-level step-factor value |

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
