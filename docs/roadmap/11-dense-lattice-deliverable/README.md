# Deliverable: visualizing & manufacturing-exporting dense TPMS lattices

> A focused, multi-step deliverable spun out of the main [roadmap](../README.md)
> because it is a large-scale, cross-cutting effort (its own viewer paths, file
> writers, vendored deps, and a future slicing pipeline). The engine's goal is to
> produce 3D-print-ready parts filled with TPMS lattice; this roadmap is how a
> user **sees** and **exports** those parts when the lattice is dense enough that
> the contoured mesh would no longer fit in memory. Cross-references
> [05-tpms-lattices/](../05-tpms-lattices/README.md) #16 (TPMS lattices, done), #18
> (Lipschitz `cellOverlaps`, deferred), and
> [10-infrastructure-and-integration.md](../10-infrastructure-and-integration.md)
> #19 (C ABI for Rhino/Grasshopper — the headline downstream consumer).

The three delivered steps each have their own page:
[step 1 — `dualc_slice`](../06-slice.md), [step 2 — STL/3MF export](../09-io-formats.md),
[step 3 — `dualc_raymarch`](../08-raymarch.md). The decision that drives all of them,
and the open steps 4–6, are below.

---

## The pages of this topic

Split on 2026-09-11 from the single frozen file (the record is verbatim; only status
moved out of headings). Step numbers (1–6, 5a, 5b) are the stable addresses; steps 4 and
6 and the status summary stay on this page.

| Page | Sections |
| --- | --- |
| [The decision](01-decision.md) | [the 2026-06-02 problem statement and plan](01-decision.md#the-decision) |
| [Phase 0 benchmark](02-phase-0-benchmark.md) | [the measured RAM wall (2026-06-12)](02-phase-0-benchmark.md#phase-0-benchmark) |
| [Streaming export](03-streaming-export.md) | [step 5 delivered](03-streaming-export.md#5-tiledstreaming-mesh--break-the-ram-wall-for-stl3mf-at-high-density) + [§ 5a deferred enhancements](03-streaming-export.md#5a-deferred-enhancements-gains-vs-the-shipped-stl-version); the 2026-09-22 pointer to #48 (cancel/progress hooks, `.part`, the finalize-then-fail exits closed) |
| [Streaming 3MF record](04-streaming-3mf-record.md) | [§ 5b implementation record (2026-07-03)](04-streaming-3mf-record.md#5b-implementation-record--streaming-3mf--seam-welding) |

## The open steps

### 4. Rhino preview/LOD mesh — let Rhino *draw* a dense part
🍄 **Moved to Boletus.**


> 🍄 **Moved to Boletus.** The in-Rhino LOD/preview proxy is now owned by the
> Boletus project (`D:\Boletus`) — realized as the capped `ProxyPreviewComponent`
> (DONE 2026-06-20). It consumes DualC's C-ABI proxy contour (flat data, the
> `dualc_field_contour` coarse-`maxDepth` lever); DualC keeps the ABI, Boletus
> keeps the viewport component. Original DualC intent and the full record:
> [15-boletus-handoff.md](../15-boletus-handoff.md).

### 6. Not on the critical path
**DEFERRED** — each item names its trigger.


- **B.2 narrow-band VDB / voxel export.** "Hand the tool the field" for
  implicit-native consumers (nTop, medical/FEA, DLP). Deferred: OpenVDB is a
  heavy dependency (TBB/Blosc, awkward on MSVC), and **Rhino is not a VDB
  consumer** — no concrete downstream yet. If voxels are ever needed sooner, a
  lightweight NRRD / raw-grid + JSON sidecar (reusing `bakeToGrid`) dodges the
  OpenVDB dep. *Trigger:* a confirmed implicit/voxel-native consumer.
- **B.4 hybrid parametric descriptor.** Ship a small outer-shell mesh + a tiny
  implicit infill descriptor (type/λ/offset/clip). Smallest possible, trivial
  over the C ABI — but only works in a closed loop we own (no third-party
  consumer understands it). Best framing: the Rhino plugin's *project file*, not
  a manufacturing export. *Trigger:* the #19 plugin needs to round-trip lattice
  parameters.
- **A.3 LOD / proxy mesh.** Coarse depth + aggressive `--collapse` for
  interaction, full-res on demand (the 50k-leaf gate already exists in
  `dualc_view`). For the *standalone* "see dense parts" need it is superseded by
  [step 3](../08-raymarch.md)'s raymarch (still-meshing looks misleadingly wrong at
  coarse TPMS); but the technique is exactly what **step 4** needs to put a
  drawable preview in Rhino's viewport, so it is now folded there rather than
  deferred on its own.

---

## Status summary

| Step | Item | Status |
|---|---|---|
| 1 | [Field-on-plane sampler (`dualc_slice`, A.1 + B.1 kernel)](../06-slice.md) | **DONE** 2026-06-02 |
| 2 | [STL + 3MF-mesh writer (`writeField` ext-dispatch, B.3)](../09-io-formats.md) | **DONE** 2026-06-02 |
| 3 | [Analytic raymarch viewer (`dualc_raymarch`, A.2)](../08-raymarch.md) | **DONE** 2026-06-03 |
| 4 | Rhino preview/LOD mesh (A.3) | 🍄 **moved to Boletus** — `ProxyPreviewComponent`, DONE 2026-06-20 ([15](../15-boletus-handoff.md)) |
| 5 | Tiled/streaming mesh + direct slicing (B.5 / B.1) | **DONE (STL 2026-06-15; 3MF + seam-welding 2026-07-03; `--mem` auto-budget 2026-07-06)** (`dualc_field --tile-depth -o .stl\|.3mf [--weld]` / `--mem BUDGET`; one-tile peak RAM, bit-identical STL / geometry-identical 3MF / topology-identical welded 3MF — the numbers are in [03](03-streaming-export.md)); direct slicer DEFERRED ([D-13](../../decisions/README.md)) |
| 6 | VDB (B.2), descriptor (B.4) | DEFERRED |

Steps 1–2 committed as `e996cda`, step 3 as `248d706` (2026-06-12). The 2026-06-12 re-sequencing
(field-graph → raymarch app → streaming,
ahead of the C ABI) is in [12-field-graph-and-app/](../12-field-graph-and-app/README.md).

---

← Back to the [Roadmap index](../README.md).
