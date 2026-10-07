# C ABI — native-consumer boundary (proxy + export)

> The complete development record **and** reference for DualC's C ABI
> (`dualc_capi`), the flat C-callable boundary a plugin (Rhino/Grasshopper), a
> thin standalone shell, or any non-C++ host uses to drive the field-graph
> pipeline. This is roadmap item **#19**
> ([10-infrastructure-and-integration.md](../10-infrastructure-and-integration.md)),
> re-scoped (2026-06-12) to **batch proxy-mesh + export-mesh generation** and
> **DONE 2026-06-17**. The live viewport is *not* the ABI's job — a host side-car
> drives that, and `dualc_field_view` already provides standalone live preview.
>
> 🍄 **Consumed by Boletus.** The **C# P/Invoke wrapper** over these 11 entry
> points (the original 9 + the v0.3.0 `*_with_meshes` in-memory-mesh create twins; 13 since 0.4.0, 20 since 0.5.0 — [`capi/README.md`](../../../capi/README.md))
> is now owned by the Boletus project (`D:\Boletus`) — its hand-over spec is
> [`../../capi/CSHARP_WRAPPER_HANDOFF.md`](../../../capi/CSHARP_WRAPPER_HANDOFF.md);
> status and the original DualC intent are in
> [15-boletus-handoff.md](../15-boletus-handoff.md). **DualC owns the ABI below; the
> wrapper and plugin are Boletus'.**

## The pages of this topic

Split on 2026-09-11 from the single file (the record is verbatim; only status moved out
of headings). Section numbers § 1–§ 9 are the stable addresses; § 9 stays on this page;
entries after the 2026-06-17 verification record are on the fourth page.

| Page | Sections |
| --- | --- |
| [Design](01-design.md) | [§ 1 why](01-design.md#1-why-it-exists-and-why-it-landed-when-it-did) · [§ 2 host-side `capi/`](01-design.md#2-the-key-architectural-decision--host-side-capi-not-srcc_abi) · [§ 5 proxy vs export](01-design.md#5-proxy-vs-export--one-lever-one-caveat) |
| [Surface and contract](02-surface-and-contract.md) | [§ 3 the surface](02-surface-and-contract.md#3-the-surface--the-graph-string-is-the-construction-api) · [§ 4 ownership, lifetime, errors](02-surface-and-contract.md#4-contract--ownership-lifetime-errors) |
| [Implementation and verification](03-implementation-and-verification.md) | [§ 6 map](03-implementation-and-verification.md#6-implementation-map) · [§ 7 build & deployment](03-implementation-and-verification.md#7-build--deployment) · [§ 8 tests (2026-06-17)](03-implementation-and-verification.md#8-tests--verification-record) |
| [ABI 0.4.0 and later](04-abi-0-4-0.md) | [the `*_with_diagnostics` twins (2026-09-09)](04-abi-0-4-0.md#diagnostics-twins-abi-040) · [unknown parameter keys rejected (2026-09-11)](04-abi-0-4-0.md#unknown-parameter-keys-are-rejected-behaviour-change-no-abi-break) · later ABI entries |
| [Progress and cancellation (#48)](05-progress-and-cancel.md) | [#48 the item](05-progress-and-cancel.md#48-cooperative-cancellation--coarse-progress-engine-to-abi) · the decisions (D-44 … D-47), the checkpoints and the latency bound, the writers' `.part` convention, the ABI twins, the verification record |
| [The shared library on Linux (#53)](06-linux-shared-library.md) | [#53 the item](06-linux-shared-library.md#53-libdualc_capiso-links-on-linux-without-a-caller-supplied--fpic) — PLANNED: the static libraries the `.so` links are built position-independent |

## 9. Deferred (explicitly not in this version)
*(Heading kept as the anchor; the first two bullets have since shipped or moved — their status is inline.)*

- **In-memory mesh-source resolver — DONE (v0.3.0).** A host with meshes already
  in RAM (Rhino/Grasshopper) no longer writes a temp file: it passes
  `DualcMeshSource` buffers to `dualc_field_create_from_{json,expr}_with_meshes`
  and references them by id (`mesh(id="…")`/`winding(id="…")`). Implemented as
  `dce::fieldgraph::InMemoryMeshResolver` (registry by id + `FileMeshResolver`
  delegate for `path=`); byte-identical to the disk path
  (`cli_c_abi_mesh_inmem`). This was the single most important constraint for the
  C# wrapper / Rhino plugin and is now lifted.
- **C# P/Invoke wrapper + `.gha` Grasshopper plugin** — 🍄 **moved to Boletus**
  (`D:\Boletus`): the wrapper is DONE (`Boletus.Core`), the `.gha` is in progress.
  Hand-over spec [`../../capi/CSHARP_WRAPPER_HANDOFF.md`](../../../capi/CSHARP_WRAPPER_HANDOFF.md);
  record [15-boletus-handoff.md](../15-boletus-handoff.md).
- **3MF-streaming / `--mem` RAM-budget** behind `dualc_field_export_tiled_stl`
  (the streaming writer is STL-only today — see
  [11 step 5a](../11-dense-lattice-deliverable/03-streaming-export.md#5a-deferred-enhancements-gains-vs-the-shipped-stl-version)).
  *(2026-09-18: "STL-only" is the ABI — the CLI shipped 3MF streaming and `--mem` on
  2026-07-03/06; `dualc_field_export_tiled_stl` is still the only tiled entry point. Trigger
  written as row D-21 of the [decisions index](../../decisions/README.md).)*
- **DAG-ref serialization** in the graph string (tree-only today — block 12 §A).
- *(2026-09-22)* **Cancellation and progress — DONE**, as #48 on
  [05](05-progress-and-cancel.md): the token object, the `*_with_progress` twins,
  `DUALC_CANCELLED`; what stays deferred there is D-44 / D-45.

---

← Back to the [Roadmap index](../README.md) · related:
[#19 in Infrastructure](../10-infrastructure-and-integration.md) ·
[Field-graph & app (block 12)](../12-field-graph-and-app/README.md).
