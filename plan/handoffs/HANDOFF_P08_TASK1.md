# Handoff — P08.1: run every gate and repair until green

Unit P08.1 of [`plan/17_P08_gates-and-close.md`](../17_P08_gates-and-close.md) § T1. Session of
2026-10-22 on branch `feat/porous-shell` in `~/Documents/DualC-porous`. Nothing was pushed, no
remote touched, `main` / `feat/hygiene-and-editor` / `~/Documents/DualC` / Boletus left alone.

**Commits:**

- `b8fbc08` — `field_graph.cpp`: `readLiftParam`'s comment back above its function (P04.1 had
  inserted the porous readers between them, leaving the comment over `choiceParam`).
- `b0e7560` — the lint's fixes: new `docs/design/13-porous-shell.md`; design/12's library bullets
  moved there; design/06's intro; the design index; one pointer line in 23/03, 23/04, 23/05;
  `porous.h`, `porous_complex.h` and `porous_complex.cpp` comments point at design/13.
- `8f37ee9` — semantic-lint run 10 and its index row; `docs/roadmap/23-porous-shell/10-plan-run-record.md`
  (the gates, what stays open, the review items) and its line in the block README.
- The commit that adds this handoff, which also restores one clause the loss audit found lost
  in the design/12 → design/13 move (below).

## Each gate's final line

All four gates are green. These are the final runs, after the last code commit, unless noted.

| Gate | Command | Final line |
| --- | --- | --- |
| 1. Builds | `cmake --build build -j && cmake --build build-gl -j` | `ninja: no work to do.` (both) |
| 2. Full gate | `python3 scripts/check.py` | `32 checks: 29 passed, 0 failed, 2 skipped` / `PASS` — `ctest 408 tests, 0 failed` (190.5 s, serial); `configure` SKIP (configured), `warnings` SKIP (incremental, 0 TUs) |
| 2a. Warning scan | same, on a cleaned `build/` (`cmake --build build --target clean` first), before the commits | `32 checks: 30 passed, 0 failed, 1 skipped` / `PASS` — `warnings 260 TUs compiled, 0 DualC-origin warnings`; build 244.7 s, ctest 408/408 in 149.9 s, 6 min 36 s in all |
| 3. GPU tier | `python3 scripts/check.py --gpu --strict --build-dir build-gl` | `parity 87/87 passed` / `1 checks: 1 passed, 0 failed, 0 skipped` / `PASS` |
| 4. Docs strict | `python3 scripts/check.py --docs --strict` | `28 checks: 27 passed, 0 failed, 0 skipped` / `PASS` |
| 5. Semantic lint | `/docs-semantic-lint`, a delta over run 09 | run 10: `(a) 2 · (b) 2 · (c) 1 · (d) 0 · (e) 1 — 6 findings, 3 fixed, 3 the owner's` |

Between 2a and 2, the code changed only in comments (`field_graph.cpp`, `porous.h`,
`porous_complex.{h,cpp}`), so the clean warning scan still holds. After those edits
`ctest -R 'porous|field_graph|field_glsl'` gave 39/39 (24.2 s); the driver's smoke filter
is covered by the full 408.

## What changed, and why

No gate failed. Only the semantic lint (step 5) produced findings
(`docs/roadmap/19-docs-layers/08-semantic-lint/10-2026-10-22-after-porous-run.md`):

1. **(a)** design/06's intro still said "the one piece of third-party code". § 8 lists three.
   Fixed; the heading, an anchor, stays.
2. **(a)** design/12 defined the struts' `B` as "the larger spacing, or the largest cell radius
   if larger". Past `kPorousMaxBins` the bins coarsen by 1.25× and `B` grows with them. Fixed
   in design/13 § The packed layout.
3. **(c)** The porous library's present-tense contract existed only in the record's as-built
   pages (23/03–05), and `porous.h` named a roadmap page as the definition of its layout.
   Fixed by a new **design/13-porous-shell.md** (9.5 KB):
   - the three build stages;
   - the packed layout and `B`;
   - the query and its fold;
   - the field's contract;
   - determinism;
   - a constants table.

   design/12 keeps the graph's three rules: build-time volume + `childBakeMargin`, no cancel,
   one builder for the preview. Its four library bullets moved to 13. Every fact was re-read
   against the code (`porous.h`, `porous.cpp`, `porous_complex.{h,cpp}`, `porous_seeds.cpp`,
   `porous_delaunay.{h,cpp}`).
