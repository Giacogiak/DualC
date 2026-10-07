# --- mesh-in / mesh-out demo (v1) ---------------------------------------
add_executable(dualc_demo dualc_demo.cpp)
target_link_libraries(dualc_demo PRIVATE dualc::dualc)
target_compile_features(dualc_demo PRIVATE cxx_std_17)
dualc_target_options(dualc_demo)

# --- v2 implicit-field CLI tools ----------------------------------------

# Host-only file-I/O deps for the examples (PNG + 3MF/ZIP). These are vendored
# under third_party/ and linked ONLY into example binaries -- libdualc never
# sees them, so the library proper stays dependency-free (library-scope rule).
# They are split into TWO libs on purpose:
#   miniz.{h,c}        -- MIT (deflate ZIP for 3MF). Needed by example_common's
#                         writeField, so PUBLIC-linked into dualc_examples_common
#                         (every CLI's -o .3mf path uses it).
#   stb_image_write.h  -- Public Domain (PNG output). Used only by dualc_slice
#                         and the GL plumbing's snapshot path, so it lives in its
#                         OWN lib and is NOT pulled into dualc_examples_common:
#                         the mesh-only CLIs never carry a PNG writer.
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
# (PNG) is deliberately NOT here -- only dualc_slice links it (see above).
add_library(dualc_examples_common STATIC example_common.cpp)
target_link_libraries(dualc_examples_common
  PUBLIC dualc::dualc dualc_examples_io dualc_examples_decimate)
