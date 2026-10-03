# Why the C ABI exists, where it lives, and the proxy-vs-export lever (§ 1, § 2, § 5)

Part of [14 — C ABI](README.md). The design decisions: why it landed when it did, why it
is host-side `capi/` rather than `src/`, and the one lever with one caveat.

## 1. Why it exists, and why it landed when it did

A plugin needs exactly two things from `libdualc`: an in-memory **proxy mesh** to
draw, and a full-res **export** to STL/3MF. Everything else in the engineer's
workflow (compose a field, preview it live, boolean as a field) is already
delivered from the command line — `dualc_field` (#1), `dualc_field_view` (#2),
streaming STL (#3), section planes (D.1), `graded-onion` (block 13). The one
missing bridge to a *plugin or thin standalone shell* was the **C-callable
boundary**.

The C ABI was re-sequenced **ahead of the Rhino side-car** (a deliberate change
from the block-12 order, made with the user) because `dualc_field_view` already
covers the standalone live-raymarch preview — so the side-car's live-viewport
half is no longer the blocker; the proxy+export bridge is.

**Why a C ABI rather than direct P/Invoke against the C++ API.** DualC's public
surface uses `std::shared_ptr<ImplicitField>`, `std::unique_ptr<SurfaceMesh>`,
exceptions, and templates — none of which cross a language boundary cleanly. A
narrow C surface (opaque handle + integer error codes + flat structs) is the
stable contract a wrapper can safely target, and it is also the right foundation
for any future binding (Python `ctypes`/`cffi`, WASM/Embind, Unity P/Invoke), not
just C#.

## 2. The key architectural decision — host-side `capi/`, not `src/c_abi/`

The original #19 sketch put the ABI in `src/c_abi/dualc_c.cpp` + `include/dualc_c.h`
and proposed building `libdualc` itself as a shared `dualc.dll`. **The re-scope
invalidated that placement.** The realized ABI:

- **builds a field from a graph string** → needs `examples/field_graph.{h,cpp}`,
  which vendors **nlohmann/json**, and
- **exports files** → needs `examples/example_common.{h,cpp}`, which vendors
  **miniz** (3MF) and owns the OBJ/STL/3MF writers.

Both are **host-side** code, deliberately *outside* `libdualc` by the
library-scope rule (mesh/file I/O and third-party deps never enter
`src/`/`include/dualc/`). Therefore the ABI **cannot** live in `src/` without
dragging those host-only deps into the core library. It lives in **`capi/`** as a
separate **shared library** that *links* `dualc::dualc` (static) +
`dualc_examples_fieldgraph` (static) + `dualc_examples_common` (static). The bare
core is never what gets wrapped — the **examples layer** is.

## 5. Proxy vs. export — one lever, one caveat

There is **no separate proxy call** — `dualc_field_contour` *is* the proxy when
you pass a coarse `maxDepth`, and the export-grade mesh when you pass full depth.
**Caveat:** a coarse-depth proxy of a **lattice** is inherently lossy
(under-resolved walls drop out — the same reason the roadmap kept raymarch as the
real lattice preview). The proxy is faithful for *boundary/solid* parts and rough
framing; for true live lattice inspection the host should use `dualc_field_view`
(or, later, the Rhino side-car).

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
