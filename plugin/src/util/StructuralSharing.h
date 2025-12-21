#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <JuceHeader.h>

namespace Sidechain {
namespace Util {

/**
 * StructuralSharing - Efficient immutable collection updates
 *
 * Provides optimized patterns for updating collections within immutable state
 * without copying the entire state object each time.
 *
 * Key insight: Since state collections use shared_ptr<T>, we can:
 * 1. Update individual collection items (just replace the shared_ptr)
 * 2. Update collection metadata (error, loading flags) separately
 * 3. Avoid copying non-changing parts
 *
 * Usage for post likes:
 * ```cpp
 * // Before: Copy entire FeedState, replace entire posts vector
 * FeedState newState = feedState;  // Copy vector metadata
 * newState.posts = updatedPosts;   // Replace posts
 * slice->setState(newState);
 *
 * // After: Update just the affected post
 * if (auto post = cache.get(postId)) {
 *   auto updated = post->withLikeStatus(liked, newCount);
 *   auto newState = feedState;
 *   StructuralSharing::updateVector(newState.posts, postId, updated);
 *   slice->setState(newState);
 * }
 * ```
 *
 * Benefits:
 * - Avoids full vector copies for single-item updates
 * - Clear intent: "update this one item" vs "replace entire list"
 * - Efficient for large post lists
 */
class StructuralSharing {
public:
  /**
   * Update a single item in a collection by ID
   *
   * Finds and replaces the first item with matching getId() without copying
   * non-modified items.
   *
   * @param collection Vector of shared_ptr items
   * @param itemId ID to match (calls item->getId() == itemId)
   * @param replacement Replacement item (shared_ptr)
   * @return true if item was found and updated, false otherwise
   */
  template <typename T>
  static bool updateVector(std::vector<std::shared_ptr<T>> &collection, const juce::String &itemId,
                           const std::shared_ptr<T> &replacement) {
    auto it = std::find_if(collection.begin(), collection.end(),
                           [&itemId](const std::shared_ptr<T> &item) { return item && item->getId() == itemId; });

    if (it != collection.end()) {
      *it = replacement;
      return true;
    }
    return false;
  }

  /**
   * Update a single item in a collection where items are value types
   *
   * @param collection Vector of value-type items
   * @param predicate Function that returns true for the item to update
   * @param replacement Replacement item value
   * @return true if item was found and updated, false otherwise
   */
  template <typename T, typename Predicate>
  static bool updateVectorByPredicate(std::vector<T> &collection, Predicate predicate, const T &replacement) {
    auto it = std::find_if(collection.begin(), collection.end(), predicate);
    if (it != collection.end()) {
      *it = replacement;
      return true;
    }
    return false;
  }

  /**
   * Remove a single item from collection by ID
   *
   * @param collection Vector to modify
   * @param itemId ID to match
   * @return true if item was found and removed, false otherwise
   */
  template <typename T>
  static bool removeFromVector(std::vector<std::shared_ptr<T>> &collection, const juce::String &itemId) {
    auto it = std::find_if(collection.begin(), collection.end(),
                           [&itemId](const std::shared_ptr<T> &item) { return item && item->getId() == itemId; });

    if (it != collection.end()) {
      collection.erase(it);
      return true;
    }
    return false;
  }

  /**
   * Insert item in collection at specific position
   *
   * Useful for pagination and list management without copying the entire vector
   *
   * @param collection Vector to modify
   * @param position Position to insert (0 = front)
   * @param item Item to insert
   */
  template <typename T>
  static void insertInVector(std::vector<std::shared_ptr<T>> &collection, size_t position,
                             const std::shared_ptr<T> &item) {
    if (position > collection.size()) {
      position = collection.size();
    }
    collection.insert(collection.begin() + static_cast<int>(position), item);
  }

  /**
   * Prepend an item to the front of collection
   *
   * More efficient than insert(0, item) in terms of intent clarity
   *
   * @param collection Vector to modify
   * @param item Item to add to front
   */
  template <typename T>
  static void prependToVector(std::vector<std::shared_ptr<T>> &collection, const std::shared_ptr<T> &item) {
    collection.insert(collection.begin(), item);
  }

  /**
   * Append an item to the back of collection
   *
   * More intent-revealing than push_back() in state update context
   *
   * @param collection Vector to modify
   * @param item Item to add to back
   */
  template <typename T>
  static void appendToVector(std::vector<std::shared_ptr<T>> &collection, const std::shared_ptr<T> &item) {
    collection.push_back(item);
  }
};

} // namespace Util
} // namespace Sidechain
