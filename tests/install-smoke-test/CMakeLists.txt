# Standalone consumer that pretends to be a downstream user of an
# installed reflect package. The CI install-smoke-test job builds and
# installs reflect to a temp prefix, then runs cmake on this directory
# pointing CMAKE_PREFIX_PATH at the install — exercising the real
# find_package(reflect) → reflect::reflect → INTERFACE flag flow.
cmake_minimum_required(VERSION 3.20)
project(reflect_consumer LANGUAGES CXX)

find_package(reflect REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE reflect::reflect)
