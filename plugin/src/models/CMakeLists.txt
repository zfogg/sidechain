#==============================================================================
# SidechainModels - Data Models Library
# Data structures and DTOs
#==============================================================================

add_library(SidechainModels STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/models/FeedPost.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/models/FeedPost.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/models/FeedResponse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/models/AggregatedFeedGroup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/models/AggregatedFeedResponse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/models/Story.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/models/Story.h
)

# Include directories
target_include_directories(SidechainModels PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source
)

# Dependencies: Util library
target_link_libraries(SidechainModels PUBLIC
    SidechainUtil
    juce::juce_core
)

set_target_properties(SidechainModels PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON  # Required for linking into shared libraries (VST plugins)
)

