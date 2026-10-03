# DualC — a three-day study pack

> **Status notes moved.** The two status notes this README carried (added 2026-08-21 and
> 2026-08-31) are in [ERRATA.md](ERRATA.md), together with every reference in the pack that
> the docs restructuring of 2026-09 left dangling, each with its current target; the figures
> are indexed in [figures/README.md](figures/README.md). Read the pack for the argument; read
> [roadmap 17](../../roadmap/17-code-audit-and-hardening/README.md) for what is still true.

You commissioned this library; it was written by Claude Code. In three days you need to be
able to defend its architecture, algorithms and testing approach to **senior C++ developers
who are not graphics specialists**. This pack is built for exactly that.

Everything here was derived from **the code**, not from the repo's prose. That distinction
mattered a great deal when this was written, because `docs/ARCHITECTURE.md` then described a
version of the contourer that no longer existed — see [Before you start](#before-you-start)
for what that was and how it was resolved.

---

## What is in here

| | Document | Time | What it gets you |
| --- | --- | --- | --- |
| **A1** | [Why dual contouring](A1-why-dual-contouring.md) | 45 min | The problem, the family of solutions, why DC and not marching cubes, what Hermite data is, and the two-stage architecture in one picture |
| **A2** | [Sampling: octree, BVH and sign oracles](A2-sampling-octree-oracles.md) | 75 min | How a mesh or a field becomes a `HermiteOctree`: refinement, edge-crossing capture, the three sign methods, the BVH, and grid baking |
| **A3** | [The QEF](A3-the-qef.md) | 60 min | The mathematical heart: least squares over plane constraints, singularity and rank, the mass point, the truncated pseudo-inverse, vertex clamping |
| **A4** | [The contouring recursion](A4-contouring-recursion.md) | 90 min | `cellProc`/`faceProc`/`edgeProc`, minimal edges, mixed-depth handling, manifold DC, adaptive collapse |
| **A5** | [The implicit field algebra](A5-implicit-field-algebra.md) | 75 min | SDFs, ~30 analytic primitives plus 6 TPMS, the gradient trick that makes booleans sharp, smooth booleans, decorators, domain operators, TPMS, 2D lifts |
| **B1** | [Architecture and module boundaries](B1-architecture.md) | 45 min | Layering, public vs internal, the dependency policy, and the highest-severity bug in the library |
| **B2** | [API and type design](B2-api-and-type-design.md) | 60 min | The `ImplicitField` interface, `FieldPtr`, factories, and the polymorphism trade-off argued properly |
| **B3** | [Memory, ownership and performance](B3-memory-ownership-performance.md) | 60 min | Four ownership idioms, allocation hot spots, cache behaviour, the float/double seam |
| **B4** | [Concurrency and determinism](B4-concurrency.md) | 40 min | `parallelFor`, the bit-identical-output guarantee and how to prove it, the thread-safety contract, the exception hole |
| **B5** | [Build, dependencies and licensing](B5-build-dependencies-licensing.md) | 45 min | CMake options, the sibling-checkout decision, vendoring policy, the C ABI, and the CPU/GPU duplication problem |
| **T** | [The testing approach](T-unit-testing.md) | 90 min | The four ladders of evidence, all 182 cases mapped, what is genuinely verified, and an honest coverage ledger |
| | [**dualc-explorer.html**](dualc-explorer.html) | dip in | Six interactive panels. The 2D sandbox really solves a QEF; the numbers it reports are computed live |

Every document ends with three fixed sections, and they are the ones to lean on:

- **Key terms** — a glossary for fast revision.
- **If they ask…** — likely questions with an answer you can actually say out loud, each
  grounded in a `file:line`. At least two per document are *attacks*, answered honestly.
- **One-minute recap** — reread these on the morning.

---

## Before you start

> **Resolved — read this as history.** The five claims below were true of
> `docs/ARCHITECTURE.md` when this pack was written. This pack's corrected
> replacement was then adopted wholesale as `docs/ARCHITECTURE.md`, so none of
> them is true of the repo any more. It is left here because several documents
> in the pack refer back to it, and because "we found our own design doc
> understating the engine by two major features, and fixed it" is still the
> right thing to say out loud.

**`docs/ARCHITECTURE.md` was materially stale.** It stated, falsely, that:

- the contourer is a flat canonical-edge enumeration limited to uniform depth;
- `ContourerParams::simplificationError` is "currently ignored";
- manifold dual contouring is "not yet implemented";
- `SignMethod::PSEUDONORMAL` is a no-op;
- `ImplicitField::edgeHit` uses bisection.

All five were wrong against the shipped code. The real contourer is the full
Ju/Schaefer/Warren `cellProc`/`faceProc`/`edgeProc` recursion with correct mixed-depth
handling; adaptive collapse is wired and guarded by three topology tests; manifold DC is on
by default; all three sign methods are implemented; and `edgeHit` is Illinois-modified
regula falsi.

The current `docs/ARCHITECTURE.md` says all of that, and its own opening note records the
correction.

---

## The three-day schedule

The pack is roughly 70,000 words. You are not meant to read all of it linearly, and you are
certainly not meant to memorise it. The schedule below front-loads the material an
interviewer is most likely to probe.

### Day 1 — the algorithms

| | |
| --- | --- |
| **Morning** | **A1** then **A2**. A1 is short and is the frame for everything else. A2 is long; read §1–§7 carefully and skim §8–§11, then come back to the sign oracles and the narrow-band bake in the afternoon if there is time. |
| **Afternoon** | **A3**, then open the explorer's **QEF** panel and play with it for twenty minutes. Drag the truncation threshold past the smaller eigenvalue and watch the solution collapse to the mass point — that single interaction is worth more than rereading the section. |
| **Before you stop** | Open the explorer's **2D sandbox**. Set the shape to *sharp corner*, switch the method to *both*, and sweep the resolution. Then tick *average the normals at seams* and watch the deviation jump from 10⁻¹⁶ to 10⁻². That is the whole argument for dual contouring, in one control. |

### Day 2 — the rest of the algorithms, and the architecture

| | |
| --- | --- |
| **Morning** | **A4**. This is the densest document in the pack; give it the full ninety minutes. Then the explorer's **Octree & collapse** panel — raise `maxDepth` and notice that flat regions get exactly as many cells as curved ones, then raise `simplificationError` and watch where the adaptivity actually comes from. |
| **Afternoon** | **A5**, with the explorer's **Field algebra** panel open beside it. Switch between hard union and smooth union with the distance ramp on; you can see the field's isolines kink at the seam under a hard boolean and curve under a smooth one. Then **B1**, which is short. |

### Day 3 — the C++ and the testing

| | |
| --- | --- |
| **Morning** | **B2**, **B3**, **B4**. These are the documents a senior C++ reviewer can evaluate without knowing any geometry, so they are where you will be judged hardest on craft. B2 §4 and §6 and B4 §4 are the three highest-value passages in the pack. |
| **Afternoon** | **B5**, then **T**. T is the longest document; if time is short, read §1 (the four ladders), §4 (the honest ledger) and §7 (the questions) and skim the rest. |
| **Last hour** | Every document's **One-minute recap**, back to back, then the explorer's **Weaknesses** tab. |

---

## The seven things to have ready cold

If you retain nothing else, retain these. Each is a complete answer to a question you will
almost certainly be asked.

1. **Two stages, one contract.** The sampler is input-aware and contour-agnostic; the
   contourer is contour-aware and input-agnostic; `HermiteOctree` is the whole interface
   between them. `sampleMeshToHermiteOctree` is a three-line adapter over the field path, so
   the mesh path is a strict *subset* — one refinement algorithm to reason about, one to
   test, and a new input type never touches the contourer. (**A1**, **B1**)

2. **Why dual contouring.** DC places one vertex anywhere inside a cell rather than pinning
   vertices to grid edges, so it can put a vertex exactly at a sharp feature — which
   marching cubes structurally cannot do at any resolution. The price is that DC needs
   normals, not just signs. (**A1**)

3. **The gradient trick, and the causal chain.** Hard booleans return the *active operand's*
   un-blended gradient — the correct subgradient of `min`, not a hack. `edgeHit` takes the
   Hermite normal from `gradientAt`, so a cell straddling a seam receives two *different*
   normals, which are two independent plane constraints, and the QEF's minimiser is their
   intersection: the ridge. One line of code. (**A5**)

4. **Why the polynomial `smin`.** Because its gradient is the plain `h`-lerp of the operand
   gradients: the `∇h` term cancels *identically*, since `h = 0.5 + 0.5(b−a)/k` implies
   `a−b = k(1−2h)`. No extra evaluations, exact. That is a design decision, not a
   coincidence, and it is the best piece of maths in the codebase. (**A5**)

5. **`shared_ptr` never enters the hot loop.** Factories take `FieldPtr` by value and move
   in; nodes dereference rather than copy; the sampler takes `const ImplicitField&`. One
   atomic increment per operand at graph-construction time, zero thereafter, on any thread
   count. The "shared_ptr is slow" attack simply does not land. (**B2**)

6. **Bit-identical output regardless of thread count, and why.** Each frontier node roots a
   disjoint subtree; the frontier is built serially so its order is fixed; every node's data
   is a pure function of its own bounds and the field, so there is no reduction and no
   floating-point non-associativity; and the contourer walks the tree structurally rather
   than in completion order. For a kernel whose output becomes a physical part, that is a
   correctness property. (**B4**)

7. **How you test a mesh you cannot write down.** Four ladders of increasing strength:
   analytic ground truth where a closed form exists; exact topological invariants (Euler
   characteristic and zero boundary edges on eight procedural meshes); differential testing
   against an independently implemented oracle; and byte-identity between two code paths
   that must agree. The last is strongest because there is no tolerance left to argue about.
   (**T**)

---

## The six things to volunteer before you are asked

Raising a defect yourself, with its fix, reads as command of the codebase. Being shown one
reads as the opposite. These are real, they are all small to fix, and each is worked through
in the document named.

1. **`dualContourMesh` silently drops `SamplerParams::signMethod`** (`src/pipeline.cpp:34`
   constructs `MeshSource` with three arguments where `src/sampler.cpp:198` passes four). Ask
   for generalized winding number on triangle soup through the one-call API and you get
   3-ray parity — the one method that cannot handle that input — with no warning. No test
   catches it because every sign-method test goes through the other entry point. (**B1**)

2. **The default `cellOverlaps` encodes a Lipschitz-1 assumption that is not
   machine-checkable.** TPMS override it; the decorators that wrap them forward the
   override, each with a comment explaining why; the *domain operators* do not. The fix is
   one virtual — `lipschitzBound()` — replacing ten hand-written overrides. (**A2**, **A5**)

3. **The vendored `pinv` truncates large eigenvalues as well as small ones.** With
   `tol = 0.1`, any eigenvalue above 10 is zeroed; measured, a sharp three-plane corner
   rebuilt from more than about ten substantially-aligned samples collapses to its centroid. Per-leaf solves are safe; the collapse
   path, which merges up to 96 samples, is materially affected. (**A3**)

4. **Adaptive collapse has no tests at all.** Zero occurrences of `simplifyHermiteOctree` or
   `simplificationError` anywhere in `tests/`. Its acceptance test also thresholds the
   normal-equation residual rather than the geometric QEF energy — `QefSolver::getError()`
   is dead code. (**A4**, **T**)

5. **The six DC descent tables have no direct unit test**, and `dc_tables.h` claims they do.
   They are the classic transcription-error hot spot in every DC implementation, and their
   correctness currently rests on end-to-end Euler-characteristic tests, which is real
   evidence but localises terribly. Roughly sixty lines of combinatorics would close it.
   (**A4**, **T**)

6. **Threading determinism is documented as a guarantee and never tested.** `numThreads`
   appears nowhere in the test suite. Contour the same field at 1, 2 and 0 threads and
   compare — three lines. It is the first test you would write tomorrow. (**B4**, **T**)

---

## Notes on using the pack

- **Figures live in `figures/`** and are referenced from the documents by relative path.
  They render in VS Code's markdown preview, in GitHub, and in any browser.
- **The explorer is a single self-contained HTML file** — no build, no network, no
  dependencies. Open it directly.
- **Every `file:line` reference was checked against the source** at the time of writing. If
  the code moves, the file and function names will still find it.
- **The two dead public parameters** — `ContourerParams::weldEdges` and `SamplerParams::seed`
  — come up often enough to be worth naming here. Both are declared, documented and
  referenced nowhere in the library.
