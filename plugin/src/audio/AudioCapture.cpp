#include "AudioCapture.h"
#include "../util/Constants.h"
#include "../util/Log.h"
#include "../util/error/ErrorTracking.h"

//==============================================================================
AudioCapture::AudioCapture() {}

AudioCapture::~AudioCapture() {}

//==============================================================================
void AudioCapture::prepare(double sampleRate, int /* samplesPerBlock */, int numChannels) {
  currentSampleRate = sampleRate;
  currentNumChannels = juce::jmin(numChannels, MaxChannels);

  // Initialize buffers for max recording duration
  maxRecordingSamples = static_cast<int>(sampleRate * Constants::Audio::MAX_RECORDING_SECONDS);

  initializeBuffers();
  resetLevels();

  Log::info("AudioCapture prepared: " + juce::String(sampleRate) + "Hz, " + juce::String(currentNumChannels) +
            " channels, " + juce::String(maxRecordingSamples) + " max samples (" +
            juce::String(maxRecordingSamples / sampleRate) + "s)");
}

void AudioCapture::reset() {
  recording.store(false);
  recordingPosition.store(0);
  hasRecordedData = false;
  recordedAudio.clear();
  resetLevels();
}

//==============================================================================
void AudioCapture::startRecording(const juce::String &recordingId) {
  if (recording.load()) {
    Log::warn("Already recording, ignoring start request");
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

  Log::info("Started audio capture: " + recordingId);
}

juce::AudioBuffer<float> AudioCapture::stopRecording() {
  if (!recording.load()) {
    Log::warn("Not recording, returning empty buffer");
    return juce::AudioBuffer<float>();
  }

  // Stop recording first (audio thread will stop writing)
  recording.store(false);

  // Get the final position
  int finalPosition = recordingPosition.load();

  // Copy recorded data to return buffer
  if (finalPosition > 0) {
    recordedAudio.setSize(currentNumChannels, finalPosition, false, true, false);

    for (int channel = 0; channel < currentNumChannels; ++channel) {
      recordedAudio.copyFrom(channel, 0, recordingBuffer, channel, 0, finalPosition);
    }

    hasRecordedData = true;

    Log::info("Stopped recording: " + juce::String(finalPosition) + " samples, " +
              juce::String(finalPosition / currentSampleRate) + " seconds");
  }

  auto result = recordedAudio;

  // Reset for next recording
  recordingPosition.store(0);
  currentRecordingId = "";

  return result;
}

//==============================================================================
void AudioCapture::captureAudio(const juce::AudioBuffer<float> &buffer) {
  // Fast exit if not recording (atomic read)
  if (!recording.load(std::memory_order_relaxed))
    return;

  int currentPos = recordingPosition.load(std::memory_order_relaxed);

  // Check if we've reached the max
  if (currentPos >= maxRecordingSamples)
    return;

  int numSamples = buffer.getNumSamples();
  int numChannels = juce::jmin(buffer.getNumChannels(), currentNumChannels);

  // Calculate how many samples we can write
  int samplesToWrite = juce::jmin(numSamples, maxRecordingSamples - currentPos);

  if (samplesToWrite > 0) {
    // Copy audio data to recording buffer (lock-free write)
    for (int channel = 0; channel < numChannels; ++channel) {
      recordingBuffer.copyFrom(channel, currentPos, buffer, channel, 0, samplesToWrite);
    }

    // Update position atomically
    recordingPosition.store(currentPos + samplesToWrite, std::memory_order_relaxed);
  }

  // Update level meters (always, even when buffer is full)
  updateLevels(buffer, numSamples);
}

//==============================================================================
void AudioCapture::updateLevels(const juce::AudioBuffer<float> &buffer, int numSamples) {
  int numChannels = juce::jmin(buffer.getNumChannels(), MaxChannels);

  for (int channel = 0; channel < numChannels; ++channel) {
    const float *data = buffer.getReadPointer(channel);

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
      if (channel == numChannels - 1) {
        rmsSampleCount = 0;
      }
    }
  }
}

