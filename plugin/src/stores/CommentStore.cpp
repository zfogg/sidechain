#include "CommentStore.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

CommentStore::CommentStore(NetworkClient *client) : Store(CommentState()), networkClient(client) {
  Util::logInfo("CommentStore", "Initialized");
}

//==============================================================================
void CommentStore::loadCommentsForPost(const juce::String &postId) {
  if (postId.isEmpty() || networkClient == nullptr) {
    Util::logWarning("CommentStore", "Cannot load comments - postId empty or networkClient null");
    return;
  }

  Util::logInfo("CommentStore", "Loading comments for post: " + postId);

  // Update state: set loading flag
  updateState([postId](CommentState &state) {
    state.postId = postId;
    state.isLoading = true;
    state.offset = 0;
    state.comments.clear();
    state.error = "";
  });

  // Load from network
  networkClient->getComments(postId, 20, 0,
                             [this](Outcome<std::pair<juce::var, int>> result) { handleCommentsLoaded(result); });
}

void CommentStore::loadMoreComments() {
  auto state = getState();
  if (state.postId.isEmpty() || !state.hasMore || networkClient == nullptr) {
    return;
  }

  Util::logDebug("CommentStore", "Loading more comments for post: " + state.postId);

  updateState([](CommentState &s) { s.isLoading = true; });

  networkClient->getComments(state.postId, state.limit, state.offset,
                             [this](Outcome<std::pair<juce::var, int>> result) { handleCommentsLoaded(result); });
}

void CommentStore::refreshComments() {
  auto state = getState();
  if (state.postId.isEmpty()) {
    return;
  }
  loadCommentsForPost(state.postId);
}

void CommentStore::handleCommentsLoaded(Outcome<std::pair<juce::var, int>> result) {
  auto state = getState();
  if (!result.isOk()) {
    updateState([error = result.getError()](CommentState &s) {
      s.isLoading = false;
      s.error = error;
    });
    return;
  }

  auto [commentsData, total] = result.getValue();
  if (!commentsData.isArray()) {
    updateState([](CommentState &s) {
      s.isLoading = false;
      s.error = "Invalid comments response";
    });
    return;
  }

  juce::Array<Comment> loadedComments;
  for (int i = 0; i < commentsData.size(); ++i) {
    auto comment = Comment::fromJson(commentsData[i]);
    if (comment.isValid()) {
      loadedComments.add(comment);
    }
  }

  updateState([loadedComments, total](CommentState &s) {
    s.comments = loadedComments;
    s.isLoading = false;
    s.totalCount = total;
    s.offset += loadedComments.size();
    s.hasMore = s.offset < total;
    s.error = "";
    s.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("CommentStore", "Loaded " + juce::String(loadedComments.size()) + " comments for post");
}

//==============================================================================
void CommentStore::toggleCommentLike(const juce::String &commentId, bool shouldLike) {
  if (networkClient == nullptr) {
    Util::logWarning("CommentStore", "Cannot toggle comment like - networkClient null");
    return;
  }

  Util::logInfo("CommentStore",
                "Toggling like on comment: " + commentId + ", liked: " + (shouldLike ? "true" : "false"));

  // Optimistic update
  updateState([commentId, shouldLike](CommentState &s) {
    for (auto &comment : s.comments) {
      if (comment.id == commentId) {
        comment.isLiked = shouldLike;
        comment.likeCount += shouldLike ? 1 : -1;
        break;
      }
    }
  });

  // Send to server
  if (shouldLike) {
    networkClient->likeComment(
        commentId, [this, commentId](Outcome<juce::var> result) { handleCommentLikeToggled(commentId, true, result); });
  } else {
    networkClient->unlikeComment(commentId, [this, commentId](Outcome<juce::var> result) {
      handleCommentLikeToggled(commentId, false, result);
    });
  }
}

void CommentStore::handleCommentLikeToggled(const juce::String &commentId, bool liked, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Revert optimistic update on error
    updateState([commentId, liked](CommentState &s) {
      for (auto &comment : s.comments) {
        if (comment.id == commentId) {
          comment.isLiked = !liked;
          comment.likeCount += liked ? -1 : 1;
          break;
        }
      }
    });
    Util::logError("CommentStore", "Failed to toggle comment like: " + result.getError());
  } else {
    Util::logDebug("CommentStore", "Comment like toggled successfully");
  }
}

//==============================================================================
void CommentStore::deleteComment(const juce::String &commentId) {
  if (networkClient == nullptr) {
    Util::logWarning("CommentStore", "Cannot delete comment - networkClient null");
    return;
  }

  Util::logInfo("CommentStore", "Deleting comment: " + commentId);

  // Optimistic removal
  updateState([this, commentId](CommentState &) { removeCommentFromState(commentId); });

  // Send to server
  networkClient->deleteComment(
      commentId, [this, commentId](Outcome<juce::var> result) { handleCommentDeleted(commentId, result); });
}

void CommentStore::handleCommentDeleted([[maybe_unused]] const juce::String &commentId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Restore comment on error (re-fetch it)
    Util::logError("CommentStore", "Failed to delete comment: " + result.getError());
    refreshComments(); // Refresh to restore deleted comment
  } else {
    Util::logDebug("CommentStore", "Comment deleted successfully");
  }
}

