// ==============================================================================
// ProfileClient.cpp - Profile operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/User.h"
#include "../../models/FeedPost.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

// ==============================================================================
// Helper function to parse a single user from JSON response
static Sidechain::User parseUserFromJson(const juce::var &json) {
  Sidechain::User user;

  if (!json.isObject()) {
    return user;
  }

  try {
    auto jsonStr = juce::JSON::toString(json);
    auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
    from_json(jsonObj, user);
  } catch (const std::exception &e) {
    Log::warn("ProfileClient: Failed to parse user: " + juce::String(e.what()));
  }

  return user;
}

// ==============================================================================
// Helper function to parse user list response into typed UserResult
static NetworkClient::UserResult parseUserListResponse(const juce::var &json) {
  NetworkClient::UserResult result;

  if (!json.isObject()) {
    return result;
  }

  // Try "users" first, then "followers", then "following"
  auto usersArray = json.getProperty("users", juce::var());
  if (!usersArray.isArray()) {
    usersArray = json.getProperty("followers", juce::var());
  }
  if (!usersArray.isArray()) {
    usersArray = json.getProperty("following", juce::var());
  }

  // Extract total and pagination info
  result.total = static_cast<int>(json.getProperty("total", 0));
  result.hasMore = json.getProperty("has_more", false);

  // Parse each user
  if (usersArray.isArray()) {
    result.users.reserve(static_cast<size_t>(usersArray.size()));
    for (int i = 0; i < usersArray.size(); ++i) {
      try {
        auto jsonStr = juce::JSON::toString(usersArray[i]);
        auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
        Sidechain::User user;
        from_json(jsonObj, user);
        if (user.isValid()) {
          result.users.push_back(std::move(user));
        }
      } catch (const std::exception &e) {
        Log::warn("ProfileClient: Failed to parse user in list: " + juce::String(e.what()));
      }
    }
  }

  Log::debug("ProfileClient: Parsed " + juce::String(static_cast<int>(result.users.size())) + " users from response");
  return result;
}

// ==============================================================================
// Helper function to parse user posts response into typed FeedResult
static NetworkClient::FeedResult parseUserPostsResponse(const juce::var &json) {
  NetworkClient::FeedResult result;

  if (!json.isObject()) {
    return result;
  }

  auto postsArray = json.getProperty("posts", juce::var());
  if (!postsArray.isArray()) {
    postsArray = json.getProperty("activities", juce::var());
  }

  result.total = static_cast<int>(json.getProperty("total", 0));
  result.hasMore = json.getProperty("has_more", false);

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
        Log::warn("ProfileClient: Failed to parse user post: " + juce::String(e.what()));
      }
    }
  }

  Log::debug("ProfileClient: Parsed " + juce::String(static_cast<int>(result.posts.size())) + " user posts");
  return result;
}

// ==============================================================================
void NetworkClient::uploadProfilePicture(const juce::File &imageFile, ProfilePictureCallback callback) {
  if (!isAuthenticated()) {
    Log::warn("Cannot upload profile picture: " + juce::String(Constants::Errors::NOT_AUTHENTICATED));
    if (callback)
      callback(Outcome<juce::String>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  if (!imageFile.existsAsFile()) {
    Log::error("Profile picture file does not exist: " + imageFile.getFullPathName());
    if (callback) {
      juce::MessageManager::callAsync([callback]() { callback(Outcome<juce::String>::error("File does not exist")); });
    }
    return;
  }

  Async::runVoid([this, imageFile, callback]() {
    // Helper function for MIME types
    auto getMimeType = [](const juce::String &extension) -> juce::String {
      juce::String ext = extension.toLowerCase();
      if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
      else if (ext == ".png")
        return "image/png";
      else if (ext == ".gif")
        return "image/gif";
      else if (ext == ".webp")
        return "image/webp";
      else
        return "application/octet-stream";
    };

    // Create URL with file upload using JUCE's built-in multipart form handling
    juce::URL url(config.baseUrl + buildApiPath("/users/upload-profile-picture"));

    // Use withFileToUpload - JUCE will automatically create proper
    // multipart/form-data Note: Server expects field name "file", not
    // "profile_picture"
    url = url.withFileToUpload("file", imageFile, getMimeType(imageFile.getFileExtension()));

    // Build headers (auth only - Content-Type will be set automatically by
    // JUCE)
    juce::String headers = "Authorization: Bearer " + authToken + "\r\n";

    // Create request options
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(config.timeoutMs);

    // Make request
    auto stream = url.createInputStream(options);

    if (stream == nullptr) {
      Log::error("Failed to create stream for profile picture upload");
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<juce::String>::error("Failed to encode audio")); });
      }
      return;
    }

    auto response = stream->readEntireStreamAsString();
    Log::debug("Profile picture upload response: " + response);

    // Parse response
    auto result = juce::JSON::parse(response);
    bool success = false;
    juce::String pictureUrl;

    if (result.isObject()) {
      pictureUrl = result.getProperty("url", "").toString();
      success = !pictureUrl.isEmpty();
    }

    if (callback) {
      juce::MessageManager::callAsync([callback, success, pictureUrl]() {
        if (success)
          callback(Outcome<juce::String>::ok(pictureUrl));
        else
          callback(Outcome<juce::String>::error("Failed to upload profile picture"));
      });
    }

    if (success)
      Log::info("Profile picture uploaded successfully: " + pictureUrl);
    else
      Log::error("Profile picture upload failed");
  });
}

