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

# Enable PIC for Release builds, disable for Debug (faster compilation)
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainModels PROPERTIES
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
    set_target_properties(SidechainModels PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()

