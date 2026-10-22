# --- v2 implicit-field CLI tools ----------------------------------------

# Host-only file-I/O deps for the examples (PNG + 3MF/ZIP). These are vendored
# under third_party/ and linked ONLY into example binaries -- libdualc never
# sees them, so the library proper stays dependency-free (library-scope rule).
# They are split into TWO libs on purpose:
#   miniz.{h,c}        -- MIT (deflate ZIP for 3MF). Needed by example_common's
#                         writeField, so PUBLIC-linked into dualc_examples_common
#                         (every CLI's -o .3mf path uses it).
#   stb_image_write.h  -- Public Domain (PNG output). Used only by the slice
#                         module (dualc_field --slice) and the GL
#                         plumbing's snapshot path, so it lives in its OWN lib
#                         and is NOT pulled into dualc_examples_common: the
#                         mesh-only CLIs never carry a PNG writer.
add_library(dualc_examples_io STATIC third_party/miniz.c)
target_include_directories(dualc_examples_io
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_features(dualc_examples_io PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_io VENDORED)

add_library(dualc_examples_img STATIC third_party/stb_impl.cpp)
target_include_directories(dualc_examples_img
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_features(dualc_examples_img PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_img VENDORED)

# meshoptimizer (MIT) -- QEM mesh decimation for dualc_field --decimate/--simplify
# (a post-contour, pre-export mesh pass in example_common's writeField). A minimal
# verbatim subset of upstream src/ (simplifier + vertex-fetch TUs + header; the
# allocator is header-inline in static builds). Its own lib, PUBLIC-linked into
# dualc_examples_common -- host-side only, never linked into libdualc.
add_library(dualc_examples_decimate STATIC
  third_party/meshoptimizer/simplifier.cpp
  third_party/meshoptimizer/vfetchoptimizer.cpp)
target_include_directories(dualc_examples_decimate
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer)
target_compile_features(dualc_examples_decimate PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_decimate VENDORED)

# These are large third-party amalgamations; silence their warnings and the
# MSVC CRT-secure noise so they don't drown the project's own diagnostics.
if(MSVC)
  target_compile_definitions(dualc_examples_io  PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(dualc_examples_img PRIVATE _CRT_SECURE_NO_WARNINGS)
  set_source_files_properties(third_party/miniz.c third_party/stb_impl.cpp
    third_party/meshoptimizer/simplifier.cpp
    third_party/meshoptimizer/vfetchoptimizer.cpp
    PROPERTIES COMPILE_OPTIONS "/W0")
else()
  set_source_files_properties(third_party/miniz.c third_party/stb_impl.cpp
    third_party/meshoptimizer/simplifier.cpp
    third_party/meshoptimizer/vfetchoptimizer.cpp
    PROPERTIES COMPILE_OPTIONS "-w")
endif()

# Shared glue: field-to-mesh writers (OBJ/STL/3MF), arg parsing, the bump
# library. PUBLIC-links miniz so every CLI's writeField gains STL/3MF, and
# meshoptimizer so writeField's --decimate/--simplify path is available; stb
# (PNG) is deliberately NOT here -- only the slice module links it (see above).
add_library(dualc_examples_common STATIC example_common.cpp)
target_link_libraries(dualc_examples_common
  PUBLIC dualc::dualc dualc_examples_io dualc_examples_decimate)
target_compile_features(dualc_examples_common PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_common)
target_include_directories(dualc_examples_common
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# dualc_boolean: two meshes through one boolean (kept beside dualc_field, D-60).
add_executable(dualc_boolean dualc_boolean.cpp)
target_link_libraries(dualc_boolean PRIVATE dualc::dualc dualc_examples_common)
target_compile_features(dualc_boolean PRIVATE cxx_std_17)
dualc_target_options(dualc_boolean)

# The slice module (field_slice.{h,cpp}, roadmap 21 #59): any field sampled on
# a plane -> PNG heatmap + SVG contour. It is the only CLI code that writes PNG,
# so it alone carries the stb impl; dualc_field --slice is its caller.
add_library(dualc_examples_slice STATIC field_slice.cpp)
target_link_libraries(dualc_examples_slice
  PUBLIC dualc::dualc dualc_examples_common dualc_examples_img)
target_compile_features(dualc_examples_slice PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_slice)

# --- Field-graph parser/builder + the general dualc_field CLI ------------
# field_graph.{h,cpp} parses the composable field-graph (canonical JSON) and
# lowers it to a dualc::FieldPtr by reusing the example_common registries +
# implicit.h factories. It is its own small lib so the CLI, a future raymarch
# side-car, and an eventual C ABI share ONE parser. It vendors nlohmann/json
# (host-only, MIT) under third_party/ -- like miniz/stb, never linked into
# libdualc. The header's warnings are pragma-suppressed at its single include
# site in field_graph.cpp, which builds at the project warning level like every
# DualC target (dualc_target_options, cmake/DualCWarnings.cmake).
add_library(dualc_examples_fieldgraph STATIC field_graph.cpp)
target_link_libraries(dualc_examples_fieldgraph PUBLIC dualc::dualc dualc_examples_common)
target_compile_features(dualc_examples_fieldgraph PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_fieldgraph)
if(MSVC)
  target_compile_definitions(dualc_examples_fieldgraph PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(dualc_field dualc_field.cpp)
target_link_libraries(dualc_field PRIVATE dualc::dualc dualc_examples_fieldgraph
  dualc_examples_slice)
target_compile_features(dualc_field PRIVATE cxx_std_17)
dualc_target_options(dualc_field)

# --- Field-graph -> GLSL codegen (host-side; NO GL dependency) -----------
# field_glsl.{h,cpp} compiles a GraphNode tree to a generated sceneSDF() + a
# binding table, reusing the example_common registries / field_graph param
# semantics (the same source of truth the contour path uses). It carries no GL
# dependency -- the GL apps below assemble its output -- so it always builds,
# catching codegen regressions even in a GL-less configuration.
add_library(dualc_examples_fieldglsl STATIC field_glsl.cpp)
target_link_libraries(dualc_examples_fieldglsl PUBLIC dualc::dualc dualc_examples_fieldgraph)
target_compile_features(dualc_examples_fieldglsl PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_fieldglsl)

# Copy the sample field-graphs next to dualc_field so the smoke tests (and a
# curious user) can run them by bare filename.
set(FIELD_GRAPH_SAMPLES gyroid_box.json mesh_lattice.json
    cube_minus_sphere.json smooth_blend.json displaced_sphere.json
    mesh_shell.json mesh_minus_sphere.json twisted_mesh.json
    porous-sponge.json porous-foam-bracket.json porous-bone-bracket.json)
foreach(sample ${FIELD_GRAPH_SAMPLES})
  add_custom_command(TARGET dualc_field POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/${sample}
            $<TARGET_FILE_DIR:dualc_field>/${sample})
endforeach()

# --- Procedural demo-mesh generators (Tier 3 #12) -----------------------
# A small static lib of mesh builders (sphere/torus/knot/.../bracket) reused
# by the generator CLI and the topology test in tests/. Mesh generation is a
# host-side concern, so it lives here, not in libdualc.
add_library(dualc_demo_meshes STATIC demo_meshes.cpp)
target_link_libraries(dualc_demo_meshes PUBLIC dualc::dualc)
target_compile_features(dualc_demo_meshes PUBLIC cxx_std_17)
dualc_target_options(dualc_demo_meshes)
target_include_directories(dualc_demo_meshes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(dualc_gen_demo dualc_gen_demo.cpp)
target_link_libraries(dualc_gen_demo PRIVATE dualc::dualc dualc_demo_meshes)
target_compile_features(dualc_gen_demo PRIVATE cxx_std_17)
dualc_target_options(dualc_gen_demo)

# --- GL targets (opt-in) -------------------------------------------------
# The two GL targets below (dualc_field_view, dualc_glsl_parity) share one
# dependency pair, both permissive and both linked into these binaries
# only -- libdualc is untouched:
#   glad  -- the generated OpenGL 3.3 core loader (Public Domain), vendored under
#            third_party/glad as its own lib: one C file + two headers, built
#            like miniz above and warning-silenced the same way.
#   glfw  -- window / context / input (zlib), resolved by the root CMakeLists
#            (an existing target, DUALC_GLFW_DIR, or the pinned FetchContent).
# Each needs a GL window, so none has a CTest; dualc_field_view's --snapshot and
# the parity harness's hidden window are the scripted paths.
if(DUALC_BUILD_FIELD_VIEW OR DUALC_BUILD_GLSL_PARITY)
  add_library(dualc_examples_glad STATIC third_party/glad/src/glad.c)
  dualc_target_options(dualc_examples_glad VENDORED)
  target_include_directories(dualc_examples_glad
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include)
  if(UNIX AND NOT APPLE)
    # glad's loader dlopen()s libGL on Linux.
    target_link_libraries(dualc_examples_glad PUBLIC ${CMAKE_DL_LIBS})
  endif()
  if(MSVC)
    set_source_files_properties(third_party/glad/src/glad.c PROPERTIES COMPILE_OPTIONS "/W0")
  else()
    set_source_files_properties(third_party/glad/src/glad.c PROPERTIES COMPILE_OPTIONS "-w")
  endif()

  # Shared GL plumbing (window/context, shader compile/link, uniform setters,
  # orbit camera, headless PNG snapshot) for the GPU viewer and the parity
  # harness. PUBLIC-links glfw + glad + stb (the PNG writer, for the snapshot).
  add_library(dualc_examples_gl STATIC raymarch_gl.cpp)
  target_link_libraries(dualc_examples_gl
    PUBLIC dualc::dualc dualc_examples_img glfw dualc_examples_glad)
  target_compile_features(dualc_examples_gl PUBLIC cxx_std_17)
  dualc_target_options(dualc_examples_gl)
  target_include_directories(dualc_examples_gl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# dualc_field_view sphere-traces ANY field-graph per pixel (cost O(pixels),
# independent of lattice density): it compiles the graph to GLSL
# (dualc_examples_fieldglsl), so the previewed field is the exported field. Opt-in; needs a GL window (no CTest -- a hidden
# --snapshot path renders one frame for scripted verification).
if(DUALC_BUILD_FIELD_VIEW)
  add_executable(dualc_field_view dualc_field_view.cpp)
  target_link_libraries(dualc_field_view PRIVATE dualc::dualc dualc_examples_common
    dualc_examples_fieldgraph dualc_examples_fieldglsl dualc_examples_gl)
  target_compile_features(dualc_field_view PRIVATE cxx_std_17)
  dualc_target_options(dualc_field_view)
endif()

# dualc_glsl_parity is the field->GLSL acceptance gate: per slice node it renders
# sceneSDF over a lattice on the GPU and compares to the C++ field. Needs a GL
# context (a hidden window). No CTest: `check.py --gpu` is its one runner (CI's
# required gpu job), because it asserts the announced case count and a ctest
# exit code cannot -- cases whose demo mesh is missing skip and still exit 0.
if(DUALC_BUILD_GLSL_PARITY)
  add_executable(dualc_glsl_parity dualc_glsl_parity.cpp)
  target_link_libraries(dualc_glsl_parity PRIVATE dualc::dualc
    dualc_examples_fieldgraph dualc_examples_fieldglsl dualc_examples_gl)
  target_compile_features(dualc_glsl_parity PRIVATE cxx_std_17)
  dualc_target_options(dualc_glsl_parity)
endif()

# dualc_io_stress is the concurrent-export I/O harness (roadmap 17 #52): N
# exports at once to the temp directory, every `.part` rename must land, the
# `[dualc] error` line of any failure kept and the rename probed again. Opt-in
# and nondeterministic by design, so no CTest: `check.py --io-stress` is its
# one runner (an experiment job on Windows, never part of the gate's green).
if(DUALC_BUILD_IO_STRESS)
  add_executable(dualc_io_stress dualc_io_stress.cpp)
  target_link_libraries(dualc_io_stress PRIVATE dualc::dualc dualc_examples_fieldgraph)
  target_compile_features(dualc_io_stress PRIVATE cxx_std_17)
  dualc_target_options(dualc_io_stress)
endif()

# Demo meshes are generated, not checked in: `*.obj` is gitignored and
# dualc_gen_demo (built right here) is the version-controlled source. One
# custom command runs it once per build into <build>/data, and every
# mesh-consuming binary depends on that target and copies the meshes next to
# itself so tools, recipes and CLI smoke tests find them by bare filename. A
# clean clone therefore builds and tests with no manual step; the checkout's
# own data/ is only for running recipes from the repo root
# (`dualc_gen_demo all --dir data`).
set(DEMO_MESHES
  cube.obj sphere.obj uvsphere.obj torus.obj knot.obj genus2.obj
  cylinder.obj bracket.obj hexbore.obj)
set(DUALC_DEMO_DATA_DIR ${PROJECT_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${DUALC_DEMO_DATA_DIR})
set(DUALC_DEMO_DATA_FILES)
foreach(meshfile ${DEMO_MESHES})
  list(APPEND DUALC_DEMO_DATA_FILES ${DUALC_DEMO_DATA_DIR}/${meshfile})
endforeach()
add_custom_command(OUTPUT ${DUALC_DEMO_DATA_FILES}
  COMMAND dualc_gen_demo all --dir ${DUALC_DEMO_DATA_DIR}
  DEPENDS dualc_gen_demo
  COMMENT "Generating the demo meshes into ${DUALC_DEMO_DATA_DIR}"
  VERBATIM)
add_custom_target(dualc_demo_data DEPENDS ${DUALC_DEMO_DATA_FILES})
# Global properties, not directory variables: the helper below is also called
# from capi/, a sibling directory that does not see this file's variables.
set_property(GLOBAL PROPERTY DUALC_DEMO_MESHES "${DEMO_MESHES}")
set_property(GLOBAL PROPERTY DUALC_DEMO_DATA_DIR "${DUALC_DEMO_DATA_DIR}")

# dualc_copy_demo_meshes(<target>): make <target> build after the meshes exist
# and copy them into its output directory. Used by every CLI below and by the
# C ABI demo in capi/ (call it from the directory that defines <target>).
# The copy is a POST_BUILD step, so the meshes are also link dependencies: a
# regenerated mesh relinks <target> and the copy follows (Ninja and Makefiles;
# the Visual Studio generators ignore LINK_DEPENDS -- a clean build is fresh).
function(dualc_copy_demo_meshes target)
  get_property(meshes GLOBAL PROPERTY DUALC_DEMO_MESHES)
  get_property(datadir GLOBAL PROPERTY DUALC_DEMO_DATA_DIR)
  add_dependencies(${target} dualc_demo_data)
  foreach(meshfile ${meshes})
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
                 ${datadir}/${meshfile})
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${datadir}/${meshfile}
              $<TARGET_FILE_DIR:${target}>/${meshfile})
  endforeach()
endfunction()

foreach(bin dualc_boolean dualc_field)
  dualc_copy_demo_meshes(${bin})
endforeach()

# The field-graph viewer (gated) loads mesh sources + the sample field-graphs by
# bare filename, so it gets both the demo meshes and the sample graphs next to it.
if(DUALC_BUILD_FIELD_VIEW)
  dualc_copy_demo_meshes(dualc_field_view)
  foreach(sample ${FIELD_GRAPH_SAMPLES})
    add_custom_command(TARGET dualc_field_view POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${CMAKE_CURRENT_SOURCE_DIR}/samples/${sample}
              $<TARGET_FILE_DIR:dualc_field_view>/${sample})
  endforeach()
endif()

# The parity gate (gated) resolves its mesh/winding cases by bare filename too.
# It shares an output directory with the always-on tools above, so cube.obj is
# usually already there -- but only as a side effect of building one of them.
# Copy it here as well so `cmake --build . --target dualc_glsl_parity` on a clean
# tree still runs the baked-source cases instead of silently skipping them.
if(DUALC_BUILD_GLSL_PARITY)
  dualc_copy_demo_meshes(dualc_glsl_parity)
endif()

# --- CLI smoke tests ------------------------------------------------------
# Registered with ctest so the example binaries are exercised alongside the
# unit tests. One invocation per path the retired single-purpose tools used to
# cover (roadmap 21 #60), now through dualc_field: a mesh in (smooth, sharp,
# the GWN sign, a soup through winding), a primitive + post-ops, an infinite
# primitive with --bounds, a mesh boolean (plain and baked), both 2D lifts,
# the six composition samples, a lattice to OBJ / STL / 3MF and a slice. Each
# test checks the exit code; the `<test>_content` tests registered after them
# check what was written (check_cli_output.cmake, roadmap 17 #32).
if(DUALC_BUILD_TESTS)
  # A primitive through the post-op chain (onion decorator + twist domain op):
  #  - the onion wall (0.35) is kept comfortably thicker than the depth-6 cell
  #    so the shell resolves into a clean manifold (a thin wall under-resolved
  #    at low depth produces non-manifold junk -- one-crossing-per-edge limit);
  #  - the twist is about x, NOT the torus's y symmetry axis -- a twist about a
  #    body-of-revolution's own axis is a no-op.
  add_test(NAME cli_field_primitive
    COMMAND dualc_field --expr "twist(onion(torus(0,0,0,2,0.5),thickness=0.35),radiansPerUnit=0.4,axis=x)"
            --depth 6 -o cli_field_primitive.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # An unbounded primitive is contoured only inside an explicit --bounds box.
  add_test(NAME cli_field_infinite
    COMMAND dualc_field --expr "plane(0,1,0,0)" --bounds -2,-2,-2,2,2,2
            --depth 4 -o cli_field_infinite.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_boolean
    COMMAND dualc_boolean smooth-union cube.obj cube.obj --k 0.3
            --depth 5 -o cli_boolean.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_boolean>)
  # The same boolean as a graph; cli_field_boolean_same holds it byte-identical
  # to cli_boolean's. The baked twin samples each operand from a 64^3 grid.
  add_test(NAME cli_field_boolean
    COMMAND dualc_field --expr "smooth-union(mesh(path=\"cube.obj\"),mesh(path=\"cube.obj\"),k=0.3)"
            --depth 5 -o cli_field_boolean.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_boolean_bake
    COMMAND dualc_field --expr "smooth-union(mesh(path=\"cube.obj\",bake=64),mesh(path=\"cube.obj\",bake=64),k=0.3)"
            --depth 5 -o cli_field_boolean_bake.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # Field-graph CLI: an all-analytic composed graph (sphere carved out of a
  # box-bounded gyroid shell) to OBJ, the same to binary STL (exercises the -o
  # extension dispatch), and a graph with a mesh source (cube.obj) clipped by a
  # gyroid shell -- the field-level lattice-in-volume that replaces the mesh
  # boolean round-trip.
  add_test(NAME cli_field
    COMMAND dualc_field gyroid_box.json --depth 5 -o cli_field.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_stl
    COMMAND dualc_field gyroid_box.json --depth 5 -o cli_field.stl
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # Tiled / streaming STL export (--tile-depth): bounded-memory contour of the
  # same gyroid box. The tiled==monolithic correctness bar is the unit test
  # test_streaming_export.cpp; the content check here is the STL's structure.
  add_test(NAME cli_field_tiled_stl
    COMMAND dualc_field gyroid_box.json --depth 6 --tile-depth 4 -o cli_field_tiled.stl
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_mesh
    COMMAND dualc_field mesh_lattice.json --depth 5 -o cli_field_mesh.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # Text shorthand (--expr) end-to-end to OBJ, and --dump-json.
  add_test(NAME cli_field_expr
    COMMAND dualc_field --expr "difference(intersection(box(min=[-2,-2,-2],max=[2,2,2]),onion(normalize(gyroid(wavelength=1)),thickness=0.3)),sphere(radius=1))" --depth 5 -o cli_field_expr.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_dump_json
    COMMAND dualc_field gyroid_box.json --dump-json
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # Strut-lattice source: an octet-truss wireframe clipped to the sample cube
  # (mesh-fill), exercising the sc/bcc/fcc/octet crystal factory end-to-end
  # through the field-graph. The tiled==explicit-union correctness bar is the
  # unit test test_strut_lattice.cpp.
  add_test(NAME cli_field_strut
    COMMAND dualc_field --expr "intersection(mesh(path=\"cube.obj\"),octet(wavelength=0.5,radius=0.05))" --depth 5 -o cli_field_strut.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # The 2D layer in the graph (roadmap 21 #58): a circle revolved into a torus
  # and a unit-square polygon extruded into a prism.
  add_test(NAME cli_field_lift
    COMMAND dualc_field --expr "revolve(circle(0,0,0.5),offset=2)" --depth 5 -o cli_field_lift.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_extrude
    COMMAND dualc_field --expr "extrude(polygon(0,0,1,0,1,1,0,1),height=0.5)" --depth 5 -o cli_field_extrude.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # Any graph sliced (roadmap 21 #59): a gyroid shell clipped to cube.obj, over
  # cube.obj's AABB padded 5% per axis. cli_field_slice_refuse: --slice writes
  # no mesh, so a mesh flag beside it is refused.
  add_test(NAME cli_field_slice
    COMMAND dualc_field --expr "intersection(onion(gyroid(center=[0,0,0],wavelength=0.5),thickness=0.1),mesh(path=\"cube.obj\"))"
            --slice z=0 --res 128 --bounds -0.55,-0.55,-0.55,0.55,0.55,0.55
            -o cli_field_slice.png
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # The six composition samples (roadmap 21 #60, P05), committed graph files;
  # the mesh ones read cube.obj.
  set(DUALC_FIELD_SAMPLES cube_minus_sphere smooth_blend displaced_sphere
      mesh_shell mesh_minus_sphere twisted_mesh)
  foreach(sample ${DUALC_FIELD_SAMPLES})
    add_test(NAME cli_field_sample_${sample}
      COMMAND dualc_field ${sample}.json --depth 5 -o cli_field_${sample}.obj
      WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  endforeach()
  # The three porous samples (roadmap 23 #65, P04 T2): a graded sponge in the
  # unit sphere, a foam and a strut bone in the bracket. Depth 5 is a smoke run
  # (under 0.2 GB); their measured depth-7 runs are 23/06's.
  foreach(sample porous-sponge porous-foam-bracket porous-bone-bracket)
    add_test(NAME cli_field_sample_${sample}
      COMMAND dualc_field ${sample}.json --depth 5 -o cli_field_${sample}.obj
      WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  endforeach()
  # A gyroid lattice infill inside the sample cube (the wavelength-0.5 gyroid
  # is ~four cells across the 2-unit cube; the 0.1 onion a thick double-sheet
  # shell that resolves at depth 5), to OBJ and through the extension-dispatched
  # writer to the two manufacturing-facing formats, binary STL and 3MF-mesh.
  # Their content checks hold both to cli_field_lattice.obj's face count.
  foreach(ext obj stl 3mf)
    set(name cli_field_lattice_${ext})
    if(ext STREQUAL "obj")
      set(name cli_field_lattice)
    endif()
    add_test(NAME ${name}
      COMMAND dualc_field --expr "intersection(onion(gyroid(wavelength=0.5),thickness=0.1),mesh(path=\"cube.obj\"))"
              --depth 5 -o cli_field_lattice.${ext}
      WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  endforeach()
  # --threads 1 on the baked boolean: the per-node bakes, the sampler and the
  # contourer all run serially, and cli_field_threads_same holds the mesh
  # byte-identical to cli_field_boolean_bake's at all hardware threads (#34).
  add_test(NAME cli_field_threads
    COMMAND dualc_field --expr "smooth-union(mesh(path=\"cube.obj\",bake=64),mesh(path=\"cube.obj\",bake=64),k=0.3)"
            --depth 5 --threads 1 -o cli_field_threads.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_threads_bad
    COMMAND dualc_field --expr "sphere(radius=1)" --threads -1)
  set_tests_properties(cli_field_threads_bad PROPERTIES
    PASS_REGULAR_EXPRESSION "--threads expects a whole number 0..1024")
  add_test(NAME cli_field_slice_refuse
    COMMAND dualc_field --expr "sphere(radius=1)" --slice z=0 --depth 5)
  set_tests_properties(cli_field_slice_refuse PROPERTIES
    PASS_REGULAR_EXPRESSION "--slice writes no mesh. it cannot be combined with --depth")
  # Demo-mesh generator + a few pipeline runs over the new meshes. The sphere
  # and torus exercise smooth/genus regimes; the bracket run exercises the
  # sharp-feature normals on a part with a reentrant edge; the sphere again
  # under the generalized-winding-number sign oracle.
  # cli_gen_demo writes into a directory of its own: run in the tools' shared
  # output directory it rewrote cube.obj, sphere.obj, ... while the tests
  # above and below read them, a race under `ctest -j` (roadmap 17 #32).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_out)
  add_test(NAME cli_gen_demo
    COMMAND dualc_gen_demo all
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_out)
  add_test(NAME cli_field_mesh_sphere
    COMMAND dualc_field --expr "mesh(path=\"sphere.obj\")" --depth 6 -o cli_field_mesh_sphere.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_mesh_torus
    COMMAND dualc_field --expr "mesh(path=\"torus.obj\")" --depth 6 -o cli_field_mesh_torus.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_mesh_sharp
    COMMAND dualc_field --expr "mesh(path=\"bracket.obj\",normals=sharp)" --depth 6
            -o cli_field_mesh_sharp.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  add_test(NAME cli_field_mesh_gwn
    COMMAND dualc_field --expr "mesh(path=\"sphere.obj\",sign=gwn)" --depth 6 -o cli_field_mesh_gwn.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  # The soup workflow: two overlapping boxes, each missing its +y face, sealed
  # by the winding-number source into one closed solid -- a "not closed"
  # warning fails the test. The soup is written here (`*.obj` is gitignored).
  set(cli_soup ${CMAKE_CURRENT_BINARY_DIR}/cli_soup.obj)
  file(WRITE ${cli_soup} "# two overlapping open boxes, each without its +y face
v -1 -1 -1\nv 0.5 -1 -1\nv 0.5 0.5 -1\nv -1 0.5 -1
v -1 -1 0.5\nv 0.5 -1 0.5\nv 0.5 0.5 0.5\nv -1 0.5 0.5
v -0.5 -0.5 -0.5\nv 1 -0.5 -0.5\nv 1 1 -0.5\nv -0.5 1 -0.5
v -0.5 -0.5 1\nv 1 -0.5 1\nv 1 1 1\nv -0.5 1 1
f 1 3 2\nf 1 4 3\nf 5 6 7\nf 5 7 8\nf 1 2 6\nf 1 6 5\nf 2 3 7\nf 2 7 6\nf 4 1 5\nf 4 5 8
f 9 11 10\nf 9 12 11\nf 13 14 15\nf 13 15 16\nf 9 10 14\nf 9 14 13
f 10 11 15\nf 10 15 14\nf 12 9 13\nf 12 13 16
")
  add_test(NAME cli_field_winding
    COMMAND dualc_field --expr "winding(path=\"${cli_soup}\")" --depth 6 -o cli_field_winding.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  set_tests_properties(cli_field_winding PROPERTIES FAIL_REGULAR_EXPRESSION "not closed")

  # Content checks: a corrupt PNG or a truncated STL used to pass on its exit
  # code (audit C52). Each check reads one output of the test(s) it NEEDS,
  # which become its CTest fixtures: `ctest -R <check>` reruns them first, and
  # under `ctest -j` the check waits for them.
  function(dualc_cli_content name file kind)
    cmake_parse_arguments(C "" "" "NEEDS;OPTIONS" ${ARGN})
    add_test(NAME ${name}
      COMMAND ${CMAKE_COMMAND} -DFILE=${file} -DKIND=${kind} ${C_OPTIONS}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/check_cli_output.cmake
      WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
    foreach(producer IN LISTS C_NEEDS)
      set_tests_properties(${producer} PROPERTIES FIXTURES_SETUP ${producer})
    endforeach()
    if(C_NEEDS)
      set_tests_properties(${name} PROPERTIES FIXTURES_REQUIRED "${C_NEEDS}")
    endif()
  endfunction()

  # Each OBJ, and the least face count it must reach: about half of what Linux
  # GCC Release wrote on 2026-10-16 (the retired tools wrote the same bytes), so
  # an empty contour's placeholder triangle fails while another platform's
  # rounding passes. The newer outputs keep the default floor of 1.
  foreach(entry
      cli_field_primitive=cli_field_primitive.obj=12000
      cli_field_infinite=cli_field_infinite.obj=200
      cli_boolean=cli_boolean.obj=2500
      cli_field_boolean=cli_field_boolean.obj=2500
      cli_field_boolean_bake=cli_field_boolean_bake.obj=1000
      cli_field_lift=cli_field_lift.obj=4500
      cli_field_extrude=cli_field_extrude.obj=5000
      cli_field_lattice=cli_field_lattice.obj=8500
      cli_field_mesh_sphere=cli_field_mesh_sphere.obj=15000
      cli_field_mesh_torus=cli_field_mesh_torus.obj=20000
      cli_field_mesh_sharp=cli_field_mesh_sharp.obj=17000
      cli_field_mesh_gwn=cli_field_mesh_gwn.obj=15000
      cli_field_winding=cli_field_winding.obj=10000
      cli_field=cli_field.obj=1 cli_field_mesh=cli_field_mesh.obj=1
      cli_field_expr=cli_field_expr.obj=1 cli_field_strut=cli_field_strut.obj=1)
    string(REPLACE "=" ";" entry "${entry}")
    list(GET entry 0 producer)
    list(GET entry 1 out)
    list(GET entry 2 floor)
    dualc_cli_content(${producer}_content ${out} obj NEEDS ${producer}
      OPTIONS -DMIN_FACES=${floor})
  endforeach()
  set(DUALC_FIELD_SAMPLE_FLOORS 5000 1800 4000 8000 5000 4500)
  foreach(i RANGE 5)
    list(GET DUALC_FIELD_SAMPLES ${i} sample)
    list(GET DUALC_FIELD_SAMPLE_FLOORS ${i} floor)
    dualc_cli_content(cli_field_sample_${sample}_content
      cli_field_${sample}.obj obj NEEDS cli_field_sample_${sample}
      OPTIONS -DMIN_FACES=${floor})
  endforeach()
  add_test(NAME cli_field_boolean_same
    COMMAND ${CMAKE_COMMAND} -E compare_files cli_boolean.obj cli_field_boolean.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  set_tests_properties(cli_field_boolean_same PROPERTIES
    FIXTURES_REQUIRED "cli_boolean;cli_field_boolean")
  add_test(NAME cli_field_threads_same
    COMMAND ${CMAKE_COMMAND} -E compare_files cli_field_boolean_bake.obj cli_field_threads.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_field>)
  set_tests_properties(cli_field_threads PROPERTIES FIXTURES_SETUP cli_field_threads)
  set_tests_properties(cli_field_threads_same PROPERTIES
    FIXTURES_REQUIRED "cli_field_boolean_bake;cli_field_threads")
  # The same mesh in three formats: STL and 3MF must carry exactly the OBJ's
  # triangle count.
  foreach(ext stl 3mf)
    dualc_cli_content(cli_field_lattice_${ext}_content cli_field_lattice.${ext} ${ext}
      NEEDS cli_field_lattice_${ext} cli_field_lattice
      OPTIONS -DSAME_AS_OBJ=cli_field_lattice.obj)
  endforeach()
  dualc_cli_content(cli_field_stl_content cli_field.stl stl
    NEEDS cli_field_stl cli_field OPTIONS -DSAME_AS_OBJ=cli_field.obj)
  dualc_cli_content(cli_field_tiled_stl_content cli_field_tiled.stl stl
    NEEDS cli_field_tiled_stl)
  dualc_cli_content(cli_field_slice_content cli_field_slice.png png
    NEEDS cli_field_slice OPTIONS -DWIDTH=128 -DHEIGHT=128)
  dualc_cli_content(cli_field_slice_svg_content cli_field_slice.svg svg
    NEEDS cli_field_slice OPTIONS "-DCONTAINS=<line ")
  # --dump-json prints the graph: the check runs the tool itself.
  dualc_cli_content(cli_field_dump_json_content cli_field_dump.json json
    OPTIONS -DJSON_ROOT_OP=difference
            "-DRUN=$<TARGET_FILE:dualc_field>|gyroid_box.json|--dump-json")
  # The generated cube is exact: 8 corners, 12 triangles.
  dualc_cli_content(cli_gen_demo_content
    ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_out/cube.obj obj
    NEEDS cli_gen_demo OPTIONS -DEXACT_VERTICES=8 -DEXACT_FACES=12)
  # `all --dir` creates a missing directory, parents included; it used to
  # abort on an uncaught exception, so `--dir data` failed on a fresh clone
  # (roadmap 10 #54). The setup test removes the tree so every run starts
  # without it.
  set(gen_demo_new_dir ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_new/a/b)
  add_test(NAME cli_gen_demo_dir_reset
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_new)
  set_tests_properties(cli_gen_demo_dir_reset PROPERTIES
    FIXTURES_SETUP cli_gen_demo_dir_reset)
  add_test(NAME cli_gen_demo_dir
    COMMAND dualc_gen_demo all --dir ${gen_demo_new_dir})
  set_tests_properties(cli_gen_demo_dir PROPERTIES
    FIXTURES_REQUIRED cli_gen_demo_dir_reset)
  dualc_cli_content(cli_gen_demo_dir_content ${gen_demo_new_dir}/cube.obj obj
    NEEDS cli_gen_demo_dir OPTIONS -DEXACT_VERTICES=8 -DEXACT_FACES=12)
  # A write that cannot happen is a message and exit 1, not an abort: an
  # uncaught exception never prints the tool's own error line.
  add_test(NAME cli_gen_demo_bad_out
    COMMAND dualc_gen_demo cube
            -o ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_missing/x/cube.obj)
  set_tests_properties(cli_gen_demo_bad_out PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[dualc_gen_demo\\] error: ")
endif()
