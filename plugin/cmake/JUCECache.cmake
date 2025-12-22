#==============================================================================
# JUCE Object File Caching
# Pre-compiles JUCE modules as OBJECT libraries to .cache/ directory
# Object files physically reside in cache and survive plugin/build/ deletion
#==============================================================================

message(STATUS "JUCE object caching: ENABLED (always on)")

# Create JUCE object cache directory
set(JUCE_OBJECT_CACHE_DIR "${SIDECHAIN_DEPS_CACHE_DIR}/juce_objects")
file(MAKE_DIRECTORY "${JUCE_OBJECT_CACHE_DIR}")

message(STATUS "  JUCE object cache: ${JUCE_OBJECT_CACHE_DIR}")

# Track which modules to cache
set(JUCE_MODULES_TO_CACHE
    juce_audio_basics
    juce_audio_devices
    juce_audio_formats
    juce_audio_processors
    juce_audio_utils
    juce_core
    juce_cryptography
    juce_data_structures
    juce_dsp
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
    CACHE INTERNAL ""
)

# Set JUCE source directory for wrapper
set(JUCE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE" CACHE INTERNAL "")

# Find platform-specific dependencies for JUCE modules (Linux needs GTK)
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)

    # Pass GTK include directories to wrapper CMakeLists.txt
    set(JUCE_CACHE_GTK_INCLUDES "${GTK3_INCLUDE_DIRS};${WEBKIT_INCLUDE_DIRS}" CACHE INTERNAL "")
endif()

# Add subdirectory with custom binary dir = cache
# This forces CMake to place object files in cache instead of build/
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/juce_cache"  # Source (wrapper CMakeLists.txt)
    "${JUCE_OBJECT_CACHE_DIR}"                       # Binary dir (where .o files go)
    EXCLUDE_FROM_ALL
)

# INTERFACE_SOURCES clearing is done AFTER SidechainJuceHeaderHelper is created
# See plugin/CMakeLists.txt after line 230

#==============================================================================
# Function: Link a target against cached JUCE objects
# Call this after juce_add_plugin() or other target creation
#==============================================================================
function(juce_link_cached TARGET_NAME)
    get_property(CACHED_LIBS GLOBAL PROPERTY JUCE_CACHED_OBJECT_LIBS)

    if(NOT CACHED_LIBS)
        return()
    endif()

    # For each JUCE module the target links, add the cached objects
    foreach(MODULE IN LISTS JUCE_MODULES_TO_CACHE)
        get_property(CACHED_LIB GLOBAL PROPERTY JUCE_CACHED_${MODULE})

        if(NOT CACHED_LIB)
            continue()
        endif()

        # Check if target links this JUCE module
        get_target_property(TARGET_LIBS ${TARGET_NAME} LINK_LIBRARIES)
        if(TARGET_LIBS AND "juce::${MODULE}" IN_LIST TARGET_LIBS)
            # Add pre-compiled cached objects
            target_sources(${TARGET_NAME} PRIVATE $<TARGET_OBJECTS:${CACHED_LIB}>)

            # Ensure cached lib is built first
            add_dependencies(${TARGET_NAME} ${CACHED_LIB})

            message(STATUS "    ${TARGET_NAME} will use cached ${MODULE} objects")
        endif()
    endforeach()
endfunction()

message(STATUS "JUCE object caching configured - modules will build to cache")
