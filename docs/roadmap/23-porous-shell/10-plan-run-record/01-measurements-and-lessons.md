# The porous run — the counts, the handoff-only measurements, the corrections, the lessons

Part of [the run's record](README.md) (23/10), written by P08.2 on 2026-10-22 from the twelve
handoffs under `plan/handoffs/`. Each item's own numbers are on its record page (03–09); this
page holds what the handoffs held *across* units, or held alone.

## The counts the run moved

Local only: the owner's host (Ubuntu, GCC 15, CMake 4, Ninja, Release, 8 cores, 22 GB); no
remote run.

- **ctest.** The cases that match `porous`: 0 → 10 (P02.1) → 15 (P03.1) → 20 (P03.2) → 30
  (P04.1) → 34 (P04.2: one component test, three `cli_field_sample_porous-*` smokes) → 37
  (P05.1) → 38 (P05.2) → **39** (P06.1, `cli_c_abi_porous`). The full suite reads **408** at
  the close (P08.1, serial, 150–190 s): the hygiene run's 369 plus these 39. CI should read
  408 on Linux and macOS and two more on Windows (the writer cases
  [17/22](../../17-code-audit-and-hardening/22-hygiene-plan-run.md) counts) — an expectation,
  not a measurement.
- **GPU parity** 82 → **87** (P05.2: five porous cases, `parity_expected_cases` bumped in the
  same commit); 87/87 at the close. Every run was on Mesa llvmpipe.
- **The C ABI** stays `dualc 0.5.2`: the vocabulary widened, no signature moved (P06.1, 14/04).
- **Vendored files** 17 → 18 (`predicates.c`, P03.1). **design/13** added (P08.1).
- **IDs taken:** block 23; #63 … #69; D-67 … D-75; lint run 10. Next free: block 24, #70,
  D-76, lint run 11, design page 14.

## Measurements only the handoffs held

- **The build trees** (P01.1, fresh fetches, `-j8`): `build-gl/` configured in 81 s and built in
  244 s, with the one warning in fetched geometry-central that `build/` shows (its 134 s /
  295 s are in the [raw dump](../../../raw/2026-10-21-porous-study-measurements.md)).
- **A bake as a seeding speed-up** (P06.1, #63's lead): on the bracket, `porous(mesh,
  spacing=0.05|0.03)` sliced at 64² took 0.12 / 0.49 s unbaked and 0.80 / 1.15 s with
  `bake=128`. The bake costs more than it saves on a light mesh, so 11/16 does not advertise
  it.
- **The ABI test** (P06.1): the in-memory cube 72,092 vertices / 144,140 triangles, the sphere
  40,086 / 80,116; the export 7,207,084 bytes; `ctest -R c_abi` 6/6 in 24.0 s, serial.
- **Rim variants at depth 6** (P04.1, P06.1; 0.9–1.0 s, 0.42 GB each), non-manifold edges: the
  P04.1 smoke onion 1,776 (155,640 faces); the skinned sphere 16, with a `smooth-union` k 0.03
  7, clipped at r 0.97 inside the skin 16 (same face count, so not at the clip), the
  intersection outside the union 9; graded V6 3; the Delaunay-strut bone with a 0.03 skin 10.
  V9's slice: 5,648 contour segments in 0.19 s.
- **Scale and cost notes** (P03.1, P03.2): 2M seeds would tetrahedralize in about 30 s, 13.4M
  tets, about 430 MB of working array (extrapolated, not run). The regular grid's drop rule
  removed 1,288 point segments and 732 duplicates of 2,784 candidates. The brute-force ctest
  takes about 8 s of the porous cases' 9.1 s (6,378 segments × 10k points); precomputing each
  segment's box in `Brute` would cut it. A per-bin `SeedGrid::nearest` on a sparse box
  allocated 3000³ cells (`bad_alloc`), hence the sweeps.
- **MSVC, expected on CI's first Windows build** (P03.1): `predicates.c` compiles with `/W0`,
  which gives the D9025 "overriding /W4 with /W0" command-line warning, the same class as
  `qef.cpp`'s accepted `/W3`.
- **Boletus's header delta** (P07.1): `git diff 350cb3a 4bed5e3 -- capi/dualc_c.h` is two
  comment lines (the `numThreads` comment, the version string); `porous` needs nothing new in
  Boletus's `NativeMethods.cs`. `--dump-json` writes a seed of 2⁵³ as
  `9.007199254740992e+15` and `relax` as `2.0`; .NET writes `E+15`, which the Boletus plan
  notes for its golden strings.
- **The loss audit's whole-tree noise** stayed 10,649 … 10,655 untriaged rows (P06.1, P08.1),
  about 4 minutes a run; each unit read its own pages with `--source`.

## The plan files' corrections

The raw plan is never edited; these went into `plan/` in italics, or were recorded where the
plan's text and the code disagreed.

- *P01.1, `plan/README.md`* — the driver's `smoke` filter `porous|field_graph|field_glsl`
  matched no test before P02: the older cases read `field-graph …` and `GLSL codegen …`. Every
  new case is named `porous: …` so the filter bites.
- *P01.1, `plan/13` T1.3* — the raw plan's "`smooth` lowers the value by at most `k/4`" holds
  for two primitives only; three give 0.39k, four 0.48k, any number < k.
- *P03.2, `plan/12` T2.2* — "the bin size ≥ the largest cell radius" is the struts' bins; the
  own-cell bins only hold a walk's start and keep the smaller spacing.
- *P05.1, `plan/14` T1* — `ctest -R field_glsl` was green vacuously before the unit; the new
  cases are `porous: field_glsl …`.
- Not in a plan file: `childBakeMargin` does get a `porous` entry, and the domain-wrapper test
  does not apply (P04.1, [05](../05-porous-field.md#the-in-run-choices)); the plan's "demo mesh
  in `build/data/`" is `bracket.obj` (P04.2); P06.1's note that the smoke filter misses
  `cli_c_abi_porous` is wrong — `porous` matches it, and P08.1's smoke run counted it (39/39).
- *The owner's, in the manifest:* a human check never stops the run (`45ce8f8`); the handoff
  paths under `plan/handoffs/` (`9b1b4f1`); the memory rule after the OOM (`b1865d7`).

## What the run taught

Each cost a unit time once:

- *The docs tier skips untracked files:* stage a new page before trusting `check.py --docs`
  (P01.1).
- *A fresh `build-gl` outlives a 10-minute tool call;* give it a long timeout (P01.1).
- *Check that a tool exists before trusting a background run:* a "passing" valgrind job was a
  `timeout` with nothing behind it, exit 127 (P03.1).
- *Symbol-renaming macros belong in the one C wrapper TU,* never a shared header, where they
  rewrite `epsilon()` and friends in every C++ file (P03.1).
- *Catch2 filters:* run a bench by its case name, not its tag; no commas in a case name; a
  wildcard only at the ends; the test binary changes directory, so a mesh path is absolute
  (P03.2, P04.1, P04.2).
- *Slice through the first bad edge before sweeping parameters:* thicker walls, depth and a skin
  were tried before a slice showed the rim membrane (P04.2).
- *A porous contour at depth 7 is about 3 GB whatever the recipe;* depth 8 is an OOM on this
  host, unless tiled (`--mem`, 0.44 GB at depth 8) (P04.2, P06.1).
- *Test new GLSL on llvmpipe first* (EGL surfaceless), then the NV92 one case at a time, reading
  `journalctl -k` after each; no arrays, `continue` or returns inside loops (P05.1).
- *The parity gate needs `data/`* (`build/examples/dualc_gen_demo all --dir data`), or the
  seven mesh cases skip and the count reads 80 of 87 (P05.2).
- *Read the whole output, not its tail:* the worst error was 2.75e-5, not 3.7e-7 (P05.2).
- *Audit a docs move before committing it;* P06.1 and P08.1 both audited after, and P08.1's
  audit found a clause to restore (P06.1, P08.1).
- *`design/` rejects "since"* (`design-no-history`) (P06.1, P08.1).
- *`pkill -f <pattern>` matches its own shell;* use `pgrep -a`, then `kill <pid>` (P08.1).
- *`/tmp` does not survive a reboot:* P05.1's and P05.2's scratch harnesses and P08.1's first
  logs went with it (P08.1).

---

← Back to [23/10 — the run's record](README.md) · [block 23](../README.md) · the [Roadmap index](../../README.md).
