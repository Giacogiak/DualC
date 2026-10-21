# Code audit — the hygiene + editor run, harvested

Part of [Code audit & engine hardening](README.md) (block 17). The
[hygiene + editor plan](../../raw/2026-10-19-hygiene-and-editor-plan.md) was run by the
owner's `plan-run` driver as twelve unattended units, 2026-10-19 … 2026-10-21, each a fresh
session on branch `feat/hygiene-and-editor` (cut from `main` at `5ce5b1c`) closing with a
handoff under `plan/handoffs/`. Each item's record holds its own facts ([18](18-contourer-threads.md)
to [21](21-repeat-pruning.md), [14/06](../14-c-abi/06-linux-shared-library.md),
[22](../22-dualceditor-handoff/README.md)); this page is the harvest of what the handoffs held
*across* units — the counts, the corrections to the plan files, every in-run choice awaiting
the owner, what the run taught, what it left open — after which `plan/` is deleted
([D-51](../../decisions/01-settled.md)), as [21/09](../21-cli-consolidation/09-plan-run.md) did
for the last run.

## The hygiene run — P01 to P08, harvested

**DONE — 2026-10-21** on the local branch: #35 DONE; #34 PARTIAL (E12 reopened); #53 PARTIAL
(the remote run owed); the standalone UI moved to DualCEditor (block 22,
[D-66](../../decisions/01-settled.md)); [D-65](../../decisions/01-settled.md) decided. Nothing
was pushed: the push, the PR and the CI reading are the owner's (`main` is protected, PR only).

