#==============================================================================
# Installation Configuration
# Handles installation of VST3, AU, and Standalone plugin formats
#==============================================================================

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
message(STATUS "========================================")
message(STATUS "  Plugin Installation Paths")
message(STATUS "  VST3: ${VST3_INSTALL_DIR}")
if(APPLE)
    message(STATUS "  AU:   ${AU_INSTALL_DIR}")
endif()

# Install VST3 plugin using cmake --install
# Use the JUCE artefacts output directory structure
set(ARTEFACTS_DIR "${CMAKE_BINARY_DIR}/src/core/Sidechain_artefacts")

install(
    DIRECTORY "${ARTEFACTS_DIR}/$<CONFIG>/VST3/Sidechain.vst3"
    DESTINATION "${VST3_INSTALL_DIR}"
    COMPONENT VST3
)

# Install Standalone app (optional - only if it was built)
if(SIDECHAIN_INSTALL_STANDALONE AND SIDECHAIN_BUILD_STANDALONE)
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

# Note: AudioPluginHost setup is done in src/core/CMakeLists.txt after plugin target is created