4. **(b) ×2, handed to the owner:** design/10 is at exactly 15,360 B, the cap, and two things
   are missing from it:
   - the porous `cancel`-only drivers, from § Hooks;
   - the porous constants, from § The numeric constants.

   Adding either means splitting design/10 into a folder. That retargets about 50 links in
   about 35 files, too much for a close. design/13 states both meanwhile.
   [23/10 § Open after the gates](../../docs/roadmap/23-porous-shell/10-plan-run-record/README.md#open-after-the-gates)
   item 1.
5. **(e), handed to the owner:** D-53's trigger reads "a change to their window or snapshot
   code". #66 added two things, and is arguably that change:
   - a recompile in `dualc_field_view` between window creation and the first frame;
   - a shared data-texture upload.

   23/10 item 2.

## Measured, and under what

The owner's host: Ubuntu, GCC 15, CMake 4, Ninja, 8 cores, 22 GB, `free -g` 16 GB free at the
last gate. One heavy process at a time, `ctest` serial, never `-j`. The GPU tier ran under
`timeout 900 prlimit --as=8000000000`. Timings are in the table above.

**The renderer.** Before the reboot (below), X was still hung by P05.1's NV92 trap: `glxinfo`
hung, and I killed it. So the first GPU run went through P05.2's scratch EGL-surfaceless
harness:

- re-created in `/tmp/p08`, not committed;
- linked against the current `build-gl` objects;
- run with `LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless`;
- result: 87/87.

**The host rebooted mid-unit** (uptime 8 min at 11:42, cause unknown to this session, and no
heavy process was running). After it, X answers and `glxinfo -B` reports **llvmpipe**
("Accelerated: no"). So the committed `build-gl/examples/dualc_glsl_parity` ran the gate as
the plan writes it, with "GL renderer: llvmpipe (LLVM 21.1.8)": 87/87, twice. The NV92 ran no
case in this run.

## What I got wrong on the way

- **`pkill -f "glxinfo -B"` killed my own shell**: the pattern matched its command line
  (exit 144). Use `pgrep -a` then `kill <pid>`.
- **The first design/13 draft used "since"**, which `design-no-history` rejects. The
  pre-commit hook stopped the commit; I reworded it to "as".
- **I lost the scratch files to the reboot.** `/tmp/p08` and `/tmp/p052` went with it, and
  with them the first gate logs and P05.2's EGL harness. The numbers above are as printed at
  the time, and every gate was re-run after the reboot except the clean warning scan (2a).
- **The first loss audit was too slow.** Run on `--scope docs/` with `HEAD~1`, it had used
  6 CPU-minutes when I stopped it. A narrower `--scope` is refused (every base docs file
  must be in scope). It was re-run with explicit SHAs; the result is below.

## Loss audit

Run **after** `b0e7560` was committed: the tool compares two commits, and I committed the move before auditing it, against the rule to audit before committing a move. It ran over the one commit that moved docs text
(design/12 → design/13): `python3 scripts/docs_loss_audit.py --base b8fbc08 --head b0e7560
--scope docs/ --untriaged`. Result: the full report is noisy for this use (it also counts unchanged pages' raw-only and identifier rows: 10,655 untriaged across the tree), so I read the rows whose source is design/12 (`--source docs/design/12-field-graph.md --only sentence evidence`, with a scratch triage file `/tmp/p08/triage.json`, since the repo's `docs_loss_audit.json` has no audit for this base). Of the 8 rows (7 sentences), 6 are restated in design/13 (the Lipschitz argument and its pointer, the struts' cap and refusal, the walks, the bin bound, determinism) or in design/12's remaining bullets (the ceilings). One clause was lost: "`smooth` only lowers the value, by less than `k`". It is restored in design/13 § The query, in the commit that adds this handoff. No triage entry is committed.

## What is left open

1. design/10's split, or accepting design/13 as the home of the porous hooks and constants
   (lint run 10 #3, #4). This is the owner's call.
2. D-53: reopen it (a `--gpu`-tier `--snapshot` smoke check) or record why not (run 10 #6).
   This is the owner's call.
3. `index-staleness` (INFO, report-only) gains
   `docs/roadmap/23-porous-shell/README.md: committed after docs/roadmap/README.md`. It
   clears when P08.2 updates `docs/roadmap/README.md` (§ Current focus, § Next up, the
   milestones row). That page is 111 B under its cap.

## For the owner's review

**needsHumanCheck is raised**, for two things:

1. **The NV92 is untested by this run.** Parity ran on llvmpipe only, the same renderer as
   CI. If the NV92 becomes the active renderer again (`glxinfo -B` naming nouveau), run
   P05.2's one-case check first, as
   [23/08 § For the owner's review](../../docs/roadmap/23-porous-shell/08-parity-and-viewer.md#for-the-owners-review)
   writes it. Then run
   `python3 scripts/check.py --gpu --strict --build-dir build-gl`; 87/87 is expected.
   Verified headless: 87/87 on llvmpipe, twice.
2. **design/13 is new design prose written at the close.** Read it against
   `include/dualc/porous.h` and 23/03–05:
   `less docs/design/13-porous-shell.md`.

## What the next unit (P08.2) needs to know

- The block's plan-run record page exists: **23/10** (`10-plan-run-record.md`). Its sections
  are:
  - § The gates at the close (P08.1);
  - § Open after the gates;
  - § For the owner's review.

  P08.2 adds the per-unit table, the measurements, the plan-file corrections and the lessons.
  23/10 is 3.8 KB. If it outgrows the cap, the page becomes a folder.
- The next free semantic-lint run is 11, and the next design page is 14. No `#N` or `D-NN`
  was opened by this unit (the open items are in 23/10, not new IDs).
- `docs/roadmap/README.md` is untouched, and still reads P07's state.
- The plan file's T1 criteria held as written. No correction.
