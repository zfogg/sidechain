#pragma once

#include <JuceHeader.h>
#include <optional>
#include <map>

//==============================================================================
/**
 * FileCache provides cross-platform file caching with LRU eviction.
 *
 * Features:
 * - Cross-platform cache directory (~/Library/Application Support/Sidechain/cache/{subdir}/)
 * - Automatic LRU eviction when size limits exceeded
 * - Manifest tracking file metadata (URL, size, access time)
 * - Thread-safe operations
 * - Configurable size limits
 */
class FileCache {
public:
  /**
   * Initialize cache for a specific subdirectory.
   * @param subdirectory Name of cache subdirectory (e.g., "images", "audio")
   * @param maxSizeBytes Maximum cache size before LRU eviction
   */
  FileCache(const juce::String &subdirectory, int64_t maxSizeBytes);
  ~FileCache() = default;

  //==============================================================================
  // Public API

  /**
   * Get cached file for URL. Returns std::nullopt if not cached.
   * Updates last access time on hit.
   */
  std::optional<juce::File> getFile(const juce::String &url);

  /**
   * Store file in cache. Returns the cached file path.
   * Overwrites if file already exists for this URL.
   */
  juce::File cacheFile(const juce::String &url, const juce::File &sourceFile);

  /**
   * Remove file from cache by URL.
   */
  void removeFile(const juce::String &url);

  /**
   * Clear entire cache directory.
   */
  void clear();

  /**
   * Get current cache directory.
   */
  juce::File getCacheDirectory() const {
    return cacheDir;
  }

  /**
   * Get current cache size in bytes.
   */
  int64_t getCacheSizeBytes() const;

  /**
   * Get maximum cache size in bytes.
   */
  int64_t getMaxSizeBye() const {
    return maxSize;
  }

  /**
   * Force manifest to disk.
   */
  void flush();

private:
  //==============================================================================
  // Manifest structure
  struct CacheEntry {
    juce::String url;
    juce::String filename;
    int64_t fileSize = 0;
    double lastAccessTime = 0.0; // juce::Time::currentTimeInSeconds()

    juce::var toJSON() const;
    static CacheEntry fromJSON(const juce::var &json);
  };

  //==============================================================================
  juce::File cacheDir;
  int64_t maxSize;
  std::map<juce::String, CacheEntry> manifest;
  juce::ReadWriteLock manifestLock;

  // Manifest file path
  juce::File getManifestFile() const;

  // Load manifest from disk
  void loadManifest();

  // Save manifest to disk
  void saveManifest();

  // Evict files using LRU strategy until target size is reached
  void evictLRU(int64_t targetSizeBytes);

  // Generate unique filename for URL
  juce::String generateCacheFilename(const juce::String &url);

  // Hash URL to create filename
  static juce::String hashString(const juce::String &str);
};
