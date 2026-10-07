# #52 — evidence and verdict

Part of [#52](README.md#52-windows-export-io-flake--the-part-rename-under-an-on-access-scanner):
the five experiment samples of 2026-10-07, what they ruled out and what they reproduced, the
verdict, and what shipped. The predictions these were run against are on
[01](01-desk-analysis.md).

## Evidence

Five pushes of `io-stress.yml`, 2026-10-07, every job on `windows-2022` unless named
(raw: [`2026-10-07-io-stress-windows-runs.md`](../../../raw/2026-10-07-io-stress-windows-runs.md)):

| Job | What it exercises | Samples × exports | Failures |
| --- | --- | --- | --- |
| `scanner-on` | Boletus's scenario as is, six at once | 5 × 180 | 0 |
| `scanner-off` | the same with real-time protection toggled off | 5 × 180 | 0 (void: it was off already) |
| `sequential` | one at a time (H3) | 5 × 60 | 0 |
| `fresh-process` | no warm-up, fifteen fresh processes (H2) | 3 × 90 | 0 |
| `linux-control` (`ubuntu-24.04`) | the harness itself | 5 × 180 | 0 |
| `force` | a reader without `FILE_SHARE_DELETE` holds each `.part` past the export's return | 5 × 30 | 30/30 every time, all at `commit` |
| `child-inherit` | a child process alive throughout, started with `bInheritHandles = TRUE` | 1 × 60 | **30/60 — every tiled export, no monolithic one** |

- **The image has no on-access scanner.** Every Windows job printed
  `RealTimeProtectionEnabled : False`, `DisableRealtimeMonitoring : True`,
  `ExclusionPath : {C:\, D:\}`, the search indexer `Stopped`. The hypothesis Boletus's ask
  rested on cannot be the cause on this runner; H6 answered.
- **The wild rate is zero.** 1,890 Windows exports of the exact scenario (field, depth,
  tile depth, six at once, fresh GUID paths in `%TEMP%`) without one failure, against a
  Boletus point estimate of one in fifty: H2, H3 and H5 answered (every facet count was
  163,740, the golden), H1 never observed (no delete ever needed a retry).
- **The mechanism is handle inheritance.** The MSVC CRT opens files inheritable by
  default; .NET's `Process.Start` with a redirected stream calls `CreateProcess` with
  `bInheritHandles = TRUE`; Boletus's `CliParityTests` starts `dualc_field.exe` that way
  while xunit runs the other classes' exports in parallel. `child-inherit` reproduces it:
  with such a child alive, every tiled export (its `.part` open for the whole contour,
  ~3.5 s) failed at the rename — `cannot move … The process cannot access the file because it
  is being used by another process.` — and every monolithic export (its `.part` open for
  milliseconds) passed; the harness's own rename probe landed 1.5–2.0 s later, when the
  child exited. That is Boletus's record exactly: a tiled export, `DUALC_ERR_IO`, never on
  Linux, a second pass green, the uncaptured reds on the tiled test and on the parity test
  itself.
- **The tuple.** `force` pinned what MSVC hands `std::filesystem::rename` for a sharing
  violation: `ec{value=32 category=system condition=13}` — `std::errc::permission_denied` —
  and that the destructor's `std::remove` fails under the same hold (`part_after=y`), so a
  stray `.part` is left.
- **Why the asked fix would not have worked.** The hold lasts the child's lifetime
  (seconds: a whole CLI run); a retry bounded in the hundreds of milliseconds, git's and
  installers' practice for scanners, would have retried and failed. A longer one would hang
  a host for the length of a foreign process.

## Verdict and what shipped

Confirmed — DualC's, at the rename, with a different holder than inferred. Shipped on
`exp/io-stress-windows`, 2026-10-07:

1. **Non-inheritable output handles** (`examples/example_common.cpp`): one `openOutput`
   helper (`_fsopen(…, "wbN")` under the MSVC `filebuf(FILE*)` extension; `O_CLOEXEC` on
   POSIX) for every `std::ofstream` the writers and the tiled sinks open, and a caller-owned
   `FILE*` through `mz_zip_writer_init_cfile` for the three miniz ZIPs. The non-decimated
   OBJ path writes through geometry-central's own stream and is the one output this does not
   cover (its `.part` is open for milliseconds, the monolithic case the measurements never
   caught).
2. **A bounded retry** in `AtomicOutput::commit()` (≤ 0.5 s, back-off 1 → 100 ms, on
   `permission_denied` / raw 32, 5, 33, Windows only) and around the destructor's remove
   (≤ 150 ms); a success after retries prints a `[dualc] warning:` with the count, a failure
   names the attempts and the time.
3. **The error line reaches the ABI**: every writer-path `[dualc] error:` goes through
   `reportError`, kept in a thread-local read by `dce::lastError()`; the two export twins
   put it in `err` after `export failed: ` / `tiled STL export failed: ` — ABI **0.5.1**
   ([14/04](../../14-c-abi/04-abi-0-4-0.md#051--the-writers-error-text-reaches-err-output-handles-non-inheritable-no-abi-break)).
   The `cannot open` lines gain the OS text.
4. **Tests** (`tests/test_writers_io.cpp`): on every OS a failed open returns 2 with the OS
   text in `lastError()` and leaves nothing, an unknown extension names itself, a success
   leaves `lastError()` empty; on Windows a 300 ms foreign hold is retried through, a 3 s
   hold fails within the budget with `cannot move`, and a child started with handle
   inheritance from the first `Tile` progress event of a tiled export inherits nothing —
   the export lands. The harness stays the opt-in `check.py --io-stress` tier.
5. **The build**: `DUALC_BUILD_C_ABI` now sets `CMAKE_POSITION_INDEPENDENT_CODE` for the
   tree — the shared ABI never linked on Linux (a `thread_local` in `field_graph.cpp` was
   already local-exec TLS in a non-PIC static library; found here because the new one is
   the same shape).

**For Boletus** (the paragraph for its 07 § 10): the flake is DualC's and is fixed by
opening output files non-inheritable, not by a rename retry — the holder was the
`dualc_field.exe` child that `CliParityTests` starts with redirected streams while the other
classes export; a retry would have waited for a whole CLI run. Bump the pin to the commit
that lands `main`, read `err` on `DUALC_ERR_IO` (it now carries the OS text), and expect at
most a `[dualc] warning: moved … after N retries` line on a machine where a scanner holds
files. Nothing else to change; a `File.Delete` retry on the Boletus side is not needed by
the evidence (no delete ever failed in 1,890 exports).

## The experiment itself, kept

`.github/workflows/io-stress.yml` is retired with the branch; its text is in the raw file.
What stays: the harness, the `--io-stress` tier (`--io-stress-args`, `--io-stress-repeat`),
and the convention that a flake is *measured* with it before it is explained.

---

← Back to the [item README](README.md) · the [block README](../README.md).
