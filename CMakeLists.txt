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

add_library(rapid_stats INTERFACE)
add_library(rapid_stats::rapid_stats ALIAS rapid_stats)
target_include_directories(rapid_stats INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(rapid_stats INTERFACE cxx_std_17)
target_sources(rapid_stats INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/algo/order_statistic_algorithm.hpp)

set(FORMAT_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/algo/order_statistic_algorithm.hpp
)

find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
find_package(Python3 COMPONENTS Interpreter QUIET)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_FILES}
        COMMENT "Format C++ sources"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${FORMAT_FILES}
        COMMENT "Check C++ formatting"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
    )
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()

if(CLANG_TIDY_EXE AND Python3_EXECUTABLE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} --warnings-as-errors=*
                ${CMAKE_CURRENT_SOURCE_DIR}/algo/order_statistic_algorithm.hpp
                -- -std=c++17
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/check_function_comments.py
                ${FORMAT_FILES}
        COMMENT "Run clang-tidy and the function comment check"
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy or Python 3 was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()
