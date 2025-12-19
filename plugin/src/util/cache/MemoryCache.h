#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace Sidechain {
namespace Util {

/**
 * MemoryCache - Type-safe in-memory caching with expiration
 *
 * Replaces std::any-based caching with compile-time type safety using templates.
 * Provides automatic expiration, LRU eviction, and optional cleanup callbacks.
 *
 * Features:
 * - Type-safe key-value storage (Key, Value both template parameters)
 * - Configurable TTL per cache or per entry
 * - LRU eviction when capacity exceeded
 * - Optional value factory for lazy initialization
 * - Thread-safe operations with reader/writer locking
 * - Cleanup callbacks on eviction
 *
 * Usage:
 *   // Create type-safe cache for <string, Image> pairs
 *   MemoryCache<juce::String, juce::Image> imageCache(100); // 100 entry capacity
 *
 *   // Store value
 *   imageCache.put("user_123", loadImage("path/to/image.jpg"));
 *
 *   // Retrieve value
 *   if (auto image = imageCache.get("user_123")) {
 *     drawImage(*image);
 *   }
 *
 *   // With expiration
 *   imageCache.put("temp_image", image, 5000); // Expires in 5 seconds
 *
 *   // With factory (lazy loading)
 *   auto image = imageCache.getOrCreate("lazy_image",
 *     [](){ return loadImage("path/to/image.jpg"); });
 *
 * Performance:
 * - O(log n) lookup
 * - O(log n) insertion
 * - O(n) cleanup scan (periodic)
 * - Memory: O(n) where n = max capacity
 *
 * Thread safety:
 * - Reader lock for gets (multiple readers allowed)
 * - Writer lock for puts/deletes (exclusive)
 * - Cleanup runs in-place during operations (lock-held)
 */
template <typename Key, typename Value> class MemoryCache {
public:
  // Cleanup callback signature: called when entry is evicted or expires
  using CleanupCallback = std::function<void(const Key &, const Value &)>;

  /**
   * Create a new memory cache
   *
   * @param maxCapacity Maximum number of entries before LRU eviction
   * @param defaultTtlMs Default time-to-live in milliseconds (0 = no expiration)
   */
  explicit MemoryCache(size_t maxCapacity = 1000, int defaultTtlMs = 0)
      : maxCapacity_(maxCapacity), defaultTtlMs_(defaultTtlMs) {}

  virtual ~MemoryCache() {
    // Clear cache and invoke cleanup callbacks
    clear();
  }

  /**
   * Store a value in the cache
   *
   * @param key Cache key
   * @param value Value to store
   * @param ttlMs Time-to-live in milliseconds (0 = use default, -1 = never expire)
   */
  void put(const Key &key, const Value &value, int ttlMs = 0) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);

