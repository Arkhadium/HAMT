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