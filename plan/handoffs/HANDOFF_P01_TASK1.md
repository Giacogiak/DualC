# Handoff — P01.1: the porous study, block 23, the build trees

Unit P01.1 of [`plan/10_P01_explore-and-record.md`](../10_P01_explore-and-record.md) § T1,
the only unit of P01; session of 2026-10-21, branch `feat/porous-shell` in
`~/Documents/DualC-porous`. Nothing was pushed, no remote was touched, and `main`,
`feat/hygiene-and-editor` and `~/Documents/DualC` were left alone. The unit is docs and
gate data only: no source, test or CMake file changed. The record is
[block 23](../../docs/roadmap/23-porous-shell/README.md). This handoff points at it and adds
what the next units need.

## Branch and IDs

- **Rebase done.** `feat/hygiene-and-editor` (`adc465b`) is an ancestor of `HEAD`
  (`git merge-base --is-ancestor` → true).
- **IDs read from the files:**
  - the last block was `22-dualceditor-handoff`;
  - the last `#N` was #62 (14/04, 17/18, 17/22). The `#101`–`#105` in
    `02-implicit-sdf-foundation.md` are design/08 § anchors, not items;
  - the last `D-NN` was D-66 (`01-settled.md`).
- **Taken by this unit:**

| ID | What | Where |
| --- | --- | --- |
| block **23** | `23-porous-shell/`: README, `01-study.md`, `02-record.md` | `docs/roadmap/` |
| **#63** | Deterministic seeds in any volume — PLANNED (P02) | 23/02 |
| **#64** | Delaunay tetrahedralization and the packed complex — PLANNED (P03) | 23/02 |
| **#65** | The porous node on the CPU — PLANNED (P04) | 23/02 |
| **#66** | The porous node in GLSL and `dualc_field_view` — PLANNED (P05) | 23/02 |
| **#67** | The C ABI test and the documentation — PLANNED (P06) | 23/02 |
| **#68** | The Boletus implementation plan — PLANNED (P07) | 23/02 |
| **#69** | Graded nodes miss a negative thickness in `cellOverlaps` — DEFERRED with trigger; it was not tracked before | 23/02 + a row on `decisions/README.md` |
| **D-67…D-73** | The § 1 rows: cells, mode, thickness reuse, smoothing in the node, distributions, Boletus plan-only, unattended + checks at the end | `decisions/03-settled-continued.md` |
| **D-74** | Engine: own Bowyer–Watson on Shewchuk's predicates — **Chosen in-run (pending owner review)** | same |
| **D-75** | Settled rows from D-67 on go to the new child page `03-settled-continued.md` — **Chosen in-run (pending owner review)** | same |

- **Next free after this unit:** block 24, #70, D-76.

## What changed, and why

1. **The study** — [23/01](../../docs/roadmap/23-porous-shell/01-study.md): (a) engine,
   (b) distributions, (c) distance formulas, (d) GPU layout, and **the defaults table**. Every
   later unit takes its defaults from that table. The rows marked † are *Chosen in-run
   (pending owner review)*.
2. **The engine.** geogram's PSM 1.10.1 builds clean under C++17 `-Wall -Wextra` and is fast,
   but it needs `GEO::initialize()`. Its BRIO order and its walk also draw from a process-wide
   `static std::mt19937_64` with no lock, through `std::uniform_int_distribution` /
   `std::shuffle`, which differ by standard library. As a result the tet list changes from one
   call to the next in one process. Under the plan's rule ("global state not thread-safe or not
   deterministic → the fallback wins"), **the fallback wins**. The prototype Bowyer–Watson is
   correct and deterministic, with no flat tets, and is 2.1× slower than `BDEL`.
3. **Design refinements the table makes** (the raw plan § 3 stands otherwise):
   - **Own-cell exact distances.** Voronoi walls use a greedy nearest-seed walk plus the
     bisectors. Delaunay faces use a visibility walk to the containing tet plus its 4 face
     planes. Both are exact everywhere with no bin bound. Struts use segment bins, and their
     value is `min(d, B)`, with `B` the larger spacing.
   - **Smoothing.** `max(0, smin)` in the own-cell cases, so the field stays continuous and
     1-Lipschitz across walls. The fold runs in packed order, so CPU and GPU agree.
   - **The smin bound is corrected:** k/4 holds only for two primitives; three give 0.39k and
     any number < k. The italic correction is in `plan/13_P04_field-and-node.md` T1.3.
   - **Graded Lloyd must weight the samples by density**, or it erases the grading.
   - `predicates.c` must be compiled as `-std=gnu89` (GCC 15's default C23 rejects its K&R
     definitions) and with `-ffp-contract=off` / `/fp:precise`.
