#==============================================================================
# SidechainStores - State Management Library
# State management and caching
#==============================================================================

add_library(SidechainStores STATIC
    # Core Store Framework
    ${CMAKE_CURRENT_SOURCE_DIR}/Store.h

    # Unified AppStore (Phase 3 - replaces all individual stores)
    ${CMAKE_CURRENT_SOURCE_DIR}/AppStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/AppState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Auth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Feed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/User.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Chat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Search.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Notifications.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Presence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Stories.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Upload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Playlists.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Challenges.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Sounds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/Draft.cpp
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
    rxcpp
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

