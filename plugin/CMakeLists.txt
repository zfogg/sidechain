# Sidechain VST Plugin - CMake Build Configuration
# This replaces the Projucer-based build system

cmake_minimum_required(VERSION 3.22)

# Prefer Ninja generator for faster builds (auto-parallelizes)
if(NOT CMAKE_GENERATOR)
    find_program(NINJA_EXECUTABLE ninja)
    if(NINJA_EXECUTABLE)
        set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)
    endif()
endif()

#==============================================================================
# Version from git tags (via scripts/version.sh)
# NOTE: Must run BEFORE project() so we can set PROJECT_VERSION
#==============================================================================
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh" --major
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION_MAJOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh" --minor
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION_MINOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/version.sh" --patch
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    OUTPUT_VARIABLE SIDECHAIN_VERSION_PATCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback to 0.0.0 if script fails
if(NOT SIDECHAIN_VERSION OR SIDECHAIN_VERSION STREQUAL "")
    set(SIDECHAIN_VERSION "0.0.0")
    set(SIDECHAIN_VERSION_MAJOR "0")
    set(SIDECHAIN_VERSION_MINOR "0")
    set(SIDECHAIN_VERSION_PATCH "0")
endif()

message(STATUS "Sidechain version: ${SIDECHAIN_VERSION}")

