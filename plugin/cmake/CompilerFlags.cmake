#==============================================================================
# Compiler Flags
#==============================================================================

include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#==============================================================================
# Windows SDK include directories (when not using Developer Command Prompt)
# These are needed when building with Ninja outside of VS environment
#==============================================================================
if(MSVC AND DEFINED SIDECHAIN_WIN_INCLUDE_DIRS)
    include_directories(SYSTEM ${SIDECHAIN_WIN_INCLUDE_DIRS})
    link_directories(${SIDECHAIN_WIN_LIB_DIRS})
    message(STATUS "Added Windows SDK include/lib directories globally")
endif()

#==============================================================================
# Global include directories for all targets
#==============================================================================
# Add RxCpp header-only library to all targets
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/../deps/RxCpp/Rx/v2/src")

# Strict warning flags for our code (no JUCE suppressions here)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # GCC/Clang flags - always supported
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )

    # Conditionally supported flags (check compiler support) - only essentials, not JUCE suppressions
    set(CONDITIONAL_CXX_FLAGS
        "-Wno-implicit-int-float-conversion"          # Audio processing math (GCC 7.1+, Clang 5+)
        "-Wno-nullable-to-nonnull-conversion"         # macOS API bridge (Clang 3.8+)
        "-Wno-missing-designated-field-initializers"  # Designated field init (GCC 9.2+, Clang 10+)
        "-Wno-implicit-int-conversion-on-negation"    # int8_t conversions in math (Clang 12+)
        "-Wnan-infinity-disabled"                     # Audio processing math
    )

    foreach(flag ${CONDITIONAL_CXX_FLAGS})
        check_cxx_compiler_flag("${flag}" CXX_FLAG_SUPPORTED_${flag})
        if(CXX_FLAG_SUPPORTED_${flag})
            add_compile_options("${flag}")
        endif()
    endforeach()

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

        # Use LLD linker for faster link times on macOS (if available)
        # Note: LLD + LTO has compatibility issues, so only enable if LTO is also available
        find_program(LLD_LINKER ld.lld)
        if(LLD_LINKER)
            message(STATUS "LLD linker found at: ${LLD_LINKER}")
            message(STATUS "NOTE: LLD+LTO has compatibility issues on macOS with mixed compilations")
            message(STATUS "To use LLD, disable LTO or rebuild all dependencies with matching settings")
            message(STATUS "For now, using default macOS linker (ld64)")
        endif()
    elseif(UNIX AND NOT APPLE)
        # Linux-specific
        add_compile_options(-fvisibility=hidden)
        # Note: LTO is applied per-target later, not globally, to avoid issues with helper executables
        add_link_options("$<$<CONFIG:Release>:-s>")  # Strip symbols

        # Use LLD linker for faster link times (if available)
        find_program(LLD_LINKER ld.lld)
        if(LLD_LINKER)
            message(STATUS "Using LLD linker for faster link times: ${LLD_LINKER}")
            add_link_options(-fuse-ld=lld)
        else()
            message(STATUS "LLD linker not found, using default linker. Install lld for faster builds.")
        endif()
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

#==============================================================================
# JUCE Library Warning Suppressions
# These flags are only for JUCE and similar third-party code
#==============================================================================
function(suppress_juce_warnings target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # JUCE-specific warning suppressions (not for our code)
        target_compile_options(${target_name} PRIVATE
            -Wno-unused-parameter                      # JUCE has unused params
            -Wno-missing-field-initializers             # JUCE structures initialization
            -Wno-deprecated-declarations                # JUCE uses deprecated APIs
            -Wno-sign-conversion                        # size_t conversions
            -Wno-missing-braces                         # JUCE macros
            -Wno-unused-function                        # JUCE conditionally used functions
            -Wno-unused-variable                        # JUCE unused vars
            -Wno-float-equal                            # JUCE comparisons
            -Wno-switch-enum                            # JUCE enum handling
            -Wno-shadow                                 # JUCE captures
            -Wno-virtual-in-final                       # JUCE final classes
            -Wno-unnecessary-virtual-specifier          # JUCE VST3
            -Wno-nan-infinity-disabled                  # Audio processing math
        )
    elseif(MSVC)
        target_compile_options(${target_name} PRIVATE
            /wd4100 # Unreferenced formal parameter
            /wd4244 # Conversion warnings
            /wd4267 # Size_t conversions
            /wd4458 # Declaration hides class member
        )
    endif()
endfunction()
