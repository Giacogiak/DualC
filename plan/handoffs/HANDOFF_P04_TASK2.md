# Handoff — P04.2: #32 Batch B, the missing tests

Unit P04.2 of [`plan/13_P04_test-coverage.md`](../13_P04_test-coverage.md) § T2, the last unit
of P04; session of 2026-10-07, branch `ci/test-coverage` (pushed to `origin`; **not merged**,
`main` untouched). The record is
[roadmap 17/15 § Batch B](../../docs/roadmap/17-code-audit-and-hardening/15-test-coverage-batches.md#batch-b--the-missing-tests);
this handoff points at it and adds what the next unit and Giacomo need.

## What changed, and why

Commits: `12fb369` (tests), `2b3036b` (docs), then this handoff with the CI note.

- **`partitionCubeEdges` on all 256 configurations** (C45), in `tests/test_cube_components.cpp`.
  Per configuration it checks four things:
  - crossing ⇔ labelled, with dense ids and at least 3 edges per component;
  - on every face, each crossing pairs inside its own component (the inside-pair rule on
    saddle faces);
  - the component count equals an independent oracle that never pairs crossings: *k* loops
    cut the cube's surface into *k* + 1 regions;
  - the census over all 256: 2 / 162 / 82 / 8 / 2 configurations with 0 / 1 / 2 / 3 / 4
    components.

  The nine hand-picked cases are kept.
- **`tests/test_accuracy.cpp`, new** (C47, C49, C50, plus the `simplificationError` knob):
  - *Refinement convergence:* vertex error against an exact SDF falls at least 3× per level,
    depth 4 to 7, on an off-centre sphere and a torus.
  - *Collapse trade-off:* `simplificationError` cuts faces and raises the error by a bounded
    amount.
  - *Output normals:* unit length, and `n · ∇f ≥ 0.99`, on a sphere, a torus and a box.
  - *`interpolateNormals` at `MeshBVH::segmentFirstHit`:* the blend is recomputed
    independently from geometry-central's vertex normals.
  - *`interpolateNormals` end to end on a cube:* sharp makes every Hermite normal
    axis-aligned and puts every vertex on the cube.
- **The other three `ContourerParams` knobs** (C48) are leaf-level cases in
  `tests/test_contourer.cpp`, on one fabricated leaf whose QEF has a 0.046 eigenvalue and a
  minimum 0.436 cells outside the cell. They are `qefRegularization`, `clampVertexToCell` and
  `clampToleranceCells`.
- **CLI content checks** (C52):
  - `examples/check_cli_output.cmake` is a `cmake -P` script. It checks:
    - the PNG signature and the IHDR dimensions;
    - a binary STL's size against its declared count;
    - that a 3MF unpacks, with its three parts present;
    - that the STL and 3MF triangle counts equal the same run's OBJ face count;
    - that an OBJ is all triangles;
    - that an SVG is complete;
    - that the `--dump-json` output parses, with the right root op.
  - It backs 21 `*_content` CTest cases, registered in `examples/CMakeLists.txt` as
    `FIXTURES_REQUIRED` of the tests that write the files.
- **The strut-lattice oracle** (07's "certifies less than it claims"):
  - 600 seam-targeted points join the 400 uniform ones;
  - the comment is restated to what the oracle certifies.

  See *The finding* below. Running time: 0.6 s and 1.4 s for the two cases.
- **Coverage, measured once** (C51's other half): 87.9 % of libdualc's lines. No option or CI
  job was added; that choice is deferred as **D-50**, with its trigger.
- **Docs:**
  - 17/15 has the Batch B section;
  - 17/04 #32 is **DONE**, with dated notes on #32, on its oracle paragraph and on #35;
  - 17/06 gives C45, C47–C50 and C52 DONE, C46 closed by its correction, and C51 its measured
    coverage;
  - 17/07 has two dated corrections (headings and text untouched);
  - 17/12 receives #33's history paragraph, verbatim;
  - 17/README: the #32 row (DONE), the 07 index line, and *Latest verification* (ctest
    **300**);
  - the roadmap README: the focus line (**NEXT** = the rest of #34) and the milestone row
    (it replaces Batch A's);
  - `docs/decisions/README.md` has the D-50 row;
  - `STRUCTURE.md` has rows for `test_accuracy.cpp` and `check_cli_output.cmake`, and the
    `test_cube_components` / `test_contourer` rows are extended.

## The finding: 07's strut-lattice claim does not hold

[07](../../docs/roadmap/17-code-audit-and-hardening/07-repeat-tiling-fix.md) said three things:

- the `RepeatField` fix moved 678 vertices of a shipped octet lattice that had been misplaced
  at the tile seams;
- the strut oracle passed clean over that error;
- strut lattices therefore pay 2.3× for correctness.

**Measured, none of that holds.** Three measurements:

- **The fold, mutated back.** With `RepeatField` forced back to the single fold
  (`foldExact_(true || …)`), both strut gates still pass.
- **A dense grid.** A 41³ grid across the seams reads a difference of exactly 0 for all four
  crystals, at r = 0.05 and r = 0.06.
- **07's own case.** `octet(wavelength=0.4,radius=0.05)`, `--bounds -1..1`, depth 7, run once
  on the fold and once on the fix:
  - 260 vertices differ, **every one within two cells of the root box**, and none near a seam;
  - the fold's vertices sit no further from the true SDF than the fix's (summed |f| over the
    moved set: 0.038 against 0.050).

The reason is mirror symmetry. The four cells are mirror-symmetric about their own faces, so
every neighbour strut has a mirror image in the own tile that is at least as close. The fix
stays right for asymmetric children: the off-centre sphere in `test_domain_ops.cpp` is the
real defect. What is not established is why boundary vertices move at all; the values are
bit-identical in the interior probe. Routed to #35 as an input: a symmetry flag on the cell
would put strut lattices back on the fast path.

## Measured numbers (conditions)

Local: Ubuntu, GCC 15, CMake 4, Ninja, Release, 8 cores.

| What | Value |
| --- | --- |
| `ctest` | 270 → **300** (9 Catch cases + 21 content checks) |
| full gate | PASS: clean rebuild (`--target clean` first), 255 TUs, 0 DualC-origin warnings, ctest 300/300 in 101.5 s serial (was 78–90 s) |
| `--docs --strict` | PASS |
| `ctest -R _content -j 8` (with fixtures) | 40/40, 12.7 s |
| convergence, max error depth 4→7 | sphere 4.0e-3, 1.1e-3, 2.9e-4, 7.2e-5; torus 8.9e-3, 1.9e-3, 5.2e-4, 1.4e-4 |
| collapse at depth 7 (sphere) | 127,740 faces / 7.2e-5 → 112,926 / 2.2e-4 at 1e-6; saturates at 111,900 |
| normals | worst \|n\| − 1 = 4e-16; min n · ∇f 0.999999 (sphere, torus), 1 (box) |
| sharp vs smooth cube, depth 5 | 20,184/20,184 vs 24/20,184 axis-aligned normals; worst vertex 0 vs 1.2e-2 |
| `[accuracy]` cases | 43 assertions, 2.8 s |
| coverage (separate `--coverage` tree, Release, ctest 300/300 in 110 s at `-j 6`) | 2,469 / 2,810 lines = **87.9 %** over the 23 `src/` `.cpp` files with lines; `qef.cpp` 53.7 %, `combinators.cpp` 63.8 %, `svd.cpp` 67.9 % lowest |

## Proven to fail

Each mutation was made in the working tree and reverted before the commits.

- **Saddle rule inverted** in `partitionCubeEdges`: the sweep fails 400 assertions; the nine
  old cases fail once.
- **QEF vertex replaced by its mass point:**
  - the box-normals case and the sharp-cube case fail;
  - **convergence does not fail**, because a mass point also converges at O(h²). That test
    proves convergence, not QEF quality, and 17/15 says so.
- **Corrupt PNG signature, STL 50 bytes short, 3MF cut to 1,000 bytes:** each run with
  `ctest -FA '.*'`, so the producers do not rewrite the files first. Each `_content` case
  fails, naming the defect. The files were restored and pass.
- **`check_cli_output.cmake` by hand:**
  - a wrong `WIDTH` fails;
  - a mismatched `SAME_AS_OBJ` fails;
  - a quad OBJ fails.

## What I got wrong on the way

- **I guessed the 256-configuration census** (230 / 22 / 0 for 1 / 2 / 3 components) before
  measuring it, and the first run failed on it. The invariants and the oracle had passed on
  every configuration. The pinned census is the measured one. The comment says the three
  components are three non-adjacent **inside** corners, because the inside-pair rule makes
  the census asymmetric under complement.
- **The first strut-oracle comment I wrote claimed a proof I had not run.** It said the seam
  points catch the single fold and the uniform points do not. Running the mutation showed
  that neither catches it, and that led to the finding above. The comment was rewritten
  before the commit.
- **The 04 split.** P04.1's handoff said to split 17/04 into a folder. That would break three
  links in the immutable `docs/raw/2026-10-05-hosted-ci-plan.md`, so I followed the
  2026-09-18 precedent instead and moved #33's history paragraph, verbatim, to 17/12. 04 is
  now at 15,254 bytes.
- **The CMake list escaping.** `"a\;b"` pairs in a `foreach` and a `;` inside an `add_test`
  `-D` argument both failed at configure. I switched to `producer=file` pairs and a
  `|`-separated `RUN`.
- **Page sizes:**
  - `docs/decisions/README.md` went 28 bytes over the cap with the first D-50 row, which was
    shortened;
  - the roadmap README is at 15,356 of 15,360 bytes.
- **The coverage tree took about 45 minutes**, mostly geometry-central under `--coverage`.
  The gate was run after it finished, so the timing-sensitive tests were not run under load.

## Exit criteria — one by one

- **17/04 #32's body line is DONE:** `**DONE (2026-10-07).**`.
- **C42–C54 carry their dispositions:**
  - C42, C43, C45, C47–C50 and C52 are DONE;
  - C46 is closed by its correction;
  - C51: sanitizers DONE, coverage measured, with the CI job as D-50;
  - C44 and C54 are MINOR-OPEN;
  - C53 points to 10 #11.
- **The decisions rows touched and the snapshot agree:** D-50 is the one row touched. The
  roadmap README says #32 is done and **NEXT** is the rest of **#34**, and 17/README's #32 row
  says DONE.
- **The branch's latest run is green:** run
  [37591221067](https://github.com/Giacogiak/DualC/actions/runs/37591221067) on `2b3036b`
  (the test and docs commits) was green on every job: `build` ubuntu-24.04 128 s, macos-14
  149 s, windows-2022 484 s; `sanitize` 298 s; `gpu` 169 s; `docs` 5 s.
  `python3 plan/remote_run_check.py` exited 0 on it, after one transient DNS failure. The
  commit adding this handoff triggers one more run, and the driver's `remote-run` check
  verifies that one.
- **The handoff lists the before/after counts for both batches:** Batch A is in
  `HANDOFF_P04_TASK1.md` and 17/15 (38 → 0 unguarded `Approx(0.0)`, 31 → 32 gate checks,
  ctest 270). Batch B is above (ctest 270 → 300; 9 → 256 cube configurations; 1 → 5 knobs
  varied; 0 → 20 output files plus `--dump-json` content-checked).
- **What Giacomo must merge:** `ci/test-coverage`. It sits on `ci/build-hardening`'s head
  (`ab5ce1e`), so it carries P01–P03 too; if that branch merges first, this one is its
  fast-forward plus P04's commits (`b38e43a`, `e637913`, `d8eeef2`, `12fb369`, `2b3036b` and
  this handoff's).

## Open, and what comes next

- **The plan:** P04 is the last phase in this plan file. The roadmap's **NEXT** is the rest
  of **#34** (API hygiene).
- **#35** has a new input: strut cells are symmetric, so the 2.3× buys nothing for them.
- **Still open from earlier units:**
  - the Linux C-ABI link failure (17/14 finding 7);
  - the macOS `parallelForPolled` flake (finding 8);
  - `dualc_gen_demo all --dir <missing>` aborts.
- **Not established:** why the RepeatField fix moves vertices near the root box. Values are
  bit-identical in the interior probe; the widened `cellOverlaps` is the likely cause and
  was not tested.
