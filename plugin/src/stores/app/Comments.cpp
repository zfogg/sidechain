#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../EntityStore.h"

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Legacy Observable API (for backwards compatibility, use loadPostComments instead)

rxcpp::observable<juce::Array<juce::var>> AppStore::getCommentsObservable(const juce::String &postId, int limit,
                                                                          int offset) {
  return rxcpp::sources::create<juce::Array<juce::var>>([this, postId, limit, offset](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Cannot get comments - network client not set");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not set")));
      return;
    }

    Util::logInfo("AppStore", "Fetching comments for post: " + postId);

    networkClient->getComments(postId, limit, offset, [observer, postId](Outcome<std::pair<juce::var, int>> result) {
      if (result.isOk()) {
        auto [commentsData, totalCount] = result.getValue();
        juce::Array<juce::var> commentsList;

        // Parse comments array
        if (commentsData.isArray()) {
          for (int i = 0; i < commentsData.size(); ++i) {
            commentsList.add(commentsData[i]);
          }
        }

        Util::logInfo("AppStore", "Loaded " + juce::String(commentsList.size()) + " comments for post: " + postId);
        observer.on_next(commentsList);
        observer.on_completed();
      } else {
        Util::logError("AppStore", "Failed to get comments: " + result.getError());
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });
}

void AppStore::createComment(const juce::String &postId, const juce::String &content, const juce::String &parentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot create comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Creating comment on post: " + postId);

  networkClient->createComment(postId, content, parentId, [postId](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Comment created successfully");
      // Invalidate comments cache for this post so next load gets fresh data
    } else {
      Util::logError("AppStore", "Failed to create comment: " + result.getError());
    }
  });
}

void AppStore::deleteComment(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot delete comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Deleting comment: " + commentId);

  networkClient->deleteComment(commentId, [](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Comment deleted successfully");
      // Invalidate all comments caches since we don't know which post this came from
    } else {
      Util::logError("AppStore", "Failed to delete comment: " + result.getError());
    }
  });
}

void AppStore::likeComment(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot like comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Liking comment: " + commentId);

  networkClient->likeComment(commentId, [](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Comment liked successfully");
      // Invalidate all comments caches to refresh like counts
    } else {
      Util::logError("AppStore", "Failed to like comment: " + result.getError());
    }
  });
}

void AppStore::unlikeComment(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot unlike comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Unliking comment: " + commentId);

  networkClient->unlikeComment(commentId, [](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Comment unliked successfully");
      // Invalidate all comments caches to refresh like counts
    } else {
      Util::logError("AppStore", "Failed to unlike comment: " + result.getError());
    }
  });
}

void AppStore::updateComment(const juce::String &commentId, const juce::String &content) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot update comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Updating comment: " + commentId);

  networkClient->updateComment(commentId, content, [](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Comment updated successfully");
      // Invalidate all comments caches to refresh content
    } else {
      Util::logError("AppStore", "Failed to update comment: " + result.getError());
    }
  });
}

void AppStore::reportComment(const juce::String &commentId, const juce::String &reason,
                             const juce::String &description) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot report comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Reporting comment: " + commentId);

  networkClient->reportComment(commentId, reason, description, [](Outcome<juce::var> result) {
    if (result.isOk()) {
      Util::logInfo("AppStore", "Comment reported successfully");
    } else {
      Util::logError("AppStore", "Failed to report comment: " + result.getError());
    }
  });
}

// ==============================================================================
// New Model-Based API with EntityStore and CommentsState

