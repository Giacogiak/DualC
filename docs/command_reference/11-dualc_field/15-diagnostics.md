# Errors and diagnostics

Part of [Tool 11: `dualc_field`](README.md): what the tool reports when a graph is wrong, and
where its runtime warnings are documented. *(Moved here verbatim from the tool's README on
2026-10-19, when `--threads` needed its room under the size cap.)*

## Diagnostics

Errors are reported with the offending node's **JSON pointer**:

```
[dualc_field] field-graph error at /root/in/0/in/1: unknown op 'frobnicate'
```

Caught cases: invalid JSON, unknown op, wrong boolean arity, missing required
param, bad `mesh` sign/normals (the message lists the accepted values), a `bake` outside
[2, 512] or a `band` without `bake` on `mesh`/`winding`, a 2D profile outside a lift or a
3D child under one ([09](09-2d-profiles-and-lifts.md#typing-errors)), mesh-not-found.
An **unbounded** graph contoured without `--bounds` prints the sampler's
`--bounds` hint and exits non-zero.

Runtime **`[dualc] warning:` lines** (empty contour, bounds fallback, sampling
past a baked grid, a non-closed output) are documented once for every tool in
[Shared behaviour § Diagnostics warnings](../00-shared-behaviour.md#diagnostics-warnings-every-exporting-tool).

---

← Back to [Tool 11: `dualc_field`](README.md) · the [Command Reference index](../README.md)
