# `dualc_io_stress` on `windows-2022` — the five experiment samples (roadmap 17 #52)

Raw observations, all of 2026-10-07, of `.github/workflows/io-stress.yml` on branch `exp/io-stress-windows`,
copied from each job's check-run annotations the same day (the step summary of a public
run needs a login; annotations do not — every line below is verbatim from
`api.github.com/repos/Giacogiak/DualC/check-runs/<job>/annotations`). The harness is
`examples/dualc_io_stress.cpp`; the record that reads these is
[17/16](../roadmap/17-code-audit-and-hardening/16-windows-rename-race/README.md). Every job:
`python scripts/check.py --io-stress --strict --build-dir build --io-stress-args "<args>"`,
Visual Studio 17 2022 x64 Release on Windows, Ninja Release on Ubuntu, the harness built
from the commit named. Defaults from `scripts/check_data.json`: concurrency 6, iterations 30
(180 exports); the field is Boletus's `intersection(onion(gyroid(wavelength=0.5),thickness=0.12),box(min=[-1,-1,-1],max=[1,1,1]))`
at depth 6, tile depth 4; even workers `writeFieldTiledStl`, odd `writeField` (`.stl`).

## Sample 1 — run 37609636680, commit `64a8e84`

Pushed 2026-10-07 10:46 UTC.

<https://github.com/Giacogiak/DualC/actions/runs/37609636680>

| Job | Args | Started → completed (UTC) | Conclusion | Annotations |
| --- | --- | --- | --- | --- |
| `scanner-on` (windows-2022), job 112753474809 | defaults | 10:46:43 → 10:55:12 | success | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `scanner-off` (windows-2022), job 112753474680 | defaults | 10:46:44 → 10:54:50 | success | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `sequential` (windows-2022), job 112753474520 | `--concurrency 1 --iterations 60` | 10:46:43 → 10:53:31 | success | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `force` (windows-2022), job 112753474793 | `--force-hold through --iterations 5` | 10:46:43 → 10:52:40 | **failure** (as predicted) | below |
| `linux-control` (ubuntu-24.04), job 112753474897 | defaults | 10:46:43 → 10:50:35 | success | `1 checks: 1 passed, 0 failed, 0 skipped` |

The workflow at this commit put the full report in the step summary only, so for the
green jobs the one readable line is the closing count: a green `io-stress` check means
the tally read `0 failed … 0 stray .part` (the harness exits 1 otherwise) over the
configured 180 exports (60 for `sequential`). The environment printout (Defender status,
`%TEMP%`) was likewise unreadable for this sample; sample 2 onward emits both as notices.

`force` — every annotation, verbatim (ten per level is GitHub's cap; the job ran 5
iterations × 6 = 30 exports and the tally says all 30 failed the same way):

```
notice  | 1 checks: 0 passed, 1 failed, 0 skipped
failure | io-stress: FAIL #1/0 tiled rc=2 site=commit ms=3099 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_1_0.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_1_0.stl': The process cannot access the file because it is being used by another process. | probe=holder: held .part from 0ms to 3107ms | rename ok@0ms after 0 failures
failure | io-stress: FAIL #0/5 mono rc=2 site=commit ms=1707 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_5.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_5.stl': The process cannot access the file because it is being used by another process. | probe=holder: held .part from 1657ms to 1719ms | rename ok@4ms after 0 failures
failure | io-stress: FAIL #0/4 tiled rc=2 site=commit ms=2966 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_4.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_4.stl': The process cannot access the file because it is being used by another process. | probe=holder: held .part from 218ms to 2986ms | rename ok@0ms after 0 failures
failure | io-stress: FAIL #0/3 mono rc=2 site=commit ms=1723 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_3.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_3.stl': The process cannot access the file because it is being used by another process. | probe=holder: held .part from 1273ms to 1319ms | rename ok@0ms after 0 failures
failure | io-stress: FAIL #0/2 tiled rc=2 site=commit ms=2974 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_2.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_2.stl': The process cannot access the file because it is being used by another process. | probe=holder: held .part from 185ms to 2984ms | rename ok@0ms after 0 failures
failure | io-stress: FAIL #0/1 mono rc=2 site=commit ms=1749 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_1.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_1.stl': The process cannot access the file because it is being used by another process. | probe=holder: held .part from 1719ms to 1749ms | rename ok@0ms after 0 failures
failure | io-stress: FAIL #0/0 tiled rc=2 site=commit ms=2981 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_0.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_7216_0_0.stl': The process cannot access the file because it is being used by another process. | probe=holder: held .part from 16ms to 3001ms | rename ok@0ms after 0 failures
failure | io-stress: dualc_io_stress: baseline tiled=163740 facets (1115ms), mono=163740 facets (925ms)
failure | io-stress: dualc_io_stress: field=boletus-gyroid-box depth=6 tile_depth=4 concurrency=6 iterations=5 dir=C:\Users\RUNNER~1\AppData\Local\Temp\ hw_threads=4 force_hold=through
failure | io-stress: 30 exports, 30 failed (open 0, finish 0, write 0, commit 30, count 0, other 0), 0 delete failures, 0 stray .part
failure | Process completed with exit code 1.
```

