# Documentation Build Configuration
# Generates documentation using Doxygen + Sphinx + Breathe + Exhale

cmake_minimum_required(VERSION 3.22)

# Find required tools
find_package(Doxygen QUIET)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen not found. Install with: brew install doxygen (macOS) or apt install doxygen (Linux)")
    message(WARNING "Documentation will not be built.")
    return()
endif()

# Check if Sphinx is available
# Priority: 1) Virtual environment (.venv), 2) System Python
set(SPHINX_FOUND FALSE)
set(VENV_PYTHON "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python3")
set(VENV_PYTHON_ALT "${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin/python")

# Build list of Python executables to try (venv first, then system)
set(PYTHON_EXECUTABLES)
if(EXISTS ${VENV_PYTHON})
    list(APPEND PYTHON_EXECUTABLES ${VENV_PYTHON})
    message(STATUS "Found virtual environment at ${CMAKE_CURRENT_SOURCE_DIR}/.venv")
elseif(EXISTS ${VENV_PYTHON_ALT})
    list(APPEND PYTHON_EXECUTABLES ${VENV_PYTHON_ALT})
    message(STATUS "Found virtual environment at ${CMAKE_CURRENT_SOURCE_DIR}/.venv")
endif()
# Add system Python executables
list(APPEND PYTHON_EXECUTABLES ${Python3_EXECUTABLE} python3 python)

foreach(PYTHON_EXE ${PYTHON_EXECUTABLES})
    if(EXISTS ${PYTHON_EXE})
        execute_process(
            COMMAND ${PYTHON_EXE} -m sphinx --version
            OUTPUT_VARIABLE SPHINX_VERSION
            ERROR_VARIABLE SPHINX_ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE SPHINX_RESULT
        )
        if(SPHINX_RESULT EQUAL 0)
            set(SPHINX_FOUND TRUE)
            set(SPHINX_PYTHON ${PYTHON_EXE})
            message(STATUS "Found Sphinx: ${SPHINX_VERSION} (using ${PYTHON_EXE})")
            break()
        endif()
    endif()
endforeach()

if(NOT SPHINX_FOUND)
    message(WARNING "Sphinx not found. Install with: pip install -r docs/requirements.txt")
    message(WARNING "Documentation will not be built.")
    return()
endif()

# Set paths
set(DOXYGEN_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../plugin/source")
set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
set(DOXYGEN_XML_DIR "${DOXYGEN_OUTPUT_DIR}/xml")
set(SPHINX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SPHINX_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")
set(DOXYFILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile")
set(DOXYFILE_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

# Configure Doxyfile with absolute path to plugin source
# This ensures Doxygen can find the source files regardless of where it's run from
configure_file(
    ${DOXYFILE_IN}
    ${DOXYFILE_OUT}
    @ONLY
)

# Custom command to generate Doxygen XML
add_custom_command(
    OUTPUT ${DOXYGEN_XML_DIR}/index.xml
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Doxygen XML documentation..."
    DEPENDS ${DOXYFILE_OUT}
    VERBATIM
)

# Custom target for Doxygen
add_custom_target(docs-doxygen
    DEPENDS ${DOXYGEN_XML_DIR}/index.xml
    COMMENT "Doxygen XML generation complete"
)

# Custom command to generate Sphinx HTML
add_custom_command(
    OUTPUT ${SPHINX_BUILD_DIR}/index.html
    COMMAND ${CMAKE_COMMAND} -E env DOXYGEN_XML_DIR=${DOXYGEN_XML_DIR}
        ${SPHINX_PYTHON} -m sphinx
        -b html
        -d ${CMAKE_CURRENT_BINARY_DIR}/doctrees
        ${SPHINX_SOURCE_DIR}
        ${SPHINX_BUILD_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Sphinx HTML documentation..."
    DEPENDS docs-doxygen
    VERBATIM
)

# Custom target for Sphinx
add_custom_target(docs-sphinx
    DEPENDS ${SPHINX_BUILD_DIR}/index.html
    COMMENT "Sphinx HTML generation complete"
)

# Combined documentation target
add_custom_target(docs
    DEPENDS docs-doxygen docs-sphinx
    COMMENT "Documentation build complete. Output: ${SPHINX_BUILD_DIR}"
)

# Cross-platform target to open documentation in browser
if(WIN32)
    # Windows - use start command (needs empty first arg for title)
    add_custom_target(docs-open
        COMMAND ${CMAKE_COMMAND} -E echo "Opening documentation in default browser..."
        COMMAND start "" "${SPHINX_BUILD_DIR}/index.html"
        DEPENDS docs
        COMMENT "Opening documentation: ${SPHINX_BUILD_DIR}/index.html"
        USES_TERMINAL
    )
elseif(APPLE)
    # macOS - use open command
    add_custom_target(docs-open
        COMMAND ${CMAKE_COMMAND} -E echo "Opening documentation in default browser..."
        COMMAND open "${SPHINX_BUILD_DIR}/index.html"
        DEPENDS docs
        COMMENT "Opening documentation: ${SPHINX_BUILD_DIR}/index.html"
        USES_TERMINAL
    )
else()
    # Linux and other Unix-like systems - use xdg-open with file:// URL
    add_custom_target(docs-open
        COMMAND ${CMAKE_COMMAND} -E echo "Opening documentation in default browser..."
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_SOURCE_DIR}" bash -c "xdg-open \"file://$$(realpath ${SPHINX_BUILD_DIR}/index.html)\""
        DEPENDS docs
        COMMENT "Opening documentation: ${SPHINX_BUILD_DIR}/index.html"
        USES_TERMINAL
    )
endif()

# Install target for documentation
install(
    DIRECTORY ${SPHINX_BUILD_DIR}/
    DESTINATION share/doc/sidechain
    OPTIONAL
)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Documentation Configuration")
message(STATUS "========================================")
message(STATUS "  Doxygen XML:  ${DOXYGEN_XML_DIR}")
message(STATUS "  Sphinx HTML:  ${SPHINX_BUILD_DIR}")
message(STATUS "  Build target: docs")
message(STATUS "  Open target:  docs-open")
message(STATUS "========================================")
message(STATUS "")
