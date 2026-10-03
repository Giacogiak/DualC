# Docs layers — the documentation restructuring

The record of the **2026-09-17 documentation restructuring**: the plan that splits the
docs into layers (an entry file, a compiled `design/` layer, a `decisions/` index, the
append-only `roadmap/`, the `command_reference/` usage contract, and immutable `raw/`
inputs), executed one phase per session with the gate green at every close. The plan
itself is immutable and lives in
[`docs/raw/2026-09-17-docs-restructuring-plan.md`](../../raw/2026-09-17-docs-restructuring-plan.md);
its inputs — the chat review **A** and the three screenings **S1–S3** — are indexed in
[`docs/raw/README.md`](../../raw/README.md). This README is where status lives — the item
list and the phase table; the dated entry each phase closes with, and its evidence, is in
the numbered record children indexed under § Record. The previous audit of
the docs system, whose split passes this plan builds on, is
[17/10](../17-code-audit-and-hardening/10-docs-system-screening.md).

Two rules from the plan (§ 4) apply to every phase:

- **Resolution rule.** No conflict is settled by preferring one document over another.
  Present-tense facts are checked against the implementation (`examples/`, `src/`,
  `include/`, `capi/`); historical values against git at that commit. Where git cannot
  decide, the line becomes `**Reconstructed (<date>) from commit <hash>**` or
  `DRIFT-PENDING: <what is owed>` — never a guess.
- **Close of a phase.** `python scripts/check.py --fast` green, link/anchor counts reported,
  `ctest` count unchanged for a docs-only phase, and a dated entry in § Record below.

## Items

