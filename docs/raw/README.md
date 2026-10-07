# `docs/raw/` — immutable inputs

Files here are **inputs, not documentation**: chat imports, read-only screenings, plans as
they were drafted, the 2026-08-19 study pack, and run dumps. **They are never edited** — a
correction is a new dated file, or a row in [`study/ERRATA.md`](study/ERRATA.md) for the
study pack — and they are exempt from the size contract (`frozen_prefixes` in
`scripts/check_data.json`; the gate still reads their links and anchors). Compiled, maintained
text lives in the other `docs/` folders; the record of what was done with these inputs is
[roadmap 19](../roadmap/19-docs-layers/README.md).

| File | What it is | Date | Why it is here |
| --- | --- | --- | --- |
| [2026-09-17-docs-workflow-review.md](2026-09-17-docs-workflow-review.md) | **A** — the claude.ai chat review of the docs workflow; its ten pieces of advice are cited as A1–A10. | 2026-09-17 | The plan follows every one of them; the evidence must be in the repo, not in a chat. |
| [2026-09-17-docs-screening-roadmap.md](2026-09-17-docs-screening-roadmap.md) | **S1** — read-only screening of `docs/roadmap/` at commit `3d73220`: inventory, contract violations, 16 factual conflicts, navigation test. | 2026-09-17 | The worklist for plan Phase 5; file:line evidence the plan only names. |
| [2026-09-17-docs-screening-command-reference.md](2026-09-17-docs-screening-command-reference.md) | **S2** — read-only screening of `docs/command_reference/`: inventory, duplication map, 19 inconsistencies. | 2026-09-17 | The worklist for plan Phase 6. |
| [2026-09-17-docs-screening-architecture.md](2026-09-17-docs-screening-architecture.md) | **S3** — screening of `ARCHITECTURE.md`, `docs/study/`, the quality report and the root files; includes the `ARCHITECTURE.md` cut list. | 2026-09-17 | The worklist for plan Phases 1, 3, 5 (step 4, the quality report) and 7. |
| [2026-09-17-docs-restructuring-plan.md](2026-09-17-docs-restructuring-plan.md) | The nine-phase documentation restructuring plan built from A and S1–S3, with the six decisions of 2026-09-17. | 2026-09-17 | The plan roadmap 19 executes; status lives in 19, never here. |
| [study/](study/README.md) | The **2026-08-19 study pack**: eleven documents `A1`–`A5`, `B1`–`B5`, `T` (~70,000 words), the interactive `dualc-explorer.html`, and 24 figures indexed in [study/figures/README.md](study/figures/README.md). Its one writable file is [study/ERRATA.md](study/ERRATA.md): every reference the pack makes that the docs restructuring left dangling, with its current target. | 2026-08-19 | The audit whose 87 findings [roadmap 17](../roadmap/17-code-audit-and-hardening/README.md) ledgers, cited there by `file:line`; `git mv`'d from `docs/study/` on 2026-09-20 (roadmap 19 Phase 7), so `git log --follow` reaches its history. |
| [2026-06-24-parity-run.txt](2026-06-24-parity-run.txt) | The `dualc_glsl_parity` console run of 2026-06-24 (Intel HD Graphics 630): 62 cases, 16³ samples each, all passed. | 2026-06-24 | The evidence for [12/02](../roadmap/12-field-graph-and-app/02-glsl-codegen.md)'s "62/62" line; today's count has one home, 17/README's *Latest verification*. |
| [2026-09-20-repo-docs-lifecycle-skill-before-phase-8.txt](2026-09-20-repo-docs-lifecycle-skill-before-phase-8.txt) | The `repo-docs-lifecycle` skill as it stood before roadmap 19 Phase 8 revised it: its four files (`SKILL.md`, `doc_conventions.md`, `session_start.md`, `session_end.md`, 28,415 B) verbatim, one header line each. | 2026-09-20 | The "before" of #44's ≤ ⅓ budget; the skill lives outside the repo (`~/.claude/skills/`), so nothing else keeps it — [19/07](../roadmap/19-docs-layers/07-record-phase-8.md). |
| [2026-09-20-loss-audit-3d73220-3c0070f.txt](2026-09-20-loss-audit-3d73220-3c0070f.txt) | The full report of `scripts/docs_loss_audit.py` for base `3d73220` against `3c0070f`: every residual sentence, evidence token, heading and identifier with its bucket, best match and verdict; the rules and what each absorbed. | 2026-09-20 | The evidence behind [19/09](../roadmap/19-docs-layers/09-pre-merge-loss-audit.md); a rerun must reproduce it byte for byte, so it is kept, not summarised. |
| [2026-10-07-io-stress-windows-runs.md](2026-10-07-io-stress-windows-runs.md) | The five `io-stress.yml` samples of 2026-10-07 on `windows-2022` (runs 37609636680 … 37612620026): every job's `check.py --io-stress` line and FAIL lines verbatim from the check-run annotations, the Defender/indexer state of the image, the `force` job's `error_code` tuple, and the `child-inherit` job that reproduced Boletus's failure. | 2026-10-07 | The evidence behind [17 #52](../roadmap/17-code-audit-and-hardening/15-windows-rename-race/README.md); the verdict lives there, never here. |
| [2026-10-05-hosted-ci-plan.md](2026-10-05-hosted-ci-plan.md) | The four-phase hosted-CI plan drafted once the repo had a remote: **#51** (the gate as a GitHub Actions job, three-OS matrix), then the items it unblocks — #31 (headless-GL parity), #33 (the rest of build hardening), #32 (the test-coverage ledger) — one fresh session per phase, with the repo facts it relies on verified that day. | 2026-10-05 | The plan the phases execute; status lives in the roadmap blocks each phase names ([20](../roadmap/20-public-delivery/README.md), [17](../roadmap/17-code-audit-and-hardening/README.md)), never here. |

