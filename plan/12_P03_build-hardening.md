# P03 — The rest of build & tooling hardening (#33)

Section 4 of [the raw plan](../docs/raw/2026-10-05-hosted-ci-plan.md). The item is
[17/04 #33](../docs/roadmap/17-code-audit-and-hardening/04-engineering-quality.md#33-build--tooling-hardening);
its record so far is [17/12 § #33](../docs/roadmap/17-code-audit-and-hardening/12-engineering-quality-records.md).
Its trigger ("the first external consumer that is not this repo") is superseded: CI makes
each sub-item near-free to run. Work on `main` after the previous merge, on a branch named
`ci/build-hardening`; push that branch only, never `main`, never force-push.

**Verify each claim against HEAD before scoping it**, as #34 did; the rule for every new
check is *proven to fail before it ships* (break it on the branch, see red, revert, name the
run in the record).

### T1. The four sub-items, each as an option off by default and a CI job

1. **`-Werror` / `/WX` as an option** — `DUALC_WERROR`, OFF by default (the local gate keeps
   its log filter, a recorded decision in
   [17/09/02](../docs/roadmap/17-code-audit-and-hardening/09-local-checks-gate/02-three-decisions.md)),
   ON in the CI build matrix. Widening `/W4 /permissive-` and `-Wall -Wextra -Wpedantic` to
   tests and examples is part of this, as is correcting the claim at
   `examples/CMakeLists.txt:84`. Expect a first batch of warnings in tests and examples; fix
   or justify each.
2. **Sanitizers** — `DUALC_SANITIZE` (`address,undefined`; GCC/Clang only), one Linux CI job
   in `RelWithDebInfo` running `ctest`. A real finding is a defect record (a dated entry,
   possibly a new `#NN`), never a silenced report.
3. **Layering assertion** — after every `add_subdirectory`, read `dualc`'s
   `LINK_LIBRARIES` / `INTERFACE_LINK_LIBRARIES` and `message(FATAL_ERROR)` on anything beyond
   geometry-central and threads (the rule `THIRD_PARTY.md` states in prose). Prove it by
   linking `glfw` into `dualc` on the branch.
4. **Dialect-flag leak** — `set(CMAKE_CXX_STANDARD 17)` at `CMakeLists.txt:41` precedes the
   geometry-central fetch; replace it with `target_compile_features(dualc PUBLIC cxx_std_17)`
   or scope the globals after the fetches; confirm the warnings scan and `ctest` unchanged on
   all runners.
5. **`clang-tidy` and coverage** — left out unless trivially cheap; a `gcov`/`llvm-cov` job is
   a natural host for P04's ledger counts and may be taken there. Name the choice either way.

**Docs at close**, routed by the table in `docs/README.md`: 17/04 #33 body line (DONE, or
PARTIAL naming the rest); the delivery record in 17/12 — if it trips the cap, a same-numbered
folder or a new `17/13` page indexed in 17/README; the decisions #33 row; `README.md`'s build
section and `AGENTS.md`'s opt-in list for the new CMake options, with their real defaults;
`STRUCTURE.md` for any new file; a 17/09/03 note; the roadmap README snapshot (NEXT = P04).

## Exit criteria

- `DUALC_WERROR` and `DUALC_SANITIZE` exist, OFF by default, documented in `README.md` and
  `AGENTS.md`, and the CI workflow runs them; the layering assertion and the dialect fix are
  in `CMakeLists.txt`; each was proven to fail and the proving run is named in the record.
- `python3 scripts/check.py` is green locally; the branch's latest run is green on its
  required jobs (`python3 plan/remote_run_check.py` exits 0).
- 17/04 #33, its record page, the decisions row and the roadmap README snapshot agree.
- The handoff lists every warning or sanitizer finding and what was done with each.
