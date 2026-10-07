include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  # Pinned by commit, not by tag: a tag is mutable, so "v3.5.4" alone does not
  # make the configure step reproducible. This SHA is v3.5.4 -- verified against
  # the fetched tree (catch_version_macros.hpp reports 3.5.4, and the v3.5.4 tag
  # resolves to this commit). GIT_SHALLOW is deliberately left off: it is
  # unreliable when GIT_TAG names a commit hash rather than a ref.
  GIT_TAG        b5373dadca40b7edc8570cf9470b9b1cb1934d40 # v3.5.4
)
FetchContent_MakeAvailable(Catch2)
# Catch2 detects its C++17 support (std::uncaught_exceptions, the string_view
# and byte StringMakers) per translation unit, so the library and the test TUs
# should agree on the standard. It used to get C++17 from the global
# CMAKE_CXX_STANDARD that leaked into every fetch; it gets it by name now
# (roadmap 17 #33), which keeps its build what the suite was proven on.
set_target_properties(Catch2 Catch2WithMain PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(dualc_tests
  test_main.cpp
  test_octree.cpp
  test_parallel.cpp
  test_qef.cpp
  test_dc_tables.cpp
  test_cube_components.cpp
  test_accuracy.cpp
  test_sampler.cpp
  test_contourer.cpp
  test_pipeline.cpp
  test_implicit.cpp
  test_grid_field.cpp
  test_combinators.cpp
  test_primitives.cpp
  test_domain_ops.cpp
  test_lift.cpp
  test_mesh_bvh.cpp
  test_tpms.cpp
  test_strut_lattice.cpp
  test_demo_meshes.cpp
  test_field_graph.cpp
  test_field_glsl.cpp
  test_streaming_export.cpp
  test_diagnostics.cpp
  test_validation.cpp
  test_cancel_progress.cpp
  # Reuse the procedural mesh generators from examples/ (compiled in directly
  # so this does not depend on DUALC_BUILD_EXAMPLES being on).
  ${PROJECT_SOURCE_DIR}/examples/demo_meshes.cpp
  # Field-graph parser/builder + the registries it wires (likewise compiled in
  # directly, with the vendored miniz that example_common's 3MF writer needs).
  ${PROJECT_SOURCE_DIR}/examples/field_graph.cpp
  # Field->GLSL codegen (GL-free: it generates source + bakes meshes on the CPU),
  # so the dedup tests in test_field_glsl.cpp run without a GL context.
  ${PROJECT_SOURCE_DIR}/examples/field_glsl.cpp
  ${PROJECT_SOURCE_DIR}/examples/example_common.cpp
  ${PROJECT_SOURCE_DIR}/examples/third_party/miniz.c
  # meshoptimizer (QEM decimation) that example_common's writeField --decimate
  # path needs -- compiled in directly like miniz.
  ${PROJECT_SOURCE_DIR}/examples/third_party/meshoptimizer/simplifier.cpp
  ${PROJECT_SOURCE_DIR}/examples/third_party/meshoptimizer/vfetchoptimizer.cpp
)

target_link_libraries(dualc_tests PRIVATE dualc::dualc Catch2::Catch2WithMain)
target_compile_features(dualc_tests PRIVATE cxx_std_17)
dualc_target_options(dualc_tests)

# Tests need access to the internal headers under src/ for white-box checks,
# and to examples/ (incl. third_party/) for the demo-mesh generators, the
# field-graph builder and the vendored miniz/json headers.
target_include_directories(dualc_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/examples
  ${PROJECT_SOURCE_DIR}/examples/third_party
  ${PROJECT_SOURCE_DIR}/examples/third_party/meshoptimizer)

# Quiet the vendored miniz + meshoptimizer amalgamations only (json.hpp is
# pragma-guarded at its include site in field_graph.cpp).
if(MSVC)
  target_compile_definitions(dualc_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  set_source_files_properties(${PROJECT_SOURCE_DIR}/examples/third_party/miniz.c
    ${PROJECT_SOURCE_DIR}/examples/third_party/meshoptimizer/simplifier.cpp
    ${PROJECT_SOURCE_DIR}/examples/third_party/meshoptimizer/vfetchoptimizer.cpp
    PROPERTIES COMPILE_OPTIONS "/W0")
else()
  set_source_files_properties(${PROJECT_SOURCE_DIR}/examples/third_party/miniz.c
    ${PROJECT_SOURCE_DIR}/examples/third_party/meshoptimizer/simplifier.cpp
    ${PROJECT_SOURCE_DIR}/examples/third_party/meshoptimizer/vfetchoptimizer.cpp
    PROPERTIES COMPILE_OPTIONS "-w")
endif()

include(CTest)
include(Catch)
catch_discover_tests(dualc_tests)