void NetworkClient::changeUsername(const juce::String &newUsername, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, newUsername, callback]() {
    auto data = createJsonObject({{"username", newUsername}});

    auto result = makeRequestWithRetry(buildApiPath("/users/username"), "PUT", data, true);
    Log::debug("Change username response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getFollowers(const juce::String &userId, int limit, int offset, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/users") + "/" + userId + "/followers?limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getFollowing(const juce::String &userId, int limit, int offset, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/users") + "/" + userId + "/following?limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getUser(const juce::String &userId, ResponseCallback callback) {
  if (userId.isEmpty()) {
    if (callback)
      callback(Outcome<juce::var>::error("User ID is empty"));
    return;
  }

  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/users") + "/" + userId + "/profile";

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getUserPosts(const juce::String &userId, int limit, int offset, ResponseCallback callback) {
  if (userId.isEmpty()) {
    if (callback)
      callback(Outcome<juce::var>::error("User ID is empty"));
    return;
  }

  if (callback == nullptr)
    return;

  juce::String endpoint =
      buildApiPath("/users") + "/" + userId + "/posts?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

// ==============================================================================
// Reactive Observable Methods
// ==============================================================================

rxcpp::observable<Sidechain::User> NetworkClient::getUserObservable(const juce::String &userId) {
  auto source = rxcpp::sources::create<Sidechain::User>([this, userId](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getUser(userId, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto user = parseUserFromJson(result.getValue());
        if (user.isValid()) {
          observer.on_next(std::move(user));
          observer.on_completed();
        } else {
          observer.on_error(std::make_exception_ptr(std::runtime_error("Failed to parse user")));
        }
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::FeedResult> NetworkClient::getUserPostsObservable(const juce::String &userId,
                                                                                   int limit, int offset) {
  auto source = rxcpp::sources::create<FeedResult>([this, userId, limit, offset](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getUserPosts(userId, limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto feedResult = parseUserPostsResponse(result.getValue());
        observer.on_next(std::move(feedResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::UserResult> NetworkClient::getFollowersObservable(const juce::String &userId,
                                                                                   int limit, int offset) {
  auto source = rxcpp::sources::create<UserResult>([this, userId, limit, offset](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getFollowers(userId, limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto userResult = parseUserListResponse(result.getValue());
        observer.on_next(std::move(userResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::UserResult> NetworkClient::getFollowingObservable(const juce::String &userId,
                                                                                   int limit, int offset) {
  auto source = rxcpp::sources::create<UserResult>([this, userId, limit, offset](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getFollowing(userId, limit, offset, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto userResult = parseUserListResponse(result.getValue());
        observer.on_next(std::move(userResult));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<Sidechain::User> NetworkClient::changeUsernameObservable(const juce::String &newUsername) {
  auto source = rxcpp::sources::create<Sidechain::User>([this, newUsername](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    if (newUsername.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("Username cannot be empty")));
      return;
    }

    changeUsername(newUsername, [observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        auto user = parseUserFromJson(result.getValue());
        observer.on_next(std::move(user));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<juce::String> NetworkClient::uploadProfilePictureObservable(const juce::File &imageFile) {
  auto source = rxcpp::sources::create<juce::String>([this, imageFile](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    if (!imageFile.existsAsFile()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("Image file does not exist")));
      return;
    }

    uploadProfilePicture(imageFile, [observer](Outcome<juce::String> result) {
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
