# Semantic-lint runs

The dated runs of `/docs-semantic-lint` (the owner's `~/.claude/commands/docs-semantic-lint.md`, outside the repo — [D-41](../../../decisions/01-settled.md)), the
procedure that reads `docs/design/` against the code and the record against `design/` —
the meaning check the gate cannot make. The command names no path: this folder is
`semantic_lint_runs.folder` of `scripts/check_data.json`, the one per-repo fact it reads,
and the fast check `semantic-lint-runs` keeps the folder an index root and every run named
`NN-<date>-<slug>.md` with its date on the first body line ([19/07](../07-record-phase-8.md),
2026-09-21). Item 1 of [roadmap 19](../README.md) Phase 8
wrote the procedure; the cadence — after any session that edits both `src/` and
`docs/design/`, and at least monthly — is principle 1 of `AGENTS.md`. Each run is one
child here, the date in its filename and on its first body line; the findings table names
`file:line` on both sides so a run can be repeated. Fixes are session work, never part of
a run; a run's disposition column is the only cell a later session changes, as a dated
append. The five finding classes the runs count: **(a)** a design claim the code
contradicts; **(b)** a design page older than a code change to the files it names;
**(c)** present-tense "how it works" prose in `roadmap/` or `command_reference/` that
belongs in `design/`; **(d)** an orphan page no page links but its folder README;
**(e)** a DEFERRED decision whose trigger is met.

| Run | Date | Findings |
| --- | --- | --- |
| [01-2026-09-20-first-run.md](01-2026-09-20-first-run.md) | 2026-09-20 | (a) 4 · (b) 0 · (c) 3 · (d) 0 · (e) 0 — 6 fixed, 1 kept with its reason; one code observation handed to 17 #25 |
| [02-2026-09-21-after-46.md](02-2026-09-21-after-46.md) | 2026-09-21 | delta run after the #46 session (src + design): (a) 0 · (b) 1 · (c) 0 · (d) 0 · (e) 0 — 1 fixed |
| [03-2026-09-21-after-47.md](03-2026-09-21-after-47.md) | 2026-09-21 | delta run after the #47 session (vendored nanort, the CMake resolver + design 05/06): (a) 2 · (b) 0 · (c) 1 · (d) 0 · (e) 0 (#8's second trigger met, retired by the session) — 3 fixed |
| [04-2026-09-22-after-48.md](04-2026-09-22-after-48.md) | 2026-09-22 | delta run after the #48 session (the hooks through sampler, contourer, writers, ABI + design 01/09/10/11): (a) 3 · (b) 3 · (c) 1 · (d) 0 · (e) 0 — 7 fixed, six of them quoted code that moved under a page |
| [05-2026-10-05-after-50.md](05-2026-10-05-after-50.md) | 2026-10-05 | full run after the #50 session (Polyscope retired, GLFW + glad owned; design 09/11) and the first since the history rebase: (a) 11 · (b) 0 · (c) 0 · (d) 0 · (e) 0 — 11 fixed, two of them with the code comment they mirrored; D-40 within 6 % of its trigger |
| [06-2026-10-07-after-ci-plan.md](06-2026-10-07-after-ci-plan.md) | 2026-10-07 | delta run after the hosted-CI plan (#51, #31, #33, #32: build, tests, `check.py`; design 10/11): (a) 1 · (b) 1 · (c) 1 · (d) 0 · (e) 0 — run 05's "area-weighted" survived in four more places; the #33 build rules and the test conventions had no `design/` home |
| [07-2026-10-16-after-cli-consolidation.md](07-2026-10-16-after-cli-consolidation.md) | 2026-10-16 | delta run after the CLI consolidation plan (#55–#60: `examples/` graph, codegen, slice, seven tools retired; design 02/09): (a) 3 · (b) 1 · (c) 2 · (d) 0 · (e) 2 — 5 fixed (a new `design/12` for the graph's own rules), 1 handed to 21/09, D-40 and D-33 met, the owner's call |
| [08-2026-10-19-after-contourer-threads.md](08-2026-10-19-after-contourer-threads.md) | 2026-10-19 | delta run after #34's thread count (P02: contourer, bakes, C ABI 0.5.2, `--threads`; design 03/06): (a) 1 · (b) 1 · (c) 0 · (d) 0 · (e) 0 — 2 fixed, the invariant list and the graph's bake rule |
| [09-2026-10-21-after-hygiene-run.md](09-2026-10-21-after-hygiene-run.md) | 2026-10-21 | delta run at the hygiene run's close (#34 gradient contract + `trueGradientAt`, the `GridField` normal, #35 pruning, #53 gate; design 08/10): (a) 1 · (b) 0 · (c) 0 · (d) 0 · (e) 1 — the FD-step row fixed; #11's trigger met by the run's scratch benchmarks, the owner's |
| [10-2026-10-22-after-porous-run.md](10-2026-10-22-after-porous-run.md) | 2026-10-22 | delta run at the porous shell run's close (#63–#67: seeds, exact Delaunay, packed complex, `PorousField`, GLSL data textures; design/06/12): (a) 2 · (b) 2 · (c) 1 · (d) 0 · (e) 1 — 3 fixed (a new `design/13` for the porous library's contract); design/10 at its cap cannot take the porous hooks and constants, D-53's trigger arguably met — the owner's |
| [11-2026-10-28-after-e12.md](11-2026-10-28-after-e12.md) | 2026-10-28 | delta run after the E12 session (`GridField::gradientAt` minmod, `dualc_mesh_deviation`; design 08/11): (a) 1 · (b) 0 · (c) 0 · (d) 0 · (e) 1 — the sources list's stale `gradientAt` claim fixed, #11's trigger answered by the tool |

---

← Back to the [Docs layers index](../README.md) · the [Roadmap index](../../README.md).
