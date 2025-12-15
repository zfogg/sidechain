#pragma once

#include "../models/FeedPost.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * ArchivedPostsState - Immutable state for archived posts
 */
struct ArchivedPostsState {
  juce::Array<FeedPost> posts;
  bool isLoading = false;
  juce::String error;
  int totalCount = 0;
  int offset = 0;
  int limit = 20;
  bool hasMore = true;
  int64_t lastUpdated = 0;
};

/**
 * ArchivedPostsStore - Reactive store for managing archived posts
 *
 * Features:
 * - Load archived posts from server
 * - Pagination support
 * - Optimistic removal (restore)
 * - Error handling and recovery
 *
 * Usage:
 * ```cpp
 * auto archivedPostsStore = std::make_shared<ArchivedPostsStore>(networkClient);
 * archivedPostsStore->subscribe([this](const ArchivedPostsState& state) {
 *   // Update UI with archived posts
 * });
 * archivedPostsStore->loadArchivedPosts();
 * ```
 */
class ArchivedPostsStore : public Store<ArchivedPostsState> {
public:
  explicit ArchivedPostsStore(NetworkClient *client);
  ~ArchivedPostsStore() override = default;

  //==============================================================================
  // Data Loading
  void loadArchivedPosts();
  void loadMoreArchivedPosts();
  void refreshArchivedPosts();

  //==============================================================================
  // Post Operations
  /**
   * Restore an archived post (move back to active)
   */
  void restorePost(const juce::String &postId);

  /**
   * Get a specific archived post by ID
   */
  std::optional<FeedPost> getPostById(const juce::String &postId) const;

  //==============================================================================
  // Current State Access
  bool isLoading() const {
    return getState().isLoading;
  }
  int getTotalArchivedCount() const {
    return getState().totalCount;
  }

protected:
  //==============================================================================
  // Helper methods (accessible to subclasses)
  void removePostFromState(const juce::String &postId);
  void updatePostInState(const FeedPost &post);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handleArchivedPostsLoaded(Outcome<juce::var> result);
  void handlePostRestored(const juce::String &postId, Outcome<juce::var> result);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArchivedPostsStore)
};

} // namespace Stores
} // namespace Sidechain