4. **#69** opened: `graded-onion` / `graded-offset` grow by `max(t1, t2, 0)`
   (`src/implicit/decorators.cpp`, `maxThickness()`), so a shrinking `graded-offset` is
   uncovered. `OffsetField` uses `|r|`.
5. **The decision pages.** `decisions/01-settled.md` stood at 15,356 / 15,360 bytes, so the new
   settled rows open `03-settled-continued.md` (D-75), after the D-61 precedent. The rejected
   alternative was splitting 01 into a folder: about 40 inbound links, some of them in
   immutable `docs/raw/` files. The README intro names the new page, and
   `scripts/check_data.json` lists it under `status_vocab_only`.
6. **Gate data.** The block README is added to `status_tables`, and `23/02-record.md` to
   `ordered_item_files`, as was done for block 21.
7. **`roadmap/README.md`:**
   - § Current focus rewritten with the porous run first and the hygiene run's line condensed.
   - Track 1 item 1 is now Porous shell; the DONE CLI-consolidation item left Next up, and its
     facts remain in the milestones table, the blocks table and block 21.
   - Block 23 row added.
   - Bytes: 15,027 → 15,289 of 15,360.
8. **Raw dump** [`docs/raw/2026-10-21-porous-study-measurements.md`](../../docs/raw/2026-10-21-porous-study-measurements.md)
   holds every driver output, both download hashes and the GL limits, with a row in
   `docs/raw/README.md`.

## Measured, and under what

- **Host and conditions:** this Linux box, 8 threads, GCC 15, `-O2`, single thread unless
  named. Timings were taken with no build running (the full dump is in the raw file).
- **geogram PSM** on 100k points (mt19937 seed 42):
  - `BDEL` 580–595 ms, 671,842 tets; the 20³ grid 397–403 ms, 41,154 tets.
  - `PDEL` (8 threads) 220–227 ms and 156–178 ms.
  - The hashes of three consecutive calls differ; the first call of each fresh process
    repeats.
  - On the prototype's PCG32 points, sorted canonically: 672,288 tets, and `BDEL` = `PDEL`.
