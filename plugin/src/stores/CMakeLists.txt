#==============================================================================
# SidechainStores - State Management Library
# State management and caching
#==============================================================================

add_library(SidechainStores STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/stores/UserDataStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/stores/UserDataStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/stores/FeedDataManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/stores/FeedDataManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/stores/ImageCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/stores/ImageCache.h
)

# Include directories
target_include_directories(SidechainStores PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source
)

# Dependencies
target_link_libraries(SidechainStores PUBLIC
    SidechainUtil
    SidechainModels
    SidechainNetwork
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_events
)

set_target_properties(SidechainStores PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON  # Required for linking into shared libraries (VST plugins)
)

