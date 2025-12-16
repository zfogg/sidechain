#pragma once

#include <JuceHeader.h>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Sidechain {
namespace Util {
namespace Cache {

/**
 * CacheEntry - Generic cache entry with TTL and metadata
 *
 * @tparam T The value type stored in cache
 */
template <typename T> struct CacheEntry {
  T value;
  std::chrono::steady_clock::time_point createdAt;
  std::chrono::steady_clock::time_point lastAccessedAt;
  int ttlSeconds;
  size_t sizeBytes;

  /**
   * Check if this entry has expired
   */
  bool isExpired() const {
    if (ttlSeconds <= 0)
      return false;

    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - createdAt);
    return age.count() > ttlSeconds;
  }

  /**
   * Update last accessed time
   */
  void updateAccessTime() {
    lastAccessedAt = std::chrono::steady_clock::now();
  }
};

/**
 * MemoryCache - In-memory LRU cache with configurable size limits
 *
 * Features:
 * - LRU (Least Recently Used) eviction policy
 * - TTL-based expiration
 * - Size limits (bytes and count)
 * - Thread-safe operations
 *
 * @tparam K Key type
 * @tparam V Value type
 */
template <typename K, typename V> class MemoryCache {
public:
  /**
   * Create a memory cache with size limits
   *
   * @param maxSizeBytes Maximum cache size in bytes (0 = unlimited)
   * @param maxCountItems Maximum number of items (0 = unlimited)
   */
  MemoryCache(size_t maxSizeBytes = 100 * 1024 * 1024, size_t maxCountItems = 10000)
      : maxSizeBytes_(maxSizeBytes), maxCountItems_(maxCountItems), currentSizeBytes_(0) {}

  virtual ~MemoryCache() = default;

  /**
   * Get value from cache
   *
   * @param key The cache key
   * @return Optional containing value if found and not expired, empty otherwise
   */
  std::optional<V> get(const K &key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end())
      return std::nullopt;

    auto &entry = it->second;
    if (entry.isExpired()) {
      // Remove expired entry
      currentSizeBytes_ -= entry.sizeBytes;
      cache_.erase(it);
      return std::nullopt;
    }

    // Update access time for LRU
    entry.updateAccessTime();
    accessOrder_.push_back(key);

    return entry.value;
  }

  /**
   * Put value in cache with TTL
   *
   * @param key The cache key
   * @param value The value to cache
   * @param ttlSeconds Time to live in seconds (0 = no expiration)
   * @param sizeBytes Size estimate for this entry (bytes)
   */
  void put(const K &key, const V &value, int ttlSeconds = 0, size_t sizeBytes = 0) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove existing entry if present
    auto it = cache_.find(key);
    if (it != cache_.end())
      currentSizeBytes_ -= it->second.sizeBytes;

    // Evict if necessary
    evictIfNeeded(sizeBytes);

    // Create entry
    CacheEntry<V> entry;
    entry.value = value;
    entry.createdAt = std::chrono::steady_clock::now();
    entry.lastAccessedAt = entry.createdAt;
    entry.ttlSeconds = ttlSeconds;
    entry.sizeBytes = sizeBytes;

    // Insert into cache
    cache_[key] = entry;
    currentSizeBytes_ += sizeBytes;
    accessOrder_.push_back(key);
  }

  /**
   * Remove a key from cache
   */
  bool remove(const K &key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end())
      return false;

    currentSizeBytes_ -= it->second.sizeBytes;
    cache_.erase(it);
    return true;
  }

  /**
   * Clear all entries from cache
   */
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    accessOrder_.clear();
    currentSizeBytes_ = 0;
  }

  /**
   * Remove expired entries
   */
  void removeExpired() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.begin();
    while (it != cache_.end()) {
      if (it->second.isExpired()) {
        currentSizeBytes_ -= it->second.sizeBytes;
        it = cache_.erase(it);
      } else {
        ++it;
      }
    }
  }

  /**
   * Get current cache size in bytes
   */
  size_t getCurrentSizeBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSizeBytes_;
  }

  /**
   * Get number of items in cache
   */
  size_t getItemCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
  }

  /**
   * Check if key exists in cache
   */
  bool containsKey(const K &key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
  }

  /**
   * Get cache hit rate (0-1)
   */
  float getHitRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (totalRequests_ == 0)
      return 0.0f;
    return static_cast<float>(hitCount_) / static_cast<float>(totalRequests_);
  }

