cmake_minimum_required(VERSION 3.25)

project(cpp-project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_minimum_required(VERSION 3.25)

project(hamt LANGUAGES CXX)

add_executable(hamt
    src/main.cpp
)

target_compile_features(hamt PRIVATE cxx_std_26)

target_compile_options(hamt PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set_target_properties(hamt PROPERTIES
    CXX_EXTENSIONS OFF
)
