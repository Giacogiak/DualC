# Phase 5, session 5c — the evidence pass

Part of the [Phase 5 record](README.md) (roadmap 19): the two S1 findings the phase had
closed without addressing — "DONE without evidence" (S1 § 3) and "status tables doing
record duty" (S1 § 6) — taken as a short third session so the phase answers the whole
worklist rather than the plan's item list alone.

**Session 5c — DONE (2026-09-20).** Every undated DONE now names its commit, re-derived
from git the way 5b's conflicts were; no `Reconstructed` marker was needed because none
of the values had to be guessed.

- **S1 § 3 "DONE without evidence".** `01/README`'s six Tier items carry their date and
  commit on the status line — adaptive collapse `4d72221` (2026-04-30), the sharp-feature
  toggle `5c16aea` (2026-05-15, `interpolateNormals` first in `sampler.h`; the
  2026-04-30 `ARCHITECTURE.md` had only planned it), multi-threading `00abec4`
  (2026-05-16, `parallel.h` added; marked done `bbae479`), manifold DC `6f6d48b`
  (2026-05-21), the closest-point query `669652c` and GWN `084a19c` + `f06d60f`
  (2026-05-22) — with one sentence on the page saying the dates were reconstructed. `02`'s
  phase table gets a dated line with the six commits (all 2026-05-15) and the Catch2
  counts at each tree (31 → 65 → 72 → 79, then 79 + 5 smoke tests), which is where `03`'s
  "31 tests" and `02`'s "84 tests" come from. `10 #8` Phase 1 is `aab403b`. `12/README`'s
  D.1 cell names `f19baa0`. `05/03`'s closing names `aa0d577`. `12/03`'s "`uClipMask==0` is
  bit-identical" — the one claim git cannot settle — now says what it rests on: the shader
  structure, not a snapshot diff. `12/README` row A's "pending" was 5a.
- **S1 § 6 "status tables doing record duty".** `12/README`'s five long cells (B, D.1,
  D.2, the C ABI row, G) are status + date + link; the sentences they carried are on the
  pages they now link (`§ B`, `§ D › Section planes`, `§ D.2`, `14`, `§ G`). The README is
  9.5 KB (was 11.6 at Phase 4's close).
- **Not touched.** `05/03 § Closing`'s "README index row refreshed" stays as the record
  of what that session did; `12/README`'s row B still lists what landed on 2026-06-24 in
  one clause, because those four sub-deliveries have no headings of their own on `§ B`
  (making them headings is a Phase 8 judgement, not a link fix).

*Verified (5c):* `python scripts/check.py --fast` — 26 checks, 24 OK, 0 failed, 2 INFO
(`index-staleness`; `directional` the same three `command_reference/` sites); 0 broken
links, 0 unresolved anchors; `sizes` 0 baselined, 0 over; `status-sync` 15 rows, 0
disagree. Nothing under `src/`, `include/`, `tests/`, `examples/`, `capi/` or
`scripts/check.py` changed; `ctest -N` **256**.

---

← Back to the [phase record](README.md) · the [Docs layers index](../README.md).
