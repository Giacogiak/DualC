# P05 — Move every retiring page's content into the framework docs

§ 3 P05 of [the raw plan](../docs/raw/2026-10-14-cli-consolidation-plan.md). Nothing is deleted
here: the old tools still run, so every migrated recipe can be checked against them. Branch
`feat/cli-consolidation`.

### T1. Re-home the content of pages 01–07 and 10 under 11 and 12, with ready-to-run recipes

For each retiring page, read it whole and give every fact a home under
`docs/command_reference/11-dualc_field/` or `12-dualc_field_view/` (new children where a page
would trip the size cap — split, never append past it):

1. **01 `dualc_demo`** → the mesh-in workflow: re-mesh, `normals="sharp"`, *Choosing a sign
   oracle* (`parity` / `pseudonormal` / `gwn` / the `winding` field), `--no-manifold`, the input
   requirements, and **the soup recipe**:
   `dualc_field --expr 'winding(path="mesh-soup.obj")' -o sealed.stl`.
2. **02 `dualc_primitive`** → the catalogue P1–P30 with parameters and defaults, and the
   decorator / domain-operator walk-through, in graph syntax (keep the P-numbers as anchors).
3. **03 `dualc_boolean`** → booleans of meshes, `k` sizing, baking (`bake=`, from P02).
4. **04 `dualc_lift`** → already written by P03; check nothing on 04 is missing.
5. **05 `dualc_csg_demo`** → the six recipes as committed graph files in `examples/samples/`
   (`.json` or `.fld`, one each, parameters at the recipe defaults), each with a run line.
6. **06 `dualc_lattice`** → the TPMS-in-mesh recipe with the defaults spelled out: wavelength =
   mesh AABB diagonal / 10, centre = AABB centre, bounds = AABB + 5 %; `--offset T` = `onion(…,
   thickness=T)`; `--normalize-thickness` = `normalize(…)`; OBJ / STL / 3MF by `-o`.
7. **07 `dualc_slice`** → already written by P04; check.
8. **10 `dualc_raymarch`** → the density-independent TPMS-in-mesh preview as a
   `dualc_field_view` recipe, and what replaces its `t` / `n` hotkeys (edit the file, auto-reload).
9. `command_reference/README.md` § *Where a feature is documented* points at the new homes; the
   appendix inventory is updated in P06, not here.

Run every recipe you write once, against the current build, and record in block 21 that it ran
(and, for the six samples and the lattice recipe, that its output equals the old tool's).

#### Acceptance criteria

- Every heading of pages 01–07 and 10 maps to a new home, listed in the handoff as a table
  (old page § → new page §); `scripts/docs_loss_audit.py` run with `--scope` over the eight
  pages against the branch base shows nothing untriaged that P06's deletion would lose.
- `python3 scripts/check.py` green (the six samples are files the gate builds or reads).

## Exit criteria

- The framework pages alone document every capability the eight tools had.
