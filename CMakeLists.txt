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
file(GLOB_RECURSE ALGORITHM_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/algo/*.hpp)
target_sources(rapid_stats INTERFACE ${ALGORITHM_HEADERS})

include(CTest)
option(RAPID_STATS_SANITIZERS "Check tests with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(BUILD_TESTING)
    add_executable(algorithms_test tests/algorithms_test.cpp)
    target_link_libraries(algorithms_test PRIVATE rapid_stats::rapid_stats)
    if(MSVC)
        target_compile_options(algorithms_test PRIVATE /utf-8 /W4)
    else()
        target_compile_options(algorithms_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(RAPID_STATS_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
            target_compile_options(algorithms_test PRIVATE
                -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(algorithms_test PRIVATE -fsanitize=address,undefined)
        else()
            message(FATAL_ERROR "RAPID_STATS_SANITIZERS requires GCC or Clang with GNU-style flags")
        endif()
    endif()
    add_test(NAME algorithms_contract COMMAND algorithms_test)
endif()

set(FORMAT_FILES ${ALGORITHM_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/algorithms_test.cpp)

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
                ${ALGORITHM_HEADERS}
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