# Use the git-derived version for the CMake project
project(Sidechain VERSION ${SIDECHAIN_VERSION} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate Version.h from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# Default to Debug if no build type specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Export compile_commands.json for IDE support (clangd, etc.)
# This can be overridden by setting CMAKE_EXPORT_COMPILE_COMMANDS=OFF
if(NOT DEFINED CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for IDE support" FORCE)
endif()

#==============================================================================
# Compiler flags for all platforms
#==============================================================================

# Common warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # GCC/Clang flags
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter      # JUCE has many unused params
        -Wno-missing-field-initializers
    )

    # Debug-specific flags
    add_compile_options(
        "$<$<CONFIG:Debug>:-O0>"           # No optimization
        "$<$<CONFIG:Debug>:-g3>"           # Maximum debug info
        "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"  # Better stack traces
        "$<$<CONFIG:Debug>:-DDEBUG=1>"
        "$<$<CONFIG:Debug>:-D_DEBUG=1>"
    )

    # Release-specific flags
    add_compile_options(
        "$<$<CONFIG:Release>:-O3>"         # Maximum optimization
        "$<$<CONFIG:Release>:-DNDEBUG=1>"
        "$<$<CONFIG:Release>:-ffast-math>" # Fast floating point (safe for audio)
    )

    # RelWithDebInfo flags
    add_compile_options(
        "$<$<CONFIG:RelWithDebInfo>:-O2>"
        "$<$<CONFIG:RelWithDebInfo>:-g>"
        "$<$<CONFIG:RelWithDebInfo>:-DNDEBUG=1>"
    )

    # Platform-specific flags
    if(APPLE)
        # macOS-specific
        add_compile_options(-fvisibility=hidden)
        add_link_options(-dead_strip)
    elseif(UNIX AND NOT APPLE)
        # Linux-specific
        add_compile_options(-fvisibility=hidden)
        add_compile_options("$<$<CONFIG:Release>:-flto>")
        add_link_options("$<$<CONFIG:Release>:-flto>")
        add_link_options("$<$<CONFIG:Release>:-s>")  # Strip symbols
    endif()

elseif(MSVC)
    # MSVC flags
    add_compile_options(
        /W4                    # Warning level 4
        /wd4100                # Unreferenced formal parameter
        /wd4244                # Conversion warnings (JUCE generates many)
        /wd4267                # Size_t conversions
        /wd4458                # Declaration hides class member
        /MP                    # Multi-processor compilation
        /utf-8                 # UTF-8 source files
    )

    # Debug-specific flags
    add_compile_options(
        "$<$<CONFIG:Debug>:/Od>"           # No optimization
        "$<$<CONFIG:Debug>:/Zi>"           # Debug information
        "$<$<CONFIG:Debug>:/RTC1>"         # Runtime checks
        "$<$<CONFIG:Debug>:/MDd>"          # Debug runtime library
    )

    # Release-specific flags
    add_compile_options(
        "$<$<CONFIG:Release>:/O2>"         # Maximum optimization
        "$<$<CONFIG:Release>:/Oi>"         # Enable intrinsics
        "$<$<CONFIG:Release>:/GL>"         # Whole program optimization
        "$<$<CONFIG:Release>:/Gy>"         # Function-level linking
        "$<$<CONFIG:Release>:/MD>"         # Release runtime library
        "$<$<CONFIG:Release>:/fp:fast>"    # Fast floating point
        "$<$<CONFIG:Release>:/DNDEBUG>"    # Define NDEBUG for Release builds
    )

    # Release linker flags
    add_link_options(
        "$<$<CONFIG:Release>:/LTCG>"       # Link-time code generation
        "$<$<CONFIG:Release>:/OPT:REF>"    # Remove unreferenced code
        "$<$<CONFIG:Release>:/OPT:ICF>"    # Identical COMDAT folding
    )

    # RelWithDebInfo flags
    add_compile_options(
        "$<$<CONFIG:RelWithDebInfo>:/O2>"
        "$<$<CONFIG:RelWithDebInfo>:/Zi>"
        "$<$<CONFIG:RelWithDebInfo>:/MD>"
        "$<$<CONFIG:RelWithDebInfo>:/DNDEBUG>"  # Define NDEBUG for RelWithDebInfo builds
    )
endif()

# Option to build AudioPluginHost for testing
option(SIDECHAIN_BUILD_PLUGIN_HOST "Build JUCE AudioPluginHost for testing" ON)

# Option to build tests
option(SIDECHAIN_BUILD_TESTS "Build unit tests with Catch2" OFF)

# Option to enable coverage (requires SIDECHAIN_BUILD_TESTS)
option(SIDECHAIN_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

# Option to build Standalone app (default OFF - only build VST3/AU by default)
option(SIDECHAIN_BUILD_STANDALONE "Build Standalone app (in addition to VST3/AU)" ON)

# Option to install standalone app alongside VST3
option(SIDECHAIN_INSTALL_STANDALONE "Install Standalone app with cmake --install" OFF)

# Option to build documentation (Doxygen + Sphinx)
option(SIDECHAIN_BUILD_DOCS "Build documentation with Doxygen and Sphinx" OFF)

# JUCE is added as a subdirectory from the deps folder
# EXCLUDE_FROM_ALL prevents JUCE from being installed with cmake --install
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    # Fetch JUCE if not present
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/zfogg/JUCE.git
        GIT_TAG fix/audio-plugin-instance-constructor
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Build AudioPluginHost manually (without JUCE_BUILD_EXTRAS which builds everything)
# EXCLUDE_FROM_ALL prevents it from being installed
if(SIDECHAIN_BUILD_PLUGIN_HOST AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/extras/AudioPluginHost/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/extras/AudioPluginHost" "${CMAKE_BINARY_DIR}/AudioPluginHost" EXCLUDE_FROM_ALL)
endif()

#==============================================================================
# libkeyfinder - Musical Key Detection Library
#==============================================================================
# Option to enable key detection (requires FFTW3)
option(SIDECHAIN_ENABLE_KEY_DETECTION "Enable musical key detection using libkeyfinder" ON)

if(SIDECHAIN_ENABLE_KEY_DETECTION)
    # Find FFTW3 (required by libkeyfinder)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../deps/libkeyfinder/cmake")
    find_package(FFTW3 QUIET)

    if(FFTW3_FOUND)
        message(STATUS "FFTW3 found - Key detection enabled")

        # Build libkeyfinder as a static library
        set(LIBKEYFINDER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/libkeyfinder")
        if(EXISTS "${LIBKEYFINDER_DIR}/CMakeLists.txt")
            # Configure libkeyfinder build options
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
            set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

            # Add libkeyfinder subdirectory
            add_subdirectory("${LIBKEYFINDER_DIR}" "${CMAKE_BINARY_DIR}/libkeyfinder" EXCLUDE_FROM_ALL)

            # Create an alias for cleaner usage
            if(TARGET keyfinder)
                set(SIDECHAIN_HAS_KEYFINDER TRUE)
                # Enable position independent code for shared library linking
                set_target_properties(keyfinder PROPERTIES POSITION_INDEPENDENT_CODE ON)
                message(STATUS "libkeyfinder will be built from: ${LIBKEYFINDER_DIR}")
            endif()
        else()
            message(WARNING "libkeyfinder not found at ${LIBKEYFINDER_DIR}")
            message(STATUS "Run: git submodule update --init --recursive")
            set(SIDECHAIN_HAS_KEYFINDER FALSE)
        endif()
    else()
        message(WARNING "FFTW3 not found - Key detection disabled")
        message(STATUS "Install FFTW3:")
        message(STATUS "  Arch:   sudo pacman -S fftw")
        message(STATUS "  Ubuntu: sudo apt install libfftw3-dev")
        message(STATUS "  macOS:  brew install fftw")
        set(SIDECHAIN_HAS_KEYFINDER FALSE)
    endif()
else()
    message(STATUS "Key detection disabled")
    set(SIDECHAIN_HAS_KEYFINDER FALSE)
endif()

#==============================================================================
# ASIO (Standalone) - Asynchronous I/O Library
#==============================================================================
# ASIO is a cross-platform C++ library for network and low-level I/O
# programming that provides developers with a consistent asynchronous model.
# We use the standalone version (no Boost dependency).
#
# Documentation: https://think-async.com/Asio/
# GitHub: https://github.com/chriskohlhoff/asio
#==============================================================================
set(ASIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/asio")
# ASIO 1.14.1 has headers in asio/include/ directory
if(EXISTS "${ASIO_DIR}/asio/include/asio.hpp" OR EXISTS "${ASIO_DIR}/include/asio.hpp")
    message(STATUS "ASIO found at: ${ASIO_DIR}")

    # Create an interface library for ASIO (header-only)
    add_library(asio INTERFACE)
    # Try nested path first (ASIO 1.14.1), then flat path (newer versions)
    if(EXISTS "${ASIO_DIR}/asio/include/asio.hpp")
        target_include_directories(asio INTERFACE "${ASIO_DIR}/asio/include")
    endif()

    # Define ASIO_STANDALONE so ASIO doesn't require Boost
    # Define ASIO_HAS_STD_INVOKE_RESULT for C++17+ (std::result_of removed in C++20)
    target_compile_definitions(asio INTERFACE
        ASIO_STANDALONE=1
        ASIO_HAS_STD_INVOKE_RESULT=1
    )

    # ASIO requires C++11 or later (we're using C++23)
    target_compile_features(asio INTERFACE cxx_std_11)

    # Platform-specific requirements for ASIO
    if(WIN32)
        # Windows: ASIO needs Windows sockets
        target_link_libraries(asio INTERFACE ws2_32 wsock32)
    elseif(APPLE)
        # macOS: ASIO uses system libraries (no additional linking needed for basic functionality)
    elseif(UNIX)
        # Linux/Unix: ASIO uses pthread
        target_link_libraries(asio INTERFACE pthread)
    endif()

    set(SIDECHAIN_HAS_ASIO TRUE)
    message(STATUS "ASIO will be available as an interface library (standalone mode)")
else()
    message(WARNING "ASIO not found at ${ASIO_DIR}")
    message(STATUS "Run: git submodule update --init --recursive")
    set(SIDECHAIN_HAS_ASIO FALSE)
endif()

#==============================================================================
# websocketpp - WebSocket Protocol Library (Header-Only)
#==============================================================================
# WebSocket++ is a header-only C++ library that implements RFC6455 WebSocket
# Protocol. It provides WebSocket client and server functionality with
# interchangeable transport backends.
#
# Usage in source files:
#   #include <websocketpp/config/asio_no_tls_client.hpp>
#   #include <websocketpp/client.hpp>
#
# Transport backends available:
#   - asio_no_tls_client.hpp: ASIO without TLS (requires Boost or standalone ASIO)
#   - asio_client.hpp: ASIO with TLS support
#   - minimal_client.hpp: Minimal client (no networking, for testing)
#
# Dependencies:
#   - For ASIO backend: Requires either Boost.Asio or standalone ASIO library
#   - For TLS support: Requires OpenSSL libraries
#
# Documentation: https://docs.websocketpp.org/
# GitHub: https://github.com/zaphoyd/websocketpp
#==============================================================================
set(WEBSOCKETPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/websocketpp")
if(EXISTS "${WEBSOCKETPP_DIR}/websocketpp")
    message(STATUS "websocketpp found at: ${WEBSOCKETPP_DIR}")

    # Create an interface library for websocketpp (header-only)
    add_library(websocketpp INTERFACE)
    target_include_directories(websocketpp INTERFACE "${WEBSOCKETPP_DIR}")

    # Link ASIO if available (websocketpp needs it for ASIO backend)
    if(SIDECHAIN_HAS_ASIO)
        target_link_libraries(websocketpp INTERFACE asio)
        message(STATUS "websocketpp will use standalone ASIO backend")
    else()
        message(WARNING "websocketpp found but ASIO not available - ASIO backend will not work")
    endif()

    # Find OpenSSL for TLS support (required for wss:// connections)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(websocketpp INTERFACE OpenSSL::SSL OpenSSL::Crypto)
        message(STATUS "OpenSSL found - TLS support enabled for websocketpp")
        set(SIDECHAIN_HAS_OPENSSL TRUE)
    else()
        message(WARNING "OpenSSL not found - TLS WebSocket connections (wss://) will not work")
        message(STATUS "Install OpenSSL:")
        message(STATUS "  macOS:   brew install openssl")
        message(STATUS "  Ubuntu:  sudo apt install libssl-dev")
        message(STATUS "  Arch:    sudo pacman -S openssl")
        set(SIDECHAIN_HAS_OPENSSL FALSE)
    endif()

    set(SIDECHAIN_HAS_WEBSOCKETPP TRUE)
    message(STATUS "websocketpp will be available as an interface library")
else()
    message(WARNING "websocketpp not found at ${WEBSOCKETPP_DIR}")
    message(STATUS "Run: git submodule update --init --recursive")
    set(SIDECHAIN_HAS_WEBSOCKETPP FALSE)
endif()

# Build formats - conditionally include Standalone
# VST3 is always built, AU only on macOS, Standalone only if option is enabled
set(PLUGIN_FORMATS "VST3")
if(APPLE)
    list(APPEND PLUGIN_FORMATS "AU")
endif()
if(SIDECHAIN_BUILD_STANDALONE)
    list(APPEND PLUGIN_FORMATS "Standalone")
    message(STATUS "Standalone app will be built")
else()
    message(STATUS "Standalone app disabled (enable with -DSIDECHAIN_BUILD_STANDALONE=ON)")
endif()

#==============================================================================
# Source File Modules
#==============================================================================

# Plugin entry points
set(SIDECHAIN_CORE_SOURCES
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/PluginEditor.cpp
    source/PluginEditor.h
)

# State management
set(SIDECHAIN_STORES_SOURCES
    source/stores/UserDataStore.cpp
    source/stores/UserDataStore.h
    source/stores/FeedDataManager.cpp
    source/stores/FeedDataManager.h
    source/stores/ImageCache.cpp
    source/stores/ImageCache.h
)

# Data structures and DTOs
set(SIDECHAIN_MODELS_SOURCES
    source/models/FeedPost.cpp
    source/models/FeedPost.h
    source/models/FeedResponse.h
    source/models/AggregatedFeedGroup.h
    source/models/AggregatedFeedResponse.h
    # Story.cpp/Story.h - TODO: Enable when story APIs are implemented
)

# Audio capture, playback, and analysis
set(SIDECHAIN_AUDIO_SOURCES
    source/audio/AudioCapture.cpp
    source/audio/AudioCapture.h
    source/audio/MIDICapture.cpp
    source/audio/MIDICapture.h
    source/audio/HttpAudioPlayer.cpp
    source/audio/HttpAudioPlayer.h
    source/audio/BufferAudioPlayer.cpp
    source/audio/BufferAudioPlayer.h
    source/audio/KeyDetector.cpp
    source/audio/KeyDetector.h
    source/audio/ProgressiveKeyDetector.cpp
    source/audio/ProgressiveKeyDetector.h
)

# API clients and data managers
set(SIDECHAIN_NETWORK_SOURCES
    source/network/NetworkClient.cpp
    source/network/NetworkClient.h
    source/network/WebSocketClient.cpp
    source/network/WebSocketClient.h
    source/network/StreamChatClient.cpp
    source/network/StreamChatClient.h
)

# UI - Auth
set(SIDECHAIN_UI_AUTH_SOURCES
    source/ui/auth/Auth.cpp
    source/ui/auth/Auth.h
)

# UI - Messages
set(SIDECHAIN_UI_MESSAGES_SOURCES
    source/ui/messages/MessagesList.cpp
    source/ui/messages/MessagesList.h
    source/ui/messages/MessageThread.cpp
    source/ui/messages/MessageThread.h
    source/ui/messages/AudioSnippetRecorder.cpp
    source/ui/messages/AudioSnippetRecorder.h
    source/ui/messages/UserPickerDialog.cpp
    source/ui/messages/UserPickerDialog.h
)

# UI - Profile
set(SIDECHAIN_UI_PROFILE_SOURCES
    source/ui/profile/ProfileSetup.cpp
    source/ui/profile/ProfileSetup.h
    source/ui/profile/Profile.cpp
    source/ui/profile/Profile.h
    source/ui/profile/EditProfile.cpp
    source/ui/profile/EditProfile.h
)

# UI - Feed
set(SIDECHAIN_UI_FEED_SOURCES
    source/ui/feed/PostsFeed.cpp
    source/ui/feed/PostsFeed.h
    source/ui/feed/PostCard.cpp
    source/ui/feed/PostCard.h
    source/ui/feed/Comment.cpp
    source/ui/feed/Comment.h
    source/ui/feed/EmojiReactionsPanel.cpp
    source/ui/feed/EmojiReactionsPanel.h
)

# UI - Recording
set(SIDECHAIN_UI_RECORDING_SOURCES
    source/ui/recording/Recording.cpp
    source/ui/recording/Recording.h
    source/ui/recording/Upload.cpp
    source/ui/recording/Upload.h
)

# UI - Notifications
set(SIDECHAIN_UI_NOTIFICATIONS_SOURCES
    source/ui/notifications/NotificationBell.cpp
    source/ui/notifications/NotificationBell.h
    source/ui/notifications/NotificationList.cpp
    source/ui/notifications/NotificationList.h
)

# UI - Social
set(SIDECHAIN_UI_SOCIAL_SOURCES
    source/ui/social/UserCard.cpp
    source/ui/social/UserCard.h
    source/ui/social/UserDiscovery.cpp
    source/ui/social/UserDiscovery.h
    source/ui/search/Search.cpp
    source/ui/search/Search.h
    source/ui/social/FollowersList.cpp
    source/ui/social/FollowersList.h
)

# UI - Stories (Phase 7.5)
set(SIDECHAIN_UI_STORIES_SOURCES
    source/ui/stories/StoryRecording.cpp
    source/ui/stories/StoryRecording.h
    source/ui/stories/StoriesFeed.cpp
    source/ui/stories/StoriesFeed.h
    source/ui/stories/StoryViewer.cpp
    source/ui/stories/StoryViewer.h
    source/ui/stories/PianoRoll.cpp
    source/ui/stories/PianoRoll.h
    source/ui/stories/NoteWaterfall.cpp
    source/ui/stories/NoteWaterfall.h
    source/ui/stories/CircularVisualization.cpp
    source/ui/stories/CircularVisualization.h
)

# UI - Common
set(SIDECHAIN_UI_COMMON_SOURCES
    source/ui/common/Header.cpp
    source/ui/common/Header.h
    source/ui/common/ConnectionIndicator.h
)

# Utilities
set(SIDECHAIN_UTIL_SOURCES
    source/util/Time.cpp
    source/util/Time.h
    source/util/Colors.h
    source/util/HttpErrorHandler.h
    source/util/Log.cpp
    source/util/Log.h
    source/util/Image.cpp
    source/util/Image.h
    source/util/Json.cpp
    source/util/Json.h
    source/util/Validate.cpp
    source/util/Validate.h
    source/util/Async.cpp
    source/util/Async.h
    source/util/Result.h
    source/util/UIHelpers.cpp
    source/util/UIHelpers.h
    source/util/Constants.h
    source/util/StringFormatter.cpp
    source/util/StringFormatter.h
    source/util/TextEditorStyler.cpp
    source/util/TextEditorStyler.h
    source/util/Animation.cpp
    source/util/Animation.h
    source/util/HoverState.h
    source/util/LongPressDetector.h
)

# Combined UI sources
set(SIDECHAIN_UI_SOURCES
    ${SIDECHAIN_UI_AUTH_SOURCES}
    ${SIDECHAIN_UI_MESSAGES_SOURCES}
    ${SIDECHAIN_UI_PROFILE_SOURCES}
    ${SIDECHAIN_UI_FEED_SOURCES}
    ${SIDECHAIN_UI_RECORDING_SOURCES}
    ${SIDECHAIN_UI_NOTIFICATIONS_SOURCES}
    ${SIDECHAIN_UI_SOCIAL_SOURCES}
    ${SIDECHAIN_UI_STORIES_SOURCES}
    ${SIDECHAIN_UI_COMMON_SOURCES}
)

# All plugin sources
set(SIDECHAIN_ALL_SOURCES
    ${SIDECHAIN_CORE_SOURCES}
    ${SIDECHAIN_STORES_SOURCES}
    ${SIDECHAIN_MODELS_SOURCES}
    ${SIDECHAIN_AUDIO_SOURCES}
    ${SIDECHAIN_NETWORK_SOURCES}
    ${SIDECHAIN_UI_SOURCES}
    ${SIDECHAIN_UTIL_SOURCES}
)

#==============================================================================
# Plugin Target
#==============================================================================

# Plugin target
juce_add_plugin(Sidechain
    # Plugin metadata (version from git tags)
    VERSION ${SIDECHAIN_VERSION}
    COMPANY_NAME "Joopal"
    COMPANY_WEBSITE "https://sidechain.live"
    COMPANY_EMAIL "hi@sidechain.live"
    COMPANY_COPYRIGHT "Copyright (c) 2025 Joopal"
    BUNDLE_ID "gg.zfo.Sidechain"

    # Plugin identifiers
    PLUGIN_MANUFACTURER_CODE Joop
    PLUGIN_CODE Schn
    PLUGIN_NAME "Sidechain"
    DESCRIPTION "Social VST for Music Producers"

    # Plugin type configuration
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Plugin format categories
    VST3_CATEGORIES "Fx"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    AU_EXPORT_PREFIX "SidechainAU"
    AAX_IDENTIFIER "gg.zfo.Sidechain"

    # Build formats - use the conditionally constructed list
    FORMATS ${PLUGIN_FORMATS}

    # Output name
    PRODUCT_NAME "Sidechain"

    # macOS specific
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Sidechain needs microphone access to capture audio"

    # Don't copy after build in CI - we handle installation separately
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Generate JuceHeader.h for Projucer compatibility
# This allows existing source files that use #include <JuceHeader.h> to work
juce_generate_juce_header(Sidechain)

# Add source files to plugin target
target_sources(Sidechain PRIVATE ${SIDECHAIN_ALL_SOURCES})

# Preprocessor definitions
target_compile_definitions(Sidechain
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_PreferredChannelConfigurations=1
        JucePlugin_ChannelConfigurations="{2, 2}"
)

# Add generated headers to include path (for Version.h)
target_include_directories(Sidechain PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Add keyfinder compile definition if available
if(SIDECHAIN_HAS_KEYFINDER)
    target_compile_definitions(Sidechain PUBLIC SIDECHAIN_HAS_KEYFINDER=1)
    target_include_directories(Sidechain PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../deps/libkeyfinder/src")
endif()

# Link JUCE modules
target_link_libraries(Sidechain
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Don't treat warnings as errors (the codebase has some warnings to fix later)
if(MSVC)
    target_compile_options(Sidechain PRIVATE /W3)
else()
    target_compile_options(Sidechain PRIVATE -Wall -Wno-unused-parameter -Wno-deprecated-declarations)
endif()

# Platform-specific link libraries
if(APPLE)
    target_link_libraries(Sidechain PRIVATE "-framework Security")
endif()

# Link libkeyfinder if available
if(SIDECHAIN_HAS_KEYFINDER)
    target_link_libraries(Sidechain PRIVATE keyfinder)
    message(STATUS "Sidechain will link with libkeyfinder for key detection")
endif()

# Link websocketpp if available (header-only interface library)
if(SIDECHAIN_HAS_WEBSOCKETPP)
    target_link_libraries(Sidechain PRIVATE websocketpp)
    message(STATUS "Sidechain will link with websocketpp for WebSocket support")
endif()

# Installation paths for plugins (platform-specific defaults)
# These are the SYSTEM-WIDE VST3/AU installation directories (for sudo cmake --install)
# For user-local installs, override with: cmake -DVST3_INSTALL_DIR=~/.vst3 ...
if(APPLE)
    # System-wide: /Library/Audio/Plug-Ins/VST3
    # User-local: ~/Library/Audio/Plug-Ins/VST3
    set(VST3_INSTALL_DIR "/Library/Audio/Plug-Ins/VST3" CACHE PATH "VST3 install directory")
    set(AU_INSTALL_DIR "/Library/Audio/Plug-Ins/Components" CACHE PATH "AU install directory")
elseif(WIN32)
    # System-wide: C:\Program Files\Common Files\VST3
    # Use COMMONPROGRAMFILES env var which resolves to the correct path
    set(VST3_INSTALL_DIR "$ENV{COMMONPROGRAMFILES}/VST3" CACHE PATH "VST3 install directory")
else()
    # Linux system-wide: /usr/lib/vst3 or /usr/local/lib/vst3
    # User-local: ~/.vst3
    set(VST3_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/vst3" CACHE PATH "VST3 install directory")
endif()

# Print install path information
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  Plugin Installation Paths")
message(STATUS "========================================")
message(STATUS "  VST3: ${VST3_INSTALL_DIR}")
if(APPLE)
    message(STATUS "  AU:   ${AU_INSTALL_DIR}")
endif()
message(STATUS "")
message(STATUS "  To install: sudo cmake --install build/<preset>")
message(STATUS "  To override: cmake -DVST3_INSTALL_DIR=/path/to/vst3 ...")
message(STATUS "========================================")

# Install VST3 plugin using cmake --install
# Use the JUCE artefacts output directory structure
set(ARTEFACTS_DIR "${CMAKE_BINARY_DIR}/Sidechain_artefacts")

install(
    DIRECTORY "${ARTEFACTS_DIR}/$<CONFIG>/VST3/Sidechain.vst3"
    DESTINATION "${VST3_INSTALL_DIR}"
    COMPONENT VST3
)

# Install Standalone app (optional - only if it was built)
if(SIDECHAIN_INSTALL_STANDALONE AND SIDECHAIN_BUILD_STANDALONE)
    if(APPLE)
        install(
            DIRECTORY "${ARTEFACTS_DIR}/$<CONFIG>/Standalone/Sidechain.app"
            DESTINATION "/Applications"
            COMPONENT Standalone
        )
    else()
        install(
            PROGRAMS "${ARTEFACTS_DIR}/$<CONFIG>/Standalone/Sidechain"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
            COMPONENT Standalone
        )
    endif()
endif()

# Install AU plugin (macOS only)
if(APPLE)
    install(
        DIRECTORY "${ARTEFACTS_DIR}/$<CONFIG>/AU/Sidechain.component"
        DESTINATION "${AU_INSTALL_DIR}"
        COMPONENT AU
    )
endif()

# Copy AudioPluginHost to bin/ after build
if(SIDECHAIN_BUILD_PLUGIN_HOST AND TARGET AudioPluginHost)
    # Make sure AudioPluginHost builds when we build our plugin
    add_dependencies(Sidechain_All AudioPluginHost)

    # Copy after Sidechain_All builds (which depends on AudioPluginHost)
    add_custom_command(
        TARGET Sidechain_All POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_BINARY_DIR}/AudioPluginHost/AudioPluginHost_artefacts/$<CONFIG>/AudioPluginHost"
            "${CMAKE_BINARY_DIR}/bin/AudioPluginHost"
        COMMENT "Copying AudioPluginHost to ${CMAKE_BINARY_DIR}/bin/"
    )

    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "  Sidechain Build Configuration")
    message(STATUS "========================================")
    message(STATUS "  VST3 output:     ${ARTEFACTS_DIR}/\${CONFIG}/VST3/Sidechain.vst3")
    message(STATUS "  VST3 install:    ${VST3_INSTALL_DIR}/Sidechain.vst3")
    message(STATUS "  AudioPluginHost: ${CMAKE_BINARY_DIR}/bin/AudioPluginHost")
    message(STATUS "========================================")
    message(STATUS "")
endif()

#==============================================================================
# Unit Tests with Catch2
#==============================================================================
if(SIDECHAIN_BUILD_TESTS)
    # Enable CTest
    enable_testing()

    # Fetch Catch2
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    # Include Catch2's CMake helpers
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)

    # Audio sources for tests (excludes KeyDetector which requires libkeyfinder)
    set(SIDECHAIN_TEST_AUDIO_SOURCES
        source/audio/AudioCapture.cpp
        source/audio/AudioCapture.h
        source/audio/MIDICapture.cpp
        source/audio/MIDICapture.h
        source/audio/HttpAudioPlayer.cpp
        source/audio/HttpAudioPlayer.h
        source/audio/BufferAudioPlayer.cpp
        source/audio/BufferAudioPlayer.h
    )

    # Utility sources for tests (subset needed by test lib)
    set(SIDECHAIN_TEST_UTIL_SOURCES
        source/util/Time.cpp
        source/util/Time.h
        source/util/Json.cpp
        source/util/Json.h
        source/util/Async.cpp
        source/util/Async.h
        source/util/Validate.cpp
        source/util/Validate.h
        source/util/StringFormatter.cpp
        source/util/StringFormatter.h
        source/util/Animation.cpp
        source/util/Animation.h
        source/util/Log.cpp
        source/util/Log.h
    )

    # Create a static library from plugin source for testing (without plugin wrapper)
    add_library(SidechainTestLib STATIC
        ${SIDECHAIN_TEST_AUDIO_SOURCES}
        ${SIDECHAIN_NETWORK_SOURCES}
        ${SIDECHAIN_MODELS_SOURCES}
        ${SIDECHAIN_STORES_SOURCES}
        ${SIDECHAIN_TEST_UTIL_SOURCES}
    )

    # SidechainTestLib needs access to Sidechain's generated JuceHeader.h
    # We need to build Sidechain first to generate the header
    add_dependencies(SidechainTestLib Sidechain)

    target_include_directories(SidechainTestLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/source
        # Use the JuceHeader generated for the main plugin
        ${CMAKE_CURRENT_BINARY_DIR}/Sidechain_artefacts/JuceLibraryCode
    )

    target_compile_definitions(SidechainTestLib PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STANDALONE_APPLICATION=1
    )

    target_link_libraries(SidechainTestLib PUBLIC
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_events
        juce::juce_graphics
        juce::juce_data_structures
        juce::juce_gui_basics
    )

    # Link websocketpp and asio if available (needed for WebSocketClient tests)
    if(SIDECHAIN_HAS_WEBSOCKETPP)
        target_link_libraries(SidechainTestLib PRIVATE websocketpp)
    endif()
    if(SIDECHAIN_HAS_ASIO)
        target_link_libraries(SidechainTestLib PRIVATE asio)
    endif()

    # Coverage flags
    if(SIDECHAIN_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(SidechainTestLib PRIVATE --coverage -fprofile-arcs -ftest-coverage)
            target_link_options(SidechainTestLib PRIVATE --coverage)
        endif()
    endif()

    # Test source files
    set(SIDECHAIN_TEST_SOURCES
        tests/AudioCaptureTest.cpp
        tests/NetworkClientTest.cpp
        tests/FeedDataTest.cpp
    )

    # Test executable
    add_executable(SidechainTests ${SIDECHAIN_TEST_SOURCES})

    target_link_libraries(SidechainTests PRIVATE
        SidechainTestLib
        Catch2::Catch2WithMain
    )

    # Coverage flags for test executable
    if(SIDECHAIN_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(SidechainTests PRIVATE --coverage -fprofile-arcs -ftest-coverage)
            target_link_options(SidechainTests PRIVATE --coverage)
        endif()
    endif()

    # Create test-results directory for JUnit XML output
    # Use both file(MAKE_DIRECTORY) at configure time AND a POST_BUILD command
    # to ensure directory exists when tests run (fixes CI race condition)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test-results)
    add_custom_command(TARGET SidechainTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test-results
        COMMENT "Ensuring test-results directory exists"
    )

    # Register tests with CTest
    catch_discover_tests(SidechainTests
        REPORTER junit
        OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
        OUTPUT_PREFIX "test-"
        OUTPUT_SUFFIX ".xml"
    )

    # Custom target for running tests with coverage
    if(SIDECHAIN_ENABLE_COVERAGE)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND SidechainTests
            COMMAND lcov --capture --directory ${CMAKE_BINARY_DIR} --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info --ignore-errors mismatch
            COMMAND lcov --remove ${CMAKE_BINARY_DIR}/coverage/coverage.info '/usr/*' '*/deps/*' '*/Catch2/*' '*/build/*' --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info --ignore-errors unused
            COMMAND genhtml ${CMAKE_BINARY_DIR}/coverage/coverage.info --output-directory ${CMAKE_BINARY_DIR}/coverage/html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating coverage report"
            DEPENDS SidechainTests
        )
    endif()

    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "  Test Configuration")
    message(STATUS "========================================")
    message(STATUS "  Tests enabled:   ON")
    message(STATUS "  Coverage:        ${SIDECHAIN_ENABLE_COVERAGE}")
    message(STATUS "  Test binary:     ${CMAKE_BINARY_DIR}/SidechainTests")
    message(STATUS "  JUnit output:    ${CMAKE_BINARY_DIR}/test-results/")
    message(STATUS "========================================")
    message(STATUS "")
endif()

#==============================================================================
# Documentation (Doxygen + Sphinx)
#==============================================================================
if(SIDECHAIN_BUILD_DOCS)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../docs" "${CMAKE_BINARY_DIR}/docs" EXCLUDE_FROM_ALL)
    message(STATUS "Documentation build enabled")
else()
    message(STATUS "Documentation build disabled (enable with -DSIDECHAIN_BUILD_DOCS=ON)")
endif()
