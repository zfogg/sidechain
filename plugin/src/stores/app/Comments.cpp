#include "../AppStore.h"
#include "../../network/NetworkClient.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include "../EntityStore.h"
#include "../../models/Comment.h"
#include <rxcpp/rx.hpp>
#include <nlohmann/json.hpp>

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

    networkClient->getComments(
        postId, limit, offset, [observer, postId](Outcome<std::pair<nlohmann::json, int>> result) {
          if (result.isOk()) {
            auto [commentsData, totalCount] = result.getValue();
            juce::Array<juce::var> commentsList;

            // Parse comments array
            if (commentsData.is_array()) {
              for (const auto &item : commentsData) {
                commentsList.add(juce::var(item.dump()));
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

  auto commentsState = stateManager.comments;
  if (!commentsState) {
    Util::logError("AppStore", "Cannot create comment - comments slice not available");
    return;
  }

  Util::logInfo("AppStore", "Creating comment on post: " + postId);

  networkClient->createCommentObservable(postId, content, parentId)
      .subscribe(
          [this, postId](const Comment &createdComment) {
            auto slice = stateManager.comments;
            if (!slice) {
              Util::logError("AppStore", "Cannot update comments state - comments slice is null");
              return;
            }

            // Normalize comment to shared_ptr for storage
            auto normalizedComment = std::make_shared<Comment>(createdComment);

            // Also add to EntityStore for cache access
            EntityStore::getInstance().comments().set(createdComment.id, normalizedComment);

            Util::logInfo("AppStore", "Comment created successfully with ID: " + juce::String(normalizedComment->id));

            // Update CommentsState - add new comment to the post's comments list
            CommentsState newState = slice->getState();
            auto commentsIt = newState.commentsByPostId.find(postId.toStdString());
            if (commentsIt != newState.commentsByPostId.end()) {
              commentsIt->second.insert(commentsIt->second.begin(), normalizedComment);
              newState.totalCountByPostId[postId.toStdString()]++;
            } else {
              Util::logWarning("AppStore",
                               "Post " + postId + " not found in commentsByPostId map, initializing empty list");
              newState.commentsByPostId[postId.toStdString()] = {normalizedComment};
              newState.totalCountByPostId[postId.toStdString()] = 1;
            }
            newState.commentsError.clear();
            slice->setState(newState);
          },
          [this](std::exception_ptr ep) {
            auto slice = stateManager.comments;
            if (!slice)
              return;

            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Failed to create comment: " + juce::String(e.what()));

              CommentsState errorState = slice->getState();
              errorState.commentsError = "API Error: " + juce::String(e.what());
              slice->setState(errorState);
            }
          });
}

void AppStore::deleteComment(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot delete comment - network client not set");
    return;
  }

  auto commentsState = stateManager.comments;
  if (!commentsState) {
    Util::logError("AppStore", "Cannot delete comment - comments slice not available");
    return;
  }

  Util::logInfo("AppStore", "Deleting comment: " + commentId);

  // Find which post this comment belongs to
  auto currentState = commentsState->getState();
  juce::String postId;
  for (const auto &[pId, comments] : currentState.commentsByPostId) {
    for (const auto &comment : comments) {
      if (comment->id == commentId) {
        postId = juce::String(pId);
        break;
      }
    }
    if (postId.isNotEmpty())
      break;
  }

  networkClient->deleteCommentObservable(commentId).subscribe(
      [this, commentId, postId](int) {
        auto slice = stateManager.comments;
        if (!slice)
          return;

        Util::logInfo("AppStore", "Comment deleted successfully: " + commentId);

        // Update CommentsState - remove comment from the post's comments list
        if (postId.isNotEmpty()) {
          CommentsState deleteState = slice->getState();
          auto commentsIt = deleteState.commentsByPostId.find(postId.toStdString());
          if (commentsIt != deleteState.commentsByPostId.end()) {
            auto &comments = commentsIt->second;
            comments.erase(
                std::remove_if(comments.begin(), comments.end(),
                               [commentId](const std::shared_ptr<Comment> &c) { return c->id == commentId; }),
                comments.end());

            deleteState.totalCountByPostId[postId.toStdString()]--;
          }
          deleteState.commentsError.clear();
          slice->setState(deleteState);
        }

        // Remove from EntityStore
        EntityStore::getInstance().comments().remove(commentId);
      },
      [this](std::exception_ptr ep) {
        auto slice = stateManager.comments;
        if (!slice)
          return;

        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to delete comment: " + juce::String(e.what()));

          CommentsState errorState = slice->getState();
          errorState.commentsError = juce::String(e.what());
          slice->setState(errorState);
        }
      });
}

void AppStore::likeComment(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot like comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Liking comment: " + commentId);

  // Optimistic update in EntityStore
  bool updated = EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
    comment.isLiked = true;
    comment.likeCount++;
  });

  if (!updated) {
    Util::logWarning("AppStore", "Cannot like comment - comment not found in cache: " + commentId);
  }

  // Make network request using observable
  networkClient->likeCommentObservable(commentId).subscribe(
      [commentId](int) { Util::logInfo("AppStore", "Comment liked successfully: " + commentId); },
      [commentId](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to like comment: " + juce::String(e.what()));

          // Rollback optimistic update
          EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
            comment.isLiked = false;
            comment.likeCount--;
          });
        }
      });
}

