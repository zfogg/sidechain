#pragma once

#include "../ui/feed/Comment.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>
#include <map>

namespace Sidechain {
namespace Stores {

/**
 * CommentState - Immutable state for comments on a specific post
 */
struct CommentState {
  juce::String postId;
  juce::Array<Comment> comments;
  bool isLoading = false;
  juce::String error;
  int totalCount = 0;
  int offset = 0;
  int limit = 20;
  bool hasMore = true;
  int64_t lastUpdated = 0;

  bool operator==(const CommentState &other) const = default;
};

// Forward declaration
class NetworkClient;

/**
 * CommentStore - Reactive store for managing comments on a post
 *
 * Features:
 * - Load comments for a post
 * - Optimistic updates for likes/deletes
 * - Real-time sync of comment changes
 * - Pagination support
 * - Error handling and recovery
 *
 * Usage:
 * ```cpp
 * auto commentStore = std::make_shared<CommentStore>(networkClient);
 * commentStore->subscribe([this](const CommentState& state) {
 *   // Update UI with new comments
 * });
 * commentStore->loadCommentsForPost(postId);
 * ```
 */
class CommentStore : public Store<CommentState> {
public:
  explicit CommentStore(NetworkClient *networkClient);
  ~CommentStore() override = default;

  //==============================================================================
  // Data Loading
  void loadCommentsForPost(const juce::String &postId);
  void loadMoreComments();
  void refreshComments();

  //==============================================================================
  // Comment Operations
  /**
   * Toggle like on a comment (optimistic update + server sync)
   */
  void toggleCommentLike(const juce::String &commentId, bool shouldLike);

  /**
   * Delete a comment (optimistic removal + server sync)
   */
  void deleteComment(const juce::String &commentId);

  /**
   * Add a new comment to the post
   */
  void addComment(const juce::String &content, const juce::String &parentId = "");

  /**
   * Update an existing comment
   */
  void updateComment(const juce::String &commentId, const juce::String &newContent);

  /**
   * Get a specific comment by ID
   */
  std::optional<Comment> getCommentById(const juce::String &commentId) const;

  //==============================================================================
  // Current State Access
  juce::String getCurrentPostId() const {
    return getState().postId;
  }
  int getTotalCommentCount() const {
    return getState().totalCount;
  }
  bool isLoading() const {
    return getState().isLoading;
  }

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handleCommentsLoaded(Outcome<std::pair<juce::var, int>> result);
  void handleCommentCreated(Outcome<juce::var> result, const juce::String &tempId);
  void handleCommentLikeToggled(const juce::String &commentId, bool liked, Outcome<juce::var> result);
  void handleCommentDeleted(const juce::String &commentId, Outcome<juce::var> result);
  void handleCommentUpdated(const juce::String &commentId, Outcome<juce::var> result);

  //==============================================================================
  // Helper methods
  void updateState(std::function<void(CommentState &)> mutator);
  void notifySubscribers();
  Comment parseCommentFromJson(const juce::var &json) const;
  void removeCommentFromState(const juce::String &commentId);
  void updateCommentInState(const Comment &comment);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommentStore)
};

} // namespace Stores
} // namespace Sidechain
