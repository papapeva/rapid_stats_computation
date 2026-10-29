cmake_minimum_required(VERSION 3.16)
project(selection_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(benchmark benchmark.cpp)

if (MSVC)
    target_compile_options(benchmark PRIVATE /O2 /W4)
else()
    target_compile_options(benchmark PRIVATE -O2 -Wall -Wextra -pedantic)
endif()
