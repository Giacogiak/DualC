# Record — Phases 0 to 2 (landing, entry points, the gate)

The dated close-of-phase entries of [roadmap 19](README.md) for Phase 0 (the plan landed, the
"frozen" contradiction resolved), Phase 1 (the entry files) and Phase 2 (the gate extension
and the hook). Each entry lists what was delivered, what the plan did not foresee, and the
gate evidence at close. Later phases record in their own numbered child; this file is closed.

**Phase 0 — DONE (2026-09-17).** Delivered:

- This block: the README, row 19 in the [roadmap index](../README.md) and in `STRUCTURE.md`.
- `docs/raw/` created with its [index](../../raw/README.md): the plan, A and S1–S3 — five
  files, so the evidence the plan cites is in the repo. Two things the plan did not foresee,
  both forced by the gate reading a staged `docs/raw/`: (a) `docs/raw/` joined
  `frozen_prefixes` in `scripts/check_data.json` (the size exemption the plan schedules for
  Phase 7 step 6, pulled forward — the same mechanism `docs/study/` uses); (b) an import
  normalization before the first commit, recorded in the raw index: the dated parenthetical
  left each screening's H1 for its first body line (`heading-status`), and two pseudo-links
  in prose were wrapped in backticks (`links`). No `check.py` change.
- The "frozen" contradiction resolved with a dated one-liner at every site the plan lists:
  roadmap `README` rows 16 and 18 and its "Legacy files (frozen)" paragraph (replaced), `16`
  and `18` intros, `17/09` (the `frozen-decl` table row and the "eight files" paragraph),
  `17/05` (the "frozen page 11" sentence), the command-reference README's "Legacy pages
  (frozen)" paragraph (replaced), and the two `STRUCTURE.md` annotations. Record files took
  an appended italic line (never a rewrite); the two index READMEs and `STRUCTURE.md` were
  rewritten in place, as their genre allows.
- `scripts/check_data.json`: the `_comment` no longer points at the removed paragraphs (the
  contract's prose home becomes `docs/README.md` in Phase 1); the two dead `frozen_sections`
  markers dropped; `17/05`'s baseline raised by the appended line and its blank separator (273 → 275 lines,
  16,980 → 17,087 bytes — bytes was the binding axis, 8 B of headroom; accepted because
  Phase 5 step 3 brings the file under cap), the roadmap README's lowered to its new measured size.

*Verified:* `python scripts/check.py --fast` **16/16** — 773 links, 0 broken; 122 cross-file +
26 same-file anchors, 0 unresolved; 221 tracked files, 0 undocumented. Docs-only: no file under `src/`, `include/`, `tests/`,
`examples/` or `capi/` changed (diff stat), so `ctest` and `dualc_glsl_parity` are unchanged
from their last run, recorded in [17/10](../17-code-audit-and-hardening/10-docs-system-screening.md);
neither was re-run here.

**Phase 1 — DONE (2026-09-17).** Delivered, in the plan's order:

- `AGENTS.md` (new, 69 lines): two intro lines, then exactly three sections — build/test/gate
  with every opt-in CMake option (the four GL targets the plan names *and* `DUALC_BUILD_C_ABI`,
  which `CMakeLists.txt` also declares opt-in), the five principles (the session-end routine
  and the semantic-lint cadence are clauses of principle 1), the reading order. No count,
  version, item range, date or tool list; every path in backticks so the check below cannot
  misread one as a count.
- `CLAUDE.md` is the one line `@AGENTS.md`. Its former content went where the plan said:
  build → `AGENTS.md` + root README; "where to look" → `docs/README.md`; the CLI list was
  already the command-reference index; the architecture digest and the conventions keep their
  homes in `ARCHITECTURE.md` until Phase 3 moves them into `design/`.
- `docs/README.md` (new, 58 lines): the folder table (genre per folder), the fact-class →
  owner table, the conventions block (IDs, README per level, footer, caps and the
  same-numbered-folder rule, `raw/` exemption, legend words, nothing frozen) and "How to find
  things" (A6, five `rg` recipes). `design/` and `decisions/` are named in backticks, not
  linked, until Phases 3 and 4 create them — the `links` check chases every relative target.