*Import normalization (2026-09-17, before the first commit of these files):* the date each
screening carried in its H1 moved to the first body line, and two pseudo-links inside prose
(plan §4 Phase 3 footer example, S1 § 2 "Misdirected anchors") were wrapped in backticks, so the
gate's `heading-status` and `links` checks read the files as data. Nothing else was touched.

*Import normalization (2026-09-20, roadmap 19 Phase 7):* `parity.txt` came from a PowerShell
redirect as UTF-16LE with CRLF and a BOM; it was re-encoded as UTF-8/LF and given its dated
name (the `parity*.txt` ignore pattern in `.gitignore` does not match it, so no exception was
added). In the study pack, the README's one outbound link gained a `../` with the move, and its
two status notes (2026-08-21, 2026-08-31) moved verbatim to `study/ERRATA.md` with a pointer in
their place — the README was the only file in the pack ever edited (`015353b`, `40bfb9b`,
`30725a0`); the A/B/T documents, the explorer and the figures are byte-identical to `f03ef3d`,
the commit that landed them.

*Link retargeting (2026-10-06, roadmap 20 #51, decided by the owner at the P01 approve gate):* when block 20 became a folder, the two links in `2026-10-05-hosted-ci-plan.md` that pointed at `20-public-delivery.md` were retargeted to its children — link targets only, no word changed — because the gate's `links` check reads this folder too; the same reason as the 2026-09-17 pseudo-link wrapping. The plan's text remains the 2026-10-05 draft.

*Reading the screenings and the plan today:* their line numbers are pinned to commit `3d73220`,
and four things they name have moved since — roadmap 16 is a tombstone for
[12/07](../roadmap/12-field-graph-and-app/07-mesh-preview-sweep.md), 18 for
[14/04](../roadmap/14-c-abi/04-abi-0-4-0.md), `docs/report-quality-inspection-contouring.md` is
[12/08/](../roadmap/12-field-graph-and-app/08-quality-inspection-gyroid-shell/README.md), and
`docs/ARCHITECTURE.md` is [`docs/design/`](../design/README.md) by the section map in
[19/02](../roadmap/19-docs-layers/02-record-phase-3.md), and since 2026-09-20 roadmap `17/03` is the folder
[17/03/](../roadmap/17-code-audit-and-hardening/03-correctness-and-robustness/README.md), its `#NN` anchors unchanged.
