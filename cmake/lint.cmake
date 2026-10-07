file(GLOB_RECURSE RAPID_STATS_FORMAT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*"
    "${PROJECT_SOURCE_DIR}/tests/*"
    "${PROJECT_SOURCE_DIR}/benchmarks/*"
    "${PROJECT_SOURCE_DIR}/examples/*"
)
list(FILTER RAPID_STATS_FORMAT_FILES INCLUDE REGEX "\\.(hpp|cpp)$")

if(NOT RAPID_STATS_FORMAT_FILES)
    message(FATAL_ERROR "clang-format file list is empty")
endif()

set(RAPID_STATS_TIDY_FILES ${RAPID_STATS_FORMAT_FILES})
list(FILTER RAPID_STATS_TIDY_FILES INCLUDE REGEX "\\.cpp$")

find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${RAPID_STATS_FORMAT_FILES}
        COMMENT "Format C++ sources with clang-format"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${RAPID_STATS_FORMAT_FILES}
        COMMENT "Check C++ formatting"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()

if(CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            --warnings-as-errors=*
            ${RAPID_STATS_TIDY_FILES}
        COMMENT "Run clang-tidy"
        VERBATIM
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
endif()
