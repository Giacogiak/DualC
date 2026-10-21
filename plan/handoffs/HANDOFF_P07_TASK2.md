# Handoff — P07.2: pointer stubs, Track 2, decision rows, indexes

Unit P07.2 of [`plan/16_P07_editor-acknowledgement.md`](../16_P07_editor-acknowledgement.md)
§ T2, the last unit of P07. Session of 2026-10-21, branch `feat/hygiene-and-editor`, start
commit `0510dc8`; the phase's start commit is `3a72cd8`. Docs only: no `src/`, `examples/`,
`tests/` or `docs/design/` file changed. Nothing pushed, no remote touched, `main`
untouched, `~/Documents/DualCEditor` not opened this unit (P07.1 read it at `74023cf`). The
record is [22 § How the block was opened](../../docs/roadmap/22-dualceditor-handoff/README.md#how-the-block-was-opened);
this handoff adds the evidence and what P08 needs.

## Outcome in one paragraph

No Track 2 UI item has DualC as its owner any more. The roadmap README's Track 2 is a
pointer to block 22 plus the retained, editor-driven D-29 / D-30 / D-31 list; the old homes
point to 22; decision rows D-16 / D-17 / D-25 name DualCEditor and cite D-66 and 22; row 22
of § Principal blocks is enriched, row 12's status cell names what moved, and "How the
blocks relate" names 22. `--docs --strict` passes (28 checks); the full gate result is in
§ Gates; the loss audit's triage is in § Loss audit.

## What changed, and why

1. **`docs/roadmap/README.md`**
   - **§ Track 2 — Standalone raymarch app**: the heading is kept (02 links its anchor
     `#track-2--standalone-raymarch-app`); the body is one paragraph: moved to DualCEditor
     (D-66), section-plane UI and parameter edits DONE there, clip mask / Web build / future
     UI moved, all indexed with the intent verbatim in 22; DualC retains the baked-preview
     hardening D-31 / D-30 / D-29, all DEFERRED, with the 12/07 § H link. The five items it
     replaced are quoted verbatim in [22/02](../../docs/roadmap/22-dualceditor-handoff/02-archived-intent-continued.md).
   - **Row 22**: "What it covers" now names the hand-off table's items, the boundary rule,
     the retained items and the verbatim intent; status "index — DONE here; DualCEditor
     tracks its own status" (block 15's wording).
   - **Row 12** status: "clip mask" left the DEFERRED list; "sliders, parameter edits, clip
     mask, Web build → **DualCEditor** ([22])" added.
   - **How the blocks relate**: one clause — the standalone-app UI above the codegen lives
     in DualCEditor (22).
   - Size: 15,357 → about 14.7 KB.
2. **Old homes** (headings untouched):
   - **12/03 § Real-time parameter push**: the body is a two-line pointer (D-25, editor's
     uniform-only edits, the intent and the difference in 22). The old body is quoted in
     [22/01](../../docs/roadmap/22-dualceditor-handoff/01-archived-intent.md).
   - **12/03 § Section planes**: the DONE keyboard-planes record stays; the sliders sentence
     (deferred to the public release … PLANNED) became "The UI sliders (D-17) moved to
     DualCEditor, which shipped a section tool on these uniforms — [22]". Quoted in 22/01.
   - **12/03 § D**: the record stays; a dated pointer `*(2026-10-21: the Web build and every
     interactive UI item moved to DualCEditor — [22].)*` follows the intro.
   - **12/05 § Deferred (optional) — render-time clip mask**: the body is
     "**DEFERRED** — moved to DualCEditor (D-16), not started there; the intent, verbatim, and
     its relation to #13 / #14: [22]". Quoted in 22/02. § F's status blockquote gained
     ", and DualCEditor's since 2026-10-21 ([22])" (added after the audit's scratch commit;
     the line's old words are all kept, checked by hand).
   - **08 § Generalized**: a trailing dated pointer for the Web build.
   - **12 README**: the page-table rows for 03 (parameter push) and 05 (clip mask) and the
     status rows D.1 and F name DualCEditor with a link to 22 instead of PLANNED / DEFERRED.
3. **Usage pages** (the contract a reader acts on): cmd-ref
   [12/01](../../docs/command_reference/12-dualc_field_view/01-controls.md) said the host
   uniform push "is not built" — now adds that DualCEditor edits uniforms in its own process;
   [12/04](../../docs/command_reference/12-dualc_field_view/04-open-surface-preview.md) says
   the clip mask moved to DualCEditor and 12 § F points to the hand-off. No flag changed.
4. **`docs/decisions/README.md`**:
   - D-16: `DEFERRED — owned by DualCEditor, not started there ([D-66], [22]); trigger †: …`
     (the trigger kept word for word).
   - D-17: `DONE 2026-10-14 by DualCEditor, a section tool on the same uniforms ([D-66], [22])`.
   - D-25: `DONE 2026-10-10 by DualCEditor: parameter edits are in-process uniforms, no
     channel ([D-66], [22])`.
   - Their old status cells are quoted verbatim in 22/02. Legend words only (`status-vocab`
     and `decisions-index` pass). Size 14,579 → 14,663.
5. **Block 22 README § How the block was opened**: the P07.2 in-run choices (below).
6. **The plan file**: T2 step 2 gained a dated correction (see § What I got wrong).

**Chosen in-run (pending owner review)** — also in 22 § How the block was opened:
- **Two old homes keep their record.** 12/03 § D and § Section planes are DualC's
  `dualc_field_view` records, and 08 has no Web-build section; each got a pointer sentence for
  its moved part, not a one-line body. Only 12/03 § Parameter push, 12/05's clip mask and
  Track 2 became pointer bodies.
- **D-16 stays DEFERRED with its trigger**, owner named as DualCEditor: nothing started it on
  either side. D-17 and D-25 read DONE and stay in the open table (the #31 / #33 precedent),
  not moved to `01-settled.md`.
- **The two usage pages** name DualCEditor where they said "not built".
- **No decision row for the Web build**: it never had one; D-66 and 22's table cover it.

## Exit criteria — evidence, bullet by bullet

- **"The roadmap acknowledges DualCEditor as the home of the standalone UI."** Block 22 (P07.1)
  plus this unit's Track 2 pointer, row 22 / row 12 / "How the blocks relate", the old-home
  pointers and the D-16 / D-17 / D-25 rows. Check run: `grep -rniE "clip mask|sliders?|web
  build|D-16|D-17|D-25|parameter push"` over `docs/` minus `raw/` and 22 — every remaining hit
  is either a dated record (19/*, 07, 12/02, 13/01, 01's #13/#14 note), the DualCEditor
  pointer itself, or a heading kept for its anchor; none names DualC as the owner of a UI item.
- **"Nothing is pushed."** `git status -sb` shows no upstream move; no `git push` was run,
  no remote added or changed.
- **T2 acceptance**: old homes point to 22 (listed above); loss audit clean (§ Loss audit);
  `--docs --strict` green (§ Gates); committed, not pushed.
- **T1 acceptance** (P07.1): unchanged and still true — 22 cites every editor fact with a
  commit or file at `74023cf`; 01 / 02 verbatim.

## Gates

- `python3 scripts/check.py --docs --strict`: **PASS**, 28 checks (27 passed; index-staleness
  lists the usual report-only rows). Re-run after the last edit.
- `python3 scripts/check.py`: **PASS**, exit 0, on the final working tree before the commit:
  32 checks, 29 passed, 0 failed, 2 skipped (configure reused, warnings incremental: nothing
  compiled, docs only). ctest **369 / 369** (117 s, serial). Log: `/tmp/p07/gate2.log`.
- No GPU tier: no field node, `field_graph.cpp`, `field_glsl.cpp` or `parity_expected_cases`
  moved.

## Loss audit

Base `3a72cd8` (the phase's start), head the scratch `git commit-tree` `c243701` of the
staged tree, `--scope docs/ --source <page> --untriaged --triage /tmp/p07/triage.json`
(P07.1's triage: audit key `3a72cd8`, the `3d73220` thresholds). Eight pages: roadmap
README, 12/03, 12/05, 12 README, 08, decisions README, cmd-ref 12/01 and 12/04. Run as 8
parallel processes on 8 cores: about 13 min wall clock together (≈5 min alone, per P07.1).
Tree-wide untriaged: sentence 3,204 (P07.1: 3,203), evidence 5,683, heading 178,
identifier 978 — the known baseline noise. Every row on these pages, triaged:

| Row | Class / best match | Verdict |
| --- | --- | --- |
| 12/03:78 "**DEFERRED** — a performance optimization …" | rewritten / 22/01 1.000 | moved (verbatim in 22/01) |
| 12/03:108 "Until then the file-watch is sufficient." | rewritten / 22/01 1.000 | moved |
| 12/05:62 "Exposed as a second field-graph …" | rewritten / 22/02 1.000 | moved |
| 12/05:90 "**Implementation sketch …**" | rewritten / 22/02 1.000 | moved |
| roadmap README:108 (D-25 item), :119 (Web build item) | rewritten / 22/02 1.000 | moved |
| 12 README:69, the 03 page-table row | rewritten 0.562 / same file | kept, rewritten in place; its dropped clause "its Boletus-side write-up is Boletus 07/02 § 6" (evidence `07/02`, token-only) is in 22/01's verbatim § Parameter push, link included — moved |
| roadmap README:41 (#60 milestone), :21 (17/20, 2026-10-14) | P07.1's rows | kept — triaged in P07.1's handoff (homes 21/08:68, 17/20) |
| 12/03:3, :32, :60; 12/05:3, :39; 12 README:42, :72; 08:35; decisions README:19; cmd-ref 12/01:39, :65, :119; 12/04:33, :37 | rewritten / same file (or a sibling) at 1.000 | kept — untouched lines, sentence-splitting noise |
| 12/03:7, 12 README:20 (dates) | unverifiable-context, token present | kept — untouched lines |

**0 untriaged** on the touched pages after this triage. The triage lives here, not in
`scripts/docs_loss_audit.json` (as in P07.1). The hand check of every `^-` line of
`git diff -U0 -- docs/` agrees: the deleted lines are the 12/03 sliders sentence and § Parameter
push body, 12/05's clip-mask body, Track 2's body and the three D-row status cells (all
verbatim in 22/01 or 22/02), plus lines rewritten in place (rows 12 and 22, "How the blocks
relate", the 12 README rows, the two cmd-ref sentences, 08's last line, 12/03's line 17).

## What I got wrong on the way

- **The plan's T2 step 2 asked for one-line bodies in 12/03 § D and § D.1 and in "08's web
  build".** P07.1 had warned that § D and § Section planes are DualC's own records and 08 has
  no Web-build section. I followed the warning and corrected the plan file in place (a dated
  note under step 2) rather than wiping DualC records.
- **A first Python edit of 22's README failed its assertion** (I matched a line ending that
  was not there); nothing was written, the second edit matched `  that difference.`.
- **The first D-17 / D-25 cells kept their old trigger history**; I cut it, since 22/02 quotes
  the old cells verbatim and the #31 precedent is `DONE date (record)`.
- **The loss audit run in parallel** (8 pages at once on 8 cores) overran a 10-minute
  foreground call and finished in the background after about 13 min. My `until pgrep -f …`
  wait loops matched their own command lines, so they never ended; I killed them. Next time:
  a foreground call per page, as the briefing says, or `pgrep -f` with a pattern that cannot
  match the waiter.

## Left open — for P08 and the owner

- The P07.1 and P07.2 in-run choices in 22 § How the block was opened.
- Still stale, harvest work, not touched here: Current focus and Track 1 item 1 say block
  21's "PR is the owner's", and the 2026-10-16 milestone says "the branch awaits its PR"; the
  raw plan records the PR merged as `5ce5b1c` on 2026-10-19.
- No milestone row was added for block 22: the five-row window is P08's harvest call.
- **Q7 in the editor's questions** may be a real DualC OBJ-reader bug, unverified.
- Next free IDs unchanged: **D-67, #63**, next roadmap block **23**.
- Cap room now: roadmap README ≈ 14.7 KB, decisions README 14,663, 01-settled 15,356,
  22 README 10,574.
- Scratch, safe to delete: `/tmp/p07/` (P07.1's archive builder, the triage, both units'
  audit outputs in `/tmp/p07/a2/`).
