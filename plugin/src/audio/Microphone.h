#pragma once

#include <JuceHeader.h>

// ==============================================================================
/**
 * Microphone handles recording audio from the system microphone
 *
 * Thread Safety:
 * - audioDeviceIOCallback() is called from the AUDIO THREAD
 * - All other methods are called from the MESSAGE THREAD
 * - Uses std::atomic for thread-safe state sharing
 *
 * Features:
 * - Records from system microphone using AudioDeviceManager
 * - Up to 60 seconds of recording
 * - Real-time level metering (peak + RMS)
 * - Similar API to AudioCapture for easy integration
 */
class Microphone : public juce::AudioIODeviceCallback {
public:
  Microphone();
  ~Microphone() override;

  // ==============================================================================
  // Configuration - call from message thread
  void prepare(double sampleRate = 44100.0, int numChannels = 1);
  void reset();

  // ==============================================================================
  // Recording control - call from MESSAGE THREAD only
  void startRecording(const juce::String &recordingId = "");
  juce::AudioBuffer<float> stopRecording();
  bool isRecording() const {
    return recording.load();
  }

  // ==============================================================================
  // AudioIODeviceCallback - called from AUDIO THREAD
  // Note: In JUCE 8, audioDeviceIOCallback is deprecated in favor of
  // audioDeviceIOCallbackWithContext This method is kept for compatibility but
  // override is removed as it may not be virtual in JUCE 8
  void audioDeviceIOCallback(const float **inputChannelData, int numInputChannels, float **outputChannelData,
                             int numOutputChannels, int numSamples);

  void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
  void audioDeviceStopped() override;
  void audioDeviceError(const juce::String &errorMessage) override;

  // ==============================================================================
  // Recording info - thread-safe reads
  double getRecordingLengthSeconds() const;
  int getRecordingLengthSamples() const {
    return recordingPosition.load();
  }
  int getMaxRecordingSamples() const {
    return maxRecordingSamples;
  }
  double getMaxRecordingLengthSeconds() const;
  float getRecordingProgress() const; // 0.0 - 1.0
  bool isBufferFull() const;

  // ==============================================================================
  // Level metering - thread-safe, updated during audioDeviceIOCallback
  float getPeakLevel(int channel) const;
  float getRMSLevel(int channel) const;
  void resetLevels();

  // ==============================================================================
  // Device management
  juce::StringArray getAvailableInputDevices() const;
  bool setInputDevice(const juce::String &deviceName);
  juce::String getCurrentInputDevice() const;
  bool isDeviceAvailable() const;

  // Get sample rate for export
  double getSampleRate() const {
    return currentSampleRate;
  }
  int getNumChannels() const {
    return currentNumChannels;
  }

  // ==============================================================================
  // Check if there is recorded audio available
  bool hasRecordedAudio() const {
    return hasRecordedData && recordedAudio.getNumSamples() > 0;
  }

  // Get the recorded audio buffer (for inspection/preview)
  const juce::AudioBuffer<float> &getRecordedAudioBuffer() const {
    return recordedAudio;
  }

private:
  // ==============================================================================
  // Thread-safe state (accessed from both threads)
  std::atomic<bool> recording{false};
  std::atomic<int> recordingPosition{0};

  // Level metering (written on audio thread, read on message thread)
  static constexpr int MaxChannels = 2;
  std::atomic<float> peakLevels[MaxChannels]{{0.0f}, {0.0f}};
  std::atomic<float> rmsLevels[MaxChannels]{{0.0f}, {0.0f}};

  // RMS calculation state (audio thread only)
  float rmsSums[MaxChannels] = {0.0f, 0.0f};
  int rmsSampleCount = 0;
  static constexpr int RMSWindowSamples = 2048; // ~46ms @ 44.1kHz

  // ==============================================================================
  // Configuration (set on message thread before recording)
  juce::String currentRecordingId;
  double currentSampleRate = 44100.0;
  int currentNumChannels = 1;
  int maxRecordingSamples = 0; // 60 seconds max

  // ==============================================================================
  // Recording buffer (allocated on message thread, written on audio thread)
  juce::AudioBuffer<float> recordingBuffer;

  // Recorded data (message thread only)
  juce::AudioBuffer<float> recordedAudio;
  bool hasRecordedData = false;

  // ==============================================================================
  // Audio device management
  std::unique_ptr<juce::AudioDeviceManager> audioDeviceManager;
  juce::String currentInputDeviceName;

  // ==============================================================================
  void initializeBuffers();
  void updateLevels(const float *const *inputChannelData, int numChannels, int numSamples);
  bool initializeAudioDevice();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Microphone)
};
