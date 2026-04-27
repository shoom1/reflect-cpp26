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

# Examples
option(REFLECT_BUILD_EXAMPLES "Build example programs" ON)
if (REFLECT_BUILD_EXAMPLES)
    add_executable(showcase examples/showcase.cpp)
    target_link_libraries(showcase PRIVATE reflect)

    add_executable(critical_issues examples/critical_issues.cpp)
    target_link_libraries(critical_issues PRIVATE reflect)
endif()

# Tests
option(REFLECT_BUILD_TESTS "Build tests" ON)
if (REFLECT_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    file(GLOB TEST_SOURCES tests/*.cpp)
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE reflect)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

# Benchmarks
option(REFLECT_BUILD_BENCHMARKS "Build benchmarks" ON)
if (REFLECT_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
    file(GLOB BENCH_SOURCES benchmarks/*.cpp)
    foreach(bench_src ${BENCH_SOURCES})
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
