# Handoff — P07.1: harvest, close block 21, hand over the PR

Unit P07.1 of [`plan/16_P07_close.md`](../16_P07_close.md) § T1, the last unit of the plan;
session of 2026-10-16, branch `feat/cli-consolidation` (pushed to `origin`; `main` untouched).
The record is [roadmap 21/09](../../docs/roadmap/21-cli-consolidation/09-plan-run.md) — the
plan's run harvested, the close's lint, loss audit and gates. Block 21 is **closed**; the
branch is ready for the owner's PR. `plan/` is **not** deleted (the driver still needs it; D-51's
deletion is one commit after the driver marks P07 done).

## What changed, and why

Commit `13a6de2` (docs only), then this handoff. No code, test, CMake or workflow file touched.

- **21/09, new** — the harvest of the eleven handoffs: the green run of every unit, the counts
  the run moved (ctest 306 → 344, parity 73 → 82, ABI `dualc 0.5.1` unchanged), the three owner
  reviews (D-58 form + D-61; D-60 + D-62; D-63 + D-64), the plan files' corrections (P01.1 F2,
  P01.2 the mixed bracket, P02.2 the bake region / `band` on `winding` / page 03, the P02 gate's
  "seven", P03.1 11/09, P04.1 the eight refusals, P05.1 recipe letters, P06.1 `--clean`), the
  lessons, the measurements only the handoffs held (parity `mesh-gwn` 0/3,969, `mesh-bake`
  2.72e-4, `mesh-bake-smooth` 5.70e-4, the viewer's two-texture line, R3 / R4 135,308 / 172,060,
  `onion(torus)` 38,960), the *Open after the run* table (17 rows, each with a home), and the
  dated close entry. 21/README: page line + a status row (DONE, anchored, status-sync checks it).
