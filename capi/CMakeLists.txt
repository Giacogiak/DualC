# DualC C ABI -- host-side SHARED library (re-scoped #19: proxy + export).
#
# It links the field-graph parser (dualc_examples_fieldgraph, vendors
# nlohmann/json) and the export writers (dualc_examples_common, vendors miniz),
# both of which are deliberately OUTSIDE libdualc by the library-scope rule.
# So the ABI lives here as a separate shared library, NOT in src/c_abi -- it
# must never drag those host-only deps into libdualc. Requires
# DUALC_BUILD_EXAMPLES (enforced by the parent CMakeLists).

add_library(dualc_capi SHARED dualc_c.cpp)
add_library(dualc::capi ALIAS dualc_capi)

target_link_libraries(dualc_capi
  PRIVATE dualc::dualc dualc_examples_fieldgraph dualc_examples_common)

# The public C header sits next to this file.
target_include_directories(dualc_capi
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_compile_features(dualc_capi PRIVATE cxx_std_17)
dualc_target_options(dualc_capi)

# DUALC_CAPI_BUILD switches the export macro to dllexport while building the
# DLL; only the extern "C" entry points are exported. PIC is a no-op on MSVC
# but lets the same target build a Linux .so later. Version string is the CMake
# project version, embedded as a C string literal.
target_compile_definitions(dualc_capi PRIVATE
  DUALC_CAPI_BUILD
  "DUALC_C_VERSION_STR=\"${PROJECT_VERSION}\"")
set_target_properties(dualc_capi PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_definitions(dualc_capi PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# C harness -- compiled by the C compiler, the real proof the header is C-clean.
add_executable(dualc_c_demo dualc_c_demo.c)
target_link_libraries(dualc_c_demo PRIVATE dualc_capi)
dualc_target_options(dualc_c_demo)
if(MSVC)
  target_compile_definitions(dualc_c_demo PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# The "mesh" demo mode loads cube.obj by bare filename from the working dir, so
# the generated demo meshes are copied next to the binary -- the same helper
# the example CLIs use (defined in examples/CMakeLists.txt, which this
# directory requires).
dualc_copy_demo_meshes(dualc_c_demo)

if(DUALC_BUILD_TESTS)
  # End-to-end: build a graph from --expr, contour a proxy, export an STL.
  add_test(NAME cli_c_abi
    COMMAND dualc_c_demo c_abi_demo.stl
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_c_demo>)
  # Same, but through a mesh(path=...) source -- exercises the handle's
  # FileMeshResolver lifetime (the one piece of ABI-specific logic).
  add_test(NAME cli_c_abi_mesh
    COMMAND dualc_c_demo c_abi_mesh.stl mesh
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_c_demo>)
  # In-memory mesh source: asserts mesh(id=...) via the *_with_meshes create call
  # contours BYTE-IDENTICAL to the mesh(path=...) disk form (the diskless gate).
  add_test(NAME cli_c_abi_mesh_inmem
    COMMAND dualc_c_demo c_abi_inmem.stl inmem
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_c_demo>)
  # The 0.5.0 cancel token + progress callback: a pre-requested token cancels
  # before any work, a mid-way request leaves no file and no .part, the
  # all-NULL twins are byte-identical to the older calls, a live callback
  # sees every stage end at done == total.
  add_test(NAME cli_c_abi_cancel
    COMMAND dualc_c_demo c_abi_cancel.stl cancel
    WORKING_DIRECTORY $<TARGET_FILE_DIR:dualc_c_demo>)
endif()
