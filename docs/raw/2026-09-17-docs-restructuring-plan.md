# DualC documentation restructuring plan

**Drafted 2026-09-17** from two inputs, both saved beside this file in the DualCDoc project:

- **A** — the chat review of the docs workflow (`00-chat-advice-2026-09-17.md`), whose ten pieces of
  advice are labelled A1–A10 below. The plan follows every one of them.
- **S** — a full read of `D:\DualC\docs\` (88 markdown files, ~15,200 lines) plus `CLAUDE.md`,
  `README.md`, `STRUCTURE.md`, at commit `3d73220`: `01-screening-roadmap.md` (S1),
  `02-screening-command-reference.md` (S2), `03-screening-architecture-study-root.md` (S3). Those hold
  the file:line evidence; this plan only names the moves.

Repo facts the plan relies on (verified 2026-09-17): the gate the chat calls `tools/docs_lint.py`
already exists as `scripts/check.py` (1,117 lines, 16 fast checks + configure/build/warnings/ctest/parity
tiers, exceptions in `scripts/check_data.json`, run by `scripts/hooks/pre-commit`); no `AGENTS.md`;
`.claude/` holds only `settings.local.json` (permissions, no hooks); the `frozen` list in
`check_data.json` is empty; the next free roadmap number is **19**.

---

## 1. What the screening adds to the chat's diagnosis

The chat diagnosed the *shape* of the system; the screening confirms it file by file and adds the cost of
not fixing it. The five weaknesses (A1–A5) each have a concrete face in the tree today:

1. **No current-state layer (A1).** `docs/ARCHITECTURE.md` is the only design document and has become a
   changelog: 1,265 lines / 65 KB, five dated patches in the last three weeks, ≈190 lines of history,
   ≈225 lines of usage/inventory, one self-contradiction (silent vs reported bounds fallback, S3 §2.3).
   Ten of its passages duplicate roadmap or command_reference text (S3 §2.2). The "living doc with a
   byte ceiling" exemption in `check_data.json` is bumped on every edit, so it is not a ceiling.
2. **Enforcement half in prose (A2).** `check.py` is structural and green; every defect the screening
   found is semantic: 16 factual conflicts in the roadmap alone (S1 §4), 19 in the command reference
   (S2 §5), three "frozen" claims that contradict the empty frozen list, a status ledger for #22–#35 in
   five copies, `DRIFT-PENDING` and `Reconstructed` used nowhere.
3. **No root entry (A3).** `CLAUDE.md` is 130 lines and ≈60 % restated facts (architecture digest, build,
   conventions, the 12-tool list); it contradicts itself ("future C ABI" vs "C ABI shipped") and omits
   two of the four opt-in CMake targets. There is no `docs/README.md`; two 26–66 KB files sit loose at
   `docs/` root.
4. **Decisions buried (A4).** DROPPED/DEFERRED entries live in 20+ places; at least 11 DEFERRED items
   name no trigger (S1 §3); the same decision ("why a C ABI", "moved to Boletus") is restated verbatim
   in 2–3 files.
5. **No knowhow home (A5).** The thin-feature "≥ 2–3 cells" rule is stated in 7 places, the PowerShell
   quoting rule in 6, the `mix`-gap caveat in 6, "1 unit = 1 mm" in 9 (S2 §4) — each a cross-cutting
   fact with no owner, so each copy drifts independently (`--offset` default 1 vs 0, weld "deferred" vs
   shipped, `--tile-depth` bound stated three ways).

Two things the screening found that the chat could not see: (a) the split of 2026-09-11 left mechanical
residue in every child (H1 repeated as `###`, "above/below" pointing into other files, stale
`11-dualc_field.md` link texts in 6+ places, heading hierarchies starting at `###`); (b) `docs/study/`
is an interview-prep pack ("You commissioned this library… in three days…", 55 rehearsed "I would"
answers) that is frozen, cited by file:line 30 times from roadmap 17, and 2–5× over every cap — it is
raw material (A7) and must be treated as such, not as documentation.

## 2. Advice → action map

Every item of the chat's advice, where it lands in this plan, and what "done" means.

