# #52 — desk analysis, written before the runs

Part of [#52](README.md#52-windows-export-io-flake--the-part-rename-under-an-on-access-scanner):
what was known and predicted on 2026-10-07 before `dualc_io_stress` ran once. Nothing here
was edited after the runs; the evidence and the verdict are on [02](02-evidence-and-verdict.md).

## What Boletus actually observed

Boletus's record is `docs/roadmap/10-public-delivery/03-ci.md` § "The flaky Windows gate"
and the ask is its `07-upstream-coordination/README.md` § 10 (both 2026-10-06, Boletus
pinned at DualC `2fcd19f`; `git diff 2fcd19f main` touches none of the writer or ABI code).

- Four of nine runs on GitHub's `windows-2022` runner lost one file-writing test; never on
  Linux; never the same test twice in a row; a second `dotnet test` pass on the same build
  was always green.
- **One failure was captured with its message.** In `ConcurrencyTests` — six exports at
  once via `Parallel.For`, each to a fresh GUID path under `%TEMP%`, even indices tiled
  STL at tile depth 4, odd monolithic, the field
  `intersection(onion(gyroid(wavelength=0.5),thickness=0.12),box(min=[-1,-1,-1],max=[1,1,1]))`
  at depth 6 — one tiled export returned `DualC error 2 (Io): tiled STL export failed
  (writer or validation error)` while the other five, and four parallel contours, were at
  the golden facet count.
- The two earlier reds (`WrapperTests.ExportTiledStl_…`, `CliParityTests.Wrapper_export_is_byte_identical_to_the_cli`)
  were **not captured**: an rc 2, a C#-side read-back or delete failure, or a CLI exit
  code are all consistent with their log.
- DualC's stderr line naming the OS error has **never been seen** in a failing run.
- "A sharing violation on the rename" is Boletus's inference. Defender real-time
  protection is on by default on GitHub's Windows runners.

## The rc 2 sites, ranked before the runs

The ABI collapses every writer rc other than 0 / 1 / 3 to `DUALC_ERR_IO` with a fixed
message (`capi/dualc_c.cpp`, the two export twins); the text that names the cause goes to
`std::cerr` only. In the tiled driver (`forEachOwnedTileImpl`) rc 2 has exactly three
sources; the non-I/O ones (`tileDepth < 2`, an unknown extension) are excluded by
Boletus's arguments, and an exception would surface as `DUALC_ERR_UNKNOWN`, not 2.

| Rank | Site | Mechanism on `windows-2022` | What stderr shows |
| --- | --- | --- | --- |
| 1 | `AtomicOutput::commit()` — one `std::filesystem::rename` | MSVC's STL calls `MoveFileExW`, which needs DELETE access on the source; `finish()` has just closed a freshly *written* `.part`, the event an on-access scan reacts to, and a scanner handle opened without `FILE_SHARE_DELETE` makes the move fail with `ERROR_SHARING_VIOLATION` (32); a scan-pending denial reads as `ERROR_ACCESS_DENIED` (5). Fresh destination, so replace-existing is not involved. | `[dualc] error: cannot move '<p>.part' to '<p>': <OS text>` |
| 2 | `StreamingStl::finish()` → "failed finalizing" | seek, write the count, flush, close on a handle the writer owns; a reader cannot fail those writes (the stream shares read and write). Only disk-full or a failing final flush. | `[dualc] error: failed finalizing tiled STL '<p>'` — **no OS text** |
| 3 | `sink.open(out.tmp())` → "cannot open" | create-always on a name nothing has ever opened (fresh GUID + `.part`). No mechanism. | `[dualc] error: cannot open '<p>.part' for writing` — **no OS text** |

A secondary consequence of site 1, a defect of its own: after a failed `commit()` the
destructor's `std::remove(tmp_)` also needs DELETE access, its result is ignored, and a
stray `<path>.part` is left behind — which contradicts the `.part` contract on
[command_reference/00](../../../command_reference/00-shared-behaviour.md#the-output-file-appears-only-complete-part)
("a run that fails … removes its `.part`"). The harness checks for this explicitly.

Facts the harness must **verify, not assume**: that MSVC maps 32 and 5 to
`std::errc::permission_denied` (then the portable retry predicate is
`ec == std::errc::permission_denied`, widened by the raw values under `_WIN32`). Every
failure it meets is printed as the full tuple — `value`, `category`, the default
condition, `message()`.

## The alternative hypotheses and what separates them

| | Hypothesis | Discriminator |
| --- | --- | --- |
| H1 | **Boletus-side race.** After a *successful* export the test reads the file back and deletes it in `finally`; a scanner holding the renamed `<path>` without delete sharing makes `File.Delete` throw a .NET `IOException` — a red test with no `DualcException`, a possible reading of the two uncaptured reds. A DualC retry in `commit()` cannot fix this. | The harness deletes every output after reading it back and tallies **delete failures separately**; Boletus's next captured red names the exception type. |
| H2 | **First-run effects** (the scanner on the freshly built binaries, cold caches). | Latency, not an rc; failures clustered at iterations 0–1 vs uniform. |
| H3 | **Oversubscription** (six exports × `numThreads = 0` on 4 vCPU). | Nothing in the writer path has a timeout, so load cannot *produce* rc 2, only widen a window; the `--concurrency 1` job is the control. |
| H4 | **A non-I/O rc 2.** | Enumerated above; the captured line decides. |
| H5 | **An engine race under MSVC** (wrong mesh). | Boletus: no facet count ever differed; the harness compares every output to a sequential baseline. |
| H6 | **Environment** (`%TEMP%` volume, exclusions, tamper protection, the search indexer). | Printed by the workflow and copied into the raw file. |

## The instrument

`examples/dualc_io_stress.cpp` (opt-in, `-DDUALC_BUILD_IO_STRESS`, never a CTest case),
run by the gate's opt-in `--io-stress` tier (`--io-stress-args` passes flags through), in
`.github/workflows/io-stress.yml` on this branch only — never merged, D-49 keeps the gate
file to `check.py` and nothing else — five jobs per push, every job `continue-on-error`:
`scanner-on` (Boletus's scenario as is: six at once, 30 rounds), `scanner-off`
(`Set-MpPreference -DisableRealtimeMonitoring $true`, an exclusion on `%TEMP%` as the
fallback A/B), `sequential` (`--concurrency 1`, H3), `force` (`--force-hold through`: a
reader without `FILE_SHARE_DELETE` holds each `.part` until the export returned — must fail
every time on today's code; what it prints is the exact `error_code` MSVC gives), and
`linux-control` (must be zero). The harness builds Boletus's field at the same depth and
tile depth, swaps `std::cerr` for a per-thread buffer so each worker keeps its own
`[dualc] error` line, classifies the failing site from it, **retries the rename itself**
after a failure (20 ms steps, 3 s) printing every `error_code` it meets — the direct
evidence of whether a bounded retry would have landed the file — reads every output back
(H5), deletes it with the same retry (H1), and ends with one tally line. The full report
of every job lands in the run's step summary, readable without a login.

**Sample size.** Boletus's point estimate is one failure in 54 concurrent exports
(p ≈ 0.02, a wide interval). One `scanner-on` job is 180 exports, so a zero there is
P ≈ 0.03 at that rate; the rule is **three pushes (540 exports) before a zero means
anything** (≈ 2 × 10⁻⁵ at p = 0.02, ≈ 0.07 at a pessimistic p = 0.005). One positive with
its line is conclusive on its own.

## The decision criteria, fixed before the runs

| Verdict | Evidence | Then |
| --- | --- | --- |
| Confirmed, DualC, in `commit()` | ≥ 1 wild failure whose line is `cannot move … : <OS text>`, the probe's `error_code` value 32 or 5, and the probe rename landing within ≤ 1 s (or the `.part` already released). `scanner-off` at zero strengthens causation but is not required. | The bounded retry in `commit()` and the destructor, the error text into the ABI `err` buffer, the forcing variant as the regression test. |
| Confirmed, a different site | lines `cannot open` / `failed finalizing` / other. | Not the rename retry alone: re-rank, fix the matching site; the OS text on those two messages ships either way. |
| Not DualC's bug as reported | zero export failures in ≥ 540 `scanner-on` exports, and delete failures > 0 and/or Boletus's next captured red is a .NET `IOException`. | Hand back to Boletus (their read-back / `File.Delete` needs the retry); then the hardening question. |
| Inconclusive | rc 2 with no captured line, the scanner toggle ineffective with no fallback, failures only at iterations 0–1. | Fix the capture or the A/B, three more pushes; the item stays PARTIAL with the next push named. |

**The hardening question**, decided by the owner if the wild rate is zero: the `force` job
will always show the rename fails under a no-`FILE_SHARE_DELETE` handle — Windows
semantics, not evidence of occurrence. The recommendation is to ship the error text into
the ABI regardless (this investigation was crippled by its absence) and the retry as
hardening with the forcing test; the alternative is the error text only, the retry
DEFERRED with the trigger "a Boletus red whose captured line reads `cannot move`".

---

← Back to the [item README](README.md) · the [block README](../README.md).
