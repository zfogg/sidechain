#include "HttpAudioPlayer.h"
#include "../network/NetworkClient.h"
#include "../util/Async.h"
#include "../util/Constants.h"
#include "../util/Log.h"
#include <memory>
#include <nlohmann/json.hpp>

// ==============================================================================
HttpAudioPlayer::HttpAudioPlayer() {
  // Register common audio formats (WAV, AIFF, FLAC, Ogg)
  formatManager.registerBasicFormats();

  // Note: JUCE doesn't support MP3 decoding out of the box
  // All audio URLs must use WAV, AIFF, FLAC, or Ogg format

  // Log registered formats
  juce::String formats;
  for (int i = 0; i < formatManager.getNumKnownFormats(); i++) {
    if (i > 0)
      formats += ", ";
    formats += formatManager.getKnownFormat(i)->getFormatName();
  }
  Log::info("HttpAudioPlayer: Registered audio formats: " + formats);

  // Create progress timer
  progressTimer = std::make_unique<HttpAudioPlayer::ProgressTimer>(*this);

  Log::info("HttpAudioPlayer: Initialized");
}

HttpAudioPlayer::~HttpAudioPlayer() {
  Log::debug("HttpAudioPlayer: Destroying");

  // CRITICAL: Mark as destroyed to prevent use-after-free in pending async callbacks
  aliveFlag->store(false, std::memory_order_release);

  // CRITICAL: Stop timer BEFORE destroying anything
  if (progressTimer)
    progressTimer->stopTimer();

  // Stop playback
  stop();

  // Clear the timer to ensure callback doesn't fire during destruction
  progressTimer.reset();
}

// ==============================================================================
// Transport Controls

void HttpAudioPlayer::loadAndPlay(const juce::String &postId, const juce::String &audioUrl) {
  // If same post is already playing, just toggle play/pause
  if (postId == currentPostId && readerSource != nullptr) {
    Log::debug("HttpAudioPlayer: Toggling play/pause for post: " + postId);
    if (playing)
      pause();
    else
      play();
    return;
  }

  Log::info("HttpAudioPlayer: Loading and playing post: " + postId);

  // Stop current playback
  stop();

  currentPostId = postId;
  currentAudioUrl = audioUrl;

  // Check if we have this audio cached
  if (auto *cachedData = getFromCache(postId)) {
    Log::debug("HttpAudioPlayer: Using cached audio for post: " + postId);
    loadFromMemory(postId, *cachedData);
    play();
    return;
  }

  // Download the audio
  loading = true;
  Log::info("HttpAudioPlayer: Downloading audio for post: " + postId);
  if (onLoadingStarted)
    onLoadingStarted(postId);

  downloadAudio(postId, audioUrl);
}

void HttpAudioPlayer::play() {
  if (readerSource == nullptr) {
    Log::warn("HttpAudioPlayer: Cannot play - no audio source loaded");
    return;
  }

  playing = true;
  Log::info("HttpAudioPlayer: Playback started - post: " + currentPostId);

  // Start progress timer
  progressTimer->startTimer(50); // Update every 50ms

  if (onPlaybackStarted)
    onPlaybackStarted(currentPostId);
}

void HttpAudioPlayer::pause() {
  playing = false;
  progressTimer->stopTimer();
  Log::debug("HttpAudioPlayer: Playback paused - post: " + currentPostId);

  if (onPlaybackPaused)
    onPlaybackPaused(currentPostId);
}

void HttpAudioPlayer::stop() {
  if (playing || !currentPostId.isEmpty()) {
    Log::info("HttpAudioPlayer: Playback stopped - post: " + currentPostId);
  }

  playing = false;
  progressTimer->stopTimer();

  {
    const juce::ScopedLock sl(audioLock);

    if (resamplingSource)
      resamplingSource->releaseResources();

    resamplingSource.reset();
    readerSource.reset();
  }

  if (!currentPostId.isEmpty() && onPlaybackStopped)
    onPlaybackStopped(currentPostId);

  currentPostId = "";
  currentAudioUrl = "";
}