target_compile_features(dualc_examples_common PUBLIC cxx_std_17)
dualc_target_options(dualc_examples_common)
target_include_directories(dualc_examples_common
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

foreach(tool dualc_primitive dualc_boolean dualc_lift dualc_csg_demo dualc_lattice dualc_slice)
  add_executable(${tool} ${tool}.cpp)
  target_link_libraries(${tool} PRIVATE dualc::dualc dualc_examples_common)
  target_compile_features(${tool} PRIVATE cxx_std_17)
  dualc_target_options(${tool})
endforeach()

# dualc_slice is the only example that writes PNG -> give it the stb impl.
target_link_libraries(dualc_slice PRIVATE dualc_examples_img)

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
target_link_libraries(dualc_field PRIVATE dualc::dualc dualc_examples_fieldgraph)
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
set(FIELD_GRAPH_SAMPLES gyroid_box.json mesh_lattice.json)
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
# The three GL targets below (dualc_raymarch, dualc_field_view, dualc_glsl_parity)
# share one dependency pair, both permissive and both linked into these binaries
# only -- libdualc is untouched:
#   glad  -- the generated OpenGL 3.3 core loader (Public Domain), vendored under
#            third_party/glad as its own lib: one C file + two headers, built
#            like miniz above and warning-silenced the same way.
#   glfw  -- window / context / input (zlib), resolved by the root CMakeLists
#            (an existing target, DUALC_GLFW_DIR, or the pinned FetchContent).
# Each needs a GL window, so none has a CTest; dualc_field_view's --snapshot and
# the parity harness's hidden window are the scripted paths.
if(DUALC_BUILD_RAYMARCH_VIEWER OR DUALC_BUILD_FIELD_VIEW OR DUALC_BUILD_GLSL_PARITY)
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
  # orbit camera, headless PNG snapshot) for the GPU viewers and the parity
  # harness. PUBLIC-links glfw + glad + stb (the PNG writer, for the snapshot).
  add_library(dualc_examples_gl STATIC raymarch_gl.cpp)
  target_link_libraries(dualc_examples_gl
    PUBLIC dualc::dualc dualc_examples_img glfw dualc_examples_glad)
  target_compile_features(dualc_examples_gl PUBLIC cxx_std_17)
  dualc_target_options(dualc_examples_gl)
  target_include_directories(dualc_examples_gl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# dualc_raymarch is the first GPU app: it sphere-traces the analytic TPMS field
# per pixel, so cost is O(pixels) and independent of lattice density.
if(DUALC_BUILD_RAYMARCH_VIEWER)
  add_executable(dualc_raymarch dualc_raymarch.cpp)
  target_link_libraries(dualc_raymarch PRIVATE dualc::dualc dualc_examples_common dualc_examples_gl)
  target_compile_features(dualc_raymarch PRIVATE cxx_std_17)
  dualc_target_options(dualc_raymarch)
endif()

# dualc_field_view generalises the raymarcher to ANY field-graph: it compiles the
# graph to GLSL (dualc_examples_fieldglsl) and sphere-traces it, so the previewed
# field is the exported field. Opt-in; needs a GL window (no CTest -- a hidden
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
function(dualc_copy_demo_meshes target)
  get_property(meshes GLOBAL PROPERTY DUALC_DEMO_MESHES)
  get_property(datadir GLOBAL PROPERTY DUALC_DEMO_DATA_DIR)
  add_dependencies(${target} dualc_demo_data)
  foreach(meshfile ${meshes})
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${datadir}/${meshfile}
              $<TARGET_FILE_DIR:${target}>/${meshfile})
  endforeach()
endfunction()

foreach(bin dualc_demo dualc_primitive dualc_boolean dualc_lift dualc_csg_demo dualc_lattice dualc_slice dualc_field)
  dualc_copy_demo_meshes(${bin})
endforeach()

# The raymarch viewer (gated) also reads demo meshes by bare filename.
if(DUALC_BUILD_RAYMARCH_VIEWER)
  dualc_copy_demo_meshes(dualc_raymarch)
endif()

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
# unit tests. They cover one invocation per tool, including the post-op
# chain, the infinite-bounds path, a mesh boolean, a 2D lift and a recipe.
# Each test checks the exit code; the `<test>_content` tests registered after
# them check what was written (check_cli_output.cmake, roadmap 17 #32).
if(DUALC_BUILD_TESTS)
  # Exercises the post-op chain (onion decorator + twist domain op). Notes:
  #  - the onion wall (0.35) is kept comfortably thicker than the depth-6 cell
  #    so the shell resolves into a clean manifold (a thin wall under-resolved
  #    at low depth produces non-manifold junk -- one-crossing-per-edge limit);
  #  - the twist is about x (axis 0), NOT the torus's y symmetry axis -- a
  #    twist about a body-of-revolution's own axis is a no-op.
  add_test(NAME cli_primitive
    COMMAND dualc_primitive torus 0 0 0 2 0.5 --onion 0.35 --twist 0.4,0
            --depth 6 -o cli_primitive.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_primitive>)
  add_test(NAME cli_primitive_infinite
    COMMAND dualc_primitive plane 0 1 0 0 --bounds -2,-2,-2,2,2,2
            --depth 4 -o cli_plane.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_primitive>)
  add_test(NAME cli_boolean
    COMMAND dualc_boolean smooth-union cube.obj cube.obj --k 0.3
            --depth 5 -o cli_boolean.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_boolean>)
  add_test(NAME cli_lift
    COMMAND dualc_lift revolve circle 0 0 0.5 --offset 2
            --depth 5 -o cli_lift.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_lift>)
  add_test(NAME cli_csg_demo
    COMMAND dualc_csg_demo --recipe=smooth-blend --depth 5 -o cli_csg.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_csg_demo>)
  # Gyroid lattice infill inside the sample cube. Wavelength 0.5 = ~four
  # cells across the 2-unit cube; offset 0.1 makes a thick double-sheet
  # shell that comfortably resolves at depth 5.
  add_test(NAME cli_lattice
    COMMAND dualc_lattice cube.obj --type gyroid --wavelength 0.5
            --offset 0.1 --depth 5 -o cli_lattice.obj
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_lattice>)
  # Same lattice, exported through the extension-dispatched writer to the two
  # manufacturing-facing formats (binary STL and 3MF-mesh). Their content
  # checks hold both to cli_lattice.obj's face count.
  add_test(NAME cli_lattice_stl
    COMMAND dualc_lattice cube.obj --type gyroid --wavelength 0.5
            --offset 0.1 --depth 5 -o cli_lattice.stl
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_lattice>)
  add_test(NAME cli_lattice_3mf
    COMMAND dualc_lattice cube.obj --type gyroid --wavelength 0.5
            --offset 0.1 --depth 5 -o cli_lattice.3mf
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_lattice>)
  # Field-on-plane sampler: a z=0 cross-section of the same gyroid-in-cube
  # field, emitting a PNG heatmap + an SVG of the zero-isocontour.
  add_test(NAME cli_slice
    COMMAND dualc_slice cube.obj --type gyroid --wavelength 0.5 --offset 0.1
            --plane z=0 --res 128 -o cli_slice.png
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_slice>)
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
  # Demo-mesh generator + a few pipeline runs over the new meshes. The sphere
  # and torus exercise smooth/genus regimes; the bracket run exercises the
  # sharp-feature toggle on a part with a reentrant edge.
  # cli_gen_demo writes into a directory of its own: run in the tools' shared
  # output directory it rewrote cube.obj, sphere.obj, ... while the tests
  # above and below read them, a race under `ctest -j` (roadmap 17 #32).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_out)
  add_test(NAME cli_gen_demo
    COMMAND dualc_gen_demo all
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_out)
  add_test(NAME cli_demo_sphere
    COMMAND dualc_demo sphere.obj cli_sphere.obj --depth 6
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_demo>)
  add_test(NAME cli_demo_torus
    COMMAND dualc_demo torus.obj cli_torus.obj --depth 6
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_demo>)
  add_test(NAME cli_demo_bracket_sharp
    COMMAND dualc_demo --sharp bracket.obj cli_bracket.obj --depth 6
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_demo>)

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

  foreach(pair
      cli_primitive=cli_primitive.obj cli_primitive_infinite=cli_plane.obj
      cli_boolean=cli_boolean.obj cli_lift=cli_lift.obj
      cli_csg_demo=cli_csg.obj cli_lattice=cli_lattice.obj
      cli_field=cli_field.obj cli_field_mesh=cli_field_mesh.obj
      cli_field_expr=cli_field_expr.obj cli_field_strut=cli_field_strut.obj
      cli_demo_sphere=cli_sphere.obj cli_demo_torus=cli_torus.obj
      cli_demo_bracket_sharp=cli_bracket.obj)
    string(REPLACE "=" ";" pair "${pair}")
    list(GET pair 0 producer)
    list(GET pair 1 out)
    dualc_cli_content(${producer}_content ${out} obj NEEDS ${producer})
  endforeach()
  # The same mesh in three formats: STL and 3MF must carry exactly the OBJ's
  # triangle count.
  dualc_cli_content(cli_lattice_stl_content cli_lattice.stl stl
    NEEDS cli_lattice_stl cli_lattice OPTIONS -DSAME_AS_OBJ=cli_lattice.obj)
  dualc_cli_content(cli_lattice_3mf_content cli_lattice.3mf 3mf
    NEEDS cli_lattice_3mf cli_lattice OPTIONS -DSAME_AS_OBJ=cli_lattice.obj)
  dualc_cli_content(cli_field_stl_content cli_field.stl stl
    NEEDS cli_field_stl cli_field OPTIONS -DSAME_AS_OBJ=cli_field.obj)
  dualc_cli_content(cli_field_tiled_stl_content cli_field_tiled.stl stl
    NEEDS cli_field_tiled_stl)
  dualc_cli_content(cli_slice_content cli_slice.png png
    NEEDS cli_slice OPTIONS -DWIDTH=128 -DHEIGHT=128)
  dualc_cli_content(cli_slice_svg_content cli_slice.svg svg
    NEEDS cli_slice OPTIONS "-DCONTAINS=<line ")
  # --dump-json prints the graph: the check runs the tool itself.
  dualc_cli_content(cli_field_dump_json_content cli_field_dump.json json
    OPTIONS -DJSON_ROOT_OP=difference
            "-DRUN=$<TARGET_FILE:dualc_field>|gyroid_box.json|--dump-json")
  # The generated cube is exact: 8 corners, 12 triangles.
  dualc_cli_content(cli_gen_demo_content
    ${CMAKE_CURRENT_BINARY_DIR}/cli_gen_demo_out/cube.obj obj
    NEEDS cli_gen_demo OPTIONS -DEXACT_VERTICES=8 -DEXACT_FACES=12)
endif()
