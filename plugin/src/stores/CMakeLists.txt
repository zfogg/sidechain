#==============================================================================
# SidechainStores - State Management Library
# State management and caching
#==============================================================================

add_library(SidechainStores STATIC
    # Legacy stores (to be deprecated)
    ${CMAKE_CURRENT_SOURCE_DIR}/UserDataStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UserDataStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FeedDataManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FeedDataManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DraftStorage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DraftStorage.h

    # Reactive stores (Phase 2)
    ${CMAKE_CURRENT_SOURCE_DIR}/Store.h
    ${CMAKE_CURRENT_SOURCE_DIR}/FeedStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FeedStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/UserStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UserStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatStore.h
)

# Include directories
target_include_directories(SidechainStores PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
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
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Enable PIC for Release builds, disable for Debug (faster compilation)
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainStores PROPERTIES
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
    set_target_properties(SidechainStores PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()

