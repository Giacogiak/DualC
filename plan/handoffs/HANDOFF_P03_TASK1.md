# Handoff — P03.1: 2D profiles and `revolve` / `extrude`, the CPU half (#58)

Unit P03.1 of [`plan/12_P03_2d-lifts.md`](../12_P03_2d-lifts.md) § T1; session of 2026-10-15,
branch `feat/cli-consolidation` (pushed to `origin`; `main` untouched). The record is
[roadmap 21 #58](../../docs/roadmap/21-cli-consolidation/01-record.md#58-2d-profiles-and-revolve--extrude-in-the-graph)
(now PARTIAL: CPU half done, GPU half NEXT) and its new evidence page
[21/05](../../docs/roadmap/21-cli-consolidation/05-2d-profiles-and-lifts.md); the usage
contract is the new [11/09](../../docs/command_reference/11-dualc_field/09-2d-profiles-and-lifts.md).

## What changed, and why

Commits: `b5e22b1` (code, tests, ctest, docs, plan correction), then this handoff.

- **Six graph nodes** (`examples/field_graph.{h,cpp}`), pure wiring over the unchanged
  library (`include/dualc/implicit2d.h`): profiles `circle`, `rect`, `segment`, `polygon`;
  lifts `revolve(profile, offset=1)` → `RevolveField` (around y) and
  `extrude(profile, height=0.5)` → `ExtrudeField` (± `height` along z — the **half**-height,
  as `dualc_lift --height`). Defaults are `dualc_lift`'s, so every lift recipe maps 1:1.
- **Token `rect`** for the 2D box (step 1's choice): `box` is the 3D primitive; `rect` is the
  sketcher's word, needs no suffix, collides with no registry token (`box2` was the named
  alternative; a digit reads as a version). Why recorded in 21/05 § Why `rect`.
- **One parser, a second builder entry point** (step 3): the readers are unchanged in shape;
  `buildField` dispatches the lifts (after the mesh sources, before the decorators) and
  refuses a bare profile; the lift calls a file-local `buildProfile(child, lift)` returning
  `Field2DPtr`, which refuses any non-profile child and runs the same `ReadScope` /
  `rejectUnreadParams` / `invalid_argument` → located error wrapping as `buildField`.
  Public in the header: `ProfileKind`, `profileKinds()`, `isProfileOp`, `isLiftOp`.
- **Typing errors**, both located: a profile anywhere but as a lift's one child →
  *'circle' is a 2D profile: it is legal only as the one child of 'revolve' or 'extrude'*
  (pointer = the profile); a 3D or unknown child under a lift → *'revolve' takes one 2D
  profile (circle, rect, segment, polygon), not 'sphere'* (pointer = the child). Arity via
  `requireChildren`; a profile with children is refused.
- **Parameters:** named (`center=[x,y]`, `radius`, `half=[x,y]`, `a`, `b`, `points`) and flat
  `params=[…]` / shorthand positionals; a short flat list overrides from the front (as on 3D
  primitives), a longer one is refused (as `dualc_lift` refuses it). `polygon` requires
  `points` or `params` (not both, ≥ 3 vertices, even count). Graph-side validation the tool
  never had: radii / half extents / `height` > 0, all values finite; `revolve` `offset` finite.
- **Nested arrays** for `points=[[x,y],…]` (step 1 asks for that form): `ParamValue` gained
  `rowSize` (0 = flat). JSON and shorthand readers accept one level of equal-length rows,
  flattened; `dumpJson` writes rows back, so the form round-trips as written. Ragged rows,
  empty rows and deeper nesting are parse errors. Every other reader sees the flat vector,
  so a nested value on another key reads as its flattening (`center=[[0,0,0]]` = `[0,0,0]`) —
  lenient but never a different number order; the GLSL TU needed no change for this.
- **`--list`** gained the "2D profiles / lifts" group (printed from `profileKinds()`).
- **ctest:** `cli_field_lift` (graph revolve at depth 5) + `_content` + `cli_field_lift_same`
  (`cmake -E compare_files` against `cli_lift`'s OBJ).
- **Tests** (`tests/test_field_graph.cpp`, `[lift]`, 4 cases): library equality per profile
  under both lifts (140 points + bounds, exact `==`); flat / positional / default forms; a
  lift under a boolean contours; every typing error with its pointer; 15 bad-value cases +
  ragged / nested arrays in both readers; round-trip with rows preserved. No existing test
  edited.
- **Docs:** 11/09 new (keys, typing, R1–R4, the "not in the viewer yet" note T2 removes);
  11/01 a short *2D profiles and lifts* section + the grammar's array-of-arrays line and
  bullet; 11/README page row, R1–R4 recipe row, Diagnostics; command_reference/README
  feature row; 04-dualc_lift pointer (`box` is `rect` in the graph); 14/04 entry + 14/README
  link (ABI: vocabulary widened, no version move — `dualc 0.5.1`); 21/01 #58 PARTIAL + a
  landing line; 21/README page list + row; `roadmap/README.md` § Current focus, Track 1 NEXT,
  principal-blocks row. No milestone row yet (#58 is not DONE). No `src/` or `docs/design/`
  edit → no `/docs-semantic-lint` obligation.

## Where the plan was wrong (corrected in the plan file)

Step 7 asked for "an 11/01 section". 11/01 was at 13,824 of the 15,360-byte cap — no room
for a keys table, typing errors and recipes. 11/01 got a ~400-byte section pointing at the new
page 11/09, which carries the content. Dated correction under step 7.

## Measured, and under what

Linux, GCC 15, Ninja, Release, `build/examples/`, default `--depth 7`, auto-fit root box; both
CLIs call `dce::writeField` with default sampler/contourer params. `cmp` on the OBJ.

| `dualc_lift` | Graph (`dualc_field --expr`) | Faces | `cmp` |
| --- | --- | ---: | --- |
| `revolve circle 0 0 0.5 --offset 2` | `revolve(circle(0,0,0.5),offset=2)`; also `circle(center=[0,0],radius=0.5)` | 153,888 | identical |
| `extrude polygon 0,0 1,0 1,1 0,1 --height 0.5` | `extrude(polygon(points=[[0,0],[1,0],[1,1],[0,1]]),height=0.5)`; also `polygon(0,0,1,0,1,1,0,1)` | 164,268 | identical |
| `extrude box 0 0 1 0.5 --height 0.75` | `extrude(rect(half=[1,0.5]),height=0.75)` | — | identical |
| `extrude segment` | `extrude(segment)` | — | identical |

SHA-256: revolve `3a0d846f0bbfe844a7db7032b0dbb810f50a34c272a0ed4c35460ee0aa336bee`;
extrude `a46c983c6f509c8cbc38bad372eb084cd20cb64d3a170a0f940f0f80330d80d7`.
11/09's recipes R1–R4 were run verbatim (R1/R2 `cmp`-identical to the tool; R3 135,308 faces,
R4 172,060 faces).

- **Full gate** `python3 scripts/check.py` with the four changed sources touched: 32 checks,
  30 passed, 1 SKIP (configure), 0 failed; **8 TUs compiled, 0 DualC-origin warnings, ctest
  322 / 0 failed** (87 s; 315 + 4 test cases + 3 cli tests).
- **GPU tier** `cmake --build build-gl -j && python3 scripts/check.py --gpu --strict
  --build-dir build-gl` → **parity 76/76** (count unchanged: no GLSL in T1).
- `dualc_field_view --expr 'revolve(circle,offset=2)' --snapshot v.png` →
  `codegen error at 0: GLSL codegen does not support op 'revolve'` (the note 11/09 carries).
- `--docs --strict` green.
- **CI:** run 38800024751 on `b5e22b1` — completed, success.
- **Loss audit** (`docs_loss_audit.py --base eeddf3f --head b5e22b1 --scope docs/`, scratch
  triage): **not finished** when the unit had to return (the full-tree run takes > 10 min).
  No docs file was deleted; the edits that replace text are the #58 status cells (NEXT →
  PARTIAL) in 21/01, 21/README and `roadmap/README.md`, and 11/01's grammar line (widened,
  not removed). Rerun it if a deletion is suspected.