- **`roadmap/README.md`** — § Current focus: block closed 2026-10-16, the PR is the owner's,
  **NEXT** is the rest of #34 after that PR; Track 1 item 1 DONE, item 2 **NEXT**; milestone row
  *Block 21 closed* (the #56 row left the five-row window, D-43; its record is 21/03);
  principal-blocks row 21 **DONE (2026-10-16)**. 15,123 / 15,360 bytes.
- **Decisions** — D-55 … D-58's status cells say *executed* (P05 / P06, #56, #58, #59 + #60);
  D-40's and D-33's rows record their **met triggers** (below), the owner's call.
- **`/docs-semantic-lint`**, delta run [19/08 run 07](../../docs/roadmap/19-docs-layers/08-semantic-lint/07-2026-10-16-after-cli-consolidation.md)
  (run by a sub-agent, its findings verified against the code before fixing): (a) 3 · (b) 1 ·
  (c) 2 · (d) 0 · (e) 2. Fixed: **new `docs/design/12-field-graph.md`** (one reader for builder
  and codegen; per-node bake — region, band, `childBakeMargin` op by op, ceiling 512; the 2D
  typing rule; what the preview cannot show — unbaked mesh re-bake at `--grid-res`, polygon
  ceiling 64, `vec2` bindings), indexed in `design/README.md`; design/09 scopes the `-o`
  dispatch and `.part` to mesh exports and states `--slice`'s output rule; design/10's
  *Preview equals export* names `field_glsl` and links design/12, `.part` scoped to mesh
  exports, the `padFraction` row names `--slice`'s region (design/10 trimmed to 15,353 / 15,360);
  11/01 and 11/09 link design/12; 11/01 no longer explains `gwn` by `dualc_demo --gwn`;
  00-shared-behaviour "the six above" → "the two above". Folder README row added.

## Measured, and under what

Linux, GCC 15, CMake 4, Ninja, Release; GL renderer NV92.

- `python3 scripts/check.py --clean` (step 5 as written), on `f5dcf0f`'s tree: 32 checks, 30
  passed, 1 SKIP — **warnings: 0 TUs (incremental)**, as P06.1 found: `--clean` reconfigures in
  place, it does not rebuild. So: `cmake --build build --target clean && python3 scripts/check.py
  --build` → build 245 s, **251 TUs compiled, 0 DualC-origin warnings, ctest 344 / 0 failed**
  (128 s). Code unchanged since (docs only).
- `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict --build-dir build-gl` →
  `ninja: no work to do` (P06.1's rebuild is current), **parity 82/82**.
- `python3 scripts/check.py --docs --strict` after the last docs edit: 28 checks, 27 passed,
  1 INFO (index-staleness, report-only, 15 rows), 0 failed.
- Full `python3 scripts/check.py` on the final tree and the remote run: § Gate and remote run.
- **Loss audit of `plan/`** — `python3 scripts/docs_loss_audit.py --base HEAD(f5dcf0f) --head
  <scratch> --scope docs/ plan/ --annex plan/ --triage /tmp/<scratch>.json --write-dump …`;
  the head a `git commit-tree` of the final docs with `plan/` removed from a copied index (no
  branch, HEAD untouched); scratch triage = audit key `f5dcf0f`, the script's thresholds, no
  items or rules. ~7 min a run, run twice (before and after the lint fixes). Final: annex
  sentences 742 / 759 residual, evidence 435 / 733, headings 119 / 119, identifiers 112 / 1,392;
  docs side 6 real residual rows on the 14 changed pages, all intentional (21/09 lists them); the
  rest ≈ 11,100 rows of unchanged files. Triage in 21/09; the dump not kept (as 20/04). The
  handoff-only measurements the evidence tier surfaced were added to 21/09 before the second run.
- D-40's trigger, measured: `git ls-files docs/raw` minus `.svg` / `.html` → 29 files,
  1,149,176 bytes (> 1 MB).

## What I got wrong on the way

- design/10 went over the byte cap (15,499) with the lint fixes; condensed the *Preview equals
  export* paragraph (its "opt-in" and "the field on screen is the field contoured" folded into
  shorter words) to 15,353 — the loss audit lists the two rows.
- My first docs-side filter of the audit flagged 19/08's README rows as residual: they are
  pre-existing raw-only noise (the 2026-09-20 dump quotes them), present on the first run when
  that file was unchanged.
- 21/09's first draft pointed the 12/05 interactive-check row at 21/07, which never recorded
  that check; it points at 21/09 now.

## Exit criteria of P07, one by one

- **Block 21 closed** — 21/09 § *The consolidation plan's run* DONE 2026-10-16; 21/README row;
  `roadmap/README.md` (Current focus, Track 1, milestone, block row); every item #55–#60 DONE and
  every decision D-55 … D-64 decided (`status-sync` 27 rows, 0 disagree; `decisions-index` 0
  problems).
- **The branch ready for the owner's PR** — full gate, GPU tier and `--docs --strict` green
  locally (above); remote run § below; compare link and PR text below.
- T1's acceptance: *plan/'s facts harvested* — 21/09 + the loss audit (above); *every gate green
  locally and on the remote run* — above / below; *the handoff carries the compare link and the
  PR text* — below.

## Gate and remote run

- Full `python3 scripts/check.py` on the final docs tree (`13a6de2`): 32 checks, 29 passed,
  2 SKIP (configure: configured; warnings: 0 TUs, docs-only — the meaningful scan is the
  251-TU run above), 0 failed; **ctest 344, 0 failed** (92 s).
- Pushed `13a6de2`; `python3 plan/remote_run_check.py` was still polling its Actions run when
  the unit had to return (no result read — not claimed green). This handoff's commit (plan
  only) is pushed after it; the driver's remote-run check reads that HEAD's run.

## The PR

Compare link: <https://github.com/Giacogiak/DualC/compare/main...feat/cli-consolidation>

**Title:** CLI consolidation — one framework, `dualc_field` + `dualc_field_view` (block 21, #55–#60)

**Description:**

> The user surface becomes the two framework CLIs, `dualc_field` + `dualc_field_view`: every
> capability a side tool alone held moves into the field graph — and so within reach of the C
> ABI, which is the graph — and seven side CLIs retire (D-55 … D-58). Record: roadmap block 21
> (`docs/roadmap/21-cli-consolidation/`), its run harvested in 21/09.
>
> **What changed**
> - **#55** — six `dualc_gen_demo` meshes were inside out (the bracket mixed); all wound
>   outward, tested by signed volume and GWN; mesh copies next to the CLIs refresh on regeneration.
> - **#56** — `mesh(…, sign="gwn")`: the GWN sign oracle in the graph (CPU + GLSL).
> - **#57** — per-node `bake=N[, band=W]` on `mesh` / `winding`: preview == export for a baked
>   node; a baked node inherits its ancestors' margin (smooth `k`, offsets, ÷ scale — D-62).
> - **#58** — 2D profiles `circle`, `rect`, `segment`, `polygon` under `revolve` / `extrude`,
>   CPU + GLSL (`vec2` uniform bindings; polygon ≤ 64 vertices in the preview).
> - **#59** — `dualc_field <graph> --slice AXIS=VALUE [--res N]`: any graph sampled on a plane
>   to PNG + SVG; contour-only flags refused (D-64).
> - **#60** — docs migrated to the framework pages (11/09–11/14, 12/05), six sample graphs in
>   `examples/samples/`, the tools' code deleted, every retired test twinned through
>   `dualc_field`, pages 01–07 and 10 tombstones; new `docs/design/12-field-graph.md`.
>
> **Retired tools and their replacements**
>
> | Retired | Replaced by |
> | --- | --- |
> | `dualc_demo` | `dualc_field` + `mesh(path=…, sign=parity\|pseudonormal\|gwn, normals=…)` / `winding(…)` (11/11) |
> | `dualc_primitive` | `dualc_field` primitive / decorator / domain-op nodes (11/12) |
> | `dualc_lift` | `revolve(profile, offset=…)` / `extrude(profile, height=…)` (11/09) |
> | `dualc_csg_demo` | the six graphs in `examples/samples/` (11/13) |
> | `dualc_lattice` | `intersection(onion(tpms(…), thickness=…), mesh(…))` with the defaults written out (11/14) |
> | `dualc_slice` | `dualc_field <graph> --slice AXIS=VALUE` (11/10) |
> | `dualc_raymarch` | `dualc_field_view` (12/05); `DUALC_BUILD_RAYMARCH_VIEWER` removed |
>
> `dualc_boolean` stays (D-60), beside the graph's `bake=`; `dualc_gen_demo`, `dualc_glsl_parity`,
> `dualc_io_stress` stay (D-59).
>
> **Counts** — ctest 306 → **344** (0 failed; the retired tools' 39 cases out, their twins in);
> GLSL parity 73 → **82** / 82; 251 TUs, 0 DualC-origin warnings. **ABI: `dualc 0.5.1`,
> unchanged** — the graph vocabulary widened, no signature, struct or code moved (14/04). Hosts
> that upload bindings themselves must handle the new `vec2` type (DualCEditor's
> `trace_renderer.cpp` needs one `glUniform2f` line on re-pin).
>
> 🤖 Generated with [Claude Code](https://claude.com/claude-code)

## Open — for Giacomo

1. **D-40's trigger is met** (`docs/raw/` 1.15 MB of text, over 1 MB): build the FTS5 index, or
   re-defer with a new trigger. Recorded on the row.
2. **D-33's trigger is met in substance** (#57's clamp reads gave a wrong result
   `gridBoundsExceeded` did not catch); D-62 prevents the read, counting is still not taken —
   keep DEFERRED with a reworded trigger, or reopen. Recorded on the row.
3. The two "closed, oriented" comments (`include/dualc/types.h`, `src/internal/sign_oracle.cpp`)
   — a comment edit in `src/` + `include/`, left out of the close.
4. `--slice` writes PNG / SVG in place (no `.part`): documented; routing it through
   `AtomicOutput` is a code change if you want the invariant there too.
5. Carried from the units (21/09 § Open after the run): the keyboard-map heading, D-14 dropped by
   a unit, the twins' floors vs exact counts, the SECTION name, M4's wall, `vec2` in DualCEditor,
   the polygon ceiling, nested arrays read flat, GPU bakes > 256, `mesh-gwn`'s discrimination,
   CI's cold Eigen fetch, #61's specks, 12/05's hotkeys in a live window.

## What the next session (the `/plan-run` hand-back) needs to know

- Delete `plan/` in one commit after the driver marks P07 done (D-51), with
  `scripts/docs_loss_audit.py` run first — its audit is 21/09's (the deletion changes no docs
  text, so the result stands; re-run it if anything under `docs/` moved since `13a6de2`).
- `roadmap/README.md` 15,123 / 15,360; 21/01 15,354 (no room — later facts go to 21/09 or a new
  child); design/10 15,353 (none); `decisions/README.md` ~14,000.
- Next free IDs unchanged: **#62**, **D-65**. The next full semantic-lint run is due by
  2026-11-07 (this one was a delta).

---

← Back to the [plan](../README.md).
