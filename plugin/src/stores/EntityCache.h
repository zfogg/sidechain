#pragma once

#include "../util/logging/Logger.h"
#include <JuceHeader.h>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Sidechain {
namespace Stores {

// ==============================================================================
/**
 * EntityCache<T> - Thread-safe cache for entities with shared memory management
 *
 * Uses std::shared_ptr for automatic memory management and deduplication:
 * - Same entity ID returns the same shared_ptr (memory deduplication)
 * - When all references drop (state cleared, UI closed), memory is freed automatically
 * - Thread-safe operations with std::mutex
 * - TTL-based expiration (time-to-live)
 * - Per-entity reactive subscriptions (observers notified on updates)
 * - Optimistic updates with rollback support
 * - Pattern-based invalidation (e.g., "post:*")
 *
 * Usage:
 *   EntityCache<FeedPost> posts;
 *   posts.setDefaultTTL(30000); // 30 seconds
 *
 *   // Set entity (creates or updates shared_ptr)
 *   auto post = std::make_shared<FeedPost>(data);
 *   posts.set(post->id, post);
 *
 *   // Get entity (returns same shared_ptr if called multiple times)
 *   auto postPtr = posts.get(postId);
 *   if (postPtr) { use(*postPtr); }
 *
 *   // Or get/create pattern for deduplication
 *   auto post = posts.getOrCreate(postId, []() {
 *       return std::make_shared<FeedPost>();
 *   });
 *
 *   // Subscribe to changes
 *   auto unsub = posts.subscribe(postId, [](const std::shared_ptr<FeedPost>& post) {
 *       updateUI(*post);
 *   });
 *
 * @tparam T Entity type (must have getId() method)
 */
template <typename T> class EntityCache {
public:
  using Observer = std::function<void(const std::shared_ptr<T> &)>;
  using Unsubscriber = std::function<void()>;

  // ==============================================================================
  // Constructor

  explicit EntityCache(int64_t defaultTTLMs = 0) : defaultTTL_(defaultTTLMs) {}

  // ==============================================================================
  // Core operations

  /**
   * Get entity by ID
   * Returns nullptr if not found or expired
   * Returns the SAME shared_ptr on repeated calls (memory deduplication)
   */
  std::shared_ptr<T> get(const juce::String &id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(id);
    if (it == entities_.end()) {
      return nullptr;
    }

    // Check if expired
    if (isExpired(id)) {
      // Remove expired entry (mutable operation in const method)
      const_cast<EntityCache<T> *>(this)->remove(id);
      return nullptr;
    }

    return it->second;
  }

  /**
   * Get or create entity
   * If entity exists, returns existing shared_ptr
   * If not, calls factory to create new entity and stores it
   * This ensures deduplication - same ID always returns same shared_ptr
   */
  template <typename Factory> std::shared_ptr<T> getOrCreate(const juce::String &id, Factory &&factory) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(id);
    if (it != entities_.end() && !isExpired(id)) {
      return it->second;
    }

    // Create new entity
    auto entity = factory();
    entities_[id] = entity;
    timestamps_[id] = juce::Time::currentTimeMillis();

    return entity;
  }

  /**
   * Set entity
   * If entity with same ID exists, updates the existing shared_ptr's contents
   * If not, creates new shared_ptr
   */
  void set(const juce::String &id, const std::shared_ptr<T> &entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    entities_[id] = entity;
    timestamps_[id] = juce::Time::currentTimeMillis();

    // Notify observers
    notifyObservers(id, entity);
  }

  /**
   * Update entity in place
   * Applies updater function to existing entity (if found)
   * Notifies observers after update
   */
  template <typename Updater> bool update(const juce::String &id, Updater &&updater) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(id);
    if (it == entities_.end() || isExpired(id)) {
      return false;
    }

    // Apply update
    updater(*it->second);
    timestamps_[id] = juce::Time::currentTimeMillis();

