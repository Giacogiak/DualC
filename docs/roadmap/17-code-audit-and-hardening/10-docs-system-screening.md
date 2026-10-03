# Docs-system screening

**DONE (2026-09-11).** The record of the first audit of the **documentation system
itself** — the roadmap,
the command reference and the gate that polices them — rather than of the engine. It
was run because the project's continuity depends on a future thread reconstructing
state from the repo alone, and nobody had checked whether the repo actually
supported that. The method, the verdict, what was fixed the same day (R1–R4) and
what was deliberately left for a separately requested pass (R5) are all here; the
usage side of every fix is in [`../../command_reference/`](../../command_reference/README.md).

## Method

Three read-only screenings in parallel (roadmap 01–16 + README; this folder;
`command_reference/`), each reading every file in scope in full (~8,000 lines) and
checking **semantics** the gate cannot see: status claims against the code and
`git log`, quoted numbers against `ctest -N` / the registries / `capi/`, the item-ID
ledger, each contract clause, duplication across files, and 21 zero-context
navigation questions. Every reported defect was then re-verified here with one
grep/git/ctest command before it counted (a claim that needed a build or a GPU to
check was reported as *unverifiable*, not as a defect). `scripts/check.py --fast`
was green throughout (links, anchors, indexes, sizes), so none of this was
mechanical breakage.

## Verdict

**The data was accurate; the system was thorough, redundant, and enforced only the
structural half of its own contract.**

- *Accurate:* `ctest -N` = 253 matched every "253/253"; the fifteen historical counts
  reconstruct by formula (`TEST_CASE − hidden + add_test` at each commit) and agree;
  parity 73/73 and ABI 0.4.0 match the build config; every DEFERRED names a trigger;
  all fourteen items #22–#35 had the status the code shows; 22 of 23 sampled study
  citations land on the exact passage.
- *Redundant:* ~950 of 3,613 roadmap lines had another home (node/param tables
  already in the command reference, the C header already in `capi/`, an architecture
  hand-off duplicating `ARCHITECTURE.md`), and the status ledger existed in **four
  copies** (roadmap README, this README, `CLAUDE.md`, `STRUCTURE.md`). Every factual
  conflict found had that shape — *a fact stated in N places, updated in N−1*:
  primitive count 28/29/30/40, C-ABI entry points 11 vs 13, item range #22–#34 vs #35,
  parity 69 vs 73, an export bug "still open" three months after 09 recorded the fix,
  13's header saying "design only" 300 lines above its DONE. `CLAUDE.md`, the first
  file every session reads, was three months behind the roadmap README.
- *Half-enforced:* the gate's nine checks were all structural. Of fourteen semantic
  clauses (link-don't-duplicate, frozen headings, one-line-only edits to frozen
  files, append-only chronology, status vocabulary, footers, …) twelve showed
  violations; the two that held (triggers, evidence) held because the author cares.
  The size ratchet deliberately skipped frozen files, and two of them had received
  multi-line record appends after their freeze.

**One code defect** surfaced from a recipe: the `mix` recipes on page 11 used
`plane(normal=[1,0,0],offset=0)`, a grouped key `plane` never had, so the control
field kept the registry default and the morph ran along **Y** while the prose said X.
`field_graph.cpp` and `field_glsl.cpp` shared the omission, so export and GPU preview
were wrong *identically* — a class of defect `dualc_glsl_parity` cannot see, and its
own `plane` case used the same spelling (with the default's value, so it "passed"
while testing nothing).

## Fixed the same day

**R1 — the gate enforces what the contract already said.** Seven fast-tier checks
added to `scripts/check.py` — `footer`, `frozen-growth`, `heading-status`,
`status-vocab`, `index-staleness`, `claude-md`, `flag-table` — what each holds and why
it exists: [09 § The semantic half](09-local-checks-gate/01-the-docs-checks.md#the-semantic-half) (the one
copy; the per-check descriptions that stood here too went there on 2026-09-18).
`ordering` now also fails a duplicated `## #NN`; the `--metrics` needle that could
never match was fixed. 16 checks, ~1 s.

