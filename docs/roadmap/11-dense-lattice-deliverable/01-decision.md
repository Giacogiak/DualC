# The decision: how a dense lattice gets seen and printed

Part of [11 — Dense-lattice deliverable](README.md). The problem statement and the
three-step plan (slice · export · raymarch) as decided on 2026-06-02.

## The decision
**2026-06-02.**


Testing a gyroid infill at scale surfaced the binding problem: a *dense* lattice
in a *large* volume (e.g. a 300 mm cube) explodes the output triangle mesh.

**Measured cost (bunny, gyroid, dev PC):** throughput ≈ 0.24 ms/output-face;
OBJ ≈ 270 bytes/face; faces ∝ N³ (N = box/wavelength); ~4× per `--depth` level.
A *solid* gyroid filling a 300 mm cube:

| λ | N | depth | faces | time | peak RAM |
|---|---|---|---|---|---|
| 30 mm | 10 | 7 | ~0.7 M | ~3 min | ~0.3 GB |
| 10 mm | 30 | 9 | ~19 M | ~75 min | ~6 GB |
| 5 mm | 60 | 10 | ~155 M | ~10 h | ~45 GB |

**RAM is the wall, not time** (geometry-central halfedge mesh ≈ 200–300 B/face)
— OOM around λ≈8–12 mm (~10–20 M faces) on a 16–32 GB machine. Absolute mm are
irrelevant; only N (cells across) and octree depth matter.

**Core insight that drives the whole roadmap:** the `ImplicitField`, not the
mesh, is the asset. `valueAt`/`gradientAt` are cheap (TPMS ~20–30 flops;
`MeshSource` ~40–100 via BVH), fully `const` and thread-safe. The cure for both
visualization and manufacturing is the same: **stop materializing the full mesh
— sample the field only where needed** (screen pixels / a slice plane / per
print-layer).

**Two findings that fixed the scope:**

- **Manufacturing reality (researched).** 3MF's *beam-lattice* extension encodes
  **strut** lattices only — it cannot represent a surface TPMS (even nTopology
  must mesh gyroids). Its *volumetric/implicit* extension can carry the field
  compactly, but consumer slicers (Prusa/Bambu) don't read it yet (industrial-AM
  only). So a surface TPMS bound for a desktop printer **is a triangle mesh** —
  STL or 3MF-mesh (3MF-mesh ≈ ⅓ STL size). STL/3MF is the necessary "printable
  now" interchange; it does not by itself beat the density wall (that is the
  tiling step). VDB/voxel handoff only pays off for an implicit-native downstream
  (nTop, DLP), which Rhino is not.
- **Convergence.** Both visualization and manufacturing rest on **one shared
  primitive: sample the field on a plane/grid.** The slice plane is the cheapest
  new visualization *and* the kernel of direct print-slicing — so it is built
  first and kept separable.

**Downstream target:** a C ABI for Rhino/Grasshopper
([10-infrastructure-and-integration.md](../10-infrastructure-and-integration.md)
#19). Every new capability here uses flat data + extension dispatch so it stays
C-wrappable, but no ABI is implemented in this roadmap. Per the library-scope
rule, all file I/O lives in `examples/`; `src/`/`include/dualc/` stay
dependency-free.

---

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
