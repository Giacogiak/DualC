# Input forms, op vocabulary & text shorthand

Part of [Tool 11: `dualc_field`](README.md). How to hand the tool a graph (JSON file,
`--expr`, stdin — and shell quoting), then every node `dualc_field` (and
`dualc_field_view`) accepts — sources, booleans, decorators, domain operators, the
primitive parameter forms — and the `--expr` text grammar. Strut and graded lattices
have their own pages ([02](02-strut-lattices.md), [03](03-graded-and-morph.md)).

## Choosing an input form (JSON file vs `--expr` vs stdin)

JSON and the shorthand are **two notations for the same graph** — you never put
JSON inside `--expr`, and you never put shorthand in a `.json` file. Pick by task:

| Situation | Use | Why |
| --- | --- | --- |
| Any graph that references a **`mesh`/`winding` path** | **JSON file** | No shell-quoting pain — paths with `.`/`\` just work; the canonical, shareable form. |
| A quick **analytic** shape (primitives, TPMS, booleans, decorators) | **`--expr`** | One line, no file to create. |
| Piping a graph from another tool / Grasshopper | **stdin** (`-`) | `… | dualc_field - -o out.stl` |
| The version-controlled / reviewable source of truth | **JSON file** | Canonical; `--dump-json` produces it from shorthand. |

### Writing a JSON file (PowerShell, zero escaping)

A single-quoted here-string lets you paste JSON verbatim — its double quotes are
literal, so nothing needs escaping:

```powershell
@'
{ "version": 1, "units": "mm",
  "root": { "op": "intersection", "in": [
    { "op": "mesh", "path": "cube.obj" },
    { "op": "onion", "thickness": 0.12, "in": [
      { "op": "normalize", "in": [ { "op": "gyroid", "wavelength": 0.5 } ] } ] } ] } }
'@ | Set-Content -Encoding utf8 lattice.json

.\dualc_field.exe lattice.json --depth 6 -o lattice.stl
```

> Mesh `path`s are resolved **relative to your current working directory**. The
> demo meshes and the shipped sample graphs are copied next to the binary, so
> `cd build/examples/Release` first and reference them by bare name.

### `--expr` and shell quoting

`--expr` shines for **analytic** graphs, where the expression has no inner quotes:

```powershell
# PowerShell — wrap the whole expression in normal double quotes, no escaping:
.\dualc_field.exe --expr "union(sphere(center=[0,0,0],radius=1),box(min=[0,0,0],max=[1.5,1.5,1.5]))" --depth 5 -o demo.stl
```

A **`mesh`/`winding` path needs quotes** in the shorthand (`path="cube.obj"` — the
`.obj` dot isn't a bare word). The parser must see a **real `"`**; the only trick is
getting one past the shell without it being eaten:

- **bash / zsh** (or PowerShell **7+**): single-quote the whole expression — the
  inner double quotes pass through unchanged. This is the form the recipes below
  use:

  ```bash
  dualc_field --expr 'intersection(mesh(path="cube.obj"),sphere(radius=0.7))' --depth 5 -o m.stl
  ```

- **Windows PowerShell 5.1**: a **bare inner `"` is consumed by the native-argument
  layer** before the `.exe` sees it, so the bash form above *fails* with
  `field-graph error … expected ',' or ')'` (the parser receives an *unquoted*
  path). Fix: **escape every path quote as `\"`** — the backslash makes a literal
  quote reach the exe. The simplest form is the bash recipe with `\"` swapped in for
  `"` (outer single quotes unchanged — just add the backslashes):

  ```powershell
  .\dualc_field.exe --expr 'intersection(mesh(path=\"cube.obj\"),sphere(radius=0.7))' --depth 5 -o m.stl
  ```

  Also valid but fiddlier: a **double-quoted** outer string with each path quote
  written as backslash-backtick-quote (`\` then `` ` `` then `"`), or the `--%`
  stop-parsing token (still escaping as `\"`, but `--%` blocks `$env:…` expansion in
  later args). What **never** works is a **bare** `"`, single- or double-quoted. Or
  **skip escaping entirely with a JSON file** (above).

**Bottom line:** reach for `--expr` for analytic one-liners; reach for a **JSON
file** for anything with a mesh path (and as the canonical, shareable form).
`--dump-json` bridges them (shorthand → canonical JSON); `--list` prints the
vocabulary.

