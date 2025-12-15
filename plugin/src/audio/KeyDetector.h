#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * KeyDetector provides musical key detection for audio data.
 *
 * Uses libkeyfinder (when available) to analyze audio and determine
 * the musical key. Falls back to manual selection when libkeyfinder
 * is not compiled in.
 *
 * Key Detection Process:
 * 1. Audio is resampled to 4410Hz (optimal for key detection)
 * 2. libkeyfinder's KeyFinder analyzes the audio
 * 3. Returns key in standard notation (e.g., "A minor", "F# major")
 *
 * Camelot Wheel Support:
 * Also provides Camelot notation for DJ mixing compatibility
 * (e.g., "8A" for A minor, "4B" for E major)
 */
class KeyDetector {
public:
  //==========================================================================
  // Key representation
  struct Key {
    juce::String name;      // Standard name: "A minor", "F# major"
    juce::String shortName; // Short: "Am", "F#"
    juce::String camelot;   // Camelot: "8A", "4B"
    bool isMajor = true;
    int rootNote = 0;        // 0-11 (C=0, C#=1, ..., B=11)
    float confidence = 0.0f; // 0.0-1.0 detection confidence

    bool isValid() const {
      return name.isNotEmpty();
    }

    // Create from standard key string
    static Key fromString(const juce::String &keyStr);

    // Get all 24 keys in Camelot order (useful for UI dropdowns)
    static juce::StringArray getAllKeys();
    static juce::StringArray getAllCamelotKeys();
  };

  //==========================================================================
  KeyDetector();
  ~KeyDetector();

  //==========================================================================
  /**
   * Analyze audio buffer and detect the musical key
   *
   * @param buffer Audio buffer (any sample rate, will be resampled)
   * @param sampleRate Sample rate of the audio
   * @param numChannels Number of channels (will be mixed to mono)
   * @return Detected key (check isValid() for success)
   */
  Key detectKey(const juce::AudioBuffer<float> &buffer, double sampleRate, int numChannels);

  /**
   * Analyze audio from file
   *
   * @param audioFile Audio file to analyze
   * @return Detected key
   */
  Key detectKeyFromFile(const juce::File &audioFile);

  //==========================================================================
  /**
   * Check if key detection is available
   * (libkeyfinder compiled in and initialized)
   */
  static bool isAvailable();

  //==========================================================================
  // Key name utilities

  /**
   * Convert key enum value to string
   * Maps libkeyfinder's key_t enum to readable strings
   */
  static juce::String keyToString(int keyValue);

  /**
   * Convert key to Camelot notation
   */
  static juce::String keyToCamelot(int keyValue);

  /**
   * Get the root note name (C, C#, D, etc.)
   */
  static juce::String getRootNoteName(int keyValue);

  /**
   * Check if key is major (vs minor)
   */
  static bool isMajorKey(int keyValue);

private:
  class Impl;
  std::unique_ptr<Impl> impl;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyDetector)
};
