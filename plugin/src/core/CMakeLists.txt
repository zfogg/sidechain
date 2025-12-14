#==============================================================================
# Sidechain - Main Plugin Target
# Plugin entry points only - links all library modules
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
    # Set to TRUE so standalone app generates audio (for feed playback)
    # Even though we're not a traditional synth, we generate audio output
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # Plugin format categories
    VST3_CATEGORIES "Network Tools Synth"
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

    # Configure standalone to use microphone input by default
    # JUCE will enable input channels and use the default input device (microphone)

    # Don't copy after build in CI - we handle installation separately
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Use the already-generated JuceHeader from SidechainJuceHeader
# (juce_generate_juce_header is called in main CMakeLists.txt)
# Link to the header target to get the include path
target_link_libraries(Sidechain PRIVATE SidechainJuceHeader)

# Add plugin entry point sources only
target_sources(Sidechain PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.h
)

# Preprocessor definitions
target_compile_definitions(Sidechain
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1  # Enable curl for HTTPS audio downloads
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_PreferredChannelConfigurations=1
        JucePlugin_ChannelConfigurations="{2, 2}"
)

# Configure standalone app to use microphone by default
# JUCE's auto-generated standalone wrapper will use the default input device (microphone)
# when preferredDefaultNumInputChannels is set via StandalonePluginHolder::Options
if(SIDECHAIN_BUILD_STANDALONE)
    message(STATUS "Standalone app will be configured to use microphone input by default")
    # The audio device configuration is handled in PluginProcessor for standalone mode
endif()

# Add generated headers to include path (for Version.h)
target_include_directories(Sidechain PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    "${CMAKE_CURRENT_BINARY_DIR}/../../generated"
)

# Link all library modules
target_link_libraries(Sidechain
    PRIVATE
        SidechainUtil
        SidechainModels
        SidechainNetwork
        SidechainAudio
        SidechainStores
        SidechainUI
        # JUCE modules
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
        juce::juce_recommended_warning_flags
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

# Link curl library for HTTPS support
find_package(CURL REQUIRED)
target_link_libraries(Sidechain PRIVATE CURL::libcurl)

# Link libkeyfinder if available (already linked via SidechainAudio, but keep for compatibility)
if(SIDECHAIN_HAS_KEYFINDER)
    target_compile_definitions(Sidechain PUBLIC SIDECHAIN_HAS_KEYFINDER=1)
endif()

# Link websocketpp if available (already linked via SidechainNetwork, but keep for compatibility)
if(SIDECHAIN_HAS_WEBSOCKETPP)
    message(STATUS "Sidechain will link with websocketpp for WebSocket support")
endif()

# Copy AudioPluginHost to bin/ after build (Sidechain_All is created by juce_add_plugin)
if(SIDECHAIN_BUILD_PLUGIN_HOST AND TARGET AudioPluginHost)
    # Make sure AudioPluginHost builds when we build our plugin
    add_dependencies(Sidechain_All AudioPluginHost)

    # Copy after Sidechain_All builds (which depends on AudioPluginHost)
    # CMAKE_BINARY_DIR is the root build directory (plugin/build), not the subdirectory
    add_custom_command(
        TARGET Sidechain_All POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/../bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/AudioPluginHost/AudioPluginHost_artefacts/$<CONFIG>/AudioPluginHost"
            "${CMAKE_BINARY_DIR}/../bin/AudioPluginHost"
        COMMENT "Copying AudioPluginHost to bin/"
    )
endif()

