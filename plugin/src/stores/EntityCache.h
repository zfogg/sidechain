#pragma once

#include "../util/logging/Logger.h"
#include <JuceHeader.h>
#include <algorithm>
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
      // Don't remove expired entries in const method to maintain const-correctness
      // Expired entries will be cleaned up by expireStale() or invalidatePattern()
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
    notifyAllObservers();
  }

  /**
   * DEPRECATED: Immutable alternative pattern for entity updates
   *
   * INSTEAD OF: cache.update(id, [](T& e) { e.field = value; });
   * USE THIS:
   *   auto entity = cache.get(id);
   *   if (entity) {
   *     auto newEntity = std::make_shared<T>(*entity);
   *     newEntity->field = value;
   *     cache.set(id, newEntity);
   *   }
   *
   * This maintains immutability and provides clean state snapshots to observers.
   * See optimisticUpdate() if you need rollback semantics with snapshots.
   *
   * DEPRECATED: Kept for backwards compatibility only.
   * Apply updater function to existing entity (if found)
   * Notifies observers after update
   *
   * @param id Entity ID to update
   * @param updater Function that mutates entity in-place
   * @return true if entity found and updated, false if not found or expired
   *
   * WARNING: This mutates shared entity in-place. All observers receive the mutated reference.
   * Prefer creating new entities with set() instead for better state management.
   */
  template <typename Updater> bool update(const juce::String &id, Updater &&updater) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(id);
    if (it == entities_.end() || isExpired(id)) {
      return false;
    }

    // MUTATION WARNING: This mutates shared entity. All observers receive the mutated entity.
    // Prefer immutable patterns: get() -> copy -> modify -> set()
    updater(*it->second);
    timestamps_[id] = juce::Time::currentTimeMillis();

    // Notify observers
    notifyObservers(id, it->second);
    notifyAllObservers();
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
      std::lock_guard<std::mutex> unsubscribeLock(mutex_);
      auto &list = observers_[id];
      list.erase(
          std::remove_if(list.begin(), list.end(), [observerId](const auto &pair) { return pair.first == observerId; }),
          list.end());
    };
  }

  /**
   * Subscribe to all entity updates
   * Observer is called with vector of all entities whenever cache changes
   * Returns unsubscribe function
   */
  Unsubscriber subscribeAll(std::function<void(const std::vector<std::shared_ptr<T>> &)> observer) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto observerId = nextObserverId_++;
    allObservers_.emplace_back(observerId, std::move(observer));

    // Immediately call observer with current state
    std::vector<std::shared_ptr<T>> all;
    for (const auto &[id, entity] : entities_) {
      if (!isExpired(id)) {
        all.push_back(entity);
      }
    }
    allObservers_.back().second(all);

    // Return unsubscribe function
    return [this, observerId]() {
      std::lock_guard<std::mutex> unsubLock(mutex_);
      allObservers_.erase(std::remove_if(allObservers_.begin(), allObservers_.end(),
                                         [observerId](const auto &pair) { return pair.first == observerId; }),
                          allObservers_.end());
    };
  }

  // ==============================================================================
  // Key-based subscriptions (cleaner API for subscribing to specific entity IDs)

  /**
   * Subscribe to updates for a single entity key/ID
   * Observer is called whenever that entity is updated
   * Call unsubscribeFromKey() with the same key to stop listening
   */
  void subscribeToKey(const std::string &key, std::function<void(const std::shared_ptr<T> &)> observer) {
    std::lock_guard<std::mutex> lock(mutex_);

    keySubscriptions_.push_back({std::vector<std::string>{key}, observer});

    // Call observer immediately with current entity if it exists
    auto it = entities_.find(juce::String(key));
    if (it != entities_.end() && !isExpired(juce::String(key))) {
      observer(it->second);
    }
  }

  /**
   * Subscribe to updates for multiple entity keys/IDs
   * Observer is called with vector of updated entities whenever any of these keys change
   * Call unsubscribeFromKeys() with the same keys to stop listening
   */
  void subscribeToKeys(const std::vector<std::string> &keys,
                       std::function<void(const std::vector<std::shared_ptr<T>> &)> observer) {
    std::lock_guard<std::mutex> lock(mutex_);

    keySubscriptions_.push_back({keys, observer});

    // Call observer immediately with current entities for these keys
    std::vector<std::shared_ptr<T>> current;
    for (const auto &key : keys) {
      auto it = entities_.find(juce::String(key));
      if (it != entities_.end() && !isExpired(juce::String(key))) {
        current.push_back(it->second);
      }
    }
    observer(current);
  }

  /**
   * Unsubscribe from a single entity key/ID
   * Removes all subscriptions to this specific key
   */
  void unsubscribeFromKey(const std::string &key) {
    std::lock_guard<std::mutex> lock(mutex_);

    keySubscriptions_.erase(
        std::remove_if(keySubscriptions_.begin(), keySubscriptions_.end(),
                       [&key](const KeySubscription &sub) { return sub.keys.size() == 1 && sub.keys[0] == key; }),
        keySubscriptions_.end());
  }

  /**
   * Unsubscribe from multiple entity keys/IDs
   * Removes subscriptions matching this exact set of keys
   */
  void unsubscribeFromKeys(const std::vector<std::string> &keys) {
    std::lock_guard<std::mutex> lock(mutex_);

    keySubscriptions_.erase(std::remove_if(keySubscriptions_.begin(), keySubscriptions_.end(),
                                           [&keys](const KeySubscription &sub) { return sub.keys == keys; }),
                            keySubscriptions_.end());
  }

  /**
   * Get all entities as vector of shared_ptrs
   * Returns vector of all non-expired entities
   */
  std::vector<std::shared_ptr<T>> getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<T>> result;

    for (const auto &[id, entity] : entities_) {
      if (!isExpired(id)) {
        result.push_back(entity);
      }
    }

    return result;
  }

  /**
   * Get number of entities currently in cache (including expired)
   */
  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entities_.size();
  }

  // ==============================================================================
  // Optimistic updates

  /**
   * Optimistic update with automatic rollback on error
   *
   * Saves current state snapshot, applies update immediately (optimistic),
   * and provides rollback mechanism if network request fails.
   *
   * USAGE: For optimistic UI updates that should roll back on error:
   *   cache.optimisticUpdate(id, [](T& e) { e.isLiked = true; });
   *   networkCall([id]() {
   *     if (failed) cache.rollbackOptimistic(id);
   *     else cache.confirmOptimistic(id);
   *   });
   *
   * WARNING: This mutates shared entity in-place during optimistic phase.
   * Use confirmOptimistic() or rollbackOptimistic() to finalize the update.
   *
   * @param id Entity ID to update optimistically
   * @param updater Function that mutates entity in-place
   */
  template <typename Updater> void optimisticUpdate(const juce::String &id, Updater &&updater) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(id);
    if (it == entities_.end()) {
      return;
    }

    // Save snapshot (deep copy of the entity) for potential rollback
    optimisticSnapshots_[id] = std::make_shared<T>(*it->second);

    // Apply update immediately (optimistic - show to user before server confirms)
    updater(*it->second);
    timestamps_[id] = juce::Time::currentTimeMillis();

    // Notify observers of optimistic change
    notifyObservers(id, it->second);
    notifyAllObservers();
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
    notifyAllObservers();

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
  // Key-based subscription struct
  struct KeySubscription {
    std::vector<std::string> keys;
    std::function<void(const std::vector<std::shared_ptr<T>> &)> callback;
  };

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

  // All-entity observers (for subscribeAll)
  std::vector<std::pair<uint64_t, std::function<void(const std::vector<std::shared_ptr<T>> &)>>> allObservers_;

  // Key-based subscriptions (cleaner API for subscribing to specific entity IDs)
  std::vector<KeySubscription> keySubscriptions_;

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
    // Notify per-entity observers
    auto it = observers_.find(id);
    if (it != observers_.end()) {
      for (const auto &[_, observer] : it->second) {
        observer(entity);
      }
    }

    // Notify key-based subscribers that are interested in this ID
    std::string idStr = id.toStdString();
    for (const auto &keySub : keySubscriptions_) {
      // Check if this subscription cares about this ID
      if (std::find(keySub.keys.begin(), keySub.keys.end(), idStr) != keySub.keys.end()) {
        // Build vector of all current entities for this subscription
        std::vector<std::shared_ptr<T>> updated;
        for (const auto &key : keySub.keys) {
          auto entityIt = entities_.find(juce::String(key));
          if (entityIt != entities_.end() && !isExpired(juce::String(key))) {
            updated.push_back(entityIt->second);
          }
        }
        keySub.callback(updated);
      }
    }
  }

  void notifyAllObservers() {
    if (allObservers_.empty()) {
      return;
    }

    // Collect all non-expired entities
    std::vector<std::shared_ptr<T>> all;
    for (const auto &[id, entity] : entities_) {
      if (!isExpired(id)) {
        all.push_back(entity);
      }
    }

    // Notify all subscribers
    for (const auto &[_, observer] : allObservers_) {
      observer(all);
    }
  }
};

} // namespace Stores
} // namespace Sidechain