void HttpAudioPlayer::togglePlayPause() {
  if (playing)
    pause();
  else
    play();
}

void HttpAudioPlayer::seekToPosition(double positionSeconds) {
  const juce::ScopedLock sl(audioLock);

  if (readerSource == nullptr)
    return;

  auto *reader = readerSource->getAudioFormatReader();
  if (reader == nullptr)
    return;

  juce::int64 samplePosition = static_cast<juce::int64>(positionSeconds * reader->sampleRate);
  samplePosition = juce::jlimit<juce::int64>(0, reader->lengthInSamples, samplePosition);

  readerSource->setNextReadPosition(samplePosition);
}

void HttpAudioPlayer::seekToNormalizedPosition(double normalizedPosition) {
  double duration = getDurationSeconds();
  if (duration > 0)
    seekToPosition(normalizedPosition * duration);
}

// ==============================================================================
// State Queries

double HttpAudioPlayer::getPositionSeconds() const {
  const juce::ScopedLock sl(audioLock);

  if (readerSource == nullptr)
    return 0.0;

  auto *reader = readerSource->getAudioFormatReader();
  if (reader == nullptr || reader->sampleRate <= 0)
    return 0.0;

  return static_cast<double>(readerSource->getNextReadPosition()) / reader->sampleRate;
}

double HttpAudioPlayer::getDurationSeconds() const {
  const juce::ScopedLock sl(audioLock);

  if (readerSource == nullptr)
    return 0.0;

  auto *reader = readerSource->getAudioFormatReader();
  if (reader == nullptr || reader->sampleRate <= 0)
    return 0.0;

  return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
}

double HttpAudioPlayer::getPlaybackProgress() const {
  double duration = getDurationSeconds();
  if (duration <= 0)
    return 0.0;

  return getPositionSeconds() / duration;
}

bool HttpAudioPlayer::isPostPlaying(const juce::String &postId) const {
  return playing && currentPostId == postId;
}

// ==============================================================================
// Volume Control

void HttpAudioPlayer::setVolume(float newVolume) {
  volume = juce::jlimit(0.0f, 1.0f, newVolume);
}

void HttpAudioPlayer::setMuted(bool shouldMute) {
  muted = shouldMute;
}

// ==============================================================================
// Audio Processing

