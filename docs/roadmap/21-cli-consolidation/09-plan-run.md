# CLI consolidation — the plan's run, harvested

Part of [CLI consolidation](README.md) (block 21). The
[consolidation plan](../../raw/2026-10-14-cli-consolidation-plan.md) was run by the owner's
`plan-run` driver as eleven unattended units, 2026-10-14 … 2026-10-16, each a fresh session on
branch `feat/cli-consolidation` closing with a handoff under `plan/handoffs/`. Each item's
record already holds its own facts ([01](01-record.md) to [08](08-retire-tools.md)); this page
is the harvest of what the handoffs held *across* units — how the run went, what the sessions
corrected in the plan files, what it taught, what it left open — after which `plan/` is
deleted ([D-51](../../decisions/01-settled.md)), as [20/04](../20-public-delivery/04-hosted-ci-plan-run.md) did for the last run.

## The consolidation plan's run — P01 to P07, harvested

**DONE — 2026-10-16.** Block 21 closed: #55–#60 DONE, D-55 … D-64 decided; the branch waits for
the owner's PR (`main` is protected, PR only).

| Unit | Item | Record | Green run on the unit's code or docs commit |
| --- | --- | --- | --- |
| P01.1 | block 21 opened, D-55 … D-60 | [01](01-record.md) | — (docs only; full gate local, ctest 306) |
| P01.2 | #55 | [02](02-demo-mesh-winding.md) | [38621264223](https://github.com/Giacogiak/DualC/actions/runs/38621264223) (`75bfd27`) |
| P02.1 | #56 | [03](03-mesh-sign-gwn.md) | red [38767086631](https://github.com/Giacogiak/DualC/actions/runs/38767086631): gitlab.com down for the `gpu` job's Eigen clone, not the change ([20/03](../20-public-delivery/03-hosted-ci.md)); the handoff's run re-triggered it |
| P02.2 | #57, D-62 | [04](04-mesh-node-bake.md) | [38780112757](https://github.com/Giacogiak/DualC/actions/runs/38780112757) (`a906934`), [38782978749](https://github.com/Giacogiak/DualC/actions/runs/38782978749) (`284920c`); the resumed code `1b126db` by the driver |
| P03.1 | #58 CPU | [05](05-2d-profiles-and-lifts.md) | [38800024751](https://github.com/Giacogiak/DualC/actions/runs/38800024751) (`b5e22b1`) |
| P03.2 | #58 GPU | [05](05-2d-profiles-and-lifts.md) | [38806451193](https://github.com/Giacogiak/DualC/actions/runs/38806451193) (`9405702`) |
| P04.1 | #59 | [06](06-slice-any-graph.md) | [38859390269](https://github.com/Giacogiak/DualC/actions/runs/38859390269) (`d201539`) |
| P05.1 | #60, docs | [07](07-docs-migration.md) | [38866229941](https://github.com/Giacogiak/DualC/actions/runs/38866229941) (`f0ea075`) |
| P06.1 | #60, code | [08](08-retire-tools.md) | [38871382436](https://github.com/Giacogiak/DualC/actions/runs/38871382436) (`ac1e7e8`) |
| P06.2 | #60, tombstones | [08](08-retire-tools.md) | [38877047030](https://github.com/Giacogiak/DualC/actions/runs/38877047030) (`fc98e5a`) |
| P07.1 | the close | this page | the driver's (a page cannot quote the run of its own commit) |

