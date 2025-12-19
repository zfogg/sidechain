#include "Microphone.h"
#include "../util/Constants.h"
#include "../util/Log.h"

// ==============================================================================
Microphone::Microphone() {
  audioDeviceManager = std::make_unique<juce::AudioDeviceManager>();
}

Microphone::~Microphone() {
  // Stop recording if active
  if (isRecording()) {
    stopRecording();
  }

  // Remove callback and close device
  if (audioDeviceManager) {
    audioDeviceManager->removeAudioCallback(this);
    audioDeviceManager->closeAudioDevice();
  }
}

// ==============================================================================
void Microphone::prepare(double sampleRate, int numChannels) {
  currentSampleRate = sampleRate;
  currentNumChannels = juce::jmin(numChannels, MaxChannels);

  // Initialize buffers for max recording duration
  maxRecordingSamples = static_cast<int>(sampleRate * Constants::Audio::MAX_RECORDING_SECONDS);

  initializeBuffers();
  resetLevels();

  Log::info("Microphone prepared: " + juce::String(sampleRate) + "Hz, " + juce::String(currentNumChannels) +
            " channels, " + juce::String(maxRecordingSamples) + " max samples (" +
            juce::String(maxRecordingSamples / sampleRate) + "s)");
}

void Microphone::reset() {
  recording.store(false);
  recordingPosition.store(0);
  hasRecordedData = false;
  recordedAudio.clear();
  resetLevels();
}

// ==============================================================================
void Microphone::startRecording(const juce::String &recordingId) {
  if (recording.load()) {
    Log::warn("Microphone: Already recording, ignoring start request");
    return;
  }

  // Initialize audio device if not already done
  if (!initializeAudioDevice()) {
    Log::error("Microphone: Failed to initialize audio device");
    return;
  }

  currentRecordingId = recordingId;
  hasRecordedData = false;

  // Clear the recording buffer
  recordingBuffer.clear();

  // Reset levels before starting
  resetLevels();
  rmsSums[0] = 0.0f;
  rmsSums[1] = 0.0f;
  rmsSampleCount = 0;

  // Reset position and start recording (order matters for thread safety)
  recordingPosition.store(0);
  recording.store(true);

  Log::info("Microphone: Started recording: " + recordingId);
}

juce::AudioBuffer<float> Microphone::stopRecording() {
  if (!recording.load()) {
    Log::warn("Microphone: Not recording, cannot stop");
    return juce::AudioBuffer<float>();
  }

  // Stop recording flag first
  recording.store(false);

  // Wait a moment for any in-flight audio callbacks to finish
  juce::Thread::sleep(50);

  // Extract recorded audio
  int recordedSamples = recordingPosition.load();
  juce::AudioBuffer<float> result;

  if (recordedSamples > 0) {
    result.setSize(currentNumChannels, recordedSamples, false, false, true);

    for (int channel = 0; channel < currentNumChannels; ++channel) {
      result.copyFrom(channel, 0, recordingBuffer, channel, 0, recordedSamples);
    }

    hasRecordedData = true;
    recordedAudio = result;

    Log::info("Microphone: Stopped recording: " + juce::String(recordedSamples) + " samples (" +
              juce::String(recordedSamples / currentSampleRate, 2) + "s)");
  } else {
    Log::warn("Microphone: No audio recorded");
  }

  // Reset for next recording
  recordingPosition.store(0);
  currentRecordingId = "";

  return result;
}

// ==============================================================================
void Microphone::audioDeviceIOCallback(const float **inputChannelData, int numInputChannels,
                                       float ** /* outputChannelData */, int /* numOutputChannels */, int numSamples) {
  // Fast exit if not recording (atomic read)
  if (!recording.load(std::memory_order_relaxed))
    return;

  int currentPos = recordingPosition.load(std::memory_order_relaxed);

  // Check if we've reached the max
  if (currentPos >= maxRecordingSamples)
    return;

  // Calculate how many samples we can write
  int samplesToWrite = juce::jmin(numSamples, maxRecordingSamples - currentPos);

  if (samplesToWrite > 0 && inputChannelData != nullptr && numInputChannels > 0) {
    // Copy audio data to recording buffer (lock-free write)
    int channelsToRecord = juce::jmin(numInputChannels, currentNumChannels);

    for (int channel = 0; channel < channelsToRecord; ++channel) {
      if (inputChannelData[channel] != nullptr) {
        recordingBuffer.copyFrom(channel, currentPos, inputChannelData[channel], samplesToWrite);
      }
    }

    // Update position atomically
    recordingPosition.store(currentPos + samplesToWrite, std::memory_order_relaxed);
  }

  // Update level meters (always, even when buffer is full)
  updateLevels(inputChannelData, numInputChannels, samplesToWrite);
}

void Microphone::audioDeviceAboutToStart(juce::AudioIODevice *device) {
  if (device != nullptr) {
    double deviceSampleRate = device->getCurrentSampleRate();
    int deviceNumChannels = device->getActiveInputChannels().countNumberOfSetBits();

    // Update our configuration to match the device
    prepare(deviceSampleRate, juce::jmax(1, deviceNumChannels));

    Log::info("Microphone: Audio device starting - " + juce::String(deviceSampleRate) + "Hz, " +
              juce::String(deviceNumChannels) + " input channels");
  }
}

void Microphone::audioDeviceStopped() {
  Log::info("Microphone: Audio device stopped");
}

void Microphone::audioDeviceError(const juce::String &errorMessage) {
  Log::error("Microphone: Audio device error: " + errorMessage);
}

