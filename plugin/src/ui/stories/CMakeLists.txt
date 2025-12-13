#==============================================================================
# SidechainUIStories - Stories UI Components
#==============================================================================

add_library(SidechainUIStories STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/StoryRecording.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StoryRecording.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StoriesFeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StoriesFeed.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StoryViewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StoryViewer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PianoRoll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PianoRoll.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NoteWaterfall.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NoteWaterfall.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CircularVisualization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CircularVisualization.h
)

target_include_directories(SidechainUIStories PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(SidechainUIStories PUBLIC
    SidechainUtil
    SidechainModels
    SidechainNetwork
    SidechainAudio
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Enable PIC for Release builds, disable for Debug (faster compilation)
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainUIStories PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>"
    )
else()
    # Single-config generator (Unix Makefiles, Ninja) - check CMAKE_BUILD_TYPE
    set(PIC_ENABLED ON)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(PIC_ENABLED ON)
    endif()
    set_target_properties(SidechainUIStories PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()