Read: the message is Windows error 32, `ERROR_SHARING_VIOLATION`, from the rename; the
`.part` was still on disk when the export returned (`part_after=y`), so the destructor's
`std::remove` had failed silently under the same hold; the probe's rename succeeded at
once because at this commit the harness joined the holder *before* probing (fixed in
`20b0a85`, sample 2), which is also why no `error_code` tuple was printed; the two
baseline counts equal Boletus's golden 163,740.

## Sample 2 — run 37611188182, commit `20b0a85`

Pushed 2026-10-07 ~11:05 UTC.

<https://github.com/Giacogiak/DualC/actions/runs/37611188182>

Harness change: the forcing holder keeps the `.part` 150 ms past the export's return so the
probe meets it. Workflow change: the key lines and the Defender status go out as
annotations. The new "Key lines as annotations" step itself failed on every job of this
sample (its grep was anchored at column 0 while `check.py` indents its detail lines; under
`pipefail` the empty match ended the step with exit 1) — so every job shows `failure` as
its conclusion while its *stress step* reads from the `io-stress` check's own closing
count; fixed in `51f2b22` (sample 4).

| Job | Args | Stress result (the `1 checks:` notice) |
| --- | --- | --- |
| `scanner-on`, job 112758549697 | defaults | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `scanner-off`, job 112758549685 | defaults | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `sequential`, job 112758549694 | `--concurrency 1 --iterations 60` | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `force`, job 112758549379 | `--force-hold through --iterations 5` | `1 checks: 0 passed, 1 failed, 0 skipped` (as predicted) |
| `linux-control`, job 112758549771 | defaults | `1 checks: 1 passed, 0 failed, 0 skipped` |

**The environment, as every Windows job of this sample printed it** (the `env` notices,
verbatim; identical on `scanner-on`, `sequential` and `force`, i.e. *before* any toggle):

```
TEMP=C:\Users\RUNNER~1\AppData\Local\Temp
CPUs=4
RealTimeProtectionEnabled : False
IsTamperProtected         : False
AntivirusSignatureVersion : 1.459.588.0
DisableRealtimeMonitoring : True
ExclusionPath             : {C:\, D:\}
Status    : Stopped            (the WSearch service)
```

