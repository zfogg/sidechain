#pragma once

#include "../models/FeedPost.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * SavedPostsState - State for saved/bookmarked posts
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
 * ArchivedPostsState - State for archived posts
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
 * PostsState - Immutable state for all user post collections
 *
 * Manages:
 * - Saved posts (bookmarked/liked posts)
 * - Archived posts (hidden/deleted posts)
 *
 * Each collection has independent loading, pagination, and error state
 * allowing components to load different collections independently.
 */
struct PostsState {
  // Saved posts collection
  SavedPostsState savedPosts;

  // Archived posts collection
  ArchivedPostsState archivedPosts;

  // Global error tracking
  juce::String errorMessage;
  int64_t lastUpdated = 0;
};

/**
 * PostsStore - Reactive store for managing all user post collections
 *
 * Consolidates saved and archived posts into a single generalized store
 * similar to StoriesStore's approach of managing multiple related data types.
 *
 * Features:
 * - Load saved/bookmarked posts from server
 * - Load archived/hidden posts from server
 * - Pagination support for both collections
 * - Optimistic removal (unsave, restore)
 * - Independent loading and pagination state per collection
 * - Error handling and recovery
 *
 * Usage:
 * ```cpp
 * auto postsStore = std::make_shared<PostsStore>(networkClient);
 * postsStore->subscribe([this](const PostsState& state) {
 *   displaySavedPosts(state.savedPosts.posts);
 *   displayArchivedPosts(state.archivedPosts.posts);
 * });
 * postsStore->loadSavedPosts();
 * postsStore->loadArchivedPosts();
 * ```
 */
class PostsStore : public Store<PostsState> {
public:
  explicit PostsStore(NetworkClient *client);
  ~PostsStore() override = default;

  //==============================================================================
  // Saved Posts Loading
  void loadSavedPosts();
  void loadMoreSavedPosts();
  void refreshSavedPosts();

  //==============================================================================
  // Archived Posts Loading
  void loadArchivedPosts();
  void loadMoreArchivedPosts();
  void refreshArchivedPosts();

  //==============================================================================
  // Saved Posts Operations
  /**
   * Remove a post from saved (optimistic update + server sync)
   */
  void unsavePost(const juce::String &postId);

  /**
   * Get a specific saved post by ID
   */
  std::optional<FeedPost> getSavedPostById(const juce::String &postId) const;

  //==============================================================================
  // Archived Posts Operations
  /**
   * Restore an archived post (move back to active)
   */
  void restorePost(const juce::String &postId);

  /**
   * Get a specific archived post by ID
   */
  std::optional<FeedPost> getArchivedPostById(const juce::String &postId) const;

  //==============================================================================
  // Current State Access - Saved Posts
  bool isSavedPostsLoading() const {
    return getState().savedPosts.isLoading;
  }

  int getSavedPostsTotal() const {
    return getState().savedPosts.totalCount;
  }

  juce::Array<FeedPost> getSavedPosts() const {
    return getState().savedPosts.posts;
  }

  //==============================================================================
  // Current State Access - Archived Posts
  bool isArchivedPostsLoading() const {
    return getState().archivedPosts.isLoading;
  }

  int getArchivedPostsTotal() const {
    return getState().archivedPosts.totalCount;
  }

  juce::Array<FeedPost> getArchivedPosts() const {
    return getState().archivedPosts.posts;
  }

protected:
  //==============================================================================
  // Helper methods (accessible to subclasses)
  void removePostFromSaved(const juce::String &postId);
  void removePostFromArchived(const juce::String &postId);
  void updatePostInSaved(const FeedPost &post);
  void updatePostInArchived(const FeedPost &post);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Internal state update helpers
  void updateSavedPosts(const juce::Array<FeedPost> &posts, int totalCount, int offset, bool hasMore);
  void updateArchivedPosts(const juce::Array<FeedPost> &posts, int totalCount, int offset, bool hasMore);

  //==============================================================================
  // Network callbacks
  void handleSavedPostsLoaded(Outcome<juce::var> result);
  void handleArchivedPostsLoaded(Outcome<juce::var> result);
  void handlePostUnsaved(const juce::String &postId, Outcome<juce::var> result);
  void handlePostRestored(const juce::String &postId, Outcome<juce::var> result);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostsStore)
};

} // namespace Stores
} // namespace Sidechain
