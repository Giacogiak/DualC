# `dualc_io_stress` on `windows-2022` — the two samples after the fix (roadmap 17 #52)

Raw observations, 2026-10-07, of the same workflow as
[2026-10-07-io-stress-windows-runs.md](2026-10-07-io-stress-windows-runs.md) once the fix
was on the branch: sample 6 on the first cut (`1a9a877`), sample 7 on the corrected one
(`307c08f`). Every line is verbatim from the check-run annotations. The record that reads
these is [17/15 § 02](../roadmap/17-code-audit-and-hardening/15-windows-rename-race/02-evidence-and-verdict.md).

## Sample 6 — run 37615682796, commit `1a9a877` (the first cut of the fix)

The streams were built over the non-inheritable `FILE*` and left to close it; MSVC's
`<fstream>` says of that constructor "extension, no ownership taking", so no `FILE` was
ever closed and the process held its own `.part`. Every Windows job failed in the harness's
**sequential baseline**, before any round; `force`, job 112773319238:

```
dualc_io_stress: field=boletus-gyroid-box depth=6 tile_depth=4 concurrency=6 iterations=5 dir=C:\Users\RUNNER~1\AppData\Local\Temp\ hw_threads=4 force_hold=through spawn_child=off
dualc_io_stress: the sequential baseline failed (tiled rc=0 '', mono rc=2 '[dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_5472_-1_1.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_5472_-1_1.stl': The process cannot access the file because it is being used by another process. (after 11 attempts over 537 ms)')
[FAIL] io-stress          could not parse dualc_io_stress output (rc 2, process 1)  8.82s
```

The same commit's gate run (37615682842) was red on `build (windows-2022)` alone,
`ctest: 8 of 274 tests failed` — every one a writer test:

```
214 - tiled streaming STL equals the monolithic mesh
215 - tiled streaming STL equals monolithic with the surface on the bounds
246 - a pre-existing destination survives a cancelled re-export and is replaced by a successful one
247 - a monolithic export reports Sample, Contour, then Write (0,1) (1,1)
252 - lastError is empty after a successful export and after an unknown extension it names it
253 - Windows: a short hold on the .part is retried through, a long one fails with the OS text
261 - cli_lattice_stl
265 - cli_field_stl
```

Ubuntu and macOS were green on the same commit (their streams own their files), as was the
`linux-control` job. The retry text (`after 11 attempts over 537 ms`) and the ABI message
plumbing worked as written; what was wrong was the ownership.

## Sample 7 — run 37617028883, commit `307c08f` (the owner, closing twice)

An `OutputFile` owner now closed the `FILE` after the stream — but MSVC's
`basic_filebuf::close()` fcloses unconditionally (only the destructor respects the
no-ownership flag), so the owner's `fclose` was the second one and the close reported
failure. Every Windows export failed at the write/finish step; `scanner-on`, job
112777742661:

```
dualc_io_stress: the sequential baseline failed (tiled rc=2 '[dualc] error: failed finalizing STL 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6884_-1_0.stl'', mono rc=2 '[dualc] error: failed writing STL 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6884_-1_1.stl.part'')
[FAIL] io-stress          could not parse dualc_io_stress output (rc 2, process 1)  1.66s
```

## Sample 8 — run 37618039955, commit `dc5ac38` (the fix as it stands)

<https://github.com/Giacogiak/DualC/actions/runs/37618039955>

| Job | Args | The `io-stress` line |
| --- | --- | --- |
| **`child-inherit`**, job 112781061275 | `--spawn-child --iterations 10` | **`[OK  ] io-stress          60 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 39.21s`** (30/60 before the fix, sample 5) |
| `scanner-on`, job 112781061027 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 86.26s` |
| `scanner-off`, job 112781061435 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 103.90s` |
| `sequential`, job 112781060889 | `--concurrency 1 --iterations 60` | `[OK  ] io-stress          60 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 52.02s` |
| `fresh-process`, job 112781061086 | `--no-baseline --iterations 1` × 15 | `[OK  ] io-stress          90 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part over 15 processes 43.17s` |
| `force`, job 112781061831 | `--force-hold through --iterations 5` | `[FAIL] io-stress          30 exports, 30 failed (open 0, finish 0, write 0, commit 30, count 0, other 0), 0 delete failures, 0 stray .part 19.94s` — as intended: the hold outlives any bounded retry |
| `linux-control`, job 112781061025 | defaults | `[OK  ] io-stress          180 exports, 0 failed (open 0, finish 0, write 0, commit 0, count 0, other 0), 0 delete failures, 0 stray .part 89.43s` |

