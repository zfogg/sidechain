#==============================================================================
# External Dependencies
#==============================================================================

#==============================================================================
# Helper function to suppress JUCE-specific warnings
# Only applied to JUCE libraries, not our code
#==============================================================================
function(suppress_juce_warnings target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(target_type ${target_name} TYPE)

    # For interface libraries, mark includes as SYSTEM (suppresses warnings from header-only libs)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        get_target_property(include_dirs ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
        if(include_dirs)
            set_target_properties(${target_name} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${include_dirs}"
            )
        endif()
    else()
        # For compiled JUCE modules, apply targeted warning suppressions
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE
                -Wno-unused-parameter
                -Wno-missing-field-initializers
                -Wno-deprecated-declarations
                -Wno-sign-conversion
                -Wno-missing-braces
                -Wno-unused-function
                -Wno-unused-variable
                -Wno-float-equal
                -Wno-switch-enum
                -Wno-shadow
                -Wno-virtual-in-final
                -Wno-unnecessary-virtual-specifier
            )
        elseif(MSVC)
            target_compile_options(${target_name} PRIVATE
                /wd4100 /wd4244 /wd4267 /wd4458
            )
        endif()

        # Also mark include directories as SYSTEM
        get_target_property(include_dirs ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
        if(include_dirs)
            set_target_properties(${target_name} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${include_dirs}"
            )
        endif()
    endif()
endfunction()


#==============================================================================
# Dependencies Cache Directory
# Dependencies are built to a separate cache directory so they survive
# `cmake --build --clean-first` rebuilds. Only plugin code rebuilds.
#==============================================================================
set(SIDECHAIN_DEPS_CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.cache/cmake/${CMAKE_BUILD_TYPE}"
    CACHE PATH "Cache directory for dependency builds")
file(MAKE_DIRECTORY "${SIDECHAIN_DEPS_CACHE_DIR}")
message(STATUS "Dependencies cache: ${SIDECHAIN_DEPS_CACHE_DIR}")

# Set FetchContent base directory to use our cache (for any FetchContent dependencies)
set(FETCHCONTENT_BASE_DIR "${SIDECHAIN_DEPS_CACHE_DIR}/_deps" CACHE PATH "" FORCE)

#==============================================================================
# JUCE - Build to cache directory
#==============================================================================
# Dependencies are built to .cache/cmake/${CMAKE_BUILD_TYPE}/ so they persist
# when you delete plugin/build/. Ninja will skip rebuilding if already built.

set(JUCE_CACHE_DIR "${SIDECHAIN_DEPS_CACHE_DIR}/JUCE")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE" "${JUCE_CACHE_DIR}" EXCLUDE_FROM_ALL)
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
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../deps/JUCE/extras/AudioPluginHost" "${SIDECHAIN_DEPS_CACHE_DIR}/AudioPluginHost" EXCLUDE_FROM_ALL)
endif()

# Suppress warnings for all JUCE modules
set(JUCE_MODULES
    juce_analytics
    juce_audio_basics
    juce_audio_devices
    juce_audio_formats
    juce_audio_plugin_client
    juce_audio_processors
    juce_audio_utils
    juce_box2d
    juce_core
    juce_cryptography
    juce_data_structures
    juce_dsp
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
    juce_midi_ci
    juce_opengl
    juce_osc
    juce_product_unlocking
    juce_video
)
foreach(module ${JUCE_MODULES})
    suppress_juce_warnings(${module})
endforeach()

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
        set(LIBKEYFINDER_CACHE_DIR "${SIDECHAIN_DEPS_CACHE_DIR}/libkeyfinder")
        set(LIBKEYFINDER_LIB "${LIBKEYFINDER_CACHE_DIR}/libkeyfinder.a")

        if(EXISTS "${LIBKEYFINDER_DIR}/CMakeLists.txt")
            # Check if libkeyfinder is already built in cache
            if(EXISTS "${LIBKEYFINDER_LIB}")
                message(STATUS "Using pre-built libkeyfinder from cache: ${LIBKEYFINDER_LIB}")

                # Create IMPORTED target pointing to cached library
                add_library(keyfinder STATIC IMPORTED GLOBAL)
                set_target_properties(keyfinder PROPERTIES
                    IMPORTED_LOCATION "${LIBKEYFINDER_LIB}"
                    INTERFACE_INCLUDE_DIRECTORIES "${LIBKEYFINDER_DIR}/src"
                    INTERFACE_LINK_LIBRARIES "${FFTW3_LIBRARIES}"
                    POSITION_INDEPENDENT_CODE ON
                )

                set(SIDECHAIN_HAS_KEYFINDER TRUE CACHE INTERNAL "")
            else()
                message(STATUS "Building libkeyfinder to cache (first time)...")

                # Configure libkeyfinder build options
                set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
                set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

                # Add libkeyfinder subdirectory - use cache dir for binary output
                add_subdirectory("${LIBKEYFINDER_DIR}" "${LIBKEYFINDER_CACHE_DIR}" EXCLUDE_FROM_ALL)

                # Create an alias for cleaner usage
                if(TARGET keyfinder)
                    set(SIDECHAIN_HAS_KEYFINDER TRUE CACHE INTERNAL "")
                    # Enable position independent code for shared library linking
                    set_target_properties(keyfinder PROPERTIES POSITION_INDEPENDENT_CODE ON)
                    # Suppress JUCE-like warnings from libkeyfinder
                    suppress_juce_warnings(keyfinder)
                endif()
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
    # Use SYSTEM to suppress warnings from ASIO headers
    if(EXISTS "${ASIO_DIR}/asio/include/asio.hpp")
        target_include_directories(asio SYSTEM INTERFACE "${ASIO_DIR}/asio/include")
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
        # Define Windows version to avoid ASIO warnings
        # 0x0A00 = Windows 10, 0x0601 = Windows 7
        target_compile_definitions(asio INTERFACE
            _WIN32_WINNT=0x0A00
            WINVER=0x0A00
        )
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
    # Use SYSTEM to suppress warnings from websocketpp headers
    add_library(websocketpp INTERFACE)
    target_include_directories(websocketpp SYSTEM INTERFACE "${WEBSOCKETPP_DIR}")

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

#==============================================================================
# RxCpp - Reactive Extensions for C++ (Header-Only)
#==============================================================================
set(RXCPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/RxCpp")
if(EXISTS "${RXCPP_DIR}/Rx/v2/src/rxcpp/rx.hpp")
    message(STATUS "RxCpp found at: ${RXCPP_DIR}")

    # Create an interface library for RxCpp (header-only)
    # Use SYSTEM to suppress warnings from RxCpp headers
    add_library(rxcpp INTERFACE)
    target_include_directories(rxcpp SYSTEM INTERFACE "${RXCPP_DIR}/Rx/v2/src")

    # RxCpp requires C++14 or later (we're using C++20+)
    target_compile_features(rxcpp INTERFACE cxx_std_14)

    # RxCpp uses std threading instead of Boost
    set_target_properties(rxcpp PROPERTIES INTERFACE_SYSTEM TRUE)

    set(SIDECHAIN_HAS_RXCPP TRUE CACHE INTERNAL "")
    message(STATUS "RxCpp will be available as an interface library (header-only)")
else()
    message(WARNING "RxCpp not found at ${RXCPP_DIR}")
    message(STATUS "Run: git clone --depth=1 --branch v4.1.1 https://github.com/ReactiveX/RxCpp.git ${RXCPP_DIR}")
    set(SIDECHAIN_HAS_RXCPP FALSE CACHE INTERNAL "")
endif()

#==============================================================================
# stduuid - UUID Generation Library (Header-Only)
#==============================================================================
set(STDUUID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/uuid")
if(EXISTS "${STDUUID_DIR}/include/uuid.h")
    message(STATUS "stduuid found at: ${STDUUID_DIR}")

    # Create an interface library for stduuid (header-only)
    # Use SYSTEM to suppress warnings from stduuid headers
    add_library(stduuid INTERFACE)
    target_include_directories(stduuid SYSTEM INTERFACE "${STDUUID_DIR}/include")

    # stduuid requires C++20 or later (we're using C++20+)
    target_compile_features(stduuid INTERFACE cxx_std_20)

    set(SIDECHAIN_HAS_STDUUID TRUE CACHE INTERNAL "")
    message(STATUS "stduuid will be available as an interface library (header-only)")
else()
    message(WARNING "stduuid not found at ${STDUUID_DIR}")
    message(STATUS "Run: git submodule update --init --recursive")
    set(SIDECHAIN_HAS_STDUUID FALSE CACHE INTERNAL "")
endif()