void HttpAudioPlayer::processBlock(juce::AudioBuffer<float> &buffer, int numSamples) {
  static int logCounter = 0;
  if (playing && ++logCounter % 1000 == 0) // Log every 1000 blocks (~20 seconds at 512 samples/44.1kHz)
  {
    Log::debug("HttpAudioPlayer: processBlock called - playing: true, numSamples: " + juce::String(numSamples));
  }

  if (!playing || muted)
    return;

  const juce::ScopedLock sl(audioLock);

  if (resamplingSource == nullptr) {
    Log::warn("HttpAudioPlayer: processBlock - resamplingSource is null!");
    return;
  }

  // Re-prepare if block size changed (rare but can happen)
  if (numSamples != currentBlockSize) {
    currentBlockSize = numSamples;
    resamplingSource->prepareToPlay(numSamples, currentSampleRate);
    Log::debug("HttpAudioPlayer: Re-prepared for block size: " + juce::String(numSamples));
  }

  // Create temp buffer for playback audio
  juce::AudioSourceChannelInfo info;
  juce::AudioBuffer<float> tempBuffer(buffer.getNumChannels(), numSamples);
  tempBuffer.clear();
  info.buffer = &tempBuffer;
  info.startSample = 0;
  info.numSamples = numSamples;

  // Get audio from the source
  resamplingSource->getNextAudioBlock(info);

  // Check if we got any audio data
  float maxSample = 0.0f;
  for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel) {
    maxSample = juce::jmax(maxSample, tempBuffer.getMagnitude(channel, 0, numSamples));
  }

  static int audioLogCounter = 0;
  if (++audioLogCounter % 100 == 0) // Log every 100 blocks
  {
    Log::debug("HttpAudioPlayer: Audio block - maxSample: " + juce::String(maxSample, 4) +
               ", volume: " + juce::String(volume.load(), 2));
  }

  // Apply volume and mix into output buffer
  float vol = volume.load();
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    buffer.addFrom(channel, 0, tempBuffer, channel % tempBuffer.getNumChannels(), 0, numSamples, vol);
  }

  // Check if playback has ended
  if (readerSource != nullptr) {
    auto *reader = readerSource->getAudioFormatReader();
    if (reader != nullptr && readerSource->getNextReadPosition() >= reader->lengthInSamples) {
      // Schedule end-of-playback handling on message thread
      // Capture aliveFlag by value (shared_ptr) to check if object still exists
      auto flagCopy = aliveFlag;
      juce::MessageManager::callAsync([this, flagCopy]() {
        // Check if object was destroyed before this callback ran
        if (!flagCopy->load(std::memory_order_acquire)) {
          return;
        }

        juce::String finishedPostId = currentPostId;
        Log::info("HttpAudioPlayer: Playback finished - post: " + finishedPostId);

        // Notify that playback finished
        if (onPlaybackFinished)
          onPlaybackFinished(finishedPostId);

        // Handle auto-play
        if (autoPlayEnabled) {
          int nextIndex = getCurrentPlaylistIndex() + 1;
          const juce::ScopedLock playlistSl(playlistLock);
          if (nextIndex >= 0 && nextIndex < playlistPostIds.size()) {
            // Play next post
            juce::String nextPostId = playlistPostIds[nextIndex];
            juce::String nextUrl = playlistAudioUrls[nextIndex];

            Log::debug("HttpAudioPlayer: Auto-playing next post: " + nextPostId);

            if (onAutoPlayNext)
              onAutoPlayNext(nextPostId);

            loadAndPlay(nextPostId, nextUrl);

            // Pre-buffer the post after the next one for seamless playback
            if (nextIndex + 1 < playlistPostIds.size()) {
              preloadAudio(playlistPostIds[nextIndex + 1], playlistAudioUrls[nextIndex + 1]);
            }
            return;
          } else {
            Log::debug("HttpAudioPlayer: End of playlist reached");
          }
        }

        // No auto-play or end of playlist - just stop
        stop();
      });
    }
  }
}

void HttpAudioPlayer::prepareToPlay(double sampleRate, int blockSize) {
  currentSampleRate = sampleRate;
  currentBlockSize = blockSize;
  Log::info("HttpAudioPlayer: Prepared - " + juce::String(sampleRate) + "Hz, block size: " + juce::String(blockSize));

  const juce::ScopedLock sl(audioLock);

  if (resamplingSource)
    resamplingSource->prepareToPlay(blockSize, sampleRate);
}

void HttpAudioPlayer::releaseResources() {
  const juce::ScopedLock sl(audioLock);

  if (resamplingSource)
    resamplingSource->releaseResources();
}

// ==============================================================================
// Cache Management

void HttpAudioPlayer::clearCache() {
  const juce::ScopedLock sl(cacheLock);
  size_t oldSize = currentCacheSize;
  audioCache.clear();
  currentCacheSize = 0;
  Log::info("HttpAudioPlayer: Cache cleared - freed " + juce::String((int)oldSize) + " bytes");
}

void HttpAudioPlayer::setMaxCacheSize(size_t bytes) {
  maxCacheSize = bytes;
  evictCacheIfNeeded(0);
}

size_t HttpAudioPlayer::getCurrentCacheSize() const {
  return currentCacheSize;
}