| Unit | Item | Record | Commit |
| --- | --- | --- | --- |
| P01.1 | the record opened: D-65, D-66, the #53 cell | [04](04-engineering-quality.md), [settled](../../decisions/01-settled.md) | `95fcc94` |
| P02.1 | #34 thread count, ABI 0.5.2 | [18](18-contourer-threads.md) | `dc0a261`; the owner's answer `db8ffef` (#62) |
| P03.1 | #34 `gradientAt` contract, the census | [19](19-gradient-contract.md) | `e9c7c59` |
| P03.2 | #34 `trueGradientAt`, the Newton step | [19](19-gradient-contract.md#the-true-gradient-path) | `2f1f2ea` |
| P04.1 | #34 a continuous `GridField` normal | [20](20-gridfield-normal.md) | `f939a35` |
| P04b.1 | #34 limited stencils; the cell gradient restored | [20 § P04b](20-gridfield-normal.md#p04b--a-limited-node-stencil-measured) | `220e4df` |
| P05.1 | #53 the gate builds the C ABI | [14/06](../14-c-abi/06-linux-shared-library.md) | `5da4cd0` |
| P06.1 | #35 the pruned neighbour set | [21](21-repeat-pruning.md) | `6a231c7` |
| P06.2 | #35 byte-identity, the speed-up | [21](21-repeat-pruning.md#gates) | `594c1d5` |
| P07.1 | block 22, the hand-off index | [22](../22-dualceditor-handoff/README.md) | `e550558` |
| P07.2 | Track 2 pointers, D-16 / D-17 / D-25 | [22](../22-dualceditor-handoff/README.md#how-the-block-was-opened) | `5bad79d` |
| P08.1 | the close | this page | — |

**The counts the run moved** (local: Linux, GCC 15, Ninja, Release, i7-920; no remote run).
ctest 344 → 350 (P02) → 360 (P03.2) → 362 (P04.1) → 362 (P04b: a crease test in, the
continuity test hidden) → 367 (P05: the five `cli_c_abi*` cases enter the gate) → 368
(P06.1) → **369** (P06.2). GPU parity **82/82** wherever the tier ran (P02.1, P03.2, P06.1,
the close). The ABI went **0.5.1 → 0.5.2** (P02: `numThreads` now also bounds the contourer —
a patch, the raw plan's default). CI should read 369 on Linux and macOS and 371 on Windows
(two more writer cases there, [16](16-windows-rename-race/README.md)) — an expectation from
2026-10-07's offsets, not a measurement.

**Measurements only the handoffs held**, carried here. `libdualc_capi.so` 2,640,496 bytes, no
`TEXTREL` (P05). `dualc_c_demo m.stl mesh` on the `TESTS=OFF` tree: 70,032 v / 120,612 f
(P05). Full rebuilds: P04b's `--clean` after `cmake --build build --target clean`, 252 TUs,
build 350 s; P05's, 254 TUs (PIC changes every TU's flags), build 255 s, ctest 133 s — 0
DualC-origin warnings both. P04b's peak RSS ≈ 496 MB (case A) and 29 MB (case B), the same
in every variant. P05's red run stops at step 111/111, the `.so` link; its sanitized tree configured in 85 s.
The loss audit's whole-tree untriaged count stayed 9,979 … 10,060 across the units, all
baseline noise.

**How the run went.**

- **Unattended overnight** (Giacomo, 2026-10-19): every gate `auto`; an open choice took the
  raw plan's § 3 default and was written as *Chosen in-run (pending owner review)*. P02.1
  stopped once anyway, on the C ABI's create-time bakes; the owner answered on 2026-10-20
  (#62 DEFERRED) and ruled that in-run choices never stop the run: they are reviewed at P08,
  below.
- **One owner review mid-run.** P04.1's continuous normal raised the CAD pair's mean
  deviation by 35 %; Giacomo (2026-10-21): keep continuity, limit the stencil at creases —
  the added phase P04b, whose own fallback restored the cell gradient.
- **P04.1 stopped before its commit**: the whole-tree loss audit overran a 10-minute
  background call. The owner's session audited per touched page and committed. From then
  on every unit audited per page, in foreground calls of about 5 minutes each.
- **The GPU tier** ran wherever the field layer moved (P02.1, P03.2, P06.1), always after
  rebuilding `build-gl`.
- **The byte cap shaped the record.** 17's README sat within 25 bytes of 15,360 from P03.2 on;
  04 was relieved twice by verbatim moves (#33's original scope to 12 at P01, #35's levers to
  21 at P06.2), so each delivery went to its own page, 18–21, with one line in 04.

**The plan files' corrections** (the raw plan is never edited):

- *P04b.1* — step 3's "minmod equals the central difference to second order" is wrong
  (minmod is one-sided, first order), and the acceptance "the continuity test stays green"
  cannot hold on the plan's own fallback path.
- *P05.1* — five `cli_c_abi*` cases, not four; the sanitized check needs its own `--build-dir`.
- *P06.1* — there is no `lipschitzBound()`, and Lipschitz-1 is the wrong property (it
  bounds the slope, not the value far away): the certificate is `boundsBoundValue()`.
- *P07.2* — 12/03 § D, § Section planes and 08 are DualC records: a pointer sentence each,
  not the one-line bodies the plan asked for.
- Raw-plan facts found wrong, not in a plan file: the editor's questions are Q1–Q9, not
  Q1–Q4, and its parameter edits date from `2676c6a` (P07.1, [22](../22-dualceditor-handoff/README.md));
  the 2026-10-07 PIC error's first victim is now `field_graph.cpp`'s `g_readKeys`, not
  `__tls_guard` — ld names the first offending object only (P05.1, [14/06](../14-c-abi/06-linux-shared-library.md)).

## The in-run choices, for the owner's review

Every *Chosen in-run (pending owner review)* of the run, in one place. Each is written on its
record page too; accepting one costs nothing, the alternative is named.

| Unit | Choice | The alternative | Record |
| --- | --- | --- | --- |
| P01.1 | #33's original scope moved verbatim from 04 to 12 for cap room | D-37's folder split of 04, which broke three anchored links from the immutable `docs/raw/2026-10-05-hosted-ci-plan.md` | [12](12-engineering-quality-records.md) |
| P02.1 | `--threads` capped at 1024 (a typo such as `-1` exits 2) | no cap | [18](18-contourer-threads.md) |
| P03.1 | `elongated` named with the smooth blends and `mix` as shortening `gradientAt` | the plan's two | [19](19-gradient-contract.md) |
| P03.2 | (a) hard booleans, mirror, both repeats forward `trueGradientAt`; the graded decorators do not · (b) a sphere's is zero at its centre · (c) the winding Newton test at edge 4 and 0.25, not 1 (at 1 the old step wins 10 of 24 points) · (d) `MeshSource` keeps the FD default | (d): forward its unit `gradientAt` — exact off the surface, 6 BVH queries cheaper | [19](19-gradient-contract.md#the-true-gradient-path) |
| P04.1 | the record on a new page, 20, not appended to 19; its fallback and its loosened P03 assertion were undone by P04b | append to 19 | [20](20-gridfield-normal.md) |
| P04b.1 | (a) **the cell gradient shipped, #34 PARTIAL** · (b) the continuity test hidden `[.][grid][e12]`, not deleted · (c) the angle mask's chord criterion | (a): minmod — continuous, crease test green, case A +18 % — or the 40° angle mask (A +6 %); either re-applies its branch and sets #34 DONE | [20 § P04b](20-gridfield-normal.md#p04b--a-limited-node-stencil-measured) |
| P05.1 | (a) the C ABI built by every CI job, macOS and Windows included — their first hosted `.dylib` / `.dll` · (b) re-configure in place when a gate option is missing from the cache · (c) the sanitized check in its own build dir | (a): if `build (macos-14)` goes red, fix it or a per-OS rule in `GATE_OPTIONS`; (b): require `--clean`, and old `build/` dirs silently skip the ABI | [14/06](../14-c-abi/06-linux-shared-library.md), this page |
| P06.1 | lever (b) as a public opt-in virtual, `boundsBoundValue`; lever (a) not kept (it breaks value identity); home tile first in `cellOverlaps` (unplanned, exact, 67.5 → 56.9 s); six reference meshes, not three | an internal type check on known primitives, or no lever (b) (≈ 2.9×) | [21](21-repeat-pruning.md) |
| P06.2 | lever (c) not taken at 1.30–1.31×, on the plan's bar: the fold's mesh is not byte-identical (3,452 OBJ lines differ); the levers' text moved 04 → 21 verbatim; the speed-up on 21, not a new page | take (c) and accept a new reference mesh | [21](21-repeat-pruning.md) |
| P07.1 | block 22 a folder; more archived than listed; D-25 read as delivered (no channel, in-process uniforms) | — | [22](../22-dualceditor-handoff/README.md#how-the-block-was-opened) |
| P07.2 | two old homes keep their record; D-16 DEFERRED with its trigger, D-17 / D-25 DONE in the open table; the two usage pages name DualCEditor; no decision row for the Web build | — | [22](../22-dualceditor-handoff/README.md#how-the-block-was-opened) |
| P08.1 | this page as 17's next child, P07's choices linked to 22 rather than restated; one milestone row for the run, which leaves the window's last 2026-10-15 row (#57) to [21/04](../21-cli-consolidation/04-mesh-node-bake.md) | a milestone row per item | this page |

**What the run taught.** Each cost a unit time once:

- *`--clean` only reconfigures*: it reuses the objects, so the warnings scan can compile 2
  TUs. A proof of zero warnings is `cmake --build build --target clean`, then the gate (P04b).
- *A whole-tree loss audit outlives a 10-minute call.* Audit per touched page
  (`--source <page>`), in the foreground; parallel runs on 8 cores took 13 minutes together
  (P07.2), and `until pgrep -f …` loops match their own command line.
- *Never split a page that `docs/raw/` links by anchor*: raw files cannot be re-pointed (P01).
- *An unescaped `|` inside backticks splits a table cell* (P03.1, P03.2).
- *A design page's anchor must carry no date* (`design-no-history`), nor a heading
  (`heading-status`): dates go in the body (P04b, P05, P07.1).
- *A near-full tmpfs fails writes silently*: timings ran against empty files until `/tmp`
  was cleared; gate every timed run with a `cmp` (P06.2).
- *A test that is right by accident*: the unit-size tetrahedron's cap has |∇w| ≈ 1.013, where
  the old Newton step looks fine (P03.2).

## Open after the run

None blocks the PR; each has its home.

| What | State | Home |
| --- | --- | --- |
| #53's remote proof | the five `cli_c_abi*` lines in `build (ubuntu-24.04)` → #53 DONE in 14/06, 14's README and the roadmap README | [14/06](../14-c-abi/06-linux-shared-library.md) |
| 17's Latest verification | the CI counts after the push replace text there (the README is at the cap) | [README](README.md) |
| E12 and #34 | the owner's pick among the measured stencils (§ The in-run choices, P04b.1) | [20 § P04b](20-gridfield-normal.md#p04b--a-limited-node-stencil-measured) |
| The stencil variants' source | scratch, `/tmp/p04b/src/grid_field_variants.cpp` (variant by `P04B_V`, ratio by `P04B_R`), gone at the next reboot; each formula is on 20. The deviation evaluator links `build/libdualc.a` and every `examples/libdualc_examples_*.a`, `_decimate`, `_img` and `_slice` included; the hidden test runs as `dualc_tests "[e12]"` | [20](20-gridfield-normal.md) |
| #11's trigger, "work needing before/after numbers" | met by #34 and #35, which built scratch benchmarks under `/tmp`, none committed — build the suite or re-word the trigger, the owner's call | [lint run 09](../19-docs-layers/08-semantic-lint/09-2026-10-21-after-hygiene-run.md), [#11](../../decisions/README.md) |
| The editor's Q7, an OBJ that fails to load | reproduced at the close: the trigger is a **negative (relative) face index** (`f -4 -3 -2 -1`), not the comment or `o` line. geometry-central's reader (`simple_polygon_mesh.cpp`, `parseFaceIndex` and the line after it, pinned upstream) reads a negative index as a line continuation and, at the file's end, calls `substr(npos)`. `dualc_field` exits 2 with `basic_string::substr`; the same faces with positive indices load | this page |
| The GLSL mirror of `repeat` | still reads all 8 copies; parity holds, pruning it is not planned | [21](21-repeat-pruning.md) |
| `MeshSource` under `normalizedOf` | 6 BVH queries a `valueAt` (P03.2 (d)) | [19](19-gradient-contract.md) |
| Boletus | 0.5.2 on its next re-vendor; it can drop `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` | [14/06](../14-c-abi/06-linux-shared-library.md) |

*2026-10-21, the close (P08.1).* What the unit ran before handing over:

- **`/docs-semantic-lint`**, a delta run over the branch:
  [19/08 run 09](../19-docs-layers/08-semantic-lint/09-2026-10-21-after-hygiene-run.md), 2
  findings — design/10's finite-difference row fixed; #11's met trigger, the table above.
- **The loss audit of `plan/`**: `docs_loss_audit.py --base 5bad79d --head <scratch> --scope
  docs/ plan/ --annex plan/`, the head a `git commit-tree` of the staged close with `plan/`
  removed, a scratch triage (the `3d73220` thresholds, no rules); one run, 8 minutes.
  - *The annex:* 1,101 sentences, 515 evidence units, 129 headings and 118 identifiers
    residual. Sentences and headings: the handoffs paraphrased, not copied. Evidence: page
    byte counts of a given day, audit row hashes and noise counts, commit hashes (git keeps
    them), single timing runs whose medians are on 18–21, and tokens present elsewhere in
    other words. Identifiers: `/tmp` paths, scratch binaries, line ranges already in 22,
    the compare link (the PR's). The few handoff-only facts it surfaced are carried above.
  - *The `docs/` side:* on the pages the close changed, every row is intentional — the
    Current-focus, Track 1 and milestone rewrites (the #57 row out of the window, its
    record [21/04](../21-cli-consolidation/04-mesh-node-bake.md)), 17's condensed 15 entry
    (15's headings hold it), design/10's two edits, 21's row 09. About 10,050 rows of
    unchanged files are the known noise.
- **The gates.** `cmake --build build --target clean`, then `check.py --clean`: 254 TUs, 0
  DualC-origin warnings, build 248 s, ctest **369 / 369** (152 s). `cmake --build build-gl -j
  && check.py --gpu --strict --build-dir build-gl`: parity **82 / 82**. `check.py --docs
  --strict` green.
- **`plan/` is not deleted here.** The driver still reads the manifest and this unit's
  handoff. The deletion (D-51) is one commit after the driver marks P08 done.

---

← Back to [Code audit & engine hardening](README.md) · the [Roadmap index](../README.md).