void AppStore::unlikeComment(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot unlike comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Unliking comment: " + commentId);

  // Optimistic update in EntityStore
  bool updated = EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
    comment.isLiked = false;
    comment.likeCount--;
  });

  if (!updated) {
    Util::logWarning("AppStore", "Cannot unlike comment - comment not found in cache: " + commentId);
  }

  // Make network request using observable
  networkClient->unlikeCommentObservable(commentId).subscribe(
      [commentId](int) { Util::logInfo("AppStore", "Comment unliked successfully: " + commentId); },
      [commentId](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to unlike comment: " + juce::String(e.what()));

          // Rollback optimistic update
          EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
            comment.isLiked = true;
            comment.likeCount++;
          });
        }
      });
}

void AppStore::updateComment(const juce::String &commentId, const juce::String &content) {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot update comment - network client not set");
    return;
  }

  Util::logInfo("AppStore", "Updating comment: " + commentId);

  // Store original content for rollback
  juce::String originalContent;
  auto &entityStore = EntityStore::getInstance();
  auto currentComment = entityStore.comments().get(commentId);
  if (currentComment) {
    originalContent = juce::String(currentComment->content);
  }

  // Optimistic update in EntityStore
  bool updated = entityStore.comments().update(
      commentId, [&content](Comment &comment) { comment.content = content.toStdString(); });

  if (!updated) {
    Util::logWarning("AppStore", "Cannot update comment - comment not found in cache: " + commentId);
  }

  // Make network request using observable
  networkClient->updateCommentObservable(commentId, content)
      .subscribe(
          [commentId](const Comment &updatedComment) {
            Util::logInfo("AppStore", "Comment updated successfully: " + commentId);

            // Update EntityStore with the new comment data
            auto commentPtr = std::make_shared<Comment>(updatedComment);
            EntityStore::getInstance().comments().set(commentId, commentPtr);
          },
          [commentId, originalContent](std::exception_ptr ep) {
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Failed to update comment: " + juce::String(e.what()));

              // Rollback optimistic update
              EntityStore::getInstance().comments().update(
                  commentId, [&originalContent](Comment &comment) { comment.content = originalContent.toStdString(); });
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

  // Make network request using observable
  networkClient->reportCommentObservable(commentId, reason, description)
      .subscribe([commentId](int) { Util::logInfo("AppStore", "Comment reported successfully: " + commentId); },
                 [commentId](std::exception_ptr ep) {
                   try {
                     std::rethrow_exception(ep);
                   } catch (const std::exception &e) {
                     Util::logError("AppStore", "Failed to report comment: " + juce::String(e.what()));
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

  auto commentsState = stateManager.comments;
  if (!commentsState) {
    Util::logError("AppStore", "Cannot load comments - comments slice not available");
    return;
  }

  // Update loading state
  CommentsState loadingState = commentsState->getState();
  loadingState.isLoadingByPostId[postId.toStdString()] = true;
  loadingState.currentLoadingPostId = postId;
  commentsState->setState(loadingState);

  Util::logInfo("AppStore", "Loading comments for post: " + postId + " (limit=" + juce::String(limit) +
                                ", offset=" + juce::String(offset) + ")");

  // Make network request using observable
  networkClient->getCommentsObservable(postId, limit, offset)
      .subscribe(
          [this, postId, limit](const NetworkClient::CommentResult &result) {
            auto slice = stateManager.comments;
            if (!slice)
              return;

            // Convert typed comments to shared_ptr and add to EntityStore
            std::vector<std::shared_ptr<Comment>> normalizedComments;
            normalizedComments.reserve(result.comments.size());
            for (const auto &comment : result.comments) {
              auto commentPtr = std::make_shared<Comment>(comment);
              EntityStore::getInstance().comments().set(comment.id, commentPtr);
              normalizedComments.push_back(commentPtr);
            }

            Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(normalizedComments.size())) +
                                          " comments for post: " + postId);

            // Update CommentsState with models
            CommentsState successState = slice->getState();
            successState.commentsByPostId[postId.toStdString()] = normalizedComments;
            successState.totalCountByPostId[postId.toStdString()] = result.total;
            successState.limitByPostId[postId.toStdString()] = limit;
            successState.isLoadingByPostId[postId.toStdString()] = false;
            successState.lastUpdatedByPostId[postId.toStdString()] = juce::Time::getCurrentTime().toMilliseconds();
            successState.hasMoreByPostId[postId.toStdString()] =
                static_cast<int>(normalizedComments.size()) >= limit || result.hasMore;
            successState.commentsError.clear();
            slice->setState(successState);
          },
          [this, postId](std::exception_ptr ep) {
            auto slice = stateManager.comments;
            if (!slice)
              return;

            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Failed to load comments: " + juce::String(e.what()));

              // Update state with error
              CommentsState errorState = slice->getState();
              errorState.isLoadingByPostId[postId.toStdString()] = false;
              errorState.commentsError = juce::String(e.what());
              slice->setState(errorState);
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

  auto commentsState = stateManager.comments;
  if (!commentsState) {
    Util::logError("AppStore", "Cannot subscribe to comments - comments slice not available");
    return []() {};
  }

  // Subscribe to CommentsState changes and invoke callback with state
  commentsState->subscribe([callback, postId](const CommentsState &state) {
    auto commentsIt = state.commentsByPostId.find(postId.toStdString());
    if (commentsIt != state.commentsByPostId.end()) {
      callback(commentsIt->second);
    }
  });

  // StateSubject doesn't support unsubscription, return no-op
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

// ==============================================================================
// Reactive Comment Observables (Phase 6)
// ==============================================================================
//
// These methods return rxcpp::observable with proper model types (Comment values,
// not shared_ptr). They use the same network calls as the Redux actions above
// but wrap them in reactive streams for compose-able operations.

rxcpp::observable<std::vector<Comment>> AppStore::loadCommentsObservable(const juce::String &postId, int limit,
                                                                         int offset) {
  using ResultType = std::vector<Comment>;

  if (!networkClient) {
    Util::logError("AppStore", "Network client not initialized");
    return rxcpp::sources::error<ResultType>(
        std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  Util::logDebug("AppStore", "Loading comments via observable for post: " + postId);

  // Use the network client's observable API and transform the result
  return networkClient->getCommentsObservable(postId, limit, offset)
      .map([postId](const NetworkClient::CommentResult &result) {
        Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(result.comments.size())) +
                                      " comments for post: " + postId);
        return result.comments;
      });
}

rxcpp::observable<int> AppStore::likeCommentObservable(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Network client not initialized");
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  Util::logDebug("AppStore", "Liking comment via observable: " + commentId);

  // Optimistic update in EntityStore
  bool updated = EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
    comment.isLiked = true;
    comment.likeCount++;
  });

  if (!updated) {
    Util::logWarning("AppStore", "Cannot like comment - comment not found in cache: " + commentId);
  }

  // Use the network client's observable API
  return networkClient->likeCommentObservable(commentId)
      .map([commentId](int) {
        Util::logInfo("AppStore", "Comment liked successfully: " + commentId);
        return 0;
      })
      .on_error_resume_next([commentId](std::exception_ptr ep) {
        // Rollback optimistic update
        EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
          comment.isLiked = false;
          comment.likeCount--;
        });

        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to like comment: " + juce::String(e.what()));
        }

        return rxcpp::sources::error<int>(ep);
      });
}

rxcpp::observable<int> AppStore::unlikeCommentObservable(const juce::String &commentId) {
  if (!networkClient) {
    Util::logError("AppStore", "Network client not initialized");
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  Util::logDebug("AppStore", "Unliking comment via observable: " + commentId);

  // Optimistic update in EntityStore
  bool updated = EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
    comment.isLiked = false;
    comment.likeCount--;
  });

  if (!updated) {
    Util::logWarning("AppStore", "Cannot unlike comment - comment not found in cache: " + commentId);
  }

  // Use the network client's observable API
  return networkClient->unlikeCommentObservable(commentId)
      .map([commentId](int) {
        Util::logInfo("AppStore", "Comment unliked successfully: " + commentId);
        return 0;
      })
      .on_error_resume_next([commentId](std::exception_ptr ep) {
        // Rollback optimistic update
        EntityStore::getInstance().comments().update(commentId, [](Comment &comment) {
          comment.isLiked = true;
          comment.likeCount++;
        });

        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to unlike comment: " + juce::String(e.what()));
        }

        return rxcpp::sources::error<int>(ep);
      });
}

} // namespace Stores
} // namespace Sidechain
