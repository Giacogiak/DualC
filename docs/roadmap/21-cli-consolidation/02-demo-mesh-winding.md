# CLI consolidation — the demo meshes wound outward, measured

Part of [CLI consolidation](README.md) (block 21): the measurements behind
[#55](01-record.md#55-generated-demo-meshes-wound-outward), landed 2026-10-14 by the plan-run
unit P01.2 on branch `feat/cli-consolidation`. The item's record holds the summary; this page
holds the numbers, so the record stays under the cap.

## What was wrong, per shape

Signed volume of the `dualc_gen_demo` output (fan over each face, against the origin). Five
shapes were wound inward throughout; the **bracket was mixed** — its caps faced out and its
six side walls in, so each cap-to-wall edge was walked the same way by both its faces and its
signed volume was neither +0.384 (the true L-prism, 0.64 × 0.6) nor its negative. The screening
called all six "negative"; the bracket needed only its walls reversed, not the whole mesh.

| Shape | Before | After | What was reversed in `examples/demo_meshes.cpp` |
| --- | --- | --- | --- |
| `torus` | −2.399 | 2.399 | every quad |
| `knot` | −12.118 | 12.118 | every quad |
| `cylinder` | −6.265 | 6.265 | side quads and both n-gon caps |
| `bracket` | −0.128 | 0.384 | the six side walls only (the caps were already outward) |
| `hexbore` | −2.363 | 2.363 | outer wall, bore wall, both annulus caps |
| `uvsphere` | −4.159 | 4.159 | both pole fans and the middle quads |

`cube` (1.000), `sphere` (4.153) and `genus2` (7.712; contoured by DualC itself) were already
outward and are unchanged. The new case *Every generated demo mesh winds outward*
(`tests/test_demo_meshes.cpp`, tag `[orientation]`) asserts, for all nine shapes, a positive
signed volume and a generalized winding number of 1 ± 0.1 at an interior point (inside the
tube for `torus`, `knot` and `genus2`, inside the wall for `hexbore`). On the old winding it
failed 14 of 18 checks — GWN −1.067 … −1.001 on the five reversed shapes and **−0.310** on the
mixed bracket; on the new winding all 18 pass, the GWN reading 1.001 … 1.067. The margin is
wide because `WindingNumberField` uses the first-order far-field approximation (β = 2): it
reads 1.066 at the icosphere's centre.

## The oracles, before and after

`dualc_demo --depth 6 <mode> <shape>.obj` on Linux (GCC 15, Release), faces and the signed
volume of the output. Before = the meshes the build generated at `22e6582`; after = this
unit's generator.

| Shape | Parity (both) | `--gwn` before → after | `--gwn-field` before → after | `--pseudonormal` before → after |
| --- | --- | --- | --- | --- |
| `torus` | 40,032 / 2.399 | 1 (placeholder) → 40,032 / 2.399 | 1 → 18,320 / 2.401 | 40,032 / **−2.399** → 40,032 / 2.399 |
| `knot` | 30,116 / 12.169 | 1 → 30,116 / 12.169 | 1 → 15,316 / 12.213 | 30,116 / −12.169 → 30,116 / 12.169 |
| `cylinder` | 38,396 / 6.268 | 1 → 38,396 / 6.268 | 1 → 20,652 / 6.265 | 38,396 / −6.268 → 38,396 / 6.268 |
| `bracket` | 34,984 / 0.384 → 0.385 | 13,428 / −0.040 → 34,984 / 0.385 | 9,804 / 0.030 → 18,504 / 0.384 | 55,832 / −0.134 → **34,984** / 0.385 |
| `hexbore` | 45,408 / 2.364 | 1 → 45,408 / 2.364 | 1 → 23,936 / 2.361 | 45,408 / −2.364 → 45,408 / 2.364 |
| `uvsphere` | 31,644 / 4.160 | 1 → 31,644 / 4.160 | 1 → 17,580 / 4.161 | 31,644 / −4.160 → 31,644 / 4.160 |

So: an inside-out input made `--gwn` and `--gwn-field` (and the graph's `winding(…)`, which is
the same field) return the empty placeholder, and made `--pseudonormal` return the right
surface turned inside out — same face count, negated volume — except on the mixed bracket,
where it built a different surface. Parity counts ray crossings and ignored the winding
throughout: its output was the correct solid before and after.

What parity's output does see is the input's normals: the smooth Hermite normal is the blend
of the input's vertex normals. After the fix, on the parity runs above, the face lists are
identical, every output `vn` flips to point outward (on the bracket 47,868 of 104,952 — the
other half came from its already-outward caps), and vertex positions move by at most 0.0056
(`hexbore`; `torus` 0.0011, `knot` 7 × 10⁻⁶): reversing a quad changes the diagonal its
triangulation uses, so the vertex normals and the QEF inputs move slightly. No test or page
recorded a count or normal on these six shapes: `check_cli_output.cmake` checks
`cli_demo_torus` and `cli_demo_bracket_sharp` for a parseable OBJ with at least one face only,
and the 306 existing ctest cases still pass (307 with the new one).

## The stale copies next to the tools

Regenerating `build/data/` did not refresh the copies next to the CLIs: those are a
`POST_BUILD` step of each consuming target, and nothing relinks a CLI when only the meshes
change, so a local build kept the old inside-out meshes in `build/examples/` (CI builds clean
and never saw it). `dualc_copy_demo_meshes` (`examples/CMakeLists.txt`) now also makes the
meshes `LINK_DEPENDS` of each consumer: a regenerated mesh relinks it and the copy follows.
Verified with Ninja by touching `dualc_gen_demo.cpp`: the meshes regenerate, the consumers
relink, the copies match `build/data/`. The Visual Studio generators ignore `LINK_DEPENDS`;
there a clean build is fresh.

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
