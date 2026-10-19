# P05 — The gate builds the C ABI (#53)

Source: § 2 "#53" of [the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md). What is
owed is listed in `docs/roadmap/14-c-abi/06-linux-shared-library.md`. Branch
`feat/hygiene-and-editor`. **Never push.** The remote proof is the owner's, after they push.

### T1. The C ABI in the default gate, the red run, the TESTS=OFF copy

1. **Add the C ABI to the gate.** Read `scripts/check.py:1430-1467` (configure) and
   `.github/workflows/gate.yml`. The default choice is to add `-DDUALC_BUILD_C_ABI=ON` to
   `check.py`'s default configure. Every CI `build` job (Linux, Windows, macOS), the `sanitize`
   job and any local gate would then build `dualc_capi` and run the four `cli_c_abi*` CTest
   cases.

   Check the sanitize job locally in its own build directory:
   `python3 scripts/check.py --build -D CMAKE_BUILD_TYPE=RelWithDebInfo -D DUALC_SANITIZE=address,undefined`.
   If the flag breaks a job you can run locally, say so in the handoff and turn it on only for
   the jobs that pass. Mark that as a **Chosen in-run** decision.
2. **Docs.** Update the gate's documentation: the page in
   `docs/roadmap/17-code-audit-and-hardening/09-local-checks-gate/` that lists the build tier's
   flags. Update AGENTS.md's opt-in list too, if the gate no longer treats `DUALC_BUILD_C_ABI`
   as opt-in. The CMake default stays OFF; only the gate turns it on.
3. **Red run, locally, in a scratch build directory.**
   - Remove the global `CMAKE_POSITION_INDEPENDENT_CODE` lines (`CMakeLists.txt:34-41`) and the
     target property in `capi/CMakeLists.txt`.
   - Configure and build, and record the link error (relocation against non-PIC
     geometry-central or dualc objects).
   - Restore both files. Do not commit the break.
4. **Boletus's report.** Configure a scratch tree with
   `-DDUALC_BUILD_TESTS=OFF -DDUALC_BUILD_EXAMPLES=ON -DDUALC_BUILD_C_ABI=ON` and build it. If
   the demo-mesh copy (`examples/CMakeLists.txt:246-264`) fails, fix it and add the case to the
   record. If it works, record that it works and at which commit.
5. **Full gate.** Run it with `--clean`, since the configure changed. Record the new ctest count
   (344, plus the four C ABI cases, plus what P02–P04 added) in 17 § Latest verification,
   labelled as the local Linux count.
6. **Record and handoff.**
   - Append a dated entry to 14/06: the gate proof landed locally, and the remote proof waits
     for the owner's push.
   - In the handoff, write the exact command the owner runs
     (`git push -u origin feat/hygiene-and-editor`) and which jobs' `cli_c_abi*` lines to read.
   - Leave #53 **PARTIAL** until someone reads the remote run. P08 or the owner flips it.

#### Acceptance criteria

- `python3 scripts/check.py` builds `dualc_capi` and runs the four `cli_c_abi*` cases, all green.
- The red run and the TESTS=OFF result are recorded. The work is committed, not pushed.

## Exit criteria

- 14/06 and 17's verification line are current, and #53 waits only for the owner's remote run.