| # | Advice (from A) | Plan | Done when |
| --- | --- | --- | --- |
| A1 | Compiled, freely-rewritten current-state layer separate from the record | Phase 3: `docs/design/` built from `ARCHITECTURE.md` + current-state paragraphs harvested from roadmap intros | `ARCHITECTURE.md` gone; every design child ≤ cap; no dates/"since"/"used to" in `design/` (gate check `design-no-history`) |
| A2 | Mechanical checks in a hook-run script, not in prose | Phase 2: extend `scripts/check.py` with the six named checks + four the screening shows are needed; wire a Claude Code hook | `check.py --docs` runs in < 3 s from a `Stop` hook and pre-commit; session-end skill step is "run it and fix the report" |
| A3 | ≤ 80-line root entry: build/test, principles, reading order, nothing else; `@AGENTS.md` | Phase 1: `AGENTS.md` (canonical) + `CLAUDE.md` = `@AGENTS.md`; `docs/README.md` as the docs entry | `AGENTS.md` ≤ 80 lines; gate check `root-entry` fails on counts, versions, dates, or > 80 lines |
| A4 | One-table decisions index (ID, decision, date, topic link) | Phase 4: `docs/decisions/README.md` seeded by grep for DROPPED/DEFERRED/"decision" | Every DROPPED/DEFERRED heading in the roadmap has exactly one row (gate check `decisions-index`) |
| A5 | Home for cross-cutting knowhow (tolerances, invariants, conventions, "never do X") | Phase 3: three `design/` children — conventions, invariants & tolerances, glossary — plus a "Things we do not do" section in the design README | The seven multi-copy facts of S2 §4 each have one home in `design/` and links elsewhere |
| A6 | No vector DB / RAG; index + grep; FTS5 or QMD only if raw material grows | Phase 7: `docs/README.md` "How to find things" section (rg recipes by ID, flag, message); FTS5 recorded as DEFERRED with trigger in roadmap 19 | Trigger written: "`docs/raw/` exceeds 50 files or 1 MB of text" |
| A7 | Layered tree: entry file, `STRUCTURE.md`, `design/`, `decisions/`, `roadmap/`, `command_reference/`, `raw/`, lint script | Target tree in §3; `docs/raw/` created in Phase 7 (study pack, chat import, `parity.txt`, benchmark dumps) | Tree matches §3; `STRUCTURE.md` stays the mutable codebase map with no status clauses |
| A8 | Migration path: design from topic intros; decisions by grep; lint from the audit checklist | Phases 3, 4, 2 respectively use exactly those sources (S1–S3 are the checklist) | — |
| A9 | Skill shrinks to: lint → harvest → route facts to layer → lint | Phase 8: revised `repo-docs-lifecycle` body with a fact-class → layer routing table | Skill ≤ ⅓ of its current instruction budget; session-end = 4 steps |
| A10 | Periodic semantic lint: design/ vs code; contradictions, orphan pages, stale claims | Phase 8: `.claude/commands/docs-semantic-lint.md` procedure; output appended to roadmap 19; cadence rule | First run recorded; rule in `AGENTS.md`: run after any session that touches `src/` + `design/` |

## 3. Target tree

```
D:\DualC\
├── AGENTS.md                      ≤ 80 lines: build/test commands, 5 principles, reading order
├── CLAUDE.md                      "@AGENTS.md" + nothing else
├── README.md                      public front page (v2-era pitch, build, consume, one link per doc area)
├── STRUCTURE.md                   codebase map — mutable, no status/date annotations
├── scripts/check.py               the gate (extended, Phase 2)   ← A2's tools/docs_lint.py
├── .claude/settings.json          Stop hook → scripts/check.py --docs
└── docs/
    ├── README.md                  ≤ 60 lines: what lives where, fact-class → owner table, conventions, how to search
    ├── design/                    CURRENT STATE — rewritten freely, dated nowhere       ← A1, A5
    │   ├── README.md              pipeline overview + HermiteOctree contract + mini-index
    │   ├── 01-sampler.md … 08-implicit-field-layer.md      (from ARCHITECTURE.md, S3 §2.4)
    │   ├── 09-conventions.md      indexing tables, coordinate/unit conventions, CLI/PowerShell conventions
    │   ├── 10-invariants-and-tolerances.md   mesh validity, thin-feature rule, 4-ULP merge, BBox sentinel, D < depth
    │   └── 11-glossary.md         terms + item-ID vocabulary (#N = roadmap item; Tier-1 item N; § letters of block 12)
    ├── decisions/README.md        one table: ID · decision · date · rationale link         ← A4
    ├── roadmap/                   append-only RECORD (unchanged role; consolidated, Phase 5)
    │   ├── …01–15, 17 as today; 16 → 12/07, 18 → 14/04 (tombstone rows keep the numbers)
    │   └── 19-docs-layers/        the record of this restructuring (plan, per-phase DONE + evidence)
    ├── command_reference/         usage contract (unchanged role; cleaned, Phase 6)
    └── raw/                       IMMUTABLE inputs                                        ← A7
        ├── README.md              index: what each item is, date, why it is here
        ├── study/                 the 2026-08-19 interview pack, verbatim (moved from docs/study/)
        ├── 2026-09-17-docs-workflow-review.md   the chat advice (A)
        ├── 2026-09-17-docs-screening-{roadmap,command-reference,architecture}.md  (S1–S3)
        └── parity.txt, benchmark dumps as they accrue
```