## Op vocabulary

### Sources

| op | params | notes |
| --- | --- | --- |
| `gyroid` · `schwarz-p` · `diamond` · `fischer-koch` · `lidinoid` · `neovius` | `center`:[x,y,z]=0, `wavelength`=1 | TPMS. Raw value is non-metric — wrap in `normalize` before a metric `onion`/smooth boolean. |
| `sc` · `bcc` · `fcc` · `octet` | `center`:[x,y,z]=0, `wavelength`=1, `radius`=0.1 | **Strut lattices** (wireframe crystals): simple-cubic, body-centered, face-centered, octet truss. A unit cell of capsule struts tiled infinitely; `radius` is the metric strut thickness. Already a true SDF — no `normalize` needed. Infinite extent, so clip it: `intersection(mesh(…), bcc(…))` or `intersection(box(…), bcc(…))`, or set `--bounds` for an open-ended inspection view. |
| `mesh` | `path` **or** `id` (exactly one), `sign` ∈ `parity`(def)/`pseudonormal`, `normals` ∈ `smooth`(def)/`sharp` | Signed distance to a watertight mesh (BVH). `sign:"gwn"` is **not** accepted here — use `winding`. |
| `winding` | `path` **or** `id` (exactly one) | Generalized-winding-number field: contours triangle **soup** / open shells / self-intersections into a watertight solid. |
| 30 analytic primitives | grouped keys or flat `params:[…]` (see below) | `sphere`, `box`, `roundbox`, `torus`, … — full param order in [02-dualc_primitive.md](../02-dualc_primitive.md). |

Mesh `path`s are resolved host-side by a caching resolver (each unique path is
loaded once); the loaded meshes are owned for the field's lifetime. The `id` form
(`mesh(id="…")` / `winding(id="…")`) references a host-provided **in-memory** mesh
buffer and is **C-ABI-only** — it has no meaning on the `dualc_field` CLI (which
registers no buffers and will error), and exists for the C ABI's
`dualc_field_create_from_{json,expr}_with_meshes` calls (see
[`docs/roadmap/14-c-abi/`](../../roadmap/14-c-abi/README.md)).

### Booleans (two children)

`union` · `intersection` · `difference` · `xor` — no params. `smooth-union` ·
`smooth-intersection` · `smooth-difference` — `k`=0.25 (blend radius, world units).

> Hard booleans need only the operand **sign**, so they are correct over the
> non-metric `winding`/raw-TPMS inputs (the safe core for soup-mixing). Smooth
> booleans assume metric distance — `normalize` such inputs first.

### Decorators & placement (one child)

| op | params |
| --- | --- |
| `offset` / `round` | `r` (req) |
| `onion` | `thickness` (req) |
| `scale` | `s`=1 |
| `elongate` | `h`:[x,y,z] (req) |
| `translate` | `by`:[x,y,z] (req) |
| `rotate` | `axis`:[x,y,z] (req), `degrees` (req) |
| `transform` | `matrix`:[16 row-major floats] (req) |
| `normalize` | — |

### Domain operators (one child)

| op | params |
| --- | --- |
| `mirror` | `normal`:[x,y,z] (req) |
| `repeat` | `period`:[x,y,z] (req) — *infinite → needs `--bounds`*. A child **wider than its period** costs ~8× more per evaluation: it must consult neighbouring tiles ([roadmap 17/07](../../roadmap/17-code-audit-and-hardening/07-repeat-tiling-fix.md)). |
| `repeat-limited` | `period`:[x,y,z] (req), `count`:[nx,ny,nz] (req) |
| `twist` | `radiansPerUnit` (req), `axis`:`"x"`\|`"y"`\|`"z"` |
| `bend` | `curvature` (req), `axis`:`"x"`\|`"y"`\|`"z"` |
| `displace` | `fn`:`"sine"`\|`"gyroid"`\|`"bumps"`, `amplitude`=0.1, `frequency`=6 |

### Primitive parameters: grouped keys + flat fallback

Core/common primitives accept **grouped semantic keys**:

