# P02 — The contourer's thread count (#34)

Sources: § 2 "Threads" and § 3 P02 of [the raw plan](../docs/raw/2026-10-19-hygiene-and-editor-plan.md).
Branch `feat/hygiene-and-editor`. Never push.

### T1. `ContourerParams::numThreads`, passed through the C ABI, `dualc_field` and the bakes

1. **Contourer.** Add `numThreads` to `ContourerParams` (`include/dualc/contourer.h:19-29`), where 0
   means all hardware threads, worded as `SamplerParams::numThreads` is (`sampler.h:32-35`). Use it
   in `src/contourer.cpp:699-725` in place of `internal::resolveThreadCount(0)`, and drop the #34
   comment at `:703`.
2. **Bakes.** `bakeToGrid` (`src/implicit/grid_field.cpp:171`) and the `MeshSource` narrow-band bake
   (`mesh_source.cpp:167,195`) take a thread-count parameter. A default argument of 0 keeps source
   compatibility. Do not use global state.
3. **C ABI.** `capi/dualc_c.cpp:183,249,355,411` copy `DualcContourParams.numThreads` into
   `ContourerParams`, as they already do into `SamplerParams`. Update the header comment at
   `capi/dualc_c.h:132`, and bump the ABI version as a patch (default 0.5.2: the field already
   exists and is now honoured). Note the change on the C ABI design page and in block 14's record.
4. **`dualc_field --threads N`.** Add the flag (`examples/dualc_field.cpp:273-374`), default 0; it
   feeds the sampler, the contourer and the per-node bakes.
   - Add its row to the flag table on the `docs/command_reference/11-dualc_field/` page that owns
     the flags, with the real default and a ready-to-run recipe (`--threads 1` for a reproducible
     timing). The gate compares that page to the parser.
   - If `dualc_field_view` bakes too, give it the same flag and update its cmd-ref page
     (`12-dualc_field_view/`).
5. **Tests.**
   - Extend the #30 threading-determinism test: the contourer at 1 thread and at N threads gives a
     byte-identical mesh.
   - Add a C ABI or Catch2 case proving that `numThreads=1` reaches the contourer.
   - Add a CLI test for `--threads`.
6. **Gate and record.** Run the full gate, `python3 scripts/check.py`. Record the ctest count in
   the handoff, and append to 17's 04 § #34 that this bullet is DONE, with the evidence.

#### Acceptance criteria

- The contourer, both bakes, the C ABI and `dualc_field` honour the thread count, and the
  determinism test pins it.
- The full gate and `--docs --strict` are green. The work is committed, not pushed.

## Exit criteria

- #34's thread bullet is DONE in 04 with its evidence, and the cmd-ref pages agree with the parser.
