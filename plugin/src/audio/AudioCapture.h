#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * AudioCapture handles recording audio from the DAW
 *
 * Thread Safety:
 * - captureAudio() is called from the AUDIO THREAD (processBlock)
 * - All other methods are called from the MESSAGE THREAD
 * - Uses std::atomic for thread-safe state sharing
 *
 * Features:
 * - Lock-free audio capture from processBlock
 * - Up to 60 seconds of recording
 * - Real-time level metering (peak + RMS)
 * - Waveform generation for UI
 */
class AudioCapture {
public:
  AudioCapture();
  ~AudioCapture();

  //==============================================================================
  // Configuration - call from prepareToPlay() or message thread
  void prepare(double sampleRate, int samplesPerBlock, int numChannels);
  void reset();

  //==============================================================================
  // Recording control - call from MESSAGE THREAD only
  void startRecording(const juce::String &recordingId = "");
  juce::AudioBuffer<float> stopRecording();
  bool isRecording() const {
    return recording.load();
  }

  //==============================================================================
  // Audio capture - call from AUDIO THREAD (processBlock) only

  /**
   * Capture audio from the DAW's processBlock callback.
   *
   * @warning CRITICAL: This function MUST be called from the AUDIO THREAD only.
   *          Calling from any other thread will cause audio dropouts and
   * crashes.
   *
   * @warning MUST be lock-free and allocation-free. This function uses atomic
   *          operations and lock-free circular buffer writes to ensure
   * real-time audio performance. Any blocking operations or memory allocations
   *          will cause audio glitches.
   *
   * @param buffer Audio buffer from processBlock (must be valid during call)
   *
   * @note This function is designed to be called every audio block during
   *       recording. It writes to a lock-free circular buffer that can hold
   *       up to 60 seconds of audio at the configured sample rate.
   *
   * @note If the recording buffer is full, new audio will overwrite the oldest
   *       data (circular buffer behavior). Check isBufferFull() from the
   * message thread to detect this condition.
   *
   * @see startRecording() To begin recording (call from message thread)
   * @see stopRecording() To end recording and get the captured audio (call from
   * message thread)
   * @see PluginProcessor::processBlock() For typical usage in audio callback
   */
  void captureAudio(const juce::AudioBuffer<float> &buffer);

  //==============================================================================
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

  //==============================================================================
  // Level metering - thread-safe, updated during captureAudio()
  float getPeakLevel(int channel) const;
  float getRMSLevel(int channel) const;
  void resetLevels();

  //==============================================================================
  // Export utilities - call from MESSAGE THREAD
  juce::String generateWaveformSVG(const juce::AudioBuffer<float> &buffer, int width = 400, int height = 100);

  // Get sample rate for export
  double getSampleRate() const {
    return currentSampleRate;
  }
  int getNumChannels() const {
    return currentNumChannels;
  }

  //==============================================================================
  // Audio file export - call from MESSAGE THREAD

  /** Export format options for audio files */
  enum class ExportFormat {
    WAV_16bit,  ///< CD quality, smaller files
    WAV_24bit,  ///< Professional quality, larger files
    WAV_32bit,  ///< Maximum quality (float), largest files
    FLAC_16bit, ///< Lossless compression, ~50-60% of WAV size
    FLAC_24bit  ///< High-quality lossless, for professional use
  };

  /** Save an audio buffer to a file (WAV or FLAC based on format)
   *  @param file The destination file
   *  @param buffer The audio buffer to save
   *  @param sampleRate The sample rate of the audio
   *  @param format The export format (default: 16-bit WAV)
   *  @return true if successful
   */
  static bool saveBufferToFile(const juce::File &file, const juce::AudioBuffer<float> &buffer, double sampleRate,
                               ExportFormat format = ExportFormat::WAV_16bit);

