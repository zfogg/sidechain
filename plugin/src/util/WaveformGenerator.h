#pragma once

#include <JuceHeader.h>

// ==============================================================================
/**
 * WaveformGenerator provides utilities for generating waveform visualizations
 * from audio buffers.
 *
 * This is used in both StoryRecording and StoryViewer to display audio
 * waveforms.
 */
class WaveformGenerator {
public:
  // ==============================================================================
  /**
   * Generate a Path representing a waveform from an audio buffer.
   *
   * The waveform is drawn as a centered path, with peaks extending above and
   * below the center line based on the audio amplitude.
   *
   * @param buffer The audio buffer to generate waveform from
   * @param bounds The bounds where the waveform will be drawn
   * @return A Path representing the waveform
   */
  static juce::Path generateWaveformPath(const juce::AudioBuffer<float> &buffer, juce::Rectangle<int> bounds);

  /**
   * Generate a Path representing a waveform from an audio buffer loaded from a
   * URL. This will download and decode the audio file, then generate the
   * waveform.
   *
   * @param audioUrl The URL of the audio file
   * @param bounds The bounds where the waveform will be drawn
   * @param callback Called on the message thread with the generated path (or
   * empty path on error)
   */
  static void generateWaveformPathFromUrl(const juce::String &audioUrl, juce::Rectangle<int> bounds,
                                          std::function<void(juce::Path)> callback);

private:
  WaveformGenerator() = default; // Static utility class
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformGenerator)
};
