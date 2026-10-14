# Handoff — P01.1: open roadmap block 21, its tracked items and the owner's decisions

Unit P01.1 of [`plan/10_P01_record-and-demo-meshes.md`](../10_P01_record-and-demo-meshes.md)
§ T1; session of 2026-10-14, branch `feat/cli-consolidation` (pushed to `origin`; `main`
untouched). Docs and gate configuration only — no source, test or CMake file changed. The
record is [roadmap 21](../../docs/roadmap/21-cli-consolidation/README.md); this handoff points
at it and adds what the next unit and Giacomo need.

## What changed, and why

Commits: `f0f464e` (block 21, the decisions, the snapshot, `check_data.json`), then this
handoff with the loss-audit line added to 21/01's cap-relief note.

- **Block 21, a folder from the start:** `docs/roadmap/21-cli-consolidation/README.md` (index +
  the one status table) and `01-record.md` (the record child). One tracked item per
  precondition, each opening with its acceptance line from the plan's phase:

  | ID | Item | Status | Phase |
  | --- | --- | --- | --- |
  | #55 | Generated demo meshes wound outward (F1) | NEXT | P01 T2 |
  | #56 | The mesh node takes `sign="gwn"` | PLANNED | P02 T1 |
  | #57 | Per-node bake on the mesh nodes | PLANNED | P02 T2 |
  | #58 | 2D profiles and `revolve` / `extrude` in the graph | PLANNED | P03 |
  | #59 | Slice any graph | PLANNED | P04 |
  | #60 | Retire the eight tools | PLANNED | P05, P06 |

