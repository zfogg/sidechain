#==============================================================================
# SidechainAudio - Audio Library
# Audio capture, playback, and analysis
#==============================================================================

add_library(SidechainAudio STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioCapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioCapture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MIDICapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MIDICapture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpAudioPlayer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpAudioPlayer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BufferAudioPlayer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BufferAudioPlayer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/KeyDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/KeyDetector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NotificationSound.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NotificationSound.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ProgressiveKeyDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ProgressiveKeyDetector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ChordSequenceDetector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ChordSequenceDetector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SynthEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SynthEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Microphone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Microphone.h
)

# Include directories
target_include_directories(SidechainAudio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Add keyfinder include if available
if(SIDECHAIN_HAS_KEYFINDER)
    target_include_directories(SidechainAudio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/libkeyfinder/src
    )
endif()

# Dependencies
target_link_libraries(SidechainAudio PUBLIC
    SidechainCore
    SidechainNetwork
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_events
    juce::juce_dsp
)

# Apply JUCE object caching to use pre-compiled JUCE modules
if(COMMAND juce_link_cached)
    juce_link_cached(SidechainAudio)
endif()

# Link libkeyfinder if available
if(SIDECHAIN_HAS_KEYFINDER)
    target_link_libraries(SidechainAudio PRIVATE keyfinder)
    target_compile_definitions(SidechainAudio PUBLIC SIDECHAIN_HAS_KEYFINDER=1)
endif()

# GTK include directories (required by juce_gui_extra if linked transitively)
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    target_include_directories(SidechainAudio PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT_INCLUDE_DIRS})
    target_link_libraries(SidechainAudio PRIVATE ${GTK3_LIBRARIES} ${WEBKIT_LIBRARIES})
endif()

# Enable PIC for Release builds, disable for Debug (faster compilation)
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainAudio PROPERTIES
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
    set_target_properties(SidechainAudio PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()

