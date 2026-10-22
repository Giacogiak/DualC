# Semantic lint — after the hygiene run

Run on 2026-10-21, at the close (unit P08.1) of the hygiene + editor run on branch
`feat/hygiene-and-editor` ([17/22](../../17-code-audit-and-hardening/22-hygiene-plan-run.md)).
It is a **delta run** over [08](08-2026-10-19-after-contourer-threads.md), which covered the
thread count (P02). Since then the run changed the `gradientAt` contract and added
`trueGradientAt` (P03, [17/19](../../17-code-audit-and-hardening/19-gradient-contract.md)),
changed and then restored the `GridField` normal (P04, P04b,
[17/20](../../17-code-audit-and-hardening/20-gridfield-normal.md)), made the gate build the
C ABI (P05, [14/06](../../14-c-abi/06-linux-shared-library.md)) and pruned the repeats'
neighbour set behind a new `boundsBoundValue` claim (P06,
[17/21](../../17-code-audit-and-hardening/21-repeat-pruning.md)). Those sessions edited
`design/08` and `design/10`. For (a) and (b) this run reads every design page that names a
changed file or symbol (`rg 'gradientAt|trueGradientAt|closestSurfacePoint|boundsBoundValue|fd_gradient|repeat|WindingNumberField|GridField|C_ABI|PIC' docs/design`);
(c), (d) and (e) are swept over what the run wrote and the rows it could have met. Line
numbers are as read on the working tree before this run's fix. The gate was green before
the read (`check.py --docs`, 28 checks).

**Inputs.**

- **Last code change:** `6a231c7` (2026-10-21, P06.1).
- **Last design change:** `6a231c7` (2026-10-21, design/08 in the same commit).
- **`doc-lag`:** 0 commits.
- **Code changed since 08:** `include/dualc/{implicit,primitives}.h`,
  `src/implicit/{implicit_field,grid_field,winding_field,decorators,combinators,domain_ops,lift,primitives_tierA,primitives_tierB}.cpp`,
  `src/internal/fd_gradient.h` (new), `scripts/check.py`, `capi/CMakeLists.txt` (comments),
  four test files.

**Pages read for (a) and (b).**

- `design/08:12-110` (§ 10.1: the virtual list, the `gradientAt` / `trueGradientAt` /
  `boundsBoundValue` paragraphs, the three defaults) against `include/dualc/implicit.h:44-125`,
  `src/implicit/implicit_field.cpp:73-95`, `src/implicit/grid_field.cpp:108-155`,
  `src/implicit/winding_field.cpp:76-108`, the `trueGradientAt` / `boundsBoundValue`
  overrides in `combinators.cpp:63-170`, `decorators.cpp:54-414`, `domain_ops.cpp:177-436`,
  `primitives.h:51-207`, `primitives_tierA.cpp:63-116`, `primitives_tierB.cpp:161`.
- `design/08:130-162` (§ 10.4: `GridField`, the repeats' skip rule) against
  `src/implicit/domain_ops.cpp:25-100` (`CopyPruner`, `anyTileOverlaps`).
- `design/10:170-180` (the tolerance table) against `src/internal/fd_gradient.h` and
  `src/implicit/winding_field.cpp:55`.
- `design/01:150-220` (`edgeHit`, the miss-fallback ladder) — `closestSurfacePoint` named at
  its depth, still true.
- `design/05`, `design/06`, `design/10:36-42` for the build and the gate — no page states the
  gate's configure options or the PIC rule; their homes are `AGENTS.md`, `README.md` and
  [17/09](../../17-code-audit-and-hardening/09-local-checks-gate/README.md), updated by P05.

## Findings

| # | Class | Finding | Docs `file:line` | Code `file:line` | Disposition |
| --- | --- | --- | --- | --- | --- |
| 1 | (a) | The central-difference row says `e = 1e-4`, absolute, is the step of *every* finite-difference gradient. `WindingNumberField`'s straddle-aware difference — its `trueGradientAt` since P03.2 — steps `1e-4 ×` its mesh's bounding-box diagonal, a relative step | `design/10-invariants-and-tolerances.md:176` | `src/implicit/winding_field.cpp:55`, `:76-97` | fixed at the close (P08.1): the row names the winding field's step; one sentence of the resolution rule shortened to keep the page under the cap |
| 2 | (e) | #11's trigger names "work needing before/after numbers (#18, #27)". #34 (P04.1, P04b.1) and #35 (P06.1, P06.2) each needed them and each built a scratch benchmark and evaluator under `/tmp` (`bench.sh`, the deviation evaluator, the dihedral histogram, the reference meshes), none committed | `docs/decisions/README.md:24` | — (`/tmp/p04b/`, `/tmp/p06/`, scratch) | handed to the owner: [17/22 § Open after the run](../../17-code-audit-and-hardening/22-hygiene-plan-run.md#open-after-the-run) |

**(a) — 1 finding, above.** Read and true: the 3 + 6 virtuals; the unit-direction contract
and its three shorter-than-1 nodes; `trueGradientAt`'s default (`fdTrueGradient`, step 1e-4,
6 calls), its exact overrides (sphere, plane, `GridField`) and its forwarders (offset,
onion — `roundedOf` is an offset — transform, scale, mirror, both repeats, the four hard
booleans); the Newton step `q - G·f/|G|²` and its `false` cases; `boundsBoundValue`'s
claimants (sphere, box, capsule, round cone, hard union); the repeats' two skips and their
strict `<`. `design/08:44` calls `fd_gradient.h` "the stencil shared with the distorted
operators" — the lifts share it too, which the tolerance row says; not a contradiction.

**(b) — 0 findings.** Every design page naming a changed file was edited in the session that
changed it (design/08 at P03.2, P04.1, P04b.1 and P06.1; design/10 at P03.2), and the read
above found them current.

**(c) — 0 findings.** The run's new record pages (17/19, 17/20, 17/21, 14/06's section,
block 22) are dated; the "today" hits are dated measurements (17/21:145) or verbatim
archive (22/01, 22/02).

**(d) — 0 findings.** Born since 08: 17/19, 17/20 and 17/21 are linked from 17/04 and
design/08 (21 from cmd-ref 11/01); 22/01 and 22/02 link each other and are the archive their
folder README indexes, as block 15's is.

**(e) — 1 finding, above.** Also read: #25 (no domain wrapper or decorator added —
`boundsBoundValue` is a virtual, not a wrapper), #24, #18, #27 (no benchmark suite), D-29 /
D-30 / D-31 (no preview change), D-33 (no baked-region read changed), D-40 (`docs/raw/` at
28 hand-written text files, about 0.76 MB), D-45, D-53, #62 (no host asked).

## Counts

(a) 1 · (b) 0 · (c) 0 · (d) 0 · (e) 1 — **2 findings**, 1 fixed, 1 the owner's. The design
layer kept up with every code change of the run; what lagged was a table row that stated a
step for "every" finite difference before the winding field's difference became a
`trueGradientAt`.

---

← Back to the [semantic-lint runs](README.md) · the [Docs layers index](../README.md).
