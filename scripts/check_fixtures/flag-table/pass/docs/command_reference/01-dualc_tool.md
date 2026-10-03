# Tool 1: `dualc_tool`

| Flag | Default | Meaning |
| --- | --- | --- |
| `--depth N` | `7` | Octree max depth. |
| `--collapse E` | `0` | Adaptive cell-collapse threshold. |

## Recipes

| # | Command | What it does |
| --- | --- | --- |
| X1 | `dualc_tool --depth 5 --collapse 1 in.obj` | Both flags are in the table. |

```bash
# a comment mentioning --frob is not a recipe
./dualc_tool --depth 6 \
  --collapse 2 in.obj
```