**R2 — one status ledger, one home per fact.** `CLAUDE.md` "Current focus" is a
five-line pointer with no numbers. The roadmap README's milestone rows are back to
date + clause + link (its own rule); legend gains DROPPED and the PARTIAL qualifier;
every Status cell is on-legend; `#16` named; the QEM/Approach-B credit and the
"step 6" address corrected; `STRUCTURE.md`'s tree lines lost their per-item status
clauses (it is a layout map, and was the fourth ledger copy). Item ranges widened to #35
in every copy. Usage tables
removed from the frozen 12 / 13 / 14 in favour of one link each (the only
restructuring of frozen files, done under this explicitly requested pass); 14's
post-freeze follow-up moved to the new [14/04](../14-c-abi/04-abi-0-4-0.md). Dated
one-line resolutions appended (never rewritten) at 03:25, 11 (export bug, step 3,
5a.3's missing status + trigger), 13:8, 14 § 9, 07:108 here, 05:43 here, the C4
citation, 06's provenance sentence (the E/C ids are this ledger's, not the audit's).

**R3 — the code.** Unknown parameter keys are now rejected on both paths with the
node's locator and the accepted list (`unknown parameter 'normal' for 'plane' (a
primitive takes its values positionally …)`), tracked by the single read point
`findParam` so no per-op table exists to drift; two tests (CPU and GLSL, including
the memo path). The five recipe sites, the README example's bogus `thickness`, and
the parity harness's `plane` case corrected. The ABI sees the same rejection
(`DUALC_ERR_GRAPH`); recorded in [14/04](../14-c-abi/04-abi-0-4-0.md), Boletus checked and
unaffected. `dualc_gen_demo` gains `cube` (byte-
identical geometry to the hand-authored file) so `all` yields every recipe input;
`foot`/`molde`/`opA`/`opB`/`mesh-soup` inventoried as not generated.

**R4 — the command-reference template back-filled.** Flag tables with real defaults
on pages 01, 02, 04, 05 (05 had none: four flags undocumented); page 07's `-o`
default (`<type>_slice.png`); page 06's pointer to a `dualc_primitive` TPMS that does
not exist; page 11's `--tile-depth = depth` claim (it runs 8 near-full tiles, the
fallback needs `D ≥ depth + 1`), the `≥ 2` floor and the `--weld`-without-tiling
no-op; the README's "Common options" replaced by a per-tool matrix (`--bounds` was
claimed for three tools that lack it), six redirect rows for features that live
500–1,400 lines into page 11, and the tiled-path warning corrected.

*Verified:* `scripts/check.py --fast` 16/16 (after this file lands); `ctest`
**255/255** (+2); `dualc_glsl_parity` **73/73** with the corrected `plane` case;
`dualc_field --expr 'plane(normal=[1,0,0],offset=0)'` exits 2 with the locator;
`dualc_gen_demo cube` diff-identical to `data/cube.obj` modulo whitespace.

## The split pass (R5)

**DONE (2026-09-11), explicitly requested.** Four of the eight frozen files became
same-numbered folders; the number stays the topic's identity, the folder README is the
overview + mini-index, children restart local numbering, and every inbound link was
retargeted onto the child that owns its anchor (the gate's `links`/`anchors` checks are the
proof: 700 links, 0 broken). Text moved verbatim; the one liberty taken was the one the
contract reserves for exactly this moment — status-bearing headings (`## D. … [DONE —
2026-06-14]`) were frozen to their titles with the status on the first body line, so
`heading_status_baseline` lost three entries instead of gaining new grandfathered ones.

| Was | Now | Cut |
| --- | --- | --- |
| `command_reference/11-dualc_field.md` (1,441 lines, 85 KB) | [`11-dualc_field/`](../../command_reference/11-dualc_field/README.md): README + 8 pages | overview · input forms + op vocabulary + shorthand · strut lattices · graded & morph · open-surface workflow · decimation · streaming (with the `--mem`/`--tile-depth` mis-nesting undone) · `--mem` · 3MF & `--weld` |
| `roadmap/12-field-graph-and-app.md` (704 lines, 50 KB) | [`12-field-graph-and-app/`](../12-field-graph-and-app/README.md): README + 6 pages | § A + § C + schema appendix · § B codegen · § D app · § D.2 performance · § F open surface · § G decimation |
| `roadmap/01-core-dual-contouring.md` (273 lines, 28 KB) | [`01-core-dual-contouring/`](../01-core-dual-contouring/README.md): README + 2 pages | engineering record (§ 2/3/5/7) · bug catalogue (§ 4.1–4.5, § 4.9) — **not** deleted: the plan's premise that it duplicated `ARCHITECTURE.md` was wrong (1 of 133 sentences), and CLAUDE.md points at it for exactly that history |
| `roadmap/05-tpms-lattices.md` (332 lines, 28.5 KB) | [`05-tpms-lattices/`](../05-tpms-lattices/README.md): README + 3 pages | #16/#20/#18 on the README · #17 · #17b template + Phases 3–4 · Phase 5 + closing (the exec-spec kept verbatim, its line numbers flagged as dated) |
| `roadmap/11-dense-lattice-deliverable.md` (409 lines, 27 KB) | [`11-dense-lattice-deliverable/`](../11-dense-lattice-deliverable/README.md): README + 4 pages | the decision · Phase 0 benchmark · step 5 + § 5a · § 5b record (steps 4/6 + status on the README) |
| `roadmap/13-graded-tpms.md` (348 lines, 20 KB) | [`13-graded-tpms/`](../13-graded-tpms/README.md): README + 2 pages | analysis · design & plan (decision + status on the README) |
| `roadmap/14-c-abi.md` (276 lines, 20 KB) | [`14-c-abi/`](../14-c-abi/README.md): README + 3 pages | design (§ 1/2/5) · surface & contract (§ 3/4) · implementation & verification (§ 6–8); § 9 on the README, 18 continues |
| `command_reference/12-dualc_field_view.md` (578 lines, 34 KB) | [`12-dualc_field_view/`](../../command_reference/12-dualc_field_view/README.md): README + 4 pages | controls · nodes & examples · recipes by node · open-surface preview |

Not moved: the ~218 rationale lines in page 11. Their roadmap homes (11, 13) are frozen, so
moving them would have been the append the contract forbids; the lines are guidance with
measurements users need at the point of use, and they stayed with it. `ARCHITECTURE.md` is
**not** frozen and not split: it is a living design document edited in place by design, so
freeze + continuation does not fit it; it keeps a size ceiling in `check_data.json` that a real
architectural change bumps in the same commit — the decision is recorded there. *(2026-09-17: superseded — `ARCHITECTURE.md` is split into `docs/design/`, nine pages each under the cap, and its ceiling is gone; [19/02](../19-docs-layers/02-record-phase-3.md).)* **Second pass, same day (explicitly requested):** the
remaining four (roadmap 11, 13, 14; command_reference 12) were split the same way, so **no file
is frozen any more** — the `frozen` list in `check_data.json` is empty and the size contract
applies to every page. The retrievability re-run's other hindrance — record entries written as
single unwrapped mega-paragraphs, so a line number said little — was addressed by reflowing
over-long plain prose lines at 88 columns (126 lines across 53 files; word sequences identical
to HEAD, Markdown renders the same; tables, headings, lists, code and baselined files untouched).

The quality-inspection report's byte baseline was bumped twice (26,719 → 26,738) — link-path
growth from the retargets, not content. *Verified:* `scripts/check.py --fast` 16/16
after every cycle; the three source comments that
named `12-field-graph-and-app.md` as the schema home now name the child; Boletus (`D:\Boletus`)
holds one markdown link and six prose paths into the old files — reported, not edited.

## What a next thread should not repeat

- Do not restate a number outside its home; the gate now fails `CLAUDE.md` for it,
  but the roadmap README still can (its milestone rows are the temptation).
- No file is frozen any more; if one crosses the cap, the split pattern above is the fix
  (a same-numbered folder), never a continuation of a file that could simply be split.
- Do not read `index-staleness` as a failure — it is the list to re-check at
  session close.
- `plane`, `infinitecone` and every primitive without a grouped-key layout take
  positional values; the parser now says so.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
