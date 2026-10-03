# DualC

A small C++17 library that turns **triangle meshes** and **real-valued implicit fields**
(signed-distance functions and anything else with a sign) into clean, dual-contoured
triangle meshes — sharp features preserved, manifold output, adaptive resolution.

Two stages, joined by one data contract:

- **Sampler** — walks an adaptive octree over a mesh *or* a field and stores Hermite data
  (a sign per corner, a crossing point + normal per edge).
- **Contourer** — turns that Hermite octree into a mesh with Ju/Schaefer/Warren octree dual
  contouring: one QEF-placed vertex per cell (or per surface component, with Manifold DC)
  and optional adaptive cell collapse.

Above the engine sits a composable **implicit-field layer**: an analytic primitive catalogue,
hard and smooth booleans, decorators, domain operators, 2D lifts, and mesh / winding-number /
grid sources. Hard booleans hand the contourer the active operand's un-blended gradient, so
the QEF lands vertices exactly on the ridge — the sharp-feature preservation that marching
cubes over an SDF cannot give. Every piece is usable on its own; `dualContourMesh` and
`dualContourField` compose the whole pipeline in one call.

## Dependencies

DualC depends on [geometry-central](https://geometry-central.net) (MIT) for mesh containers,
vector math and Eigen. It is **pinned to upstream v1.1.0** and resolved at configure, first
match wins:

1. a `geometry-central` target already defined by the project that `add_subdirectory`s DualC;
2. `-DDUALC_GC_DIR=/path/to/geometry-central` — a local source tree (a sibling checkout for
   co-development; clone it with `--recurse-submodules`, it has one);
3. otherwise `FetchContent` downloads the pinned commit into the build tree — the default, so a
   bare clone configures with no flags.

A first configure therefore reaches the network up to three times: DualC → geometry-central,
geometry-central → Eigen 3.3.9 (skipped when an `Eigen3::Eigen` target or a system Eigen is
found), and, with tests on, Catch2. Nothing else is fetched; the vendored code is listed in
`THIRD_PARTY.md`.

The GL viewers (opt-in) need a [polyscope](https://github.com/nmwsharp/polyscope) checkout next
to the repo, or `-DDUALC_POLYSCOPE_DIR=/path`:

```
<parent>/
├── DualC/                <- this repo
└── polyscope/            <- optional, GL viewers only (clone with --recurse-submodules)
```

## Build

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DDUALC_BUILD_TESTS=ON -DDUALC_BUILD_EXAMPLES=ON
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

With Ninja: `-G Ninja -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build -j`. Builds and
tests on Windows (Visual Studio 2022) and Linux (GCC 15, CMake 4, Ninja; on Ubuntu
`sudo apt install cmake ninja-build g++`); the tests and examples also compile one C file
(miniz), so they need a C compiler next to the C++ one. The GL
tools and the C ABI are opt-in (`-DDUALC_BUILD_POLYSCOPE_VIEWER`, `_RAYMARCH_VIEWER`,
`_FIELD_VIEW`, `_GLSL_PARITY`, `_C_ABI`, all `OFF` by default); none of them adds a dependency
to `libdualc`. The build prelude and every flag are in the
[command reference](docs/command_reference/README.md). Before committing, run the local gate
`python scripts/check.py` — there is no CI; that command is it.

## Consume DualC from another CMake project

DualC exposes a namespaced `dualc::dualc` target. The supported pattern is source-tree
consumption:

```cmake
add_subdirectory(path/to/DualC dualc_build)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE dualc::dualc)
```

If the outer project already defines a `geometry-central` target, DualC links it; otherwise
DualC fetches its pinned version (or uses `-DDUALC_GC_DIR`). Note that geometry-central is part of
DualC's public API (`dualc::Vector3` is `geometrycentral::Vector3`; the contourer returns
geometry-central meshes), so a consumer includes its headers too. `find_package(dualc REQUIRED)`
against an installed copy is not supported;
the reason and the trigger are recorded in
[roadmap 10](docs/roadmap/10-infrastructure-and-integration.md). Native hosts that cannot
link C++ use the [C ABI](capi/README.md) (`dualc_capi`, opt-in) instead.

## Minimal usage

```cpp
#include "dualc/dualc.h"
#include "geometrycentral/surface/meshio.h"

using namespace geometrycentral::surface;

dualc::SamplerParams sp;   sp.maxDepth = 7;
dualc::ContourerParams cp;

// Mesh in, dual-contoured mesh out.
auto [mesh, geom] = readSurfaceMesh("input.obj");
auto [outMesh, outGeom, outNormals] = dualc::dualContourMesh(*mesh, *geom, sp, cp);
writeSurfaceMesh(*outMesh, *outGeom, "output.obj");

// Field in: a sphere cut out of a box, sharp edges kept by the hard boolean.
auto field = dualc::differenceOf(
    std::make_shared<dualc::BoxField>(dualc::Vector3{-1, -1, -1}, dualc::Vector3{1, 1, 1}),
    std::make_shared<dualc::SphereField>(dualc::Vector3{0, 0, 0}, 1.2));
auto [fMesh, fGeom, fNormals] = dualc::dualContourField(*field, sp, cp);
```

Mesh I/O and spatial transforms are the host's job: the library takes meshes in one global
frame and returns geometry-central meshes. The public API is `include/dualc/`; everything
under `src/internal/` is private.

## Command-line tools

Every tool lives under `examples/`, shares one set of registries (primitives, recipes,
booleans, post-ops) and chooses its export format from the `-o` extension — `.obj`, `.stl`,
`.3mf` (**1 world unit = 1 mm**). The [command reference](docs/command_reference/README.md)
has one page per tool with the full flag table and ready-to-run recipes; the one-line
version:

- [`dualc_demo`](docs/command_reference/01-dualc_demo.md) — re-mesh a triangle mesh through
  the dual contourer (sign oracles, sharp-feature mode, winding-number sealing of open scans).
- [`dualc_primitive`](docs/command_reference/02-dualc_primitive.md),
  [`dualc_boolean`](docs/command_reference/03-dualc_boolean.md),
  [`dualc_lift`](docs/command_reference/04-dualc_lift.md),
  [`dualc_csg_demo`](docs/command_reference/05-dualc_csg_demo.md) — analytic primitives
  with decorators and domain operators; hard and smooth booleans of two meshes; revolve /
  extrude of 2D profiles; baked CSG recipes.
- [`dualc_lattice`](docs/command_reference/06-dualc_lattice.md) — TPMS lattice infill
  (gyroid, Schwarz P, diamond, …) bounded by an input mesh, solid or thick-walled shell.
- [`dualc_slice`](docs/command_reference/07-dualc_slice.md) — sample a field on a cutting
  plane and write a PNG heatmap + SVG contour: inspection and slicing at a cost independent of
  lattice density.
- [`dualc_field`](docs/command_reference/11-dualc_field/README.md) — the general field-graph
  CLI: compose any node (primitives, booleans, TPMS and strut lattices, graded thickness, mesh
  sources) from JSON or a shorthand expression and contour once; streaming tiled export for
  meshes too large for RAM.
- [`dualc_field_view`](docs/command_reference/12-dualc_field_view/README.md) (opt-in) — the
  same field-graph compiled to GLSL and sphere-traced on the GPU: *the field you preview is
  the field you export*, at densities that would OOM the contourer.
- [`dualc_view`](docs/command_reference/08-dualc_view.md) (opt-in, Polyscope) and
  [`dualc_raymarch`](docs/command_reference/10-dualc_raymarch.md) (opt-in, custom GL) — the
  interactive viewer with diagnostic overlays, and the analytic TPMS raymarcher that preceded
  `dualc_field_view`.
- [`dualc_gen_demo`](docs/command_reference/09-dualc_gen_demo.md) — generates the demo
  meshes (they are gitignored; the generator is the source of truth). The build runs it into
  `build/data/` and copies the meshes next to every tool, so a clean clone needs no manual
  step; `dualc_gen_demo all --dir data` populates the repo's `data/` for running recipes from
  the root.

## Documentation

Start at [`docs/README.md`](docs/README.md): it says which folder owns which class of fact —
design, usage contract, development record, decisions, immutable inputs — and how to search
them. `STRUCTURE.md` is the file-by-file map of the codebase; `THIRD_PARTY.md` covers
vendoring and attribution; `AGENTS.md` is the entry file for coding agents.

## License

DualC is MIT-licensed (see `LICENSE`). The vendored QEF/SVD sources under `src/internal/` are
public domain (Unlicense); host-only third-party code under `examples/third_party/` is
MIT/PD. See `THIRD_PARTY.md`.

If you use DualC in a product, a publication or a derived library, please credit
**DualC by Giacomo Forcina** and link <https://github.com/Giacogiak/DualC>. This is a request,
not a license condition; `CITATION.cff` holds the citation metadata (GitHub's "Cite this
repository").
