// ==============================================================================
// CommentsClient.cpp - Comment operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../models/Comment.h"
#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

// ==============================================================================
// Helper functions for typed Comment parsing

/** Parse a single Comment from nlohmann::json */
static Sidechain::Comment parseCommentFromJson(const nlohmann::json &json) {
  Sidechain::Comment comment;
  if (!json.is_object())
    return comment;

  try {
    from_json(json, comment);
  } catch (const std::exception &e) {
    Log::warn("CommentsClient: Failed to parse comment: " + juce::String(e.what()));
  }

  return comment;
}

/** Parse a CommentResult from API response */
static NetworkClient::CommentResult parseCommentListResponse(const nlohmann::json &json) {
  NetworkClient::CommentResult result;

  if (!json.is_object())
    return result;

  result.total = json.value("total_count", 0);
  result.hasMore = json.value("has_more", false);

  if (json.contains("comments") && json["comments"].is_array()) {
    const auto &commentsArray = json["comments"];
    result.comments.reserve(commentsArray.size());
    for (const auto &commentJson : commentsArray) {
      result.comments.push_back(parseCommentFromJson(commentJson));
    }
  }

  return result;
}

// ==============================================================================
void NetworkClient::getComments(const juce::String &postId, int limit, int offset, CommentsListCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments?limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);

    int totalCount = 0;
    nlohmann::json comments;

    if (result.isSuccess() && result.data.is_object()) {
      comments = result.data.value("comments", nlohmann::json::array());
      totalCount = result.data.value("total_count", 0);
    }

    juce::MessageManager::callAsync([callback, result, comments, totalCount]() {
      if (result.isSuccess())
        callback(Outcome<std::pair<nlohmann::json, int>>::ok({comments, totalCount}));
      else
        callback(Outcome<std::pair<nlohmann::json, int>>::error(result.getUserFriendlyError()));
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
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Log::info("NetworkClient::createComment: Starting async task");
  Async::runVoid([this, postId, content, parentId, callback]() {
    Log::info("NetworkClient::createComment: ASYNC BLOCK RUNNING - About to make request");

    nlohmann::json data;
    data["content"] = content.toStdString();
    if (parentId.isNotEmpty())
      data["parent_id"] = parentId.toStdString();

    juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments";
    auto result = makeRequestWithRetry(endpoint, "POST", data, true);
    Log::debug("Create comment response: " + juce::String(result.data.dump()));
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
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);

    int totalCount = 0;
    nlohmann::json replies;

    if (result.isSuccess() && result.data.is_object()) {
      replies = result.data.value("replies", nlohmann::json::array());
      totalCount = result.data.value("total_count", 0);
    }

    juce::MessageManager::callAsync([callback, result, replies, totalCount]() {
      if (result.isSuccess())
        callback(Outcome<std::pair<nlohmann::json, int>>::ok({replies, totalCount}));
      else
        callback(Outcome<std::pair<nlohmann::json, int>>::error(result.getUserFriendlyError()));
    });
  });
}

void NetworkClient::updateComment(const juce::String &commentId, const juce::String &content,
                                  CommentCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, content, callback]() {
    nlohmann::json data;
    data["content"] = content.toStdString();

    juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
    auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
    Log::debug("Update comment response: " + juce::String(result.data.dump()));

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
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);
    Log::debug("Delete comment response: " + juce::String(result.data.dump()));

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
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);
    Log::debug("Like comment response: " + juce::String(result.data.dump()));

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
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);
    Log::debug("Unlike comment response: " + juce::String(result.data.dump()));

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
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, commentId, reason, description, callback]() {
    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/report";
    nlohmann::json data;
    data["reason"] = reason.toStdString();
    if (description.isNotEmpty())
      data["description"] = description.toStdString();

    auto result = makeRequestWithRetry(endpoint, "POST", data, true);
    Log::debug("Report comment response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Reactive Observable Methods (Phase 5)
// ==============================================================================

rxcpp::observable<NetworkClient::CommentResult> NetworkClient::getCommentsObservable(const juce::String &postId,
                                                                                     int limit, int offset) {
  auto source = rxcpp::sources::create<CommentResult>([this, postId, limit, offset](auto observer) {
    juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments?limit=" + juce::String(limit) +
                            "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, observer]() {
      auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);

      if (result.isSuccess() && result.data.is_object()) {
        auto commentResult = parseCommentListResponse(result.data);
        juce::MessageManager::callAsync([observer, commentResult]() {
          observer.on_next(commentResult);
          observer.on_completed();
        });
      } else {
        juce::MessageManager::callAsync([observer, result]() {
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getUserFriendlyError().toStdString())));
        });
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<Sidechain::Comment> NetworkClient::createCommentObservable(const juce::String &postId,
                                                                             const juce::String &content,
                                                                             const juce::String &parentId) {
  auto source = rxcpp::sources::create<Sidechain::Comment>([this, postId, content, parentId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    createComment(postId, content, parentId, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        auto comment = parseCommentFromJson(result.getValue());
        observer.on_next(comment);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::deleteCommentObservable(const juce::String &commentId) {
  auto source = rxcpp::sources::create<int>([this, commentId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    deleteComment(commentId, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        observer.on_next(0);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::likeCommentObservable(const juce::String &commentId) {
  auto source = rxcpp::sources::create<int>([this, commentId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    likeComment(commentId, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        observer.on_next(0);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::unlikeCommentObservable(const juce::String &commentId) {
  auto source = rxcpp::sources::create<int>([this, commentId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    unlikeComment(commentId, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        observer.on_next(0);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<Sidechain::Comment> NetworkClient::updateCommentObservable(const juce::String &commentId,
                                                                             const juce::String &content) {
  auto source = rxcpp::sources::create<Sidechain::Comment>([this, commentId, content](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    updateComment(commentId, content, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        auto comment = parseCommentFromJson(result.getValue());
        observer.on_next(comment);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::reportCommentObservable(const juce::String &commentId, const juce::String &reason,
                                                              const juce::String &description) {
  auto source = rxcpp::sources::create<int>([this, commentId, reason, description](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    reportComment(commentId, reason, description, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        observer.on_next(0);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}
