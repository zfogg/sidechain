#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

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

} // namespace Stores
} // namespace Sidechain
