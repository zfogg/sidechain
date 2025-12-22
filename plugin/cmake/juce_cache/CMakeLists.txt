# JUCE Object Cache Builder
# This CMakeLists.txt is added as a subdirectory with custom binary dir
# to force object files into the cache directory

cmake_minimum_required(VERSION 3.22)

# Include parent scope variables we need
# These are passed via parent scope or cache

# Build each JUCE module as an OBJECT library
foreach(MODULE IN LISTS JUCE_MODULES_TO_CACHE)
    set(MODULE_DIR "${JUCE_SOURCE_DIR}/modules/${MODULE}")

    # Check if module has .cpp (not header-only)
    if(NOT EXISTS "${MODULE_DIR}/${MODULE}.cpp")
        continue()
    endif()

    set(TARGET_NAME "juce_${MODULE}_objects")

    # Skip if already exists
    if(TARGET ${TARGET_NAME})
        continue()
    endif()

    # Create OBJECT library
    add_library(${TARGET_NAME} OBJECT EXCLUDE_FROM_ALL)

    # Add sources
    target_sources(${TARGET_NAME} PRIVATE "${MODULE_DIR}/${MODULE}.cpp")

    if(EXISTS "${MODULE_DIR}/${MODULE}_CompilationTime.cpp")
        target_sources(${TARGET_NAME} PRIVATE "${MODULE_DIR}/${MODULE}_CompilationTime.cpp")
    endif()

    # Special cases
    if(MODULE STREQUAL "juce_audio_processors")
        foreach(EXTRA ara lv2_libs)
            if(EXISTS "${MODULE_DIR}/${MODULE}_${EXTRA}.cpp")
                target_sources(${TARGET_NAME} PRIVATE "${MODULE_DIR}/${MODULE}_${EXTRA}.cpp")
            endif()
        endforeach()
    elseif(MODULE STREQUAL "juce_graphics")
        if(EXISTS "${MODULE_DIR}/${MODULE}_Harfbuzz.cpp")
            target_sources(${TARGET_NAME} PRIVATE "${MODULE_DIR}/${MODULE}_Harfbuzz.cpp")
        endif()
        if(EXISTS "${MODULE_DIR}/${MODULE}_Sheenbidi.c")
            target_sources(${TARGET_NAME} PRIVATE "${MODULE_DIR}/${MODULE}_Sheenbidi.c")
        endif()
    endif()

    # Copy properties from juce::${MODULE}
    # Note: We only copy compile flags and includes, NOT LINK_LIBRARIES
    # This prevents each module from compiling its dependencies' sources (which would cause duplicate symbols)
    if(TARGET juce::${MODULE})
        foreach(PROP INTERFACE_COMPILE_DEFINITIONS INTERFACE_COMPILE_OPTIONS
                     INTERFACE_INCLUDE_DIRECTORIES)
            get_target_property(VAL juce::${MODULE} ${PROP})
            if(VAL)
                string(REPLACE "INTERFACE_" "" TPROP "${PROP}")
                set_property(TARGET ${TARGET_NAME} PROPERTY ${TPROP} ${VAL})
            endif()
        endforeach()
    endif()

    # Add JUCE global module settings definition (required by JUCE)
    target_compile_definitions(${TARGET_NAME} PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_MODULE_AVAILABLE_${MODULE}=1
    )

    # Add platform-specific includes (Linux GTK)
    if(JUCE_CACHE_GTK_INCLUDES)
        target_include_directories(${TARGET_NAME} PRIVATE ${JUCE_CACHE_GTK_INCLUDES})
    endif()

    # Set properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    # Export to parent
    set_property(GLOBAL APPEND PROPERTY JUCE_CACHED_OBJECT_LIBS ${TARGET_NAME})
    set_property(GLOBAL PROPERTY JUCE_CACHED_${MODULE} ${TARGET_NAME})
endforeach()