  /** Save an audio buffer to a WAV file
   *  @param file The destination file
   *  @param buffer The audio buffer to save
   *  @param sampleRate The sample rate of the audio
   *  @param format The bit depth format (default: 16-bit)
   *  @return true if successful
   */
  static bool saveBufferToWavFile(const juce::File &file, const juce::AudioBuffer<float> &buffer, double sampleRate,
                                  ExportFormat format = ExportFormat::WAV_16bit);

  /** Save an audio buffer to a FLAC file
   *  @param file The destination file
   *  @param buffer The audio buffer to save
   *  @param sampleRate The sample rate of the audio
   *  @param format The bit depth format (default: 16-bit FLAC)
   *  @param quality Compression quality 0-8 (higher = smaller but slower,
   * default: 5)
   *  @return true if successful
   */
  static bool saveBufferToFlacFile(const juce::File &file, const juce::AudioBuffer<float> &buffer, double sampleRate,
                                   ExportFormat format = ExportFormat::FLAC_16bit, int quality = 5);

  /** Save the last recorded audio to a file
   *  @param file The destination file
   *  @param format The export format (default: 16-bit WAV)
   *  @return true if successful
   */
  bool saveRecordedAudioToFile(const juce::File &file, ExportFormat format = ExportFormat::WAV_16bit) const;

  // Legacy method - kept for backward compatibility
  bool saveRecordedAudioToWavFile(const juce::File &file, ExportFormat format = ExportFormat::WAV_16bit) const;

  /** Check if a format is a FLAC format */
  static bool isFlacFormat(ExportFormat format);

  /** Get the recommended file extension for a format */
  static juce::String getExtensionForFormat(ExportFormat format);

  /** Get a temporary file for audio export
   *  Creates a unique file in the system temp directory
   *  @param extension File extension including dot (e.g., ".wav")
   *  @return A unique temporary file path
   */
  static juce::File getTempAudioFile(const juce::String &extension = ".wav");

  /** Check if there is recorded audio available to export */
  bool hasRecordedAudio() const {
    return hasRecordedData && recordedAudio.getNumSamples() > 0;
  }

  /** Get the recorded audio buffer (for inspection/preview) */
  const juce::AudioBuffer<float> &getRecordedAudioBuffer() const {
    return recordedAudio;
  }

  //==============================================================================
  // Duration and size utilities

  /** Format a duration in seconds as MM:SS or M:SS string
   *  @param seconds Duration in seconds
   *  @return Formatted string like "1:23" or "0:05"
   */
  static juce::String formatDuration(double seconds);

  /** Format a duration in seconds with milliseconds as MM:SS.mmm or M:SS.mmm
   *  @param seconds Duration in seconds
   *  @return Formatted string like "1:23.456"
   */
  static juce::String formatDurationWithMs(double seconds);

  /** Format a file size in human-readable form
   *  @param bytes File size in bytes
   *  @return Formatted string like "1.2 MB" or "456 KB"
   */
  static juce::String formatFileSize(juce::int64 bytes);

  /** Estimate file size for a given audio buffer and format
   *  @param numSamples Number of samples
   *  @param numChannels Number of channels
   *  @param format Export format
   *  @return Estimated file size in bytes
   */
  static juce::int64 estimateFileSize(int numSamples, int numChannels, ExportFormat format);

  /** Get estimated file size for the recorded audio in a given format */
  juce::int64 getEstimatedFileSize(ExportFormat format) const;

  //==============================================================================
  // Audio processing utilities - all return NEW buffers (non-destructive)

  /** Trim an audio buffer to a specified range
   *  @param buffer The source audio buffer
   *  @param startSample Start sample index (inclusive)
   *  @param endSample End sample index (exclusive), -1 means end of buffer
   *  @return New trimmed buffer, or empty buffer if range is invalid
   */
  static juce::AudioBuffer<float> trimBuffer(const juce::AudioBuffer<float> &buffer, int startSample,
                                             int endSample = -1);

