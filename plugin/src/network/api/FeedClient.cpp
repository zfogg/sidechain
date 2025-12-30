// ==============================================================================
// FeedClient.cpp - Feed operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/FeedPost.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

// ==============================================================================
// Helper function to parse feed response into typed FeedResult
static NetworkClient::FeedResult parseFeedResponse(const juce::var &json) {
  NetworkClient::FeedResult result;

  if (!json.isObject()) {
    return result;
  }

  // Try "activities" first (unified feed format), then "posts" (fallback)
  auto postsArray = json.getProperty("activities", juce::var());
  if (!postsArray.isArray()) {
    postsArray = json.getProperty("posts", juce::var());
  }

  // Extract total from meta.count or total field
  auto metaObj = json.getProperty("meta", juce::var());
  if (metaObj.isObject() && metaObj.hasProperty("count")) {
    result.total = static_cast<int>(metaObj.getProperty("count", 0));
  } else {
    result.total = static_cast<int>(json.getProperty("total", 0));
  }

  // Extract has_more flag for pagination
  if (metaObj.isObject()) {
    result.hasMore = metaObj.getProperty("has_more", false);
  }

  // Parse each post into typed FeedPost
  if (postsArray.isArray()) {
    result.posts.reserve(static_cast<size_t>(postsArray.size()));
    for (int i = 0; i < postsArray.size(); ++i) {
      try {
        auto jsonStr = juce::JSON::toString(postsArray[i]);
        auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
        Sidechain::FeedPost post;
        from_json(jsonObj, post);
        if (post.isValid()) {
          result.posts.push_back(std::move(post));
        }
      } catch (const std::exception &e) {
        Log::warn("FeedClient: Failed to parse post: " + juce::String(e.what()));
      }
    }
  }

  Log::debug("FeedClient: Parsed " + juce::String(static_cast<int>(result.posts.size())) + " posts from response");
  return result;
}

// ==============================================================================
void NetworkClient::getGlobalFeed(int limit, int offset, FeedCallback callback) {
  if (!isAuthenticated())
    return;

  Async::runVoid([this, limit, offset, callback]() {
    // Use enriched endpoint to get reaction counts and own reactions from
    // getstream.io
    juce::String endpoint =
        buildApiPath("/feed/global/enriched") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync(
          [callback, response]() { callback(parseJsonResponse(response, "Invalid feed response")); });
    }
  });
}

void NetworkClient::getTimelineFeed(int limit, int offset, FeedCallback callback) {
  if (!isAuthenticated())
    return;

  Async::runVoid([this, limit, offset, callback]() {
    // Use unified endpoint which combines followed users, Gorse
    // recommendations, trending, and recent posts This ensures users always see
    // content even when not following anyone
    juce::String endpoint =
        buildApiPath("/feed/unified") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync(
          [callback, response]() { callback(parseJsonResponse(response, "Invalid feed response")); });
    }
  });
}

void NetworkClient::getTrendingFeed(int limit, int offset, FeedCallback callback) {
  if (!isAuthenticated())
    return;

  Async::runVoid([this, limit, offset, callback]() {
    // Trending feed uses engagement scoring (likes, plays, comments weighted by
    // recency)
    juce::String endpoint =
        buildApiPath("/feed/trending") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync(
          [callback, response]() { callback(parseJsonResponse(response, "Invalid feed response")); });
    }
  });
}

void NetworkClient::getForYouFeed(int limit, int offset, FeedCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    // For You feed uses personalized recommendations
    juce::String endpoint =
        buildApiPath("/recommendations/for-you") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync(
          [callback, response]() { callback(parseJsonResponse(response, "Invalid feed response")); });
    }
  });
}

void NetworkClient::getSimilarPosts(const juce::String &postId, int limit, FeedCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, limit, callback]() {
    juce::String path = "/recommendations/similar-posts/" + postId;
    juce::String endpoint = buildApiPath(path.toRawUTF8()) + "?limit=" + juce::String(limit);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync(
          [callback, response]() { callback(parseJsonResponse(response, "Invalid feed response")); });
    }
  });
}

