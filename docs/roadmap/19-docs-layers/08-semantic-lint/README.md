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

---

← Back to the [Docs layers index](../README.md) · the [Roadmap index](../../README.md).
