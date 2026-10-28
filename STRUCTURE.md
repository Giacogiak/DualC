# DualC — Project Structure

Complete file-by-file map. Excludes `build/`, `.git/`, geometry-central (required;
fetched into `build/_deps/` at its pinned commit, or a local tree named by
`-DDUALC_GC_DIR`) and GLFW (optional, GL targets only; fetched at its pinned release or a
local tree named by `-DDUALC_GLFW_DIR`), which live outside this repo.

```
DualC/
├── CMakeLists.txt              Root build: defines `dualc` static lib + options, resolves geometry-central (target / DUALC_GC_DIR / FetchContent pin) and, for the GL targets, GLFW (target / DUALC_GLFW_DIR / FetchContent pin)
├── LICENSE                     MIT license (own code)
├── CITATION.cff               Citation metadata (GitHub "Cite this repository"); the credit request in README § License
├── README.md                  Front page: pitch, dependency layout, build, consume, minimal usage, tool list, doc pointer
├── THIRD_PARTY.md             Vendoring & attribution (qef/svd Unlicense; nanort MIT; Shewchuk's predicates public domain; miniz/stb/json/meshoptimizer; the geometry-central pin + its transitive deps)
├── .gitignore                 Ignores build/, *.obj demo meshes, etc.
├── .gitattributes             Pins scripts/hooks/* to LF (core.autocrlf would break the shebang)
├── .github/
│   └── workflows/
│       ├── gate.yml           Hosted CI (roadmap 20 #51): runs `scripts/check.py` and nothing else — `docs` (--docs --strict),
│       │                        `build` matrix (Ubuntu, Windows, macOS; all required; -D DUALC_WERROR=ON), `sanitize`
│       │                        (ASan+UBSan ctest, RelWithDebInfo; 17 #33), `gpu` parity under Xvfb (required)
│       └── annotate.py        Re-prints the gate's FAIL lines as error annotations (readable without auth; a job log is not)
│
├── AGENTS.md                  Agent entry file: build/test/gate commands, the five docs principles, the reading order
├── CLAUDE.md                  `@AGENTS.md` — Claude Code's import of the entry file, nothing else
├── STRUCTURE.md               This file
│
├── docs/                      Documentation, one folder per class of fact — the map is docs/README.md
│   ├── README.md              Docs entry: what lives where, the fact-class → owner table, conventions, how to search
│   ├── design/                Compiled current state: pipeline, HermiteOctree contract, sampler, contourer, the field layer, conventions, invariants and tolerances, glossary — indexed by its README
│   ├── command_reference/     Usage contract: one page (or same-numbered folder) per CLI tool — indexed by its README
│   ├── roadmap/               Append-only development record: one numbered block per topic — indexed by its README
│   └── raw/                   Immutable inputs (chat imports, screenings, plans; exempt from the size contract) — indexed by its README
│       └── study/             The verbatim code audit (A/B/T documents, explorer, figures) — indexed by its README
│
├── include/dualc/             PUBLIC API headers (installed ABI)
│   ├── dualc.h                Umbrella header (includes the rest)
│   ├── types.h                Vector3/Vector3i/BBox/Mat4, SignMethod enum, Diagnostics
│   ├── hermite_octree.h       HermiteEdge/LeafData/Node/Octree data structures
│   ├── sampler.h              SamplerParams; sampleMeshToHermiteOctree / sampleFieldToHermiteOctree
│   ├── contourer.h            ContourerParams; contourHermiteOctree / simplifyHermiteOctree
│   ├── pipeline.h             dualContourMesh / dualContourField one-call convenience
│   ├── progress.h             CancelToken / ProgressSink / Stage / Cancelled: cooperative cancellation + coarse progress (roadmap 14 #48)
│   ├── implicit.h             v2 ImplicitField base, sources, combinators, decorators, domain ops
│   ├── implicit2d.h           2D fields (Circle/Box/Segment/Polygon) + Revolve/Extrude lifts
│   ├── primitives.h           30 analytic primitive field classes (Tiers A/B/C) + 6 TPMS
│   └── porous.h               The porous shell's seeds: Pcg32, SeedParams / SeedSet, generatePorousSeeds, the ramps and ceilings (roadmap 23 #63); PorousCells / PorousMode (#64); PorousParams, PorousField (#65) and its packed() views (#66)
│
├── src/                       Core library implementation
│   ├── hermite_octree.cpp     Octree node allocation & traversal
│   ├── sampler.cpp            Adaptive octree sampling; edge-crossing capture; sign classification
│   ├── contourer.cpp          Hermite octree → mesh via QEF + Manifold DC + adaptive collapse
│   ├── pipeline.cpp           dualContourMesh / dualContourField wrappers
│   │
│   ├── implicit/              v2 field layer
│   │   ├── implicit_field.cpp Base ImplicitField defaults (isInside/edgeHit/cellOverlaps/closestSurfacePoint)
│   │   ├── mesh_source.cpp     MeshSource: SDF from BVH; narrow-band grid baking (bakeToGrid)
│   │   ├── winding_field.cpp   WindingNumberField: 0.5-level GWN (robust to non-watertight)
│   │   ├── grid_field.cpp      GridField: trilinear SDF on a baked grid
│   │   ├── combinators.cpp     union/intersection/difference/xor + smooth variants
│   │   ├── decorators.cpp      offset/round/onion/scale/elongate/transform/normalize
│   │   ├── domain_ops.cpp      mirror/repeat/repeatLimited/twist/bend/displace
│   │   ├── field2d.cpp         2D primitives + ImplicitField2D base
│   │   ├── lift.cpp            Revolve/Extrude 2D→3D
│   │   ├── primitives_tierA.cpp  Core 8 (sphere, box, roundbox, plane, capsule, cappedcylinder, torus, ellipsoid)
│   │   ├── primitives_tierB.cpp  Common ~10 (boxframe, cone, hexprism, octahedron, pyramid, solidangle, …)
│   │   ├── primitives_tierC.cpp  Long tail ~13 (cappedtorus, link, cutsphere, deathstar, vesica, rhombus, …)
│   │   ├── primitives_tpms.cpp   6 TPMS (gyroid, schwarz-p, diamond, fischer-koch, lidinoid, neovius)
│   │   ├── porous_seeds.cpp      Porous seeds: Bridson Poisson-disk (variable radius), jitter, random, the pad, Lloyd by sampling; bit-identical on any OS (roadmap 23 #63)
│   │   ├── porous_delaunay.cpp   The seeds' Delaunay tetrahedralization: Bowyer–Watson on exact predicates, Morton order, an infinite vertex for the hull, canonical tet list (roadmap 23 #64)
│   │   ├── porous_complex.cpp    The complex: the dual (faces, edges, neighbours, clamped circumcentres), the packed arrays and bins of the four cells × mode cases, the smoothed distance query and its gradient (roadmap 23 #64, #65)
│   │   └── porous.cpp            PorousField: the volume read at build time only, the packed query as the value (struts capped at B − smooth), the analytic gradient, the Lipschitz-1 argument for the default cellOverlaps (roadmap 23 #65)
│   │
│   └── internal/             Private internals (NOT public API)
│       ├── octree.{h,cpp}        Octree structure, child indexing, topology queries
│       ├── qef.{h,cpp}           VENDORED (Unlicense, Nick Gildea) — QEF solver
│       ├── svd.{h,cpp}           VENDORED (Unlicense, Nick Gildea) — 3×3 SVD for QEF
│       ├── mesh_bvh.{h,cpp}      nanort BVH wrapper: hits, closest-point, winding number, AABB overlap
│       ├── third_party/nanort/   VENDORED (MIT, Light Transport Entertainment) — nanort.h, the 2016 single-header BVH mesh_bvh is written against; nanort_LICENSE.txt beside it
│       ├── third_party/shewchuk/ VENDORED (public domain, J. R. Shewchuk) — predicates.c, exact orient3d / insphere, byte-identical; compiled only through exact_predicates.c
│       ├── exact_predicates.{h,c} The C wrapper that compiles predicates.c with every symbol renamed dualc_pred_*, and the C++ declarations (roadmap 23 #64)
│       ├── compat/sys/time.h     An empty <sys/time.h> for predicates.c on MSVC, which has none
│       ├── porous_complex.h      PorousDual, PorousComplex (the flat float / int32 arrays + bin grid the GLSL node uploads), buildPorousComplex, porousDistance / porousDistanceGradient, the shared smin and distance pieces
│       ├── porous_delaunay.h     DelaunayTets + tetrahedralize(): the tet list (sorted vertices, neighbours, -1 on the hull) the complex is derived from
│       ├── sign_oracle.{h,cpp}   inside/outside via pseudonormal / winding / generalized-winding
│       ├── dc_tables.{h,cpp}     DC corner/edge indexing + cell/face/edge descent tables
│       ├── cube_components.{h,cpp} Union-find component labeling for Manifold DC
│       ├── contourer_internals.h Contouring algorithm internals (header-only)
│       ├── distance_grid.h       Distance-grid helper struct (header-only)
│       ├── fd_gradient.h         The one central-difference stencil: default trueGradientAt + the FD gradientAt of twist/bend/displace and the lifts (header-only)
│       ├── parallel.h            Threading helpers (header-only)
│       └── seed_grid.h           Uniform point grid: O(1) insertion, near-cell iteration, exact nearest point (header-only; porous seeds, roadmap 23)
│
├── examples/                  CLI tools + host-side glue (linked to geometry-central, NOT into libdualc)
│   ├── CMakeLists.txt         Builds the example libs, the always-on CLIs and the opt-in GL targets
│   ├── example_common.{h,cpp} Shared CLI parsing, OBJ/STL/3MF writers, and the CENTRAL registries
│   │                            (primitive catalogue, TPMS kinds, boolean ops, post-ops)
│   ├── field_graph.{h,cpp}    Field-graph parser/builder: canonical JSON → FieldPtr via the registries
│   │                            (own lib dualc_examples_fieldgraph; shared by dualc_field and the C ABI in capi/)
│   ├── field_slice.{h,cpp}    Any field sampled on an axis plane → PNG heatmap + SVG contour (marching squares)
│   │                            (lib dualc_examples_slice, the one CLI lib with the PNG writer; dualc_field --slice)
│   ├── field_glsl.{h,cpp}     Field-graph → GLSL codegen: GraphNode → generated sceneSDF() + binding table; porous as 2D data textures (roadmap 23 #66)
│   │                            (lib dualc_examples_fieldglsl, no GL dep; mirrors field_graph's walk + the C++ formulas)
│   ├── raymarch_gl.{h,cpp}    Shared GL plumbing (window/compile/snapshot/orbit camera, grid + data-texture upload) for the GPU viewer + parity
│   │                            (lib dualc_examples_gl; gated on the viewer or the parity-harness flag)
│   ├── gpu_preference.h       Exports NvOptimusEnablement / AmdPowerXpressRequestHighPerformance so the GL
│   │                            exes pick the discrete GPU on hybrid laptops (Win-only; include from one TU per exe)
│   ├── demo_meshes.{h,cpp}    Procedural mesh generators (sphere, torus, knot, genus2, bracket, hexbore, …)
│   ├── dualc_boolean.cpp      7 boolean ops on two meshes (hard + smooth; optional --bake)
│   ├── dualc_gen_demo.cpp     Generate the procedural demo meshes (the build runs it into build/data/)
│   ├── dualc_field.cpp        General field-graph CLI: parse JSON graph → build → contour once → export (or --slice)
│   ├── dualc_field_view.cpp   OPT-IN GPU viewer for ANY field-graph: compile to GLSL (field_glsl) → sphere-trace; live edit
│   ├── dualc_glsl_parity.cpp  OPT-IN field→GLSL acceptance gate: GPU sceneSDF vs C++ valueAt, per node + the baked
│   │                            mesh/winding sources (sign parity + texture plumbing) (no CTest, needs GL)
│   ├── check_cli_output.cmake `cmake -P` content check behind the `*_content` CTest cases: PNG header, STL/3MF
│   │                            structure + triangle count, OBJ/SVG/JSON (roadmap 17 #32)
│   ├── dualc_io_stress.cpp    OPT-IN concurrent-export I/O harness (17 #52): N exports at once to the temp dir, every
│   │                            `.part` rename must land; keeps each `[dualc] error` line, probes the rename again,
│   │                            `--force-hold` (Windows) pins the sharing-violation case (no CTest; `check.py --io-stress`)
│   ├── dualc_mesh_deviation.cpp OPT-IN measurement tool (17 #34, #11): |valueAt| of the exact graph at every vertex of a
│   │                            contoured OBJ, the dihedral histogram, the shift against a reference OBJ (no CTest;
│   │                            `-DDUALC_BUILD_BENCH=ON`, driven by scripts/bench_e12.py)
│   ├── samples/              Ready-to-run field-graphs (gyroid_box.json, mesh_lattice.json) copied next to dualc_field,
│   │                            with the retired dualc_csg_demo's six recipes as graphs (cube_minus_sphere.json, smooth_blend.json,
│   │                            displaced_sphere.json, mesh_shell.json, mesh_minus_sphere.json, twisted_mesh.json), and three
│   │                            porous sponges (porous-sponge.json, porous-foam-bracket.json, porous-bone-bracket.json; roadmap 23 #65)
│   └── third_party/          Host-only I/O deps (never linked into libdualc)
│       ├── miniz.{c,h}          ZIP/deflate for 3MF export (MIT)
│       ├── miniz_LICENSE.txt
│       ├── stb_image_write.h    PNG output for field_slice and the GL snapshots (PD/MIT)
│       ├── stb_impl.cpp         stb implementation TU
│       ├── json.hpp             nlohmann/json single-header (MIT) for the field-graph; dualc_field only
│       ├── json_LICENSE.txt
│       ├── meshoptimizer/       QEM decimation (MIT) for --decimate/--simplify; minimal verbatim subset
│       │   ├── meshoptimizer.h        Upstream public header
│       │   ├── simplifier.cpp         The QEM simplifier TU
│       │   ├── vfetchoptimizer.cpp    Vertex-fetch optimization TU
│       │   └── meshoptimizer_LICENSE.txt
│       └── glad/                Generated OpenGL 3.3 core loader (PD) for the opt-in GL targets only
│           ├── include/glad/glad.h    The loader header (gl=3.3, core profile)
│           ├── include/KHR/khrplatform.h  Khronos platform types the header needs
│           ├── src/glad.c             The loader TU (lib `dualc_examples_glad`)
│           └── glad_LICENSE.txt
│
├── capi/                      C ABI (#19, proxy + export) — opt-in `-DDUALC_BUILD_C_ABI=ON`; host-side
│   │                            SHARED lib (links field_graph + example_common, NOT in libdualc)
│   ├── CMakeLists.txt         Builds dualc_capi SHARED + the C demo + the cli_c_abi CTest
│   ├── dualc_c.h              Public C header: opaque DualcField, flat params/mesh, the extern "C" entry points
│   ├── dualc_c.cpp            Shim: parse (JSON/--expr) → FieldGraph::build → contour/writeField; no exception escapes
│   ├── dualc_c_demo.c         C-compiled smoke test (proves the header is C-clean): build → proxy → export STL; modes mesh, inmem, cancel, threads, porous (roadmap 23 #67)
│   ├── README.md              API + build + usage for the C ABI
│   └── CSHARP_WRAPPER_HANDOFF.md  Client-side P/Invoke hand-off notes (the client itself lives in Boletus)
│
├── tests/                     Catch2 v3 unit + smoke tests (target: dualc_tests, CTest-registered)
│   ├── CMakeLists.txt         Fetches Catch2 v3.5.4; defines dualc_tests
│   ├── test_main.cpp          Shared fixtures (main() is Catch2WithMain): a listener runs each case in its own working dir
│   ├── test_octree.cpp        Hermite octree structure
│   ├── test_qef.cpp           QEF solver
│   ├── test_dc_tables.cpp     DC table correctness
│   ├── test_cube_components.cpp  Cube vertex / component labeling; all 256 sign configurations vs a region-count oracle
│   ├── test_accuracy.cpp      Vertex error vs analytic SDFs falls with depth; collapse trade-off; normal content; sharp/smooth normals (17 #32)
│   ├── test_sampler.cpp       Mesh sampling
│   ├── test_contourer.cpp     Octree contouring; the per-leaf ContourerParams knobs
│   ├── test_pipeline.cpp      Full dualContourMesh pipeline
│   ├── test_implicit.cpp      v2 field module
│   ├── test_gradient.cpp      The gradient contract (17 #34): trueGradientAt vs FD per node, normalizedOf + the Newton step use it, every field type's gradientAt is unit
│   ├── test_grid_field.cpp    Voxel grid field
│   ├── test_combinators.cpp   Boolean combinators
│   ├── test_primitives.cpp    Primitive generation
│   ├── test_domain_ops.cpp    Domain warps
│   ├── test_lift.cpp          Revolve/extrude
│   ├── test_mesh_bvh.cpp      BVH queries
│   ├── test_tpms.cpp          TPMS correctness
│   ├── test_strut_lattice.cpp Strut lattices (sc/bcc/fcc/octet) vs an independent segment-distance oracle
│   ├── test_demo_meshes.cpp   Procedural mesh topology validation
│   ├── test_parallel.cpp      parallelFor: exception propagation + the bit-identical-across-thread-counts guarantee
│   ├── test_field_graph.cpp   Field-graph parse/build/dispatch + mesh-resolver caching + located errors; the `porous` token (roadmap 23 #65)
│   ├── test_field_glsl.cpp    Field→GLSL codegen: DAG dedup + emission; the porous data textures + fallback (GL-free, so it runs under CTest)
│   ├── test_streaming_export.cpp  Tiled/streamed STL + 3MF vs the monolithic mesh
│   ├── test_diagnostics.cpp   Diagnostics channel: every reported degradation shown firing (roadmap 17 #26)
│   ├── test_validation.cpp    Argument validation + the BBox::empty sentinel (roadmap 17 #34)
│   ├── test_cancel_progress.cpp  CancelToken unwinds every driver; hooks are inert; report shape; the writers' rc 3 + `.part` guarantee (roadmap 14 #48)
│   ├── test_porous.cpp        Porous seeds (roadmap 23 #63): PCG32 reference stream, ramps, determinism, Poisson distance, graded density, pad, relax, cancel, ceilings; the Delaunay (#64): Euler and the convex hull, empty sphere, the regular grid, a pinned tet-list digest, errors; the complex (#64): dual adjacency, the bin query bit-equal to brute force in the four cases, pinned digests, errors and cancel; the field (#65): 1-Lipschitz, the smoothing bound, the analytic gradient, a mesh volume, determinism and errors; hidden `[porous-bench]` rate and size cases
│   └── test_writers_io.cpp    The writers' failure surface (17 #52): a failed open names the OS error through dce::lastError(); on
│                                Windows a held `.part` is renamed after retries, a long hold fails with the text, a child started
│                                with handle inheritance mid-export inherits nothing (output handles are non-inheritable)
│
├── cmake/
│   └── DualCWarnings.cmake    dualc_target_options(): C++17 dialect, warning level, DUALC_WERROR, DUALC_SANITIZE — per DualC target, never global (17 #33)
│
├── scripts/
│   ├── check.py               THE GATE — every check in one command; hosted CI (.github/workflows/gate.yml) runs exactly this.
│   │                            `--fast` = docs/hygiene, ~1s; default adds configure+build+ctest; `--gpu` opt-in
│   ├── check_data.json        Its declared exceptions: size baseline, index roots, forbidden patterns, summarized prefixes
│   ├── check_fixtures/        `check.py --selftest` trees: one pass/ and one fail/ miniature repo per fast check
│   ├── hooks/
│   │   └── pre-commit         sh wrapper running `check.py --fast`; enable with
│   │                            `git config core.hooksPath scripts/hooks`
│   ├── docs_loss_audit.py     Docs loss audit between two commits: sentences, evidence tokens, headings,
│   │                            identifiers of the base tree looked for on the head tree; exit 0 = clean
│   ├── docs_loss_audit.json   Its triage: verdict per residual (record citation, restoring commit), rules, ratchet
│   ├── bench_e12.py           The GridField-normal measurement round (17 #34): cases A–E through an old and a new
│   │                            dualc_field, /usr/bin/time ×3, dualc_mesh_deviation on each output, the record's three tables
│   └── count_nm.py            Dev utility (non-manifold / count analysis)
│
├── data/                      Demo meshes for root-run recipes — *.obj are GITIGNORED; the build generates its own copies under build/data/; `dualc_gen_demo all --dir data` populates this one
│                                (cube, sphere, uvsphere, torus, knot, genus2, cylinder, bracket, hexbore, bunny, molde, opA, opB)
│
└── .claude/
    ├── settings.json          Project hooks: Stop → `scripts/check.py --docs --hook` (the docs gate after every turn)
    └── settings.local.json    Project-local permitted Bash commands
```

