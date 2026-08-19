cmake_minimum_required(VERSION 3.25)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(hamt LANGUAGES CXX)

add_executable(hamt
    src/main.cpp
)

target_compile_features(hamt PRIVATE cxx_std_26)

target_include_directories(hamt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(hamt
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

set_target_properties(hamt PROPERTIES
    CXX_EXTENSIONS OFF
)

find_package(GTest REQUIRED)

add_executable(Tests
    tests/emplaceTests.cpp
    tests/eraseTests.cpp
    tests/findTests.cpp
    tests/iteratorTests.cpp
    tests/lifetimeTests.cpp
)

target_include_directories(Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Tests PRIVATE
    GTest::gtest_main
)

# Testbench 

add_executable(Testbench
    benchmark/testBench.cpp
)

target_include_directories(Testbench
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include(GoogleTest)
gtest_discover_tests(Tests)
