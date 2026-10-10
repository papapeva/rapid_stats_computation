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

option(RAPID_STATS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(rapid_stats INTERFACE)
add_library(rapid_stats::rapid_stats ALIAS rapid_stats)
target_include_directories(rapid_stats INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(rapid_stats INTERFACE cxx_std_17)
target_sources(rapid_stats INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/order_statistic.hpp)

add_executable(rapid_stats_tests order_statistic_test.cpp)
target_link_libraries(rapid_stats_tests PRIVATE rapid_stats)
target_compile_features(rapid_stats_tests PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(rapid_stats_tests PRIVATE /W4 /permissive- /utf-8 /w44062)
else()
    target_compile_options(rapid_stats_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wswitch-enum)
endif()
if(RAPID_STATS_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(rapid_stats_tests PRIVATE /WX)
    else()
        target_compile_options(rapid_stats_tests PRIVATE -Werror)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME order_statistic COMMAND rapid_stats_tests)
endif()

set(FORMAT_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/order_statistic.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/order_statistic_test.cpp
)

find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

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

if(CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} --warnings-as-errors=*
                ${CMAKE_CURRENT_SOURCE_DIR}/order_statistic_test.cpp
        COMMENT "Run clang-tidy"
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()