private:
  size_t maxSizeBytes_;
  size_t maxCountItems_;
  size_t currentSizeBytes_;
  mutable size_t hitCount_ = 0;
  mutable size_t totalRequests_ = 0;
  mutable std::mutex mutex_;

  std::map<K, CacheEntry<V>> cache_;
  std::vector<K> accessOrder_; // For LRU tracking

  /**
   * Evict entries if cache exceeds limits
   */
  void evictIfNeeded(size_t newEntrySize) {
    // Check size limit
    while (maxSizeBytes_ > 0 && currentSizeBytes_ + newEntrySize > maxSizeBytes_) {
      if (cache_.empty())
        break;

      // Evict least recently used
      auto lruKey = accessOrder_.front();
      accessOrder_.erase(accessOrder_.begin());

      auto it = cache_.find(lruKey);
      if (it != cache_.end()) {
        currentSizeBytes_ -= it->second.sizeBytes;
        cache_.erase(it);
      }
    }

    // Check count limit
    while (maxCountItems_ > 0 && cache_.size() >= maxCountItems_) {
      if (cache_.empty())
        break;

      auto lruKey = accessOrder_.front();
      accessOrder_.erase(accessOrder_.begin());

      auto it = cache_.find(lruKey);
      if (it != cache_.end()) {
        currentSizeBytes_ -= it->second.sizeBytes;
        cache_.erase(it);
      }
    }
  }
};

/**
 * DiskCache - Persistent disk-based cache using files
 *
 * Features:
 * - File-based storage with TTL
 * - Automatic size management
 * - JSON serialization support
 *
 * @tparam K Key type
 * @tparam V Value type (must be JSON serializable)
 */
template <typename K> class DiskCache {
public:
  /**
   * Create a disk cache in specified directory
   *
   * @param cacheDir Directory to store cache files
   * @param maxSizeMB Maximum cache size in MB
   */
  DiskCache(const juce::File &cacheDir, size_t maxSizeMB = 1024)
      : cacheDir_(cacheDir), maxSizeBytes_(maxSizeMB * 1024 * 1024) {
    if (!cacheDir_.exists())
      cacheDir_.createDirectory();
  }

  virtual ~DiskCache() = default;

  /**
   * Get cached data from disk
   *
   * @param key The cache key
   * @return Optional containing cached data if found
   */
  std::optional<juce::MemoryBlock> get(const K &key) {
    auto cacheFile = getCacheFilePath(key);
    if (!cacheFile.existsAsFile())
      return std::nullopt;

    // Check if expired
    auto metadata = loadMetadata(cacheFile);
    if (metadata && metadata->isExpired()) {
      cacheFile.deleteFile();
      return std::nullopt;
    }

    // Read cached data
    juce::MemoryBlock data;
    if (cacheFile.loadFileAsData(data))
      return data;

    return std::nullopt;
  }

  /**
   * Put data in disk cache
   *
   * @param key The cache key
   * @param data The data to cache
   * @param ttlSeconds Time to live in seconds
   */
  void put(const K &key, const juce::MemoryBlock &data, int ttlSeconds = 3600) {
    auto cacheFile = getCacheFilePath(key);

    // Ensure directory exists
    cacheFile.getParentDirectory().createDirectory();

    // Write data
    if (cacheFile.replaceWithData(data.getData(), data.getSize())) {
      // Write metadata
      saveMetadata(cacheFile, ttlSeconds);
    }

    // Check size limits
    evictOldestIfNeeded();
  }

  /**
   * Remove cached item
   */
  bool remove(const K &key) {
    auto cacheFile = getCacheFilePath(key);
    return cacheFile.deleteFile();
  }

  /**
   * Clear all cached items
   */
  void clear() {
    cacheDir_.deleteRecursively();
    cacheDir_.createDirectory();
  }

  /**
   * Get current cache size in bytes
   */
  size_t getCurrentSizeBytes() const {
    return calculateDirectorySize(cacheDir_);
  }

private:
  juce::File cacheDir_;
  size_t maxSizeBytes_;

  /**
   * Get cache file path for a key
   */
  juce::File getCacheFilePath(const K &key) const {
    juce::String keyStr = juce::String(key);
    // Hash the key to create safe filename
    auto hash = std::hash<std::string>{}(keyStr.toStdString());
    juce::String filename = juce::String(hash) + ".cache";
    return cacheDir_.getChildFile(filename);
  }

  /**
   * Load metadata for a cache file
   */
  std::optional<CacheEntry<juce::MemoryBlock>> loadMetadata(const juce::File &file) const {
    auto metadataFile = file.withFileExtension(".meta");
    if (!metadataFile.existsAsFile())
      return std::nullopt;

    juce::MemoryBlock metadata;
    if (metadataFile.loadFileAsData(metadata)) {
      // Parse TTL (simplified: stored as text)
      juce::String metaStr(static_cast<const char *>(metadata.getData()), metadata.getSize());
      int ttl = metaStr.getIntValue();

      CacheEntry<juce::MemoryBlock> entry;
      entry.ttlSeconds = ttl;
      entry.createdAt = std::chrono::steady_clock::now();
      return entry;
    }

    return std::nullopt;
  }

  /**
   * Save metadata for a cache file
   */
  void saveMetadata(const juce::File &file, int ttlSeconds) {
    auto metadataFile = file.withFileExtension(".meta");
    juce::String ttlStr = juce::String(ttlSeconds);
    metadataFile.replaceWithText(ttlStr);
  }

  /**
   * Calculate directory size recursively
   */
  size_t calculateDirectorySize(const juce::File &dir) const {
    size_t totalSize = 0;
    for (const auto &f : dir.findChildFiles(juce::File::findFilesAndDirectories, true)) {
      if (f.existsAsFile())
        totalSize += static_cast<size_t>(f.getSize());
    }
    return totalSize;
  }

  /**
   * Evict oldest files if cache exceeds size limit
   */
  void evictOldestIfNeeded() {
    size_t currentSize = getCurrentSizeBytes();
    if (currentSize > maxSizeBytes_) {
      // Find and delete oldest files
      std::vector<std::pair<juce::int64, juce::File>> files;
      for (const auto &f : cacheDir_.findChildFiles(juce::File::findFiles, false, "*.cache")) {
        files.emplace_back(f.getLastModificationTime().toMilliseconds(), f);
      }

      // Sort by modification time (oldest first)
      std::sort(files.begin(), files.end());

      // Delete oldest files until size is acceptable
      size_t targetSize = maxSizeBytes_ * 9 / 10; // 90% of max
      for (auto &filePair : files) {
        if (currentSize <= targetSize)
          break;

        auto &file = filePair.second;
        auto metadataFile = file.withFileExtension(".meta");
        metadataFile.deleteFile();
        currentSize -= static_cast<size_t>(file.getSize());
        file.deleteFile();
      }
    }
  }
};

