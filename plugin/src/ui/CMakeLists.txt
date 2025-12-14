#==============================================================================
# SidechainUI - UI Components Library
# All UI components and views
#==============================================================================

# Add subdirectories for each UI module
add_subdirectory(auth)
add_subdirectory(challenges)
add_subdirectory(common)
add_subdirectory(feed)
add_subdirectory(messages)
add_subdirectory(notifications)
add_subdirectory(playlists)
add_subdirectory(profile)
add_subdirectory(recording)
add_subdirectory(search)
add_subdirectory(social)
add_subdirectory(sounds)
add_subdirectory(stories)
add_subdirectory(synth)

# Create the main UI library that links all submodules
add_library(SidechainUI STATIC)

# Include directories
target_include_directories(SidechainUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link all UI submodules
target_link_libraries(SidechainUI PUBLIC
    SidechainUIAuth
    SidechainUIChallenges
    SidechainUICommon
    SidechainUIFeed
    SidechainUIMessages
    SidechainUINotifications
    SidechainUIPlaylists
    SidechainUIProfile
    SidechainUIRecording
    SidechainUISearch
    SidechainUISocial
    SidechainUISounds
    SidechainUIStories
    SidechainUISynth
)

# Dependencies: All other libraries (propagated from submodules)
target_link_libraries(SidechainUI PUBLIC
    SidechainUtil
    SidechainModels
    SidechainNetwork
    SidechainAudio
    SidechainStores
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# GTK and WebKit include directories (required by juce_gui_extra on Linux)
# JUCE should handle this automatically, but we need to ensure includes are propagated
# Apply to all UI submodules
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    # Add GTK and WebKit includes directly (they're needed for juce_gui_extra compilation)
    target_include_directories(SidechainUI PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    target_link_libraries(SidechainUI PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
    # Also apply to all submodules
    foreach(UI_MODULE IN ITEMS
        SidechainUIAuth
        SidechainUIChallenges
        SidechainUICommon
        SidechainUIFeed
        SidechainUIMessages
        SidechainUINotifications
        SidechainUIPlaylists
        SidechainUIProfile
        SidechainUIRecording
        SidechainUISearch
        SidechainUISocial
        SidechainUISounds
        SidechainUIStories
        SidechainUISynth
    )
        target_include_directories(${UI_MODULE} PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
        target_link_libraries(${UI_MODULE} PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
    endforeach()
endif()

# Enable PIC for Release builds, disable for Debug (faster compilation)
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainUI PROPERTIES
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
    set_target_properties(SidechainUI PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()

