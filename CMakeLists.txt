cmake_minimum_required(VERSION 3.20)
project(reflect
    VERSION 0.2.1
    DESCRIPTION "C++26 reflection utilities — header-only, zero dependencies"
    LANGUAGES CXX
)

# Header-only library
add_library(reflect INTERFACE)
target_include_directories(reflect INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
# Use -std=c++26 directly rather than target_compile_features(cxx_std_26):
# bleeding-edge Clang/GCC builds aren't always known to CMake's compile-
# features database for the C++26 meta-feature, which causes a hard
# configure error ("cxx_std_26 is not known to CXX compiler").
target_compile_options(reflect INTERFACE -std=c++26)

# Compiler-specific reflection flag. Wrapped in a generator expression
# so the decision is made at the *consumer's* build time, not when this
# package is configured/installed. Without this, installing after
# configuring with Clang would leak -freflection-latest into a
# downstream GCC consumer's build via the exported INTERFACE.
target_compile_options(reflect INTERFACE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-freflection-latest>
)

# GCC 16+ enables reflection automatically with -std=c++26. This warning
# fires only at our configure time and is not exported to consumers.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.0")
    message(WARNING "GCC < 16 does not support C++26 reflection. "
                    "Build from GCC trunk or use Bloomberg Clang fork.")
endif()

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(REFLECT_DEFAULT_BUILD_AUX ON)
else()
    set(REFLECT_DEFAULT_BUILD_AUX OFF)
endif()

# Tests
# Listed explicitly rather than file(GLOB ...): a glob runs at configure
# time, so a new test_xxx.cpp added on disk wouldn't be picked up until
# CMake re-configures. With an explicit list, adding a test is a 20-second
# tax for guaranteed determinism. The downside — a forgotten entry would
# silently skip the test rather than failing the build — is caught by
# scripts/verify-tests-listed.sh, run as a CI pre-flight step.
set(REFLECT_TEST_SOURCES
    tests/test_args.cpp
    tests/test_critical_issues.cpp
    tests/test_enum.cpp
    tests/test_json.cpp
    tests/test_struct.cpp
)

option(REFLECT_BUILD_TESTS "Build tests" ${REFLECT_DEFAULT_BUILD_AUX})
if (REFLECT_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    foreach(test_src ${REFLECT_TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        set(test_target reflect_${test_name})
        add_executable(${test_target} ${test_src})
        target_link_libraries(${test_target} PRIVATE reflect)
        add_test(NAME ${test_name} COMMAND ${test_target})
    endforeach()

    # Compile-failure tests: each source listed here MUST fail to
    # compile. The target is EXCLUDE_FROM_ALL (so the main build stays
    # green) and ctest invokes cmake --build on it expecting non-zero
    # exit (WILL_FAIL TRUE). A successful compile means the guard the
    # test exercises has regressed.
    set(REFLECT_COMPILE_FAIL_SOURCES
        tests/compile_fail_map_string_view.cpp
    )
    foreach(cf_src ${REFLECT_COMPILE_FAIL_SOURCES})
        get_filename_component(cf_name ${cf_src} NAME_WE)
        set(cf_target reflect_${cf_name})
        add_executable(${cf_target} EXCLUDE_FROM_ALL ${cf_src})
        target_link_libraries(${cf_target} PRIVATE reflect)
        add_test(
            NAME ${cf_name}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                    --target ${cf_target} --config $<CONFIG>
        )
        set_tests_properties(${cf_name} PROPERTIES WILL_FAIL TRUE)
    endforeach()
endif()

# Examples
# Per-feature programs (plus the all-in-one showcase). When tests are
# enabled, each example is also registered as a ctest entry — so CI not
# only builds them but runs them and verifies a clean exit. A regression
# in any example fails CI, same as a unit test.
set(REFLECT_EXAMPLE_SOURCES
    examples/showcase.cpp
    examples/type_name.cpp
    examples/enum.cpp
    examples/visit.cpp
    examples/print.cpp
    examples/compare.cpp
    examples/tuple.cpp
    examples/json.cpp
    examples/diff.cpp
    examples/args.cpp
    examples/format.cpp
    examples/generic.cpp
)

option(REFLECT_BUILD_EXAMPLES "Build example programs" ${REFLECT_DEFAULT_BUILD_AUX})
if (REFLECT_BUILD_EXAMPLES)
    foreach(ex_src ${REFLECT_EXAMPLE_SOURCES})
        get_filename_component(ex_name ${ex_src} NAME_WE)
        set(ex_target reflect_example_${ex_name})
        add_executable(${ex_target} ${ex_src})
        target_link_libraries(${ex_target} PRIVATE reflect)
        if (REFLECT_BUILD_TESTS)
            add_test(NAME example_${ex_name} COMMAND ${ex_target})
        endif()
    endforeach()
endif()

# Benchmarks
set(REFLECT_BENCHMARK_SOURCES
    benchmarks/bench_json.cpp
)

option(REFLECT_BUILD_BENCHMARKS "Build benchmarks" ${REFLECT_DEFAULT_BUILD_AUX})
if (REFLECT_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
    foreach(bench_src ${REFLECT_BENCHMARK_SOURCES})
        get_filename_component(bench_name ${bench_src} NAME_WE)
        set(bench_target reflect_${bench_name})
        add_executable(${bench_target} ${bench_src})
        target_link_libraries(${bench_target} PRIVATE reflect)
        target_compile_options(${bench_target} PRIVATE -O2)
    endforeach()
endif()

# Install
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS reflect EXPORT reflectTargets)
install(EXPORT reflectTargets
    FILE reflectConfig.cmake
    NAMESPACE reflect::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/reflect
)