// ==============================================================================
double Microphone::getRecordingLengthSeconds() const {
  if (currentSampleRate <= 0)
    return 0.0;

  return recordingPosition.load() / currentSampleRate;
}

double Microphone::getMaxRecordingLengthSeconds() const {
  if (currentSampleRate <= 0)
    return 60.0;

  return maxRecordingSamples / currentSampleRate;
}

float Microphone::getRecordingProgress() const {
  if (maxRecordingSamples <= 0)
    return 0.0f;

  return static_cast<float>(recordingPosition.load()) / static_cast<float>(maxRecordingSamples);
}

bool Microphone::isBufferFull() const {
  return recordingPosition.load() >= maxRecordingSamples;
}

// ==============================================================================
float Microphone::getPeakLevel(int channel) const {
  if (channel < 0 || channel >= MaxChannels)
    return 0.0f;

  return peakLevels[channel].load(std::memory_order_relaxed);
}

float Microphone::getRMSLevel(int channel) const {
  if (channel < 0 || channel >= MaxChannels)
    return 0.0f;

  return rmsLevels[channel].load(std::memory_order_relaxed);
}

void Microphone::resetLevels() {
  for (int i = 0; i < MaxChannels; ++i) {
    peakLevels[i].store(0.0f, std::memory_order_relaxed);
    rmsLevels[i].store(0.0f, std::memory_order_relaxed);
  }
}

// ==============================================================================
juce::StringArray Microphone::getAvailableInputDevices() const {
  juce::StringArray devices;

  if (audioDeviceManager) {
    auto &deviceTypes = audioDeviceManager->getAvailableDeviceTypes();

    for (auto *deviceType : deviceTypes) {
      deviceType->scanForDevices();
      auto inputDevices = deviceType->getDeviceNames(true); // true = input devices

      for (const auto &deviceName : inputDevices) {
        devices.add(deviceName);
      }
    }
  }

  return devices;
}

bool Microphone::setInputDevice(const juce::String &deviceName) {
  if (!audioDeviceManager)
    return false;

  auto &deviceTypes = audioDeviceManager->getAvailableDeviceTypes();

  for (auto *deviceType : deviceTypes) {
    deviceType->scanForDevices();
    auto inputDevices = deviceType->getDeviceNames(true);

    if (inputDevices.contains(deviceName)) {
      currentInputDeviceName = deviceName;

      // Close current device if open
      audioDeviceManager->closeAudioDevice();
      audioDeviceManager->removeAudioCallback(this);

      // Reinitialize with new device
      return initializeAudioDevice();
    }
  }

  return false;
}

juce::String Microphone::getCurrentInputDevice() const {
  return currentInputDeviceName;
}

bool Microphone::isDeviceAvailable() const {
  return audioDeviceManager != nullptr && !getAvailableInputDevices().isEmpty();
}

// ==============================================================================
void Microphone::initializeBuffers() {
  if (maxRecordingSamples > 0 && currentNumChannels > 0) {
    recordingBuffer.setSize(currentNumChannels, maxRecordingSamples, false, false, true);
    recordingBuffer.clear();
  }
}

void Microphone::updateLevels(const float *const *inputChannelData, int numChannels, int numSamples) {
  if (inputChannelData == nullptr || numChannels == 0 || numSamples == 0)
    return;

  int channelsToProcess = juce::jmin(numChannels, MaxChannels);

  for (int channel = 0; channel < channelsToProcess; ++channel) {
    if (inputChannelData[channel] == nullptr)
      continue;

    const float *data = inputChannelData[channel];

    // Calculate peak for this buffer
    float bufferPeak = 0.0f;
    float sumSquares = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
      float sample = std::abs(data[i]);
      bufferPeak = juce::jmax(bufferPeak, sample);
      sumSquares += data[i] * data[i];
    }

    // Update peak with decay (peak hold with fast attack, slow decay)
    float currentPeak = peakLevels[channel].load(std::memory_order_relaxed);
    if (bufferPeak > currentPeak) {
      peakLevels[channel].store(bufferPeak, std::memory_order_relaxed);
    } else {
      // Decay: ~300ms to reach 10% at 44.1kHz with 512 sample buffers
      float decay = 0.95f;
      peakLevels[channel].store(currentPeak * decay, std::memory_order_relaxed);
    }

    // Accumulate RMS
    rmsSums[channel] += sumSquares;
    rmsSampleCount += numSamples;

    // Update RMS when we have enough samples
    if (rmsSampleCount >= RMSWindowSamples) {
      float rms = std::sqrt(rmsSums[channel] / static_cast<float>(rmsSampleCount));
      rmsLevels[channel].store(rms, std::memory_order_relaxed);

      // Reset accumulators
      rmsSums[channel] = 0.0f;
      if (channel == channelsToProcess - 1) {
        rmsSampleCount = 0;
      }
    }
  }
}

bool Microphone::initializeAudioDevice() {
  if (!audioDeviceManager)
    return false;

  // Try to initialize with default input device
  juce::String error = audioDeviceManager->initialise(1, 0, nullptr, true, currentInputDeviceName, nullptr);

  if (error.isNotEmpty()) {
    Log::error("Microphone: Failed to initialize audio device: " + error);
    return false;
  }

  // Add ourselves as callback
  audioDeviceManager->addAudioCallback(this);

  // Get the actual device name
  if (auto *device = audioDeviceManager->getCurrentAudioDevice()) {
    currentInputDeviceName = device->getName();
    Log::info("Microphone: Initialized with device: " + currentInputDeviceName);
  }

  return true;
}
