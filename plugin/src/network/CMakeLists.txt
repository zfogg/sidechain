#==============================================================================
# SidechainNetwork - Network Library
# API clients and WebSocket functionality
#==============================================================================

add_library(SidechainNetwork STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/NetworkClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NetworkClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WebSocketClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WebSocketClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamChatClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StreamChatClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/asio_compat.h
    # API client implementations (split from NetworkClient.cpp for maintainability)
    ${CMAKE_CURRENT_SOURCE_DIR}/api/AuthClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/AudioClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/FeedClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/SocialClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/UploadDownloadClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/StoriesClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/CommentsClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/ProfileClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/NotificationClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/DiscoveryClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/SearchClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/ChallengesClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/PlaylistClient.cpp
)

# Include directories
target_include_directories(SidechainNetwork PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Dependencies: Util library and external networking libraries
target_link_libraries(SidechainNetwork PUBLIC
    SidechainUtil
    juce::juce_core
    juce::juce_events
    juce::juce_cryptography
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# External dependencies (optional)
if(SIDECHAIN_HAS_ASIO)
    target_link_libraries(SidechainNetwork PRIVATE asio)
endif()

if(SIDECHAIN_HAS_WEBSOCKETPP)
    # Link as PUBLIC so dependents (like SidechainUI) can use websocketpp types
    target_link_libraries(SidechainNetwork PUBLIC websocketpp)
endif()

# Enable PIC for Release builds, disable for Debug (faster compilation)
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainNetwork PROPERTIES
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
    set_target_properties(SidechainNetwork PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()

