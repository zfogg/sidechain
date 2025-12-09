#==============================================================================
# Windows Toolchain Configuration
# Sets up MSVC compiler paths and Windows SDK paths on Windows
# Must be included BEFORE project() to ensure proper compiler detection
#==============================================================================

if(NOT WIN32)
    return()
endif()

#==============================================================================
# vcpkg Integration (auto-detect if not already configured)
#==============================================================================
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    # Check for VCPKG_ROOT environment variable
    if(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_TOOLCHAIN "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${VCPKG_TOOLCHAIN}")
            set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN}" CACHE STRING "vcpkg toolchain file")
            message(STATUS "Auto-detected vcpkg at: $ENV{VCPKG_ROOT}")
        endif()
    else()
        # Try common vcpkg locations
        set(VCPKG_SEARCH_PATHS
            "$ENV{USERPROFILE}/vcpkg"
            "$ENV{USERPROFILE}/scoop/apps/vcpkg/current"
            "C:/vcpkg"
            "C:/src/vcpkg"
        )
        foreach(VCPKG_PATH ${VCPKG_SEARCH_PATHS})
            set(VCPKG_TOOLCHAIN "${VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake")
            if(EXISTS "${VCPKG_TOOLCHAIN}")
                set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN}" CACHE STRING "vcpkg toolchain file")
                message(STATUS "Auto-detected vcpkg at: ${VCPKG_PATH}")
                break()
            endif()
        endforeach()
    endif()
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Check if environment is already properly configured (e.g., from Developer Command Prompt)
# We check for both LIB containing Windows SDK and INCLUDE containing ucrt
if(DEFINED ENV{LIB} AND DEFINED ENV{INCLUDE})
    if("$ENV{LIB}" MATCHES "Windows Kits" AND "$ENV{INCLUDE}" MATCHES "ucrt")
        message(STATUS "Windows SDK paths already configured via environment")
        return()
    endif()
endif()

message(STATUS "Configuring Windows SDK and MSVC paths...")

#==============================================================================
# Find Visual Studio installation
#==============================================================================
set(VS_PATHS
    "C:/Program Files/Microsoft Visual Studio/2022/Community"
    "C:/Program Files/Microsoft Visual Studio/2022/Professional"
    "C:/Program Files/Microsoft Visual Studio/2022/Enterprise"
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional"
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise"
)

set(SIDECHAIN_VS_ROOT "")
foreach(PATH ${VS_PATHS})
    if(EXISTS "${PATH}/VC/Tools/MSVC")
        set(SIDECHAIN_VS_ROOT "${PATH}")
        break()
    endif()
endforeach()

if(NOT SIDECHAIN_VS_ROOT)
    message(WARNING "Visual Studio not found. Build may fail if LIB/INCLUDE not set.")
    return()
endif()

message(STATUS "Found Visual Studio at: ${SIDECHAIN_VS_ROOT}")

#==============================================================================
# Find MSVC toolset version (newest available)
#==============================================================================
file(GLOB MSVC_VERSIONS "${SIDECHAIN_VS_ROOT}/VC/Tools/MSVC/*")
list(SORT MSVC_VERSIONS ORDER DESCENDING)
list(GET MSVC_VERSIONS 0 SIDECHAIN_MSVC_ROOT)
get_filename_component(SIDECHAIN_MSVC_VERSION "${SIDECHAIN_MSVC_ROOT}" NAME)
message(STATUS "Using MSVC toolset: ${SIDECHAIN_MSVC_VERSION}")

