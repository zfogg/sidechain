#pragma once

#include <JuceHeader.h>

namespace Sidechain {
namespace Util {

/**
 * Utility functions for working with JUCE PropertiesFile
 */
class PropertiesFileUtils {
public:
  /**
   * Get standard PropertiesFile::Options for Sidechain plugin
   *
   * All PropertiesFile instances in the codebase should use these
   * consistent settings to ensure files are stored in the same location.
   *
   * @return Configured PropertiesFile::Options
   */
  static juce::PropertiesFile::Options getStandardOptions() {
    juce::PropertiesFile::Options options;
    options.applicationName = "Sidechain";
    options.filenameSuffix = ".settings";
    options.folderName = "SidechainPlugin";
    options.osxLibrarySubFolder = "Application Support"; // Required on macOS to avoid jassert
    return options;
  }
};

} // namespace Util
} // namespace Sidechain