    // Remove old entry if exists (to invoke cleanup)
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      invokeCleanup_(it->first, it->second.value);
      cache_.erase(it);
    }

    // Determine expiration time
    auto expireMs = ttlMs;
    if (expireMs == 0) {
      expireMs = defaultTtlMs_;
    }

    auto expiresAt = (expireMs > 0) ? (now() + expireMs) : -1;

    // Insert new entry
    cache_[key] = CacheEntry{value, expiresAt};

    // Evict if over capacity
    while (cache_.size() > maxCapacity_) {
      evictLRU_();
    }
  }

  /**
   * Retrieve a value from the cache
   *
   * @param key Cache key
   * @return Optional containing value if found and not expired
   */
  std::optional<Value> get(const Key &key) {
    std::shared_lock<std::shared_mutex> lock(cacheMutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
      return std::nullopt;
    }

    // Check expiration
    if (isExpired_(it->second)) {
      // Can't evict while holding read lock, so defer
      lock.unlock();
      std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
      cache_.erase(it);
      return std::nullopt;
    }

    // Update access time for LRU
    it->second.lastAccessMs = now();
    return it->second.value;
  }

  /**
   * Get or create value using factory function
   *
   * If value exists and not expired, returns it.
   * Otherwise, calls factory to create new value and stores it.
   *
   * @param key Cache key
   * @param factory Function to create value if not found
   * @param ttlMs Time-to-live in milliseconds
   * @return The cached or newly created value
   */
  Value getOrCreate(const Key &key, std::function<Value()> factory, int ttlMs = 0) {
    {
      std::shared_lock<std::shared_mutex> lock(cacheMutex_);
      auto it = cache_.find(key);
      if (it != cache_.end() && !isExpired_(it->second)) {
        it->second.lastAccessMs = now();
        return it->second.value;
      }
    }

    // Value not found or expired, create new one
    Value value = factory();
    put(key, value, ttlMs);
    return value;
  }

  /**
   * Check if key exists and is not expired
   */
  bool contains(const Key &key) {
    std::shared_lock<std::shared_mutex> lock(cacheMutex_);
    auto it = cache_.find(key);
    return it != cache_.end() && !isExpired_(it->second);
  }

  /**
   * Remove a specific entry
   */
  void remove(const Key &key) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      invokeCleanup_(it->first, it->second.value);
      cache_.erase(it);
    }
  }

  /**
   * Clear all entries from cache
   */
  void clear() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);
    for (auto &entry : cache_) {
      invokeCleanup_(entry.first, entry.second.value);
    }
    cache_.clear();
  }

  /**
   * Clean up expired entries
   *
   * Useful to call periodically to free memory
   */
  void cleanupExpired() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex_);
    std::vector<Key> toRemove;

    for (auto &entry : cache_) {
      if (isExpired_(entry.second)) {
        toRemove.push_back(entry.first);
      }
    }

    for (const auto &key : toRemove) {
      auto it = cache_.find(key);
      if (it != cache_.end()) {
        invokeCleanup_(it->first, it->second.value);
        cache_.erase(it);
      }
    }
  }

  /**
   * Get cache statistics
   */
  size_t size() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex_);
    return cache_.size();
  }

  size_t capacity() const {
    return maxCapacity_;
  }

  /**
   * Set cleanup callback invoked when entries are evicted
   */
  void setCleanupCallback(CleanupCallback callback) {
    cleanupCallback_ = callback;
  }

private:
  struct CacheEntry {
    Value value;
    long long expiresAtMs; // -1 = never expires, >0 = expiration time
    long long lastAccessMs = 0;

    CacheEntry() : expiresAtMs(-1), lastAccessMs(0) {}
    CacheEntry(const Value &v, long long exp) : value(v), expiresAtMs(exp), lastAccessMs(now()) {}
  };

  /**
   * Get current time in milliseconds
   */
  static long long now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
  }

  /**
   * Check if entry is expired
   */
  bool isExpired_(const CacheEntry &entry) const {
    if (entry.expiresAtMs < 0) {
      return false; // Never expires
    }
    return now() >= entry.expiresAtMs;
  }

  /**
   * Evict least-recently-used entry
   * Must be called with write lock held
   */
  void evictLRU_() {
    if (cache_.empty()) {
      return;
    }

    auto lruIt = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
      if (it->second.lastAccessMs < lruIt->second.lastAccessMs) {
        lruIt = it;
      }
    }

    invokeCleanup_(lruIt->first, lruIt->second.value);
    cache_.erase(lruIt);
  }

  /**
   * Invoke cleanup callback if set
   */
  void invokeCleanup_(const Key &key, const Value &value) {
    if (cleanupCallback_) {
      cleanupCallback_(key, value);
    }
  }

  // Cache storage
  std::map<Key, CacheEntry> cache_;
  mutable std::shared_mutex cacheMutex_;

  // Configuration
  size_t maxCapacity_;
  int defaultTtlMs_;

  // Cleanup callback
  CleanupCallback cleanupCallback_;
};

} // namespace Util
} // namespace Sidechain