`force`, the first two FAIL lines verbatim — the retry now visible in the message
(`after 11 attempts over 539 ms`), the probe landing ~180 ms after the holder let go:

```
dualc_io_stress: baseline tiled=163740 facets (865ms), mono=163740 facets (727ms)
FAIL #0/0 tiled rc=2 site=commit ms=3508 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6260_0_0.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6260_0_0.stl': The process cannot access the file because it is being used by another process. (after 11 attempts over 544 ms) | probe=rename ok@188ms after 6 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'} | holder: held .part from 0ms to 3680ms
FAIL #0/1 mono rc=2 site=commit ms=2369 part_after=y dest_after=n | [dualc] error: cannot move 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6260_0_1.stl.part' to 'C:\Users\RUNNER~1\AppData\Local\Temp\dualc_io_stress_6260_0_1.stl': The process cannot access the file because it is being used by another process. (after 11 attempts over 539 ms) | probe=rename ok@184ms after 6 failures ec{value=32 category=system condition=13 message='The process cannot access the file because it is being used by another process.'} | holder: held .part from 1583ms to 2538ms
```

## The workflow, verbatim (as it ran for samples 5–8; retired with the branch)

```yaml
# The concurrent-export I/O experiment (roadmap 17 #52) -- lives on the
# exp/io-stress* branches only and is never merged: every job here is an
# experiment whose result is recorded in docs/raw, never part of the gate's
# green (decisions D-49), which is why it is its own workflow and not a job
# in gate.yml. Every push to the branch is one sample; more samples are
# empty commits. Each job runs the one command, `check.py --io-stress`, and
# copies the full report into the step summary, which the public run page
# shows without a login (job logs and artifacts need one).
name: io-stress

on:
  push:
    branches: ['exp/io-stress*']

permissions:
  contents: read

env:
  PYTHONUTF8: '1'

jobs:
  stress:
    name: ${{ matrix.job }} (${{ matrix.os }})
    runs-on: ${{ matrix.os }}
    timeout-minutes: 60
    continue-on-error: true
    strategy:
      fail-fast: false
      matrix:
        include:
          # Boletus's scenario as is: six exports at once, the scanner as the
          # runner ships it.
          - { job: scanner-on,  os: windows-2022, scanner: 'on',  args: '' }
          # The A/B: real-time protection off (runners are admin). If the
          # status still reads True after the toggle the row is void; the
          # fallback A/B is an exclusion on the temp dir.
          - { job: scanner-off, os: windows-2022, scanner: 'off', args: '' }
          # H3: one export at a time -- a failure here is a per-file hold,
          # not a timing race widened by load.
          - { job: sequential,  os: windows-2022, scanner: 'on',  args: '--concurrency 1 --iterations 60' }
          # The forcing variant: a reader without FILE_SHARE_DELETE holds the
          # .part until the export returned. Expected to fail every time on
          # today's code; what it prints is the exact error_code MSVC gives.
          - { job: force,       os: windows-2022, scanner: 'on',  args: '--force-hold through --iterations 5' }
          # H2, Boletus's shape more closely: a fresh process whose FIRST files
          # are the six concurrent ones (no sequential warm-up), fifteen times.
          - { job: fresh-process, os: windows-2022, scanner: 'on',  args: '--no-baseline --iterations 1', repeat: 15 }
          # H8, the lead after sample 2 showed the scanner off on this image:
          # a child process started with handle inheritance (what .NET's
          # Process.Start does with a redirected stream, as Boletus's CLI
          # parity test does in parallel with its exports) inherits any .part
          # the CRT has open and holds it, without FILE_SHARE_DELETE, for as
          # long as it lives.
          - { job: child-inherit, os: windows-2022, scanner: 'on',  args: '--spawn-child --iterations 10' }
          # The control: the harness itself must be clean where no scanner
          # can hold a file.
          - { job: linux-control, os: ubuntu-24.04, scanner: 'none', args: '' }
    steps:
      - uses: actions/checkout@v5
      - uses: actions/setup-python@v6
        with:
          python-version: '3.x'
      - name: Install Ninja (Ubuntu)
        if: runner.os == 'Linux'
        run: sudo apt-get update && sudo apt-get install -y ninja-build
      - name: Restore FetchContent sources
        uses: actions/cache@v5
        with:
          path: |
            build/_deps/*-src
            build/_deps/*-subbuild
            build/_deps/geometry-central-build/deps/eigen-src
          key: fetchcontent-${{ matrix.os }}-${{ hashFiles('**/CMakeLists.txt') }}
          restore-keys: fetchcontent-${{ matrix.os }}-
      - name: Environment (Windows)
        if: runner.os == 'Windows'
        shell: pwsh
        run: |
          & {
            "TEMP=$env:TEMP"
            "CPUs=$env:NUMBER_OF_PROCESSORS"
            Get-Volume -DriveLetter ($env:TEMP.Substring(0,1)) | Format-List DriveLetter, FileSystem, SizeRemaining
            Get-MpComputerStatus | Format-List AMServiceEnabled, AntivirusEnabled, RealTimeProtectionEnabled, IsTamperProtected, AMProductVersion, AntivirusSignatureVersion
            Get-MpPreference | Format-List DisableRealtimeMonitoring, ExclusionPath, ExclusionExtension
            Get-Service WSearch | Format-List Name, Status, StartType
          } 2>&1 | Tee-Object -FilePath env.log
      - name: Scanner off (Windows)
        if: matrix.scanner == 'off'
        shell: pwsh
        run: |
          & {
            Set-MpPreference -DisableRealtimeMonitoring $true
            Start-Sleep -Seconds 5
            $s = Get-MpComputerStatus
            "RealTimeProtectionEnabled after toggle: $($s.RealTimeProtectionEnabled)"
            if ($s.RealTimeProtectionEnabled) {
              "toggle ineffective -- falling back to an exclusion on TEMP"
              Add-MpPreference -ExclusionPath $env:TEMP
              Get-MpPreference | Format-List DisableRealtimeMonitoring, ExclusionPath
            }
          } 2>&1 | Tee-Object -FilePath env.log -Append
      - name: Configure (Windows)
        if: runner.os == 'Windows'
        shell: bash
        run: >
          cmake -S . -B build -G "Visual Studio 17 2022" -A x64
          -DDUALC_BUILD_TESTS=OFF -DDUALC_BUILD_EXAMPLES=ON -DDUALC_BUILD_IO_STRESS=ON
      - name: Configure (Ubuntu)
        if: runner.os == 'Linux'
        shell: bash
        run: >
          cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
          -DDUALC_BUILD_TESTS=OFF -DDUALC_BUILD_EXAMPLES=ON -DDUALC_BUILD_IO_STRESS=ON
      - name: Build the harness
        shell: bash
        run: cmake --build build --target dualc_io_stress --config Release
      - name: Stress
        shell: bash
        run: python scripts/check.py --io-stress --strict --build-dir build --io-stress-args "${{ matrix.args }}" --io-stress-repeat "${{ matrix.repeat || 1 }}" | tee gate.log
      - name: Annotate the report
        if: always()
        shell: bash
        run: python .github/workflows/annotate.py gate.log ${{ job.status }}
      # The step summary needs a login to read in practice (the public run
      # page loads it client-side); check-run annotations do not, so the lines
      # that carry the result are re-printed as notices too (ten per step).
      - name: Key lines as annotations
        if: always()
        shell: bash
        run: |
          # check.py indents its detail lines and folds the tally into its
          # own [OK]/[FAIL] line, so match anywhere; no match is not an error.
          { grep -E 'io-stress|dualc_io_stress:|FAIL #|DELETE-' gate.log || true; } | sed 's/^ *//; s/%/%25/g' | head -10 | while IFS= read -r l; do echo "::notice title=io-stress::$l"; done
      - name: Environment as annotations
        if: always() && runner.os == 'Windows'
        shell: bash
        run: |
          { grep -E 'TEMP=|CPUs=|RealTimeProtectionEnabled|IsTamperProtected|DisableRealtimeMonitoring|ExclusionPath|Status|toggle|AntivirusSignatureVersion' env.log || true; } | sed 's/^ *//; s/%/%25/g' | grep -v '^$' | head -10 | while IFS= read -r l; do echo "::notice title=env::$l"; done || true
      - name: Report to the step summary
        if: always()
        shell: bash
        run: |
          echo '## ${{ matrix.job }} (${{ matrix.os }}) -- `check.py --io-stress ${{ matrix.args }}`' >> "$GITHUB_STEP_SUMMARY"
          echo '```' >> "$GITHUB_STEP_SUMMARY"
          cat gate.log >> "$GITHUB_STEP_SUMMARY"
          echo '```' >> "$GITHUB_STEP_SUMMARY"
          if [ -f env.log ]; then
            echo '### environment' >> "$GITHUB_STEP_SUMMARY"
            echo '```' >> "$GITHUB_STEP_SUMMARY"
            cat env.log >> "$GITHUB_STEP_SUMMARY"
            echo '```' >> "$GITHUB_STEP_SUMMARY"
          fi
```
