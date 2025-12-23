// ==============================================================================
// SocialClient.cpp - Social operations (follow, unfollow, play tracking)
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::followUser(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    auto data = createJsonObject({{"target_user_id", userId}});

    auto result = makeRequestWithRetry(buildApiPath("/social/follow"), "POST", data, true);
    Log::debug("Follow response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    auto data = createJsonObject({{"target_user_id", userId}});

    auto result = makeRequestWithRetry(buildApiPath("/social/unfollow"), "POST", data, true);
    Log::debug("Unfollow response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    auto data = createJsonObject({{"target_user_id", userId}});

    auto result = makeRequestWithRetry(buildApiPath("/social/block"), "POST", data, true);
    Log::debug("Block response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    auto data = createJsonObject({{"target_user_id", userId}});

    auto result = makeRequestWithRetry(buildApiPath("/social/unblock"), "POST", data, true);
    Log::debug("Unblock response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, activityId, callback]() {
    auto data = createJsonObject({{"activity_id", activityId}});

    auto result = makeRequestWithRetry(buildApiPath("/social/play"), "POST", data, true);
    Log::debug("Track play response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  // Only track if duration is meaningful (at least 1 second)
  if (durationSeconds < 1.0) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, activityId, durationSeconds, callback]() {
    auto data = createJsonObject({{"activity_id", activityId}, {"duration", durationSeconds}});

    auto result = makeRequestWithRetry(buildApiPath("/social/listen-duration"), "POST", data, true);
    Log::debug("Track listen duration response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/save";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
    Log::debug("Save post response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/save";
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
    Log::debug("Unsave post response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/users/me/saved") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
    Log::debug("Get saved posts response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, quote, callback]() {
    juce::var data = juce::var(new juce::DynamicObject());
    if (quote.isNotEmpty()) {
      data.getDynamicObject()->setProperty("quote", quote);
    }

    juce::String endpoint = "/api/v1/posts/" + postId + "/repost";
    auto result = makeRequestWithRetry(endpoint, "POST", data, true);
    Log::debug("Repost response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/repost";
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
    Log::debug("Undo repost response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/archive";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
    Log::debug("Archive post response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/unarchive";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
    Log::debug("Unarchive post response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/users/me/archived") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
    Log::debug("Get archived posts response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = buildApiPath("/users/") + userId + "/mute";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
    Log::debug("Mute user response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = buildApiPath("/users/") + userId + "/mute";
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
    Log::debug("Unmute user response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, limit, offset, callback]() {
    juce::String endpoint =
        buildApiPath("/users/me/muted") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
    Log::debug("Get muted users response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = buildApiPath("/users/") + userId + "/muted";
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
    Log::debug("Is user muted response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/pin";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
    Log::debug("Pin post response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, callback]() {
    juce::String endpoint = "/api/v1/posts/" + postId + "/pin";
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
    Log::debug("Unpin post response: " + juce::JSON::toString(result.data));

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, postId, order, callback]() {
    auto data = createJsonObject({{"order", order}});

    juce::String endpoint = "/api/v1/posts/" + postId + "/pin-order";
    auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
    Log::debug("Update pin order response: " + juce::JSON::toString(result.data));

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
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), false);
    Log::debug("Is post pinned response: " + juce::JSON::toString(result.data));

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
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), false);
    Log::debug("Get sound response: " + juce::JSON::toString(result.data));

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
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), false);
    Log::debug("Get sound posts response: " + juce::JSON::toString(result.data));

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
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), false);
    Log::debug("Get trending sounds response: " + juce::JSON::toString(result.data));

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
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), false);
    Log::debug("Search sounds response: " + juce::JSON::toString(result.data));

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
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), false);
    Log::debug("Get post sound response: " + juce::JSON::toString(result.data));

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

    auto body = new juce::DynamicObject();
    if (name.isNotEmpty())
      body->setProperty("name", name);
    if (description.isNotEmpty())
      body->setProperty("description", description);
    body->setProperty("is_public", isPublic);

    auto result = makeRequestWithRetry(endpoint, "PATCH", juce::var(body), true);
    Log::debug("Update sound response: " + juce::JSON::toString(result.data));

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

  auto data = createJsonObject({{"activity_id", postId}});

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
    auto result = makeRequestWithRetry(endpoint, method, juce::var(), true);
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
    auto result = makeRequestWithRetry(endpoint, method, juce::var(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::addEmojiReaction(const juce::String &postId, const juce::String &emoji, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  auto data = createJsonObject({{"activity_id", postId}, {"emoji", emoji}});

  Async::runVoid([this, data, callback]() {
    auto result = makeRequestWithRetry("/api/v1/social/react", "POST", data, true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}
