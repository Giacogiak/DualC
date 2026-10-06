# `plan/` — the runner's split of the hosted-CI plan

The plan itself is the immutable input
[`docs/raw/2026-10-05-hosted-ci-plan.md`](../docs/raw/2026-10-05-hosted-ci-plan.md): why
now, the repo facts verified on the drafting day, and the four phases. This folder is the
**working split** the `plan-run` driver executes — one file per phase, one fresh session per
task, with a handoff file per unit under `handoffs/`. These files may be corrected by a session
when the code proves a step wrong; the raw plan never is. Status lives in the roadmap blocks
each phase names ([20](../docs/roadmap/20-public-delivery/README.md),
[17](../docs/roadmap/17-code-audit-and-hardening/README.md)) and in the handoffs, never here.

| File | Phase | Advances |
| --- | --- | --- |
| [10_P01_hosted-ci.md](10_P01_hosted-ci.md) | P01 — hosted CI: the gate as a GitHub Actions job | **#51** (new item) |
| [11_P02_parity-gate.md](11_P02_parity-gate.md) | P02 — the parity gate made binding through headless GL | #31 |
| [12_P03_build-hardening.md](12_P03_build-hardening.md) | P03 — the rest of build & tooling hardening | #33 |
| [13_P04_test-coverage.md](13_P04_test-coverage.md) | P04 — the test-coverage ledger, two batches | #32 |

`run.manifest.json` is the driver's manifest (order, gates, checks); `remote_run_check.py` is
the verification that reads the pushed branch's latest GitHub Actions run. The run state is
`.plan-run/` at the repo root, gitignored.

Every session follows `AGENTS.md`: the reading order at the start, the full gate before a
code commit, and at the close `python3 scripts/check.py --docs --strict` plus the routing of
every fact to its home by the table in `docs/README.md`. The handoff is in addition to the
roadmap record, never a substitute for it.
