# Handoff — P04.1: PorousField and the porous graph token

Unit P04.1 of [`plan/13_P04_field-and-node.md`](../13_P04_field-and-node.md) § T1; session
of 2026-10-21, branch `feat/porous-shell` in `~/Documents/DualC-porous`. Nothing was pushed,
no remote was touched, and `main`, `feat/hygiene-and-editor` and `~/Documents/DualC` were
left alone.

**Commits:**

- `75990cc`: code, tests and the record.
- `5a7d74d`: the record page keeps what the graph checks; the defaults stay in the raw plan
  (this cleared the `usage-in-record` INFO).
- The commit that adds this handoff.

**The record:**

- [23/02 #65](../../docs/roadmap/23-porous-shell/02-record.md#65-the-porous-node-on-the-cpu)
  is now **PARTIAL**, with a dated T1 append.
- The contract, the choices and the numbers are in the new child page
  [23/05](../../docs/roadmap/23-porous-shell/05-porous-field.md).

This handoff points at those pages and adds what P04.2, P05 and P06 need.

## What changed, and why

| File | What |
| --- | --- |
| `include/dualc/porous.h` | `PorousParams { SeedParams seeds; cells; mode; smooth }`; `class PorousField : ImplicitField` (pimpl), with `stats()` and `params()` |
| `src/implicit/porous.cpp` (new) | The field. Its top comment is the Lipschitz-1 argument for the default `cellOverlaps` |
| `src/internal/porous_complex.{h,cpp}` | `porousDistanceGradient(c, p, smooth, &grad)`: the same value as `porousDistance`, bit for bit, plus its gradient. Inside, the three query functions take an optional `Vector3* grad`, and `Candidates::foldGradient` folds the `(1 − h/2, h/2)` weights |
| `examples/field_graph.cpp` | The `porous` token (`readPorousParams`, `choiceParam`, `wholeParam`). `childBakeMargin(porous, 0)` is the pad; for the control it is 0 |
| `examples/dualc_field.cpp` | The `Porous source` group of `--list` |
| `CMakeLists.txt` | `porous.cpp` added to the sources and to the `-ffp-contract=off` list |
| `tests/test_porous.cpp`, `tests/test_field_graph.cpp` | 10 `porous:` cases and one hidden bench (listed below) |
| docs | 23/05 (new); 23/02 #65 PARTIAL; the block README row; the roadmap index (three status phrases, and "one Delaunay tetrahedralization" shortened to "one Delaunay" to stay under the 15 KB cap: the page was at 15,325 bytes); italic P04 notes on two rows of the 23/01 defaults table; one `porous` row in `11-dualc_field/01-op-vocabulary.md`; `STRUCTURE.md` |

### The contract later units build on

- **`PorousField(volume, control*, PorousParams, cancel*)`** builds the seeds and the
  complex, then keeps only the packed complex and the volume's box. The volume and the
  control are never read again.
- **The value** is `porousDistance(complex, p, smooth)`, with the struts capped at
  `B − smooth` (choice 1). It is ≥ 0, and 0 on the complex.
- **The gradient.** `trueGradientAt` is analytic: one query, magnitude kept, zero where a
  clamp holds the value. `gradientAt` normalizes it, and returns `(0,0,1)` where it
  vanishes.
- **`bounds()`** = `volume.bounds()`. `cellOverlaps` is the default.
- **Errors:**
  - `std::invalid_argument("porous: …")` for a negative or non-finite `smooth`;
  - for struts, also for `smooth ≥ B`;
  - the seeds' and the complex's own errors pass through;
  - `Cancelled` from the token.
- **The token.** `porous(volume[, control], cells, mode, dist, spacing (req), spacing2, d0,
  d1, ramp, relax, seed, smooth, jitter)`, with the raw plan's defaults. The graph's own
  checks are the table in 23/05; every error is a located `GraphError`.

## The in-run choices

Each is **Chosen in-run (pending owner review)**, recorded in 23/05 § The in-run choices and
in the 02 #65 append:

1. **The field caps the struts at `B − smooth`, not at `B`.**
   - At k > 0, P03's `porousDistance` steps where a segment crosses `d = B` while the
     accumulator is within k of `B`.
   - Measured on 400k pairs 1e-5 apart (sphere seeds, k = 0.06, `B` = 0.241): the largest
     slope is 15.2 (Voronoi struts) and 1,076 (Delaunay struts) with the cap at `B`, and
     0.99998 with the cap at `B − k`.
   - The cap is in `PorousField`, so P03's query, its digests and its brute-force test are
     unchanged.
   - The price: far from every strut, the smoothed value reads `B − k`, not `B`.
2. **`smooth ≥ B` with `mode=struts` is an error.** At that value the field would be 0
   everywhere. The walls take any `smooth`.
3. **The gradient is analytic for all four cases**, through `porousDistanceGradient`. No
   central difference is used.
4. **`childBakeMargin(porous, 0)` = max(`spacing`, `spacing2`); for the control it is 0.**
   - The plan expected no entry, because the volume is never read below zero. It is read
     above zero, though, up to the pad, where a baked mesh's far field over-estimates.
   - The node's own value never reads the volume, so the ancestors' margin does not pass
     down.
5. **The domain-wrapper property test (`test_domain_ops.cpp`) does not apply.** The node
   reads no child at evaluation time, so the file is unchanged.
6. **`PorousField` is public, and the packed complex stays internal** (see P05 below).
7. **No cancel token reaches the graph build.** `FieldGraph::build` has no token parameter,
   and neither do the C ABI's `dualc_field_create_from_*` that call it. Only contour and
   export take one. The graph therefore passes `nullptr`, as the plan asked me to record.

## Measured, and under what

This Linux host, GCC 15, Release, one thread; two runs, the slower given. Bench:
`build/tests/dualc_tests "porous: the struts' ceiling and the raw query's slope"`. The
volume is the analytic unit sphere, `spacing` 0.05 (**23,344 seeds, 148,406 tets**),
`smooth` 0.015, and 200k queries:

| Case | Build | Value /s | Analytic gradient /s | Central difference /s |
| --- | --- | --- | --- | --- |
| Voronoi walls | 0.86 s | 1.28M | 1.18M | 277k |
| Voronoi struts | 0.96 s | 112k | 108k | 26k |
| Delaunay walls | 0.94 s | 3.44M | 3.33M | 1.04M |
| Delaunay struts | 0.86 s | 134k | 129k | 28k |

The analytic gradient runs at 3.2–4.6× the central difference's rate.

**Smoke contours** (8 threads; not a T1 criterion, recorded for T2):

- `intersection(sphere(radius=1), onion(porous(sphere(radius=1), spacing=0.25),
  thickness=0.02))`, `--depth 7`:
  - 5.6 s, 641,676 faces;
  - **14 non-manifold edges**: the walls are 0.04 thick, which is 2.5 cells.
- The same at `--depth 6`: 155,640 faces, 1,776 non-manifold edges.

**T2 must choose a thickness of several cells** (or a depth to match) to get a watertight
result.

## Checks run

These follow the rules for P01–P07: no full gate.

- `cmake --build build -j`: no warnings.
- `ctest --test-dir build -R 'porous|field_graph|domain' -j8`: **34/34 passed** (the T1
  acceptance command).
- The driver's smoke filter `ctest -R 'porous|field_graph|field_glsl'`: 30/30 passed.
- `ctest -R 'field-graph|GLSL codegen|childBakeMargin|bake|shorthand'`: 45/45 passed (the
  existing graph cases).
- `ctest -R 'cli_field|cli_c_abi'`: 68/68 passed.
- `dualc_field --list` shows the `Porous source` group.
- `python3 scripts/check.py --docs`: 27 OK + 1 INFO (`index-staleness`, the same 15 entries
  as before this unit), PASS. The pre-commit hook ran on both commits.
- No docs file was deleted or moved, so no loss audit was due.
- **Not run:** the full gate, `--docs --strict`, the GPU tier, MSVC or Apple Clang. These
  belong to P08 and CI.

## What I got wrong on the way

- **A local `g` (the gradient) shadowed the query's `g` (the bins).** The build caught it,
  and the local is now `gd`.
- **Commas in Catch2 test names** split the command-line filter. I renamed the cases
  without commas.
- **Two wrong expectations in my first tests:**
  - "no point is farther than one spacing from a wall" failed at the box corners outside
    the pad (0.131 > 0.12). It now covers only points inside the volume, with a bound of
    1.2 × the spacing (the study's largest empty sphere is 1.16).
  - "pores outnumber material" was wrong for 0.1-thick walls in cells about 0.5 wide,
    which are half material. The thickness is now 0.02, and the test only checks that
    both occur.
- **The record page first carried a key/default table.** That is usage, and the
  `usage-in-record` INFO flagged it, so the page now holds only what the graph checks.
  The defaults stay with the raw plan's signature until #67's command-reference page
  carries them.

## Plan corrections

None to the acceptance criteria. Two facts differ from the plan's text, and both are
recorded as in-run choices:

- `childBakeMargin` does get an entry (choice 4).
- The domain-wrapper test does not apply (choice 5).

## T1 acceptance

- **`ctest --test-dir build -R 'porous|field_graph|domain'` is green:** 34/34.
- **`dualc_field --list` shows the node:** yes.
- **The three properties of the raw plan § 3 P04 T1:**
  - Lipschitz-1, sampled: `porous: the field is 1-Lipschitz on random pairs`, four cases ×
    k ∈ {0, 0.06} × 20,000 pairs.
  - The smoothing bound, as P01.1 corrected it: `porous: smooth only lowers the value by
    < k and by k/4 at two primitives`. It checks k/4 at more than 50 two-primitive points
    per case and < k in the exact region.
  - Unknown keys rejected: `porous: unknown keys a missing spacing and bad values are
    located errors`.
- **Also covered by the plan's list:**
  - the round trip: `porous: the node builds from JSON and shorthand and round-trips`;
  - a mesh volume: `porous: a mesh volume is read at build time only`, and `porous: a mesh
    volume through the graph`;
  - the missing `spacing` error, in the error case above;
  - the domain-wrapper test: not applicable (choice 5).
- **Everything is committed and the tree is clean.**

## Open, and what the next units need

- **P04.2 (T2, the sponge end to end):**
  - Keep each wall several cells thick (see the smoke contours above).
  - `PorousField::stats()` gives seeds, pad seeds, tets, faces, edges, segments, bins and
    packed bytes for the record. `dualc_field` does not print them; reach them from the
    test, or add a stderr line if T2 wants one.
  - The test's component counter belongs in the test, not the library.
- **P05 (GLSL):**
  - **Emit `min(value, B − smooth)` for struts** (choice 1). Without it the GPU field steps
    where the CPU field does not, and parity fails in the far field.
  - The codegen in `examples/` cannot reach `internal::PorousComplex`. It needs a
    read-only public view of the packed arrays (a struct in `dualc/porous.h`, or an
    accessor on `PorousField`). That is P05's design call, and I did not pre-empt it.
  - `nodeFeatureScale` = `params().seeds.spacing` / `spacing2`, the smaller one.
  - If the analytic gradient is wanted in GLSL, `porousDistanceGradient` is the formula.
- **P06 (ABI + docs):**
  - The command-reference page for `porous` does not exist yet. I added one row in
    `01-op-vocabulary.md` pointing at 23/05, which P06 should repoint to its new page.
  - The defaults table belongs on that page.
  - Cancel: the field takes a token, but the graph build and the ABI's create calls have
    none (choice 7). Decide whether the ABI widens for it.
- **Cross-OS:** `porous.cpp` is in the `-ffp-contract=off` list. The gradient code reuses
  the query's arithmetic, so the values stay what P03 pinned. CI's first macOS and Windows
  runs confirm or refute this.
- **MSVC `/W4`** is unverified for the new code. I used explicit casts and `std::snprintf`.
- **The owner's review list (P08.2) gets** the seven in-run choices above.
- **No human check is raised by this unit.** Everything was verified headless, and nothing
  here needs a window or a visual judgement.