//==============================================================================
double AudioCapture::getRecordingLengthSeconds() const {
  if (currentSampleRate <= 0)
    return 0.0;

  return recordingPosition.load() / currentSampleRate;
}

double AudioCapture::getMaxRecordingLengthSeconds() const {
  if (currentSampleRate <= 0)
    return 60.0;

  return maxRecordingSamples / currentSampleRate;
}

float AudioCapture::getRecordingProgress() const {
  if (maxRecordingSamples <= 0)
    return 0.0f;

  return static_cast<float>(recordingPosition.load()) / static_cast<float>(maxRecordingSamples);
}

bool AudioCapture::isBufferFull() const {
  return recordingPosition.load() >= maxRecordingSamples;
}

//==============================================================================
float AudioCapture::getPeakLevel(int channel) const {
  if (channel < 0 || channel >= MaxChannels)
    return 0.0f;

  return peakLevels[channel].load(std::memory_order_relaxed);
}

float AudioCapture::getRMSLevel(int channel) const {
  if (channel < 0 || channel >= MaxChannels)
    return 0.0f;

  return rmsLevels[channel].load(std::memory_order_relaxed);
}

void AudioCapture::resetLevels() {
  for (int i = 0; i < MaxChannels; ++i) {
    peakLevels[i].store(0.0f, std::memory_order_relaxed);
    rmsLevels[i].store(0.0f, std::memory_order_relaxed);
  }
}

juce::String AudioCapture::generateWaveformSVG(const juce::AudioBuffer<float> &buffer, int width, int height) {
  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    return "";

  // Generate SVG waveform visualization
  juce::String svg = "<svg width=\"" + juce::String(width) + "\" height=\"" + juce::String(height) +
                     "\" xmlns=\"http://www.w3.org/2000/svg\">";
  svg += "<rect width=\"100%\" height=\"100%\" fill=\"#1a1a1e\"/>";

  // Calculate samples per pixel
  int numSamples = buffer.getNumSamples();
  float samplesPerPixel = static_cast<float>(numSamples) / static_cast<float>(width);

  // Generate path for waveform
  juce::String pathData = "M";

  for (int x = 0; x < width; ++x) {
    int sampleIndex = static_cast<int>(static_cast<float>(x) * samplesPerPixel);

    if (sampleIndex < numSamples) {
      // Get peak amplitude for this pixel (mono or average of channels)
      float peak = 0.0f;
      for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        peak += std::abs(buffer.getSample(channel, sampleIndex));
      }
      peak /= static_cast<float>(buffer.getNumChannels());

      // Convert to SVG coordinates
      int y = static_cast<int>((1.0f - peak) * static_cast<float>(height) * 0.5f);
      y = juce::jlimit(0, height, y);

      if (x == 0)
        pathData += juce::String(x) + "," + juce::String(height / 2);
      else
        pathData += " L" + juce::String(x) + "," + juce::String(y);
    }
  }

  svg += "<path d=\"" + pathData + "\" stroke=\"#00d4ff\" stroke-width=\"1\" fill=\"none\"/>";
  svg += "</svg>";

  return svg;
}

//==============================================================================
void AudioCapture::initializeBuffers() {
  recordingBuffer.setSize(currentNumChannels, maxRecordingSamples, false, true, false);
  recordingBuffer.clear();
  recordingPosition = 0;
}