## What I got wrong on the way

- Linked 11/09 to a #58 anchor I guessed from the plan's title (`…the-two-lifts…`); the
  heading is *2D profiles and revolve / extrude in the graph*. Fixed before the gate.
- Wrote a truncated SHA in 21/05 with the wrong tail; fixed against `sha256sum`.
- First draft of a typing test (a `translate` with two children) tested arity, not typing;
  replaced by a profile two levels down (`/in/1/in/0`).
- The first gate run skipped the warnings scan (incremental build); touching the changed
  sources gave the meaningful 8-TU scan.

## Open — for Giacomo

1. **Nested arrays are lenient off `points`:** `center=[[0,0,0]]` reads as `[0,0,0]`. Making
   every other reader refuse `rowSize != 0` is a few lines in each TU's param helpers
   (`field_graph.cpp` and `field_glsl.cpp`); I judged it not worth the duplication — say if
   you want it strict.
2. **Defaults kept for `revolve offset=1` / `extrude height=0.5`** (`dualc_lift`'s), and
   `polygon` has none (required) where `dualc_lift` defaults to a unit triangle.
3. Still open from P02: decision (b)'s breadth, GPU bakes above 256, `mesh-gwn`'s
   discrimination, CI's cold Eigen fetch.

## What the next unit (P03.2, #58 GPU half) needs to know

- `field_glsl.cpp` is untouched: lifts hit its *unknown op* branch (`emitNode`'s end).
  Add `revolve` / `extrude` there with a 2D emitter for the four profiles; `isProfileOp` /
  `isLiftOp` / `profileKinds()` are public in `field_graph.h`. Mirror the CPU reading exactly
  — defaults, slot layout (`circle` {0,0,0.5}: center 0..1, radius 2; `rect` {0,0,1,0.5}:
  center, half 2..3; `segment` {-1,0,1,0,0.3}: a, b, radius 4), flat-overrides-front,
  longer-refused, polygon `points` xor `params` — or, better, share the reader (the
  `readNodeBake` pattern: a small read in each TU, validation in `field_graph.cpp`).
- `ParamValue::rowSize` is new; `field_glsl.cpp`'s `nodeKey` hashes `vec` only — fine (rows
  and flat mean the same numbers).
- Polygon parity needs the library's formula exactly (`src/implicit/field2d.cpp`:
  min edge distance, sign flipped per crossing by the c1/c2/c3 test); lifts in
  `src/implicit/lift.cpp` (`revolve`: `profile(hypot(x,z) - offset, y)`; `extrude`:
  `min(max(d, |z|-h), 0) + length(max((d, |z|-h), 0))`).
- Remove 11/09's "Not in the live viewer yet" note and 11/01's "Not in `dualc_field_view`
  yet."; 12's recipes by node gain a lift recipe; when #58 is DONE: 21/01 status, 21/README
  row, `roadmap/README.md` milestone row + § Current focus + principal-blocks row, and D-11's
  row in `decisions/README.md` may note the sub-grammar landed.
- **Byte room:** 21/01 at 15,243 of 15,360 — #58's DONE flip must replace, not add (append
  the GPU evidence to 21/05). `roadmap/README.md` at 15,062; 11/01 at ~14.4 KB.
- `parity_expected_cases` is **76**. Next free IDs: **#61**, **D-63**.
- P06 (retire `dualc_lift`) must replace `cli_field_lift_same`'s comparison with a fixed
  expectation (e.g. a content check on face count) — it reads `cli_lift.obj`.

---

← Back to the [plan](../README.md).
