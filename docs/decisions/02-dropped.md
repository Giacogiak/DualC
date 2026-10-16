# Dropped decisions — what was rejected, and why

Part of the [decisions index](README.md). The decisions **DROPPED**: proposed, weighed and
rejected, each with the rationale in the row and the entry that holds the argument, so it is
not proposed again. Same columns and the same ID sequence as the README's table; a dropped
decision that is revived gets a new row with the new date. Split from the README on
2026-10-15 to give it room under the byte cap (D-61); the rows moved unchanged.

| ID | Decision | Status | Date | Where recorded |
| --- | --- | --- | --- | --- |
| D-05 | A multi-mesh "soup source" CLI: a soup is one mesh (GWN is connectivity-agnostic) | DROPPED | 2026-05-22 | [01 Tier-2 item 6](../roadmap/01-core-dual-contouring/README.md#6-generalized-winding-number-with-hierarchical-acceleration) |
| D-08 | Polyscope's `VolumeGrid` for dense-lattice preview: it marches a baked grid, which aliases by Nyquist (the viewer sphere-traces the analytic field instead, D-06) | DROPPED | 2026-06-03 | [08 step 3](../roadmap/08-raymarch.md#step-3-analytic-raymarch-viewer-dualc_raymarch--see-dense-parts-done--2026-06-03) |
| D-10 | The C ABI's live-viewport ambitions: it is batch proxy-mesh + export only; a host side-car draws | DROPPED | 2026-06-12 | [10 #19](../roadmap/10-infrastructure-and-integration.md#19-c-abi-for-native-consumers-phase-1-proxy--export-done--2026-06-17) |
| D-14 | `dualc_lattice --tile-depth` wiring (5a.3) — convenience parity only | DROPPED 2026-10-16 — the tool retired (#60, D-55), so its trigger, a `dualc_lattice` user who cannot switch to `dualc_field`, can no longer fire; its graph streams through `dualc_field --tile-depth` | 2026-06-15 | [11/03 § 5a](../roadmap/11-dense-lattice-deliverable/03-streaming-export.md#5a-deferred-enhancements-gains-vs-the-shipped-stl-version) |
| D-18 | Graded *wavelength*: out of scope (chirp / eikonal PDE); graded thickness ships as `graded-onion` over a generic control field | DROPPED | 2026-06-17 | [13 § Decision](../roadmap/13-graded-tpms/README.md#decision) |
| D-32 | A GPU snapshot script for the preview sweep: it duplicated the parity cases and needed a mesh no clean checkout has — a gate that cannot run on a clean checkout is not a gate | DROPPED | 2026-08-25 | [12/07 § H](../roadmap/12-field-graph-and-app/07-mesh-preview-sweep.md#h-mesh-preview-correctness-sweep) |
| D-34 | A general "root box exceeds `bounds()`" diagnostic: written and rejected — it fires on nearly every run; `gridBoundsExceeded` fires only for a `GridField` at the root | DROPPED | 2026-09-09 | [17/05 § Not reported](../roadmap/17-code-audit-and-hardening/05-diagnostics-channel.md#what-is-deliberately-not-reported) |

---

← Back to the [decisions index](README.md) · the [docs index](../README.md).