- Root `README.md` 270 → 144 lines: pitch rewritten for the field path (the two stages and
  the implicit-field layer), the submodule sentence deleted (geometry-central is a sibling,
  never a submodule — `CMakeLists.txt`), the seven per-tool sections replaced by one bullet
  + link each, § Documentation is one pointer to `docs/README.md`, and the restated counts and
  dates dropped. One correction found while rewriting: the minimal-usage snippet bound a
  two-element structured binding to `dualContourMesh`, which has returned a three-tuple
  (mesh, geometry, normals — `include/dualc/pipeline.h`) — it now binds three, and a
  `dualContourField` example sits beside it.
- `STRUCTURE.md` 280 → 184 lines, 25.0 → 14.9 KB, no exemption: the `docs/` subtree is one
  line per folder (each folder's README is the file-by-file index it was duplicating; the
  four prefixes joined `structure_summarized_prefixes`, replacing `docs/study/figures/`;
  Phase 7's move of the study pack must rename the `docs/study/` key to `docs/raw/study/`),
  which took the fifteen date/status annotations with it — including the two "nothing is
  frozen" parentheticals Phase 0 wrote there, which is the plan's end state ("no status
  clauses"), not an undo. Counts corrected against the sources: "the 13 documents" of the
  study (eleven A/B/T files, `ls docs/study`), "9 always-on CLIs + 2 opt-in viewers"
  (four opt-in GL targets, `examples/CMakeLists.txt`), "9 extern C entry points" (thirteen
  `DUALC_CAPI_EXPORT` declarations in `capi/dualc_c.h`) — each now stated without the number
  or with the names. "Build targets at a glance" lists `dualc_field_view`,
  `dualc_glsl_parity`, the two GLSL support libs and the C ABI; `AGENTS.md` and
  `docs/README.md` have their lines.
- Gate (plan step 6): `claude-md` now scans every file in the new `root_entry_files` map of
  `check_data.json` (`AGENTS.md` capped at 80 lines, `CLAUDE.md`) with a fifth forbidden
  pattern, an ISO date; negative-tested (a planted count, version, date and 82 lines — four
  findings, then reverted). No new check id: `root-entry` is Phase 2's, where it absorbs
  the line cap. Dated one-liners in [17/09](../17-code-audit-and-hardening/09-local-checks-gate/README.md)
  (the check row and the "already in `CLAUDE.md`" sentence) and in
  [05/02](../05-tpms-lattices/02-strut-enhancements.md) (its "§ Build & test … `CLAUDE.md`"
  pointer). The `check_data.json` `_comment` now names `docs/README.md` § Conventions as the
  contract's prose home.

*Verified:* `python scripts/check.py --fast` **16/16** — 799 links, 0 broken; 122 cross-file +
26 same-file anchors, 0 unresolved; 129 tracked files scanned by `structure`, 0 undocumented
(the four summarized prefixes account for the rest). `index-staleness` reports the same
structural INFO as Phase 0 (this README newer than the roadmap index; `17/09` and `05/02`
newer than theirs — the effect of dated appends, not stale rows). Docs-only: nothing under
`src/`, `include/`, `tests/`, `examples/` or `capi/` changed; `scripts/check.py` and
`scripts/check_data.json` did (the gate extension above). `ctest` and `dualc_glsl_parity`
unchanged from their last recorded run; neither re-run.

**Phase 2 — DONE (2026-09-17).** The gate does the mechanical work:

- Ten fast checks in `scripts/check.py`, each with its exception list in `check_data.json`,
  the plan's table one for one: `drift-pending` (report-only), `status-sync` (anchored index
  rows only; 14 checked, 19 whole-file rows counted as unverifiable until Phase 5 adds anchors),
  `doc-lag` (report-only; `doc_lag` = 5 commits / 7 days), `root-entry` (line cap + digits
  before a slash, version, item range, ISO date — the cap moved here from `claude-md`, which
  keeps the restated-fact patterns), `decisions-index` and `design-no-history` (implemented,
  SKIP until `docs/decisions/` and `docs/design/` exist), `heading-hierarchy` and
  `stale-paths` (per-file baselines that may only shrink: 15 split-residue headings in 14
  files, 10 stale names in 9 files — the plan's ≈ 20 counted backticked historical literals,
  which the check deliberately ignores), `directional` and `usage-in-record` (report-only;
  5 and 0 hits today). 26 checks; report-only INFO becomes FAIL under `--strict`, whose
  meaning widened from "SKIP fails" to the session-end mode, and INFO details now print.
