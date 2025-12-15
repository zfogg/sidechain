#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

//==============================================================================
/**
 * BufferAudioPlayer - Handles audio playback from in-memory AudioBuffer
 *
 * This class is designed for playing audio directly from
 * juce::AudioBuffer<float> without needing to encode/decode or save to files.
 * Perfect for previewing recorded audio before upload.
 *
 * Key features:
 * - Direct buffer-based playback (no file I/O)
 * - Transport controls (play, pause, seek)
 * - Playback progress tracking
 * - Volume control
 * - Automatic resampling to match DAW sample rate
 */
class BufferAudioPlayer {
public:
  BufferAudioPlayer();
  ~BufferAudioPlayer();

  //==============================================================================
  // Loading

  /**
   * Load audio buffer for playback
   * @param buffer The audio buffer to play (copied internally)
   * @param sampleRate The sample rate of the buffer
   */
  void loadBuffer(const juce::AudioBuffer<float> &buffer, double sampleRate);

  /**
   * Clear the loaded buffer
   */
  void clear();

  //==============================================================================
  // Transport Controls

  /** Play the loaded audio */
  void play();

  /** Pause playback */
  void pause();

  /** Stop playback and reset position */
  void stop();

  /** Toggle between play and pause */
  void togglePlayPause();

  /**
   * Seek to a position
   * @param positionSeconds Position in seconds
   */
  void seekToPosition(double positionSeconds);

  /**
   * Seek to a normalized position (0.0 to 1.0)
   * @param normalizedPosition Position from 0 to 1
   */
  void seekToNormalizedPosition(double normalizedPosition);

  //==============================================================================
  // State Queries

  bool isPlaying() const {
    return playing;
  }
  bool hasBuffer() const;

  /** Get current playback position in seconds */
  double getPositionSeconds() const;

  /** Get total duration in seconds */
  double getDurationSeconds() const;

  /** Get playback progress (0.0 to 1.0) */
  double getPlaybackProgress() const;

  //==============================================================================
  // Volume Control

  /** Set volume (0.0 to 1.0) */
  void setVolume(float newVolume);

  /** Get current volume */
  float getVolume() const {
    return volume;
  }

  /** Mute/unmute */
  void setMuted(bool shouldMute);
  bool isMuted() const {
    return muted;
  }

  //==============================================================================
  // Audio Processing (called from PluginProcessor::processBlock)

  /**
   * Process and mix playback audio into the output buffer
   * Should be called from the audio thread only
   * @param buffer The audio buffer to mix into
   * @param numSamples Number of samples to process
   */
  void processBlock(juce::AudioBuffer<float> &buffer, int numSamples);

  /**
   * Prepare for playback
   * Should be called from PluginProcessor::prepareToPlay
   * @param sampleRate The DAW's sample rate
   * @param blockSize Maximum block size
   */
  void prepareToPlay(double sampleRate, int blockSize);

  /**
   * Release resources
   * Should be called from PluginProcessor::releaseResources
   */
  void releaseResources();

  //==============================================================================
  // Callbacks

  /** Called when playback starts */
  std::function<void()> onPlaybackStarted;

  /** Called when playback pauses */
  std::function<void()> onPlaybackPaused;

  /** Called when playback stops (end of file) */
  std::function<void()> onPlaybackStopped;

  /** Called periodically with playback progress */
  std::function<void(double progress)> onProgressUpdate;

  /** Called when playback finishes (reached end of audio) */
  std::function<void()> onPlaybackFinished;

private:
  //==============================================================================
  // Audio data
  juce::AudioBuffer<float> audioBuffer;
  double bufferSampleRate = 44100.0;
  int numChannels = 0;
  int totalSamples = 0;

  // Playback state
  std::atomic<bool> playing{false};
  std::atomic<bool> muted{false};
  std::atomic<float> volume{0.8f};

  // Position tracking (in samples at buffer sample rate)
  std::atomic<juce::int64> currentSamplePosition{0};

  // Resampling state
  double outputSampleRate = 44100.0;
  double resamplingRatio = 1.0;
  juce::LagrangeInterpolator interpolator;

  // Thread safety
  juce::CriticalSection bufferLock;

  // Progress timer
  class ProgressTimer : public juce::Timer {
  public:
    ProgressTimer(BufferAudioPlayer &p) : player(p) {}
    void timerCallback() override;

  private:
    BufferAudioPlayer &player;
  };

  std::unique_ptr<ProgressTimer> progressTimer;

  // Helper methods
  double samplePositionToSeconds(juce::int64 samplePos) const;
  juce::int64 secondsToSamplePosition(double seconds) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BufferAudioPlayer)
};
