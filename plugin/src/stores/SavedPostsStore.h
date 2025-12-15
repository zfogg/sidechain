#pragma once

#include "../models/FeedPost.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * SavedPostsState - Immutable state for saved/bookmarked posts
 */
struct SavedPostsState {
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
 * SavedPostsStore - Reactive store for managing saved/bookmarked posts
 *
 * Features:
 * - Load saved posts from server
 * - Pagination support
 * - Optimistic removal (unsave)
 * - Error handling and recovery
 *
 * Usage:
 * ```cpp
 * auto savedPostsStore = std::make_shared<SavedPostsStore>(networkClient);
 * savedPostsStore->subscribe([this](const SavedPostsState& state) {
 *   // Update UI with saved posts
 * });
 * savedPostsStore->loadSavedPosts();
 * ```
 */
class SavedPostsStore : public Store<SavedPostsState> {
public:
  explicit SavedPostsStore(NetworkClient *client);
  ~SavedPostsStore() override = default;

  //==============================================================================
  // Data Loading
  void loadSavedPosts();
  void loadMoreSavedPosts();
  void refreshSavedPosts();

  //==============================================================================
  // Post Operations
  /**
   * Remove a post from saved (optimistic update + server sync)
   */
  void unsavePost(const juce::String &postId);

  /**
   * Get a specific saved post by ID
   */
  std::optional<FeedPost> getPostById(const juce::String &postId) const;

  //==============================================================================
  // Current State Access
  bool isLoading() const {
    return getState().isLoading;
  }
  int getTotalSavedCount() const {
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
  void handleSavedPostsLoaded(Outcome<juce::var> result);
  void handlePostUnsaved(const juce::String &postId, Outcome<juce::var> result);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SavedPostsStore)
};

} // namespace Stores
} // namespace Sidechain
