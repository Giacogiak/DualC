# Third-party code in DualC

The library proper (`libdualc`) bundles five files vendored from other projects:
four Unlicense source files (the QEF/SVD solver) and one MIT header (nanort).
All retain their original headers; all but one are byte-identical to upstream,
and that one carries a single documented local modification (see below).

## QEF / SVD solver

- `src/internal/qef.h`
- `src/internal/qef.cpp`
- `src/internal/svd.h`
- `src/internal/svd.cpp`

Origin: <https://github.com/nickgildea/DualContouringSample> by Nick Gildea.
They expose the `svd::QefSolver` class used internally by DualC's contourer.

`qef.h`, `qef.cpp` and `svd.h` are dropped in unchanged.

### Local modification to `svd.cpp`

`src/internal/svd.cpp` carries **one** deliberate divergence from upstream,
marked in the file with a `LOCAL MODIFICATION (DualC)` comment at its site and
summarised in a note at the top of the file.

`Svd::pinv()` upstream reads:

```cpp
return (fabs(x) < tol || fabs(1 / x) < tol) ? 0 : (1 / x);
```

The first clause is the intended regularisation: eigenvalues of `A^T A` below
`tol` correspond to directions the QEF does not constrain, and zeroing them
makes the solution fall back to the mass point along those directions instead
of diverging. The second clause also zeroes every eigenvalue **above** `1/tol`,
which is not regularisation and appears to be unintended.

It matters because the rows of `A` are unit normals, so
`lambda_max(A^T A) <= n`, the number of accumulated samples. With DualC's
default `ContourerParams::qefRegularization` of 0.1 the usable window was
`[0.1, 10]`:

- a per-leaf QEF accumulates at most 12 samples, so it was rarely affected;
- **adaptive collapse** (`simplifyHermiteOctree`) merges up to 8 x 12 = 96
  samples, and there the solve routinely truncated every direction and
  returned the centroid, so a sharp feature that the merged QEF could have
  placed exactly was lost.

Measured on a sharp three-plane corner at `(0.3, 0.3, 0.3)`: solved exactly for
up to 10 repetitions of the sample set, then jumping to the sample set's mass
point `(0.4333, 0.5333, 0.4333)` from 11 onwards. With the clause removed it is
exact at every repetition count, and the small-eigenvalue regularisation still
behaves (a flat cell of 12 parallel samples still resolves onto its plane).
That exact configuration is pinned by `tests/test_qef.cpp`
("QefSolver resolves a sharp corner at any sample multiplicity"), which fails
against the upstream clause.

The change is one clause; the rest of the file is upstream. Re-syncing with
upstream means re-applying it.

## nanort (BVH ray tracing)

- `src/internal/third_party/nanort/nanort.h`
- `src/internal/third_party/nanort/nanort_LICENSE.txt`

Origin: <https://github.com/lighttransport/nanort> (MIT — Light Transport
Entertainment, Inc.), the 2015-2016 single-header release, taken byte-identical
from the copy geometry-central bundled up to its `8579865` commit (its
`deps/nanort/include/nanort/` folder). It backs `src/internal/mesh_bvh.cpp`,
which builds `nanort::BVHAccel<float, TriangleMesh, TriangleSAHPred,
TriangleIntersector>` over the input mesh and walks its nodes directly
(`IntersectRayAABB`, `real3`).

Why vendored, and why this version: geometry-central v1.1.0 made nanort a
**private** dependency of its own target, moved the header and updated it to
the current upstream release, whose `BVHAccel<T>` takes one template argument
and no longer compiles `mesh_bvh.cpp`. DualC therefore owns the header it was
written against and pins the older API on purpose. Bumping to current nanort
is a port of `mesh_bvh.cpp`, not a header swap.

## Example-only host I/O deps (`examples/third_party/`)

These are vendored libraries used **only** by the example binaries (for image
and 3MF/ZIP file output, and post-contour mesh decimation). They are **never**
linked into `dualc` itself, so the library proper stays dependency-free
(mesh/file I/O and mesh post-processing are host-side concerns). Permissive
licenses, consistent with the project posture. They are compiled into small
helper libraries:

