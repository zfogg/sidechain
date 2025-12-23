// ==============================================================================
// CommentsClient.cpp - Comment operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::getComments(const juce::String &postId, int limit, int offset, CommentsListCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments?limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    int totalCount = 0;
    juce::var comments;

    if (result.isSuccess() && result.data.isObject()) {
      comments = result.data.getProperty("comments", juce::var());
      totalCount = static_cast<int>(result.data.getProperty("total_count", 0));
    }

    juce::MessageManager::callAsync([callback, result, comments, totalCount]() {
      if (result.isSuccess())
        callback(Outcome<std::pair<juce::var, int>>::ok({comments, totalCount}));
      else
        callback(Outcome<std::pair<juce::var, int>>::error(result.getUserFriendlyError()));
    });
  });
}

void NetworkClient::createComment(const juce::String &postId, const juce::String &content, const juce::String &parentId,
                                  CommentCallback callback) {
  Log::info("NetworkClient::createComment: Called with postId=" + postId +
            ", callback=" + juce::String(callback ? "set" : "null"));

  if (!isAuthenticated()) {
    Log::error("NetworkClient::createComment: Not authenticated!");
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Log::info("NetworkClient::createComment: Starting async task");
  Async::runVoid([this, postId, content, parentId, callback]() {
    Log::info("NetworkClient::createComment: ASYNC BLOCK RUNNING - About to make request");

    auto data = createJsonObject({{"content", content}});
    if (parentId.isNotEmpty())
      data.getDynamicObject()->setProperty("parent_id", parentId);

    juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments";
    auto result = makeRequestWithRetry(endpoint, "POST", data, true);
    Log::debug("Create comment response: " + juce::JSON::toString(result.data));
    Log::info("NetworkClient::createComment: Request completed. callback=" + juce::String(callback ? "set" : "null"));

    if (callback) {
      Log::info("NetworkClient::createComment: Callback is set, posting to message thread");
      juce::MessageManager::callAsync([callback, result]() {
        Log::info("NetworkClient::createComment: CALLBACK FIRED ON MESSAGE THREAD!");
        auto outcome = requestResultToOutcome(result);
        Log::info("NetworkClient::createComment: Created outcome, about to invoke callback");
        callback(outcome);
        Log::info("NetworkClient::createComment: Callback invoked successfully");
      });
    } else {
      Log::error("NetworkClient::createComment: Callback is null!");
    }
  });
  Log::info("NetworkClient::createComment: Async task posted");
}

void NetworkClient::getCommentReplies(const juce::String &commentId, int limit, int offset,
                                      CommentsListCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/replies?limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    int totalCount = 0;
    juce::var replies;

    if (result.isSuccess() && result.data.isObject()) {
      replies = result.data.getProperty("replies", juce::var());
      totalCount = static_cast<int>(result.data.getProperty("total_count", 0));
    }

    juce::MessageManager::callAsync([callback, result, replies, totalCount]() {
      if (result.isSuccess())
        callback(Outcome<std::pair<juce::var, int>>::ok({replies, totalCount}));
      else
        callback(Outcome<std::pair<juce::var, int>>::error(result.getUserFriendlyError()));
    });
  });
}

void NetworkClient::updateComment(const juce::String &commentId, const juce::String &content,
                                  CommentCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, content, callback]() {
    auto data = createJsonObject({{"content", content}});

    juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
    auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
    Log::debug("Update comment response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::deleteComment(const juce::String &commentId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
    Log::debug("Delete comment response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::likeComment(const juce::String &commentId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
    Log::debug("Like comment response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unlikeComment(const juce::String &commentId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
    Log::debug("Unlike comment response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::reportComment(const juce::String &commentId, const juce::String &reason,
                                  const juce::String &description, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, reason, description, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/report";
    auto data = createJsonObject({{"reason", reason}});
    if (description.isNotEmpty())
      data.getDynamicObject()->setProperty("description", description);

    auto result = makeRequestWithRetry(endpoint, "POST", data, true);
    Log::debug("Report comment response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}