  /** Trim by time in seconds (convenience wrapper)
   *  @param buffer The source audio buffer
   *  @param sampleRate Sample rate for time conversion
   *  @param startSeconds Start time in seconds
   *  @param endSeconds End time in seconds, -1.0 means end of buffer
   *  @return New trimmed buffer
   */
  static juce::AudioBuffer<float> trimBufferByTime(const juce::AudioBuffer<float> &buffer, double sampleRate,
                                                   double startSeconds, double endSeconds = -1.0);

  /** Fade type for fade in/out operations */
  enum class FadeType {
    Linear,      ///< Linear ramp (constant rate)
    Exponential, ///< Exponential curve (more natural for audio)
    SCurve       ///< S-curve (smooth start and end)
  };

  /** Apply fade in to the beginning of a buffer
   *  @param buffer The buffer to modify (IN-PLACE modification)
   *  @param fadeSamples Number of samples for the fade
   *  @param fadeType Type of fade curve
   */
  static void applyFadeIn(juce::AudioBuffer<float> &buffer, int fadeSamples, FadeType fadeType = FadeType::Linear);

  /** Apply fade out to the end of a buffer
   *  @param buffer The buffer to modify (IN-PLACE modification)
   *  @param fadeSamples Number of samples for the fade
   *  @param fadeType Type of fade curve
   */
  static void applyFadeOut(juce::AudioBuffer<float> &buffer, int fadeSamples, FadeType fadeType = FadeType::Linear);

  /** Apply fade in by time in seconds
   *  @param buffer The buffer to modify
   *  @param sampleRate Sample rate for time conversion
   *  @param fadeSeconds Fade duration in seconds
   *  @param fadeType Type of fade curve
   */
  static void applyFadeInByTime(juce::AudioBuffer<float> &buffer, double sampleRate, double fadeSeconds,
                                FadeType fadeType = FadeType::Linear);

  /** Apply fade out by time in seconds
   *  @param buffer The buffer to modify
   *  @param sampleRate Sample rate for time conversion
   *  @param fadeSeconds Fade duration in seconds
   *  @param fadeType Type of fade curve
   */
  static void applyFadeOutByTime(juce::AudioBuffer<float> &buffer, double sampleRate, double fadeSeconds,
                                 FadeType fadeType = FadeType::Linear);

  /** Get the peak level of a buffer (0.0 to 1.0+)
   *  @param buffer The audio buffer to analyze
   *  @return Peak absolute sample value across all channels
   */
  static float getBufferPeakLevel(const juce::AudioBuffer<float> &buffer);

  /** Get the peak level in decibels
   *  @param buffer The audio buffer to analyze
   *  @return Peak level in dB (0 dB = 1.0 linear)
   */
  static float getBufferPeakLevelDB(const juce::AudioBuffer<float> &buffer);

  /** Normalize a buffer to a target peak level
   *  @param buffer The buffer to modify (IN-PLACE modification)
   *  @param targetPeakDB Target peak level in dB (default: -1.0 dB)
   *  @return The gain applied (for display purposes)
   */
  static float normalizeBuffer(juce::AudioBuffer<float> &buffer, float targetPeakDB = -1.0f);

  /** Convert decibels to linear gain */
  static float dbToLinear(float db);

  /** Convert linear gain to decibels */
  static float linearToDb(float linear);

private:
  //==============================================================================
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

  //==============================================================================
  // Configuration (set on message thread before recording)
  juce::String currentRecordingId;
  double currentSampleRate = 44100.0;
  int currentNumChannels = 2;
  int maxRecordingSamples = 0; // 60 seconds max

  //==============================================================================
  // Recording buffer (allocated on message thread, written on audio thread)
  juce::AudioBuffer<float> recordingBuffer;

  // Recorded data (message thread only)
  juce::AudioBuffer<float> recordedAudio;
  bool hasRecordedData = false;

  //==============================================================================
  void initializeBuffers();
  void updateLevels(const juce::AudioBuffer<float> &buffer, int numSamples);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioCapture)
};