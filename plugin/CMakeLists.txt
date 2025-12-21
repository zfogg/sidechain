# Sidechain VST Plugin - CMake Build Configuration
# This replaces the Projucer-based build system

cmake_minimum_required(VERSION 3.22)

#==============================================================================
# Windows Toolchain Setup (MUST be before project())
# Sets up MSVC and Windows SDK paths for standalone builds
#==============================================================================
include(cmake/WindowsToolchain.cmake)

# Prefer Ninja generator for faster builds (auto-parallelizes)
# Note: CMAKE_GENERATOR is determined when CMake starts and cannot be changed from CMakeLists.txt
# Use -G Ninja on command line, CMakePresets.json, or the Makefile which auto-detects Ninja
find_program(NINJA_EXECUTABLE ninja)
if(CMAKE_GENERATOR STREQUAL "Ninja" OR CMAKE_GENERATOR MATCHES "Ninja")
    message(STATUS "Using Ninja generator for faster parallel builds")
elseif(NINJA_EXECUTABLE AND NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(STATUS "Ninja is available. For faster builds, specify: cmake -G Ninja ...")
    message(STATUS "Or use: make plugin-configure (auto-detects Ninja)")
elseif(NOT NINJA_EXECUTABLE)
    message(STATUS "Ninja not found. Install it for faster parallel builds:")
    if(UNIX AND NOT APPLE)
        message(STATUS "  sudo apt install ninja-build  # Ubuntu/Debian")
        message(STATUS "  sudo pacman -S ninja           # Arch")
    elseif(APPLE)
        message(STATUS "  brew install ninja")
    endif()
endif()

#==============================================================================
# ccache Support for Faster Incremental Builds
# Caches compiled object files to speed up recompilation
#==============================================================================
find_program(CCACHE_EXECUTABLE ccache)
if(CCACHE_EXECUTABLE)
    message(STATUS "Found ccache: ${CCACHE_EXECUTABLE}")
    message(STATUS "Enabling ccache for faster incremental builds")

    # Set ccache as compiler launcher
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")

    # Optional: Set ccache options via environment variables or directly
    # Increase cache size if needed (default is often 5GB)
    # You can configure this with: ccache -M 10G

    # Show ccache statistics after build with: ccache -s
else()
    message(STATUS "ccache not found. Install it for faster incremental builds:")
    if(UNIX AND NOT APPLE)
        message(STATUS "  sudo apt install ccache        # Ubuntu/Debian")
        message(STATUS "  sudo pacman -S ccache          # Arch")
        message(STATUS "  sudo dnf install ccache        # Fedora")
    elseif(APPLE)
        message(STATUS "  brew install ccache")
    elseif(WIN32)
        message(STATUS "  choco install ccache           # Chocolatey")
        message(STATUS "  Or download from: https://ccache.dev/download.html")
    endif()
endif()

#==============================================================================
# Version from git tags (via scripts/version.sh)
# NOTE: Must run BEFORE project() so we can set PROJECT_VERSION
#==============================================================================
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh" --major
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION_MAJOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh" --minor
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION_MINOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh" --patch
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION_PATCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback to 0.0.0 if script fails
if(NOT SIDECHAIN_VERSION OR SIDECHAIN_VERSION STREQUAL "")
    set(SIDECHAIN_VERSION "0.0.0")
    set(SIDECHAIN_VERSION_MAJOR "0")
    set(SIDECHAIN_VERSION_MINOR "0")
    set(SIDECHAIN_VERSION_PATCH "0")
endif()

message(STATUS "Sidechain version: ${SIDECHAIN_VERSION}")

# Use the git-derived version for the CMake project
project(Sidechain VERSION ${SIDECHAIN_VERSION} LANGUAGES CXX C)

# Generate Version.h from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# Default to Debug if no build type specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# PIC is required for all static libraries since they link into a VST3 shared library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Export compile_commands.json for IDE support (clangd, etc.)
# This can be overridden by setting CMAKE_EXPORT_COMPILE_COMMANDS=OFF
if(NOT DEFINED CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for IDE support" FORCE)
endif()

#==============================================================================
# Include CMake helper files
#==============================================================================
include(cmake/Options.cmake)
include(cmake/CompilerFlags.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Install.cmake)
include(cmake/CPUCores.cmake)
include(cmake/Linting.cmake)

#==============================================================================
# Build formats configuration
#==============================================================================
# Build formats - conditionally include AU and Standalone
# VST3 is always built, AU and Standalone are optional
set(PLUGIN_FORMATS "VST3")
if(APPLE AND SIDECHAIN_BUILD_AU)
    list(APPEND PLUGIN_FORMATS "AU")
    message(STATUS "AU format will be built")
elseif(APPLE)
    message(STATUS "AU format disabled (enable with -DSIDECHAIN_BUILD_AU=ON for faster builds, skip AU)")
endif()
if(SIDECHAIN_BUILD_STANDALONE)
    list(APPEND PLUGIN_FORMATS "Standalone")
    message(STATUS "Standalone app will be built")
else()
    message(STATUS "Standalone app disabled (enable with -DSIDECHAIN_BUILD_STANDALONE=ON)")
endif()

#==============================================================================
# Generate JuceHeader.h early for library use
#==============================================================================
# Create a minimal JUCE GUI application to generate JuceHeader.h before libraries need it
# This allows library source files to #include <JuceHeader.h>
# We use juce_add_gui_app since we need GUI modules for header generation
juce_add_gui_app(SidechainJuceHeaderHelper
    COMPANY_NAME "Joopal"
    PRODUCT_NAME "SidechainJuceHeaderHelper"
    VERSION 1.0.0
)

# We only need this target for header generation, not for building
# Exclude it from default build targets
set_target_properties(SidechainJuceHeaderHelper PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Suppress JUCE warnings for helper
suppress_juce_warnings(SidechainJuceHeaderHelper)

target_sources(SidechainJuceHeaderHelper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/JuceHeaderHelper.cpp
)

target_link_libraries(SidechainJuceHeaderHelper PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_cryptography
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    # All modules linked to generate complete header
    juce::juce_recommended_config_flags
)

# Add GTK includes for Linux (required by juce_gui_extra)
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    pkg_check_modules(CURL REQUIRED libcurl)
    target_include_directories(SidechainJuceHeaderHelper PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    target_link_libraries(SidechainJuceHeaderHelper PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES} ${CURL_LIBRARIES})
endif()

# Generate the header - this creates JuceLibraryCode directory with JuceHeader.h
juce_generate_juce_header(SidechainJuceHeaderHelper)

# Create an interface library to propagate the include directory
add_library(SidechainJuceHeader INTERFACE)
target_include_directories(SidechainJuceHeader INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/SidechainJuceHeaderHelper_artefacts/JuceLibraryCode
)

# The header is generated at BUILD time by juce::juceaide (not configure time).
# We must ensure SidechainJuceHeaderHelper builds before any library that includes JuceHeader.h.
# This dependency propagates through the INTERFACE library.
add_dependencies(SidechainJuceHeader SidechainJuceHeaderHelper)

#==============================================================================
# Global Coverage Configuration
#==============================================================================
# Apply coverage flags globally when SIDECHAIN_ENABLE_COVERAGE is set
# This ensures ALL compiled object files have coverage instrumentation,
# allowing lcov to capture coverage for the entire linked executable
if(SIDECHAIN_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Apply coverage flags to all compilation targets
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    endif()
endif()

#==============================================================================
# Build libraries in dependency order (4-library architecture)
#==============================================================================
# Libraries are built in dependency order:
# 1. Core (util + models + security - no other deps)
# 2. Network (network + stores - depends on Core)
# 3. Audio (depends on Core, Network)
# 4. UI (depends on Core, Network)
# 5. Core plugin (depends on all libraries)

add_subdirectory(src/util)
add_subdirectory(src/network)
add_subdirectory(src/audio)
add_subdirectory(src/ui)
add_subdirectory(src/core)


#==============================================================================
# Unit Tests with Catch2
#==============================================================================
include(cmake/Tests.cmake)

#==============================================================================
# Documentation (Doxygen + Sphinx)
#==============================================================================
if(SIDECHAIN_BUILD_DOCS)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../docs" "${CMAKE_BINARY_DIR}/docs" EXCLUDE_FROM_ALL)
    message(STATUS "Documentation build enabled")
else()
    message(STATUS "Documentation build disabled (enable with -DSIDECHAIN_BUILD_DOCS=ON)")
endif()
