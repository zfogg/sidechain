#pragma once

#include <JuceHeader.h>

/**
 * NotificationSound - Simple utility to play notification sounds
 *
 * Tries to use the operating system's standard notification sound first,
 * falling back to a generated beep if no system sound is available.
 * Uses JUCE's AudioDeviceManager to play audio without interfering
 * with the plugin's main audio processing.
 */
class NotificationSound {
public:
  /** Play a notification sound
   * Tries to use OS standard notification sound, falls back to generated beep
   * This is a non-blocking operation that plays on a separate audio device
   */
  static void playBeep();

private:
  /** Try to find and play system notification sound
   * @return true if system sound was found and played, false otherwise
   */
  static bool tryPlaySystemSound();

  /** Generate and play a simple beep sound (fallback) */
  static void playGeneratedBeep();

  static juce::AudioDeviceManager *getAudioDeviceManager();
  static void scheduleCleanup();
  static std::unique_ptr<juce::AudioDeviceManager> audioDeviceManager;
  static std::unique_ptr<juce::AudioSourcePlayer> audioSourcePlayer;
  static std::unique_ptr<juce::AudioSource> beepSource; // Can be AudioFormatReaderSource or BufferAudioSource
};
