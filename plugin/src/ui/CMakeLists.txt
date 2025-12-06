#==============================================================================
# SidechainUI - UI Components Library
# All UI components and views
#==============================================================================

add_library(SidechainUI STATIC
    # UI - Auth
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/auth/Auth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/auth/Auth.h
    
    # UI - Messages
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/MessagesList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/MessagesList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/MessageThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/MessageThread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/MessageThreadComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/MessageThreadComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/AudioSnippetRecorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/AudioSnippetRecorder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/UserPickerDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/messages/UserPickerDialog.h
    
    # UI - Profile
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/profile/ProfileSetup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/profile/ProfileSetup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/profile/ProfileSetupComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/profile/Profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/profile/Profile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/profile/EditProfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/profile/EditProfile.h
    
    # UI - Feed
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/PostsFeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/PostsFeed.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/PostsFeedComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/PostCard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/PostCard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/Comment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/Comment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/EmojiReactionsPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/feed/EmojiReactionsPanel.h
    
    # UI - Recording
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/recording/Recording.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/recording/Recording.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/recording/Upload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/recording/Upload.h
    
    # UI - Notifications
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/notifications/NotificationBell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/notifications/NotificationBell.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/notifications/NotificationList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/notifications/NotificationList.h
    
    # UI - Social
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/social/UserCard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/social/UserCard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/social/UserDiscovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/social/UserDiscovery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/search/Search.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/search/Search.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/social/FollowersList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/social/FollowersList.h
    
    # UI - Stories
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/StoryRecording.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/StoryRecording.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/StoriesFeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/StoriesFeed.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/StoryViewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/StoryViewer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/PianoRoll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/PianoRoll.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/NoteWaterfall.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/NoteWaterfall.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/CircularVisualization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/stories/CircularVisualization.h
    
    # UI - Common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/common/Header.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/common/Header.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/ui/common/ConnectionIndicator.h
)

# Include directories
target_include_directories(SidechainUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source
)

# GTK and WebKit include directories (required by juce_gui_extra on Linux)
# JUCE should handle this automatically, but we need to ensure includes are propagated
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    # Add GTK and WebKit includes directly (they're needed for juce_gui_extra compilation)
    target_include_directories(SidechainUI PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    target_link_libraries(SidechainUI PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
endif()

# Dependencies: All other libraries
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
)

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
    set(PIC_ENABLED OFF)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(PIC_ENABLED ON)
    endif()
    set_target_properties(SidechainUI PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()