#==============================================================================
# Find Windows SDK
#==============================================================================
set(SIDECHAIN_WIN_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")

if(NOT EXISTS "${SIDECHAIN_WIN_SDK_ROOT}/Include")
    message(WARNING "Windows SDK not found at ${SIDECHAIN_WIN_SDK_ROOT}")
    return()
endif()

# Find newest SDK version
file(GLOB SDK_VERSIONS "${SIDECHAIN_WIN_SDK_ROOT}/Include/*")
list(SORT SDK_VERSIONS ORDER DESCENDING)
# Filter to only directories starting with 10.
set(VALID_SDK_VERSIONS "")
foreach(SDK_PATH ${SDK_VERSIONS})
    get_filename_component(SDK_NAME "${SDK_PATH}" NAME)
    if(SDK_NAME MATCHES "^10\\.")
        list(APPEND VALID_SDK_VERSIONS "${SDK_PATH}")
    endif()
endforeach()

if(NOT VALID_SDK_VERSIONS)
    message(WARNING "No valid Windows SDK versions found")
    return()
endif()

list(GET VALID_SDK_VERSIONS 0 SDK_INCLUDE_ROOT)
get_filename_component(SIDECHAIN_SDK_VERSION "${SDK_INCLUDE_ROOT}" NAME)
message(STATUS "Using Windows SDK: ${SIDECHAIN_SDK_VERSION}")

#==============================================================================
# Detect architecture
#==============================================================================
if(CMAKE_GENERATOR_PLATFORM)
    set(SIDECHAIN_ARCH "${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_VS_PLATFORM_NAME)
    set(SIDECHAIN_ARCH "${CMAKE_VS_PLATFORM_NAME}")
else()
    # Default to x64
    set(SIDECHAIN_ARCH "x64")
endif()

# Map architecture names
if(SIDECHAIN_ARCH STREQUAL "Win32" OR SIDECHAIN_ARCH STREQUAL "x86")
    set(SIDECHAIN_MSVC_ARCH "x86")
    set(SIDECHAIN_SDK_ARCH "x86")
else()
    set(SIDECHAIN_MSVC_ARCH "x64")
    set(SIDECHAIN_SDK_ARCH "x64")
endif()

#==============================================================================
# Build include and library paths
#==============================================================================
set(SIDECHAIN_INCLUDE_PATHS
    "${SIDECHAIN_MSVC_ROOT}/include"
    "${SIDECHAIN_WIN_SDK_ROOT}/Include/${SIDECHAIN_SDK_VERSION}/ucrt"
    "${SIDECHAIN_WIN_SDK_ROOT}/Include/${SIDECHAIN_SDK_VERSION}/um"
    "${SIDECHAIN_WIN_SDK_ROOT}/Include/${SIDECHAIN_SDK_VERSION}/shared"
    "${SIDECHAIN_WIN_SDK_ROOT}/Include/${SIDECHAIN_SDK_VERSION}/winrt"
    "${SIDECHAIN_WIN_SDK_ROOT}/Include/${SIDECHAIN_SDK_VERSION}/cppwinrt"
)

set(SIDECHAIN_LIB_PATHS
    "${SIDECHAIN_MSVC_ROOT}/lib/${SIDECHAIN_MSVC_ARCH}"
    "${SIDECHAIN_WIN_SDK_ROOT}/Lib/${SIDECHAIN_SDK_VERSION}/ucrt/${SIDECHAIN_SDK_ARCH}"
    "${SIDECHAIN_WIN_SDK_ROOT}/Lib/${SIDECHAIN_SDK_VERSION}/um/${SIDECHAIN_SDK_ARCH}"
)

#==============================================================================
# Set environment variables for compiler detection and build
#==============================================================================
list(JOIN SIDECHAIN_INCLUDE_PATHS ";" SIDECHAIN_INCLUDE_ENV)
list(JOIN SIDECHAIN_LIB_PATHS ";" SIDECHAIN_LIB_ENV)

set(ENV{INCLUDE} "${SIDECHAIN_INCLUDE_ENV}")
set(ENV{LIB} "${SIDECHAIN_LIB_ENV}")
message(STATUS "Set INCLUDE and LIB environment variables for ${SIDECHAIN_ARCH}")

# Cache these paths for use elsewhere in CMake (they'll be available after project())
set(SIDECHAIN_WIN_INCLUDE_DIRS ${SIDECHAIN_INCLUDE_PATHS} CACHE INTERNAL "Windows SDK include directories")
set(SIDECHAIN_WIN_LIB_DIRS ${SIDECHAIN_LIB_PATHS} CACHE INTERNAL "Windows SDK library directories")

#==============================================================================
# Set compiler if not already specified
#==============================================================================
if(NOT CMAKE_C_COMPILER AND NOT CMAKE_CXX_COMPILER)
    set(MSVC_BIN "${SIDECHAIN_MSVC_ROOT}/bin/Hostx64/${SIDECHAIN_MSVC_ARCH}")
    if(EXISTS "${MSVC_BIN}/cl.exe")
        set(CMAKE_C_COMPILER "${MSVC_BIN}/cl.exe" CACHE FILEPATH "C compiler")
        set(CMAKE_CXX_COMPILER "${MSVC_BIN}/cl.exe" CACHE FILEPATH "C++ compiler")
        message(STATUS "Using MSVC compiler: ${MSVC_BIN}/cl.exe")
    endif()
endif()
