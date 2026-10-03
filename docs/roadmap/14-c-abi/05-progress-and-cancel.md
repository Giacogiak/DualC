# Progress and cancellation (#48)

Part of [14 — C ABI](README.md): the record of tracked item **#48** — cooperative
cancellation and coarse progress through the whole pipeline (sampler, contourer,
writers) and, on top of it, the token object and the `*_with_progress` twins of the
ABI. The mechanism as it is: [design/10 § Hooks](../../design/10-invariants-and-tolerances.md#hooks-cancellation-and-progress);
the engine header: [`include/dualc/progress.h`](../../../include/dualc/progress.h);
usage for hosts: [`capi/README.md`](../../../capi/README.md).

## 48. Cooperative cancellation + coarse progress, engine to ABI

**DONE (2026-09-22)** — engine hooks, the writers, the ABI twins; ABI 0.5.0. Merged into
`main` on 2026-10-02 (`a6e11f6`, six commits, the gate re-run on the merge: 42 TUs compiled
with no DualC-origin warning, 274 tests, 0 failed).

### Why, and why now

The Grasshopper user must keep control of a heavy write: launch it deliberately and
**abort it mid-flight**. Boletus's `Write to File` (2026-07-05) runs the export on a
worker thread, but a native call cannot be interrupted from .NET, so its "abort" is
wait-then-restart and its progress is elapsed seconds. Boletus recorded the ask as its
**D-30** (DEFERRED, trigger "the wait-only abort becoming a real workflow pain"), with a
sketch of a per-tile callback whose return value cancels
(`D:\Boletus\docs\roadmap\07-upstream-coordination\03-export-callback-and-strut-sync.md` § 7).
On the DualC side there was **nothing**: no item, no decision row, no callback typedef,
and [15 § DualC retains](../15-boletus-handoff.md#dualc-retains--engine--abi-boletus-only-requests-these-upstream)
did not list it although its sibling ask (tiled 3MF, D-21) is there. The owner's
2026-09-22 session was the D-30 trigger firing; #48 is its DualC home.

### The decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Scope | the whole pipeline, not the tile loop alone | Boletus maps 3MF and monolithic STL to `dualc_field_export` — one blocking `dualContourField` — and the proxy `Contour` is atomic too; a tile-only hook would leave the Cancel button inert for those |
| Cancel is a **token object** the host sets, not the callback's return value (**D-47**) | `dualc::CancelToken`, one `std::atomic<bool>`, `request()` from any thread, sticky, one per job | *push* over *pull*: every checkpoint is a relaxed load, safe from any worker, no rate-limiting, no reverse P/Invoke in the hot path; latency is bounded by checkpoint granularity alone, including the `join()` tail; the lifetime is independent of the field handle; it maps 1:1 to `CancellationToken.Register`. The callback-return shape would have rebuilt this token inside the DLL behind a thread-affinity rule the next phase could silently break |
| Progress is a **callback on the calling thread only** | `dualc::ProgressSink::report(Stage, done, total) noexcept` | the invariant `Diagnostics` already documents — a managed delegate is never invoked from a native worker; `noexcept` makes `Cancelled` the only exception that unwinds a parallel region |
| Hook shape | two trailing defaulted pointers after `Diagnostics* diag` on every driver | the precedent; ~90 call sites compile untouched; a `RunHooks` struct only if a fourth hook ever appears |
| Stages | `Sample`, `Contour`, `Write`, `Tile` | monolithic reports the first three; tiled reports `Tile` only — forwarding the sink into every tile would restart Sample/Contour thousands of times |
| Progress inside a parallel region | `internal::parallelForPolled` — the caller spawns the workers, claims no index, and polls on a condition variable with a 100 ms timeout | exact counts, ≤ 100 ms cadence, calling-thread-only by construction, dynamic scheduling untouched. Rejected: batching the frontier into K `parallelFor` calls — each batch tail idles the workers for up to one heavy subtree on a ≤ 512-subtree frontier of wildly uneven cost |
| The **`.part` convention** (**D-46**) | every export writes `path + ".part"` and renames on success (`AtomicOutput`) | a destination file only ever appears complete: a cancel, a writer failure, an unbounded-field error and a killed process all leave at most a `.part`; a pre-existing destination survives a failed re-export. Not a Boletus requirement — Boletus asked only that "a partial file is cleaned up"; chosen here because a hooks-only temp path would be two code paths for one guarantee |
| The contourer's QEF pre-pass thread count | stays `resolveThreadCount(0)` | `ContourerParams` has no thread knob — that absence is [#34](../17-code-audit-and-hardening/04-engineering-quality.md#34-api-hygiene-batch), not this item's business |
| Deferred | a CLI Ctrl+C handler (**D-44**); mid-write checkpoints in the monolithic writers and bake-time (`bakeToGrid`, inside `dualc_field_create_*`) cancellation (**D-45**) | with `.part` a killed CLI leaves no bad destination and CTest cannot send SIGINT; the write phase is expected short next to the contour but that is *unmeasured* for monolithic 3MF, whose writer builds the model XML in RAM — the D-45 trigger is a measurement, not the premise |

Rows: [decisions](../../decisions/README.md) D-44, D-45; [settled](../../decisions/01-settled.md) D-46, D-47.

### The checkpoints and the latency bound

Where the token is polled and what each stage reports is the mechanism, held once in
[design/10 § Hooks](../../design/10-invariants-and-tolerances.md#hooks-cancellation-and-progress)
(the sampler's internal nodes, the QEF pre-pass per leaf, the contour and collapse
recursions per internal node, the writers per tile). The choice of *that* granularity is
the record's:

Worst case at depth 8: once a `maxDepth − 1` node has passed its check, its 8 leaves
build unchecked — ≤ 64 `isInside` + 96 `edgeHit` calls, microseconds on an analytic
field, ~5–10 ms on a GWN mesh source. The throwing worker sets `parallelFor`'s `failed`
flag; every other worker abandons at *its* next internal node and stops claiming; join;
rethrow on the caller. The partial octree's destruction (millions of nodes, ~50–200 ms)
then dominates, which is why a finer checkpoint would buy nothing. Checking only per
frontier subtree — 1/512 of the build, up to 32 768 leaves — would have been seconds:
rejected.

### The writers

`dce::writeField`, `writeFieldTiledStl` and `writeFieldTiled3mf` gain the two hooks and
a new **rc 3 = cancelled**; on every non-zero rc nothing is left at `path`. The tiled
driver's two error exits that used to call `sink.finish()` — a failed contour in the
single-pass fallback and a failed tile — *finalized a truncated file at the destination*;
they now abort, and the sinks gain an `abort()` rung (streams closed, own temps removed)
that a guard runs on every non-success exit **before** `AtomicOutput` removes the
`.part`, because Windows cannot unlink an open file — the order the first test run got
wrong. The CLI surface this changes is on
[command_reference/00 § The output file appears only complete](../../command_reference/00-shared-behaviour.md#the-output-file-appears-only-complete-part).

### The ABI

ABI 0.5.0, additive: `DualcCancelToken` (create / request / is_requested / destroy,
the engine token by containment), `DualcProgressFn`, `DUALC_CANCELLED` (6), the four
`DUALC_STAGE_*` values `static_assert`ed against `dualc::Stage`, and the three
`*_with_progress` twins of `contour`, `export` and `export_tiled_stl`; the older calls
are forwarders passing `NULL` (`contour → _with_diagnostics → _with_progress`, the
same for `export`), so a host built against 0.4.0 is unchanged in source and binary.
`DualcContourParams` does not grow — Boletus mirrors it as a hand-padded 80-byte
struct. The host callback is wrapped in a stack `ProgressSink` inside the call, so it
is invoked on the P/Invoke thread only; `dualc::Cancelled` is caught before
`std::exception` in every twin and the writers' rc 3 maps to `DUALC_CANCELLED`. The
tiled twin takes no `diag` (the tiled writer never tallies one — an always-zero
out-param would be a trap). Usage: [`capi/README.md` § Cancellation](../../../capi/README.md#cancellation--progress-050);
the C# shape Boletus binds, superseding its `_cb` sketch:
[`CSHARP_WRAPPER_HANDOFF.md` § 9](../../../capi/CSHARP_WRAPPER_HANDOFF.md#9-cancellation-and-progress-050).
Owed to Boletus, outside this repo: re-vendor the DLL, flip D-30, Phase C.

### Verification

- **2026-09-22, engine (commit `205aa72`)** — `parallelForPolled`: same sum as
  `parallelFor`, poll ≥ 1 and last `(n, n)`, monotonic, never on a worker (thread id),
  exceptions propagate on both paths, `n = 0` ends `(0, 0)`. Cancel: a pre-requested
  token throws before the field is queried (a counting sphere sees 0 queries) and names
  `sampling`; a request during the parallel build unwinds at 1, 2 and all threads; during
  the contourer names `contouring`; during the collapse pass names `collapsing`; the
  hand-rolled drivers honour it too. Inertness: an unrequested token plus a recording
  sink is bit-identical to no hooks at 1, 2 and all threads. Shape: per stage `done`
  monotonic, `total` constant, last `(total, total)`, Sample before Contour. Full gate:
  263 tests, 0 failed, warnings clean.
- **2026-09-22, writers** — a cancelled monolithic export returns 3 and leaves nothing
  for `.stl`, `.3mf`, `.obj`; a pre-requested token cancels before sampling; a
  pre-existing destination survives a cancelled re-export and is replaced by a successful
  one; a monolithic run reports Sample, Contour, `Write (0,1) (1,1)`; a cancelled tiled
  export leaves nothing for the STL sink (cancelled at a tile boundary and inside a tile's
  contour), the per-tile 3MF sink, the welded sink, the single-pass fallback and a
  pre-requested token; a tiled run reports `Tile` only, `(0, T)` first and `(T, T)` last
  over 6³ tiles; a hooked tiled export is byte-identical to a plain one. Full gate: 270
  tests, 0 failed, warnings clean.
- **2026-09-22, ABI** — `cli_c_abi_cancel` (`capi/dualc_c_demo.c`, compiled as C, so
  the header stays C-clean): (a) a pre-requested token → `DUALC_CANCELLED`, the mesh
  zeroed, `err` names the cancel; (b) a callback that requests the token at the second
  tile → `DUALC_CANCELLED`, no file, no `.part`; (c) `export_with_progress(NULL, NULL,
  NULL)` is byte-identical to `export`; (d) a live callback with an unrequested token
  gives the same mesh as `contour`, Sample and Contour each ending at `done == total`,
  monotonic, no Write/Tile; the token functions are no-ops on `NULL`. No threads: a
  request from inside the callback is legal and deterministic. Full gate with
  `-DDUALC_BUILD_C_ABI=ON`: the ctest total in the commit's gate line, 0 failed.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
