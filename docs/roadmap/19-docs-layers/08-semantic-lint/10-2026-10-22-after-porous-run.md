# Semantic lint — after the porous shell run

Run on 2026-10-22, at the close (unit P08.1) of the porous shell run on branch
`feat/porous-shell` ([block 23](../../23-porous-shell/README.md)). It is a **delta run** over
[09](09-2026-10-21-after-hygiene-run.md), whose tree (`adc465b`, the hygiene run's close) is
this branch's base. Since then the run added the porous source: the seeds (#63), the exact
Delaunay tetrahedralization and the packed complex (#64, with Shewchuk's predicates vendored),
`PorousField` and the graph token (#65), the GLSL emitter and the viewer's data textures (#66),
the ABI test and the docs (#67). Those sessions edited `design/06`, `design/12` and the design
index. For (a) and (b) this run reads every design page that names a changed file or symbol,
and every page that enumerates something the run added to (the hooks' drivers, the numeric
constants, the vendored code, the field library's shape); (c), (d) and (e) are swept over what
the run wrote and the rows it could have met. Line numbers are as read on `8260482`, before
this run's fixes. The gate was green before the read (`check.py --docs`, 28 checks).

**Inputs.**

- **Last code change:** `f2fe93b` (2026-10-22, P06.1).
- **Last design change:** `f2fe93b` (2026-10-22, design/12 in the same commit).
- **`doc-lag`:** 0 commits.
- **Code changed since 09:** `include/dualc/{porous,dualc}.h`,
  `src/implicit/porous{,_seeds,_delaunay,_complex}.cpp`,
  `src/internal/{porous_complex,porous_delaunay,seed_grid,exact_predicates}.h`,
  `src/internal/exact_predicates.c`, `src/internal/compat/sys/time.h`, the vendored
  `src/internal/third_party/shewchuk/predicates.c`, `examples/{field_graph,field_glsl,raymarch_gl}.{h,cpp}`,
  `examples/{dualc_field,dualc_field_view,dualc_glsl_parity}.cpp`, three samples,
  `capi/dualc_c_demo.c`, `CMakeLists.txt`, three test files.

**Pages read for (a) and (b).**

- `design/12:59-104` (§ The porous source, all eight bullets) against `include/dualc/porous.h`,
  `src/implicit/porous.cpp:1-160`, `src/implicit/porous_complex.cpp:85-105`, `:440-470`
  (`makeBins`, `packStruts`), `src/internal/porous_complex.h:140-215`,
  `examples/field_graph.cpp:1180-1196`, `:1324-1335` (the token, `childBakeMargin`),
  `examples/field_glsl.cpp:1518-1630`, `:2031-2060` (`emitPorous`, the step and feature
  scales), `examples/field_glsl.h:96-110`, `CMakeLists.txt` (the porous sources' options).
- `design/06:1-8`, `:41-70` (§ 8) against `src/internal/exact_predicates.c:1-60`,
  `CMakeLists.txt` (C as a project language, C90 + extensions, `-ffp-contract=off`).
- `design/10:52-87` (§ Hooks) and `:167-186` (§ The numeric constants) against the cancel
  polls of `porous_seeds.cpp:77-89`, `porous_delaunay.cpp:36`, `porous_complex.cpp:28-38`
  and the ceilings of `porous.h:60-61`, `porous_complex.h:145`, `:162`.
- `design/08:129-150` (§ 10.4) — it delegates the enumeration to the command reference's
  inventory; not a finding. `design/05` — states no compile language rule; `README.md` and
  `STRUCTURE.md` carry "a C compiler is needed", updated by P03.1.

## Findings

| # | Class | Finding | Docs `file:line` | Code `file:line` | Disposition |
| --- | --- | --- | --- | --- | --- |
| 1 | (a) | The page's intro calls § 8 "the one piece of third-party code in the tree"; § 8 lists three (QEF/SVD, nanort, Shewchuk's predicates) — stale since nanort, made more wrong by this run | `design/06-parameters-and-vendoring.md:4` | `src/internal/third_party/`, `src/internal/qef.h` | fixed `b0e7560` (the heading, an anchor, kept) |
| 2 | (a) | The bin bound is defined as "the larger spacing, or the largest cell radius if larger"; past `kPorousMaxBins` the grid coarsens by 1.25× steps and `B` grows with it, so `B` can exceed both | `design/12-field-graph.md:77-79` | `src/implicit/porous_complex.cpp:94`, `:459-467` | fixed `b0e7560`: the rule, with the coarsening, is design/13 § The packed layout |
| 3 | (b) | § Hooks enumerates every driver taking the hooks; `generatePorousSeeds`, `internal::tetrahedralize`, `buildPorousComplex` and the `PorousField` constructor take `cancel` only and are not named | `design/10-invariants-and-tolerances.md:54-57` | `include/dualc/porous.h:101-104`, `:134-135` | handed to the owner: design/10 is at the 15,360 B cap; adding the line needs the split principle 5 prescribes ([23/10](../../23-porous-shell/10-plan-run-record/README.md#open-after-the-gates)); design/13 states the polls meanwhile |
| 4 | (b) | § The numeric constants claims "every numeric constant that decides a behaviour"; the porous ceilings and caps (`kPorousMaxSeeds`, `kPorousMaxRelax`, `kPorousMaxBins`, `kPorousMaxWalk`, the box ceiling, the poll intervals) are absent | `design/10-invariants-and-tolerances.md:4`, `:167-183` | `include/dualc/porous.h:60-61`, `src/internal/porous_complex.h:145`, `:162`, `src/implicit/porous_seeds.cpp:71-89` | handed to the owner with #3 (same cap); design/13 § The constants holds them meanwhile |
| 5 | (c) | The porous library's present-tense contract — the tets' canonical output, the packed layout, the query rules, the field's contract — lived only in the record's as-built pages, and `porous.h` named a roadmap page as its layout's definition | `roadmap/23-porous-shell/03-delaunay-engine.md:16-23`, `04-complex-and-bins.md:26-72`, `05-porous-field.md:19-50` | `include/dualc/porous.h:162`, `src/internal/porous_complex.h:22` | fixed `b0e7560`: a new `design/13-porous-shell.md`; design/12's library bullets moved there; one line in 23/03–05 and the code comments point at it |
| 6 | (e) | D-53's trigger includes "a change to their window or snapshot code". #66 (P05.2) added code to `dualc_field_view` between window creation and the first frame (a recompile against the queried texture limits) and a data-texture upload shared with the parity harness; its snapshots were checked by hand on llvmpipe | `docs/decisions/README.md:51` | `examples/dualc_field_view.cpp:283-311`, `examples/raymarch_gl.cpp:142-163` | handed to the owner ([23/10](../../23-porous-shell/10-plan-run-record/README.md#open-after-the-gates)) |

**(a) — 2 findings, above.** Read and true: the volume read at build time only and
`childBakeMargin`'s pad / 0; the 1-Lipschitz argument (the top of `porous.cpp`), the fold over
the primitives within `k` in ascending (distance, index) order, the struts' cap at `B − smooth`
and the refusal of `smooth ≥ B`; `kPorousMaxWalk` = 1024 and the walls' walks; the Delaunay
walls' 0 outside the hull; every determinism clause (PCG32, exact predicates, Morton
insertion, the canonical list, `-ffp-contract=off` on the four sources, Lloyd summed in
sample order); the ceilings and the graph locating `std::invalid_argument`; no cancel through
`FieldGraph::build`; the preview's one builder, the 2048-wide textures and their formats, the
literals, the step scale 1 and the feature scale, the bake fallback at step 0.5. design/06
§ 8: the renaming to `dualc_pred_*`, the `random()` stub, C90 with extensions, no contraction,
the library's one C source.

**(b) — 2 findings, above,** both on the one page the run did not edit because it is full.

**(c) — 1 finding, above.** The other new record pages (23/06–09) are dated measurements,
choices and reviews; 11/16 prescribes the node's keys and recipes.

**(d) — 0 findings.** Born since 09: 23/01–09 and 11/16 are each linked from a page other than
their folder README (23/08 from 23/07, 23/09 from 23/02); `decisions/03-settled-continued.md`
from the decisions index and block 23.

**(e) — 1 finding, above.** Also read: #25 (`porous` is a source, not a wrapper or
decorator), #11 (the run measured new code, no before/after), D-45 (no create measured from
Boletus; a 2M-seed build would pass 10 s, which is a DualC measurement, not the trigger), #62
(the Boletus plan names it; no host asked), D-29 / D-30 / D-31 (no mesh-scene change), #61 (the
tracer's hit / miss test unchanged), D-33 (no baked-region read traced), D-40 (`docs/raw/` at
30 hand-written text files, about 0.79 MB), #69 (opened by this run; no graded node changed).

## Counts

(a) 2 · (b) 2 · (c) 1 · (d) 0 · (e) 1 — **6 findings**, 3 fixed, 3 the owner's. The run kept
the graph's page current with every code change; what lagged was the library's own contract,
which had no design page, and design/10, which could not grow.

---

← Back to the [semantic-lint runs](README.md) · the [Docs layers index](../README.md).
