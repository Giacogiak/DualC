# Boletus hand-off — the C# / Rhino / Grasshopper client (archived from DualC)

> 🍄 **What this file is.** The Rhino/Grasshopper front-end that consumes DualC's
> C ABI now lives in its **own project — Boletus** (`D:\Boletus`, roadmap at
> `D:\Boletus\docs\roadmap\`). Several items the DualC roadmap originally planned
> as its own "next up" have therefore crossed the repo boundary. To keep the DualC
> roadmap focused exclusively on the **engine + C ABI**, those items were **moved
> here** rather than left scattered across the topic files. The original DualC
> intent for each is **preserved verbatim below** (the "Archived intent" sections),
> so nothing is lost — the topic files keep only a one-line pointer back to here.

**The boundary rule.** DualC owns the *engine* and the *C-callable boundary*
(`dualc_capi`): the field-graph, dual contourer, GLSL codegen, `dualc_field_view`,
streaming export, and the C ABI (entry-point count and version:
[`capi/README.md`](../../capi/README.md)). **Boletus** owns everything *above*
that boundary: the C# P/Invoke wrapper, the managed field-graph model, the
Grasshopper `.gha` components and Rhino glue, the Rhino in-viewport proxy, the live
raymarch side-car, and plugin packaging.

## Hand-off table — moved to Boletus

| DualC origin | What it was | Boletus home | Status |
| --- | --- | --- | --- |
| [10 §19](10-infrastructure-and-integration.md) — Phase 2 | C# P/Invoke wrapper over the C ABI | `Boletus.Core` — [`03-phase2-core-wrapper.md`](file:///D:/Boletus/docs/roadmap/03-phase2-core-wrapper.md) | **DONE** 2026-06-17 |
| [12 §A](12-field-graph-and-app/README.md) — DAG-ref / GH authoring | Managed field-graph model + JSON serializer (`--dump-json`-faithful) | `Boletus.Core` — [`04-phase3-field-graph-serializer.md`](file:///D:/Boletus/docs/roadmap/04-phase3-field-graph-serializer.md) | **DONE** 2026-06-18 |
| [10 §19](10-infrastructure-and-integration.md) — Phase 3 | `.gha` Grasshopper components + Rhino glue | `Boletus.Grasshopper` — [`05-phase3-grasshopper-components/`](file:///D:/Boletus/docs/roadmap/05-phase3-grasshopper-components/README.md) | **PARTIAL** (status on that page) |
| [11 §4](11-dense-lattice-deliverable/README.md) | Rhino preview / LOD proxy mesh | `ProxyPreviewComponent` (Boletus 3b) | **DONE** 2026-06-20 |
| [12 §E](12-field-graph-and-app/README.md) / sequence item 5 (was #4 before 12's sequence was renumbered in `65a97ea`) | Rhino side-car (live raymarch beside Rhino) | Boletus Phase 5 — [`07-upstream-coordination/02-viewer-and-uniform-push.md` §5](file:///D:/Boletus/docs/roadmap/07-upstream-coordination/02-viewer-and-uniform-push.md#5-dualc_field_view-viewer-binary--gates-the-raymarch-preview-phase) | **DONE** 2026-07-03 |
| Plugin packaging | Yak package, zero-prereq install | Boletus Phase 4 — [`06-phase4-distribution-and-packaging.md`](file:///D:/Boletus/docs/roadmap/06-phase4-distribution-and-packaging.md) | **PLANNED** |

## DualC retains — engine & ABI (Boletus only *requests* these upstream)

These stay DualC's responsibility and keep their detail in the topic files; Boletus
is the *driver* (the reason to do them), not the owner.

| Item | Lives in | Boletus relationship |
| --- | --- | --- |
| The C ABI itself (`dualc_capi`; entry-point count and version in [`capi/README.md`](../../capi/README.md)) | [14-c-abi/](14-c-abi/README.md), [10 §19](10-infrastructure-and-integration.md) | **DONE** — Boletus links it (Phase 1, vendored DLL) |
| In-memory mesh-source resolver (`*_with_meshes`) | [14 §9](14-c-abi/README.md) | **DONE** (v0.3.0) — unblocked Boletus' meshless `Volume` |
| Field-graph engine, GLSL codegen, `dualc_field_view`, streaming STL | [12](12-field-graph-and-app/README.md) | **DONE** — Boletus consumes (`dualc_field_view` is the Phase-5 side-car binary) |
| Static `/MT` MSVC-runtime build of `dualc_capi.dll` | build/CMake | **Boletus-driven** — needed for Boletus Phase 4 zero-prereq Yak install |
| DAG-ref serialization superset (`id`/`ref`, `let … in …`) | [12 §A](12-field-graph-and-app/README.md) | **Boletus-driven** — wanted when large GH canvases share sub-fields |
| Cancel token + progress callback (`*_with_progress` twins, ABI 0.5.0) | [14/05](14-c-abi/05-progress-and-cancel.md) | **DONE** (2026-09-22) — Boletus-driven: its D-30 / 07 § 7 ask, the "Cancel ■" button and the real % of `Write to File` Phase C; the token object supersedes the sketch's callback-return shape |

---

## Archived intent — original DualC prose (preserved verbatim)

The text below was lifted from the topic files when the work moved to Boletus. It
records *why DualC planned each item* and the shape it expected, kept for history.
For current status and implementation, follow the Boletus links above.

### From [10 §19](10-infrastructure-and-integration.md) — C# wrapper + `.gha` (Phase 2/3)

> The C# P/Invoke wrapper that consumes the C-ABI entry points (Phase 2, separate
> project) was specified in
> [`../../capi/CSHARP_WRAPPER_HANDOFF.md`](../../capi/CSHARP_WRAPPER_HANDOFF.md).
>
> The Tier 3 integration deliverable for DualC's Rhino 8 / Grasshopper plugin: on
> top of the narrow C ABI ships a managed C# wrapper that P/Invokes it; downstream,
> a `.gha` Grasshopper plugin lives in a separate repo and consumes the C# wrapper.
>
> **Zero-copy patterns (user request).** Both directions have different
> constraints:
> - **Input mesh (C# → native).** C# can pin a managed array with
>   `fixed (float* p = verts) { … }` or `GCHandle.Alloc(…, GCHandleType.Pinned)`,
>   so the native side reads directly from managed memory without a copy. Caveat:
>   `Rhino.Geometry.Mesh` does not store vertices as a contiguous `float[]`, so the
>   wrapper still flattens Rhino mesh → `float[]` once before pinning. Investigate
>   whether RhinoCommon exposes a contiguous accessor that memoizes
>   (`Mesh.Vertices.ToFloatArray()`).
> - **Output mesh (native → C#).** Two-call pattern is the standard zero-copy
>   answer: the contour call writes pointer + length into out-params; C# wraps them
>   with `Span<T>`/`Memory<T>` via `MemoryMarshal.CreateSpan`, reads directly, then
>   releases. Avoids the `Marshal.Copy` round-trip. The buffer lifetime is
>   contractually tied to "until release is called."
> - **Constraint that may force a copy regardless.** Converting the native
>   float-array output into a `Rhino.Geometry.Mesh` requires populating
>   `Mesh.Vertices`/`Mesh.Faces`, which has its own allocation pass. Zero-copy at
>   the C ABI boundary still saves the marshalling copy but cannot remove the
>   Rhino-side construction.
>
> **Phase 2 (separate repo, separate entry).** `.gha` Grasshopper plugin built
> against the C# wrapper. Lives outside DualC for the same reason the `.gha` output
> of a typical C++/C# stack does: deployment-specific, its own release cadence
> (Rhino version tracking), doesn't belong in the library's core repo.

*Realized in Boletus:* the wrapper is `Boletus.Core` (P/Invoke + `SafeHandle` +
status→exception, **DONE** 2026-06-17, 9/9 marshaling tests); the managed
field-graph model + canonical-JSON serializer (**DONE** 2026-06-18, 70/70 tests,
`--dump-json`-faithful); the `.gha` palette + Rhino glue (**IN PROGRESS**, 3b).

### From [11 §4](11-dense-lattice-deliverable/README.md) — Rhino preview / LOD mesh

> **4. Rhino preview/LOD mesh — let Rhino *draw* a dense part.**
> Re-framed 2026-06-12: this LOD proxy is the *in-Rhino* half of the side-car model
> — Rhino draws the proxy while a separate native window raymarches the exact field
> beside it.
>
> The "Rhino mesh later" half of the decision. Since Rhino's viewport draws native
> geometry, the in-Rhino visualization of a dense lattice is a **mesh Rhino can
> render** — but the full-density contour OOMs, so it must be a *preview*: a coarse
> / decimated lattice (larger effective λ or aggressive `--collapse`), a
> region-of-interest sub-box at full detail (reuse `--bounds`), or an outer-shell
> proxy, with the full part reserved for export (STL/3MF). Targets the #19 C-ABI
> surface (flat data, two-call span). GhGL raymarch in the viewport is explicitly
> **not** this path (high coupling, re-implements the field in GLSL). *Open when
> picked up:* decimate-vs-coarsen quality; how the proxy is keyed to the exact
> field so preview and export agree.

*Realized in Boletus:* the capped `ProxyPreviewComponent` (**DONE** 2026-06-20) —
hard depth ceiling `MaxProxyDepth = 7`, contour-once-and-cache, viewport-only (no
Mesh output), OOM-proof by design. True lattice fidelity is the Phase-5 raymarch
side-car's job, not the proxy's.

### From [12 §E](12-field-graph-and-app/README.md) — Rhino side-car

> **E. Rhino side-car. [PLANNED]**
> Rhino/Grasshopper is the parametric/CAD front-end; a **separate native process**
> (the standalone app, launched by the plugin) is the fast raymarched window beside
> it. Rhino's viewport shows an LOD/boundary proxy mesh; the side-car receives the
> **field-graph over IPC** (kilobytes, not meshes) and raymarches it exactly.
> Separate process (not an in-Rhino DLL) for GL-context isolation + crash safety.
> This **shrinks the C ABI** to batch proxy + export mesh generation — it never
> feeds a real-time viewport, which de-risks it. *Open: v1 (launch + push graph +
> manual refresh) vs. fuller (camera sync, live GH-recompute push, watchdog/
> relaunch, signed dual-artifact distribution).*

> **From the block-12 sequence (item #4 at the time; item 5 after `65a97ea` renumbered 12's sequence in place):** "Rhino side-car — the plugin launches
> the standalone app and pushes the field-graph over IPC; Rhino shows an LOD proxy
> + exports via the C ABI." (Item #5, the C ABI re-scope, stayed in DualC and is
> **DONE** — see [14-c-abi/](14-c-abi/README.md).)

*In progress in Boletus:* Phase 5 — a `Live Preview` component serializes the current
`Volume` to a temp `.json` and launches the separate-process `dualc_field_view.exe`
(the DualC viewer binary, vendored). No new C ABI needed; GPU-bound, can't run
headless/CI. **5a** (component + vendored viewer + mesh→temp-OBJ materializer) landed
2026-07-03 (verified in Rhino). **5b** — the one upstream change it needs — is the
`dualc_field_view` **disk file-watch**, **DONE 2026-07-03**
([12 § D › Disk file-watch](12-field-graph-and-app/03-raymarch-app.md#disk-file-watch-auto-reload)):
Boletus rewrites the graph file each GH solve and the viewer live-reloads, no IPC.

### Discrete-GPU selection (side-car binary)

**DONE (2026-07-09, DualC-side).** The vendored `dualc_field_view.exe` auto-selects the
discrete GPU on a hybrid-graphics laptop — the mechanism and its verification:
[12 § D.2 › Discrete-GPU auto-selection](12-field-graph-and-app/04-preview-performance.md#discrete-gpu-auto-selection-hybrid-graphics-laptops).
What it means on the Boletus side:

- **Boletus gets it for free** — re-vendor the rebuilt `dualc_field_view.exe`, no
  Boletus code change.
- **A client that builds its *own* GL executable** must export the two driver symbols
  from its own `.exe` (a static lib and the C-ABI DLL cannot carry them) — not
  Boletus' situation today, which *launches* the prebuilt exe.
- **The C-ABI DLL does no GL**, so the headless contour/export path is unaffected.
*(2026-09-18: the DualC-side description that stood in both paragraphs is one copy now,
in 12; this page keeps the Boletus-side facts.)*

---

## Cross-links

- **Boletus roadmap index:** [`D:\Boletus\docs\roadmap\README.md`](file:///D:/Boletus/docs/roadmap/README.md)
- **Architecture & C-ABI contract (Boletus side):** [`01-architecture-and-contract.md`](file:///D:/Boletus/docs/roadmap/01-architecture-and-contract.md)
- **Upstream coordination (what Boletus needs from DualC):** [`07-upstream-coordination/`](file:///D:/Boletus/docs/roadmap/07-upstream-coordination/README.md)
- **DualC C-ABI record (this repo):** [14-c-abi/](14-c-abi/README.md)
- **DualC hand-over spec consumed by Boletus.Core:** [`../../capi/CSHARP_WRAPPER_HANDOFF.md`](../../capi/CSHARP_WRAPPER_HANDOFF.md)

*(2026-09-21 — link sweep after Boletus's docs restructuring.)* Boletus's roadmap 09 split its
pages into layers and reported, in its § DualC handoff
([`D:\Boletus\docs\roadmap\09-docs-layers\README.md`](file:///D:/Boletus/docs/roadmap/09-docs-layers/README.md)),
the five DualC citations it broke — Boletus's precedent is this repo's: a sibling's paths are
reported to it, not kept alive. Applied here, each re-found by content: the roadmap index
`roadmap.md` → `README.md` and `07-upstream-coordination.md` → its folder README (§ Cross-links);
the `.gha` row → the `05-phase3-grasshopper-components/` folder, its status **PARTIAL** as that
page says; the side-car row → `07/02` § 5, **DONE** 2026-07-03 (Boletus's *Live Preview*
milestone); and 12/03's § 6 pointer, the fifth row, on its own page. The two Boletus anchors
were verified with Boletus's slugifier (`scripts/check.py` `heading_slugs`), since `file:` and
out-of-repo links are not gate-checked; the four `file://` rows the sweep did not list (03, 04,
06, 01) still resolve.

---

← Back to the [Roadmap index](README.md).
