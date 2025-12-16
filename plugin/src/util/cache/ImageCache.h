#pragma once

#include "FileCache.h"
#include <JuceHeader.h>
#include <optional>

//==============================================================================
/**
 * SidechainImageCache caches downloaded images with automatic LRU eviction.
 *
 * Features:
 * - 500MB default limit (configurable)
 * - Automatic format detection (PNG, JPG, etc.)
 * - Thread-safe load/cache operations
 * - LRU eviction when limit exceeded
 *
 * Note: Class named SidechainImageCache to avoid conflict with juce::ImageCache
 */
class SidechainImageCache {
public:
  explicit SidechainImageCache(int64_t maxSizeBytes = 500 * 1024 * 1024); // 500MB default
  ~SidechainImageCache() = default;

  /**
   * Get cached image for URL. Returns std::nullopt if not cached.
   * Updates last access time on hit.
   */
  std::optional<juce::Image> getImage(const juce::String &url);

  /**
   * Store image in cache. Writes to disk and caches the image object.
   */
  void cacheImage(const juce::String &url, const juce::Image &image);

  /**
   * Remove image from cache by URL.
   */
  void removeImage(const juce::String &url) {
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
  std::map<juce::String, juce::Image> memoryCache;
  juce::ReadWriteLock memoryCacheLock;

  // Load image from disk file
  std::optional<juce::Image> loadImageFromFile(const juce::File &file);
};