//==============================================================================
bool AudioCapture::saveBufferToWavFile(const juce::File &file, const juce::AudioBuffer<float> &buffer,
                                       double sampleRate, ExportFormat format) {
  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0) {
    Log::warn("saveBufferToWavFile: Empty buffer, nothing to save");
    return false;
  }

  if (sampleRate <= 0) {
    Log::error("saveBufferToWavFile: Invalid sample rate: " + juce::String(sampleRate));

    // Track audio parameter error (Task 4.19)
    using namespace Sidechain::Util::Error;
    auto errorTracker = ErrorTracker::getInstance();
    errorTracker->recordError(ErrorSource::Audio, "Invalid sample rate for audio export", ErrorSeverity::Error,
                              {{"sample_rate", juce::String(sampleRate)}, {"file", file.getFullPathName()}});

    return false;
  }

  // Delete existing file if present
  if (file.exists()) {
    if (!file.deleteFile()) {
      Log::warn("saveBufferToWavFile: Could not delete existing file: " + file.getFullPathName());
      return false;
    }
  }

  // Create parent directory if needed
  auto parentDir = file.getParentDirectory();
  if (!parentDir.exists()) {
    if (!parentDir.createDirectory()) {
      Log::error("saveBufferToWavFile: Could not create directory: " + parentDir.getFullPathName());
      return false;
    }
  }

  // Create the output stream
  auto fileStream = std::make_unique<juce::FileOutputStream>(file);
  if (!fileStream->openedOk()) {
    Log::error("saveBufferToWavFile: Could not open file for writing: " + file.getFullPathName());

    // Track file I/O error (Task 4.19)
    using namespace Sidechain::Util::Error;
    auto errorTracker = ErrorTracker::getInstance();
    errorTracker->recordError(
        ErrorSource::FileSystem, "Failed to open audio file for writing", ErrorSeverity::Error,
        {{"file", file.getFullPathName()}, {"parent_exists", file.getParentDirectory().exists() ? "true" : "false"}});

    return false;
  }

  // Determine bit depth from format
  int bitsPerSample;
  switch (format) {
  case ExportFormat::WAV_16bit:
    bitsPerSample = 16;
    break;
  case ExportFormat::WAV_24bit:
    bitsPerSample = 24;
    break;
  case ExportFormat::WAV_32bit:
    bitsPerSample = 32;
    break;
  case ExportFormat::FLAC_16bit:
    bitsPerSample = 16;
    break;
  case ExportFormat::FLAC_24bit:
    bitsPerSample = 24;
    break;
  }

  // Create WAV format writer (need unique_ptr<OutputStream> for the API)
  juce::WavAudioFormat wavFormat;
  std::unique_ptr<juce::OutputStream> outputStream(std::move(fileStream));
  std::unique_ptr<juce::AudioFormatWriter> writer(
      wavFormat.createWriterFor(outputStream.get(), sampleRate,
                                static_cast<unsigned int>(buffer.getNumChannels()),
                                bitsPerSample,
                                juce::StringPairArray(), 0));

  if (writer == nullptr) {
    Log::error("saveBufferToWavFile: Could not create WAV writer");

    // Track audio encoding error (Task 4.19)
    using namespace Sidechain::Util::Error;
    auto errorTracker = ErrorTracker::getInstance();
    errorTracker->recordError(ErrorSource::Audio, "Failed to create WAV audio writer",
                              ErrorSeverity::Critical, // Critical because encoding completely failed
                              {{"file", file.getFullPathName()},
                               {"sample_rate", juce::String(sampleRate)},
                               {"channels", juce::String(buffer.getNumChannels())},
                               {"bit_depth", juce::String(bitsPerSample)}});

    return false;
  }

  // Ownership of outputStream is transferred to writer when successful
  outputStream.release();

  // Write the audio buffer
  bool success = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

  if (success) {
    Log::info("saveBufferToWavFile: Successfully saved " + juce::String(buffer.getNumSamples()) + " samples to " +
              file.getFullPathName() + " (" + juce::String(bitsPerSample) + "-bit, " + juce::String(sampleRate) +
              "Hz)");
  } else {
    Log::error("saveBufferToWavFile: Failed to write audio data");

    // Track write failure (Task 4.19)
    using namespace Sidechain::Util::Error;
    auto errorTracker = ErrorTracker::getInstance();
    errorTracker->recordError(ErrorSource::Audio, "Failed to write audio data to WAV file", ErrorSeverity::Error,
                              {{"file", file.getFullPathName()},
                               {"num_samples", juce::String(buffer.getNumSamples())},
                               {"channels", juce::String(buffer.getNumChannels())}});
  }

  return success;
}