//==============================================================================
void CommentStore::addComment(const juce::String &content, const juce::String &parentId) {
  if (networkClient == nullptr || content.isEmpty()) {
    Util::logWarning("CommentStore", "Cannot add comment - networkClient null or content empty");
    return;
  }

  auto state = getState();
  if (state.postId.isEmpty()) {
    Util::logWarning("CommentStore", "Cannot add comment - no postId set");
    return;
  }

  Util::logInfo("CommentStore", "Adding comment to post: " + state.postId);

  // Create temporary comment with placeholder ID
  juce::String tempId = "temp_" + juce::String(juce::Random().nextInt());
  Comment tempComment;
  tempComment.id = tempId;
  tempComment.postId = state.postId;
  tempComment.content = content;
  tempComment.parentId = parentId;
  tempComment.isOwnComment = true;

  // Optimistic addition
  updateState([tempComment](CommentState &s) { s.comments.insert(0, tempComment); });

  // Send to server
  networkClient->createComment(state.postId, content, parentId,
                               [this, tempId](Outcome<juce::var> result) { handleCommentCreated(result, tempId); });
}

void CommentStore::handleCommentCreated(Outcome<juce::var> result, const juce::String &tempId) {
  if (!result.isOk()) {
    // Remove temporary comment on error
    updateState([this, tempId](CommentState &) { removeCommentFromState(tempId); });
    Util::logError("CommentStore", "Failed to create comment: " + result.getError());
  } else {
    // Replace temporary comment with actual comment from server
    auto newComment = Comment::fromJson(result.getValue());
    if (newComment.isValid()) {
      updateState([this, tempId, newComment](CommentState &s) {
        removeCommentFromState(tempId);
        if (s.comments.size() > 0) {
          s.comments.insert(0, newComment);
        }
      });
      Util::logDebug("CommentStore", "Comment created successfully");
    }
  }
}

//==============================================================================
void CommentStore::updateComment(const juce::String &commentId, const juce::String &newContent) {
  if (networkClient == nullptr || newContent.isEmpty()) {
    Util::logWarning("CommentStore", "Cannot update comment - networkClient null or content empty");
    return;
  }

  Util::logInfo("CommentStore", "Updating comment: " + commentId);

  // Send to server
  networkClient->updateComment(
      commentId, newContent, [this, commentId](Outcome<juce::var> result) { handleCommentUpdated(commentId, result); });
}

void CommentStore::handleCommentUpdated([[maybe_unused]] const juce::String &commentId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    Util::logError("CommentStore", "Failed to update comment: " + result.getError());
  } else {
    auto updatedComment = Comment::fromJson(result.getValue());
    if (updatedComment.isValid()) {
      updateCommentInState(updatedComment);
      Util::logDebug("CommentStore", "Comment updated successfully");
    }
  }
}

//==============================================================================
std::optional<Comment> CommentStore::getCommentById(const juce::String &commentId) const {
  auto state = getState();
  for (const auto &comment : state.comments) {
    if (comment.id == commentId) {
      return comment;
    }
  }
  return std::nullopt;
}

//==============================================================================
void CommentStore::removeCommentFromState(const juce::String &commentId) {
  auto state = getState();
  for (int i = 0; i < state.comments.size(); ++i) {
    if (state.comments[i].id == commentId) {
      state.comments.remove(i);
      setState(state);
      return;
    }
  }
}

void CommentStore::updateCommentInState(const Comment &updatedComment) {
  auto state = getState();
  for (int i = 0; i < state.comments.size(); ++i) {
    if (state.comments[i].id == updatedComment.id) {
      state.comments.set(i, updatedComment);
      setState(state);
      return;
    }
  }
}

} // namespace Stores
} // namespace Sidechain
