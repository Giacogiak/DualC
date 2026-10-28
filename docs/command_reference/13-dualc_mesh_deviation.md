# Tool 13: `dualc_mesh_deviation` (deviation and creases of a contoured mesh)

How far a contoured OBJ sits from the field that produced it, and how creased it is: the
before/after figures the roadmap's records tabulate (first [21/04](../roadmap/21-cli-consolidation/04-mesh-node-bake.md#the-measurements),
then [17/20](../roadmap/17-code-audit-and-hardening/20-gridfield-normal/README.md)), as a tool
instead of a scratch evaluator. **Opt-in build** (`-DDUALC_BUILD_BENCH=ON`), no CTest case;
`scripts/bench_e12.py` drives it on the record's cases and prints the record's tables.

```
dualc_mesh_deviation <mesh.obj> (--expr "<shorthand>" | --graph FILE) [--old ref.obj] [--threads N] [--json]
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `<mesh.obj>` | (required) | The contoured mesh to measure (an OBJ `dualc_field` wrote). |
| `--expr S` | — | The field, as `dualc_field`'s [text shorthand](11-dualc_field/01-op-vocabulary.md#text-shorthand). Pass the **exact** graph: the contoured expression with its `bake=` removed; the tool strips nothing itself. |
| `--graph FILE` | — | The field from a file instead: JSON when the first non-blank character is `{`, shorthand otherwise (`dualc_field`'s rule). Exactly one of `--expr` / `--graph`. |
| `--old PATH` | — | A reference OBJ with the **same vertex count** (a run of another binary on the same graph): adds whether the face lists are identical and each vertex's shift, max and mean. A different vertex count is an error. |
| `--threads N` | `0` (all) | Worker threads for the per-vertex evaluation and the graph's own bakes, at most `1024`. |
| `--json` | off | One JSON object instead of `key value` lines (what `bench_e12.py` reads). |

## What it prints

One `key value` line each, in this order:

| Key | Meaning |
| --- | --- |
| `faces`, `vertices` | Of the measured mesh. |
| `dev_max`, `dev_mean` | `\|valueAt\|` of the field at every vertex, max and mean, in the field's units: zero for a vertex exactly on the surface. For a metric field (a `mesh` source, hard booleans of them) it is the distance to the exact surface. |
| `dihedral_lt1` … `dihedral_ge90` | The unsigned dihedral angle over every interior edge, in degrees, binned `< 1`, `1–5`, `5–10`, `10–20`, `20–45`, `45–90`, `≥ 90`. |
| `dihedral_boundary` | Edges skipped: boundary edges and edges with other than two faces. |
| `old_faces`, `old_vertices`, `faces_identical`, `shift_max`, `shift_mean` | With `--old` only. `faces_identical` is `true` when both face lists are the same, index for index: the two runs found the same signs and the same crossings, and only the vertex positions moved. |

The mesh node of the graph resolves `path=` against the working directory, so run it from the
folder that holds the OBJs the graph names (the recipes below run from `data/`).

## Recipes

```bash
# R1 — Build the tool (off by default), once:
cmake -S . -B build -DDUALC_BUILD_BENCH=ON && cmake --build build --target dualc_mesh_deviation

# R2 — A baked sphere, contoured finer than its grid, measured against the exact mesh field:
cd data
../build/examples/dualc_field --expr 'mesh(path="sphere.obj",bake=16)' --depth 6 -o /tmp/e.obj
../build/examples/dualc_mesh_deviation /tmp/e.obj --expr 'mesh(path="sphere.obj")'

# R3 — The same run by two binaries, old and new, with the per-vertex shift:
../build/examples/dualc_mesh_deviation /tmp/e_new.obj --expr 'mesh(path="sphere.obj")' --old /tmp/e_old.obj --json

# R4 — The record's whole round (cases A–E, wall median of 3, RSS max of 3), from the repo root:
python3 scripts/bench_e12.py --old /path/to/old/dualc_field --new build/examples/dualc_field --data data
```

`bench_e12.py` takes `--tool` (the measurer's path, default `build/examples/dualc_mesh_deviation`),
`--out` (where the meshes land), `--cases` (a subset of `A,B,C,D,E`) and `--runs` (timed runs
per binary, default 3). Passing the same binary as `--old` and `--new` checks the tool against
the record's rows: the shift is zero and the figures reproduce. It needs `/usr/bin/time`
(`-v` on Linux, `-l` on macOS).

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