Renumbering never happens (contract §1): 16 and 18 keep their numbers as one-line "merged into" rows;
`study/` keeps its A/B/T names inside `raw/`. Item IDs are untouched.

## 4. Phases

Ordered so that each phase's output is checked by the gate before the next one starts. Effort is in
Claude Code sessions of the kind that produced the 2026-09-11 split. Each phase ends with:
`python scripts/check.py --fast` green, link/anchor count reported, `ctest` count unchanged (docs-only
phases), and a dated DONE entry with that evidence in `roadmap/19-docs-layers/README.md`.

**Resolution rule (applies to every conflict, contradiction and error in S1–S3).** No conflict is
settled by preferring one document over another. Present-tense facts are checked against the
implementation — CLI defaults, synopses and exact messages against `examples/<tool>.cpp`; engine
behaviour against `src/` and `include/`; ABI surface against `capi/`. Historical values (a test count
or parity count at a past date, an entry-point count at a past version) are checked against git at
that commit (`git show <hash>:…`, `ctest -N` / `TEST_CASE` count at that tree); where git cannot
decide, the line becomes `**Reconstructed (<date>) from commit <hash>**` or `DRIFT-PENDING: <what is
owed>` — never a guess. Every resolution line names the file:line or commit it was checked against,
so the check can be re-run by a reviewer.

**Size end-state.** Every maintained file over the cap today is re-dimensioned by a named phase:

| File (today) | Size | Phase |
| --- | --- | --- |
| `docs/ARCHITECTURE.md` | 1,265 lines / 65 KB | 3 → 9 files in `design/`, each ≤ 200 lines |
| `STRUCTURE.md` | 271 lines / 24.4 KB | 1 → annotations stripped, `docs/` subtree collapsed to one line |
| `docs/report-quality-inspection-contouring.md` | 499 lines / 26 KB | 5 → README + 2 children under 12/08 |
| `roadmap/17/04-engineering-quality.md` | 18.7 KB | 5 step 3 |
| `roadmap/17/05-diagnostics-channel.md` | 17.0 KB | 5 step 3 |
| `roadmap/README.md` | 16.1 KB | 5 step 3 |
| `command_reference/README.md` | 15.2 KB | 6 step 5 |
| `docs/study/README.md` | 15.3 KB | 7 step 2 (callouts → ERRATA) |

At the close of Phase 7, `size_baseline` in `check_data.json` is empty and `size_scope` is every
markdown file outside `docs/raw/`. The only files left over the cap are the study pack's A/B/T files,
immutable by decision 2. Near-cap files (`17/02`, `17/03`, `10`, cmdref `11/02`) are not split; the
dedup moves of Phases 5–6 only remove lines from them, and the gate reports their headroom.

### Phase 0 — Land the plan, fix the contract contradictions (½ session)

1. Create `docs/roadmap/19-docs-layers/README.md`: intro, this plan's §2 map as the item list (items
   #36–#45, one per phase deliverable, assigned at birth), status table, footer. Add row 19 to the
   roadmap README "Principal blocks" table and the `STRUCTURE.md` tree.
2. Import A and S1–S3 into `docs/raw/` (creates the folder; `raw/README.md` with four rows) so the
   evidence this plan cites is in the repo, not in a chat.
3. One-line, dated resolutions of the "frozen" contradiction: roadmap `README.md:168-169,172-177`,
   `16:3-4`, `18:3-4`, `17/09:39-49,77-79`, `17/05:17-18`, `STRUCTURE.md:108-109`, cmdref
   `README.md:33-38` — all say "nothing is frozen since 2026-09-11; see 17/10". Remove the `frozen-decl`
   prose paragraphs the `check_data.json` comment still points at, and point the comment at
   `docs/README.md` (written in Phase 1).
4. Commit. Verify: `check.py --fast` 16/16.

### Phase 1 — Entry points (A3) (1 session)

