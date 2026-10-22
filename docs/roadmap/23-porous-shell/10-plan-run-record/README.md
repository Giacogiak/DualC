# The porous plan-run — the gates at the close and the run's record

The close of the unattended porous shell run (`plan-run`, branch `feat/porous-shell`, P08):
the gates P08.1 ran and repaired on 2026-10-22, and what stays open after them. P08.2 appends
the harvest of the run's handoffs (the per-unit table, the measurements, the corrections to
the plan files, the lessons) and the owner's review list.

*2026-10-22, P08.2:* the harvest would take this page past the size cap, so the page became
this same-numbered folder (principle 5). This README keeps the gates, what stays open and the
per-unit table; the rest is in the pages below.

| Page | Holds |
| --- | --- |
| [01](01-measurements-and-lessons.md) | the counts the run moved, the measurements only the handoffs held, the corrections to the plan files, the lessons |
| [02](02-in-run-choices.md) | every *Chosen in-run (pending owner review)* of the run, by unit, with its record page |
| [03](03-owners-review.md) | the owner's review list: what needs a person, the decisions owed, the snapshots and cavity counts, the Boletus plan, the counts, the push, the compare link and the PR text |

## The gates at the close (P08.1)

Run on the owner's host (Ubuntu, GCC 15, CMake 4, Ninja, 8 cores, 22 GB), one heavy process
at a time. The host rebooted during the unit (between the lint's read and its fixes); every
gate below that was run before the reboot was run again after the last commit.

| Gate | Command | Final line |
| --- | --- | --- |
| Builds | `cmake --build build -j && cmake --build build-gl -j` | both up to date, no warnings |
| Full gate | `python3 scripts/check.py` on a cleaned `build/` (`cmake --build build --target clean` first, so the warning scan has input) | `32 checks: 30 passed, 0 failed, 1 skipped` — `PASS`; `warnings` 260 TUs, 0 DualC-origin warnings; `ctest` 408 tests, 0 failed (150 s, serial); `configure` SKIP (configured) |
| GPU tier | `python3 scripts/check.py --gpu --strict --build-dir build-gl` | `parity 87/87 passed` — `1 checks: 1 passed` — `PASS` |
| Docs, strict | `python3 scripts/check.py --docs --strict` | `28 checks: 27 passed, 0 failed, 0 skipped` — `PASS` (the INFO `index-staleness` list, 15 pages, all outside this block and older than this run) |
| Semantic lint | `/docs-semantic-lint`, delta over run 09 | [run 10](../../19-docs-layers/08-semantic-lint/10-2026-10-22-after-porous-run.md): (a) 2 · (b) 2 · (c) 1 · (d) 0 · (e) 1 — 3 fixed, 3 below |

**The GPU tier's renderer.** Before the reboot X was still hung by P05.1's NV92 trap, so the
tier first ran through P05.2's scratch EGL-surfaceless harness on llvmpipe ([08 § The GPU
tier](../08-parity-and-viewer.md#the-gpu-tier-run-on-llvmpipe)), 87/87. After the reboot X
answers, and its renderer is Mesa llvmpipe (`glxinfo -B`: "Accelerated: no"), so the committed
`build-gl` harness ran the gate as written, 87/87, on llvmpipe — CI's renderer. The NV92 itself
ran no case in this run.

**Repairs.** No gate failed. The lint's fixes: `b0e7560` (a new
[design/13](../../../design/13-porous-shell.md) for the porous library's contract, design/12's
library bullets moved there, design/06's intro), and one code comment `b8fbc08`
(`readLiftParam`'s comment, split from its function when the porous readers were inserted
above it).

## Open after the gates

1. **design/10 is at the 15,360 B cap** and lacks the porous hooks (the seeds, the tets, the
   complex and `PorousField` take `cancel` only) and the porous constants (lint run 10 #3,
   #4). Adding either needs design/10 split into a same-numbered folder (principle 5), which
   retargets about 50 links across 35 files — not a close-of-run edit. design/13 states both
   meanwhile. The owner's: split, or accept design/13 as their home and narrow design/10's
   "every numeric constant" claim.
2. **D-53's trigger, arguably met** (lint run 10 #6): #66 changed `dualc_field_view` between
   window creation and the first frame, and the data-texture upload both GL tools share. The
   owner's: reopen D-53 (a `--gpu`-tier `--snapshot` smoke check) or record why not.

*2026-10-22, P08.2:* the handoffs left four more, none blocking the PR:

3. **The crisp `intersection` rim on a curved volume** leaves 2–16 non-manifold edges, an
   engine limit older than this block ([06 finding 1](../06-sponge-end-to-end.md#what-the-cases-showed)).
   It has no tracked item and no design/10 line, both homes being at the cap (P04.2). The
   owner's: open #70, or the design/10 line with item 1's split.
4. **`relax` ignores `--threads`** and the ABI's thread count: Lloyd runs on every hardware
   thread at create time, the output identical at any count (P06.1,
   [02 #67](../02-record.md#67-the-c-abi-test-and-the-documentation)). It sits beside
   [#62](../../../decisions/README.md); Boletus's plan names create-time threads D-51 on its side.
5. **No committed headless GL route.** P05.2 and P08.1 ran the GPU tier through a scratch EGL
   surfaceless context while X was hung ([08](../08-parity-and-viewer.md#the-gpu-tier-run-on-llvmpipe)).
   An `--egl` mode for the harness is the owner's call.
6. **What only CI can confirm:** the pinned digests (seeds, tets, packed arrays) on macOS and
   Windows, MSVC's build of the vendored `predicates.c` with its shim
   ([03](../03-delaunay-engine.md)), MSVC `/W4` on the porous C++ sources (written with
   explicit casts, no shadowing, `std::snprintf`; never compiled by MSVC), and the component
   test's seed 3 being watertight ([06](../06-sponge-end-to-end.md#the-in-run-choices); its
   other seeds' open edges are a membrane under a cell thick, not a floating-point tie, so
   another OS should not flip it).
   How to read a tet digest failure: [03 § The tests](../03-delaunay-engine.md#the-tests). The branch's first CI run decides; if only the struts
   digests fail, look at `voronoiEdge`'s line parameters first.

## For the owner's review

- **The NV92.** Run P05.2's one-case check on the GPU when it is the active renderer
  ([08 § For the owner's review](../08-parity-and-viewer.md#for-the-owners-review)); this run's
  parity is llvmpipe's. Verified headless: 87/87 on llvmpipe, twice.
- **design/13** is new prose written from the code at the close: read it against
  `include/dualc/porous.h` and the record pages 03–05 it summarises.
- *2026-10-22, P08.2:* every item of the run for the owner, with its command, is gathered
  in [03](03-owners-review.md).

## The run — P01 to P08, harvested

**DONE — 2026-10-22** on the local branch: #63–#68 DONE, #69 DEFERRED with its trigger, D-67 …
D-75 written (D-74 and D-75 pending owner review). Twelve unattended units, 2026-10-21 …
2026-10-22, each a fresh session closing with a handoff under `plan/handoffs/`. Nothing was
pushed: the push, the PR and the CI reading are the owner's (`main` is protected, PR only).

| Unit | Item | Record | Commit |
| --- | --- | --- | --- |
| P01.1 | the study, the block, D-67 … D-75, #63 … #69 opened, the build trees | [01](../01-study.md), [02](../02-record.md) | `5cd4399` |
| P02.1 | #63 the seeds | [09](../09-seeds-as-built.md) | `3c94d02` |
| P03.1 | #64 Shewchuk's predicates vendored, the tetrahedralization | [03](../03-delaunay-engine.md) | `5d915e3`, `e3479ca` |
| P03.2 | #64 the packed complex, its bins, the query | [04](../04-complex-and-bins.md) | `a7a05ce` |
| P04.1 | #65 `PorousField` and the `porous` token | [05](../05-porous-field.md) | `75990cc`, `5a7d74d` |
| P04.2 | #65 the sponge end to end, three samples (second attempt) | [06](../06-sponge-end-to-end.md) | `b02ef48` |
| P05.1 | #66 the GLSL emitter, data textures | [07](../07-glsl-node.md) | `94139e7` |
| P05.2 | #66 parity 87, the viewer's queried limits, the snapshots | [08](../08-parity-and-viewer.md) | `b49f148`, `829cc43` |
| P06.1 | #67 the ABI test, the porous docs, #63's detail to 09 | [02 #67](../02-record.md#67-the-c-abi-test-and-the-documentation) | `82183c7`, `f2fe93b` |
| P07.1 | #68 the Boletus plan (Boletus `1ccbe23`) | [02 #68](../02-record.md#68-the-boletus-implementation-plan) | `22a0a9f` |
| P08.1 | the gates, lint run 10, design/13 | this page | `b8fbc08`, `b0e7560`, `8f37ee9` |
| P08.2 | the harvest, the statuses, the review list | 01 – 03 | the harvest commit |

**How the run went.**

- **P01.1 stopped on a path, not on its work** (2026-10-21 18:00): it wrote its handoff under
  `plan/handoffs/`, as the split said, while the manifest's per-unit `handoff` paths named the
  repo root, so the driver's `handoff` check failed. The owner fixed the twelve paths
  (`9b1b4f1`), re-verified P01.1 with `planRun.mjs verify P01.1` and relaunched (20:33).
- **No unit stopped on a question** ([D-73](../../../decisions/03-settled-continued.md)): every
  open choice took the study's defaults table or was made and written for review
  ([02](02-in-run-choices.md)).
- **P04.2's first attempt was OOM-killed** (2026-10-21 22:27): a `--depth 8` sponge went past
  the free RAM and systemd-oomd killed the session tree with the run — the whole VS Code
  scope the driver had been started from, so nothing ran until the morning. The basis: a
  unit-sphere sponge took 2.1–3.3 GB at `--depth 7` on this 22 GB, 8-core host. The owner
  added the memory rule (`b1865d7`): one heavy process at a time and never `ctest -j` on
  contouring cases, `prlimit --as=8000000000` on ad-hoc runs, depth ≤ 7 (a larger depth only
  as a measured exception after `free -g`), ctests under 1 GB, mesh checkers streaming or under
  ~2M faces. The driver was relaunched in its own `systemd-run --user --scope` with
  `MemoryMax=15G` and `OOMPolicy=continue`, so an oversized case dies alone; the second
  attempt kept to the rule.
- **P05.1 hung the host's NV92** with a first GLSL draft (a `STACK_LIMIT` trap,
  [07](../07-glsl-node.md#a-first-draft-hung-this-hosts-gpu)). X stayed hung (the PGRAPH
  timeout at 10:07; `glxinfo` hung too) and the owner force-rebooted the host at about 11:33,
  during P08.1. P08.1 was resumed in its own session (`planRun.mjs finish P08.1`, nothing
  lost: its uncommitted design/13 draft survived on disk), and the rest of the run had
  `LIBGL_ALWAYS_SOFTWARE=1` forced on its scope. Every GL run of P05.2 and P08.1 was on
  llvmpipe, and the NV92 ran no committed case.
- **P08.2 stopped with its harvest staged, not committed:** a background wait loop
  (`until ! pgrep -f "docs_loss_audit.py --base 0d2a8e8"`) matched its own command line and
  ran to the tool's time limit, long after the gate and the audit had finished. The session
  reported `failed`; `planRun.mjs finish P08.2` resumed it and it committed.
- **The checks at the end held** (the owner's rule of 2026-10-19, which for this run replaced
  AGENTS.md's full gate before every code commit): P01–P07 ran their own ctests and the docs
  tier, the driver's `smoke` re-ran `porous|field_graph|field_glsl`, and P08 alone ran the full
  gate, `--gpu --strict`, `--docs --strict` and the semantic lint; P08.1's four
  gates were green on their first run, and its only repairs were the lint's.
- **The byte cap shaped the record:** `01-settled.md` full → `03-settled-continued.md`
  (D-75); 02 at 15,357 B → #63's detail moved verbatim to 09 (P06.1); design/10 full →
  design/13 (P08.1); this page → this folder (P08.2).

*2026-10-22, the harvest (P08.2).* What the unit ran before handing over, on the owner's host
(free RAM 15 GB, one heavy process at a time):

- **The gates**, with no code change since P08.1: `check.py` — `32 checks: 29 passed, 0
  failed, 2 skipped`, `PASS`; ctest **408 / 408** (146.9 s, serial); `warnings` skipped as
  incremental (P08.1's clean scan holds: 0 DualC-origin warnings over 260 TUs). `check.py --gpu
  --strict --build-dir build-gl` on llvmpipe — parity **87/87**, `PASS`. `check.py --docs
  --strict` — `PASS`.
- **The loss audit of `plan/`** before its deletion: `docs_loss_audit.py --base 0d2a8e8 --head
  <scratch> --scope docs/ plan/ --annex plan/`, the head a `git commit-tree` of the staged
  harvest with `plan/` removed, a scratch triage with the `3d73220` thresholds and no rules.
  - *The docs side:* every row on a changed page is intentional — the Current-focus rewrite
    (23/01 stays linked from the block README), the #58 milestone row out of the window
    ([21/05](../../21-cli-consolidation/05-2d-profiles-and-lifts.md)), the block README's
    page-10 sentence; the rest is the known whole-tree noise.
  - *The annex:* 1,255 sentence, 346 evidence, 132 heading and 111 identifier rows residual
    — the handoffs paraphrased into 03–09, 23/10 and its pages. A scan for numbers absent
    from `docs/` left page byte counts of a given day, single ctest timings, commit-local
    paths, and a few measurements carried on [01](01-measurements-and-lessons.md#measurements-only-the-handoffs-held)
    (the ABI test's sizes, the rim variants at depth 6, the scale notes, MSVC's D9025).

*2026-10-22, `plan/` deleted (D-51).* Before the deletion, every handoff and the manifest's
rules were checked against `docs/` fact by fact (four read-only audits, three handoffs each).
About twenty facts only the handoffs held were carried in `beba1a3`: the EGL-surfaceless recipe
([08](../08-parity-and-viewer.md#the-gpu-tier-run-on-llvmpipe)), how to read a tet digest
failure ([03](../03-delaunay-engine.md#the-tests)), the run's stops and the lessons above and
in [01](01-measurements-and-lessons.md#what-the-run-taught). The rest was present or run
bookkeeping. The loss audit, run before the commit (`--base beba1a3 --head <scratch>
--scope docs/ plan/ --annex plan/`, a scratch triage with no rules): no row from the two
record pages the commit touches; the annex's residual 1,349 sentence, 382 evidence, 142
heading and 111 identifier rows are the handoffs' wording, triaged by the fact audit above;
the rest is the known whole-tree noise. The split, the manifest and the handoffs stay in git
history (`beba1a3`).

---

← Back to [block 23](../README.md) · the [Roadmap index](../../README.md).
