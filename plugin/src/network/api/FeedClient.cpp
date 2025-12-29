// ==============================================================================
// FeedClient.cpp - Feed operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

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
