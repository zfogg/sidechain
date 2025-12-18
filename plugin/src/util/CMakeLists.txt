#==============================================================================
# SidechainUtil - Utility Library
# Base utilities with minimal dependencies
#==============================================================================

add_library(SidechainUtil STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Colors.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpErrorHandler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Json.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Validate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Validate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Async.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Result.h
    ${CMAKE_CURRENT_SOURCE_DIR}/UIHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UIHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StringFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StringFormatter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/TextEditorStyler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TextEditorStyler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HoverState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LongPressDetector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Emoji.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DAWProjectFolder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DAWProjectFolder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/OSNotification.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/OSNotification.h
    $<$<PLATFORM_ID:Darwin>:${CMAKE_CURRENT_SOURCE_DIR}/OSNotification_mac.mm>
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveformGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WaveformGenerator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/FileCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/FileCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/ImageCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/ImageCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/AudioCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/AudioCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/DraftCache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/DraftCache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/error/ErrorTracking.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/error/ErrorTracking.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PresenceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresenceManager.h
)

# Include directories - use source root so includes work as "util/Log.h"
target_include_directories(SidechainUtil PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link JuceHeader target to get generated header (generated during CMake config)
target_link_libraries(SidechainUtil PUBLIC SidechainJuceHeader)
# Header generation happens during CMake configuration, not build

# Link to SidechainNetwork for PresenceManager (which uses StreamChatClient)
target_link_libraries(SidechainUtil PUBLIC SidechainNetwork)

# Link JUCE core modules (minimal dependencies)
target_link_libraries(SidechainUtil PUBLIC
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_audio_formats  # For WaveformGenerator audio decoding
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Set visibility to hidden by default (faster linking, smaller binaries)
# Enable PIC for Release builds (required for linking into shared libraries/VST plugins)
# Disable for Debug builds for faster compilation
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainUtil PROPERTIES
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
    set_target_properties(SidechainUtil PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()

# Platform-specific dependencies for OSNotification
if(APPLE)
    # macOS: UserNotifications framework for native notifications
    target_link_libraries(SidechainUtil PUBLIC "-framework UserNotifications")
elseif(UNIX AND NOT APPLE)
    # Linux: Prefer libnotify over raw D-Bus for notifications
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        # Try libnotify first (simpler API, wraps D-Bus)
        pkg_check_modules(LIBNOTIFY QUIET libnotify)
        if(LIBNOTIFY_FOUND)
            target_include_directories(SidechainUtil PRIVATE ${LIBNOTIFY_INCLUDE_DIRS})
            target_link_libraries(SidechainUtil PRIVATE ${LIBNOTIFY_LIBRARIES})
            target_compile_definitions(SidechainUtil PRIVATE SIDECHAIN_HAS_LIBNOTIFY=1)
            message(STATUS "Found libnotify - using for desktop notifications")
        else()
            target_compile_definitions(SidechainUtil PRIVATE SIDECHAIN_HAS_LIBNOTIFY=0)
            # Fall back to raw D-Bus
            pkg_check_modules(DBUS QUIET dbus-1)
            if(DBUS_FOUND)
                target_include_directories(SidechainUtil PRIVATE ${DBUS_INCLUDE_DIRS})
                target_link_libraries(SidechainUtil PRIVATE ${DBUS_LIBRARIES})
                target_compile_definitions(SidechainUtil PRIVATE SIDECHAIN_HAS_DBUS=1)
                message(STATUS "Found D-Bus - using for desktop notifications")
            else()
                target_compile_definitions(SidechainUtil PRIVATE SIDECHAIN_HAS_DBUS=0)
                message(WARNING "Neither libnotify nor D-Bus found. OSNotification will not work on Linux.")
            endif()
        endif()
    else()
        target_compile_definitions(SidechainUtil PRIVATE SIDECHAIN_HAS_LIBNOTIFY=0)
        target_compile_definitions(SidechainUtil PRIVATE SIDECHAIN_HAS_DBUS=0)
    endif()
elseif(WIN32)
    # Windows: Windows Runtime (WinRT) for toast notifications
    # WinRT headers should be available by default on Windows 10+
    # No additional linking needed
endif()