bool AudioCapture::saveRecordedAudioToWavFile(const juce::File &file, ExportFormat format) const {
  if (!hasRecordedData || recordedAudio.getNumSamples() == 0) {
    Log::warn("saveRecordedAudioToWavFile: No recorded audio to save");
    return false;
  }

  return saveBufferToWavFile(file, recordedAudio, currentSampleRate, format);
}

//==============================================================================
bool AudioCapture::isFlacFormat(ExportFormat format) {
  return format == ExportFormat::FLAC_16bit || format == ExportFormat::FLAC_24bit;
}

juce::String AudioCapture::getExtensionForFormat(ExportFormat format) {
  switch (format) {
  case ExportFormat::FLAC_16bit:
  case ExportFormat::FLAC_24bit:
    return ".flac";
  case ExportFormat::WAV_16bit:
  case ExportFormat::WAV_24bit:
  case ExportFormat::WAV_32bit:
  default:
    return ".wav";
  }
}

bool AudioCapture::saveBufferToFile(const juce::File &file, const juce::AudioBuffer<float> &buffer, double sampleRate,
                                    ExportFormat format) {
  if (isFlacFormat(format))
    return saveBufferToFlacFile(file, buffer, sampleRate, format);
  else
    return saveBufferToWavFile(file, buffer, sampleRate, format);
}

bool AudioCapture::saveBufferToFlacFile(const juce::File &file, const juce::AudioBuffer<float> &buffer,
                                        double sampleRate, ExportFormat format, int quality) {
  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0) {
    Log::warn("saveBufferToFlacFile: Empty buffer, nothing to save");
    return false;
  }

  if (sampleRate <= 0) {
    Log::error("saveBufferToFlacFile: Invalid sample rate: " + juce::String(sampleRate));
    return false;
  }

  // Delete existing file if present
  if (file.exists()) {
    if (!file.deleteFile()) {
      Log::warn("saveBufferToFlacFile: Could not delete existing file: " + file.getFullPathName());
      return false;
    }
  }

  // Create parent directory if needed
  auto parentDir = file.getParentDirectory();
  if (!parentDir.exists()) {
    if (!parentDir.createDirectory()) {
      Log::error("saveBufferToFlacFile: Could not create directory: " + parentDir.getFullPathName());
      return false;
    }
  }

  // Create the output stream
  auto fileStream = std::make_unique<juce::FileOutputStream>(file);
  if (!fileStream->openedOk()) {
    Log::error("saveBufferToFlacFile: Could not open file for writing: " + file.getFullPathName());
    return false;
  }

  // Determine bit depth from format (FLAC supports 8, 16, 24)
  int bitsPerSample;
  switch (format) {
  case ExportFormat::FLAC_24bit:
    bitsPerSample = 24;
    break;
  case ExportFormat::FLAC_16bit:
    bitsPerSample = 16;
    break;
  case ExportFormat::WAV_16bit:
  case ExportFormat::WAV_24bit:
  case ExportFormat::WAV_32bit:
    // WAV formats shouldn't reach FLAC save, but handle gracefully
    bitsPerSample = 16;
    break;
  }

  // Clamp quality to valid range (0-8)
  quality = juce::jlimit(0, 8, quality);

  // Create FLAC format writer (need unique_ptr<OutputStream> for the API)
  juce::FlacAudioFormat flacFormat;
  std::unique_ptr<juce::OutputStream> outputStream(std::move(fileStream));
  std::unique_ptr<juce::AudioFormatWriter> writer(
      flacFormat.createWriterFor(outputStream.get(), sampleRate,
                                 static_cast<unsigned int>(buffer.getNumChannels()),
                                 bitsPerSample,
                                 juce::StringPairArray(), quality));

  if (writer == nullptr) {
    Log::error("saveBufferToFlacFile: Could not create FLAC writer");
    return false;
  }

  // Ownership of outputStream is transferred to writer when successful
  outputStream.release();

  // Write the audio buffer
  bool success = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

  if (success) {
    Log::info("saveBufferToFlacFile: Successfully saved " + juce::String(buffer.getNumSamples()) + " samples to " +
              file.getFullPathName() + " (" + juce::String(bitsPerSample) + "-bit, " + juce::String(sampleRate) +
              "Hz, quality=" + juce::String(quality) + ")");
  } else {
    Log::error("saveBufferToFlacFile: Failed to write audio data");
  }

  return success;
}

