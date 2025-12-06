# Sidechain VST Plugin - CMake Build Configuration
# This replaces the Projucer-based build system

cmake_minimum_required(VERSION 3.22)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# Default to Debug if no build type specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

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

#==============================================================================
# Build formats configuration
#==============================================================================
# Build formats - conditionally include Standalone
# VST3 is always built, AU only on macOS, Standalone only if option is enabled
set(PLUGIN_FORMATS "VST3")
if(APPLE)
    list(APPEND PLUGIN_FORMATS "AU")
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
    # JUCE modules will automatically link their system dependencies (GTK, etc.)
    juce::juce_recommended_config_flags
)

# GUI modules are now included - GTK is installed so they can compile

# Generate the header - this creates JuceLibraryCode directory with JuceHeader.h
juce_generate_juce_header(SidechainJuceHeaderHelper)

# Create an interface library to propagate the include directory
add_library(SidechainJuceHeader INTERFACE)
target_include_directories(SidechainJuceHeader INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/SidechainJuceHeaderHelper_artefacts/JuceLibraryCode
)

# The header is generated during CMake configuration by juce_generate_juce_header
# (it doesn't require building the helper executable - juce_generate_juce_header runs at configure time)
# The header should already exist after CMake configuration completes
# No need to add build-time dependencies - the header is available immediately after configuration

#==============================================================================
# Build libraries in dependency order
#==============================================================================
# Libraries must be built in dependency order:
# 1. Util (base - no dependencies)
# 2. Models (depends on Util)
# 3. Network (depends on Util)
# 4. Audio (depends on Util, Network)
# 5. Stores (depends on Util, Models, Network)
# 6. UI (depends on all above)
# 7. Core plugin (depends on all libraries)

add_subdirectory(src/util)
add_subdirectory(src/models)
add_subdirectory(src/network)
add_subdirectory(src/audio)
add_subdirectory(src/stores)
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