void HttpAudioPlayer::preloadAudio(const juce::String &postId, const juce::String &audioUrl) {
  // Don't preload if already cached
  if (getFromCache(postId) != nullptr)
    return;

  // Download in background
  Async::runVoid([this, postId, audioUrl]() {
    juce::MemoryBlock data;
    bool success = false;

    // Use NetworkClient if available, otherwise fall back to JUCE URL
    if (networkClient != nullptr) {
      auto result = networkClient->makeAbsoluteRequestSync(audioUrl, "GET", nlohmann::json(), false,
                                                           juce::StringPairArray(), &data);
      success = result.success && data.getSize() > 0;
    } else {
      // Fallback to JUCE URL
      juce::URL url(audioUrl);
      auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                         .withConnectionTimeoutMs(Constants::Api::IMAGE_TIMEOUT_MS)
                         .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS);

      if (auto stream = url.createInputStream(options)) {
        stream->readIntoMemoryBlock(data);
        success = data.getSize() > 0;
      }
    }

    if (success) {
      auto dataPtr = std::make_unique<juce::MemoryBlock>(data);
      addToCache(postId, std::move(dataPtr));
    }
  });
}

void HttpAudioPlayer::evictCacheIfNeeded(size_t bytesNeeded) {
  const juce::ScopedLock sl(cacheLock);

  while (currentCacheSize + bytesNeeded > maxCacheSize && !audioCache.empty()) {
    // Find oldest entry (LRU eviction)
    juce::String oldestKey;
    juce::int64 oldestTime = std::numeric_limits<juce::int64>::max();

    for (const auto &[key, cached] : audioCache) {
      if (cached.lastAccessTime < oldestTime && key != currentPostId) {
        oldestTime = cached.lastAccessTime;
        oldestKey = key;
      }
    }

    if (oldestKey.isEmpty())
      break;

    currentCacheSize -= audioCache[oldestKey].sizeBytes;
    audioCache.erase(oldestKey);
  }
}

void HttpAudioPlayer::addToCache(const juce::String &postId, std::unique_ptr<juce::MemoryBlock> data) {
  const juce::ScopedLock sl(cacheLock);

  size_t dataSize = data->getSize();
  evictCacheIfNeeded(dataSize);

  CachedAudio cached;
  cached.audioData = std::move(data);
  cached.sizeBytes = dataSize;
  cached.lastAccessTime = juce::Time::currentTimeMillis();

  audioCache[postId] = std::move(cached);
  currentCacheSize += dataSize;
}

juce::MemoryBlock *HttpAudioPlayer::getFromCache(const juce::String &postId) {
  const juce::ScopedLock sl(cacheLock);

  auto it = audioCache.find(postId);
  if (it != audioCache.end()) {
    it->second.lastAccessTime = juce::Time::currentTimeMillis();
    return it->second.audioData.get();
  }
  return nullptr;
}

// ==============================================================================
// Loading

