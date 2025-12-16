#include "BufferAudioPlayer.h"
#include "../util/Log.h"
#include <algorithm>

//==============================================================================
BufferAudioPlayer::BufferAudioPlayer() {
  // Create progress timer
  progressTimer = std::make_unique<BufferAudioPlayer::ProgressTimer>(*this);

  Log::info("BufferAudioPlayer: Initialized");
}

BufferAudioPlayer::~BufferAudioPlayer() {
  Log::debug("BufferAudioPlayer: Destroying");

  // CRITICAL: Stop timer BEFORE destroying anything
  if (progressTimer)
    progressTimer->stopTimer();

  // Stop playback
  stop();

  // Clear the timer to ensure callback doesn't fire during destruction
  progressTimer.reset();
}

//==============================================================================
// Loading

void BufferAudioPlayer::loadBuffer(const juce::AudioBuffer<float> &buffer, double sampleRate) {
  const juce::ScopedLock sl(bufferLock);

  // Copy the buffer
  audioBuffer.makeCopyOf(buffer);
  bufferSampleRate = sampleRate;
  numChannels = audioBuffer.getNumChannels();
  totalSamples = audioBuffer.getNumSamples();

  // Reset position
  currentSamplePosition = 0;

  // Update resampling ratio
  if (outputSampleRate > 0 && bufferSampleRate > 0) {
    resamplingRatio = bufferSampleRate / outputSampleRate;
  }

  Log::info("BufferAudioPlayer: Loaded buffer - " + juce::String(totalSamples) + " samples, " +
            juce::String(bufferSampleRate) + "Hz, " + juce::String(numChannels) + " channels, " +
            "duration: " + juce::String(getDurationSeconds(), 2) + "s");
}

void BufferAudioPlayer::clear() {
  stop();

  const juce::ScopedLock sl(bufferLock);
  audioBuffer.setSize(0, 0);
  totalSamples = 0;
  numChannels = 0;
  currentSamplePosition = 0;
}

bool BufferAudioPlayer::hasBuffer() const {
  const juce::ScopedLock sl(bufferLock);
  return totalSamples > 0 && numChannels > 0;
}

//==============================================================================
// Transport Controls

void BufferAudioPlayer::play() {
  if (!hasBuffer()) {
    Log::warn("BufferAudioPlayer: Cannot play - no buffer loaded");
    return;
  }

  playing = true;
  Log::info("BufferAudioPlayer: Playback started");

  // Start progress timer
  progressTimer->startTimer(50); // Update every 50ms

  if (onPlaybackStarted)
    onPlaybackStarted();
}

void BufferAudioPlayer::pause() {
  playing = false;
  progressTimer->stopTimer();
  Log::debug("BufferAudioPlayer: Playback paused");

  if (onPlaybackPaused)
    onPlaybackPaused();
}

void BufferAudioPlayer::stop() {
  if (playing) {
    Log::info("BufferAudioPlayer: Playback stopped");
  }

  playing = false;
  progressTimer->stopTimer();
  currentSamplePosition = 0;

  if (onPlaybackStopped)
    onPlaybackStopped();
}

void BufferAudioPlayer::togglePlayPause() {
  if (playing)
    pause();
  else
    play();
}

void BufferAudioPlayer::seekToPosition(double positionSeconds) {
  const juce::ScopedLock sl(bufferLock);

  if (totalSamples == 0 || bufferSampleRate <= 0)
    return;

  juce::int64 targetSample = secondsToSamplePosition(positionSeconds);
  targetSample = juce::jlimit<juce::int64>(0, totalSamples - 1, targetSample);

  currentSamplePosition = targetSample;

  Log::debug("BufferAudioPlayer: Seeked to " + juce::String(positionSeconds, 2) + "s (sample " +
             juce::String((int)targetSample) + ")");
}

void BufferAudioPlayer::seekToNormalizedPosition(double normalizedPosition) {
  double duration = getDurationSeconds();
  if (duration > 0)
    seekToPosition(normalizedPosition * duration);
}

//==============================================================================
// State Queries

double BufferAudioPlayer::getPositionSeconds() const {
  juce::int64 pos = currentSamplePosition.load();
  return samplePositionToSeconds(pos);
}

double BufferAudioPlayer::getDurationSeconds() const {
  const juce::ScopedLock sl(bufferLock);

  if (totalSamples == 0 || bufferSampleRate <= 0)
    return 0.0;

  return static_cast<double>(totalSamples) / bufferSampleRate;
}

