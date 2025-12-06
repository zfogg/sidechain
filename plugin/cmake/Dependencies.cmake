#==============================================================================
# External Dependencies
#==============================================================================

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
                set(SIDECHAIN_HAS_KEYFINDER TRUE CACHE INTERNAL "")
                # Enable position independent code for shared library linking
                set_target_properties(keyfinder PROPERTIES POSITION_INDEPENDENT_CODE ON)
                message(STATUS "libkeyfinder will be built from: ${LIBKEYFINDER_DIR}")
            endif()
        else()
            message(WARNING "libkeyfinder not found at ${LIBKEYFINDER_DIR}")
            message(STATUS "Run: git submodule update --init --recursive")
            set(SIDECHAIN_HAS_KEYFINDER FALSE CACHE INTERNAL "")
        endif()
    else()
        message(WARNING "FFTW3 not found - Key detection disabled")
        message(STATUS "Install FFTW3:")
        message(STATUS "  Arch:   sudo pacman -S fftw")
        message(STATUS "  Ubuntu: sudo apt install libfftw3-dev")
        message(STATUS "  macOS:  brew install fftw")
        set(SIDECHAIN_HAS_KEYFINDER FALSE CACHE INTERNAL "")
    endif()
else()
    message(STATUS "Key detection disabled")
    set(SIDECHAIN_HAS_KEYFINDER FALSE CACHE INTERNAL "")
endif()

#==============================================================================
# ASIO (Standalone) - Asynchronous I/O Library
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

    set(SIDECHAIN_HAS_ASIO TRUE CACHE INTERNAL "")
    message(STATUS "ASIO will be available as an interface library (standalone mode)")
else()
    message(WARNING "ASIO not found at ${ASIO_DIR}")
    message(STATUS "Run: git submodule update --init --recursive")
    set(SIDECHAIN_HAS_ASIO FALSE CACHE INTERNAL "")
endif()

#==============================================================================
# websocketpp - WebSocket Protocol Library (Header-Only)
#==============================================================================
set(WEBSOCKETPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/websocketpp")
if(EXISTS "${WEBSOCKETPP_DIR}/websocketpp")
    message(STATUS "websocketpp found at: ${WEBSOCKETPP_DIR}")

    # Create an interface library for websocketpp (header-only)
    add_library(websocketpp INTERFACE)
    target_include_directories(websocketpp INTERFACE "${WEBSOCKETPP_DIR}")

    # Tell websocketpp to use C++11 features instead of Boost
    target_compile_definitions(websocketpp INTERFACE
        _WEBSOCKETPP_CPP11_INTERNAL_
        _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
        _WEBSOCKETPP_CPP11_THREAD_
        _WEBSOCKETPP_CPP11_FUNCTIONAL_
        _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
        _WEBSOCKETPP_CPP11_MEMORY_
    )

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
        set(SIDECHAIN_HAS_OPENSSL TRUE CACHE INTERNAL "")
    else()
        message(WARNING "OpenSSL not found - TLS WebSocket connections (wss://) will not work")
        message(STATUS "Install OpenSSL:")
        message(STATUS "  macOS:   brew install openssl")
        message(STATUS "  Ubuntu:  sudo apt install libssl-dev")
        message(STATUS "  Arch:    sudo pacman -S openssl")
        set(SIDECHAIN_HAS_OPENSSL FALSE CACHE INTERNAL "")
    endif()

    set(SIDECHAIN_HAS_WEBSOCKETPP TRUE CACHE INTERNAL "")
    message(STATUS "websocketpp will be available as an interface library")
else()
    message(WARNING "websocketpp not found at ${WEBSOCKETPP_DIR}")
    message(STATUS "Run: git submodule update --init --recursive")
    set(SIDECHAIN_HAS_WEBSOCKETPP FALSE CACHE INTERNAL "")
endif()