    // Notify observers
    notifyObservers(id, it->second);
    return true;
  }

  /**
   * Remove entity from cache
   */
  void remove(const juce::String &id) {
    std::lock_guard<std::mutex> lock(mutex_);

    entities_.erase(id);
    timestamps_.erase(id);
    optimisticSnapshots_.erase(id);
    // Note: observers are not removed, they'll just stop receiving updates
  }

  /**
   * Check if entity exists (and is not expired)
   */
  bool has(const juce::String &id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entities_.find(id) != entities_.end() && !isExpired(id);
  }

  // ==============================================================================
  // Batch operations

  /**
   * Get multiple entities by IDs
   * Returns vector of shared_ptrs (nullptr for not found/expired)
   */
  std::vector<std::shared_ptr<T>> getMany(const std::vector<juce::String> &ids) const {
    std::vector<std::shared_ptr<T>> result;
    result.reserve(ids.size());

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &id : ids) {
      auto it = entities_.find(id);
      if (it != entities_.end() && !isExpired(id)) {
        result.push_back(it->second);
      } else {
        result.push_back(nullptr);
      }
    }

    return result;
  }

  /**
   * Set multiple entities at once
   */
  void setMany(const std::vector<std::pair<juce::String, std::shared_ptr<T>>> &entries) {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t now = juce::Time::currentTimeMillis();
    for (const auto &[id, entity] : entries) {
      entities_[id] = entity;
      timestamps_[id] = now;
      notifyObservers(id, entity);
    }
  }

  // ==============================================================================
  // Reactive subscriptions

  /**
   * Subscribe to entity updates
   * Observer is called whenever entity is set/updated
   * Returns unsubscribe function
   */
  Unsubscriber subscribe(const juce::String &id, Observer observer) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto observerId = nextObserverId_++;
    observers_[id].emplace_back(observerId, std::move(observer));

    // Return unsubscribe function
    return [this, id, observerId]() {
      std::lock_guard<std::mutex> lock(mutex_);
      auto &list = observers_[id];
      list.erase(
          std::remove_if(list.begin(), list.end(), [observerId](const auto &pair) { return pair.first == observerId; }),
          list.end());
    };
  }

  // ==============================================================================
  // Optimistic updates

  /**
   * Start optimistic update
   * Saves current state as snapshot for potential rollback
   * Updates entity immediately
   */
  template <typename Updater> void optimisticUpdate(const juce::String &id, Updater &&updater) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(id);
    if (it == entities_.end()) {
      return;
    }

    // Save snapshot (deep copy of the entity)
    optimisticSnapshots_[id] = std::make_shared<T>(*it->second);

    // Apply optimistic update
    updater(*it->second);
    timestamps_[id] = juce::Time::currentTimeMillis();

    // Notify observers
    notifyObservers(id, it->second);
  }

  /**
   * Confirm optimistic update succeeded
   * Discards rollback snapshot
   */
  void confirmOptimistic(const juce::String &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    optimisticSnapshots_.erase(id);
  }

  /**
   * Rollback optimistic update (on failure)
   * Restores entity to snapshot state
   */
  void rollbackOptimistic(const juce::String &id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto snapIt = optimisticSnapshots_.find(id);
    if (snapIt == optimisticSnapshots_.end()) {
      return;
    }

    // Restore snapshot
    entities_[id] = snapIt->second;
    timestamps_[id] = juce::Time::currentTimeMillis();

    // Notify observers of rollback
    notifyObservers(id, snapIt->second);

    // Clear snapshot
    optimisticSnapshots_.erase(id);
  }

  // ==============================================================================
  // Cache management

  /**
   * Invalidate specific entity (remove from cache)
   */
  void invalidate(const juce::String &id) {
    remove(id);
  }

  /**
   * Invalidate all entities
   */
  void invalidateAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    entities_.clear();
    timestamps_.clear();
    optimisticSnapshots_.clear();
  }

  /**
   * Invalidate entities matching pattern
   * Example: invalidatePattern("post:*") removes all post entities
   */
  void invalidatePattern(const juce::String &pattern) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Simple wildcard pattern matching (* at end)
    bool hasWildcard = pattern.endsWith("*");
    juce::String prefix = hasWildcard ? pattern.dropLastCharacters(1) : pattern;

    std::vector<juce::String> toRemove;
    for (const auto &[id, _] : entities_) {
      if (hasWildcard && id.startsWith(prefix)) {
        toRemove.push_back(id);
      } else if (!hasWildcard && id == pattern) {
        toRemove.push_back(id);
      }
    }

    for (const auto &id : toRemove) {
      entities_.erase(id);
      timestamps_.erase(id);
      optimisticSnapshots_.erase(id);
    }
  }

  /**
   * Remove expired entries
   */
  void expireStale() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<juce::String> expired;
    for (const auto &[id, _] : entities_) {
      if (isExpired(id)) {
        expired.push_back(id);
      }
    }

    for (const auto &id : expired) {
      entities_.erase(id);
      timestamps_.erase(id);
      optimisticSnapshots_.erase(id);
    }
  }

  /**
   * Get cache statistics
   */
  struct Stats {
    size_t count = 0;
    size_t observerCount = 0;
    size_t optimisticCount = 0;
  };

  Stats getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stats stats;
    stats.count = entities_.size();
    for (const auto &[_, obs] : observers_) {
      stats.observerCount += obs.size();
    }
    stats.optimisticCount = optimisticSnapshots_.size();
    return stats;
  }

  // ==============================================================================
  // TTL configuration

  void setDefaultTTL(int64_t ttlMs) {
    defaultTTL_ = ttlMs;
  }

  int64_t getDefaultTTL() const {
    return defaultTTL_;
  }

private:
  // ==============================================================================
  // Internal state

  mutable std::mutex mutex_;

  // Entity storage - maps ID to shared_ptr<T>
  std::unordered_map<juce::String, std::shared_ptr<T>> entities_;

  // Timestamps for TTL expiration
  std::unordered_map<juce::String, int64_t> timestamps_;

  // Optimistic update snapshots for rollback
  std::unordered_map<juce::String, std::shared_ptr<T>> optimisticSnapshots_;

  // Per-entity observers
  std::unordered_map<juce::String, std::vector<std::pair<uint64_t, Observer>>> observers_;
  uint64_t nextObserverId_ = 0;

  // TTL configuration
  int64_t defaultTTL_ = 0; // 0 = no expiration

  // ==============================================================================
  // Internal helpers

  bool isExpired(const juce::String &id) const {
    if (defaultTTL_ <= 0) {
      return false; // No expiration
    }

    auto it = timestamps_.find(id);
    if (it == timestamps_.end()) {
      return true;
    }

    int64_t age = juce::Time::currentTimeMillis() - it->second;
    return age > defaultTTL_;
  }

  void notifyObservers(const juce::String &id, const std::shared_ptr<T> &entity) {
    auto it = observers_.find(id);
    if (it == observers_.end()) {
      return;
    }

    for (const auto &[_, observer] : it->second) {
      observer(entity);
    }
  }
};

} // namespace Stores
} // namespace Sidechain
