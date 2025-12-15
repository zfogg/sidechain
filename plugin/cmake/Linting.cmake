#==============================================================================
# Linting and Code Formatting Targets
# Provides targets for clang-tidy (linting) and clang-format (formatting)
#==============================================================================

# Find clang-tidy
find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
if(NOT CLANG_TIDY_EXECUTABLE)
    message(WARNING "clang-tidy not found. Install with: apt install clang-tools (Linux) or brew install clang-tools (macOS)")
else()
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXECUTABLE}")
endif()

# Find clang-format
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(NOT CLANG_FORMAT_EXECUTABLE)
    message(WARNING "clang-format not found. Install with: apt install clang-tools (Linux) or brew install clang-tools (macOS)")
else()
    message(STATUS "Found clang-format: ${CLANG_FORMAT_EXECUTABLE}")
endif()

#==============================================================================
# Helper function to collect all source files
#==============================================================================
function(collect_source_files OUTPUT_VAR)
    file(GLOB_RECURSE SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.hpp"
    )
    set(${OUTPUT_VAR} ${SOURCE_FILES} PARENT_SCOPE)
endfunction()

#==============================================================================
# clang-tidy target
# Runs clang-tidy on all source files
#==============================================================================
if(CLANG_TIDY_EXECUTABLE)
    collect_source_files(TIDY_SOURCES)

    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo "Running clang-tidy..."
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_BINARY_DIR}
            --fix
            --fix-errors
            ${TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy with automatic fixes"
        VERBATIM
    )

    add_custom_target(tidy-check
        COMMAND ${CMAKE_COMMAND} -E echo "Checking code with clang-tidy..."
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_BINARY_DIR}
            --warnings-as-errors=*
            ${TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy (check mode - errors if issues found)"
        VERBATIM
    )
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "clang-tidy not found. Install it first."
        COMMAND exit 1
    )
    add_custom_target(tidy-check
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "clang-tidy not found. Install it first."
        COMMAND exit 1
    )
endif()

#==============================================================================
# clang-format targets
# format: Apply clang-format in-place to all source files
# format-check: Check formatting without modifying files (errors if not formatted)
#==============================================================================
if(CLANG_FORMAT_EXECUTABLE)
    collect_source_files(FORMAT_SOURCES)

    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "Formatting code with clang-format..."
        COMMAND ${CLANG_FORMAT_EXECUTABLE}
            -i
            -style=file
            ${FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting all source files in-place"
        VERBATIM
    )

    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E echo "Checking code formatting with clang-format..."
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckFormat.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking code formatting (errors if not formatted)"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "clang-format not found. Install it first."
        COMMAND exit 1
    )
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "clang-format not found. Install it first."
        COMMAND exit 1
    )
endif()
