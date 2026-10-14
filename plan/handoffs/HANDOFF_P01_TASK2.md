# Handoff — P01.2: the six inside-out generated meshes wound outward (F1, #55)

Unit P01.2 of [`plan/10_P01_record-and-demo-meshes.md`](../10_P01_record-and-demo-meshes.md)
§ T2, the last unit of phase P01; session of 2026-10-14, branch `feat/cli-consolidation`
(pushed to `origin`; `main` untouched). The record is
[roadmap 21 #55](../../docs/roadmap/21-cli-consolidation/01-record.md#55-generated-demo-meshes-wound-outward)
and its measurement page
[21/02](../../docs/roadmap/21-cli-consolidation/02-demo-mesh-winding.md); this handoff points at
them and adds what the next unit and Giacomo need.

## What changed, and why

Commits: ``75bfd27`` (generators, test, CMake, docs), then this handoff.

- **`examples/demo_meshes.cpp`** — the winding of `torus`, `knot`, `cylinder`, `hexbore`,
  `uvsphere` reversed throughout (each polygon's vertex order, cylinder n-gon caps included);
  the **bracket only on its six side walls**: its caps already faced out, so it was not
  inside-out but *mixed* (signed volume −0.128, not −0.384). The plan's "reverse the triangle
  order of … bracket" would have turned it inside out the other way. Comments corrected.
- **`tests/test_demo_meshes.cpp`** — new case *Every generated demo mesh winds outward*
  (`[demo][orientation]`): for all nine shapes, signed volume > 0 and the GWN
  (`WindingNumberField`, w = 0.5 − valueAt) at an interior point = 1 ± 0.1. Interior points:
  origin (spheres, cube, cylinder), (1,0,0) torus tube, (3,0,0) knot centreline at s=0,
  (−2.2,0,0) genus2 left tube, (−0.3,−0.3,0) bracket corner block, (0.65,0,0) hexbore wall.
- **`examples/CMakeLists.txt`** — not in the plan: `dualc_copy_demo_meshes` also sets the
  meshes as `LINK_DEPENDS` of each consumer. The mesh copies next to the CLIs are a
  `POST_BUILD` step, so after the generator changed, `build/data/` was fresh but
  `build/examples/*.obj` kept the old inside-out meshes — the local ctest and every CLI check
  would have run on stale input. Now a regenerated mesh relinks the consumers and the copy
  follows (Ninja / Makefiles; VS ignores `LINK_DEPENDS`, a clean build is fresh there).
- **Docs** — 21/01 #55 DONE with a dated *Landed* paragraph, #56 NEXT; new child 21/02 (the
  numbers, so 01 stays under the cap); 21/README table + page list; `roadmap/README.md`
  § Current focus / Track 1 / Principal-blocks row (#55 DONE, #56 NEXT) and a milestone row
  for #55 (the oldest, #51, rolled off per D-43 — its record is 20/03); command reference 09
  states every generated mesh is closed and wound outward; 01 § Choosing a sign oracle:
  "both require … oriented" corrected (parity ignores the winding) plus what each oracle does
  on an inside-out mesh; 11/01 `winding` row and 00's placeholder row name the winding;
  design/02 § parity: "requires closed, oriented input" → closed, not oriented (measured).

## Measured, and under what

Linux, GCC 15, Ninja, Release, `build/`; `dualc_demo --depth 6`; faces / signed volume of the
output (a fan per face against the origin, a 20-line Python script over the OBJ). Before = the
meshes the build generated at `22e6582`.

- Input signed volume before → after: torus −2.399 → 2.399, knot −12.118 → 12.118,
  cylinder −6.265 → 6.265, bracket −0.128 → 0.384, hexbore −2.363 → 2.363,
  uvsphere −4.159 → 4.159; cube 1.000, sphere 4.153, genus2 7.712 unchanged.
- The plan's CLI checks: `--gwn-field torus.obj` 1 face (placeholder) → 18,320;
  `--gwn-field knot.obj` 1 → 15,316; `--gwn torus.obj` 1 → 40,032; `--gwn knot.obj`
  1 → 30,116; `--pseudonormal bracket.obj` 55,832 → **34,984 = parity's count**. All four
  oracles on all six shapes: the table in 21/02 — after the fix every `--gwn` and
  `--pseudonormal` run equals parity's face count and volume; `--gwn-field` gives its own
  (smoother, fewer faces) surface with volume within 0.4 %.
- New test on the old winding: **14 of 18 checks failed** (GWN −1.067 … −1.001 on the five
  reversed shapes, −0.310 on the bracket; volumes negative on six); on the new: 18 / 18 pass,
  GWN 1.001 … 1.067 (1.066 at the icosphere centre — the first-order far-field GWN, β = 2,
  hence the 0.1 margin).
- Parity outputs before vs after: identical face lists; every `vn` flips outward (bracket
  47,868 of 104,952); vertex positions move ≤ 0.0056 (hexbore; torus 0.0011, knot 7e−6) —
  reversing a quad changes its triangulation diagonal, so vertex normals / QEF inputs move.
  No count or normal is recorded for these shapes in a test or a page
  (`check_cli_output.cmake` checks `cli_demo_torus` / `cli_demo_bracket_sharp` for ≥ 1 face).
- `ctest -j8` after the fix: 307 / 307 (306 before + the new case).
- Full `python3 scripts/check.py` at `75bfd27`'s tree (configure SKIP, `build/` configured; the
  two changed sources touched first so the warnings scan is meaningful): 32 checks, 30 OK,
  1 SKIP, 0 failed — 3 TUs compiled, 0 DualC-origin warnings, **ctest 307 / 0 failed**
  (87.9 s), PASS. GPU tier not run: no field node, codegen or parity case moved, and
  `dualc_glsl_parity` reads only `cube.obj` (unchanged).
- Pushed `75bfd27`; `python3 plan/remote_run_check.py` exit 0 — Actions run 38621264223
  `success` for that HEAD. This handoff's commit is docs-only on top of it.
- `docs_loss_audit.py --base 22e6582 --head 75bfd27 --scope docs/` with an empty scratch
  triage (4 min): 8 residual rows on the pages this unit edited, each triaged by hand as
  intended — 01's oracle-table row (corrected), 21/README's page-list sentence (rewritten into
  the new one) and the #55 / #56 status cells, the snapshot's NEXT clause and the #51
  milestone row (rolled off per D-43; its record is 20/03). Everything else is scoring noise
  on unchanged lines (`0.000/1.000`) or date tokens, as P01.1 found.

## What I got wrong on the way

- The first ctest/CLI pass after rebuilding read stale meshes from `build/examples/` (the
  copy is `POST_BUILD`; nothing relinked) — caught by `cmp` against `build/data/`, fixed by
  the `LINK_DEPENDS` change above rather than a manual touch.
- First GWN margin was 1e−3: the hierarchical GWN reads 1.066 at a sphere's centre. The
  margin only needs to separate +1 from −1 and from a mixed winding's −0.31.

## Exit criteria of P01, one bullet at a time

1. *Block 21, its items and the decisions are on the branch, `--docs --strict` green* —
   block 21 (#55–#60, D-55…D-60, D-11 edit) landed in `f0f464e` (P01.1); on this unit's final
   HEAD `python3 scripts/check.py --docs --strict`: 28 checks, 27 OK + 1 INFO (`index-staleness`, 20 entries: the 17 pre-existing plus 01, 11/01 and design/02 newer than their indexes — their index rows still describe them, re-read), PASS.
2. *F1 fixed and tested; full gate green; remote run green* — fixed and tested above; full
   gate green (above); remote run green on `75bfd27` (run 38621264223); the driver re-checks the final HEAD.

## Open — for Giacomo

1. **Still open from P01.1:** confirm or override the slice form
   `dualc_field <graph> --slice AXIS=VALUE` (D-58), and the decisions index's 43 bytes of
   room (D-60's cell will grow after P02).
2. `include/dualc/types.h` still says parity is "Correct for closed, oriented meshes" — true
   but implies orientation matters; I left the public header alone (a comment-only edit
   rebuilds everything). design/02 now states it exactly.
3. The screening (`docs/raw/`, immutable) calls the bracket "negative"; it was mixed. 21/02
   records the correction.

## What the next unit (P02.1, T1 — `mesh(sign="gwn")`) needs to know

- Its record is **#56**, now NEXT in 21/README, 21/01 and `roadmap/README.md` (three places,
  plus the Principal-blocks row "#56 NEXT; #57–#60 PLANNED"). On landing flip all of them.
- The demo meshes are now outward, so `mesh(sign="gwn", path="torus.obj")` vs parity is a
  meaningful equality test (before this unit GWN on torus/knot was empty).
- `roadmap/README.md` is at 15,111 of 15,360 bytes; 21/01 at 14,233 — #56's landing numbers
  belong in a new child (`03-…md`), linked from 21/README's page list, as #55 did with 02.
- After a generator change, `build/examples/*.obj` now refreshes by itself (Ninja).
- Next free IDs unchanged: **#61**, **D-61**.

---

← Back to the [plan](../README.md).