- **Decision rows**, by the index's own style (settled = DONE by choice in `01-settled.md`;
  open = the README):

  | ID | Table | Decision |
  | --- | --- | --- |
  | D-55 | settled | one framework; `dualc_csg_demo`, `dualc_primitive`, `dualc_raymarch` retire as-is, `dualc_lattice` with its defaults as a recipe (#60); tombstones, numbers never reused |
  | D-56 | settled | `dualc_demo` retires after `mesh(sign="gwn")` (#56); **F2 not fixed** (folded into this row, see *Deviations*) |
  | D-57 | settled | `dualc_lift` moves into the graph (#58), fires D-11 for the 2D + lift sub-grammar only |
  | D-58 | settled | `dualc_slice` moves into the graph (#59) as `dualc_field <graph> --slice AXIS=VALUE` — **form awaiting your confirmation** |
  | D-59 | settled | `dualc_gen_demo`, `dualc_glsl_parity`, `dualc_io_stress` stay |
  | D-60 | open, **DEFERRED** | `dualc_boolean`'s retirement — trigger: P02 T2's measurements, at the approve gate after P02; your words of 2026-10-14 quoted in 21/01 § The dualc_boolean decision |
  | D-11 | open (edited) | status cell: "fired 2026-10-14 for 2D + lifts only (D-57)"; `GridField` and the lambda `displaced` stay DEFERRED |

- **The dated note** that 12/01's "`MeshSource` has no GWN ctor" no longer holds sits in 21/01
  under #56 (it also covers the same claim in 12's README, line ~100). Verified at `d6cc19d`:
  `MeshSource(mesh, geometry, interpolateNormals, SignMethod)` in `include/dualc/implicit.h`;
  `src/implicit/mesh_source.cpp` builds the GWN oracle from `GENERALIZED_WINDING_NUMBER`;
  `sampleMeshToHermiteOctree` (`src/sampler.cpp`) constructs it from `SamplerParams`. 12/01 and
  12/README are not rewritten.
- **`docs/roadmap/README.md`:** § Current focus rewritten to name block 21 with **NEXT #55**;
  Track 1 gets block 21 as item 1 (the others renumbered — list positions, not IDs); the
  engine-hardening item's "NEXT: the rest of #34" became "After block 21: …" (one NEXT); the
  Principal-blocks row 21; one clause in *How the blocks relate*.
- **`scripts/check_data.json`:** block 21 joins `index_roots` (the README must index every
  child), `status_tables` (so `status-sync` checks its cells against the item headings) and
  `ordered_item_files` (`## #NN` order). `docs/raw/README.md` already indexed the plan and the
  screening — checked, unchanged.

## Measured, and under what

- Byte cap (`size_caps` 15,360 content bytes) at the start: `roadmap/README.md` **15,355**,
  `decisions/README.md` **15,322**. After: **15,182** and **15,317**. Block 21: README 2,211 bytes,
  01-record 13,505 bytes after the cap-relief note (under the cap; see *Next unit*).
- `python3 scripts/check.py --docs --strict`: 28 checks, 27 OK + 1 INFO (`index-staleness`,
  17 pre-existing entries, none of them block 21's), PASS. `status-sync` 25 anchored rows
  checked (block 21's eight among them), `decisions-index` 12 entries / 40 rows, `ordering` 21
  ledger items.
- Full `python3 scripts/check.py` at `f0f464e` (Linux, GCC 15, Ninja, Release, `build/` already
  configured so `configure` SKIPs): 32 checks, 30 OK, 1 SKIP, 0 failed — build 36.5 s, 0
  DualC-origin warnings, **ctest 306 / 0 failed** (94.6 s), PASS.
- `docs_loss_audit.py --base d6cc19d --head f0f464e --scope docs/ --annex --triage <scratch>`
  (~15 min on this host): 18 residual rows on the two trimmed pages, each triaged by hand —
  the removed clauses (homes in 21/01's table), list lines renumbered or moved, the reworded
  decisions intro. The tool also lists ~9,000 untriaged rows in files the commit did not touch
  (an empty scratch triage scores every base unit); those are not losses.

## What I got wrong on the way

- The first `--docs --strict` run counted nothing new: the checks walk `git ls-files`, so the
  two new pages were invisible until `git add`. Stage before trusting a docs-tier count.
- The loss audit took three runs: a narrow `--scope` is refused (every base doc file must be
  scoped or an R100 rename, so `--scope docs/`), and a scratch triage must carry an `audits`
  entry keyed by the base's short hash (`base`, the script's `thresholds`, `items`, `rules`) —
  the second run spent 15 min before failing on that. Build the triage file first.

## Deviations from the plan file (the plan file is unchanged)

- **F2 has no row of its own.** The decisions index was 38 bytes under the cap; a DROPPED row
  would have had to live there (the `decisions-index` check reads only the README). F2 is
  stated in D-56's settled row instead, which is how the raw plan § 1 itself words it (inside
  the `dualc_demo` row). If you want a separate DROPPED row, it needs the index relief below.
- **Cap relief on two index pages**, not in the plan: restated clauses removed where the
  linked home already holds them, each listed in
  [21/01 § Opening the record at the cap](../../docs/roadmap/21-cli-consolidation/01-record.md#opening-the-record-at-the-cap)
  with its home. One false fact went with them: the snapshot still said "`main` unpushed"
  (false since 2026-10-07).

## Open — for Giacomo

1. **Confirm or override the slice form** (D-58): the plan takes
   `dualc_field <graph> --slice AXIS=VALUE`, keeping the user surface at the two framework CLIs.
   P04 builds whatever you confirm; an override gets a new decisions row.
2. **`docs/decisions/README.md` has 43 bytes of room.** D-60's status cell will grow when you
   decide it after P02, and P07 finalises rows. It needs a relief of its own — options: move
   the landed tracked-item rows (#31, #33 DONE) and future landed rows to a child page (a
   change to the "keeps its row" rule and to `decisions-index`'s config, which reads only the
   README), split the DROPPED rows into a child the check also reads, or tighten older rows'
   wording (lossy-looking, slow). Your call; nothing in P01 T2 needs it.

## What the next unit (P01.2, T2 — F1) needs to know

- Its record is **#55** in `docs/roadmap/21-cli-consolidation/01-record.md`; on landing, flip
  the heading's first body line to **DONE** *and* the #55 cell in 21/README (status-sync reads
  both), record before/after numbers there, and move NEXT to #56 in both 21/README and
  `roadmap/README.md` (§ Current focus, Track 1 item 1, Principal-blocks row).
- `roadmap/README.md` has 178 bytes of room: keep edits to it word-for-word swaps.
- 01-record is 13,505 of 15,360 bytes: #55's before/after numbers will likely push it over —
  move #55 (or the decisions sections) to a numbered child `02-…md` rather than squeezing
  (principle 5), updating the 21/README table links and the D-row anchors that point at
  `01-record.md#…` (D-55…D-60 cite `#the-owners-decisions` / `#the-dualc_boolean-decision`).
- Every new `#N` / D-NN: next free are **#61** and **D-61**.

---

← Back to the [plan](../README.md).
