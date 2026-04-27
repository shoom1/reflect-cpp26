cmake_minimum_required(VERSION 3.20)
project(reflect
    VERSION 0.1.0
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

# Detect compiler and add reflection flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Bloomberg Clang fork
    target_compile_options(reflect INTERFACE -freflection-latest)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 16+ — reflection is enabled automatically with -std=c++26
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.0")
        message(WARNING "GCC < 16 does not support C++26 reflection. "
                        "Build from GCC trunk or use Bloomberg Clang fork.")
    endif()
endif()

# Tests
# Listed explicitly rather than file(GLOB ...): a glob runs at configure
# time, so a new test_xxx.cpp added on disk wouldn't be picked up until
# CMake re-configures. With an explicit list, adding a test is a 20-second
# tax for guaranteed correctness — a missing entry here surfaces as a
# linker/test-discovery failure on the next build, not a silently-skipped
# test.
set(REFLECT_TEST_SOURCES
    tests/test_args.cpp
    tests/test_critical_issues.cpp
    tests/test_enum.cpp
    tests/test_json.cpp
    tests/test_struct.cpp
)

option(REFLECT_BUILD_TESTS "Build tests" ON)
if (REFLECT_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    foreach(test_src ${REFLECT_TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE reflect)
        add_test(NAME ${test_name} COMMAND ${test_name})
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
)

option(REFLECT_BUILD_EXAMPLES "Build example programs" ON)
if (REFLECT_BUILD_EXAMPLES)
    foreach(ex_src ${REFLECT_EXAMPLE_SOURCES})
        get_filename_component(ex_name ${ex_src} NAME_WE)
        add_executable(${ex_name} ${ex_src})
        target_link_libraries(${ex_name} PRIVATE reflect)
        if (REFLECT_BUILD_TESTS)
            add_test(NAME example_${ex_name} COMMAND ${ex_name})
        endif()
    endforeach()
endif()

# Benchmarks
set(REFLECT_BENCHMARK_SOURCES
    benchmarks/bench_json.cpp
)

option(REFLECT_BUILD_BENCHMARKS "Build benchmarks" ON)
if (REFLECT_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
    foreach(bench_src ${REFLECT_BENCHMARK_SOURCES})
        get_filename_component(bench_name ${bench_src} NAME_WE)
        add_executable(${bench_name} ${bench_src})
        target_link_libraries(${bench_name} PRIVATE reflect)
        target_compile_options(${bench_name} PRIVATE -O2)
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