One tracked item per piece of advice in the plan's § 2 map, **#36–#45 assigned at birth**
(the numbers continue [17](../17-code-audit-and-hardening/README.md)'s #22–#35).

| Item | Advice | Deliverable (phase) | Done when | Status |
| --- | --- | --- | --- | --- |
| #36 | A1 — a compiled current-state layer, separate from the record | `docs/design/` from `ARCHITECTURE.md` + harvested intros (Phase 3) | `ARCHITECTURE.md` gone; every design child ≤ cap; `design-no-history` green | **DONE** (3a 2026-09-17, 3b 2026-09-18 — the harvest is in `09`–`11`, [02](02-record-phase-3.md)) |
| #37 | A2 — mechanical checks in a hook-run script, not in prose | `scripts/check.py` + ten fast checks + a `Stop` hook (Phase 2) | `check.py --docs` < 3 s from the hook and pre-commit; fixtures pass | **DONE** (2026-09-17) |
| #38 | A3 — a ≤ 80-line root entry | `AGENTS.md` (canonical), `CLAUDE.md` = `@AGENTS.md`, `docs/README.md` (Phase 1) | `root-entry` check green on counts, versions, dates, length | **DONE** (2026-09-17, Phase 2 landed `root-entry`) |
| #39 | A4 — a one-table decisions index | `docs/decisions/README.md` seeded by grep (Phase 4) | every DROPPED/DEFERRED heading has exactly one row (`decisions-index`) | **DONE** (2026-09-18 — two tables by status, the cap forced the split; [03](03-record-phase-4.md)) |
| #40 | A5 — a home for cross-cutting knowhow | `design/09-conventions`, `10-invariants-and-tolerances`, `11-glossary` + "Things we do not do" (Phase 3) | the seven multi-copy facts of S2 § 4 each have one home | **DONE** (2026-09-18 — homes in `09`/`10`; the copies become links in Phases 5 and 6, [02](02-record-phase-3.md)) |
| #41 | A6 — index + grep, no vector DB | "How to find things" in `docs/README.md`; FTS5 DEFERRED with a trigger (Phase 7) | trigger written: `docs/raw/` > 50 files or 1 MB | **DONE** (2026-09-20 — D-40 carries both clauses; assets do not count, [06](06-record-phase-7.md)) |
| #42 | A7 — the layered tree with `raw/` | target tree of plan § 3; `docs/study/` → `docs/raw/study/` (Phase 7) | tree matches § 3; `STRUCTURE.md` carries no status clauses | **DONE** (2026-09-20 — `git mv`, 45 citations retargeted, ERRATA beside the pack, [06](06-record-phase-7.md)) |
| #43 | A8 — migration from intros, grep and the audit checklist | Phases 3, 4, 2 use exactly those sources | — (method, proven by #36/#39/#37) | **DONE** (2026-09-18 — the third source used, [03](03-record-phase-4.md)) |
| #44 | A9 — the skill shrinks to lint → harvest → route → lint | revised `repo-docs-lifecycle` body with a routing table (Phase 8) | skill ≤ ⅓ of its budget; session-end = 4 steps | **DONE** (2026-09-20 — 28,415 → 8,516 B, two files, four steps each way, [07](07-record-phase-8.md)) |
| #45 | A10 — a periodic semantic lint | `.claude/commands/docs-semantic-lint.md`; first run recorded here (Phase 8) | first run recorded; cadence rule in `AGENTS.md` | **DONE** (2026-09-20 — first run [08/01](08-semantic-lint/01-2026-09-20-first-run.md), 7 findings; `AGENTS.md` principle 1 names the command, [07](07-record-phase-8.md)) |

## Phases

| Phase | Sessions | Depends on | Gate proof at close | Status |
| --- | --- | --- | --- | --- |
| 0 Land the plan, fix the contract contradictions | ½ | — | `--fast` 16/16 | **DONE** (2026-09-17) |
| 1 Entry points (A3) | 1 | 0 | `root-entry` (once Phase 2 lands) | **DONE** (2026-09-17) |
| 2 Gate extension + hook (A2, A8) | 1 | 0 | fixtures pass; `--fast` ≈ 1 s | **DONE** (2026-09-17) |
| 3 Design layer (A1, A5) | 2 | 1, 2 | `design-no-history`, `stale-paths`, 0 broken links | **DONE** (3a 2026-09-17, 3b 2026-09-18) |
| 4 Decisions index (A4) | ½ | 3 | `decisions-index` | **DONE** (2026-09-18) |
| 5 Roadmap consolidation (S1) | 1–2 | 2, 4 | `status-sync`, `--strict` | **DONE** (5a, 5b 2026-09-18; 5c evidence pass 2026-09-20) |
| 6 Command reference cleanup (S2) | 1–2 | 3 | extended `flag-table`, `heading-hierarchy` | **DONE** (6a, 6b 2026-09-20 — `--docs --strict` fully green for the first time; [05](05-record-phase-6.md)) |
| 7 `raw/` + the study pack (A6, A7) | ½–1 | 1 | `links`/`anchors` after the move | **DONE** (2026-09-20 — 0 broken, 0 unresolved; [06](06-record-phase-7.md)) |
| 8 Semantic lint + the shrunken skill (A9, A10) | 1 | 3–7 | first lint entry recorded | **DONE** (2026-09-20 — the plan's end state holds, block 19 closes; [07](07-record-phase-8.md)) |

Sequential, one thread per phase (plan § 7 decision 6); Phases 5 and 6 never run
concurrently. Nothing in the plan touches `src/`, `include/`, `capi/` or `tests/`.

## Record

One dated entry per phase, with its evidence, in numbered children of this folder (this
README stays the status surface and under the cap):

| Phases | Record | Closed |
| --- | --- | --- |
| 0 — land the plan; 1 — entry points; 2 — the gate | [01-record-phases-0-2.md](01-record-phases-0-2.md) | 2026-09-17 |
| 3 — the design layer (sessions 3a, 3b) | [02-record-phase-3.md](02-record-phase-3.md) | 3a 2026-09-17; 3b 2026-09-18 |
| 4 — the decisions index | [03-record-phase-4.md](03-record-phase-4.md) | 2026-09-18 |
| 5 — the roadmap consolidation (sessions 5a–5c) | [04-record-phase-5/](04-record-phase-5/README.md) | 5a, 5b 2026-09-18; 5c 2026-09-20 |
| 6 — the command reference cleanup (sessions 6a, 6b) | [05-record-phase-6.md](05-record-phase-6.md) | 2026-09-20 |
| 7 — `raw/` and the study pack | [06-record-phase-7.md](06-record-phase-7.md) | 2026-09-20 |
| 8 — the semantic lint and the shrunken skill; what the whole of 19 leaves; D-41 settled, then refined (the run folder is `check_data.json`'s fact) | [07-record-phase-8.md](07-record-phase-8.md) | 2026-09-20; D-41 2026-09-21 |
| the semantic-lint runs (Phase 8 item 2, then monthly) — one dated child per run; the folder is `semantic_lint_runs.folder` of `scripts/check_data.json`, kept by the `semantic-lint-runs` check | [08-semantic-lint/](08-semantic-lint/README.md) | runs 01–03, 2026-09-20 … 09-21 |
| the pre-merge loss audit of the whole block — base `3d73220` against the branch, four unit types, 0 untriaged | [09-pre-merge-loss-audit.md](09-pre-merge-loss-audit.md) | 2026-09-20 |
| the cap relief — the status snapshot trimmed by its own rule (D-43) and the gate's record (17/09) split into a folder; the bytes, the moves, the links, the gate evidence | [10-cap-relief.md](10-cap-relief.md) | 2026-09-21, three commits |

---

← Back to the [Roadmap index](../README.md).
