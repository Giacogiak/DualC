# Handoff — P06.2: tombstones, indexes, the loss audit; #60 DONE

Unit P06.2 of [`plan/15_P06_retire-tools.md`](../15_P06_retire-tools.md) § T2, the last unit of
P06; session of 2026-10-16, branch `feat/cli-consolidation` (pushed to `origin`; `main`
untouched). The record is [roadmap 21/08 § T2 and § The loss audit](../../docs/roadmap/21-cli-consolidation/08-retire-tools.md#t2--the-tombstones-the-indexes-the-loss-audit);
[#60](../../docs/roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools) is
**DONE 2026-10-16** with "ctest 358 → 344" on its status line, in 21/README's table, in the
roadmap index's milestones (the #55 row left the five-row window) and in the block-21 row.
Seven tools, not eight: `dualc_boolean` stays (D-60).

## What changed, and why

Commits: `0efb96e` (every docs change, the record), `fc98e5a` (the audit report in
`docs/raw/`, its index row, the record's link to it), then this handoff. No code, no test, no
CMake, no workflow file touched.

- **Tombstones** — pages 01, 02, 04, 05, 06, 07, 10 rewritten in `08-dualc_view.md`'s form: the
  H1 kept (anchors are links), *Retired 2026-10-16*, the decision (D-55 / D-56 / D-57 / D-58),
  21/08 + #60 as the record, and a *Was / Now* table sending every old section to its home under
  11 or 12 (P05's heading map). Each says its number is never reused. The two inbound links to
  old anchors were re-pointed (03 → 11/11 § Sharp vs. smooth normals; the README's
  `10#live-controls` → 12/01 + 12/05).
- **`docs/command_reference/README.md`**: tool index (seven rows *Retired 2026-10-16 (#60)* with
  their new home), "Four tools ship: 3, 9, 11 and 12"; build prelude's opt-in list (FIELD_VIEW,
  GLSL_PARITY); keyboard map one column (`dualc_raymarch`'s column and its four lattice rows
  out); common-options matrix down to `boolean`, `gen_demo`, `field`, `field_view` — the `view`
  column #50 left behind went too; the resolution note's `--onion` example as `onion(…)`;
  export list (`dualc_field`, `dualc_boolean`); inventory (4 tools, `rect`, decorators by node
  name, *Composition samples* instead of *Baked recipes*).
- **Other live pages**: 00 (warnings list, `sign=gwn`); 08's *Now* cell; 09's G2 / G3 / G5 as
  `dualc_field` graphs and its orientation sentence; 11/01, 11/04, 11/09–11/14, 11/README
  (§ Scope now names the two framework CLIs, D-55; `dualc_boolean` remains, D-60), 12/README,
  12/03, 12/05 — "the graph form of X" → "replaced X, retired 2026-10-16", present tense → past;
  `README.md` (tool list rebuilt around `dualc_field`, a sentence on the seven retired),
  `STRUCTURE.md` (seven `.cpp` rows, the target list, the GL list, "CSG recipes" out of the
  registry line), `AGENTS.md` (opt-in list), `THIRD_PARTY.md` (two lines), design/09 (`.obj`
  writers; the viewer rule now lists the one viewer, letters `r`, `l`, the arrows, `[` / `]` —
  `t`, `n`, the Page keys and `-` / `=` were `dualc_raymarch`'s, checked in
  `dualc_field_view.cpp`; the primitive-order link → 11/12).
- **Decisions**: #18's trigger reads "a TPMS lattice in a mesh (11/14; was `dualc_lattice`)";
  **D-14** (`dualc_lattice --tile-depth` wiring) moved from the README to `02-dropped.md`,
  *DROPPED 2026-10-16 — the tool retired, its trigger can no longer fire*; 11/03 § 5a.3 got a
  dated *Dropped* line; the roadmap index's Track 1 item 4 says so.
- **Left as they are, on purpose**: `scripts/docs_loss_audit.json` (the pages appear only as
  base-tree `source`s of the 3d73220 audit; no `record` cites them, so nothing there resolves
  against the tombstones); the roadmap records and the D-55…D-63 rows (history); code comments
  that already say *retired* (P06.1); `.claude/settings.local.json` (local permissions).

## Measured, and under what

Linux, GCC 15, CMake 4, Ninja, Release, `build/examples/`.

- **09's rewritten recipes run**: G2 `mesh(path="torus.obj")` depth 6 → 40,032 faces, G3
  `normals=sharp` on the bracket → 34,984 (both = the T1 twins' counts), G5
  `winding(path="bunny.obj")` depth 7 → 60,180. 11/13's K1 file → 164,268 (= 21/07);
  `onion(torus,thickness=0.35)` at depth 6 parses and contours (38,960).
- **Loss audit** — the full account is 21/08 § The loss audit and
  [`docs/raw/2026-10-16-loss-audit-2fcb6ee-p06.txt`](../../docs/raw/2026-10-16-loss-audit-2fcb6ee-p06.txt).
  `python3 scripts/docs_loss_audit.py --base 2fcb6ee --scope docs/ --triage <scratch>`: first on
  a scratch commit of the working tree (`git commit-tree` with a separate index, HEAD untouched,
  never a branch), **before** the rewrites were committed; then on the committed `0efb96e` —
  identical counts. Tree-wide: 9,207 residual rows on the scratch head, 8,797 on `8b154e1` (the
  pre-T2 baseline, same command) — unchanged-file noise, as P05 found. **Audited set: 437 rows**
  (every row on the eight pages: 391, P05 had 396; plus every row not residual on the
  baseline): **class A 393** (superseded by its P05 home — 36 rules, one per page × unit ×
  bucket, `max_matches` = the count), **B 6** (P05's recorded drops: `dualc_raymarch`'s thin-wall
  note and missing-mesh message, D9 / D10's nonexistent files), **C 36** (live pages rewritten in
  place, the D-14 row, the #55 milestone row), **D 2** (noise: an identifier and a `10mm`
  evidence token of the 2026-09-20 dump / roadmap 19's phase-6 record, "kept" before only via a
  retired page or a deleted `.cpp`). **0 untriaged**, every citation resolved (record + quote),
  exit 0. ~4 min a run.
- **Full gate** `python3 scripts/check.py` on `0efb96e`'s tree: 32 checks, 29 passed, 0 failed,
  2 SKIP (configure: already configured; warnings: 0 TUs, docs-only); **ctest 344, 0 failed**,
  102 s. No GPU tier owed (no field node, `field_graph.cpp`, `field_glsl.cpp` or parity case
  moved) and none run.
- `python3 scripts/check.py --docs --strict` after the last docs edit: 28 checks, 27 passed,
  1 INFO (index-staleness, report-only, 14 rows, all pre-existing pairs or this branch's), 0 failed.

## What I got wrong on the way

- My first audit run cited a record section that was not yet in the scratch commit; caught
  before it finished (it would have failed every citation), re-run on a second scratch commit
  that held 21/08's T2 section.
- 21/01 crossed the 15,360-byte cap (15,366) with the DONE line; shortened the line's links to
  15,354.
- A first triage gave shared keys (the same unit on a retired page and on the 2026-09-20 dump)
  the *noise* verdict; items apply to every row of a key, so those keys are now class A
  (superseded) on both, which is true of both.
- The keyboard-map heading kept `dualc_raymarch` in its text (below) — a choice, not a slip.

## Exit criteria of P06, one by one

- **No retired binary, target, option or live doc reference remains.**
  - *Binary*: the six stale binaries P06.1 kept in `build/examples/` for its byte comparisons
    are deleted (`build-gl/examples/` holds `dualc_boolean`, `dualc_field`, `dualc_field_view`,
    `dualc_gen_demo`, `dualc_glsl_parity` only); P06.1's fresh-tree build proved no target makes
    them. *Target / option*: `git grep -E "dualc_(demo|primitive|lift|csg_demo|lattice|slice|raymarch)\b|RAYMARCH"`
    over `*CMakeLists.txt`, `*.cmake`, `.github/`, `scripts/*.py`, `scripts/check_data.json`:
    no hit. `build/`'s cache lost its stale `DUALC_BUILD_RAYMARCH_VIEWER` entry
    (`cmake -U`); one docstring of the old `DUALC_POLYSCOPE_DIR` entry still mentions it
    (a local, pre-#50 cache line; no CMake file reads it).
  - *Live docs*: the same grep outside `docs/roadmap/`, `docs/raw/`, `plan/` hits only the
    tombstones, *Was* columns and past-tense lines naming what a page replaced, the decision
    rows (history), code comments already saying *retired* (P06.1), and two kept names: the
    keyboard map's heading `## GPU viewer keyboard map (`dualc_raymarch` & `dualc_field_view`)`
    (headings are never rewritten — design/09 links its anchor; the body says it is one viewer
    now) and `tests/test_field_graph.cpp`'s SECTION "defaults are dualc_lift's" (a passing
    test, P06.1's open item 2).
- **Every path they covered is tested through `dualc_field`**: P06.1's twin table (21/08
  § The tests); today's gate ran all 344 cases, 0 failed.
- **Remote run green**: run 38877047030 on `fc98e5a`, success (below).

## Remote run

`python3 plan/remote_run_check.py` on `fc98e5a` (both docs commits): run
[38877047030](https://github.com/Giacogiak/DualC/actions/runs/38877047030) — completed,
**success**; the script printed `OK branch feat/cli-consolidation HEAD fc98e5a green` and exited 0.
This handoff's own commit (plan only) is pushed after it; the driver's remote-run check reads
that HEAD's run.

## Open — for Giacomo

1. **The keyboard-map heading** still reads "(`dualc_raymarch` & `dualc_field_view`)": the
   rule is that a heading is never rewritten. If you would rather have it say only
   `dualc_field_view`, the cost is one anchor change (design/09 links it) — your call.
2. **D-14 dropped by me**, not by you: its trigger named the retired tool, so it could never
   fire. If you would rather keep it open with a reworded trigger ("an author who needs a
   one-flag tiled lattice export"), move the row back.
3. From P06.1, still open: the stronger `_same` equivalence (exact counts on Linux, or stored
   reference OBJs); the test SECTION name. From earlier units: P05.1's M4 wall and the recipe
   letters; P04's region padding rule; P03.2's `vec2` binding / polygon ceiling; P02's list.

## What the next unit (P07, close) needs to know

- Block 21 is all DONE (#55–#60, D-60); P07 owns the block's close: the plan's facts harvested
  into a dated record entry, `/docs-semantic-lint` (this unit edited `docs/design/09` but no
  `src/` — no lint was owed here; P07's is owed by the plan), the `plan/` loss audit
  (`--annex plan/`), the compare link and the PR text.
- **The scratch-commit audit method** (here and in P05) avoids a branch: `cp .git/index /tmp/x;
  GIT_INDEX_FILE=/tmp/x git add -A; git commit-tree $(GIT_INDEX_FILE=/tmp/x git write-tree) -p HEAD`.
  The tree-wide noise (≈ 8,800 rows) makes the useful set "rows on the changed pages + rows not
  residual on the pre-change run"; the filter and the triage builder are reproduced in the
  report's header and appendix.
- Milestones window: #60 is on top; P07 adds the block close if it is a separate milestone.
- Byte room: 21/01 15,354 / 15,360 (none); 21/08 ≈ 10.4 KB; `command_reference/README.md`
  ≈ 15.2 KB (near the cap); `roadmap/README.md` ≈ 15.1 KB.
- Next free IDs unchanged: **#62**, **D-65**.

---

← Back to the [plan](../README.md).
