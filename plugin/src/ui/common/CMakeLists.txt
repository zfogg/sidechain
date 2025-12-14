#==============================================================================
# SidechainUICommon - Common UI Components
#==============================================================================

add_library(SidechainUICommon STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Header.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Header.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ConnectionIndicator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ErrorState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ErrorState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ToastNotification.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ToastNotification.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SkeletonLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SkeletonLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveformImageView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveformImageView.h
)

target_include_directories(SidechainUICommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(SidechainUICommon PUBLIC
    SidechainUtil
    SidechainModels
    SidechainStores
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
    set_target_properties(SidechainUICommon PROPERTIES
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
    set_target_properties(SidechainUICommon PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()
