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
# clang-tidy targets
# Uses run-clang-tidy.py for parallel execution on all files
# Uses clang-tidy-diff.py for checking only changed lines (useful for PRs)
#==============================================================================
if(CLANG_TIDY_EXECUTABLE)
    # Try to find scripts in local scripts directory first, then in PATH
    find_program(RUN_CLANG_TIDY_PY NAMES run-clang-tidy.py run-clang-tidy
        PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../scripts" NO_DEFAULT_PATH)
    if(NOT RUN_CLANG_TIDY_PY)
        find_program(RUN_CLANG_TIDY_PY NAMES run-clang-tidy.py run-clang-tidy)
    endif()

    find_program(CLANG_TIDY_DIFF_PY NAMES clang-tidy-diff.py
        PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../scripts" NO_DEFAULT_PATH)
    if(NOT CLANG_TIDY_DIFF_PY)
        find_program(CLANG_TIDY_DIFF_PY NAMES clang-tidy-diff.py)
    endif()

    if(RUN_CLANG_TIDY_PY)
        add_custom_target(tidy
            COMMAND ${CMAKE_COMMAND} -E echo "Running clang-tidy in parallel on all files..."
            COMMAND ${RUN_CLANG_TIDY_PY}
                -p ${CMAKE_BINARY_DIR}
                -fix
                -j ${CMAKE_CPU_CORES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running clang-tidy with automatic fixes"
            VERBATIM
        )

        add_custom_target(tidy-check
            COMMAND ${CMAKE_COMMAND} -E echo "Checking code with clang-tidy in parallel..."
            COMMAND ${RUN_CLANG_TIDY_PY}
                -p ${CMAKE_BINARY_DIR}
                -j ${CMAKE_CPU_CORES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running clang-tidy (check mode - errors if issues found)"
            VERBATIM
        )
    else()
        add_custom_target(tidy
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "run-clang-tidy.py not found. Install LLVM tools."
            COMMAND exit 1
        )
        add_custom_target(tidy-check
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "run-clang-tidy.py not found. Install LLVM tools."
            COMMAND exit 1
        )
    endif()

    if(CLANG_TIDY_DIFF_PY)
        add_custom_target(tidy-diff
            COMMAND ${CMAKE_COMMAND} -E echo "Checking only changed lines with clang-tidy..."
            COMMAND bash -c "cd ${CMAKE_CURRENT_SOURCE_DIR}/.. && git diff -U0 --no-color HEAD | ${CLANG_TIDY_DIFF_PY} -p1 -p ${CMAKE_BINARY_DIR}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running clang-tidy-diff (check changed lines only)"
            VERBATIM
        )
    else()
        add_custom_target(tidy-diff
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "clang-tidy-diff.py not found. Install LLVM tools."
            COMMAND exit 1
        )
    endif()
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "clang-tidy not found. Install it first."
        COMMAND exit 1
    )
    add_custom_target(tidy-check
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --red "clang-tidy not found. Install it first."
        COMMAND exit 1
    )
    add_custom_target(tidy-diff
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