## Build targets at a glance

- `dualc` (`dualc::dualc`) — core static library; PUBLIC-links geometry-central
  (→ Eigen, happly transitively); nanort is its own vendored header, and Shewchuk's
  `predicates.c` its one C source (C90, so the library needs a C compiler).
- `dualc_demo_data` — runs `dualc_gen_demo all` into `build/data/`; every mesh-consuming
  binary depends on it and copies the meshes next to itself (`dualc_copy_demo_meshes()`).
- `dualc_tests` — Catch2 test executable (CTest).
- `dualc_examples_io`, `dualc_examples_img`, `dualc_examples_common`,
  `dualc_examples_fieldgraph`, `dualc_examples_slice`, `dualc_demo_meshes` — example support
  static libs.
- `dualc_examples_fieldglsl` (codegen, no GL) and `dualc_examples_gl` (GL plumbing,
  opt-in) — the field→GLSL support libs.
- The always-on CLI executables: `dualc_field`, `dualc_boolean`, `dualc_gen_demo` (seven
  more retired 2026-10-16, roadmap 21 #60).
- Opt-in, each behind its own `DUALC_BUILD_*` option and needing a GL window:
  `dualc_field_view`, `dualc_glsl_parity` (with `dualc_examples_glad`, the
  vendored loader they share).
- `dualc_capi` (`dualc::capi`, shared) + `dualc_c_demo` — the C ABI, opt-in
  (`-DDUALC_BUILD_C_ABI=ON`).
