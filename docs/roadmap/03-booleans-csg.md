# Booleans & CSG

Hard and smooth boolean combinators (`dualc_boolean`) and the baked field-tree
composition recipes (`dualc_csg_demo`). These build on the
[implicit/SDF foundation](02-implicit-sdf-foundation.md); the full per-command
flag/recipe catalogue lives in
[../command_reference/03-dualc_boolean.md](../command_reference/03-dualc_boolean.md)
and [../command_reference/05-dualc_csg_demo.md](../command_reference/05-dualc_csg_demo.md).
The `dualc_csg_demo` CLI itself landed as part of the v2
[Phase 6 milestone](02-implicit-sdf-foundation.md#phase-6--cli--demo-exposure--done).

> **Open items found 2026-06-12** (Phase 0 benchmark, see
> [11](11-dense-lattice-deliverable/02-phase-0-benchmark.md#phase-0-benchmark) /
> [12](12-field-graph-and-app/README.md)):
> 1. **Export bug:** `dualc_boolean` ignores the `-o` extension and always writes
>    OBJ — confirmed for both `.stl` and `.3mf` (vs. `dualc_lattice`, which writes
>    binary STL). Wire it to the `example_common` extension dispatch.
> 2. **Mesh-boolean over a dense lattice is catastrophic** (10–49 min for one op,
>    re-deriving an SDF from millions of thin-wall triangles). The general
>    `dualc_field` (field-level composition) supersedes the two-mesh boolean for the
>    lattice workflow — `dualc_boolean` stays as a convenience shortcut for plain
>    mesh-on-mesh CSG. **`dualc_field` is now implemented** (JSON core, 2026-06-14;
>    [command_reference/11](../command_reference/11-dualc_field/README.md),
>    [12 §C](12-field-graph-and-app/README.md)): the lattice-carve composes as a field and
>    contours once. (The `dualc_boolean` export bug in item 1 is still open.)
>    *Resolved 2026-06-12, commit `6ae7dc9` — every field CLI honours the `-o`
>    extension; recorded in [09](09-io-formats.md).* *(2026-09-18: the "still open"
>    sentence was written on 2026-06-14 (`e84dbae`), two days after that fix — stale
>    when written; the resolution line, appended 2026-09-11, is the fact.)*

## Phase 2 — Combinators + non-domain decorators — DONE

Delivered:
- `Mat4` in `types.h` — header-only rigid 4×4 transform.
- `include/dualc/implicit.h` — `FieldPtr` typedef + builder declarations.
- `src/implicit/combinators.cpp` — seven combinators:
  - Hard: `unionOf` (min), `intersectionOf` (max), `differenceOf`
    (max(a,-b)), `xorOf`. Each overrides `gradientAt` to the active operand
    for sharp seams; `isInside` is the boolean fast-path; `cellOverlaps` is
    OR-of-children.
  - Smooth: `smoothUnionOf`, `smoothIntersectionOf`, `smoothDifferenceOf` —
    Quilez polynomial smin/smax; gradient is the `h`-weighted blend.
- `src/implicit/decorators.cpp` — `offsetOf`, `roundedOf` (alias of
  `offsetOf`), `onionOf`, `elongated`, `transformed`, `scaled`. Exact
  analytic `gradientAt` and `bounds` for each.

> The decorators delivered in this phase (`offsetOf` / `roundedOf` / `onionOf` /
> `elongated` / `transformed` / `scaled`) are the "operators that edit a
> primitive" — see [04-primitives-and-operators.md](04-primitives-and-operators.md).

Combinator/decorator classes are hidden in the `.cpp` files; only the
`FieldPtr`-returning builders are public.

Verified: 31 tests pass (19 prior + 12 new); molde regression still
byte-identical.

---

← Back to the [Roadmap index](README.md).