void HttpAudioPlayer::downloadAudio(const juce::String &postId, const juce::String &url) {
  Log::debug("HttpAudioPlayer: Starting download - post: " + postId + ", url: " + url);

  Async::runVoid([this, postId, url]() {
    Log::debug("HttpAudioPlayer: Download thread started - post: " + postId);

    bool success = false;
    auto data = std::make_unique<juce::MemoryBlock>();

    // Use NetworkClient if available - it handles HTTPS properly on Linux
    if (networkClient != nullptr) {
      Log::debug("HttpAudioPlayer: Using NetworkClient for download");
      auto result = networkClient->makeAbsoluteRequestSync(url, "GET", nlohmann::json(), false, juce::StringPairArray(),
                                                           data.get());
      success = result.success && data->getSize() > 0;

      Log::debug("HttpAudioPlayer: NetworkClient returned - success: " +
                 juce::String(result.success ? "true" : "false") + ", status: " + juce::String(result.httpStatus) +
                 ", dataSize: " + juce::String((int)data->getSize()) + ", errorMsg: " + result.errorMessage);

      if (!success) {
        Log::warn("HttpAudioPlayer: NetworkClient download failed - status: " + juce::String(result.httpStatus) +
                  ", error: " + result.errorMessage + ", dataSize: " + juce::String((int)data->getSize()));
      }
    } else {
      // Fallback to JUCE URL (may fail on Linux with HTTPS)
      Log::debug("HttpAudioPlayer: Using JUCE URL for download (no NetworkClient)");
      juce::URL audioUrl(url);
      auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                         .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS)
                         .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS);

      if (auto stream = audioUrl.createInputStream(options)) {
        stream->readIntoMemoryBlock(*data);
        success = data->getSize() > 0;
      }
    }

    // Back to message thread
    // Capture aliveFlag by value (shared_ptr) to check if object still exists
    auto flagCopy = aliveFlag;
    juce::MessageManager::callAsync([this, flagCopy, postId, audioData = std::move(data), success]() mutable {
      // Check if object was destroyed before this callback ran
      if (!flagCopy->load(std::memory_order_acquire)) {
        return;
      }

      Log::debug("HttpAudioPlayer: MessageManager callback - post: " + postId +
                 ", success: " + juce::String(success ? "true" : "false"));

      loading = false;

      if (success && postId == currentPostId) {
        Log::info("HttpAudioPlayer: Download successful - post: " + postId +
                  ", size: " + juce::String((int)audioData->getSize()) + " bytes");

        // Add to cache
        addToCache(postId, std::make_unique<juce::MemoryBlock>(*audioData));

        // Load and play
        loadFromMemory(postId, *audioData);
        play();
      } else if (!success) {
        Log::error("HttpAudioPlayer: Download failed - post: " + postId);
      } else {
        Log::warn("HttpAudioPlayer: Download completed but post changed - post: " + postId +
                  ", current: " + currentPostId);
      }

      if (onLoadingComplete)
        onLoadingComplete(postId, success);
    });
  });
}

void HttpAudioPlayer::loadFromMemory(const juce::String &postId, juce::MemoryBlock &audioData) {
  const juce::ScopedLock sl(audioLock);

  Log::debug("HttpAudioPlayer: loadFromMemory - post: " + postId + ", size: " + juce::String((int)audioData.getSize()) +
             " bytes");

  // Create memory input stream (copy data so it stays valid after audioData is
  // freed)
  auto *memStream = new juce::MemoryInputStream(audioData, true);

  // Create audio format reader
  auto *reader = formatManager.createReaderFor(std::unique_ptr<juce::InputStream>(memStream));
  if (reader == nullptr) {
    // Log first few bytes to diagnose (safely, as hex)
    juce::String hexDump;
    if (audioData.getSize() > 0) {
      for (size_t i = 0; i < juce::jmin((size_t)16, audioData.getSize()); i++) {
        hexDump +=
            juce::String::toHexString((int)static_cast<const uint8_t *>(audioData.getData())[i]).paddedLeft('0', 2) +
            " ";
      }
    }

    // Check if it looks like MP3 (starts with FF FB or FF FA - MPEG sync word)
    bool looksLikeMP3 = false;
    if (audioData.getSize() >= 2) {
      const uint8_t *data = static_cast<const uint8_t *>(audioData.getData());
      looksLikeMP3 = (data[0] == 0xFF && (data[1] & 0xE0) == 0xE0);
    }

    if (looksLikeMP3) {
      Log::error("HttpAudioPlayer: Cannot play MP3 file (JUCE doesn't support "
                 "MP3 decoding) - post: " +
                 postId);
      Log::error("HttpAudioPlayer: Please use WAV, FLAC, AIFF, or Ogg format instead");
    } else {
      Log::error("HttpAudioPlayer: Failed to create reader for audio data - post: " + postId +
                 ", size: " + juce::String((int)audioData.getSize()) + " bytes");
      Log::error("HttpAudioPlayer: First 16 bytes (hex): " + hexDump);
    }
    return;
  }

  // Create reader source
  readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

  // Create resampling source to match DAW sample rate
  resamplingSource = std::make_unique<juce::ResamplingAudioSource>(readerSource.get(), false, 2);
  resamplingSource->setResamplingRatio(reader->sampleRate / currentSampleRate);
  resamplingSource->prepareToPlay(currentBlockSize, currentSampleRate);

  double duration = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
  Log::info("HttpAudioPlayer: Loaded audio from memory - post: " + postId + ", duration: " + juce::String(duration, 2) +
            "s, " + "sample rate: " + juce::String(reader->sampleRate) + "Hz, " +
            "channels: " + juce::String(reader->numChannels));
}

