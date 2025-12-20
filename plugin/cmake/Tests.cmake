#==============================================================================
# Tests Configuration
# Catch2-based unit test setup
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

    # Create a test library that uses the modular libraries
    # We link against the full libraries to ensure all dependencies are available
    add_library(SidechainTestLib STATIC
        # Audio sources for tests
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioCapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/AudioCapture.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MIDICapture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/MIDICapture.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/HttpAudioPlayer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/HttpAudioPlayer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/BufferAudioPlayer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/BufferAudioPlayer.h
        # KeyDetector is needed by NetworkClient
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/KeyDetector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/KeyDetector.h
    )

    # Disable LTO for test library to ensure symbols are preserved
    # Static library symbols need to be available when linked with LTO
    set_target_properties(SidechainTestLib PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION FALSE
    )

    # SidechainTestLib needs access to Sidechain's generated JuceHeader.h
    # We need to build Sidechain first to generate the header
    add_dependencies(SidechainTestLib Sidechain)

    target_include_directories(SidechainTestLib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # Use the JuceHeader generated early
        ${CMAKE_CURRENT_BINARY_DIR}/SidechainJuceHeaderHelper_artefacts/JuceLibraryCode
    )

    # Link to JuceHeader target to ensure it's built first
    target_link_libraries(SidechainTestLib PUBLIC SidechainJuceHeader)
    add_dependencies(SidechainTestLib SidechainJuceHeaderHelper)

    target_compile_definitions(SidechainTestLib PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STANDALONE_APPLICATION=1
    )

    # Link against modular libraries instead of duplicating sources
    target_link_libraries(SidechainTestLib PUBLIC
        SidechainCore
        SidechainNetwork
        # Use test audio lib for subset of audio sources
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

    # Link libkeyfinder if available (needed for KeyDetector)
    if(SIDECHAIN_HAS_KEYFINDER)
        target_include_directories(SidechainTestLib PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../deps/libkeyfinder/src
        )
        target_link_libraries(SidechainTestLib PRIVATE keyfinder)
        target_compile_definitions(SidechainTestLib PUBLIC SIDECHAIN_HAS_KEYFINDER=1)
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
        tests/PostCardTest.cpp
        tests/MessagingE2ETest.cpp
        tests/EntitySliceTest.cpp
    )

    # Test executable
    add_executable(SidechainTests ${SIDECHAIN_TEST_SOURCES})

    # Disable LTO for test executable when linking against non-LTO static library
    # Mixing LTO and non-LTO can cause symbol resolution issues
    set_target_properties(SidechainTests PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION FALSE
    )
    target_compile_options(SidechainTests PRIVATE -fno-lto)
    target_link_options(SidechainTests PRIVATE -fno-lto)

    # SidechainNetwork depends on KeyDetector which is in SidechainTestLib
    # List SidechainTestLib last to resolve the circular dependency
    target_link_libraries(SidechainTests PRIVATE
        Catch2::Catch2WithMain
        Sidechain
        SidechainNetwork
        SidechainUI
        SidechainAudio
        SidechainTestLib
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

    message(STATUS "Test Configuration:")
    message(STATUS "  Tests enabled:   ON")
    message(STATUS "  Coverage:        ${SIDECHAIN_ENABLE_COVERAGE}")
    message(STATUS "  Test binary:     ${CMAKE_BINARY_DIR}/SidechainTests")
    message(STATUS "  JUnit output:    ${CMAKE_BINARY_DIR}/test-results/")
endif()

