#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * DAWProjectFolder utility for detecting and accessing DAW project folders
 * (R.3.3.7.1)
 *
 * Provides functions to:
 * - Detect the current DAW hosting the plugin
 * - Get the DAW's project folder path (if accessible)
 * - Get DAW-specific MIDI import folder paths
 */
namespace DAWProjectFolder {
/** Structure representing a DAW project folder path */
struct DAWProjectInfo {
  juce::String dawName;      ///< DAW name (e.g., "Ableton Live")
  juce::File projectFolder;  ///< Main project folder (may be invalid if not accessible)
  juce::File midiFolder;     ///< MIDI-specific folder within project (may be invalid)
  bool isAccessible = false; ///< Whether the project folder is accessible
  juce::String errorMessage; ///< Error message if detection failed
};

/**
 * Detect the current DAW project folder information (R.3.3.7.1)
 *
 * Attempts to detect:
 * - Which DAW is hosting the plugin
 * - The DAW's current project folder (if accessible)
 * - The appropriate MIDI import folder for that DAW
 *
 * @param detectedDAWName Optional DAW name to use (if already detected). If
 * empty, will auto-detect.
 * @return DAWProjectInfo containing folder paths and accessibility status
 */
DAWProjectInfo detectDAWProjectFolder(const juce::String &detectedDAWName = "");

/**
 * Get the recommended MIDI file location for the current DAW (R.3.3.7.2)
 *
 * Returns the best location to save MIDI files:
 * 1. DAW project folder (if accessible)
 * 2. Default Sidechain MIDI folder as fallback
 *
 * @param detectedDAWName Optional DAW name to use (if already detected)
 * @return File path for saving MIDI files
 */
juce::File getMIDIFileLocation(const juce::String &detectedDAWName = "");

/**
 * Get the default MIDI folder location (fallback)
 * @return File path to ~/Documents/Sidechain/MIDI/
 */
juce::File getDefaultMIDIFolder();

/**
 * Check if a DAW project folder is accessible
 * @param folder The folder to check
 * @return True if folder exists and is writable
 */
bool isFolderAccessible(const juce::File &folder);
} // namespace DAWProjectFolder
