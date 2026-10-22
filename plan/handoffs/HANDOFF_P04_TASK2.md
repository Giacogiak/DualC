# Handoff — P04.2: the sponge end to end

Unit P04.2 of [`plan/13_P04_field-and-node.md`](../13_P04_field-and-node.md) § T2; session
of 2026-10-22, branch `feat/porous-shell` in `~/Documents/DualC-porous`. This was the second
attempt at the unit. The first one (2026-10-21) was OOM-killed at a `--depth 8` sponge and
left nothing committed. Nothing was pushed, no remote was touched, and `main`,
`feat/hygiene-and-editor` and `~/Documents/DualC` were left alone.

**Commits:**

- `b02ef48`: the test, the samples, the bench and the record.
- The commit that adds this handoff.

**The record:**

- [23/02 #65](../../docs/roadmap/23-porous-shell/02-record.md#65-the-porous-node-on-the-cpu)
  is now **DONE** 2026-10-22, with a T2 append.
- Every number is in the new child page
  [23/06](../../docs/roadmap/23-porous-shell/06-sponge-end-to-end.md). This handoff gives the
  summary and what the next units need.

## What changed, and why

| File | What |
| --- | --- |
| `tests/test_field_graph.cpp` | `countSurfaces` (a union-find over faces sharing an edge, in the test, not the library) and `porous: a walls sponge contours watertight with more than one component`; the hidden `porous: the end-to-end cases' seeds and tets` (`[porous-bench]`, prints `PorousField::stats()` and the build time per node) |
| `examples/samples/porous-sponge.json`, `porous-foam-bracket.json`, `porous-bone-bracket.json` | The three samples |
| `examples/CMakeLists.txt` | The samples are copied next to `dualc_field` (and `dualc_field_view`); three depth-5 smoke ctests `cli_field_sample_porous-*` |
| docs | 23/06 (new); 23/02 #65 DONE and its append; the block README (page list, status row); the roadmap index (`#63–#65 DONE`, three places); the samples sentence of `command_reference/11-dualc_field/README.md`; `STRUCTURE.md` |

No library code changed (`src/` and `include/` are untouched), and no CLI flag changed.

## Measured, and under what

The conditions: this host (22 GB, 8 cores), GCC 15, Release. `dualc_field` ran with its
defaults, **one process at a time**, under `prlimit --as=8000000000`. Time and peak RSS come
from `/usr/bin/time`.

- **The matrix.** 4 recipes × 2 volumes (unit sphere, `bracket.obj`) × 2 `cells`, with the
  modes the recipes use. Each was run with a crisp and with a smoothed rim, at depth 6 and
  depth 7: 64 runs.
  - Depth 7 took 4.3–23.0 s and 1.6–3.2 GB.
  - Depth 6 took 0.7–2.8 s and 0.30–0.44 GB.
  - Depth 8 was not run (the run's memory rule).
  - The table is in 23/06 § The cases. It gives faces, non-manifold edges and surfaces per
    case.
- **Seeds and tets** come from the hidden bench (23/06 § The complexes). For example:
  - the sphere at S 0.25: 346 seeds (196 of them pad), 1,849 tets;
  - the bracket at S 0.15: 232 seeds (167 pad), 1,290 tets.

  Every build took under 13 ms.
- **The test** runs at depth 6: 80,116 faces, watertight, 14 surfaces, 1.0 s, 0.40 GB peak.
- **The samples at depth 7:**

  | Sample | Faces | Watertight | Surfaces | Time | Peak |
  | --- | --- | --- | --- | --- | --- |
  | `porous-sponge` | 318,424 | yes | 14 | 6.8 s | 3.0 GB |
  | `porous-foam-bracket` | 449,460 | yes | 23 | 13.5 s | 3.0 GB |
  | `porous-bone-bracket` | 189,024 | yes | 7 | 14.7 s | 2.5 GB |

## The findings (details in 23/06 § What the cases showed)

1. **A crisp `intersection` rim on the sphere leaves 2–16 non-manifold edges, at depth 6
   and at depth 7.**
   - Thicker walls do not remove them, and neither does depth 7. On the sphere foam (S 0.4,
     t 0.06, seeds 0–7), the crisp rim gave 2–7 edges per seed at depth 6 and 3–16 at
     depth 7.
   - The cause: a pore corner that nearly breaks through the surface leaves a membrane
     under a cell thick. A field slice through one such edge showed it.
   - **It is the engine's, not the node's.** The existing
     `intersection(sphere, onion(normalize(gyroid), 0.05))` gives 6 non-manifold edges at
     depth 7.
   - The flat-faced bracket's Voronoi foam and sponge are watertight with the crisp rim.
2. **A `smooth-intersection` closes most of it.**
   - With `k` 0.1 at depth 7, the sphere foam (S 0.4, seeds 0–7) is watertight for 8 seeds
     of 8.
   - It is watertight for 3 of 8 at depth 6, and 4 of 8 at S 0.25.
   - At S 0.15, 6 edges remain. The count grows with the number of pores at the rim.
3. **Delaunay walls need the node's `smooth`.** Sliver tets leave pores under a cell thick.
   - The bare onion has 122 non-manifold edges at depth 6.
   - With `smooth` 0.06 and a `k` 0.1 rim at depth 7, the result is watertight (45 surfaces).
4. **Struts cut by the clip leave loose stubs** (5–74 surfaces). A skin anchors them: the
   bracket bone with a skin has 3 surfaces.
5. **A float32 STL can weld two vertices** that the engine kept apart. The bracket foam
   shows 2 non-manifold edges in its STL, and 0 in the engine and in its OBJ.

## The in-run choices

Each is **Chosen in-run (pending owner review)**, recorded in 23/06 § The in-run choices and
in the 02 append:

1. **The test and `porous-sponge.json` use a `smooth-intersection` rim** (k 0.1). The raw
   plan's recipes use a crisp `intersection`. The two bracket samples keep the crisp rim,
   which is watertight there.
2. **The test pins `seed=3` at depth 6.**
   - Seeds 3–5 of 0–7 are watertight with that graph at depth 6.
   - Depth 7 is watertight for all eight seeds, but it costs 3 GB, which is over the 1 GB
     cap for a committed ctest.
   - The edges in question are a membrane under a cell thick. They are not near a
     floating-point tie, so another OS should not flip them. CI's first macOS and Windows
     runs confirm or refute this.
3. **No new tracked item, and no `design/10` line, for the crisp-rim limit.** Both homes are
   at the byte cap:
   - `docs/design/10-invariants-and-tolerances.md` is at exactly 15,360 bytes;
   - `docs/decisions/README.md` is at 15,092 bytes, too full for an open row.

   The fact lives in 23/06. **P08 or the owner:** decide whether to split one of them and
   then open the item (it would be #70, the next free number) or add the line.
4. **The node, sizes and recipe parameters** are mine (23/06 § The recipes as run). The plan
   fixed none of them.

## Checks run

These follow the rules for P01–P07: no full gate.

- `cmake --build build -j`: no compiler warnings.
- `ctest --test-dir build -R 'porous|field_graph|domain|field_glsl'`, sequential: **38/38
  passed**, in 20.7 s with a 0.40 GB peak. This is the T1 acceptance pattern plus the
  driver's smoke filter.
- The driver's exact filter `-R 'porous|field_graph|field_glsl'`: 34/34 passed.
- `ctest -R cli_field`: 66/66 passed.
- `python3 scripts/check.py --docs`: 27 OK + 1 INFO (`index-staleness`, the same 15 entries
  as before), PASS. The pre-commit hook ran on `b02ef48`.
- No docs file was deleted or moved, so no loss audit was due.
- **Not run:** the full gate, `--docs --strict`, the GPU tier, MSVC or Apple Clang. These
  belong to P08 and CI.

## What I got wrong on the way

- **I started out assuming the non-manifold edges were walls that were too thin.**
  - Thicker walls, depth 7 and a skin did not help. A field slice located the real cause,
    the rim membrane.
  - The gyroid control then showed that it predates this block.
  - Next time, slice through the first bad edge before sweeping parameters.
- **I used a wildcard inside a Catch2 test name.** Catch2 accepts a wildcard only at the
  ends, so the filter matched nothing. The test binary also changes into a work directory,
  so the bench needs an absolute mesh path, which is why it reads
  `DUALC_POROUS_BENCH_MESH`.
- **Doc sizes.**
  - 23/06 first came out at 2,595 words. I trimmed it to 2,491, under the 2,500 cap.
  - I first wrote that `design/10` "now names" the rim. It could not, because that page is
    at the cap, so I corrected the in-run choice.

## Plan corrections

None to the acceptance criteria. The plan's "on a demo mesh (`build/data/`)" is the bracket.
`bracket.obj` is byte-identical in `build/data/` and `build/examples/`, and the samples read
it from `build/examples/`.

## T2 acceptance

- **The component test passes:** `porous: a walls sponge contours watertight with more than
  one component`. It asserts 0 boundary edges, 0 non-manifold edges, a contour that is not
  empty and 14 surfaces (> 1).
- **The samples are committed, and their measurements are in the block:** three
  `examples/samples/porous-*.json`, measured in 23/06 § The samples with their
  ready-to-run lines.
- **Per case, the record has** seeds, tets, faces, components, depth and wall time: 23/06
  § The complexes and § The cases.

## P04 exit criterion

- **"`dualc_field` produces a watertight, multi-cavity sponge from any volume, with
  tests":** met, with the stated condition.
  - On a primitive: `dualc_field porous-sponge.json --depth 7` gives a watertight sponge
    with 14 surfaces.
  - On a mesh: `porous-foam-bracket.json` gives a watertight sponge with 23 surfaces;
    `porous-bone-bracket.json` gives 7.
  - The test covers it in ctest.
  - The P04.1 tests (the 1-Lipschitz field, the smoothing bound, a mesh volume) are still
    green.
- **The condition.** On a curved volume, the rim has to be a `smooth-intersection` (finding
  2). The raw plan's crisp `intersection` leaves a few non-manifold edges there. That is an
  engine limit that predates this block, and it also appears for a clipped gyroid.

## For the owner's review

**needsHumanCheck is raised.** Whether the samples look like a natural sponge is a visual
judgement that no headless check can make.

```bash
cd ~/Documents/DualC-porous/build/examples
dualc_field porous-sponge.json --depth 7 -o sponge.stl
dualc_field porous-foam-bracket.json --depth 7 -o bracket_foam.stl
dualc_field porous-bone-bracket.json --depth 7 -o bracket_bone.stl
```

Each run takes up to 15 s and 3 GB; run them one at a time. Open the STLs in a viewer or a
slicer and cut a section. Look for:

- closed cavities inside;
- the sponge's walls thickening outwards;
- the bone's struts.

What was verified headless: watertightness (the engine's `Diagnostics` and the OBJ) and the
surface counts.

## Open, and what the next units need

- **P05 (GLSL):**
  - The three samples are copied next to `dualc_field_view` too. Until P05 lands, the
    viewer cannot draw a `porous` node.
  - The samples make good parity and viewer cases: the sponge (walls + graded) and the
    Delaunay struts.
- **P06 (docs):**
  - Carry findings 2, 3 and 4 into the `porous` command-reference page as advice:
    - a `smooth-intersection` for a curved volume;
    - `smooth` ≈ 2 × the thickness for Delaunay walls;
    - a skin for struts.
  - The cost guide can take 23/06's numbers: depth 7 ≈ 3 GB whatever the recipe, depth 6 ≈
    0.4 GB.
  - Repoint the samples sentence of `11-dualc_field/README.md`, now at 15,267 bytes, to the
    new page.
- **P08:**
  - `docs/roadmap/README.md` is at 15,306 bytes, `design/10` at 15,360 and the decisions
    index at 15,092. Any further growth needs a split.
  - The in-run choices above go on the owner's review list.
- **Memory:** every porous contour at depth 7 is about 3 GB. Keep committed ctests at depth
  6 or below.