// ==============================================================================
// Playlist and Auto-play

void HttpAudioPlayer::setPlaylist(const juce::StringArray &postIds, const juce::StringArray &audioUrls) {
  const juce::ScopedLock sl(playlistLock);
  playlistPostIds = postIds;
  playlistAudioUrls = audioUrls;

  // Pre-buffer next post if we're currently playing
  if (playing && !currentPostId.isEmpty()) {
    int currentIndex = getCurrentPlaylistIndex();
    if (currentIndex >= 0 && currentIndex + 1 < playlistPostIds.size()) {
      preloadAudio(playlistPostIds[currentIndex + 1], playlistAudioUrls[currentIndex + 1]);
    }
  }
}

int HttpAudioPlayer::getCurrentPlaylistIndex() const {
  const juce::ScopedLock sl(playlistLock);
  return playlistPostIds.indexOf(currentPostId);
}

void HttpAudioPlayer::playNext() {
  int currentIndex = getCurrentPlaylistIndex();
  const juce::ScopedLock sl(playlistLock);

  if (currentIndex >= 0 && currentIndex + 1 < playlistPostIds.size()) {
    int nextIndex = currentIndex + 1;
    loadAndPlay(playlistPostIds[nextIndex], playlistAudioUrls[nextIndex]);

    // Pre-buffer the post after the next one for seamless playback
    if (nextIndex + 1 < playlistPostIds.size()) {
      preloadAudio(playlistPostIds[nextIndex + 1], playlistAudioUrls[nextIndex + 1]);
    }
  }
}

void HttpAudioPlayer::playPrevious() {
  // If we're more than 3 seconds in, restart current track
  if (getPositionSeconds() > 3.0) {
    seekToPosition(0.0);
    return;
  }

  int currentIndex = getCurrentPlaylistIndex();
  const juce::ScopedLock sl(playlistLock);

  if (currentIndex > 0) {
    loadAndPlay(playlistPostIds[currentIndex - 1], playlistAudioUrls[currentIndex - 1]);
  } else {
    // At start of playlist, just restart
    seekToPosition(0.0);
  }
}

// ==============================================================================
// Audio Focus (DAW awareness)

void HttpAudioPlayer::onDAWTransportStarted() {
  if (!audioFocusEnabled)
    return;

  if (playing) {
    wasPlayingBeforeDAW = true;
    pausedByDAW = true;
    pause();
    Log::info("HttpAudioPlayer: Paused due to DAW transport start");
  }
}

void HttpAudioPlayer::onDAWTransportStopped() {
  if (!audioFocusEnabled)
    return;

  if (pausedByDAW && wasPlayingBeforeDAW) {
    pausedByDAW = false;
    wasPlayingBeforeDAW = false;
    play();
    Log::info("HttpAudioPlayer: Resumed after DAW transport stop");
  }
}

// ==============================================================================
// ChangeListener

void HttpAudioPlayer::changeListenerCallback(juce::ChangeBroadcaster * /*source*/) {
  // Not currently used, but available for future extensions
}

// ==============================================================================
// Progress Timer

void HttpAudioPlayer::ProgressTimer::timerCallback() {
  if (player.playing && player.onProgressUpdate) {
    player.onProgressUpdate(player.currentPostId, player.getPlaybackProgress());
  }
}