void AppStore::loadPostComments(const juce::String &postId, int limit, int offset) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load comments - network client not set");
    return;
  }

  auto commentsSlice = sliceManager.getCommentsSlice();
  if (!commentsSlice) {
    Util::logError("AppStore", "Cannot load comments - comments slice not available");
    return;
  }

  // Update loading state
  commentsSlice->dispatch([postId](CommentsState &state) {
    state.isLoadingByPostId[postId.toStdString()] = true;
    state.currentLoadingPostId = postId;
  });

  Util::logInfo("AppStore", "Loading comments for post: " + postId + " (limit=" + juce::String(limit) +
                                ", offset=" + juce::String(offset) + ")");

  // Make network request
  networkClient->getComments(postId, limit, offset, [this, postId, limit](Outcome<std::pair<juce::var, int>> result) {
    auto slice = sliceManager.getCommentsSlice();
    if (!slice)
      return;

    if (result.isOk()) {
      auto [commentsData, totalCount] = result.getValue();

      // Convert juce::var to nlohmann::json and normalize comments
      std::vector<std::shared_ptr<Comment>> normalizedComments;
      if (commentsData.isArray()) {
        for (int i = 0; i < commentsData.size(); ++i) {
          try {
            auto json = nlohmann::json::parse(commentsData[i].toString().toStdString());
            auto normalized = EntityStore::getInstance().normalizeComment(json);
            if (normalized) {
              normalizedComments.push_back(normalized);
            }
          } catch (const std::exception &e) {
            Util::logError("AppStore", "Failed to parse comment: " + juce::String(e.what()));
          }
        }
      }

      Util::logInfo("AppStore", "Loaded " + juce::String(normalizedComments.size()) + " comments for post: " + postId);

      // Update CommentsState with models
      slice->dispatch([postId, normalizedComments, totalCount, limit](CommentsState &state) {
        state.commentsByPostId[postId.toStdString()] = normalizedComments;
        state.totalCountByPostId[postId.toStdString()] = totalCount;
        state.limitByPostId[postId.toStdString()] = limit;
        state.isLoadingByPostId[postId.toStdString()] = false;
        state.lastUpdatedByPostId[postId.toStdString()] = juce::Time::getCurrentTime().toMilliseconds();
        state.hasMoreByPostId[postId.toStdString()] = (int)normalizedComments.size() >= limit;
        state.commentsError.clear();
      });
    } else {
      Util::logError("AppStore", "Failed to load comments: " + result.getError());

      // Update state with error
      slice->dispatch([postId, error = result.getError()](CommentsState &state) {
        state.isLoadingByPostId[postId.toStdString()] = false;
        state.commentsError = error;
      });
    }
  });
}

std::function<void()>
AppStore::subscribeToPostComments(const juce::String &postId,
                                  std::function<void(const std::vector<std::shared_ptr<Comment>> &)> callback) {
  if (!callback) {
    Util::logError("AppStore", "Cannot subscribe - callback is null");
    return []() {};
  }

  auto commentsSlice = sliceManager.getCommentsSlice();
  if (!commentsSlice) {
    Util::logError("AppStore", "Cannot subscribe to comments - comments slice not available");
    return []() {};
  }

  // Subscribe to CommentsState changes and invoke callback with state
  commentsSlice->subscribe([callback, postId](const CommentsState &state) {
    auto commentsIt = state.commentsByPostId.find(postId.toStdString());
    if (commentsIt != state.commentsByPostId.end()) {
      callback(commentsIt->second);
    }
  });

  // StateSlice doesn't support unsubscription, return no-op
  return []() {};
}

std::function<void()> AppStore::subscribeToComment(const juce::String &commentId,
                                                   std::function<void(const std::shared_ptr<Comment> &)> callback) {
  if (!callback) {
    Util::logError("AppStore", "Cannot subscribe - callback is null");
    return []() {};
  }

  auto &entityStore = EntityStore::getInstance();

  // Return immediate value from cache if available
  auto cachedComment = entityStore.comments().get(commentId);
  if (cachedComment) {
    callback(cachedComment);
  }

  // Subscribe to EntityStore changes for this comment
  std::function<void()> unsubscriber =
      entityStore.comments().subscribe(commentId, [callback](const std::shared_ptr<Comment> &comment) {
        // Callback receives shared_ptr directly from EntityCache
        callback(comment);
      });

  return unsubscriber;
}

} // namespace Stores
} // namespace Sidechain
