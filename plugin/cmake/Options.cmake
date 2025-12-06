#==============================================================================
# CMake Options
#==============================================================================

# Option to build AudioPluginHost for testing
option(SIDECHAIN_BUILD_PLUGIN_HOST "Build JUCE AudioPluginHost for testing" ON)

# Option to build tests
option(SIDECHAIN_BUILD_TESTS "Build unit tests with Catch2" OFF)

# Option to enable coverage (requires SIDECHAIN_BUILD_TESTS)
option(SIDECHAIN_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

# Option to build Standalone app (default OFF - only build VST3/AU by default)
option(SIDECHAIN_BUILD_STANDALONE "Build Standalone app (in addition to VST3/AU)" ON)

# Option to install standalone app alongside VST3
option(SIDECHAIN_INSTALL_STANDALONE "Install Standalone app with cmake --install" OFF)

# Option to build documentation (Doxygen + Sphinx)
option(SIDECHAIN_BUILD_DOCS "Build documentation with Doxygen and Sphinx" OFF)

# Option to enable key detection (requires FFTW3)
option(SIDECHAIN_ENABLE_KEY_DETECTION "Enable musical key detection using libkeyfinder" ON)

