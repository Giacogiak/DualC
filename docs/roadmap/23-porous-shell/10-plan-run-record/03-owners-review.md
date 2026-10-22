# The porous run — the owner's review list

Part of [the run's record](README.md) (23/10), gathered by P08.2 on 2026-10-22 from every
handoff's *For the owner's review* and *Open*. Everything here was verified headless as far as
a headless check goes; what remains needs a person, a decision, or the push. The in-run choices
are on [02](02-in-run-choices.md).

## What needs a person

Run from `~/Documents/DualC-porous`, one heavy process at a time (a porous contour at depth 7
takes up to 15 s and 3 GB).

1. **The NV92** (P05.1, P05.2, P08.1). The host rebooted during P08.1, and `glxinfo -B` then
   named **llvmpipe**, so the NV92 ran no committed case. When it is the active renderer again,
   one graph first, under a watch, then the tier:

   ```bash
   journalctl -kf | grep -i -E "trap|pgraph" &    # stop at the first line
   cmake --build build-gl -j
   timeout 60 build-gl/examples/dualc_field_view examples/samples/porous-sponge.json \
     --snapshot /tmp/sponge-nv92.png              # compare with build/porous-snapshots/porous-sponge.png
   python3 scripts/check.py --gpu --strict --build-dir build-gl   # 87/87 expected
   ```

   If it traps: bake on that GPU (`GlslLimits{…, 0}` units) or flatten the GLSL further
   ([07](../07-glsl-node.md#for-the-owners-review)). *Headless:* 87/87 on llvmpipe, twice.
2. **The viewer, live** (P05.2, P06.1's P17 / P18):
   `build-gl/examples/dualc_field_view examples/samples/porous-sponge.json` — nudge `t2` (walls
   thicken, no recompile); add `"smooth": 0.02` to the porous node and save (it reloads), then
   nudge `smooth` (corners round); `spacing` 0.4 → 0.3 and save (cells shrink). Then P17,
   `--expr "smooth-intersection(sphere(radius=1),onion(porous(sphere(radius=1),spacing=0.3,smooth=0.02),thickness=0.04),k=0.1)"`:
   `tab` to `smooth`, `thickness`, `k`. *Headless:* the GL-free rebuild test, two snapshots.
3. **Whether the samples look like a natural sponge** (P04.2). From `build/examples`:
   `dualc_field porous-sponge.json --depth 7 -o sponge.stl`, then `porous-foam-bracket.json`
   and `porous-bone-bracket.json` the same way; cut a section in a viewer or slicer: closed
   cavities, the sponge's walls thickening outwards, the bone's struts. *Headless:* watertight,
   the surface counts below.
4. **Read [design/13](../../../design/13-porous-shell.md)** against `include/dualc/porous.h`
   and 03–05 (P08.1), and **[11/16](../../../command_reference/11-dualc_field/16-porous-shell.md)**
   as a user would: the cost guide's wording, and whether V4 (8 non-manifold edges) belongs
   there (P06.1).
5. **The Boletus plan** (P07.1), below: confirm or change its § Decided by this plan.

## The decisions owed

- **design/10:** split it into a folder (about 50 links in 35 files) to take the porous hooks
  and constants, or accept design/13 as their home ([README](README.md#open-after-the-gates) item 1).
- **D-53:** reopen it (a `--gpu`-tier `--snapshot` smoke check), or record why not (item 2).
- **The crisp rim:** open #70, or a design/10 line (item 3).
- **`relax` and the thread count** beside #62 (item 4); **an `--egl` harness mode** (item 5).
- **D-74, D-75** and every row of [02](02-in-run-choices.md).

## The snapshots and the cavity counts

- `build/porous-snapshots/porous-sponge.png` (86 KB, the sponge sample) and
  `porous-sponge-struts.png` (143 KB, its recipe `porous-sponge-struts.json` beside it): llvmpipe
  renders through the viewer's `--snapshot`, gitignored ([08](../08-parity-and-viewer.md#the-snapshots)).
- Surfaces (the outer shell plus each closed cavity), every mesh watertight
  ([06](../06-sponge-end-to-end.md#the-samples)): `porous-sponge` **14**, `porous-foam-bracket`
  **23**, `porous-bone-bracket` **7** at depth 7; the ctest's sponge **14** at depth 6.

## The Boletus plan

Boletus `docs/raw/2026-10-22-porous-shell-plan.md` on branch **`feat/porous-shell-plan`**,
commit `1ccbe23` (cut from Boletus `main` at `2423275`, not pushed; the checkout is back on
`main`): `git -C ~/Documents/Boletus show feat/porous-shell-plan:docs/raw/2026-10-22-porous-shell-plan.md | less`.
Its pin is this branch's future merge commit; push it after DualC's PR merges.

## The counts

ctest **408 / 408** (39 of them the run's porous cases), GPU parity **87 / 87** on llvmpipe,
0 DualC-origin warnings over 260 TUs, `--docs --strict` green ([01](01-measurements-and-lessons.md#the-counts-the-run-moved)).

## The push and the PR

The branch holds the hygiene run's commits (`adc465b` and below) under this run's. So:

1. Push and merge `feat/hygiene-and-editor` first (its own PR, 17/22).
2. After the commit that deletes `plan/` (D-51):

   ```bash
   git push -u origin feat/porous-shell
   ```

   If the hygiene PR was squashed or rebased rather than merged, rebase first:
   `git fetch origin && git rebase --onto origin/main adc465b feat/porous-shell`.
3. Open <https://github.com/Giacogiak/DualC/compare/main...feat/porous-shell> with the text
   below. The first CI run also decides the cross-OS digests and MSVC's `predicates.c`
   ([README](README.md#open-after-the-gates) item 6).

### The PR text

> **Porous shell — a `porous` field-graph source (#63–#68)**
>
> A natural-sponge source for the field graph: seeds in any volume (`poisson` / `jitter` /
> `random`, a graded density over a control, Lloyd `relax`), one Delaunay tetrahedralization
> (own Bowyer–Watson on Shewchuk's vendored public-domain predicates), and the unsigned
> distance to its Voronoi or Delaunay complex, `walls` or `struts`, smoothed in the node and
> thickened by the existing `onion` / `graded-onion` / `graded-offset`.
>
> - **Library:** `include/dualc/porous.h` — `generatePorousSeeds`, `PorousField`; the complex
>   internal; deterministic, the seeds, tets and packed arrays pinned by digest.
> - **CLI:** `porous(…)` in `dualc_field` (12 keys), three samples, `docs/command_reference/11-dualc_field/16-porous-shell.md`.
> - **GPU:** the node in GLSL from data textures, a baked fallback past the limits; parity 82 → 87.
> - **C ABI:** no version move (0.5.2); `cli_c_abi_porous` covers an in-memory mesh volume,
>   JSON == shorthand, cancel.
> - **Boletus:** an implementation plan on Boletus `feat/porous-shell-plan` (not pushed).
> - **Design:** `docs/design/13-porous-shell.md`; the record is `docs/roadmap/23-porous-shell/`.
>
> Local gates on Linux (GCC 15): ctest 408/408, parity 87/87 (llvmpipe), 0 warnings,
> `--docs --strict` green. Not verified locally: macOS / Windows digests and MSVC's build of
> `predicates.c` (this CI run), the NV92. Known limit, older than this PR: a crisp
> `intersection` rim on a curved volume leaves a few non-manifold edges; the samples use a
> `smooth-intersection`. The in-run choices awaiting review: `23-porous-shell/10-plan-run-record/02-in-run-choices.md`.
>
> 🤖 Generated with [Claude Code](https://claude.com/claude-code)

---

← Back to [23/10 — the run's record](README.md) · [block 23](../README.md) · the [Roadmap index](../../README.md).
