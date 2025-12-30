// ==============================================================================
// SocialClient.cpp - Social operations (follow, unfollow, play tracking)
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../models/FeedPost.h"
#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Json.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::followUser(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    nlohmann::json data = {{"target_user_id", userId.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/social/follow"), "POST", data, true);
    Log::debug("Follow response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unfollowUser(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    nlohmann::json data = {{"target_user_id", userId.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/social/unfollow"), "POST", data, true);
    Log::debug("Unfollow response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::blockUser(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    nlohmann::json data = {{"target_user_id", userId.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/social/block"), "POST", data, true);
    Log::debug("Block response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unblockUser(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    nlohmann::json data = {{"target_user_id", userId.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/social/unblock"), "POST", data, true);
    Log::debug("Unblock response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::trackPlay(const juce::String &activityId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, activityId, callback]() {
    nlohmann::json data = {{"activity_id", activityId.toStdString()}};

    auto result = makeRequestWithRetry(buildApiPath("/social/play"), "POST", data, true);
    Log::debug("Track play response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::trackListenDuration(const juce::String &activityId, double durationSeconds,
                                        ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  // Only track if duration is meaningful (at least 1 second)
  if (durationSeconds < 1.0) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, activityId, durationSeconds, callback]() {
    nlohmann::json data = {{"activity_id", activityId.toStdString()}, {"duration", durationSeconds}};

    auto result = makeRequestWithRetry(buildApiPath("/social/listen-duration"), "POST", data, true);
    Log::debug("Track listen duration response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Save/Bookmark operations (P0 Social Feature)
// ==============================================================================

void NetworkClient::savePost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/save";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);
    Log::debug("Save post response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unsavePost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/save";
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);
    Log::debug("Unsave post response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getSavedPosts(int limit, int offset, FeedCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/users/me/saved") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);
    Log::debug("Get saved posts response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Repost operations (P0 Social Feature)
// ==============================================================================

void NetworkClient::repostPost(const juce::String &postId, const juce::String &quote, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, quote, callback]() {
    nlohmann::json data = nlohmann::json::object();
    if (quote.isNotEmpty()) {
      data["quote"] = quote.toStdString();
    }

    juce::String endpoint = "/api/v1/posts/" + postId + "/repost";
    auto result = makeRequestWithRetry(endpoint, "POST", data, true);
    Log::debug("Repost response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::undoRepost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/repost";
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);
    Log::debug("Undo repost response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Archive operations (hide posts without deleting)
// ==============================================================================

void NetworkClient::archivePost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/archive";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);
    Log::debug("Archive post response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unarchivePost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/unarchive";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);
    Log::debug("Unarchive post response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getArchivedPosts(int limit, int offset, FeedCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/users/me/archived") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);
    Log::debug("Get archived posts response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Mute operations
// ==============================================================================

void NetworkClient::muteUser(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = buildApiPath("/users/") + userId + "/mute";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);
    Log::debug("Mute user response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unmuteUser(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = buildApiPath("/users/") + userId + "/mute";
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);
    Log::debug("Unmute user response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getMutedUsers(int limit, int offset, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/users/me/muted") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);
    Log::debug("Get muted users response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::isUserMuted(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = buildApiPath("/users/") + userId + "/muted";
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);
    Log::debug("Is user muted response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ==============================================================================
// Pin posts to profile operations
// ==============================================================================

void NetworkClient::pinPost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/pin";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);
    Log::debug("Pin post response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::unpinPost(const juce::String &postId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/pin";
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);
    Log::debug("Unpin post response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::updatePinOrder(const juce::String &postId, int order, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, order, callback]() {
    nlohmann::json data = {{"order", order}};

    juce::String endpoint = "/api/v1/posts/" + postId + "/pin-order";
    auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
    Log::debug("Update pin order response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::isPostPinned(const juce::String &postId, ResponseCallback callback) {
  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/pinned";
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), false);
    Log::debug("Is post pinned response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

// ============================================================================
// Sound/Sample API
// ============================================================================

void NetworkClient::getSound(const juce::String &soundId, ResponseCallback callback) {
  Async::runVoid([this, soundId, callback]() {
    juce::String endpoint = "/api/v1/sounds/" + soundId;
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), false);
    Log::debug("Get sound response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getSoundPosts(const juce::String &soundId, int limit, int offset, ResponseCallback callback) {
  Async::runVoid([this, soundId, limit, offset, callback]() {
    juce::String endpoint =
        "/api/v1/sounds/" + soundId + "/posts?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), false);
    Log::debug("Get sound posts response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getTrendingSounds(int limit, ResponseCallback callback) {
  Async::runVoid([this, limit, callback]() {
    juce::String endpoint = "/api/v1/sounds/trending?limit=" + juce::String(limit);
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), false);
    Log::debug("Get trending sounds response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::searchSounds(const juce::String &query, int limit, ResponseCallback callback) {
  Async::runVoid([this, query, limit, callback]() {
    juce::String endpoint =
        "/api/v1/sounds/search?q=" + juce::URL::addEscapeChars(query, true) + "&limit=" + juce::String(limit);
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), false);
    Log::debug("Search sounds response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getSoundForPost(const juce::String &postId, ResponseCallback callback) {
  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/sound";
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), false);
    Log::debug("Get post sound response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::updateSound(const juce::String &soundId, const juce::String &name, const juce::String &description,
                                bool isPublic, ResponseCallback callback) {
  Async::runVoid([this, soundId, name, description, isPublic, callback]() {
    juce::String endpoint = "/api/v1/sounds/" + soundId;

    nlohmann::json body = nlohmann::json::object();
    if (name.isNotEmpty())
      body["name"] = name.toStdString();
    if (description.isNotEmpty())
      body["description"] = description.toStdString();
    body["is_public"] = isPublic;

    auto result = makeRequestWithRetry(endpoint, "PATCH", body, true);
    Log::debug("Update sound response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::toggleLike(const juce::String &postId, bool shouldLike, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  nlohmann::json data = {{"activity_id", postId.toStdString()}};

  juce::String method = shouldLike ? "POST" : "DELETE";

  Async::runVoid([this, method, data, callback]() {
    auto result = makeRequestWithRetry("/api/v1/social/like", method, data, true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::toggleSave(const juce::String &postId, bool shouldSave, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = "/api/v1/posts/" + postId + "/save";
  juce::String method = shouldSave ? "POST" : "DELETE";

  Async::runVoid([this, endpoint, method, callback]() {
    auto result = makeRequestWithRetry(endpoint, method, nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::toggleRepost(const juce::String &postId, bool shouldRepost, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = "/api/v1/posts/" + postId + "/repost";
  juce::String method = shouldRepost ? "POST" : "DELETE";

  Async::runVoid([this, endpoint, method, callback]() {
    auto result = makeRequestWithRetry(endpoint, method, nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::addEmojiReaction(const juce::String &postId, const juce::String &emoji, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  nlohmann::json data = {{"activity_id", postId.toStdString()}, {"emoji", emoji.toStdString()}};

  Async::runVoid([this, data, callback]() {
    auto result = makeRequestWithRetry("/api/v1/social/react", "POST", data, true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

// ==============================================================================
// Reactive Observable Methods (Phase 5)
// ==============================================================================

rxcpp::observable<NetworkClient::FollowResult> NetworkClient::followUserObservable(const juce::String &userId) {
  auto source = rxcpp::sources::create<FollowResult>([this, userId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    followUser(userId, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        FollowResult followResult;
        auto value = result.getValue();
        if (value.is_object()) {
          followResult.isFollowing = value.value("is_following", value.value("isFollowing", true));
          followResult.followerCount = value.value("follower_count", value.value("followerCount", 0));
        } else {
          // Default to following state if response doesn't include details
          followResult.isFollowing = true;
        }
        observer.on_next(followResult);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::unfollowUserObservable(const juce::String &userId) {
  auto source = rxcpp::sources::create<int>([this, userId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    unfollowUser(userId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<int> NetworkClient::savePostObservable(const juce::String &postId) {
  auto source = rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    savePost(postId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<int> NetworkClient::unsavePostObservable(const juce::String &postId) {
  auto source = rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    unsavePost(postId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<std::vector<Sidechain::FeedPost>> NetworkClient::getSavedPostsObservable(int limit, int offset) {
  using ResultType = std::vector<Sidechain::FeedPost>;
  auto source = rxcpp::sources::create<ResultType>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getSavedPosts(limit, offset, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        ResultType posts;
        auto data = result.getValue();
        // Parse posts array - check for "posts" or "saved" field, or direct array
        nlohmann::json postsArray = data;
        if (data.is_object() && data.contains("posts")) {
          postsArray = data["posts"];
        } else if (data.is_object() && data.contains("saved")) {
          postsArray = data["saved"];
        }

        if (postsArray.is_array()) {
          for (const auto &item : postsArray) {
            try {
              Sidechain::FeedPost post;
              from_json(item, post);
              posts.push_back(std::move(post));
            } catch (const std::exception &e) {
              Log::warn("NetworkClient: Failed to parse saved post: " + juce::String(e.what()));
            }
          }
        }
        observer.on_next(std::move(posts));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::repostPostObservable(const juce::String &postId, const juce::String &quote) {
  auto source = rxcpp::sources::create<int>([this, postId, quote](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    repostPost(postId, quote, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<int> NetworkClient::undoRepostObservable(const juce::String &postId) {
  auto source = rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    undoRepost(postId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<std::vector<Sidechain::FeedPost>> NetworkClient::getArchivedPostsObservable(int limit, int offset) {
  using ResultType = std::vector<Sidechain::FeedPost>;
  auto source = rxcpp::sources::create<ResultType>([this, limit, offset](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getArchivedPosts(limit, offset, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        ResultType posts;
        auto data = result.getValue();
        // Parse posts array - check for "posts" or "archived" field, or direct array
        nlohmann::json postsArray = data;
        if (data.is_object() && data.contains("posts")) {
          postsArray = data["posts"];
        } else if (data.is_object() && data.contains("archived")) {
          postsArray = data["archived"];
        }

        if (postsArray.is_array()) {
          for (const auto &item : postsArray) {
            try {
              Sidechain::FeedPost post;
              from_json(item, post);
              posts.push_back(std::move(post));
            } catch (const std::exception &e) {
              Log::warn("NetworkClient: Failed to parse archived post: " + juce::String(e.what()));
            }
          }
        }
        observer.on_next(std::move(posts));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::unarchivePostObservable(const juce::String &postId) {
  auto source = rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    unarchivePost(postId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<int> NetworkClient::muteUserObservable(const juce::String &userId) {
  auto source = rxcpp::sources::create<int>([this, userId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    muteUser(userId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<int> NetworkClient::unmuteUserObservable(const juce::String &userId) {
  auto source = rxcpp::sources::create<int>([this, userId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    unmuteUser(userId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<int> NetworkClient::pinPostObservable(const juce::String &postId) {
  auto source = rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    pinPost(postId, [observer](Outcome<nlohmann::json> result) {
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

rxcpp::observable<int> NetworkClient::unpinPostObservable(const juce::String &postId) {
  auto source = rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    unpinPost(postId, [observer](Outcome<nlohmann::json> result) {
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