- One finding fixed on the way: `status-sync` caught #33 and #34 opening with
  "**PARTIALLY DONE**" while the 17 index says PARTIAL — a status-line edit (the contract's
  one allowed in-place edit), `17/04` shrank by 3 bytes and its baseline followed.
- Wiring: `.claude/settings.json` (new, tracked) — a `Stop` hook running
  `python scripts/check.py --docs --hook`: silent on PASS, on FAIL the report goes to stderr
  with exit 2 so the agent sees it before it stops, exit 1 (non-blocking) when
  `stop_hook_active` is set so an unfixable failure cannot loop. Pre-commit keeps `--fast`.
  `AGENTS.md` gained the `--docs --strict` line and one sentence on the hook (72 lines);
  `docs/README.md` the debt rule (`Reconstructed` / `DRIFT-PENDING`, 60 lines).
- Proof: `scripts/check_fixtures/<check>/{pass,fail}/` — a miniature tree per check, walked
  rather than `git ls-files`, with an optional `data.json` override — run by
  `check.py --selftest` (22 runs: 9 checks × 2 plus four direct cases of `doc-lag`'s pure
  verdict, which reads git and so has no tree); registered as CTest `check_selftest` in the
  root `CMakeLists.txt` (not `tests/`, which the plan keeps untouched) when a Python 3 is
  found. **`ctest` is therefore 255 → 256** — the one deliberate change to the count in
  this plan, the new entry being the gate's own test, no C++ touched. Dated follow-up in
  [17/09](../17-code-audit-and-hardening/09-local-checks-gate/README.md) with the check list.
- The plan's own rule applied to its own record: with this entry the 19 README crossed the
  byte cap (15,444 > 15,360), so the phase records moved into this numbered child and the
  README keeps the item and phase tables plus a record index (`19-docs-layers` joined
  `index_roots`). Later phases open their own child rather than growing this one.
- Two things later phases should know. (a) The plan's "`ctest` (255) unchanged at every
  commit" invariant is **256 from this phase on**, the extra entry being `check_selftest`;
  docs-only phases prove themselves against 256. (b) The line scanners (`drift-pending`,
  `stale-paths`, `directional`, `design-no-history`, `root-entry`) see through `strip_code`,
  whose inline-code regex is single-line — a backtick span wrapped across lines leaks its
  text (it produced one phantom `DRIFT-PENDING:` hit in `docs/README.md`, fixed by keeping the
  span on one line). So the two per-file baselines are line-sensitive: a reflow can move a
  count without adding residue; read a change as "re-count", not "regression".

*Verified:* `python scripts/check.py --fast` — 26 checks, 22 OK, 0 failed, 2 SKIP (the two
Phase 3/4 checks), 2 INFO (`index-staleness`, the structural effect of dated appends;
`directional`, 5 sites for Phase 6); 810 links, 0 broken; in-check time ≈ 1.3 s (wall time in
this shell is dominated by process start-up, 4–6 s, the same as before this phase).
`check.py --selftest` 22/22. `ctest -R check_selftest` 1/1; `ctest -N` 256. Hook exercised by
hand: PASS → exit 0, a planted broken link → report on stderr, exit 2, exit 1 with
`stop_hook_active`. `--strict` on a planted `DRIFT-PENDING:` line → FAIL. Nothing under `src/`,
`include/`, `tests/`, `examples/` or `capi/` changed; `CMakeLists.txt` (the test
registration), `scripts/` and `.claude/settings.json` did.

---

← Back to the [Docs layers index](README.md) · the [Roadmap index](../README.md).
