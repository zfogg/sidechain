#==============================================================================
# SidechainUIFeed - Feed UI Components
#==============================================================================

add_library(SidechainUIFeed STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/PostsFeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/PostsFeed.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/PostCard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/PostCard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/Comment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/Comment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/EmojiReactionsPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/feed/EmojiReactionsPanel.h
)

target_include_directories(SidechainUIFeed PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source
)

target_link_libraries(SidechainUIFeed PUBLIC
    SidechainUtil
    SidechainModels
    SidechainNetwork
    SidechainStores
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
    set_target_properties(SidechainUIFeed PROPERTIES
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
    set_target_properties(SidechainUIFeed PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()