void NetworkClient::likePost(const juce::String &activityId, const juce::String &emoji, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, activityId, emoji, callback]() {
    auto data = createJsonObject({{"activity_id", activityId}});
    if (!emoji.isEmpty())
      data.getDynamicObject()->setProperty("emoji", emoji);

    auto result = makeRequestWithRetry(buildApiPath("/feed/like"), "POST", data, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unlikePost(const juce::String &activityId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, activityId, callback]() {
    auto data = createJsonObject({{"activity_id", activityId}});
    auto result = makeRequestWithRetry(buildApiPath("/feed/unlike"), "POST", data, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::deletePost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = buildApiPath("/posts/") + postId;
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::reportPost(const juce::String &postId, const juce::String &reason, const juce::String &description,
                               ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, reason, description, callback]() {
    auto data = createJsonObject({{"post_id", postId}, {"reason", reason}});
    if (!description.isEmpty())
      data.getDynamicObject()->setProperty("description", description);

    auto result = makeRequestWithRetry(buildApiPath("/reports"), "POST", data, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getRemixChain(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = buildApiPath("/posts/") + postId + "/remix-chain";
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getPostRemixes(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = buildApiPath("/posts/") + postId + "/remixes";
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getRemixSource(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = buildApiPath("/posts/") + postId + "/remix-source";
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::createRemixPost(const juce::String &sourcePostId, const juce::String &remixType,
                                    ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, sourcePostId, remixType, callback]() {
    auto data = createJsonObject({{"source_post_id", sourcePostId}, {"remix_type", remixType}});
    auto result = makeRequestWithRetry(buildApiPath("/remixes"), "POST", data, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::trackRecommendationClick(const juce::String &postId, const juce::String &source, int position,
                                             double playDuration, bool completed, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, source, position, playDuration, completed, callback]() {
    juce::var data = juce::var(new juce::DynamicObject());
    auto *obj = data.getDynamicObject();
    if (obj != nullptr) {
      obj->setProperty("post_id", postId);
      obj->setProperty("source", source);
      obj->setProperty("position", position);
      if (playDuration > 0.0)
        obj->setProperty("play_duration", playDuration);
      obj->setProperty("completed", completed);
    }

    auto result = makeRequestWithRetry(buildApiPath("/recommendations/click"), "POST", data, true);
    Log::debug("Track recommendation click response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Model-based feed endpoints (Phase 3 refactoring - returns shared_ptr models)

void NetworkClient::getGlobalFeedModels(int limit, int offset, FeedPostsCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/feed/global/enriched") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, response]() {
        // Check for error in response
        if (response.isObject() && response.hasProperty("error")) {
          juce::String errorMsg = response["error"].toString();
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(errorMsg));
        } else if (response.isArray() || response.isObject()) {
          // Parse the response into shared_ptr FeedPost objects
          auto outcome = parseFeedPostsResponse(response);
          callback(outcome);
        } else {
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error("Invalid feed response format"));
        }
      });
    }
  });
}

void NetworkClient::getTimelineFeedModels(int limit, int offset, FeedPostsCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/feed/unified") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, response]() {
        // Check for error in response
        if (response.isObject() && response.hasProperty("error")) {
          juce::String errorMsg = response["error"].toString();
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(errorMsg));
        } else if (response.isArray() || response.isObject()) {
          // Parse the response into shared_ptr FeedPost objects
          auto outcome = parseFeedPostsResponse(response);
          callback(outcome);
        } else {
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error("Invalid feed response format"));
        }
      });
    }
  });
}

void NetworkClient::getTrendingFeedModels(int limit, int offset, FeedPostsCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/feed/trending") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, response]() {
        // Check for error in response
        if (response.isObject() && response.hasProperty("error")) {
          juce::String errorMsg = response["error"].toString();
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(errorMsg));
        } else if (response.isArray() || response.isObject()) {
          // Parse the response into shared_ptr FeedPost objects
          auto outcome = parseFeedPostsResponse(response);
          callback(outcome);
        } else {
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error("Invalid feed response format"));
        }
      });
    }
  });
}

void NetworkClient::getForYouFeedModels(int limit, int offset, FeedPostsCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/recommendations/for-you") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto response = makeRequest(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, response]() {
        // Check for error in response
        if (response.isObject() && response.hasProperty("error")) {
          juce::String errorMsg = response["error"].toString();
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error(errorMsg));
        } else if (response.isArray() || response.isObject()) {
          // Parse the response into shared_ptr FeedPost objects
          auto outcome = parseFeedPostsResponse(response);
          callback(outcome);
        } else {
          callback(Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error("Invalid feed response format"));
        }
      });
    }
  });
}

// ==============================================================================
// Reactive Observable Methods (Phase 5)
// ==============================================================================
//
// These methods wrap the callback-based methods in RxCpp observables,
// providing a reactive API for feed operations.

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getGlobalFeedObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getGlobalFeed(limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseFeedResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  // Apply retry with backoff for transient network failures
  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getTimelineFeedObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getTimelineFeed(limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseFeedResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getTrendingFeedObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getTrendingFeed(limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseFeedResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getForYouFeedObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getForYouFeed(limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseFeedResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getPopularFeedObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getPopularFeed(limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseFeedResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getLatestFeedObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getLatestFeed(limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseFeedResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getDiscoveryFeedObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getDiscoveryFeed(limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseFeedResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::LikeResult> NetworkClient::likePostObservable(const juce::String &activityId,
                                                                               const juce::String &emoji) {
  auto source = rxcpp::sources::create<LikeResult>([this, activityId, emoji](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    likePost(activityId, emoji, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        LikeResult likeResult;
        auto value = result.getValue();
        if (value.isObject()) {
          likeResult.likeCount = Json::getInt(value, "like_count", Json::getInt(value, "likeCount", 0));
          likeResult.isLiked = Json::getBool(value, "is_liked", Json::getBool(value, "isLiked", true));
        } else {
          // Default to liked state if response doesn't include details
          likeResult.isLiked = true;
        }
        observer.on_next(likeResult);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<juce::var> NetworkClient::unlikePostObservable(const juce::String &activityId) {
  auto source = rxcpp::sources::create<juce::var>([this, activityId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    unlikePost(activityId, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        observer.on_next(result.getValue());
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}
