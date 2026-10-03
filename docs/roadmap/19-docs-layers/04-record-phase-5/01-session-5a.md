# Phase 5, session 5a — the structural moves

Part of the [Phase 5 record](README.md) (roadmap 19): plan items 1–4.

**Session 5a — DONE (2026-09-18).** The four moves, each a verbatim relocation with a
dated pointer where the text used to be; no entry was rewritten, and no heading text
changed, so every inbound anchor survived with only its path retargeted. What landed and
where it deviates from the plan:

- **Item 1 — the continuations folded.** `16` § H is
  [12/07](../../12-field-graph-and-app/07-mesh-preview-sweep.md) (H1–H4, the verification,
  the 2026-09-01 correction and the three deferred follow-ups, byte-for-byte; the
  file-relative links re-based) and `18`'s two entries are
  [14/04](../../14-c-abi/04-abi-0-4-0.md). `16` and `18` are nine-line tombstones — "merged
  into … on 2026-09-18; number retired" plus the footer the `footer` check demands — and
  keep their rows in the roadmap README, now in numeric order (16, 17, 18, 19). Seventeen
  files carried links into the two: all retargeted, including four under
  `command_reference/` (`03`, `10`, `12/README`, `12/02`) — and two more under `11/05`
  for the report move below. That is a mechanical consequence of a move on this side, not
  the rationale migration plan § 7 decision 6 reserves for Phase 6 — the same call
  Phase 3 made for `ARCHITECTURE.md`'s inbound links ([02](../02-record-phase-3.md)) — and
  the only alternative was a broken gate. **The deferred follow-ups stay bullets**
  (S1 § 6 and [03](../03-record-phase-4.md)'s hazard (a) both weighed): promoting them to
  headings whose body opens `**DEFERRED**` would make them gate-mandated entries and
  force one sub-heading per row; as bullets, `D-29`–`D-32` needed only their path
  retargeted and the `(file, anchor)` collapse stays harmless.
- **Item 2 — one status ledger for #22–#35.** The table in
  [17 § Tracked items](../../17-code-audit-and-hardening/README.md#tracked-items--status-at-a-glance)
  is the ledger; its first body line now opens `**PARTIAL**` so the roadmap README's
  row 17 can link the heading and `status-sync` reads it (15 anchored rows checked, was
  14). The README's Track 1 item 1 is one clause + that link; its rows 13, 14 and the
  Current-focus paragraph lose their restated numbers (the parity count, the ABI
  versions, "87 findings, 14 tracked items"); `12/README`'s cells lose "28/28",
  "62/62", "11 flat entry points" and the § G measurements; `11/README` row 5 loses
  "~49×"; `15`'s retained-items row cites `capi/README.md` for the count as its own
  boundary rule already said; `05/02`'s "currently 66/66" and "update it" become a link.
  `17/08` § Still open in #34 is a link to the ledger entry. **The one home of the test
  and parity counts** is a new *Latest verification* line under the ledger heading:
  `ctest` 256, parity 73/73 — the plan's "latest verification line" existed only as the
  last `Update` paragraph's closing sentence, which is what the line replaces.
- **Item 3 — under the cap, with headroom.** `roadmap/README.md` 16,091 → 14,630 bytes
  (a rewritten snapshot: tombstone rows, the ledger clause, restated numbers out, every
  DEFERRED item cited to its decisions row, six milestone rows). `17/04` 18,734 →
  13,975 and `17/05` 17,087 → 13,099, both by the plan's mechanism — full records leave
  the ledger page for a child — but with two more moves than the plan priced, because
  its arithmetic (18.7 KB − #34's 50 lines ≈ 15.0 KB) left no headroom at all:
  `17/04` #34's four landed bullets are one line + link each (their text is verbatim in
  `07`/`08`), the `FieldPtr` paragraph, which had no other home, is
  [08 § The earlier batch](../../17-code-audit-and-hardening/08-argument-validation.md#the-earlier-batch-fieldptr-const-ness),
  and the delivery records of #28, #30 and #33 are
  [17/12](../../17-code-audit-and-hardening/12-engineering-quality-records.md) (one records
  page for the small deliveries, mirroring `05`/`07`/`08` for the large ones). `17/05`'s
  re-scoping table, verification digests and untested-branch note are
  [17/11](../../17-code-audit-and-hardening/11-diagnostics-verification.md), each stub
  keeping the claim in one sentence; the plan's "the re-scoping table too" was followed
  for the headroom, `05` keeping the argument. `17/05`'s intro also lost the
  "plan for this session" / "doc_conventions §6" / frozen-`11-dualc_field.md` paragraph
  (S1 § 6 chat residue and the file's one `stale_paths_baseline` entry, both gone) —
  item 9's first piece, taken because the page was open. The four `size_baseline`
  entries and the `17/05` `stale_paths_baseline` entry are deleted; **`size_baseline` is
  empty**, which the plan's size end-state asked of Phase 7.
- **Item 4 — the quality report** is
  [12/08](../../12-field-graph-and-app/08-quality-inspection-gyroid-shell/README.md): the
  README (test case, executive summary, root-cause diagnosis, recommended workflow,
  verification, methods — 205 lines, over the plan's ≈ 120 because the diagnosis and the
  methods have no better page), `01-measurements.md` (Steps 1–4b, the ≈2× wall rule
  and the workflow-optimization section, so `command_reference/11/05`'s anchor
  `#the-feasible-win--qem-mesh-decimation` lands there) and
  `02-decimation-approaches.md` (the proposed enhancement and Approach A vs B). The
  banner is one status sentence pointing at § G and row `D-26`; the two stale prose
  paths (`11-dualc_field.md`, `11-dense-lattice-deliverable.md`) are links; the
  `05 #20` and `roadmap 11` citations too; six inbound references repointed
  (`12/README` page table and § G cell, `12/06`, `11/05` ×2, `STRUCTURE.md`); the
  folder joined `index_roots`.
- **Also on the way.** The roadmap README's legend keeps **NEXT** and uses it once
  (the rest of #34) — item 10's first half, decided here so the rewritten snapshot did
  not have to be rewritten again; `12/README`'s `VALIDATED` and `pending` cells are
  legend words; `12/README`'s "ranked fix menu in § D" points at `04` (S1 § 2's
  misdirected anchor); the `roadmap/10:13` directional site ("This section" → "Block
  10") is fixed, so `--strict` fails on the three `command_reference/` sites only,
  which are Phase 6's.
- **Not done, and why.** Items 5–11 are session 5b's (the plan's own split). The
  `#N` / "Tier-1 item" collisions, the 16 factual conflicts, the `Reconstructed` /
  `DRIFT-PENDING` markers, the § anchors on the folder-README page tables, the bold
  pseudo-headings of `12/03`, `12/04` and `01/README` (S1 § 3 — `12/07`'s "Unchanged
  from 12 § D.2" link lands on `04`'s file, not its pseudo-heading, for that reason) and
  the full 14-question navigation table wait for it. `docs/study/` and `docs/raw/` are
  untouched (immutable; Phase 7 retargets their citations of the moved files).

*Verified (5a):* `python scripts/check.py --fast` — 26 checks, 24 OK, 0 failed, 0 SKIP,
2 INFO (`index-staleness` 8 files; `directional` 3 sites, all in `command_reference/`);
1,186 links, 0 broken (was 1,093); 327 cross-file + 19 same-file anchors, 0 unresolved;
`indexes` 16 roots (was 15), 107 siblings, 0 unlinked; `footer` 85 pages (six new
children, two tombstones, this record); `sizes` 101 files, **0 baselined, 0 over** (was 4
baselined); `status-sync` 15 anchored rows, 0 disagree; `decisions-index` 10 entries,
27 rows, 0 problems; `stale-paths` 9 grandfathered (was 10); `heading-hierarchy` 15
grandfathered, 0 new. `--docs --strict` fails on `directional` only — the three
`command_reference/` sites. Nothing under `src/`, `include/`, `tests/`, `examples/`,
`capi/` or `scripts/check.py` changed (`git diff --stat`), and `ctest -N` on the existing
build is **256**. Navigation questions the moves changed (S1 § 5; the full table is 5b's):
Q4 "what remains in #34?" — README → `17 § Tracked items` → `04 #34`, one list (2 hops,
was three differing copies); Q6 "ABI version / entry-point count?" — README → `capi/README.md`
(1 hop, `15` no longer contradicts; `10:35` still does, item 5); Q7 "what does `check.py`
enforce?" — README milestones → `17/09` (1 hop, was "via CLAUDE only"); Q13 "latest
ctest/parity?" — README → `17 § Latest verification` (1 hop, one home).

---

← Back to the [phase record](README.md) · the [Docs layers index](../README.md).
