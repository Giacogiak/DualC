# Semantic lint — after the contourer's thread count

Run on 2026-10-19, after unit P02 of the hygiene + editor run
([17/18](../../17-code-audit-and-hardening/18-contourer-threads.md), #34's thread bullet):
`ContourerParams::numThreads`, a thread count on both bakes, `FieldGraph::build` and
`compileToGlsl`, the C ABI at 0.5.2 and `--threads` on `dualc_field` / `dualc_field_view`.
The session edited `design/03` and `design/06`. It is a **delta run** over
[07](07-2026-10-16-after-cli-consolidation.md): for (a) and (b) it reads every design page
that names a changed file or a thread count (`rg 'numThreads|thread count|hardware threads|bakeToGrid|resolveThreadCount' docs/design`);
(c), (d) and (e) are swept over what the session wrote and the thread-related rows. Line
numbers are as read on the session's working tree, before this run's fixes. The gate was
green before the read (`check.py --docs`, 28 checks, 27 OK, 1 INFO).

**Inputs.**

- **Last code change:** `16d8c55` (2026-10-16) committed; this session's change is the
  unit's own commit, made with this run.
- **Last design change:** `16d8c55` (2026-10-16); `design/03` and `design/06` in this
  session.
- **`doc-lag`:** 0 commits.
- **Code changed:** `include/dualc/{contourer,implicit}.h`, `src/contourer.cpp`,
  `src/implicit/{grid_field,mesh_source}.cpp`, `capi/dualc_c.{h,cpp}`, `capi/dualc_c_demo.c`,
  `examples/{field_graph,field_glsl,example_common}.{h,cpp}`, `examples/dualc_field.cpp`,
  `examples/dualc_field_view.cpp`, `tests/test_parallel.cpp`, three `CMakeLists.txt`.

**Pages read for (a) and (b).**

- `design/03:10-40` and `design/06:8-38` (this session's edits) against
  `src/contourer.cpp:697-724` and `include/dualc/contourer.h:19-34`.
- `design/01:70-104` against `src/sampler.cpp` and `src/internal/parallel.h` — unchanged
  code, still true.
- `design/08:92-100` and `design/11:63` (`bakeToGrid` named, no thread claim) against
  `include/dualc/implicit.h`.
- `design/10:22-27` and `:80` (the thread-count invariant) and `design/12:20-45` (per-node
  bake) against `src/implicit/{grid_field,mesh_source}.cpp` and
  `examples/field_graph.cpp:1260-1290`.

## Findings

| # | Class | Finding | Docs `file:line` | Code `file:line` | Disposition |
| --- | --- | --- | --- | --- | --- |
| 1 | (a) | "The output is bit-identical regardless of the thread count. Every worker owns a disjoint subtree" — true of the sampler only. The contourer's leaf pre-solve and both bakes are parallel too, now under their own counts; each is bit-identical for its own reason (a pure per-leaf solve with indices assigned serially; one writer per voxel), which the sentence did not cover | `design/10-invariants-and-tolerances.md:22-24` | `src/contourer.cpp:697-724`; `src/implicit/grid_field.cpp:168-185`; `src/implicit/mesh_source.cpp:165-205` | fixed in this session's commit: the sentence names the subtree, the leaf solve and the bake voxel |
| 2 | (b) | *Per-node bake* names `bakeToGrid` and `MeshSource::bakeToGrid` as the two bakes but not how many workers run them; `FieldGraph::build` now takes `bakeThreads`, set by `--threads` | `design/12-field-graph.md:28-30` | `examples/field_graph.h:239-244`; `examples/field_graph.cpp:1267-1290` | fixed in this session's commit: one sentence on `bakeThreads` |

**(a) — 1 finding, above.** Read and true: `design/03`'s code excerpt matches
`src/contourer.cpp:716-724`; `design/06`'s new `numThreads` row matches the header default
and comment, and its bake signatures match `include/dualc/implicit.h`; `design/01:89-104`
(the sampler's guarantee, `numThreads == 0` = all) is unchanged.

**(b) — 1 finding, above.** `design/08:98-99` lists the bakes as library calls without
parameters, which stays true at its depth.

**(c) — 0 findings.** The session's new record pages (17/18, 14/04 § 0.5.2) are dated;
14/02 § 3.3 is the ABI's mapping contract, unchanged in kind; the usage rows on 11 and 12
state the flag's meaning.

**(d) — 0 findings.** `command_reference/11-dualc_field/15-diagnostics.md` is born in this
session (the Diagnostics section moved out of 11's README for the cap) and linked by its
folder README; 17/18 is linked from 17/04, 14/02, 14/04 and 14/05.

**(e) — 0 triggers met.** Thread-related rows read: D-53 (the viewer's parser changed, not
its window or snapshot code), D-52 (no second `parallelForPolled` failure), D-45 (no
cancellation checkpoint added, no bake measured over 10 s from Boletus), D-29 (bake time
not reported). The rest are untouched by the session.

## Counts

(a) 1 · (b) 1 · (c) 0 · (d) 0 · (e) 0 — **2 findings**, both fixed. The session's own
design edits held; what lagged were the two pages that state the same guarantee from
another angle — the invariant list and the graph's bake rule.

---

← Back to the [semantic-lint runs](README.md) · the [Docs layers index](../README.md).
