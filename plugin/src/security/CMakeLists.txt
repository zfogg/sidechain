#==============================================================================
# SidechainSecurity - Security Library
# Secure storage, input validation, and rate limiting
#==============================================================================

add_library(SidechainSecurity STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/SecureTokenStore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SecureTokenStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/InputValidation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RateLimiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RateLimiter.h
)

# Include directories
target_include_directories(SidechainSecurity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link dependencies
target_link_libraries(SidechainSecurity PUBLIC
    SidechainJuceHeader
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Platform-specific dependencies for SecureTokenStore
if(APPLE)
    # macOS: Security framework for Keychain
    target_link_libraries(SidechainSecurity PUBLIC "-framework Security")
elseif(WIN32)
    # Windows: Crypt32 for DPAPI
    target_link_libraries(SidechainSecurity PUBLIC Crypt32)
elseif(UNIX AND NOT APPLE)
    # Linux: Secret Service via D-Bus
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(DBUS QUIET dbus-1)
        if(DBUS_FOUND)
            target_include_directories(SidechainSecurity PRIVATE ${DBUS_INCLUDE_DIRS})
            target_link_libraries(SidechainSecurity PRIVATE ${DBUS_LIBRARIES})
            target_compile_definitions(SidechainSecurity PRIVATE SIDECHAIN_HAS_DBUS=1)
            message(STATUS "Found D-Bus - using for secure storage (Secret Service)")
        else()
            target_compile_definitions(SidechainSecurity PRIVATE SIDECHAIN_HAS_DBUS=0)
            message(WARNING "D-Bus not found. SecureTokenStore will use fallback storage.")
        endif()
    else()
        target_compile_definitions(SidechainSecurity PRIVATE SIDECHAIN_HAS_DBUS=0)
    endif()
endif()

# Enable PIC for Release builds, disable for Debug (faster compilation)
# Handle both single-config (CMAKE_BUILD_TYPE) and multi-config generators (CMAKE_CONFIGURATION_TYPES)
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode) - set per-config via generator expressions
    set_target_properties(SidechainSecurity PROPERTIES
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
    set_target_properties(SidechainSecurity PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ${PIC_ENABLED}
    )
endif()
