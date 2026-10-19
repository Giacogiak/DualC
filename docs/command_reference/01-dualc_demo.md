# Tool 1: `dualc_demo` (mesh in → mesh out)

**Retired 2026-10-16.** `dualc_demo`, the CLI that re-meshed a triangle mesh through the dual
contourer, was removed once the graph's `mesh` node took every sign oracle it had (D-56 in
[the settled decisions](../decisions/01-settled.md)); tool number 1 is never reused. The record of the retirement —
what was deleted, and the `dualc_field` test that now covers each path the tool's tests
covered — is [roadmap 21/08](../roadmap/21-cli-consolidation/08-retire-tools.md) ([#60](../roadmap/21-cli-consolidation/01-record.md#60-retire-the-eight-tools)).

What replaces each of its uses, flag by flag, is [11/11](11-dualc_field/11-mesh-in.md) — the
same bytes for the same input:

| Was | Now |
| --- | --- |
| `dualc_demo in.obj out.obj`, its `--depth`, `--collapse` and `--no-manifold` | `dualc_field --expr 'mesh(path="in.obj")' -o out.obj` with the same options; `-o` also writes STL or 3MF ([11/11 § The input](11-dualc_field/11-mesh-in.md#the-input)) |
| `--sharp` | `mesh(…, normals=sharp)` ([11/11 § Sharp vs. smooth normals](11-dualc_field/11-mesh-in.md#sharp-vs-smooth-normals-normalssharp)) |
| `--pseudonormal`, `--gwn` | `mesh(…, sign=pseudonormal)`, `mesh(…, sign=gwn)` ([11/11 § Choosing a sign oracle](11-dualc_field/11-mesh-in.md#choosing-a-sign-oracle)) |
| `--gwn-field` — seal an open shell, a soup or a self-intersecting mesh | the `winding(path=…)` node: **the soup recipe** G9 ([11/11 § Recipes](11-dualc_field/11-mesh-in.md#recipes)) |
| Recipes D1–D10 | G1–G9, each with its old ID in a *Was* column ([11/11 § Recipes](11-dualc_field/11-mesh-in.md#recipes)) |

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
