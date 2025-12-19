#==============================================================================
# SidechainUI - All UI Components Library
# Consolidated from 13 separate UI module libraries into one
#==============================================================================

# Collect all source files from UI subdirectories
set(UI_SOURCES)

# Animations
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/AnimationController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/AnimationController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/AnimationTimeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/Easing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/NavigationStack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/NavigationStack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/TransitionAnimation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/animations/ViewTransitionManager.h
)

# Auth
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/auth/Auth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/auth/Auth.h
)

# Challenges
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/challenges/MidiChallengeDetail.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/challenges/MidiChallengeDetail.h
    ${CMAKE_CURRENT_SOURCE_DIR}/challenges/MidiChallenges.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/challenges/MidiChallenges.h
    ${CMAKE_CURRENT_SOURCE_DIR}/challenges/MidiChallengeSubmission.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/challenges/MidiChallengeSubmission.h
)

# Common
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/common/Avatar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/Avatar.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/Button.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/CarouselView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/CarouselView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/Colors.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/Divider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/EmptyState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/EmptyState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/LoadingSpinner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/LoadingSpinner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/ScrollableContainer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/ScrollableContainer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/common/SmoothScrollable.h
)

# Feed
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/AggregatedFeedCard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/AggregatedFeedCard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/Comment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/Comment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/EmojiReactionsPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/EmojiReactionsPanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/PostCard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/PostCard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/PostsFeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/feed/PostsFeed.h
)

# Messages
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/messages/DirectMessages.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messages/DirectMessages.h
    ${CMAKE_CURRENT_SOURCE_DIR}/messages/Message.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messages/Message.h
    ${CMAKE_CURRENT_SOURCE_DIR}/messages/MessagesList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/messages/NewMessageDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messages/NewMessageDialog.h
)

# Notifications
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/notifications/NotificationCenter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notifications/NotificationCenter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/notifications/NotificationItem.h
)

# Playlists
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/playlists/PlaylistDetail.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/playlists/PlaylistDetail.h
    ${CMAKE_CURRENT_SOURCE_DIR}/playlists/Playlists.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/playlists/Playlists.h
)

# Profile
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/ActivityStatusSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/ActivityStatusSettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/ArchivedPosts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/ArchivedPosts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/EditProfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/EditProfile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/NotificationSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/NotificationSettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/PrivacySettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/PrivacySettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/Profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/Profile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/ProfileSetup.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/ProfileSetup.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/SavedPosts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/SavedPosts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/TwoFactorSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/profile/TwoFactorSettings.h
)

# Recording
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/recording/Upload.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/recording/Upload.h
    ${CMAKE_CURRENT_SOURCE_DIR}/recording/UploadProgress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/recording/UploadProgress.h
)

# Search
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/search/Search.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/search/Search.h
)

# Social
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/social/FollowersList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/social/FollowersList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/social/UserCard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/social/UserCard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/social/UserDiscovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/social/UserDiscovery.h
)

# Sounds
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sounds/SoundPage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sounds/SoundPage.h
)

# Stories
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/CircularVisualization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/CircularVisualization.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/CreateHighlightDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/CreateHighlightDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/NoteWaterfall.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/NoteWaterfall.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/PianoRoll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/PianoRoll.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/SelectHighlightDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/SelectHighlightDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoriesFeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoriesFeed.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoryHighlights.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoryHighlights.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoryRecording.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoryRecording.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoryViewer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stories/StoryViewer.h
)

# Synth
list(APPEND UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/synth/HiddenSynth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/synth/HiddenSynth.h
)

# Create the main UI library
add_library(SidechainUI STATIC ${UI_SOURCES})

# Include directories
target_include_directories(SidechainUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Dependencies
target_link_libraries(SidechainUI PUBLIC
    SidechainCore
    SidechainNetwork
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
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    target_include_directories(SidechainUI PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    target_link_libraries(SidechainUI PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
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
