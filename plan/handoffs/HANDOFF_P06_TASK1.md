# Handoff — P06.1: the ABI test and entry, the command reference, the design page

Unit P06.1 of [`plan/15_P06_abi-and-docs.md`](../15_P06_abi-and-docs.md) § T1, the last (and
only) unit of P06. Session of 2026-10-22, branch `feat/porous-shell` in
`~/Documents/DualC-porous`. Nothing was pushed and no remote was touched. `main`,
`feat/hygiene-and-editor` and `~/Documents/DualC` were left alone. Boletus was not touched.

**Commits:**

- `82183c7`: `cli_c_abi_porous` — the `porous` mode of `capi/dualc_c_demo.c` and its ctest.
- `f2fe93b`: the docs (below), #63's detail moved to 23/09, #67 DONE.
- The commit that adds this handoff.

**The record:**

- [23/02 #67](../../docs/roadmap/23-porous-shell/02-record.md#67-the-c-abi-test-and-the-documentation)
  is **DONE** 2026-10-22, with a T1 append.
- The block README's table and the three places of the roadmap index read
  `#63–#67 DONE, #68 PLANNED`.

## What changed, and why

| File | What |
| --- | --- |
| `capi/dualc_c_demo.c` | mode `porous`: an in-memory cube (`cube.obj` loaded into buffers) as `mesh(id="cube")`, used twice (the volume and the clip); the shorthand (`…_from_expr_with_meshes`) and the JSON (`…_from_json_with_meshes`) of `intersection(mesh(id), onion(porous(mesh(id), spacing=0.25), thickness=0.03))`, each contoured at depth 6 with the diagnostics (non-empty, 0 boundary, 0 non-manifold, no issue) and compared byte for byte; the plain `dualc_field_create_from_expr` over `smooth-intersection(sphere, onion(porous(sphere, spacing=0.4, seed=3), thickness=0.06), k=0.1)` (P04's watertight test graph), watertight; two located `DUALC_ERR_GRAPH`s (no `spacing`; `spacing2` without a control); a pre-requested token cancels the contour; a token requested in the CONTOUR stage cancels an export, no file and no `.part`; then the export succeeds |
| `capi/CMakeLists.txt` | `add_test(cli_c_abi_porous … porous)` |
| `capi/README.md` | new § *The porous source from a host mesh*: the C call, the JSON form, the build inside the create call (not cancellable, `relax` on all threads), the errors, determinism |
| `docs/roadmap/14-c-abi/04-abi-0-4-0.md` + its README row | the entry *The `porous` source (vocabulary widening, no ABI version move)*, at the end (date order, after #62) |
| `docs/command_reference/11-dualc_field/16-porous-shell.md` (new) | synopsis, the 12-key table with the parser's defaults (`readPorousParams`), what the value is, ceilings/errors, the four recipes, advice from 23/06, the cost guide, the ABI and viewer notes, recipes V1–V9 |
| `11-dualc_field/README.md` | page row, recipe row V1–V9, the samples line points at 16. To stay under the 15,360-byte cap: one duplicated "(monolithic only)" and "Global" in the `--decimate` row merged, and the footer's "Live preview" link dropped (the § Live GPU preview section keeps it) |
| `11-dualc_field/01-op-vocabulary.md` | the `porous` row points at 16 |
| `12-dualc_field_view/02-nodes-and-examples.md` | the node in § Supported nodes, a paragraph on its preview (one uniform, rebuild on other keys, the nudge caveats, the fallback, the recompile message), recipes P17 (live) and P18 (snapshot) |
| `docs/command_reference/README.md` | feature-map row and inventory row. To fit the cap: "; the number is never reused" removed from the eight retired-tool rows and said once in the paragraph below; "the size contract applies to every page" dropped |
| `docs/design/12-field-graph.md` + `design/README.md` row | § *The porous source*: build-time volume and `childBakeMargin`, 1-Lipschitz, the bin bound and walk cap, determinism, ceilings, no cancel, one builder + the texture layout |
| `docs/roadmap/23-porous-shell/09-seeds-as-built.md` (new) | #63's four detail paragraphs and the seeds-per-second table, **moved verbatim** from 02 (02 was at 15,357 / 15,360); 02 keeps the acceptance line and a summary |
| `STRUCTURE.md` | the `dualc_c_demo.c` line names its modes, `porous` included. The two new `.md` pages are covered by the one-line docs entries (`structure_summarized_prefixes`) |

No `docs/README.md` change: no new folder.

## Measured, and under what

This host (22 GB, 8 cores), GCC 15, Release, one process at a time, ad-hoc runs under
`prlimit --as=8000000000`, time and peak RSS from `/usr/bin/time`.

- **`cli_c_abi_porous`:** 6.3 s, **0.45 GB** peak (under the 1 GB rule). In-memory cube:
  72,092 verts / 144,140 tris, watertight, JSON == shorthand byte-identical; sphere: 40,086 /
  80,116 (the same count as P04's library test), watertight. Export 7,207,084 bytes.
- **`ctest --test-dir build -R c_abi`: 6/6 passed**, 24.0 s, sequential.
- **The recipes of 11/16,** run today with `dualc_field` from `build/examples` (faces; NM =
  the CLI's non-manifold warning):

  | Recipe | Depth | Faces | Closed | Time, peak |
  | --- | --- | --- | --- | --- |
  | V2 uniform foam, sphere, S 0.4, t 0.06, k 0.1 | 7 | 322,880 | watertight | 5.6 s, 2.9 GB |
  | V4 skinned sphere (S 0.25, t 0.05, skin 0.05) | 7 | 449,656 | **8 NM** | 5.4 s, 2.9 GB |
  | V6 graded foam (spacing 0.2 → 0.4 over r 0 → 1) | 7 | 386,132 | watertight | 5.9 s, 2.9 GB |
  | V7 Delaunay walls, `smooth` 0.06, k 0.1 | 7 | 439,348 | watertight | 5.1 s, 3.1 GB |
  | V8 sponge sample, `--depth 8 --mem 2G` (tiled, 125 tiles of tile-depth 6) | 8 | 1,280,976 | (tiled STL, not checked) | 53.5 s, **0.44 GB** |
  | V9 sponge sample `--slice z=0` | — | 5,648 contour segments | — | 0.19 s; I looked at the PNG: Voronoi walls, closed cells, open pores at the rim |

  V1, V3, V5 are the three samples; their numbers are P04's (23/06 § The samples).
- **Skinned variants at depth 6** (all 0.9–1.0 s, 0.42 GB): the skinned sphere 16 NM; with a
  `smooth-union` k 0.03 7 NM; clipped at r 0.97 inside the skin: 16 NM (same face count — so
  the edges are not at the clip); intersection outside the union: 9 NM. Graded V6 at depth
  6: 3 NM; Delaunay-strut bone with a 0.03 skin at depth 6: 10 NM. Not chased further: 23/06
  already records the sphere's skinned cases at 8–120 NM; it is the resolution rule at
  wedges, as 23/06 finding 1 says.
- **Bake as a seeding speed-up (#63's lead):** on the bracket (a light mesh), `porous(mesh,
  spacing=0.05|0.03)` sliced at 64²: 0.12 / 0.49 s unbaked, 0.80 / 1.15 s with `bake=128`.
  The bake costs more than it saves on a light mesh; not advertised in 11/16.

## Checks run

- `cmake --build build -j`: no warnings from the changed targets.
- `ctest --test-dir build -R c_abi`: 6/6.
- `python3 scripts/check.py --docs` on both commits (pre-commit hook): 27 OK + 1 INFO, PASS.
- `python3 scripts/check.py --docs --strict`: PASS (run after the docs commit; the INFO
  `index-staleness` list is report-only and not failed by `--strict`).
- `scripts/docs_loss_audit.py --base 82183c7 --head f2fe93b --scope docs/` for the 02 → 09
  move and the trimmed rows: no loss on the changed pages (§ The loss audit below).
- **Not run** (P08's, by the run's rule): the full gate, the GPU tier, the semantic lint.
  The driver's smoke filter (`porous|field_graph|field_glsl`) does not include
  `cli_c_abi_porous`; the gate's ctest does.

## The loss audit

`python3 scripts/docs_loss_audit.py --base 82183c7 --head f2fe93b --scope docs/` (about 4 min
per run on this host):

- **Whole scope, no triage file:** 10,649 untriaged rows, exit 1. They come from the whole
  base tree (roadmap 19, `docs/raw/` cross-references and the like), files this unit did not
  touch; the audit lists every residual of the tree when no triage file is given.
- **The four pages text left** (`--source` each of `23-porous-shell/02-record.md`,
  `command_reference/README.md`, `11-dualc_field/README.md`, `11-dualc_field/01-op-vocabulary.md`,
  `--untriaged`, filtered for `gone`, `partial` and `rewritten`): **no row.** #63's
  paragraphs are found verbatim in 23/09, and the trimmed phrases are found in their
  remaining copies.

**My mistake:** I ran the audit after the docs commit, not before, chained in the commit's
own background command, and returned once to the driver before it had finished. The result
above is the audit of that commit; nothing needed restoring.

## What I got wrong on the way

- **I first placed the 14/04 entry before #62,** which is dated 2026-10-20; moved it to the
  end, in date order.
- **I first wrote "on any OS" for the contoured mesh** in `capi/README.md`. Only the seeds,
  tets and packed arrays are pinned bit-identical across OSes; the contour across compilers
  is not claimed. Corrected to "same cells".
- **I first wrote V2's row from 23/06's S 0.25 foam (423,624 faces)**; my V2 is S 0.4,
  t 0.06. I ran it: 322,880.
- **"since" in the design page** failed `design-no-history`; reworded.

## The in-run choices

Each is **Chosen in-run (pending owner review)**, also listed in 23/02 #67:

1. **#63's detail moved to a child page 23/09**, not 02 split into a folder: 02 was 3 bytes
   under the cap with #67 and #68 still to land; every other item already keeps its numbers in
   a child, and no anchor pointed inside the moved paragraphs (they are bold labels, not
   headings), so no link changed.
2. **The ABI version stays `dualc 0.5.2`.** Block 14 has no written version rule other than
   the one its entries apply (vocabulary moved, signatures/structs/codes did not → no move);
   that rule was applied as written. The raw plan's "no ABI version move" agrees.
3. **"The expression form"** was read as both (a) the shorthand vs JSON of the in-memory
   graph and (b) the plain `dualc_field_create_from_expr` over an analytic volume; the test
   covers both.
4. **The cancel case** cancels contour and export; the create call cannot be cancelled
   (`FieldGraph::build` has no token, 23/05) — documented in 14/04, `capi/README.md`, 11/16
   and design/12, not changed.
5. **`relax` ignores `--threads` / `numThreads`** (the graph never sets
   `SeedParams::numThreads`, so Lloyd uses every hardware thread). Documented, not changed:
   the output is identical at any count, and widening `--threads`' meaning would need its
   option row and `field_graph.h`'s `bakeThreads` comment changed too. A follow-up for the
   owner, beside #62.
6. **The skinned recipe (V4) is listed with its 8 non-manifold edges** at depth 7 rather than
   replaced by a contrived watertight variant; none of four variants closed it at depth 6.
7. **Recipe letter `V`** for the porous recipes (unused in 11; `P` is 11/12's primitives and
   12's viewer recipes).
8. **Trims to fit two pages under the cap** (11/README, command_reference/README), listed in
   the table above; no fact removed, only duplicated wording.

## Exit criteria of P06, one by one

- **T1 acceptance — `ctest --test-dir build -R c_abi` is green with the new case:** 6/6
  passed, `cli_c_abi_porous` included (24.0 s total, sequential).
- **T1 acceptance — `python3 scripts/check.py --docs --strict` is green:** PASS, 27 OK +
  1 INFO (report-only), 0 failed.
- **"A Boletus developer can use the node through the ABI from the docs alone":**
  `capi/README.md` § The porous source from a host mesh gives the C call with an in-memory
  mesh, the JSON form, what the create call does (build-time volume, buffers freeable,
  not cancellable, threads), the error class and determinism, and links 11/16, which has
  every key with the parser's default, what the value means, the ceilings, the four recipes
  as graphs, the advice and the costs. 14/04 states the version consequence (none). The
  demo's `porous` mode is a compiled C example of exactly that usage. Checked by reading
  the pages against `readPorousParams` and `--list`, and by the ctest, not by a Boletus
  developer — P07 (the Boletus plan) is the first consumer.
- **The plan's T1 items:** ABI test ✓, cancel case ✓, 14/04 entry ✓, `capi/README.md` ✓;
  11/16 with the real defaults, recipes ready to run from `build/examples/`, the cost guide
  ✓; rows in 11/README and 01 ✓; the node in 12/02 ✓; the feature map ✓; design/12 ✓;
  `STRUCTURE.md` ✓ (every file of the run is covered: the structure check reports 0
  undocumented); no new folder, so no `docs/README.md` change.

## For the owner's review

**needsHumanCheck is raised**, for the viewer recipes only (everything else was checked
headless). The display was hung from P05.1 when P05.2 ended and I did not touch the GPU.

1. After the reboot P05.2 asks for, and after its step 2 has shown the NV92 survives one
   porous graph:
   ```bash
   cd ~/Documents/DualC-porous/build-gl/examples
   timeout 60 ./dualc_field_view porous-sponge.json --snapshot /tmp/p18.png     # P18
   ./dualc_field_view --expr "smooth-intersection(sphere(radius=1),onion(porous(sphere(radius=1),spacing=0.3,smooth=0.02),thickness=0.04),k=0.1)"   # P17
   ```
   P18: compare with `build/porous-snapshots/porous-sponge.png`. P17: `tab` to `smooth` and
   nudge (corners round, no recompile), to `thickness` (walls thicken), to `k` (the rim
   softens).
2. **Read 11/16 as a user would**, especially the cost guide's wording and whether V4 (8
   non-manifold edges) belongs on the page.
3. **Optional:** open `/tmp/p061/sponge8.stl` (depth 8, tiled; gone after a reboot — re-run
   V8) in a slicer.

**Verified headless:** the ABI test, the recipes' face counts and closure, the slice PNG
(looked at), the docs gate.

## What the next unit needs to know

- **P07 (Boletus plan):** the DualC commit to pin is this branch's tip after this handoff;
  the `Ops.cs` key table is 11/16 § Parameters (the parser's defaults); the ABI facts are in
  `capi/README.md` § The porous source. The build cannot be cancelled and `relax` ignores the
  host's thread count — Boletus's plan should say so (a D-NN on its side, or wait for a
  create twin with a token / thread count: an ABI minor, as #62 says).
- **Sizes:** 23/02 is at 12.6 KB after the move (room for #68 and P08's appends);
  `docs/command_reference/README.md` 15,305 and `11-dualc_field/README.md` 15,319 are near
  the cap; `docs/roadmap/README.md` 15,288.
- **`cli_c_abi_porous` is not in the driver's smoke filter**; it runs under `-R c_abi` and in
  the full gate (P08).
- The ad-hoc outputs are under `/tmp/p061/` (not committed).
