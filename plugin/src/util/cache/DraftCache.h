#pragma once

#include "FileCache.h"
#include <JuceHeader.h>
#include <optional>

//==============================================================================
/**
 * DraftKey - Simple key type for draft caching
 *
 * Holds the draft ID used to cache draft metadata
 */
struct DraftKey {
  juce::String id;

  explicit DraftKey(const juce::String &draftId) : id(draftId) {}
  explicit DraftKey(const juce::var &draft) : id(draft.getProperty("id", "").toString()) {}
};

//==============================================================================
/**
 * Trait specialization for DraftKey
 *
 * Extracts the cache key from a DraftKey object
 */
template <> struct CacheKeyTraits<DraftKey> {
  static juce::String getKey(const DraftKey &draft) {
    return draft.id;
  }
};

//==============================================================================
/**
 * SidechainDraftCache caches draft metadata files with automatic LRU eviction.
 *
 * Features:
 * - 100MB default limit (configurable)
 * - Stores draft metadata and associated files
 * - Thread-safe file access
 * - LRU eviction when limit exceeded
 *
 * Usage:
 *   DraftKey key("draft-123");
 *   auto cachedFile = draftCache.getDraftFile(key);
 */
class SidechainDraftCache {
public:
  explicit SidechainDraftCache(int64_t maxSizeBytes = 100 * 1024 * 1024); // 100MB default
  ~SidechainDraftCache() = default;

  /**
   * Get cached draft file. Returns std::nullopt if not cached.
   * Updates last access time on hit.
   */
  std::optional<juce::File> getDraftFile(const DraftKey &key);

  /**
   * Store draft file in cache. Returns the cached file path.
   */
  juce::File cacheDraftFile(const DraftKey &key, const juce::File &sourceFile);

  /**
   * Remove draft file from cache.
   */
  void removeDraftFile(const DraftKey &key) {
    fileCache.removeFile(key);
  }

  /**
   * Clear entire draft cache.
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
  FileCache<DraftKey> fileCache;
};
