# DualC docs — what lives where

One folder per class of fact. Each folder's README indexes every file below it; start there.

| Folder | Holds | Genre |
| --- | --- | --- |
| [`design/`](design/README.md) | **How it is**: pipeline, algorithms, invariants, conventions, glossary. Rewritten freely, dated nowhere. | compiled, mutable |
| [`roadmap/`](roadmap/README.md) | **How it got here**: one numbered block per topic, tracked items, evidence, rejected approaches. Entries are appended with a date, never rewritten. | append-only record |
| [`command_reference/`](command_reference/README.md) | **How to use it**: one page per CLI tool — flag tables with real defaults, recipes, exact messages. | usage contract |
| [`decisions/`](decisions/README.md) | **What was decided**: ID, decision, status with trigger, date, where recorded — the open and dropped ones on the README, the settled ones in `01-settled.md`. | index |
| [`raw/`](raw/README.md) | **Immutable inputs**: chat imports, screenings, plans as drafted, benchmark dumps. Never edited; a correction is a new dated file. | exempt from every contract |

## One home per fact

| Fact class | Owner | Everywhere else |
| --- | --- | --- |
| a flag, a default, a synopsis, a recipe, an exact message | its `command_reference/` page | link |
| an algorithm, an invariant, a tolerance, a coordinate or unit convention | [`design/`](design/README.md) | link |
| rationale, a measurement, a verification, a rejected approach | the topic's `roadmap/` block, dated | link |
| a decision (DONE by decision, DROPPED, DEFERRED + trigger) | a `decisions/` row + the roadmap anchor it cites | link |
| the current status of anything | [`roadmap/README.md`](roadmap/README.md) § Current focus / § Next up | link |
| a benchmark log, a chat export, a screening | `raw/`, as a new dated file | link |

Restating a count, a version or a date outside its owner is the drift the gate hunts.

## Conventions

- **Numbers are IDs.** `NN-slug` files and folders, `#N` items, letter-ID recipes: assigned at
  birth, never renumbered, never reused. Headings are never rewritten (anchors are links).
- **A README at every level.** An index row per file; an index README is a rewritable snapshot,
  a topic file is a record that only takes dated appends.
- **Footer.** Every non-index page under `design/`, `roadmap/` and `command_reference/` ends
  with `← Back to …`.
- **Size cap.** `scripts/check_data.json` `size_caps` (lines, words, bytes; whichever trips
  first) applies to every page here except under `raw/` (`frozen_prefixes`). A page
  that trips it is split into a same-numbered folder (`NN-slug/README.md` + children) — never
  frozen, never appended past the cap. Files already over when the gate landed sit in
  `size_baseline`, a ceiling that only shrinks.
- **Status words** are the legend of [`roadmap/README.md`](roadmap/README.md) and nothing else;
  never in a heading. The full contract, check by check:
  [roadmap 17/09](roadmap/17-code-audit-and-hardening/09-local-checks-gate/README.md).
- **Nothing here is frozen.** The `frozen` list is empty; the record of that decision is
  [roadmap 17/10](roadmap/17-code-audit-and-hardening/10-docs-system-screening.md).
- **Debt is written, never guessed.** What git cannot re-derive is marked
  `**Reconstructed (<date>) from commit <hash>**` or `DRIFT-PENDING: <owed>` (`--strict` fails on it).

## How to find things

Index + `rg`; there is deliberately no search index to keep in sync.

```bash
rg -n '#34' docs/                                 # a tracked item, wherever it is cited
rg -n -- '--tile-depth' docs/command_reference    # a flag, on every page that documents it
rg -n 'D-24|#25' docs/decisions docs/roadmap       # a decision by its ID, index row then record
rg -n 'DRIFT-PENDING|Reconstructed' docs          # documentation debt the record admits to
rg -n 'MINOR-OPEN' docs/roadmap                   # code debt below the tracking bar, on its owner's page
rg -n 'boundsFallback' src include docs           # a symbol, code first
```

The layered layout is the work of [roadmap 19](roadmap/19-docs-layers/README.md).