1. **`AGENTS.md`** (new, ≤ 80 lines), in this order: what DualC is (2 lines); build + test + gate
   commands (the 4 opt-in CMake flags included — `POLYSCOPE_VIEWER`, `RAYMARCH_VIEWER`, `FIELD_VIEW`,
   `GLSL_PARITY`); the five non-negotiable principles (nothing stays in chat; README index at every level;
   numbers are IDs; command reference grows with the code; size cap → same-numbered folder); the reading
   order (`docs/README.md` → `design/README.md` → `decisions/` → roadmap README "Current focus" →
   the topic you touch → its command_reference page). **Exactly those three sections.** The
   session-end routine (route facts by the table in `docs/README.md`, then run the gate) is the wording
   of principle 1, not a section of its own; the semantic-lint cadence is one clause of the same
   principle. No counts, versions, item ranges, dates, tool lists.
2. **`CLAUDE.md`** becomes one line: `@AGENTS.md`. Its current content is dispersed: §Architecture →
   `design/README.md` (Phase 3); §Build & test → `AGENTS.md` + root README; §Conventions → `design/09`;
   §CLI tools → already `command_reference/README.md`; §Where to look → `docs/README.md`; current-focus
   pointer → roadmap README only.
3. **`docs/README.md`** (new, ≤ 60 lines): one sentence per child folder (design = how it is; roadmap =
   how it got here; command_reference = how to use it; decisions = what was decided; raw = immutable
   inputs); the **fact-class → owner** table (flag/default/recipe → command_reference; algorithm/
   invariant/convention → design; rationale/evidence/rejected approach → roadmap; decision → decisions
   row + roadmap anchor; benchmark log/import → raw); the conventions block (NN-IDs, README per level,
   footer, caps, folder promotion, the `raw/` exemption); "How to find things" (A6): `rg -n '#34'
   docs/`, `rg -n -- '--tile-depth' docs/command_reference`, `rg -n 'DEFERRED' docs/roadmap`,
   `rg -n 'DRIFT-PENDING' docs`.
4. **Root `README.md`**: rewrite 1–14 and 48–66 for v2 (fields + meshes; delete the submodule sentence);
   replace per-tool sections 88–239 with one paragraph + link each to command_reference; §Documentation
   → one link to `docs/README.md`; drop restated counts (215, 217, 241, 253, 260, 268) and dates
   (213, 218); fix path 224. Target ≤ 150 lines.
5. **`STRUCTURE.md`** (24.4 KB today): strip the 15 date/status annotations (they are a shadow ledger);
   fix 23, 168, 211, 270; add `dualc_field_view` and `dualc_glsl_parity` to "Build targets at a glance";
   collapse the `docs/` subtree (which repeats the roadmap and command-reference indexes line for line)
   to one line per folder pointing at `docs/README.md`, using the gate's existing
   `structure_summarized_prefixes` mechanism; target ≤ 15 KB with no exemption.
6. Gate: extend `claude-md` check to `AGENTS.md` and add the line limit (`root-entry`, Phase 2 list).

### Phase 2 — The gate does the mechanical work (A2, A8) (1 session, before the big moves)

Extend `scripts/check.py` (keep the name — the pre-commit hook and 17/09 already know it; the chat's
`tools/docs_lint.py` is the same role). New fast-tier checks, each with its exception list in
`check_data.json`:

| Check | Rule | Advice / finding |
| --- | --- | --- |
| `drift-pending` | Every `DRIFT-PENDING:` line is listed in the report; exit non-zero only with `--strict` (session-end mode) | A2 "DRIFT-PENDING sweep" |
| `status-sync` | For every Principal-blocks / folder-README status cell, the linked file's first body line under the matching heading carries the same status word | A2 "status-column-matches-file"; S1 §2 (`VALIDATED`, `pending`) |
| `doc-lag` | Newest commit touching `src/ include/ examples/ capi/` vs newest commit touching `docs/`; report if code is ahead by > N commits or > 7 days (report-only, `--strict` fails) | A2 "last-documented-commit vs HEAD" |
| `root-entry` | `AGENTS.md` ≤ 80 lines; no digits followed by `/`, no `v0.`, no `#NN–#NN`, no ISO dates | A3 |
| `decisions-index` | Every `DROPPED` / `DEFERRED` status line in `docs/roadmap/` has a row in `decisions/README.md` whose link resolves to that heading; every DEFERRED row has non-empty trigger text | A4; S1 §3 (11 trigger-less DEFERREDs) |
| `design-no-history` | No ISO date, "since", "used to", "now", "no longer", "Fixed since" in `docs/design/` | A1 (compiled layer must not accrete history) |
| `heading-hierarchy` | No `###` directly under `#`; no second heading whose text equals the H1 | S2 §7 split residue (9 files), S1 §3 (5 files) |
| `stale-paths` | No literal `NN-slug.md` in prose or link text where `NN-slug/` is a folder | S1/S2/S3 (≈20 sites) |
| `directional` | Report "above"/"below"/"this section" within 3 words of a link to another file (report-only) | S2 §7 |
| `usage-in-record` | Report a markdown table whose header contains `Flag` or `Default` inside `docs/roadmap/` (report-only) | Link-don't-duplicate; S1 §3 |

