cmake_minimum_required(VERSION 3.20)

project(rapid_stats_computation
    VERSION 0.1.0
    DESCRIPTION "Fast order statistics on large samples"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RAPID_STATS_BUILD_BENCHMARKS "Build micro-benchmarks" ON)
option(RAPID_STATS_BUILD_EXAMPLES "Build usage examples" ON)
option(RAPID_STATS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(RAPID_STATS_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan (GCC/Clang)" OFF)

include(cmake/warnings.cmake)

add_library(rapid_stats INTERFACE)
add_library(rapid_stats::rapid_stats ALIAS rapid_stats)
target_include_directories(rapid_stats INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
target_compile_features(rapid_stats INTERFACE cxx_std_17)
target_sources(rapid_stats INTERFACE
    ${PROJECT_SOURCE_DIR}/include/rapid_stats/order_statistic.hpp
    ${PROJECT_SOURCE_DIR}/include/rapid_stats/detail/requirements.hpp
    ${PROJECT_SOURCE_DIR}/include/rapid_stats/detail/full_sort.hpp
    ${PROJECT_SOURCE_DIR}/include/rapid_stats/detail/quickselect.hpp
    ${PROJECT_SOURCE_DIR}/include/rapid_stats/detail/heap_select.hpp
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(RAPID_STATS_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(RAPID_STATS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(cmake/lint.cmake)

message(STATUS "rapid_stats tests: ${BUILD_TESTING}")
message(STATUS "rapid_stats benchmarks: ${RAPID_STATS_BUILD_BENCHMARKS}")
message(STATUS "rapid_stats examples: ${RAPID_STATS_BUILD_EXAMPLES}")