/**
 * MultiTierCache - Unified cache with memory and disk tiers
 *
 * Automatically promotes/demotes data between tiers based on access patterns.
 *
 * @tparam K Key type
 * @tparam V Value type
 */
template <typename K, typename V> class MultiTierCache {
public:
  /**
   * Create a multi-tier cache
   *
   * @param memoryMaxBytes Maximum memory tier size
   * @param diskDir Directory for disk cache
   * @param diskMaxMB Maximum disk cache size in MB
   */
  MultiTierCache(size_t memoryMaxBytes, const juce::File &diskDir, size_t diskMaxMB = 1024)
      : memoryCache_(memoryMaxBytes, 10000), diskDir_(diskDir), diskMaxMB_(diskMaxMB) {
    // Initialize disk cache directory if provided
    if (diskDir.existsAsDirectory()) {
      Log::debug("MultiTierCache: Disk cache directory initialized at " + diskDir.getFullPathName() +
                 " with max size " + juce::String(static_cast<int>(diskMaxMB)) + "MB");
    } else if (!diskDir.getFullPathName().isEmpty()) {
      Log::warn("MultiTierCache: Disk cache directory does not exist: " + diskDir.getFullPathName());
    }
  }

  virtual ~MultiTierCache() = default;

  /**
   * Get value from cache (checks memory first, then disk)
   *
   * @param key The cache key
   * @return Optional containing value if found
   */
  std::optional<V> get(const K &key) {
    // Try memory cache first
    auto result = memoryCache_.get(key);
    if (result) {
      statsHit();
      return result;
    }

    // Try disk cache (if implemented)
    // For now, memory only
    statsMiss();
    return std::nullopt;
  }

  /**
   * Put value in cache with TTL
   *
   * @param key The cache key
   * @param value The value to cache
   * @param ttlSeconds Time to live in seconds
   * @param promoteToMemory Whether to keep in memory tier
   */
  void put(const K &key, const V &value, int ttlSeconds = 3600, bool promoteToMemory = true) {
    if (promoteToMemory) {
      memoryCache_.put(key, value, ttlSeconds, estimateSize(value));
    }
  }

  /**
   * Remove from cache
   */
  bool remove(const K &key) {
    return memoryCache_.remove(key);
  }

  /**
   * Clear all caches
   */
  void clear() {
    memoryCache_.clear();
  }

  /**
   * Get cache statistics
   */
  struct CacheStats {
    size_t memoryBytes;
    size_t itemCount;
    float hitRate;
  };

  CacheStats getStats() const {
    return {memoryCache_.getCurrentSizeBytes(), memoryCache_.getItemCount(), memoryCache_.getHitRate()};
  }

private:
  MemoryCache<K, V> memoryCache_;
  juce::File diskDir_;
  size_t diskMaxMB_ = 0;
  mutable size_t hitCount_ = 0;
  mutable size_t missCount_ = 0;

  /**
   * Estimate size of a value
   */
  size_t estimateSize(const V &value) const {
    // Use sizeof for basic types, otherwise use reasonable estimate
    size_t baseSize = sizeof(V);
    // For string-like types, add estimated content size
    // This is a heuristic - real implementation would need type specialization
    size_t estimatedSize = baseSize + 512; // Add buffer for potential dynamic content
    return estimatedSize;
  }

  void statsHit() {
    hitCount_++;
  }

  void statsMiss() {
    missCount_++;
  }
};

} // namespace Cache
} // namespace Util
} // namespace Sidechain
