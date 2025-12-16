#pragma once

#include "FileCache.h"
#include <JuceHeader.h>
#include <optional>

//==============================================================================
/**
 * SidechainAudioCache caches downloaded audio files with automatic LRU eviction.
 *
 * Features:
 * - 5GB default limit (configurable)
 * - Support for MP3, WAV, FLAC, AAC, etc.
 * - Thread-safe file access
 * - LRU eviction when limit exceeded
 *
 * Note: Returns file paths only, not loaded audio data.
 * AudioBuffer loading is expensive; components should load as needed.
 */
class SidechainAudioCache {
public:
  explicit SidechainAudioCache(int64_t maxSizeBytes = 5LL * 1024 * 1024 * 1024); // 5GB default
  ~SidechainAudioCache() = default;

  /**
   * Get cached audio file for URL. Returns std::nullopt if not cached.
   * Updates last access time on hit.
   */
  std::optional<juce::File> getAudioFile(const juce::String &url);

  /**
   * Store audio file in cache. Returns the cached file path.
   */
  juce::File cacheAudioFile(const juce::String &url, const juce::File &sourceFile);

  /**
   * Remove audio file from cache by URL.
   */
  void removeAudioFile(const juce::String &url) {
    fileCache.removeFile(url);
  }

  /**
   * Clear entire cache.
   */
  void clear() {
    fileCache.clear();
  }

  /**
   * Get cache directory.
   */
  juce::File getCacheDirectory() const {
    return fileCache.getCacheDirectory();
  }

  /**
   * Force manifest to disk.
   */
  void flush() {
    fileCache.flush();
  }

private:
  FileCache<juce::String> fileCache;
};