`force` — the first FAIL line verbatim (the other six differ only in path, timings and the
probe's attempt count, 5–7 failures before success at 161–203 ms):

```
FAIL #1/0 tiled rc=2 site=commit ms=3424 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_4992_1_0.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_4992_1_0.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@185ms after 6 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'} | holder: held .part from 20ms to 3593ms
dualc_io_stress: baseline tiled=163740 facets (1157ms), mono=163740 facets (900ms)
dualc_io_stress: field=boletus-gyroid-box depth=6 tile_depth=4 concurrency=6 iterations=5 dir=C:\Users\RUNNER~1\AppData\Local\Temp\ hw_threads=4 force_hold=through
30 exports, 30 failed (open 0, finish 0, write 0, commit 30, count 0, other 0), 0 delete failures, 0 stray .part
```

Read: the `error_code` MSVC hands `std::filesystem::rename` for a sharing violation is
`{32, system}` with default condition 13 = `std::errc::permission_denied`; the probe's
rename landed the file 10–50 ms after the holder let go. And the on-access scanner the
whole hypothesis rested on is **not running on this runner image**: real-time protection
is disabled, `C:\` and `D:\` are excluded, the indexer is stopped. The `scanner-off` A/B
is therefore void by construction (it toggles what is already off), and whatever holds
Boletus's freshly closed files on the same image is something else.

## Sample 3 — run 37611624199, commit `04bfae0`

Pushed 2026-10-07 ~11:20 UTC.

<https://github.com/Giacogiak/DualC/actions/runs/37611624199>

Adds the `fresh-process` job (`--no-baseline --iterations 1`, fifteen processes, tallies
summed by `check.py --io-stress-repeat 15`): no sequential warm-up, the first six files
each process writes are the concurrent ones. The key-lines step still carried the grep
defect, so every conclusion reads `failure`; the stress results are the `io-stress`
check's closing counts:

| Job | Args | Stress result |
| --- | --- | --- |
| `scanner-on`, job 112759965380 | defaults | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `scanner-off`, job 112759965647 | defaults | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `sequential`, job 112759965575 | `--concurrency 1 --iterations 60` | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `fresh-process`, job 112759965716 | `--no-baseline --iterations 1` × 15 processes | `1 checks: 1 passed, 0 failed, 0 skipped` |
| `force`, job 112759965820 | `--force-hold through --iterations 5` | `1 checks: 0 passed, 1 failed, 0 skipped` — `30 exports, 30 failed (open 0, finish 0, write 0, commit 30, count 0, other 0), 0 delete failures, 0 stray .part`, every line `cannot move … The process cannot access the file because it is being used by another process.` |
| `linux-control`, job 112759965791 | defaults | `1 checks: 1 passed, 0 failed, 0 skipped` |

## Sample 4 — run 37611755654, commit `51f2b22`

Pushed 2026-10-07 ~11:25 UTC.

<https://github.com/Giacogiak/DualC/actions/runs/37611755654>

The key-lines step fixed; from here the `io-stress` check's own line is an annotation on
every job (verbatim below, the trailing number is the check's duration).

| Job | Args | The `io-stress` line |
| --- | --- | --- |
| `scanner-on`, job 112760384788 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 108.96s` |
| `scanner-off`, job 112760386023 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 105.57s` |
| `sequential`, job 112760384790 | `--concurrency 1 --iterations 60` | `[OK  ] io-stress          60 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 51.58s` |
| `fresh-process`, job 112760384463 | `--no-baseline --iterations 1` × 15 | `[OK  ] io-stress          90 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part over 15 processes 50.04s` |
| `force`, job 112760384787 | `--force-hold through --iterations 5` | failure, as in samples 1–3 (30/30 at `commit`, error 32) |
| `linux-control`, job 112760386022 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 62.26s` |

## Sample 5 — run 37612620026, commit `a9ff9d9`

Pushed 2026-10-07 ~11:50 UTC.

<https://github.com/Giacogiak/DualC/actions/runs/37612620026>

Adds the `child-inherit` job: `--spawn-child --iterations 10` — during the rounds the
harness keeps one child process alive at all times (itself, `--child-sleep 2500`, started
with `CreateProcessW(…, bInheritHandles = TRUE, …)`, what .NET's `Process.Start` does when
a standard stream is redirected).

| Job | Args | The `io-stress` line |
| --- | --- | --- |
| `scanner-on`, job 112763282050 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 101.73s` |
| `scanner-off`, job 112763282367 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 102.63s` |
| `sequential`, job 112763282210 | `--concurrency 1 --iterations 60` | `[OK  ] io-stress          60 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 51.96s` |
| `fresh-process`, job 112763281877 | `--no-baseline --iterations 1` × 15 | `[OK  ] io-stress          90 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part over 15 processes 56.88s` |
| `force`, job 112763282281 | `--force-hold through --iterations 5` | failure, as in samples 1–4 |
| **`child-inherit`**, job 112763282301 | `--spawn-child --iterations 10` | **`[FAIL] io-stress          60 exports, 30 failed (open 0, finish 0, write 0, commit 30, count 0, other 0), 0 delete failures, 0 stray .part 55.89s`** |
| `linux-control`, job 112763282499 | defaults | success |

`child-inherit` — the annotations, verbatim (ten of the thirty FAIL lines fit the cap; every
one of the thirty is a **tiled** export, every monolithic export of the same rounds passed):

```
dualc_io_stress: field=boletus-gyroid-box depth=6 tile_depth=4 concurrency=6 iterations=10 dir=C:\Users\RUNNER~1\AppData\Local\Temp\ hw_threads=4 force_hold=off spawn_child=on
dualc_io_stress: baseline tiled=163740 facets (1145ms), mono=163740 facets (961ms)
FAIL #0/0 tiled rc=2 site=commit ms=3553 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_0_0.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_0_0.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@2036ms after 65 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'}
FAIL #0/2 tiled rc=2 site=commit ms=3549 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_0_2.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_0_2.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@2025ms after 65 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'}
FAIL #0/4 tiled rc=2 site=commit ms=3554 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_0_4.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_0_4.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@2036ms after 65 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'}
FAIL #1/0 tiled rc=2 site=commit ms=3521 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_1_0.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_1_0.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@1525ms after 49 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'}
FAIL #1/2 tiled rc=2 site=commit ms=3534 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_1_2.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_1_2.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@1528ms after 49 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'}
FAIL #1/4 tiled rc=2 site=commit ms=3533 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_1_4.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_1_4.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@1529ms after 49 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'}
FAIL #2/0 tiled rc=2 site=commit ms=3472 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_2_0.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6252_2_0.stl': The process cannot access the file because it is being used by another process. | probe=rename ok@1608ms after 52 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'}
60 exports, 30 failed (open 0, finish 0, write 0, commit 30, count 0, other 0), 0 delete failures, 0 stray .part
```

Read: with a child process alive that inherited the handles open at its start, **every tiled
export fails at the rename with error 32 and every monolithic export succeeds** — the tiled
writer holds its `.part` open for the whole contour (~3.5 s here), the monolithic writer for
the milliseconds of its write, so only the former is ever open when a child starts. The
probe's rename lands 1.5–2.0 s later, i.e. when the child (2.5 s of life) exits and its
inherited handle closes; a retry bounded in the hundreds of milliseconds would not have
saved one of them. The `.part` survives the destructor's remove for the same reason and
is left behind until the probe moves it.
