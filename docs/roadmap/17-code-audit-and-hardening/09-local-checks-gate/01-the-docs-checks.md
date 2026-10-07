# The local checks gate — the docs checks and the semantic half

Part of [the local checks gate](README.md) (#33), the record of `scripts/check.py`; the page became a folder on 2026-09-21 and the sections are verbatim.

## The docs checks lead, and that is the point

`cmake --build` and `ctest` are already in [`CLAUDE.md`](../../../../CLAUDE.md) and
runnable today; scripting them is convenience. The doc-contract invariants are
the ones that existed **only as commands typed by hand**, run in a fraction of a
second, and had actually been failing: `04-engineering-quality.md` blew its size
cap twice in one session, `#35` landed above `#34` and had to be moved by hand,
and the "538 links, 0 broken" figure was re-derived from scratch each time it was
quoted.

*(2026-09-18: the third column is the 2026-09-11 reading and stays as such — the live
counters are what `check.py --fast` prints, and `frozen-decl`'s 8 has been 0 since that
same day, [10](../10-docs-system-screening.md).)*

| Check | What it holds | Today (2026-09-11) |
| --- | --- | --- |
| `links` | every relative link target exists | 599 links, 0 broken |
| `anchors` | every `#anchor` resolves against the target's headings | 116 cross-file + 56 same-file, 0 unresolved |
| `indexes` | every numbered sibling is linked from its README | 4 roots, 51 siblings, 0 unlinked |
| `sizes` | the size contract, ratcheted ([02](02-three-decisions.md#three-decisions-worth-recording)) | 36 live files, 0 over |
| `frozen-decl` | each frozen file is still named in the paragraph that freezes it | 8 files |
| `structure` | every tracked file is named in `STRUCTURE.md` | 182 files, 0 undocumented |
| `vendoring` | vendored files are attributed and cited license paths exist | 11 vendored, 10 cited paths |
| `ordering` | `## #NN` ledger headings ascend | 14 items |
| `scripts` | every script in `scripts/` parses | 2 scripts |

*(2026-09-17: the `frozen-decl` row is dated — nothing is frozen since 2026-09-11, [10](../10-docs-system-screening.md); the list is empty and the check holds vacuously.)*

Build tier: `configure` → `build` → `warnings` → `ctest`. Opt-in `--gpu` tier:
`parity`. *(2026-10-03: the build tier picks its generator per OS and parses GCC/Ninja and
CTest 4 output — [20 #49](../../20-public-delivery/02-linux-build-host.md#49-linux-as-a-build-host).)*
*(2026-10-07: a second opt-in tier, `--io-stress` → `io-stress`, runs the concurrent-export
harness and passes `--io-stress-args` through; an experiment measuring a flake, never in the
default gate or `--all` — [15 #52](../16-windows-rename-race/README.md#52-windows-export-io-flake--the-part-rename-under-an-on-access-scanner).)*

### The semantic half

**Follow-up (2026-09-11).** The
[docs-system screening](../10-docs-system-screening.md) found that the nine checks above
were all *structural* and that twelve of the contract's fourteen *semantic* clauses
had violations nobody could see. Seven checks were added, in the same style — each
turns a sentence of the contract into a line of output:

| Check | What it holds | Why it exists |
| --- | --- | --- |
| `footer` | the last paragraph of every non-index page starts `← Back to` | 13 had no footer; 12 had its continuation pointer *after* the footer |
| `frozen-growth` | a frozen file's line count ≤ `frozen[].lines` in `check_data.json` | `sizes` skips frozen files; 14 and page 11 had taken multi-line appends after freezing. A legal one-line edit bumps the number in the same commit — the diff is the review |
| `heading-status` | no heading carries DONE/DEFERRED/… or a date; per-file legacy counts in `heading_status_baseline` may only shrink | anchors derive from headings, so a status flip in a heading breaks every inbound link (`dd1a9c4` was one such repair) |
| `status-vocab` | every Status cell in the two index tables contains a legend word | "OPEN", "open", "IN PROGRESS" and empty cells in the snapshot tables |
| `index-staleness` | *report only*: topic files whose last commit is newer than their index README's | the "index says one thing, file moved on" pattern (an export bug "still open" three months after the fix); INFO because a count sync is a legitimate lone edit |
| `claude-md` | `CLAUDE.md` contains no test/parity count, version, item range or primitive count | it was the fourth copy of the ledger and three months stale; it now links |
| `flag-table` | every `"--flag"` literal in `examples/<tool>.cpp` appears on the tool's page (`--help` exempt) | page 05 had no flag table; 01/02/04 lacked `--collapse` |

Two corrections to what stood above: `ordering` now uses `<=`, so a duplicated
`## #NN` also fails (uniqueness was unchecked); and the `--metrics` needle for C43
looked for a filename that never existed in `tests/` and reported "no" forever — it
now matches the `temp_directory_path() / "…"` construct. 16 checks, still ~1 s.

*(2026-09-17: `claude-md` now scans every file in `root_entry_files` of `check_data.json` —
`AGENTS.md`, the canonical entry file since [19](../../19-docs-layers/README.md) Phase 1, capped
at 80 lines, and `CLAUDE.md`, now the one-line `@AGENTS.md` import — with a fifth forbidden
pattern, an ISO date; the build commands cited above moved from `CLAUDE.md` to `AGENTS.md`
with the rest of its prose. Phase 2 of 19 adds the `root-entry` check on top. Still 16 checks.)*

*(2026-09-17, later: Phase 2 of [19](../../19-docs-layers/README.md) added ten fast checks —
`drift-pending`, `status-sync`, `doc-lag`, `root-entry` (the line cap moved here from
`claude-md`), `decisions-index` and `design-no-history` (SKIP until Phases 4 and 3 create
their folders), `heading-hierarchy`, `stale-paths`, `directional`, `usage-in-record` — four of
them report-only, promoted to failures by `--strict`, the session-end mode; a `Stop` hook in
`.claude/settings.json` runs `--docs --hook` after every Claude Code turn; and `--selftest`
runs each check against `scripts/check_fixtures/` (CTest `check_selftest`). 26 checks; the
in-check time is still about a second.)*

*(2026-09-21: the 27th check, `semantic-lint-runs`, ported from Boletus with its fixture
pair — the folder is `semantic_lint_runs.folder` of `check_data.json`, its README present,
the folder an `index_roots` entry, every run `NN-<date>-<slug>.md` with its date on the
first body line; `--selftest` 28 runs. The tables stay the dated 2026-09-11 reading, as
their notes say — [19/07](../../19-docs-layers/07-record-phase-8.md).)*

*(2026-10-07: the `approx-zero` check — no `Approx(<zero>)` in `tests/` without a `.margin(`,
split lines included — turns #32's `Approx(0.0)` metric into a ratchet at 0, with its fixture
pair; the `--metrics` needle had counted guarded uses too and read 70 while the unguarded count
was 38 — [15](../15-test-coverage-batches.md).)*

---

← Back to [09 — the local checks gate](README.md) · the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
