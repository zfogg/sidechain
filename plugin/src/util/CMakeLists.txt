#==============================================================================
# SidechainUtil - Utility Library
# Base utilities with minimal dependencies
#==============================================================================

add_library(SidechainUtil STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Colors.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/HttpErrorHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Json.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Validate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Validate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Async.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Result.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/UIHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/UIHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/StringFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/StringFormatter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/TextEditorStyler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/TextEditorStyler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Animation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/Animation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/HoverState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/util/LongPressDetector.h
)

# Include directories - use source root so includes work as "util/Log.h"
target_include_directories(SidechainUtil PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source
)

# Link JuceHeader target to get generated header (generated during CMake config)
target_link_libraries(SidechainUtil PUBLIC SidechainJuceHeader)
# Header generation happens during CMake configuration, not build

# Link JUCE core modules (minimal dependencies)
target_link_libraries(SidechainUtil PUBLIC
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
)

# Set visibility to hidden by default (faster linking, smaller binaries)
set_target_properties(SidechainUtil PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON  # Required for linking into shared libraries (VST plugins)
)