bool AudioCapture::saveRecordedAudioToFile(const juce::File &file, ExportFormat format) const {
  if (!hasRecordedData || recordedAudio.getNumSamples() == 0) {
    Log::warn("saveRecordedAudioToFile: No recorded audio to save");
    return false;
  }

  return saveBufferToFile(file, recordedAudio, currentSampleRate, format);
}

juce::File AudioCapture::getTempAudioFile(const juce::String &extension) {
  auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
  auto sidechainDir = tempDir.getChildFile("Sidechain");

  // Create Sidechain temp directory if it doesn't exist
  if (!sidechainDir.exists()) {
    sidechainDir.createDirectory();
  }

  // Generate unique filename with timestamp and random suffix
  auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
  auto randomSuffix = juce::String::toHexString(juce::Random::getSystemRandom().nextInt());

  juce::String filename = "sidechain_" + timestamp + "_" + randomSuffix + extension;

  return sidechainDir.getChildFile(filename);
}

//==============================================================================
juce::String AudioCapture::formatDuration(double seconds) {
  if (seconds < 0.0)
    seconds = 0.0;

  int totalSeconds = static_cast<int>(std::floor(seconds));
  int minutes = totalSeconds / 60;
  int secs = totalSeconds % 60;

  // Format as M:SS (single digit minutes) or MM:SS
  return juce::String(minutes) + ":" + juce::String(secs).paddedLeft('0', 2);
}

juce::String AudioCapture::formatDurationWithMs(double seconds) {
  if (seconds < 0.0)
    seconds = 0.0;

  int totalSeconds = static_cast<int>(std::floor(seconds));
  int minutes = totalSeconds / 60;
  int secs = totalSeconds % 60;
  int ms = static_cast<int>((seconds - totalSeconds) * 1000);

  return juce::String(minutes) + ":" + juce::String(secs).paddedLeft('0', 2) + "." +
         juce::String(ms).paddedLeft('0', 3);
}

juce::String AudioCapture::formatFileSize(juce::int64 bytes) {
  if (bytes < 0)
    bytes = 0;

  const double kb = 1024.0;
  const double mb = kb * 1024.0;
  const double gb = mb * 1024.0;

  if (static_cast<double>(bytes) >= gb)
    return juce::String(static_cast<double>(bytes) / gb, 2) + " GB";
  else if (static_cast<double>(bytes) >= mb)
    return juce::String(static_cast<double>(bytes) / mb, 2) + " MB";
  else if (static_cast<double>(bytes) >= kb)
    return juce::String(static_cast<double>(bytes) / kb, 1) + " KB";
  else
    return juce::String(bytes) + " bytes";
}

