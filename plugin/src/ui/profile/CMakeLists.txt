#==============================================================================
# SidechainUIProfile - Profile UI Components
#==============================================================================

add_library(SidechainUIProfile STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ProfileSetup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ProfileSetup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Profile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EditProfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EditProfile.h
)

target_include_directories(SidechainUIProfile PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(SidechainUIProfile PUBLIC
    SidechainUtil
    SidechainModels
    SidechainNetwork
    SidechainStores
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
    set_target_properties(SidechainUIProfile PROPERTIES
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
    set_target_properties(SidechainUIProfile PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()
