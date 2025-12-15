#==============================================================================
# Check Code Formatting
# Verifies that all source files are properly formatted with clang-format
# Fails if any file is not formatted correctly
#==============================================================================

find_program(CLANG_FORMAT_EXECUTABLE clang-format REQUIRED)

# Collect all source files
file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.hpp"
)

set(UNFORMATTED_FILES "")

foreach(SOURCE_FILE ${SOURCE_FILES})
    # Get the formatted version
    execute_process(
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -style=file:${CMAKE_CURRENT_SOURCE_DIR}/.clang-format "${SOURCE_FILE}"
        OUTPUT_VARIABLE FORMATTED_CONTENT
        RESULT_VARIABLE FORMAT_RESULT
    )

    if(NOT FORMAT_RESULT EQUAL 0)
        message(WARNING "Failed to format ${SOURCE_FILE}")
        continue()
    endif()

    # Read the current file
    file(READ "${SOURCE_FILE}" CURRENT_CONTENT)

    # Compare
    if(NOT FORMATTED_CONTENT STREQUAL CURRENT_CONTENT)
        list(APPEND UNFORMATTED_FILES "${SOURCE_FILE}")
    endif()
endforeach()

if(UNFORMATTED_FILES)
    set(ERROR_MSG "The following files are not properly formatted:\n")
    foreach(FILE ${UNFORMATTED_FILES})
        string(APPEND ERROR_MSG "  ${FILE}\n")
    endforeach()
    string(APPEND ERROR_MSG "\nRun 'cmake --build <build-dir> --target format' to fix.")
    message(FATAL_ERROR "${ERROR_MSG}")
endif()

message(STATUS "All source files are properly formatted ✓")
