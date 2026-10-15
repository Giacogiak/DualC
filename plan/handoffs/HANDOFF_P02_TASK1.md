# Handoff — P02.1: `mesh(…, sign="gwn")` (#56)

Unit P02.1 of [`plan/11_P02_mesh-node-params.md`](../11_P02_mesh-node-params.md) § T1; session
of 2026-10-15, branch `feat/cli-consolidation` (pushed to `origin`; `main` untouched). The
record is [roadmap 21 #56](../../docs/roadmap/21-cli-consolidation/01-record.md#56-the-mesh-node-takes-signgwn)
and its evidence page [21/03](../../docs/roadmap/21-cli-consolidation/03-mesh-sign-gwn.md);
this handoff points at them and adds what the next unit and Giacomo need.

## What changed, and why

Commits: `67dfe89` (code, tests, parity case, docs), then this handoff.

- **`examples/field_graph.cpp`** `meshSignMethod`: `gwn` → `SignMethod::GENERALIZED_WINDING_NUMBER`
  instead of throwing; the unknown-value error now reads
  `mesh sign must be 'parity', 'pseudonormal' or 'gwn'` (located on the `mesh` node).
  `field_graph.h`'s `build` comment no longer lists "gwn-on-mesh" among the errors.
- **`examples/field_glsl.cpp`** `emitMesh`: the same mapping (it parses the node's params
  itself, as it already did for `normals`), so the preview bakes through
  `MeshSource::bakeToGrid` with the GWN oracle inside the bake. **The dedup memo needed no
  change**: `nodeKey` already serialises every param, so `sign` is in the key — now pinned by
  a test. (A side observation, not changed: an implicit default and an explicit
  `sign=parity` give different keys, so they bake twice — harmless, value-identical.)
- **`examples/dualc_field.cpp`** `--list`: `mesh -- signed distance to a mesh (path,
  sign=parity|pseudonormal|gwn, normals);` + a line on when each applies. `--expr` and
  `--dump-json` needed nothing (string params pass through); verified by a round trip.
- **`examples/dualc_glsl_parity.cpp`**: case `mesh-gwn` — `mesh-straddle`'s region with
  `sign="gwn"`, sign-only vs the CPU field. `scripts/check_data.json` `parity_expected_cases`
  73 → 74.
- **Tests** — `tests/test_field_graph.cpp`: new case *mesh sign=gwn is the GWN sign oracle on a
  metric field* (`[gwn]`): closed box, gwn ≡ parity (face count, vertex count, every position)
  at depth 5; open box (no `+z` face), `mesh(sign=gwn)` reads −1 at the centre vs `winding`
  ≈ −1/3, and 4 vs ≈ 0.5 at (5,0,0). In *field-graph reports located errors* the assertion
  "`sign:"gwn"` throws" **was replaced** — it encoded the guard this task removes (the plan's
  step 1); the replacement asserts an unknown value (`bogus`) under a `union` names all three
  values and carries pointer `/in/1`. `tests/test_field_glsl.cpp`: section *same mesh path,
  parity vs gwn bakes two textures*.
- **Docs** — 11/01 § Sources `mesh` row (the three signs; `gwn` survives gaps / soup but a
  hole stays a hole; `winding` is the sealing field, not a sign oracle); 11/README's error
  list; 12/03's `mesh` vs `winding` callout (it said `mesh(sign=gwn)` is rejected); 14/04 a
  new entry + 14/README row (the ABI decision below); 21/01 #56 DONE + *Landed* paragraph,
  #57 NEXT; new child 21/03 (numbers); 21/README page list + table; `roadmap/README.md`
  § Current focus, Track 1 NEXT, Principal-blocks row, milestone row for #56 (the oldest,
  #31 2026-10-06, rolled off per D-43; its record is 17/13).
- **Not edited, by the plan's own conditions:** `docs/design/02-sign-oracles.md` does not
  name the graph's sign set (it describes `SignMethod`, which already had three values), so
  no design edit — and so no `/docs-semantic-lint` obligation (no `src/` edit either). The C
  ABI pages (`capi/README.md`, `capi/dualc_c.h`, 14/02) do not list node parameters.

## The ABI version — decided: no move

The rule block 14 applied to the last vocabulary change (14/04, 2026-09-11, *unknown
parameter keys are rejected*): the create calls build through the same `buildField`; the
graph vocabulary moved, signatures / structs / codes did not → no version move. Same here:
`dualc 0.5.1` stays; an input that returned `DUALC_ERR_GRAPH` now builds. Recorded in 14/04.

## Measured, and under what

Linux, GCC 15, Ninja, Release, `build/examples/`, `--depth 6`, demo meshes as generated after
#55. Wall / peak RSS from `/usr/bin/time -f "%e s %M KB"`.

- **The equivalence (acceptance):** `dualc_demo --gwn --depth 6 torus.obj` (40,032 faces,
  1.32 s, 111 MB) and `dualc_field --expr 'mesh(path="torus.obj",sign="gwn")' --depth 6`
  (40,032 faces, 1.35 s, 112 MB): **`cmp` identical**, SHA-256 `57d29ef4…`. The same graph via
  `--dump-json` → file → `dualc_field`: identical. Also identical to both tools' *parity*
  output — expected on a closed outward mesh.
- **So it is not parity in disguise** — `data/mesh-soup.obj` (local only, 31,718 faces):
  `gwn` 30,120 faces both tools, `cmp` identical; `parity` 33,610 both tools, identical; gwn ≠
  parity.
- Before this change, the baseline `dualc_demo --depth 6 torus.obj` ≡ `dualc_field --expr
  'mesh(path="torus.obj")' --depth 6` was already byte-identical (checked first).
- GPU tier: `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict --build-dir
  build-gl` → `parity 74/74 passed`; `mesh-gwn`: sign wrong 0/3969 (216 inside), the same
  counts as `mesh-straddle`. `build-gl` re-configured and rebuilt from scratch (160 steps).
- Full `python3 scripts/check.py` (configure SKIP, `build/` configured; the six changed
  sources touched so the warnings scan is meaningful): 32 checks, 30 OK, 1 SKIP, 0 failed —
  **7 TUs compiled, 0 DualC-origin warnings, ctest 308 / 0 failed** (97.5 s; 307 + the new
  case), PASS.
- `docs_loss_audit.py --base a37c8a0 --head 67dfe89 --scope docs/` with an empty scratch
  triage: on the pages this unit edited, 6 residual rows that are not scoring noise, each
  intentional — the two "`gwn` is rejected / the `gwn`→`winding` hint" statements (now
  false, rewritten), the 11/01 `mesh` row (rewritten), the #56 / #57 status cells, the
  snapshot's NEXT clause, and #31's milestone row (rolled off per D-43). The rest are
  `0.000/1.000` scoring noise and date tokens, as P01.1 / P01.2 found.

## What I got wrong on the way

- First version of the open-shell test bounded `winding` at (5,0,0) by `< 0.5 + 1e-9`; the
  hierarchical (multipole) GWN reads 0.5025 there. Now `0.5 ± 0.05`.
- The first `--gpu` run went in the background while the docs were mid-edit; harmless (the
  `--gpu` tier runs only the parity check), but the docs tier was re-run on the final tree.

## Open — for Giacomo

1. **`mesh-gwn` cannot tell GWN from parity** — `dualc_glsl_parity` finds only `cube.obj`
   (closed), where the oracles agree. It gates that the codegen accepts `gwn` and the sign
   survives the bake; that the *right* oracle is used is pinned on the CPU side (the unit test
   and the soup run) and by the GLSL memo test. An open mesh in the parity harness would
   close that; not in this task's scope.
2. Still open from P01.2: `include/dualc/types.h` says parity is "Correct for closed,
   oriented meshes" (orientation does not matter); left alone.

## What the next unit (P02.2, T2 — per-node `bake`) needs to know

- Its record is **#57**, now NEXT in 21/README, 21/01 and `roadmap/README.md` (§ Current
  focus, Track 1, the Principal-blocks row "#55–#56 DONE; #57 NEXT; #58–#60 PLANNED").
- **Byte room:** 21/01 is at 14,979 of 15,360 — #57's landing paragraph must be short, its
  measurement table (the `dualc_boolean` decision's evidence) belongs in a new child
  `04-…md` linked from 21/README's page list; `roadmap/README.md` at 15,065.
- `emitMesh` parses `sign` / `normals` itself (it does not call `meshSignMethod`, which is
  file-local in `field_graph.cpp`); a new `bake` / `band` param must be read on both paths,
  and `rejectUnreadParams` runs on both, so a param read on one side only fails the other.
- `nodeKey` keys the memo on every param, so `bake=N` vs no `bake` never share a texture.
- `parity_expected_cases` is **74**; T2 adds `mesh-bake` → 75.
- Next free IDs unchanged by this unit: **#61**, **D-62** (D-61 was used at the P01 gate).

---

← Back to the [plan](../README.md).
