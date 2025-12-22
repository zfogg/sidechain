#==============================================================================
# SidechainUI - All UI Components Library
# Consolidated from 13 separate UI module libraries into one
#==============================================================================

# Collect all source files from UI subdirectories
set(UI_SOURCES
    # Animations
    animations/AnimationController.cpp
    animations/AnimationController.h
    animations/AnimationTimeline.h
    animations/Easing.h
    animations/NavigationStack.cpp
    animations/NavigationStack.h
    animations/TransitionAnimation.h
    animations/ViewTransitionManager.h
    
    # Auth
    auth/Auth.cpp
    auth/Auth.h
    
    # Challenges
    challenges/MidiChallengeDetail.cpp
    challenges/MidiChallengeDetail.h
    challenges/MidiChallenges.cpp
    challenges/MidiChallenges.h
    challenges/MidiChallengeSubmission.cpp
    challenges/MidiChallengeSubmission.h
    
    # Common
    common/AppStoreComponent.h
    common/ConnectionIndicator.h
    common/ErrorState.cpp
    common/ErrorState.h
    common/Header.cpp
    common/Header.h
    common/SkeletonLoader.cpp
    common/SkeletonLoader.h
    common/SmoothScrollable.h
    common/ToastNotification.cpp
    common/ToastNotification.h
    common/WaveformImageView.cpp
    common/WaveformImageView.h
    
    # Feed
    feed/AggregatedFeedCard.cpp
    feed/AggregatedFeedCard.h
    feed/Comment.cpp
    feed/Comment.h
    feed/EmojiReactionsPanel.cpp
    feed/EmojiReactionsPanel.h
    feed/PostCard.cpp
    feed/PostCard.h
    feed/PostsFeed.cpp
    feed/PostsFeed.h
    
    # Messages
    messages/AudioSnippetRecorder.cpp
    messages/AudioSnippetRecorder.h
    messages/MessagesList.cpp
    messages/MessagesList.h
    messages/MessageThread.cpp
    messages/MessageThread.h
    messages/ShareToMessageDialog.cpp
    messages/ShareToMessageDialog.h
    messages/UserPickerDialog.cpp
    messages/UserPickerDialog.h
    
    # Notifications
    notifications/NotificationBell.cpp
    notifications/NotificationBell.h
    notifications/NotificationItem.h
    notifications/NotificationList.cpp
    notifications/NotificationList.h
    
    # Playlists
    playlists/PlaylistDetail.cpp
    playlists/PlaylistDetail.h
    playlists/Playlists.cpp
    playlists/Playlists.h
    
    # Profile
    profile/ActivityStatusSettings.cpp
    profile/ActivityStatusSettings.h
    profile/ArchivedPosts.cpp
    profile/ArchivedPosts.h
    profile/EditProfile.cpp
    profile/EditProfile.h
    profile/NotificationSettings.cpp
    profile/NotificationSettings.h
    profile/PrivacySettings.cpp
    profile/PrivacySettings.h
    profile/Profile.cpp
    profile/Profile.h
    profile/ProfileSetup.cpp
    profile/ProfileSetup.h
    profile/SavedPosts.cpp
    profile/SavedPosts.h
    profile/TwoFactorSettings.cpp
    profile/TwoFactorSettings.h
    
    # Recording
    recording/DraftsView.cpp
    recording/DraftsView.h
    recording/Recording.cpp
    recording/Recording.h
    recording/Upload.cpp
    recording/Upload.h
    
    # Search
    search/Search.cpp
    search/Search.h
    
    # Social
    social/FollowersList.cpp
    social/FollowersList.h
    social/UserCard.cpp
    social/UserCard.h
    social/UserDiscovery.cpp
    social/UserDiscovery.h
    
    # Sounds
    sounds/SoundPage.cpp
    sounds/SoundPage.h
    
    # Stories
    stories/CircularVisualization.cpp
    stories/CircularVisualization.h
    stories/CreateHighlightDialog.cpp
    stories/CreateHighlightDialog.h
    stories/NoteWaterfall.cpp
    stories/NoteWaterfall.h
    stories/PianoRoll.cpp
    stories/PianoRoll.h
    stories/SelectHighlightDialog.cpp
    stories/SelectHighlightDialog.h
    stories/StoriesFeed.cpp
    stories/StoriesFeed.h
    stories/StoryHighlights.cpp
    stories/StoryHighlights.h
    stories/StoryRecording.cpp
    stories/StoryRecording.h
    stories/StoryViewer.cpp
    stories/StoryViewer.h
    
    # Synth
    synth/HiddenSynth.cpp
    synth/HiddenSynth.h
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

# Apply JUCE object caching to use pre-compiled JUCE modules
if(COMMAND juce_link_cached)
    juce_link_cached(SidechainUI)
endif()

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
