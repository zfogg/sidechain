#==============================================================================
# Compiler Flags
#==============================================================================

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