juce::int64 AudioCapture::estimateFileSize(int numSamples, int numChannels, ExportFormat format) {
  if (numSamples <= 0 || numChannels <= 0)
    return 0;

  // Calculate bytes per sample based on bit depth
  int bytesPerSample;
  double compressionRatio = 1.0;

  switch (format) {
  case ExportFormat::WAV_16bit:
    bytesPerSample = 2;
    break;
  case ExportFormat::WAV_24bit:
    bytesPerSample = 3;
    break;
  case ExportFormat::WAV_32bit:
    bytesPerSample = 4;
    break;
  case ExportFormat::FLAC_16bit:
    bytesPerSample = 2;
    compressionRatio = 0.55; // Typical FLAC compression ~55% of WAV
    break;
  case ExportFormat::FLAC_24bit:
    bytesPerSample = 3;
    compressionRatio = 0.60; // 24-bit FLAC compresses slightly less
    break;
  default:
    bytesPerSample = 2;
    break;
  }

  // Calculate raw audio data size
  juce::int64 rawSize = static_cast<juce::int64>(numSamples) * numChannels * bytesPerSample;

  // Apply compression ratio for FLAC
  juce::int64 dataSize = static_cast<juce::int64>(static_cast<double>(rawSize) * compressionRatio);

  // Add header overhead (WAV: 44 bytes, FLAC: ~8KB typical metadata)
  juce::int64 headerSize = isFlacFormat(format) ? 8192 : 44;

  return dataSize + headerSize;
}

juce::int64 AudioCapture::getEstimatedFileSize(ExportFormat format) const {
  if (!hasRecordedData || recordedAudio.getNumSamples() == 0)
    return 0;

  return estimateFileSize(recordedAudio.getNumSamples(), recordedAudio.getNumChannels(), format);
}

//==============================================================================
// Audio Processing Utilities
//==============================================================================

juce::AudioBuffer<float> AudioCapture::trimBuffer(const juce::AudioBuffer<float> &buffer, int startSample,
                                                  int endSample) {
  int numSamples = buffer.getNumSamples();
  int numChannels = buffer.getNumChannels();

  // Handle default end value
  if (endSample < 0)
    endSample = numSamples;

  // Validate range
  if (startSample < 0)
    startSample = 0;
  if (endSample > numSamples)
    endSample = numSamples;
  if (startSample >= endSample || numChannels == 0)
    return juce::AudioBuffer<float>();

  int trimmedLength = endSample - startSample;
  juce::AudioBuffer<float> result(numChannels, trimmedLength);

  for (int channel = 0; channel < numChannels; ++channel) {
    result.copyFrom(channel, 0, buffer, channel, startSample, trimmedLength);
  }

  return result;
}

juce::AudioBuffer<float> AudioCapture::trimBufferByTime(const juce::AudioBuffer<float> &buffer, double sampleRate,
                                                        double startSeconds, double endSeconds) {
  if (sampleRate <= 0)
    return juce::AudioBuffer<float>();

  int startSample = static_cast<int>(startSeconds * sampleRate);
  int endSample = (endSeconds < 0) ? -1 : static_cast<int>(endSeconds * sampleRate);

  return trimBuffer(buffer, startSample, endSample);
}

//==============================================================================
// Helper function to calculate fade gain based on type
static float calculateFadeGain(float position, AudioCapture::FadeType fadeType, bool isFadeIn) {
  // position is 0.0 to 1.0 representing progress through the fade
  float gain;

  switch (fadeType) {
  case AudioCapture::FadeType::Linear:
    gain = position;
    break;

  case AudioCapture::FadeType::Exponential:
    // Attempt to replicate a natural audio fade with exponential curve
    // For fade in: starts slow, ends fast
    // For fade out: starts fast, ends slow
    if (isFadeIn)
      gain = position * position; // Quadratic (simple exponential approximation)
    else
      gain = 1.0f - (1.0f - position) * (1.0f - position);
    break;

  case AudioCapture::FadeType::SCurve:
    // Smooth S-curve using sine interpolation
    // Starts slow, speeds up in middle, slows at end
    gain = 0.5f * (1.0f - std::cos(position * juce::MathConstants<float>::pi));
    break;

  default:
    gain = position;
    break;
  }

  return isFadeIn ? gain : (1.0f - gain);
}

