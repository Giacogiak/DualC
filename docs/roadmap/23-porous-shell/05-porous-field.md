# The porous field as built — #65, PorousField and the graph token

The contract, the choices and the numbers behind
[02 #65](02-record.md#65-the-porous-node-on-the-cpu)'s first half (P04 T1, 2026-10-21): the
field over the packed complex of [04](04-complex-and-bins.md), and the `porous` token that
builds it in `dualc_field`.

## What was built

| File | What |
| --- | --- |
| `include/dualc/porous.h` | `PorousParams` (the seeds, `cells`, `mode`, `smooth`) and `PorousField : ImplicitField`, with `stats()` (seeds, pad seeds, tets, faces, edges, segments, bins, packed bytes, `B`) and `params()` |
| `src/implicit/porous.cpp` (new) | The field. The comment at its top is the Lipschitz-1 argument for the default `cellOverlaps` |
| `src/internal/porous_complex.h`, `src/implicit/porous_complex.cpp` | `porousDistanceGradient`: the same value, bit for bit, and its gradient from the same query |
| `examples/field_graph.cpp` | The `porous` token: 1 or 2 children, the twelve keys, `childBakeMargin` |
| `examples/dualc_field.cpp` | The `Porous source` group of `--list` |
| `CMakeLists.txt` | The new source, `-ffp-contract=off` like the rest of the porous code |

## The contract

- **Value.** The value is `porousDistance(complex, p, smooth)`. The struts are then capped
  at `B − smooth` (choice 1 below). The value is ≥ 0 and 0 on the complex, so a contour of
  the bare node is empty. The node is a source to give a thickness to.
- **Gradient.** `trueGradientAt` is analytic, from one query, with the magnitude kept:
  - a bisector's gradient is `−e/|e|`, a face plane's is `−n`, a segment's is the unit
    offset from its nearest point;
  - with `smooth` > 0, each step of the fold weighs them `(1 − h/2, h/2)`;
  - it is zero where a clamp holds the value (at 0, or at the struts' ceiling).

  `gradientAt` normalizes it, and returns `(0,0,1)` where it vanishes, as `implicit.h`
  requires. No central difference is used anywhere.
- **`bounds()`.** The volume's box.
- **`cellOverlaps`.** The default. It is exact because the field is 1-Lipschitz; the
  argument is in `porous.cpp`.
- **Build time only.** The volume and the control are read during construction (the seeds),
  never after. The test destroys a mesh volume and then evaluates the field.
- **Errors.** `std::invalid_argument("porous: …")` on a negative or non-finite `smooth`,
  and, for struts, on a `smooth` ≥ `B`. The seeds' and the complex's own errors pass through;
  the graph locates all of them.
- **Cancel.** The constructor takes a `CancelToken`. **`FieldGraph::build` has none**, nor
  do the C ABI's `dualc_field_create_from_*` that call it (only contour and export take a
  token), so the graph passes `nullptr`: a `porous` node's build cannot be cancelled.
  Threading one through would change `FieldGraph::build`'s signature and the ABI's, a
  question for #67.

**The node, as the graph reads it** (the raw plan's signature; the library checks the rest):

| Key | Default | Checked by the graph |
| --- | --- | --- |
| children | volume[, control] | 1 or 2 |
| `cells` | `voronoi` | `voronoi` \| `delaunay` |
| `mode` | `walls` | `walls` \| `struts` |
| `dist` | `poisson` | `poisson` \| `jitter` \| `random` |
| `spacing` | required | finite, > 0 |
| `spacing2` | `= spacing` | finite, > 0; a different value needs the control |
| `d0`, `d1` | 0, — | `d1` required when `spacing2` ≠ `spacing` |
| `ramp` | `linear` | `linear` \| `smooth` \| `gaussian` |
| `relax` | 0 | a whole number in [0, 50] |
| `seed` | 0 | a whole number in [0, 2⁵³] (a JSON number is a double) |
| `smooth` | 0 | library: finite, ≥ 0, < `B` for struts |
| `jitter` | 1 | library: ≥ 0 |

## The in-run choices

Each is **Chosen in-run (pending owner review)**:

1. **The field caps the struts at `B − smooth`, not at `B`.** At k > 0, `porousDistance`'s
   struts step where a segment crosses `d = B` while the fold's accumulator is within k of
   `B`. Measured on close pairs (1e-5 apart, 400k pairs, the sphere seeds of the tests,
   k = 0.06, `B` = 0.241), the largest slope is:
   - 15.2 for the Voronoi struts and 1,076 for the Delaunay struts with the cap at `B`
     (steps of about 0.01, or 0.18 k);
   - 0.99998 for both with the cap at `B − k`.

   Including a segment at `B` moves the fold only where the accumulator is above `B − k`,
   and then the fold stays above `B − k`, so the cap hides every step. The cap lives in
   `PorousField` and leaves P03's query and its brute-force test unchanged. **#66 must emit
   the same `min(·, B − smooth)`.** The cost: far from every strut, the smoothed value
   reads `B − k` instead of `B`. It is lowered by k there, which is outside the smoothing
   bound, and only past `m + k ≥ B`, a region no strut radius below `B − k` reaches.
2. **`smooth` ≥ `B` with `mode=struts` is an error.** At that value the cap would leave 0
   everywhere. The walls take any `smooth`.
3. **The gradient is analytic for all four cases and both values of k**, through a new
   `porousDistanceGradient` beside `porousDistance`. The value and the arithmetic are shared,
   so the pinned digests and the brute-force equality still hold.
4. **`childBakeMargin(porous, 0)` = max(`spacing`, `spacing2`); for the control it is 0.**
   The volume is read up to the pad above its zero level, where a baked mesh's far field
   over-estimates. The node's own value never reads the volume, so the ancestors' margin
   does not pass down. The plan expected no entry at all, because the volume is never read
   below zero.
5. **The domain-wrapper property test does not apply.** `porous` reads no child at
   evaluation time, so there is no read to cover. `test_domain_ops.cpp` is unchanged.
6. **`PorousField` is public** (`dualc/porous.h`) behind a pimpl, so the graph and the C
   ABI build it. The packed complex stays internal. #66 needs a read-only view of its
   arrays for the textures, either a public struct or an accessor; that is P05's call.

## Tested

`ctest -R 'porous|field_graph|domain'`: 34/34. Ten new cases:

- In `test_porous.cpp`:
  - **1-Lipschitz** on 20,000 random pairs (lengths 1e-4 to 0.3) per case and per k
    ∈ {0, 0.06}. `|Δf| ≤ |Δp|(1 + 1e-6) + 1e-9`, and the struts read `B − k` far away.
  - **The smoothing bound.** For 3,000 points per case:
    - `smooth` only lowers the value;
    - by < k where it is exact;
    - by ≤ k/4 where two primitives are within k of the minimum, at more than 50 such
      points per case;
    - the field is bit-equal to the packed query.
  - **The gradient** against a central difference: at least 97 % of 2,000 points agree to
    1e-3, `|∇| ≤ 1`, and `gradientAt` is a unit vector or `(0,0,1)`.
  - **A mesh volume** (the demo torus), destroyed before the field is evaluated.
  - **Determinism and errors:** two fields are bit-equal; the bad `smooth` values, a seeds
    error and a cancel are refused.
- In `test_field_graph.cpp`:
  - shorthand = JSON = the `dumpJson` round trip;
  - all twelve keys read, and the defaults;
  - unknown keys, the missing `spacing` and bad values come back as located errors;
  - a mesh volume through the graph;
  - `childBakeMargin`.
- Hidden: `porous: the struts' ceiling and the raw query's slope` (`[.][porous-bench]`), the
  numbers below.

## Measured

This Linux host, GCC 15, Release, one thread, two runs, the slower given. The volume is the
analytic unit sphere, `spacing` 0.05 (23,344 seeds, 148,406 tets), `smooth` 0.015, and
200k queries:

| Case | Build | Value /s | Analytic gradient /s | Central difference /s |
| --- | --- | --- | --- | --- |
| Voronoi walls | 0.86 s | 1.28M | 1.18M | 277k |
| Voronoi struts | 0.96 s | 112k | 108k | 26k |
| Delaunay walls | 0.94 s | 3.44M | 3.33M | 1.04M |
| Delaunay struts | 0.86 s | 134k | 129k | 28k |

**A smoke contour** of `intersection(sphere, onion(porous(sphere, spacing=0.25),
thickness=0.02))` at `--depth 7` (8 threads) took 5.6 s and gave 641,676 faces with 14
non-manifold edges. The walls (0.04) are 2.5 cells of 0.016. Watertightness, depths and
the multi-cavity count are T2's ([02 #65](02-record.md#65-the-porous-node-on-the-cpu)).

---

← Back to [block 23](README.md).
