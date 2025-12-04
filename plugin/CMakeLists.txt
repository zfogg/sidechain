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

project(Sidechain VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release if no build type specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
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
    )
endif()

# Option to build AudioPluginHost for testing
option(SIDECHAIN_BUILD_PLUGIN_HOST "Build JUCE AudioPluginHost for testing" ON)

# Option to build tests
option(SIDECHAIN_BUILD_TESTS "Build unit tests with Catch2" OFF)

# Option to enable coverage (requires SIDECHAIN_BUILD_TESTS)
option(SIDECHAIN_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

# Option to install standalone app alongside VST3
option(SIDECHAIN_INSTALL_STANDALONE "Install Standalone app with cmake --install" OFF)

# JUCE is added as a subdirectory from the deps folder
# EXCLUDE_FROM_ALL prevents JUCE from being installed with cmake --install
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    # Fetch JUCE if not present
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
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
        Source/RecordingComponent.cpp
        Source/RecordingComponent.h
        Source/NetworkClient.cpp
        Source/NetworkClient.h
        Source/ConnectionIndicator.h
        Source/ProfileSetupComponent.cpp
        Source/ProfileSetupComponent.h
        Source/PostsFeedComponent.cpp
        Source/PostsFeedComponent.h
        Source/UploadComponent.cpp
        Source/UploadComponent.h
        Source/FeedPost.cpp
        Source/FeedPost.h
        Source/FeedDataManager.cpp
        Source/FeedDataManager.h
        Source/PostCardComponent.cpp
        Source/PostCardComponent.h
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

# Install Standalone app (optional)
if(SIDECHAIN_INSTALL_STANDALONE)
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

    # Create a static library from plugin source for testing (without plugin wrapper)
    add_library(SidechainTestLib STATIC
        Source/AudioCapture.cpp
        Source/AudioCapture.h
        Source/NetworkClient.cpp
        Source/NetworkClient.h
        Source/FeedPost.cpp
        Source/FeedPost.h
        Source/FeedDataManager.cpp
        Source/FeedDataManager.h
    )

    # SidechainTestLib needs access to Sidechain's generated JuceHeader.h
    # We need to build Sidechain first to generate the header
    add_dependencies(SidechainTestLib Sidechain)

    target_include_directories(SidechainTestLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
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
    )

    # Coverage flags
    if(SIDECHAIN_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(SidechainTestLib PRIVATE --coverage -fprofile-arcs -ftest-coverage)
            target_link_options(SidechainTestLib PRIVATE --coverage)
        endif()
    endif()

    # Test executable
    add_executable(SidechainTests
        tests/AudioCaptureTest.cpp
        tests/NetworkClientTest.cpp
        tests/FeedDataTest.cpp
    )

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
