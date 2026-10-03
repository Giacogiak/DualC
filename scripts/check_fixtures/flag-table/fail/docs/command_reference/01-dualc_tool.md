# Tool 1: `dualc_tool`

| Flag | Default | Meaning |
| --- | --- | --- |
| `--depth N` | `7` | Octree max depth. |

## Recipes

| # | Command | What it does |
| --- | --- | --- |
| X1 | `dualc_tool --depth 5 --collapse 1 in.obj` | The parser knows the second flag; no table documents it. |

```bash
dualc_tool --frob 3 in.obj   # --frob: the parser does not know it
```