double BufferAudioPlayer::getPlaybackProgress() const {
  double duration = getDurationSeconds();
  if (duration <= 0)
    return 0.0;

  return getPositionSeconds() / duration;
}

//==============================================================================
// Volume Control

void BufferAudioPlayer::setVolume(float newVolume) {
  volume = juce::jlimit(0.0f, 1.0f, newVolume);
}

void BufferAudioPlayer::setMuted(bool shouldMute) {
  muted = shouldMute;
}

//==============================================================================
// Audio Processing

void BufferAudioPlayer::processBlock(juce::AudioBuffer<float> &buffer, int numSamples) {
  if (!playing || muted)
    return;

  const juce::ScopedLock sl(bufferLock);

  if (totalSamples == 0 || numChannels == 0 || bufferSampleRate <= 0)
    return;

  float vol = volume.load();
  juce::int64 currentPos = currentSamplePosition.load();

  // Process each output channel
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    float *outputChannel = buffer.getWritePointer(channel);
    int inputChannel = channel % numChannels;
    const float *inputChannelData = audioBuffer.getReadPointer(inputChannel);

    // Reset interpolator if needed
    if (currentPos == 0) {
      interpolator.reset();
    }

    // Render samples with resampling
    double readPos = static_cast<double>(currentPos);
    for (int sample = 0; sample < numSamples; ++sample) {
      // Calculate read position (accounting for resampling ratio)
      double readPosExact = readPos;

      // Clamp to valid range
      if (readPosExact >= totalSamples - 1) {
        readPosExact = totalSamples - 1;
      }

      // Interpolate sample
      float sampleValue = 0.0f;
      if (readPosExact >= 0 && readPosExact < totalSamples) {
        int readIndex = static_cast<int>(readPosExact);
        float frac = static_cast<float>(readPosExact - readIndex);

        if (readIndex < totalSamples - 1) {
          // Linear interpolation
          sampleValue = inputChannelData[readIndex] * (1.0f - frac) + inputChannelData[readIndex + 1] * frac;
        } else {
          sampleValue = inputChannelData[readIndex];
        }
      }

      outputChannel[sample] += sampleValue * vol;
      readPos += resamplingRatio;
    }
  }

  // Update position
  juce::int64 newPos =
      static_cast<juce::int64>(static_cast<double>(currentPos) + resamplingRatio * static_cast<double>(numSamples));
  newPos = juce::jlimit<juce::int64>(0, totalSamples, newPos);
  currentSamplePosition = newPos;

  // Check if playback has ended
  if (newPos >= totalSamples) {
    // Schedule end-of-playback handling on message thread
    juce::MessageManager::callAsync([this]() {
      Log::info("BufferAudioPlayer: Playback finished");

      if (onPlaybackFinished)
        onPlaybackFinished();

      stop();
    });
  }
}

void BufferAudioPlayer::prepareToPlay(double sampleRate, int blockSize) {
  outputSampleRate = sampleRate;

  // Update resampling ratio
  if (outputSampleRate > 0 && bufferSampleRate > 0) {
    resamplingRatio = bufferSampleRate / outputSampleRate;
  }

  // Reset interpolator
  interpolator.reset();

  Log::info("BufferAudioPlayer: Prepared - " + juce::String(sampleRate) + "Hz, block size: " + juce::String(blockSize) +
            ", resampling ratio: " + juce::String(resamplingRatio, 4));
}

void BufferAudioPlayer::releaseResources() {
  interpolator.reset();
}

//==============================================================================
// Helper Methods

double BufferAudioPlayer::samplePositionToSeconds(juce::int64 samplePos) const {
  const juce::ScopedLock sl(bufferLock);

  if (bufferSampleRate <= 0)
    return 0.0;

  return static_cast<double>(samplePos) / bufferSampleRate;
}

juce::int64 BufferAudioPlayer::secondsToSamplePosition(double seconds) const {
  const juce::ScopedLock sl(bufferLock);

  if (bufferSampleRate <= 0)
    return 0;

  return static_cast<juce::int64>(seconds * bufferSampleRate);
}

//==============================================================================
// Progress Timer

void BufferAudioPlayer::ProgressTimer::timerCallback() {
  if (player.playing && player.onProgressUpdate) {
    player.onProgressUpdate(player.getPlaybackProgress());
  }
}