- **Prototype Bowyer–Watson** on the PCG32 points:
  - 100k points: 1,232–1,240 ms, 672,241 non-super tets (47 hull tets fewer than geogram:
    the finite super-tet).
  - 20³ grid: 230 ms, 41,154 tets (a different valid triangulation than geogram's).
  - 0 tets with `orient3d ≤ 0`, 0 / 100,000 empty-sphere violations, 7.7 walk steps per
    point, identical canonical hash on both runs.
- **Distributions** (periodic 10³ box, s = 0.5):
  - Cell-volume CV, net of Monte-Carlo noise: random 0.424, jitter 0.248, poisson 0.136,
    poisson + relax 20 0.077.
  - Bridson density 0.573 seeds/s³ at 23,700 seeds/s.
  - Lloyd: 285 ms per iteration at 4,583 seeds × 32 samples.
- **GL on this host:** `NV92` (Mesa 26.0.8, core 3.3). `MAX_TEXTURE_SIZE` 8192,
  `MAX_3D_TEXTURE_SIZE` 2048, 16 texture units.
- **Build trees** (Release, Ninja, `-j8`, fresh fetches):

  | Tree | Configured with | Configure | Build | Warnings |
  | --- | --- | --- | --- | --- |
  | `build/` | TESTS + EXAMPLES + C_ABI (= `GATE_OPTIONS`) | 134 s | 295 s | 1, in fetched geometry-central |
  | `build-gl/` | the same + FIELD_VIEW + GLSL_PARITY | 81 s | 244 s | the same one |

  The first `build-gl` attempt was killed by my tool's 10-minute limit during configure; the
  tree was deleted and redone from scratch, and both trees then reported "no work to do" on a
  rebuild.

## Checks run

The rules for P01–P07: no full gate, targeted checks only.

- `python3 scripts/check.py --docs --strict`: 28 checks, 27 OK + 1 INFO (`index-staleness`,
  pre-existing), PASS, on the staged tree. The pre-commit hook ran the same check and passed on
  the commit `5cd4399`.
- `cmake --build build -j && ctest --test-dir build -R "porous|field_graph|field_glsl"`: builds,
  but **"No tests were found!!!", exit 0** (see below).
- `ctest --test-dir build -R "porous|field-graph|GLSL codegen" -j8`: 19 / 19 passed.
- **Loss audit:** `scripts/docs_loss_audit.py --base 45ce8f8 --head 5cd4399 --scope docs/
  --source docs/roadmap/README.md`, with an empty scratch triage file. It found six rows from
  the rewritten roadmap index, all intentional:
  - the Current focus clauses were condensed;
  - "through the hosted-CI plan … (20/04)" was dropped, and 20/04 stays in the blocks table's
    row 20;
  - the DONE CLI-consolidation Next-up item was dropped, and its facts stay in the milestones
    rows, the blocks table and 21/09.

  Every other row is the tool's baseline noise. No file was deleted or moved.

## What I got wrong on the way

- **The docs tier skips untracked files.** My first `--docs` runs passed while the new pages
  were untracked. The commit hook then failed on two problems: `usage-in-record` (a `Default`
  column in a roadmap table, now "What later phases follow") and `heading-status` (a date in
  the raw dump's title). **Stage new pages before trusting `check.py --docs`.**
- The background build hit the tool's default 10-minute limit. Use a long timeout for a fresh
  `build-gl`.
- The first prototype reused freed tet slots before relinking the outer neighbours, and
  segfaulted. The fix was to record the back-pointer index before freeing the cavity, which is
  worth knowing for P03.

## Open, and what the next units need

- **The driver's `smoke` filter is vacuous.** `porous|field_graph|field_glsl` matches no test
  name: the Catch2 cases read `field-graph …` / `GLSL codegen …`, and ctest exits 0 on "No
  tests were found". The manifest is the owner's and was not changed. The defaults table makes
  every new case `porous: …`, so from P02 on the filter bites. Each unit should also run
  `-R 'porous|field-graph|GLSL codegen'` itself. The correction is noted in italics in
  `plan/README.md`.
- **P02:**
  - Use the RNG, ramp, pad, variable-radius and `relax` rows of the defaults table.
  - P02's plan file defines jitter/random density at 1/spacing³; Bridson gives 0.573/s³, so
    switching `dist` changes the cell count (kept, †).
- **P03:**
  - Vendor `predicates.c` (sha256 `f8662c3f…2029`) under `src/internal/` with the compile flags
    above; Bowyer–Watson goes in DualC's own source.
  - The finite super-tet loses about 47 hull tets per 100k points, so the Euler test must allow
    for it, or implement ghost vertices (about +100 lines).
  - Measure the struts' items per bin.
- **P04:** smoothing bound per the corrected P04 text; `cellOverlaps` stays the default.
- **P05:**
  - Measure the texture limits at run time.
  - This host is `NV92` at 8192.
  - Integer textures need `precision highp usampler2D;` in the ES header.
- **No human check is raised by this unit:** everything was verified headless.
- **The owner's review list (P08.2) gets:** D-74, D-75, and the † rows of 23/01's defaults
  table (spacing semantics, variable-radius Bridson, pad, `relax`, Delaunay-walls walk,
  `smooth`).

## Exit criteria of P01

- *Every later unit can take its defaults from the block's `01-study.md` without asking
  anyone.* **Holds.** The defaults table has a row for every choice P02–P05 leaves open:

  | Phase | Rows that cover it |
  | --- | --- |
  | P02 | RNG, spacing, ramp, variable radius, pad, relax, ceilings, cancel |
  | P03 | engine with compile flags, insertion order, canonical output, bins, the Voronoi-edge clamp |
  | P04 | value per case, `smooth` and its bound, `cellOverlaps`, determinism, test names |
  | P05 | texture formats and width, limits queried, units, `nodeFeatureScale`, fallback |

  P06–P07 need no new default: their content is the parser's real defaults and the raw plan.
- *T1 acceptance:*
  - the block exists with `01-study.md` and its table;
  - the IDs were read from the files and are listed above;
  - `build/` and `build-gl/` exist and build;
  - `--docs --strict` is green;
  - everything is committed.