void AudioCapture::applyFadeIn(juce::AudioBuffer<float> &buffer, int fadeSamples, FadeType fadeType) {
  if (fadeSamples <= 0 || buffer.getNumSamples() == 0)
    return;

  // Clamp fade length to buffer length
  fadeSamples = juce::jmin(fadeSamples, buffer.getNumSamples());

  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    float *data = buffer.getWritePointer(channel);

    for (int i = 0; i < fadeSamples; ++i) {
      float position = static_cast<float>(i) / static_cast<float>(fadeSamples);
      float gain = calculateFadeGain(position, fadeType, true);
      data[i] *= gain;
    }
  }
}

void AudioCapture::applyFadeOut(juce::AudioBuffer<float> &buffer, int fadeSamples, FadeType fadeType) {
  if (fadeSamples <= 0 || buffer.getNumSamples() == 0)
    return;

  int numSamples = buffer.getNumSamples();

  // Clamp fade length to buffer length
  fadeSamples = juce::jmin(fadeSamples, numSamples);

  int fadeStart = numSamples - fadeSamples;

  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    float *data = buffer.getWritePointer(channel);

    for (int i = 0; i < fadeSamples; ++i) {
      float position = static_cast<float>(i) / static_cast<float>(fadeSamples);
      float gain = calculateFadeGain(position, fadeType, false);
      data[fadeStart + i] *= gain;
    }
  }
}

void AudioCapture::applyFadeInByTime(juce::AudioBuffer<float> &buffer, double sampleRate, double fadeSeconds,
                                     FadeType fadeType) {
  if (sampleRate <= 0 || fadeSeconds <= 0)
    return;

  int fadeSamples = static_cast<int>(fadeSeconds * sampleRate);
  applyFadeIn(buffer, fadeSamples, fadeType);
}

void AudioCapture::applyFadeOutByTime(juce::AudioBuffer<float> &buffer, double sampleRate, double fadeSeconds,
                                      FadeType fadeType) {
  if (sampleRate <= 0 || fadeSeconds <= 0)
    return;

  int fadeSamples = static_cast<int>(fadeSeconds * sampleRate);
  applyFadeOut(buffer, fadeSamples, fadeType);
}

//==============================================================================
float AudioCapture::getBufferPeakLevel(const juce::AudioBuffer<float> &buffer) {
  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    return 0.0f;

  float peak = 0.0f;

  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    const float *data = buffer.getReadPointer(channel);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      float absSample = std::abs(data[i]);
      if (absSample > peak)
        peak = absSample;
    }
  }

  return peak;
}

float AudioCapture::getBufferPeakLevelDB(const juce::AudioBuffer<float> &buffer) {
  float peak = getBufferPeakLevel(buffer);
  return linearToDb(peak);
}

float AudioCapture::dbToLinear(float db) {
  return std::pow(10.0f, db / 20.0f);
}

float AudioCapture::linearToDb(float linear) {
  if (linear <= 0.0f)
    return -std::numeric_limits<float>::infinity();

  return 20.0f * std::log10(linear);
}

float AudioCapture::normalizeBuffer(juce::AudioBuffer<float> &buffer, float targetPeakDB) {
  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    return 1.0f;

  float currentPeak = getBufferPeakLevel(buffer);

  // Avoid division by zero for silent buffers
  if (currentPeak < 1e-10f)
    return 1.0f;

  float targetPeakLinear = dbToLinear(targetPeakDB);
  float gain = targetPeakLinear / currentPeak;

  // Apply gain to all channels
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    float *data = buffer.getWritePointer(channel);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      data[i] *= gain;
    }
  }

  Log::debug("normalizeBuffer: peak " + juce::String(linearToDb(currentPeak), 1) + " dB -> " +
             juce::String(targetPeakDB, 1) + " dB (gain: " + juce::String(linearToDb(gain), 1) + " dB)");

  return gain;
}