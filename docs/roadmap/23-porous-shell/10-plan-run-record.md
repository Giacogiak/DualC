# The porous plan-run — the gates at the close and the run's record

The close of the unattended porous shell run (`plan-run`, branch `feat/porous-shell`, P08):
the gates P08.1 ran and repaired on 2026-10-22, and what stays open after them. P08.2 appends
the harvest of the run's handoffs (the per-unit table, the measurements, the corrections to
the plan files, the lessons) and the owner's review list.

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
| Semantic lint | `/docs-semantic-lint`, delta over run 09 | [run 10](../19-docs-layers/08-semantic-lint/10-2026-10-22-after-porous-run.md): (a) 2 · (b) 2 · (c) 1 · (d) 0 · (e) 1 — 3 fixed, 3 below |

**The GPU tier's renderer.** Before the reboot X was still hung by P05.1's NV92 trap, so the
tier first ran through P05.2's scratch EGL-surfaceless harness on llvmpipe ([08 § The GPU
tier](08-parity-and-viewer.md#the-gpu-tier-run-on-llvmpipe)), 87/87. After the reboot X
answers, and its renderer is Mesa llvmpipe (`glxinfo -B`: "Accelerated: no"), so the committed
`build-gl` harness ran the gate as written, 87/87, on llvmpipe — CI's renderer. The NV92 itself
ran no case in this run.

**Repairs.** No gate failed. The lint's fixes: `b0e7560` (a new
[design/13](../../design/13-porous-shell.md) for the porous library's contract, design/12's
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

## For the owner's review

- **The NV92.** Run P05.2's one-case check on the GPU when it is the active renderer
  ([08 § For the owner's review](08-parity-and-viewer.md#for-the-owners-review)); this run's
  parity is llvmpipe's. Verified headless: 87/87 on llvmpipe, twice.
- **design/13** is new prose written from the code at the close: read it against
  `include/dualc/porous.h` and the record pages 03–05 it summarises.

---

← Back to [block 23](README.md).
