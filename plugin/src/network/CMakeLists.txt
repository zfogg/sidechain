#==============================================================================
# SidechainNetwork - Network Library
# API clients and WebSocket functionality
#==============================================================================

add_library(SidechainNetwork STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/network/NetworkClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/network/NetworkClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/network/WebSocketClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/network/WebSocketClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/network/StreamChatClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/network/StreamChatClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/network/asio_compat.h
)

# Include directories
target_include_directories(SidechainNetwork PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source
)

# Dependencies: Util library and external networking libraries
target_link_libraries(SidechainNetwork PUBLIC
    SidechainUtil
    juce::juce_core
    juce::juce_events
    juce::juce_cryptography
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