Wiring: `.claude/settings.json` gains a `Stop` hook running `python scripts/check.py --docs` (fast
tier, ≈ 1–3 s) so every Claude Code turn that edited docs ends with the report on screen; pre-commit
keeps `--fast`. Record the check list in `17/09` as a dated follow-up (one line + link to 19).

Verify: each new check has a fixture that fails and one that passes (`scripts/check_fixtures/`,
`ctest` registers `check_selftest`); `--fast` still ≈ 1 s.

### Phase 3 — The design layer (A1, A5) (2 sessions)

**Session 3a — split `ARCHITECTURE.md` into `docs/design/`** using S3 §2.4 as the cut list (README +
children 01–08, ≈ 1,165 lines → ≈ 1,165 lines across 9 files, each ≤ 200). While moving, apply the
compiled-layer rule: every dated change record (373–382, 405–410, 1080–1094, 1098–1107) becomes the
present-tense fact plus a link to the roadmap entry that changed it; the four "Fixed since" callouts
become one line each; the §9/§10.5 bounds contradiction is resolved by reading `src/` (the diagnostics
path is the truth as of 17 #26); the duplicated kQuadCCWPlus and molde numbers become links. Retarget
the inbound links (20 files name `ARCHITECTURE.md` today: roadmap children, `README.md`,
`STRUCTURE.md`, study — the study ones via the errata table of Phase 7, since study files are immutable). Delete the byte-ceiling exemption from
`check_data.json`. `ARCHITECTURE.md` itself: deleted (git history keeps it; roadmap 19 records the
mapping). Section numbers (§3.2 etc.) survive as labels in headings so the study's "§ 4.6" references
still mean something.

**Session 3b — the knowhow children**, harvested by grep from roadmap intros and the duplicates lists:
- `09-conventions.md`: corner/edge/child index tables (ARCH §6), "1 world unit = 1 mm", export-format
  dispatch (the one home; cmdref README keeps the user-facing table and links here for the why),
  Windows/PowerShell 5.1 quoting rule (from `11/01:42-83` — the rule moves here, the cmdref page keeps
  the two-line reminder + link), keyboard-layout independence rule, positional-vs-grouped primitive
  parameters.
- `10-invariants-and-tolerances.md`: mesh validity (0 boundary / 0 non-manifold / χ), thin-feature
  ≥ 2–3 cells rule (one home; 7 copies become links), 4-ULP hit merge, `BBox{}` sentinel semantics and
  the `boundsFallback` diagnostic, `--tile-depth` legal range (settle `D < depth` vs `D = depth` by
  reading `examples/dualc_field.cpp`; the three conflicting statements become one), `mix` blends values
  not crystals (one home; 6 copies become links), preview == export guarantee.
- `11-glossary.md`: DC/MDC/QEF/Hermite/GWN/TPMS/… one line each (source: the study's "Key terms"
  sections, rewritten present-tense), and the **ID vocabulary** that S1 §4.5 shows is ambiguous: `#N`
  = roadmap tracked item; "Tier-1 item N" = block 01 list; "§ A–G" = block 12 sections; "build #1–#3"
  = block 12 pillars — with the rule that prose uses the qualified form.
- `design/README.md` § "Things we do not do": geometry-central is a sibling, never vendored/submoduled;
  no renumbering; no restating counts outside their home; no embedding index (A6); no reflow of `raw/`.

Verify: `check.py --docs` incl. `design-no-history` green; `rg -c 'ARCHITECTURE.md' docs` = 0 outside
`raw/` and the errata table; every design child has the footer
`← Back to the [design index](README.md) · the [docs index](../README.md)`.

### Phase 4 — Decisions index (A4) (½ session)

`docs/decisions/README.md`: purpose paragraph (5 lines) + one table, columns **ID · Decision (one
line, present tense) · Status (DONE-by-decision / DROPPED / DEFERRED + trigger) · Date · Where recorded
(file#anchor)**. Seed: `rg -n 'DROPPED|DEFERRED|decision|rejected|instead of' docs/roadmap` → 54 status
lines today, so ≈ 40–60 rows expected (S1 lists 11 trigger-less DEFERREDs, 3 DROPPED, the C-ABI/Boletus/streaming/geometry-
central decisions). Rows for the 11 trigger-less DEFERREDs get their trigger written **here** and the
roadmap entry gets a one-line dated append pointing at the row (append-only rule respected). IDs: reuse
the item ID when the decision is an item (#18, #24…); otherwise `D-NN` assigned at birth. Add the
`decisions-index` gate check (Phase 2) to `--fast`.

### Phase 5 — Roadmap consolidation (1–2 sessions; S1 is the worklist)

Only the moves; every edit to an existing entry is a dated one-line append, never a rewrite.

1. **Fold continuations**: `16-…-continued.md` § H → `12-field-graph-and-app/07-mesh-preview-sweep.md`
   (verbatim); `18-c-abi-continued.md` → `14-c-abi/04-abi-0-4-0.md`. 16 and 18 stay as 5-line files:
   "Merged into … on <date>; number retired" (never reuse). Fix `README.md:169-170` row order.
2. **One status ledger** for #22–#35: `17/README:141-156` stays; `README.md:69-80,170` → one clause +
   link; `17/08:137-143` → link; drop restated numbers `README.md:26-27,84,165,166,169`,
   `12/README:97,102,105`, `11/README:81`, `15:38`, `05/02:38,45` (each becomes a link to its home; test
   and parity counts have exactly one home: `17/README` "latest verification" line).
3. **Under the cap**: `17/04` (move #34's 50-line record `:192-240` to a two-line entry + links to 07/08),
   `17/05` (verification digests `:203-212` and the re-scoping table → `17/11-diagnostics-verification.md`),
   `README.md` (milestones → date + clause + link, per its own rule).
4. **Move the quality report**: `docs/report-quality-inspection-contouring.md` →
   `12-field-graph-and-app/08-quality-inspection-gyroid-shell/` (README ≈ 120 lines: test case, summary,
   recommended workflow; `01-measurements.md` from 119–241; `02-decimation-approaches.md` from 358–454).
   Banner 3–13 → status on 12 § G; paths 75/115 fixed; footer; 6 inbound links repointed; its byte
   baseline removed from `check_data.json`.
5. **Conflicts** (S1 §4, all 16): resolve each by reading code/`git log`; where a historical value cannot
   be re-derived (171 vs 157 ctest on 06-14/06-17; 68 vs 69 parity on 07-09) append a
   `**Reconstructed (2026-09-xx) from commit <hash>**` line rather than guessing; where nothing can be
   derived, a `DRIFT-PENDING:` line stating what is owed. Entry-point history is fixed to 9 (0.2.0) /
   11 (0.3.0) / 13 (0.4.0) in `10:35`, `14/02`, `14/03:87`; stale ABI tables `14/02:30-56` → link to
   `capi/README.md`.
6. **Chronology repairs**: `03:12-27`, `13/README:8,27,77`, `02:163` (superseded by `dualc_field`),
   `14/README:48-50` (trigger or close), `01/01:88` / `01/02:27` (`#3`/`#6` → "Tier-1 item 3/6").
7. **Duplicates → links** (S1 §3 list of 17): Hermite-tag paragraph, "why a C ABI", three side-car
   blockquotes in `12/README`, dGPU/file-watch prose in 15, dead API sketch `10:74-94` (delete: it never
   shipped, 14 is the record), `05/02` double specs, `17/README:74-133` update paragraphs.
8. **Usage tables out of the record** (S1 §3 list): each becomes a link to the command_reference page
   (model: `12/01:117-124`).
9. **Chat residue** scrubbed (S1 §6 list: "the advisor", "AskUserQuestion", "auto-memory note", "this
   session", "the owner clarified", `user_guidance.md`); stale prose paths (≈ 12) retargeted; misdirected
   anchors (`15:149-150`, `12/README:101`, `16:180`) fixed; § anchors added to folder-README page tables
   so index links land on the child.
10. Legend: either use NEXT (mark the one item that is next) or drop it from the legend; add
    `PARTIAL` qualifier rules; `VALIDATED`/`pending` → legend words.
11. **The roadmap explains the present, never describes it.** Every topic intro's "how it works
    today" paragraph (the material Phase 3 harvested into `design/`) becomes one sentence + link to the
    design child; a record entry that states current behaviour keeps the dated fact and links `design/`
    for the present-tense description. Not mechanizable — it is item (c) of the semantic lint (Phase 8).

Verify: `check.py --fast --strict` green (0 unlisted `DRIFT-PENDING`), `status-sync` green, 0 broken
links, S1 §5 navigation questions 3, 11, 12 now ≤ 2 hops (re-run the 14 questions and record the table).

### Phase 6 — Command reference cleanup (1–2 sessions; S2 is the worklist)

1. **User-misleading defects first** (S2 §8.1): `04` `--offset` default, `11/06:116-127` weld text,
   `12/02:68`, `--tile-depth` bound, `11/07:66` `\n`, `11/06:80` fraction — each fixed by reading the
   `examples/*.cpp` source, not the other page.
2. **Contract back-fill**: real flag tables on `03` and `06` (defaults from source; synopsis lines
   complete); `## Recipes` tables with letter IDs on `02`–`06`, `11/README`, all `11/*` and `12/*`
   children (≈ 60 unlabelled commands); `01` `1.x` → `D1…`; `06` T-collision resolved; Tool-12 commands
   moved from `11/02`, `11/04` to `12/*`; POSIX-safe paths in `07`; exact messages quoted where S2 says
   "unquoted" (from source).
3. **Rationale → its home**: S2 §8.4 list (≈ 250 lines) — design-class facts go to `design/10`/`09`
   (Phase 3 already created the homes), history-class to the roadmap page named in S2, each replaced by
   one sentence + link. Dates/"now"/"since"/phase jargon/"Approach A/B"/"Boletus" removed from usage pages
   (S2 §8.3 list).
4. **One canonical spot + link** for the six multi-copy facts (S2 §8.7) — the copies become one-line
   reminders linking to `design/09`/`10` or the cmdref README section.
5. **README**: redirect table rows added/split (S2 §8.6); options matrix gains the four missing
   columns; build prelude gains Ninja/POSIX + the four opt-in flags (moved from pages 08/10/12); process
   history 33–38 and dated lines 119/145/162–164 → 17/10 link; size back under 15,000 bytes.
6. **Split residue**: duplicated second headings deleted (9 files); "above/below" → links; stale link
   texts (6); `12/README` mini-index gets real summaries; `12/02:7-37` paragraph → one sentence + link.
7. Counts reconciled from source (decorators, generated/not-generated meshes) and stated once in the
   README appendix only.

Verify: `flag-table` check extended to assert every `--flag` in every recipe is in its page's table;
`heading-hierarchy`, `stale-paths`, `directional` green; S2 §6 questions 5, 6, 12 now ≤ 2 hops.

### Phase 7 — `raw/` and the study pack (A6, A7) (½–1 session)

1. `git mv docs/study docs/raw/study` (history preserved; A/B/T names untouched; no reflow — confirm
   `17/10`'s 88-column reflow excluded it via `git diff 3d73220~1 --stat -- docs/study`). Retarget the
   ≈ 33 `docs/study/` citations across 13 files (roadmap 17, `AGENTS.md`, `README.md`, `STRUCTURE.md`, `17/09:80`) with
   one `sed`; the gate's `links`/`anchors` checks are the proof (as on 2026-09-11).
2. `docs/raw/study/ERRATA.md` (new, the only writable file in the pack): a table of every dangling
   reference inside the frozen files — 12 folder paths (S3 §7), 6 line refs into the old ARCHITECTURE,
   the 13-vs-14 item count — each with its current target. The study README's two status callouts move
   here too, so the README stops growing (it is 339 B over cap today).
3. `docs/raw/study/figures/README.md`: 24 rows (file → used by); note the 655 KB SVG as an asset.
4. `raw/README.md` rows for: study pack, chat review (A), screenings (S1–S3), `parity.txt` (moved from
   repo root), future benchmark dumps. Rule stated once: files under `raw/` are never edited; a
   correction is a new dated file or an ERRATA row.
5. Search: `docs/README.md` "How to find things" (Phase 1) is the whole search layer. FTS5/QMD recorded
   in roadmap 19 as **DEFERRED — trigger: `docs/raw/` > 50 files or > 1 MB of text, or three sessions
   in a row where `rg` failed to find a fact that was in the repo**.
6. Gate: `size_scope` excludes `docs/raw/`; `footer_roots` gains `design`, `decisions`; the study
   exemption sentence in `17/09` becomes a link to `raw/README.md`.

### Phase 8 — Semantic lint and the shrunken skill (A9, A10) (1 session)

1. `.claude/commands/docs-semantic-lint.md`: a procedure, not a check — read every `design/` child
   against the `src/` files it names; list (a) claims contradicted by code, (b) design pages no roadmap
   entry has touched since the last code change to their module (`doc-lag` gives the candidates),
   (c) present-tense design description living in `roadmap/` or `command_reference/` instead of a link
   to `design/`, (d) orphan pages (no inbound link outside indexes), (e) decisions whose trigger
   condition is now met.
   Output is a dated entry in `roadmap/19-docs-layers/` with a findings table; fixes are ordinary
   session work. Cadence rule in `AGENTS.md`: after any session that edits both `src/` and `design/`,
   and at least monthly — stated as a clause of principle 1 in `AGENTS.md`, not a separate section.
2. First run performed and recorded (it will find the leftovers of Phases 3–6; that is the point).
3. **Skill revision** (`repo-docs-lifecycle`, proposed via the skill-review card): body shrinks to the
   two moments with four steps each — start: `check.py --docs`, read `AGENTS.md` order, read the topic;
   end: `check.py --docs --strict`, harvest the session into a fact list, route each fact by the
   `docs/README.md` table (usage → cmdref page; design → design child, rewritten in place; rationale/
   evidence → roadmap entry, appended; decision → decisions row; import → raw), `check.py --docs
   --strict` again. `doc_conventions.md` keeps the anatomies but drops everything the gate now enforces
   (sizes, footers, frozen headings, legend words, index completeness) to a one-line "the gate checks
   this" each. Legacy-containment section (§1b) is deleted: nothing is frozen and the folder-promotion
   rule replaces it.

## 5. Sequencing, effort, dependencies

| Phase | Sessions | Depends on | Gate proof at close |
| --- | --- | --- | --- |
| 0 Land + contract fixes | ½ | — | `--fast` 16/16 |
| 1 Entry points | 1 | 0 | `root-entry` (once Phase 2 lands) |
| 2 Gate extension + hook | 1 | 0 | fixtures pass; `--fast` ≈ 1 s |
| 3 Design layer | 2 | 1, 2 | `design-no-history`, `stale-paths`, 0 broken links |
| 4 Decisions index | ½ | 3 | `decisions-index` |
| 5 Roadmap consolidation | 1–2 | 2, 4 | `status-sync`, `--strict` |
| 6 Command reference | 1–2 | 3 | extended `flag-table`, `heading-hierarchy` |
| 7 raw/ + study | ½–1 | 1 | `links`/`anchors` after the move |
| 8 Semantic lint + skill | 1 | 3–7 | first lint entry recorded |

Total ≈ 8–10 sessions, run sequentially by one thread per phase (see §7, decision 6); 7 can be
slotted any time after 1. Nothing in the plan touches `src/`, `include/`, `capi/`, `tests/`;
`ctest` (255) and `dualc_glsl_parity` (73) must be unchanged at every commit — that is the proof no code
was edited "to match the docs".

## 6. Deliberately not done

- **No renumbering, ever** — 16/18 become tombstones; 19 is the next free number; study keeps A/B/T.
- **No vector database, no RAG skill** (A6). `rg` + the indexes are the search layer; FTS5 is DEFERRED
  with a written trigger.
- **No rewrite of the study pack** — it is raw material; errata live beside it.
- **No history rewrite in the roadmap** — every correction is a dated append or a `Reconstructed` /
  `DRIFT-PENDING` marker; the design layer is where rewriting is allowed.
- **No new ceiling exemptions** — after Phase 3 the only size-exempt tree is `docs/raw/`.

## 7. Decisions

All decided by Giacomo on 2026-09-17:

1. **`AGENTS.md` is canonical; `CLAUDE.md` = `@AGENTS.md`.** (Phase 1 as written.)
2. **`docs/study/` moves to `docs/raw/study/`**, ≈ 33 citations retargeted by the gate-checked move.
   (Phase 7 as written.)
3. **`ARCHITECTURE.md` is deleted after the split**; git history keeps it, roadmap 19 records the
   section → child mapping. No redirect stub. Boletus's prose paths are reported to Boletus, not kept
   alive here (same treatment as on 2026-09-11).
5. **Claude Code `Stop` hook** runs `scripts/check.py --docs` on every docs-editing turn; pre-commit
   keeps `--fast`. (Phase 2 as written.)

4. **Quality report → `12-field-graph-and-app/08-quality-inspection-gyroid-shell/`**, nested under
   block 12 because 12 § G (decimation) already cites it as its evidence and 12's README already
   indexes it. (Phase 5 step 4 as written.)
6. **Sequential execution**: one Claude Code thread per phase, in order, gate green and a commit at
   each close. Phases 5 and 6 are never run concurrently (Phase 6 moves rationale into roadmap pages).

All six decisions are closed; Phase 0 can start.

---

*Line numbers in this plan refer to commit `3d73220`; S1–S3 carry the full evidence. Working-tree
changes present on 2026-09-17 (`capi/`, `examples/`, cmdref `03`, `08`, study README) are not screened.*
