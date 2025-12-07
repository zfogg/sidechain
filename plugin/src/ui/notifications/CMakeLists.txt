#==============================================================================
# SidechainUINotifications - Notifications UI Components
#==============================================================================

add_library(SidechainUINotifications STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/notifications/NotificationBell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/notifications/NotificationBell.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/notifications/NotificationList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source/ui/notifications/NotificationList.h
)

target_include_directories(SidechainUINotifications PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../source
)

target_link_libraries(SidechainUINotifications PUBLIC
    SidechainUtil
    SidechainModels
    SidechainNetwork
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
    set_target_properties(SidechainUINotifications PROPERTIES
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
    set_target_properties(SidechainUINotifications PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()