**The counts the run moved.** ctest 306 → 307 (#55) → 308 (#56) → 315 (#57) → 325 (#58) →
331 (#59) → 358 (P05's 27 twins) → **344** (P06: 39 retired cases out, 25 twins in). GPU parity
73 → 74 (`mesh-gwn`) → 76 (`mesh-bake`, `mesh-bake-smooth`) → **82** (six lift cases). The ABI
stayed **`dualc 0.5.1`**: each unit widened the graph vocabulary, no signature, struct or code
moved — the rule [14/04](../14-c-abi/04-abi-0-4-0.md) applies.

**Measurements only the handoffs held**, carried here. Parity: `mesh-gwn` 0 / 3,969 sign
errors (216 samples inside), as `mesh-straddle`; `mesh-bake` max error 2.72e-4 and
`mesh-bake-smooth` 5.70e-4, each 0 / 4,096 over tolerance. `dualc_field_view` on one baked
(`bake=64,band=1`) and one unbaked mesh reports `2 mesh texture(s) @ 64^3 (cell 0.0831746),
96^3 (cell 0.0676211)`. 11/09's R3 and R4 at depth 7: 135,308 and 172,060 faces;
`onion(torus,thickness=0.35)` at depth 6: 38,960.

**How the run went.**

- **Push rule** (the owner's, 2026-10-06, kept 2026-10-14): push the branch as often as a CI
  run is needed; never `main`, never force. The owner opens and merges the PR.
- **Three owner reviews answered blocked units**: after P01 (2026-10-15) — the DROPPED rows
  split out (D-61), D-58's `--slice` form confirmed; after P02 (2026-10-15) — `dualc_boolean`
  kept (D-60), the bake margin handed down (D-62, the broader margins accepted at the P02.2
  human check); at P05 (2026-10-16) — `displace` keeps frequency 6 (D-63), `--slice` refuses
  the contour-only flags (D-64).
- **The GPU tier** ran wherever a field node or the codegen moved (P02.1, P02.2, P03.1, P03.2,
  P06.1), always after rebuilding `build-gl`; each new parity case was first shown to fail with
  its branch disabled (04, 05).
- **The byte cap shaped the record**: 01 sat within 120 bytes of 15,360 from P03 on, so every
  item's numbers went to a numbered child (02–08) and the snapshot took word-for-word swaps.

**The plan files' corrections** (the raw plan is never edited):

- *P01.1* — F2 has no DROPPED row: the index had 38 bytes, so it is stated in D-56's row; two
  index pages were relieved first ([01 § Opening the record at the cap](01-record.md#opening-the-record-at-the-cap)).
- *P01.2* — the bracket was **mixed** (caps out, walls in), not inside-out: only its walls were
  flipped; reversing it whole, as the plan said, would have inverted it the other way ([02](02-demo-mesh-winding.md)).
- *P02.2* — T2 step 1's region (the node's box + 10 %) is wrong under any smooth op whose `k`
  exceeds the pad: corrected in the plan file, then D-62. `band` on `winding` is refused as an
  unread key, not "ignored"; page 03 recommends no depth or bake for `opA` / `opB`.
- *P02 gate* — P05 T1 step 3 and P06's header say *kept (D-60)*: seven tools retire, not eight.
- *P03.1* — T1 step 7's "an 11/01 section" became page 11/09 (11/01 had no room).
- *P04.1* — all eight mesh flags refused with `--slice`, not the plan's five (D-64).
- *P05.1* — recipes reborn under new letters where tool 11 already used the old (G, K, X6–X8).
- *P06.1* — T1's "`--clean` proves a clean configure" is false: it reconfigures in place; the
  proof was a fresh build directory. P07's step 5 inherits the same caveat (below).

**What the run taught.** Each cost a unit time once:

- *Stage before you count.* The docs checks walk `git ls-files`: a new page is invisible until
  `git add`.
- *The loss audit is tree-wide.* `--scope` must hold every changed base docs file (so
  `docs/`); a scratch triage needs an `audits` entry keyed by the base's short hash; about
  9,000 rows of unchanged files come back each time, so filter to the changed pages. A scratch
  commit (`git commit-tree` on a separate index) audits the working tree without a branch; one
  run with `--write-dump` beats a loop per page; `pkill -f docs_loss_audit.py` kills the
  session's own shell.
- *A shorthand graph's error pointer is a character offset*, not a JSON pointer: compare the
  GPU path's pointer and message with `FieldGraph::build`'s for the same graph.
- *CMake reads a `;` in `PASS_REGULAR_EXPRESSION` as a list separator* (two alternatives,
  either passes): write `.`. `foreach(… IN ZIP_LISTS …)` needs CMake 3.17; DualC supports 3.14.
- *A `POST_BUILD` copy goes stale*: the demo meshes next to the CLIs kept the old winding until
  they became `LINK_DEPENDS` (#55).
- *The hierarchical GWN is approximate*: 1.066 at a sphere's centre, 0.5025 where 0.5 is
  exact — margins of 0.05–0.1, not 1e-3.
- *`remote_run_check.py` straight after a push* prints "no run for HEAD"; a later push cancels
  the earlier run.
- *Run the recipe before writing it*: page 03's B8 carved nothing (the radius-1 sphere swallows
  the cube); `--normalize-thickness` is `onion(normalize(…))`, not the reverse.

**Open after the run.** None blocks the PR; each has its home.

| What | State | Home |
| --- | --- | --- |
| The keyboard map's heading still names `dualc_raymarch` | headings are never rewritten; renaming costs one anchor (design/09 links it) — the owner's call | [08](08-retire-tools.md) |
| D-14 dropped by a unit, not by the owner | move it back with a reworded trigger if wanted | [02-dropped](../../decisions/02-dropped.md) |
| The twins hold floors, not exact counts | exact counts on Linux only, or stored reference OBJs | [08](08-retire-tools.md) |
| `test_field_graph.cpp`'s SECTION "defaults are dualc_lift's" | a passing test, left; rename at will | [08](08-retire-tools.md) |
| M4's 1-unit wall on the 0.15-unit bunny | kept with a note pointing at 0.002 | [07](07-docs-migration.md) |
| `vec2` is a new binding type | DualCEditor's `src/trace_renderer.cpp` uploads only `float` / `vec3` / `mat4`: on re-pin a profile's 2D points read zero — one `glUniform2f` line there | [05](05-2d-profiles-and-lifts.md), DualCEditor |
| Polygon ceiling 64 in the preview | a judgment, not a measurement | [05](05-2d-profiles-and-lifts.md) |
| A nested array off `points` is read flat (`center=[[0,0,0]]` = `[0,0,0]`) | lenient, never a different number order | this page |
| A bake above 256 on the GPU | untested; GL 3.3 guarantees 3D textures of 256 only | this page |
| `mesh-gwn` cannot tell GWN from parity | the harness reads only the closed `cube.obj`; an open mesh would | [03](03-mesh-sign-gwn.md) |
| `include/dualc/types.h` and `src/internal/sign_oracle.cpp` call parity correct for "closed, oriented" meshes | orientation does not matter (#55); design/02 states it; a comment edit in `src/` + `include/`, left out of the close | this page, [19/08 run 07](../19-docs-layers/08-semantic-lint/07-2026-10-16-after-cli-consolidation.md) |
| `--slice` writes its PNG / SVG in place, without the `.part` rename | documented (design/09, design/10); routing it through `AtomicOutput` is a code change | [19/08 run 07](../19-docs-layers/08-semantic-lint/07-2026-10-16-after-cli-consolidation.md) |
| D-40's trigger met (`docs/raw/` over 1 MB, this plan's three files) | build the FTS5 index or re-defer — the owner's call | [D-40](../../decisions/README.md) |
| D-33's trigger met by #57's clamp reads | answered by D-62 (no read past the region); counting not taken — the owner's call | [D-33](../../decisions/README.md) |
| CI's `gpu` job clones Eigen from gitlab.com cold | give it the `_deps` cache, or a system Eigen | [20/03](../20-public-delivery/03-hosted-ci.md) |
| Dark silhouette specks in the viewer | the sphere-trace's, not the lifts' | [#61](../17-code-audit-and-hardening/17-viewer-silhouette-specks.md) |
| 12/05's hotkey replacements in a live window | judged on `--snapshot` frames only (P05.1); one interactive look owed | this page |

*2026-10-16, the close (P07.1).* What the unit ran before handing over:

- **`/docs-semantic-lint`**, a delta run over the branch:
  [19/08 run 07](../19-docs-layers/08-semantic-lint/07-2026-10-16-after-cli-consolidation.md),
  8 findings. Five are fixed: a new [design/12](../../design/12-field-graph.md) holds the
  graph's own rules (bake margin, 2D typing, the preview's exceptions), and design/09 and 10
  scope `-o` and `.part` to mesh exports. One is handed to the table above (the two
  "oriented" comments), and two are met triggers (D-40, D-33), the owner's.
- **The loss audit of `plan/`**: `scripts/docs_loss_audit.py --base f5dcf0f --scope docs/
  plan/ --annex plan/`. The head was a scratch commit: the final docs with `plan/` removed,
  made by `git commit-tree`, never a branch. The triage was a scratch file with no rules, so
  `scripts/docs_loss_audit.json` is unchanged. Seven minutes a run.
  - *The annex:* 742 of 759 sentences, 435 of 733 evidence units, all 119 headings and 112
    of 1,392 identifiers came back residual.
  - *Sentences:* expected, the handoffs were paraphrased, not copied.
  - *Evidence:* page byte counts on a given day, gate timings, commit hashes (git keeps
    them), and run IDs and records quoted here in other words (token-only). The handoff-only
    measurements were found this way and are carried above.
  - *Identifiers:* commit hashes, `/tmp` scratch paths and snapshot names, the full SHA-256
    values (02–06 keep their ends), and the compare link, which belongs to the PR.
  - *The `docs/` side:* 6 residual rows on the changed pages, all intentional. Three are
    the snapshot's Current-focus and Track 1 lines and the #56 milestone row (out of the
    window, D-43, record [03](03-mesh-sign-gwn.md)). Three are design/10's preview paragraph
    and padding row, rewritten for the lint under the cap.
  - *The rest:* about 11,100 rows of unchanged files, the noise P05 and P06 met. The dump is
    not kept, as in 20/04.
- **The gates.** `check.py --clean` reconfigured in place and the warnings scan reported
  SKIP, as P06.1 found. So `cmake --build build --target clean` and then `check.py --build`
  were run: 251 TUs, 0 DualC-origin warnings, ctest 344, 0 failed. `cmake --build build-gl
  -j && check.py --gpu --strict --build-dir build-gl` gave parity 82/82. Then
  `check.py --docs --strict`. The remote run on the final commit is the driver's.
- **`plan/` is not deleted here.** The driver still reads the manifest and this unit's
  handoff. The deletion (D-51) is one commit after the driver marks P07 done.

*2026-10-16, after the run (the `/plan-run` hand-back), Giacomo's answers to the close.*
Two rows of the table above are done on the branch: the parity comments in
`include/dualc/types.h` and `src/internal/sign_oracle.cpp` now say *closed, either winding*
(and the GWN comments say it needs an outward winding), and `--slice` writes its PNG / SVG
through `writeBytesAtomic` (`examples/example_common.cpp`): encoded in memory, then `.part`,
non-inheritable handle, rename — the bytes are identical to before (`cmp` on the X-recipe
cube slice), and a bad directory now exits 2 with `[dualc] error: cannot open '….png.part'`.
D-40 and D-33 were re-deferred with new triggers (their rows); the other rows stay open.

---

← Back to [CLI consolidation](README.md) · the [Roadmap index](../README.md).