- **`miniz.{h,c}`** (MIT — Rich Geldreich et al.) — deflate ZIP container for
  3MF export; compiled into `dualc_examples_io` and PUBLIC-linked into
  `dualc_examples_common` (every CLI's `-o .3mf` uses it). Origin:
  <https://github.com/richgel999/miniz> (v3.0.2 release amalgamation). Full
  license retained at `examples/third_party/miniz_LICENSE.txt`.
- **`stb_image_write.h`** (Public Domain / MIT — Sean Barrett) — PNG output for
  the `dualc_slice` heatmap; compiled into a separate `dualc_examples_img`
  library linked **only** into `dualc_slice` and the GL plumbing's snapshot
  path (kept out of the shared lib so the mesh-only CLIs carry no PNG writer).
  Origin: <https://github.com/nothings/stb>. `stb_impl.cpp` beside it is **our
  own** one-line translation unit (it defines `STB_IMAGE_WRITE_IMPLEMENTATION`
  and includes the header), not vendored code — it is MIT with the rest of DualC.
- **`json.hpp`** (MIT — Niels Lohmann) — single-header JSON parser/serializer
  ("JSON for Modern C++", v3.11.3 release amalgamation) used to read/write the
  field-graph canonical form; compiled only into `dualc_examples_fieldgraph`
  (which backs `dualc_field`). Origin:
  <https://github.com/nlohmann/json>. Full license retained at
  `examples/third_party/json_LICENSE.txt`.
- **`meshoptimizer/` (`meshoptimizer.h`, `simplifier.cpp`, `vfetchoptimizer.cpp`)**
  (MIT — Arseny Kapoulkine) — QEM mesh decimation for `dualc_field --decimate` /
  `--simplify` (a post-contour, pre-export mesh pass); compiled into
  `dualc_examples_decimate` and PUBLIC-linked into `dualc_examples_common` (the
  `writeField` decimation path). A **minimal, verbatim** subset of the upstream
  `src/` — only the simplifier + vertex-fetch TUs plus the header; the allocator is
  header-inline in static builds so no other TU is needed. Origin:
  <https://github.com/zeux/meshoptimizer> (v1.2). Full license retained at
  `examples/third_party/meshoptimizer/meshoptimizer_LICENSE.txt`.

## Opt-in GL-target dependencies (`examples/third_party/glad/`, GLFW fetched)

Linked only into the opt-in GL binaries (`dualc_raymarch`, `dualc_field_view`,
`dualc_glsl_parity`) — never into `libdualc`. Both permissive:

- **glad** (Public Domain — generated output of <https://github.com/Dav1dde/glad>,
  generator 0.1.34, `gl=3.3` core profile, no extensions) — the OpenGL loader:
  `examples/third_party/glad/include/glad/glad.h`,
  `examples/third_party/glad/include/KHR/khrplatform.h` (Khronos platform
  types) and `examples/third_party/glad/src/glad.c`, compiled into
  `dualc_examples_glad`. Taken byte-identical from the copy Polyscope bundled;
  the licensing note travels with it as
  `examples/third_party/glad/glad_LICENSE.txt`.
- **GLFW** (zlib — <https://github.com/glfw/glfw>) — window / GL context /
  input. **Not vendored**: `CMakeLists.txt` resolves an existing `glfw` target,
  else a local tree named by `-DDUALC_GLFW_DIR`, else fetches the pinned 3.4
  release commit into the build tree, with its docs, tests, examples and install
  off. On Linux only its X11 backend is built unless `-DGLFW_BUILD_WAYLAND=ON`.

## Runtime dependencies (linked via geometry-central)

geometry-central itself (MIT — Nicholas Sharp and contributors,
<https://github.com/nmwsharp/geometry-central>) is not vendored. DualC's CMake
pins it to upstream commit `1e8e43d2d50b18c98ea0b4a53ffaf5848dbaaa26` (tag
`v1.1.0`) and fetches it into the build tree at configure unless a local tree
is named with `-DDUALC_GC_DIR` or an enclosing project already defines the
`geometry-central` target. Its public interface brings, transitively:

- **Eigen** (MPL 2.0) — linear algebra. geometry-central downloads Eigen 3.3.9
  from <https://gitlab.com/libeigen/eigen> at configure when no `Eigen3::Eigen`
  target or system Eigen is found.
- **happly** (MIT) — PLY I/O; a git submodule of geometry-central
  (`deps/happly`), so a manual clone needs `--recurse-submodules`.

geometry-central also bundles nanoflann (BSD-2-Clause) and its own, newer
nanort, but keeps both private to its target since v1.1.0; neither reaches
DualC, which uses no nanoflann and its own vendored nanort (above).

## Demo mesh assets (`data/`)

Most demo meshes are first-party: the procedural shapes (`cube`, `sphere`,
`uvsphere`, `torus`, `knot`, `genus2`, `cylinder`, `bracket`, `hexbore`) are
generated by `examples/dualc_gen_demo` and are © the DualC authors (MIT). `molde`,
`foot`, `mesh-soup`, `opA` and `opB` are likewise authored by the project owner.

- **`bunny.obj`** (in `data/`) — the *Stanford Bunny*, from the Stanford Computer
  Graphics Laboratory 3D Scanning Repository
  (<http://graphics.stanford.edu/data/3Dscanrep/>). An organic /
  dense-tessellation test mesh; the original scan is an open surface (a hole in
  the base). Like every mesh under `data/`, it is gitignored (`*.obj`) rather
  than committed — fetch it locally with, e.g.:

  ```bash
  curl -sSL -o data/bunny.obj \
    https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/stanford-bunny.obj
  ```

  Please credit the Stanford Computer Graphics Laboratory if you redistribute
  it. This asset relaxes DualC's otherwise permissive-only posture by carrying a
  request-for-acknowledgment term; it is data, not linked code, so it does not
  affect the license of DualC's source.

DualC's own code is MIT-licensed (see `LICENSE`).
