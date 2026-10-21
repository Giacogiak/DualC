# P07 — DualCEditor owns the standalone UI

Sources: § 1 ("Track 2") and § 2 "DualCEditor" of
[the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md). The precedent is
`docs/roadmap/15-boletus-handoff.md`. The owner prefers an extracted index page with pointer
stubs in the old homes, not inline banners left in place, and wants the original intent kept
verbatim. Branch `feat/hygiene-and-editor`. Never push. **Do not edit `~/Documents/DualCEditor`**;
read it only.

### T1. The hand-off index (new roadmap block)

1. **Re-read the editor before writing.** Read `~/Documents/DualCEditor`: its README, its
   `docs/README.md`, `docs/decisions/` (E-NN), `docs/roadmap/` (including
   `06-dualc-questions.md`), `docs/command_reference/01-viewport.md` § Section and § Parameters,
   and `git log --oneline`. Confirm each fact in the raw plan's § 2 and note anything newer.
2. **Create the block.** Use the next free block number (22 on 2026-10-19), e.g.
   `docs/roadmap/22-dualceditor-handoff.md`, with the outline of block 15:
   - **What this file is.** The standalone-app UI lives in DualCEditor
     (<https://github.com/Giacogiak/DualCEditor>); the items that crossed the repo line are
     indexed here, with their intent kept verbatim.
   - **The boundary rule.** DualC owns the engine, the field graph and the GLSL codegen
     libraries that the editor links (`dualc_examples_fieldglsl` and its dependencies, consumed
     per the editor's E-06 at a pinned DualC commit), the C ABI, and `dualc_field_view` as the
     reference viewer. DualCEditor owns every interactive UI concern.
   - **Hand-off table** (DualC origin | what it was | DualCEditor home | status):
     - D-17, section-plane UI: DONE in the editor, `1b59389`, cmd-ref § Section.
     - D-25, parameter push: DONE in the editor, in-process uniforms with no recompile
       (`95e1a8d`, `trace_renderer.cpp:284-291`), cmd-ref § Parameters.
     - D-16, render-time clip mask: moved, not started there.
     - Web build (Track 2 item 5): moved, not started there.
     - Any future UI item.
   - **DualC retains, editor-driven:**
     - The baked-preview hardening D-29 / D-30 / D-31 (codegen side).
     - The editor's E-20 request to promote the graph and codegen to a supported library.
     - The `vec2` uniform gap noted in 21/09: the editor's to fix, and DualC's to keep stable.
     - The editor's DualC questions Q1–Q4, if any has been raised.
   - **Archived intent.** The original DualC prose, verbatim, one `### From [NN § X]` subsection
     per moved item: 12/03 § D (section planes, parameter push), 12/05 § F (clip mask), 08 or
     12 (web build).
3. Fill the link in the decision row P01 created for the Track 2 boundary.

#### Acceptance criteria

- The block exists, every editor fact is cited with a commit or file in the editor repo, and the
  archived prose is verbatim.
- `--docs --strict` is green, and the work is committed.

### T2. Pointer stubs, Track 2, decision rows, indexes

1. **Track 2 in `docs/roadmap/README.md` § Next up.** Replace it with a short stub: "Track 2 —
   moved to DualCEditor", a link to the new block, and the retained editor-driven codegen list
   (D-29 / D-30 / D-31). Keep the heading text if other pages link its anchor; check with the
   anchors check.
2. **Pointer stubs in the old homes:** 12/03 § D and § D.1, 12/05 § F's clip mask, and 08's web
   build. Each section keeps its heading, and its body becomes a one-line pointer to the block.
   The verbatim text now lives in the block. Do not leave banners above the old text.
   *(P07.2 correction: 12/03 § D's body and § Section planes' keyboard record are DualC's
   `dualc_field_view` record, and 08 has no Web-build section, only a clause of a DONE
   paragraph. Those three keep their record and gain a pointer sentence for the moved part;
   12/03 § Parameter push, 12/05's clip mask and Track 2 become pointer bodies. See
   [22 § How the block was opened](../docs/roadmap/22-dualceditor-handoff/README.md#how-the-block-was-opened).)*
3. **Decision rows in the index.**
   - D-17: DONE, by DualCEditor.
   - D-16 and D-25: owned by DualCEditor (D-25 DONE there).
   - Each cites the new decision row and the block.
   - The status vocabulary is the legend's; check the `status-vocab` check.
4. **Indexes.** Add the new block to the § Principal blocks table and to `docs/README.md` if it
   lists blocks. Update the 12 block row's status cell. The memory of the project is the repo:
   nothing goes in the chat only.
5. **Loss audit before committing:** `python3 scripts/docs_loss_audit.py` over `docs/`, base =
   this phase's start commit. Triage every row as moved (verbatim in the block) or kept. There
   must be 0 untriaged. Keep the triage in the handoff.
6. Run `--docs --strict`.

#### Acceptance criteria

- No Track 2 UI item is left with DualC as its owner. The old homes point to the block, the
  loss audit is clean, and `--docs --strict` is green.
- Committed, not pushed.

## Exit criteria

- The roadmap acknowledges DualCEditor as the home of the standalone UI. Nothing is pushed.