| op | keys |
| --- | --- |
| `sphere` | `center`:[x,y,z], `radius` |
| `box` | `min`:[x,y,z], `max`:[x,y,z] |
| `roundbox` | `min`, `max`, `radius` |
| `capsule` / `cappedcylinder` | `a`:[x,y,z], `b`:[x,y,z], `radius` |
| `torus` | `center`, `major`, `minor` |
| `ellipsoid` | `center`, `radii`:[x,y,z] |

Every primitive (including these) also accepts a universal flat array
`"params": [...]` that maps positionally onto the documented parameter order in
[02-dualc_primitive.md](../02-dualc_primitive.md) (omitted trailing values fall back
to defaults). Use it for the long-tail primitives that have no grouped keys yet,
e.g. `{ "op": "hexprism", "params": [0,0,0, 1, 1] }`.

## Text shorthand

The shorthand is a one-line equivalent of the JSON form — handy on the command
line (`--expr`) or as a `.fld` file. It parses to the **same node tree** the JSON
reader produces, so every op, param key and default is identical; `--dump-json`
canonicalises shorthand → JSON.

**Grammar** (uniform prefix-call, no infix operators):

```
node  := IDENT '(' args? ')' | IDENT
args  := arg (',' arg)*
arg   := node                  # nested op-call → a child (appended to "in", in order)
       | key '=' value         # named param → params[key]
       | number | '[' … ']'    # bare positional scalar/array → flat params (see below)
value := number | '[' number (',' number)* ']' | IDENT | "string"
```

- **Children vs. params interleave freely** — `onion(normalize(gyroid()),thickness=0.3)`
  is child-then-param. A bare `IDENT` (with no `=`) is always a **child node**, e.g.
  `normalize(gyroid)` is a one-child decorator over a default gyroid.
- **Named params** use the same keys as JSON: `center`, `radius`, `min`, `max`,
  `wavelength`, `thickness`, `k`, `by`, `axis`, `degrees`, `matrix`, `period`,
  `count`, `path`, `sign`, `normals`, … (full list in the node tables above).
- **String values** are bare identifiers (`axis=z`, `fn=sine`, `sign=parity`) or
  double-quoted literals for paths: `mesh(path="bracket.obj")`. Backslashes inside
  a quoted string are literal, so Windows paths (`"D:\bracket.obj"`) survive.
- **Positional params:** bare scalars and `[…]` arrays (not `key=value`) are
  concatenated, in order, into the universal flat `params:[…]` array — the same
  positional primitive order as `--dump-json` / `dualc_primitive`. Example:
  `sphere(0,0,0,0.5)` ≡ `sphere(params=[0,0,0,0.5])` ≡ `sphere(center=[0,0,0],radius=0.5)`.

**Underscore aliases** — because shell tokens dislike hyphens, the *op token*
accepts an underscore form of each hyphenated registry token (only at op
position, never in a value): `schwarz_p`→`schwarz-p`, `fischer_koch`→`fischer-koch`,
`smooth_union`→`smooth-union`, `smooth_intersection`→`smooth-intersection`,
`smooth_difference`→`smooth-difference`, `repeat_limited`→`repeat-limited`.

## Recipes

| # | What it does |
| --- | --- |
| E1 | The gyroid-box JSON of the [overview](README.md#examples) as one shorthand expression |
| E2 | Smooth union of two primitives with an underscore alias and a blend radius |
| E3 | Canonicalise a shorthand file to JSON |

The `\` line-continuation is bash; on PowerShell put each command on one line and
mind the quoting rules in
[Choosing an input form](#choosing-an-input-form-json-file-vs---expr-vs-stdin):

```bash
# E1 — The gyroid_box JSON of the overview page, as one shorthand expression:
dualc_field --expr "difference(intersection(box(min=[-2,-2,-2],max=[2,2,2]),\
  onion(normalize(gyroid(wavelength=1)),thickness=0.3)),sphere(radius=1))" -o part.obj

# E2 — Smooth-union of two primitives with an underscore alias and a blend radius:
dualc_field --expr "smooth_union(sphere(radius=1),box(min=[0,0,0],max=[2,2,2]),k=0.3)"

# E3 — Canonicalise a shorthand file to JSON (e.g. to hand to Grasshopper):
dualc_field part.fld --dump-json
```

---

← Back to the [Tool 11 overview](README.md) · the [Command Reference index](../README.md).
