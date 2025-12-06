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

set_target_properties(SidechainNetwork PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON  # Required for linking into shared libraries (VST plugins)
)

