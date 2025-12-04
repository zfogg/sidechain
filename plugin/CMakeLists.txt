# Sidechain VST Plugin - CMake Build Configuration
# This replaces the Projucer-based build system

cmake_minimum_required(VERSION 3.22)

project(Sidechain VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to build AudioPluginHost for testing
option(SIDECHAIN_BUILD_PLUGIN_HOST "Build JUCE AudioPluginHost for testing" ON)

# JUCE is added as a subdirectory from the deps folder
# It can be fetched via FetchContent or added as a git submodule
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE" "${CMAKE_BINARY_DIR}/JUCE")
else()
    # Fetch JUCE if not present
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Build AudioPluginHost manually (without JUCE_BUILD_EXTRAS which builds everything)
if(SIDECHAIN_BUILD_PLUGIN_HOST AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/extras/AudioPluginHost/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/extras/AudioPluginHost" "${CMAKE_BINARY_DIR}/AudioPluginHost")
endif()

# Plugin target
juce_add_plugin(Sidechain
    # Plugin metadata
    VERSION 0.1.0
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

    # Build formats - AU only available on macOS
    FORMATS VST3 Standalone AU

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

# Source files (matching Sidechain.jucer)
target_sources(Sidechain
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/AudioCapture.cpp
        Source/AudioCapture.h
        Source/NetworkClient.cpp
        Source/NetworkClient.h
        Source/ConnectionIndicator.h
        Source/ProfileSetupComponent.cpp
        Source/ProfileSetupComponent.h
        Source/PostsFeedComponent.cpp
        Source/PostsFeedComponent.h
)

# Preprocessor definitions
target_compile_definitions(Sidechain
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

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

# Installation paths for plugins (platform-specific defaults)
# These are the standard VST3/AU installation directories
if(APPLE)
    set(VST3_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3" CACHE PATH "VST3 install directory")
    set(AU_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components" CACHE PATH "AU install directory")
elseif(WIN32)
    # Use forward slashes for CMake compatibility
    set(VST3_INSTALL_DIR "$ENV{COMMONPROGRAMFILES}/VST3" CACHE PATH "VST3 install directory")
else()
    set(VST3_INSTALL_DIR "$ENV{HOME}/.vst3" CACHE PATH "VST3 install directory")
endif()

# Install VST3 plugin using cmake --install
# Use the JUCE artefacts output directory structure
set(ARTEFACTS_DIR "${CMAKE_BINARY_DIR}/Sidechain_artefacts")

install(
    DIRECTORY "${ARTEFACTS_DIR}/$<CONFIG>/VST3/Sidechain.vst3"
    DESTINATION "${VST3_INSTALL_DIR}"
    COMPONENT VST3
)

# Install Standalone app
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
