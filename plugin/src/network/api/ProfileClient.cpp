// ==============================================================================
// ProfileClient.cpp - Profile operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

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

rxcpp::observable<juce::var> NetworkClient::getUserObservable(const juce::String &userId) {
  auto source = rxcpp::sources::create<juce::var>([this, userId](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getUser(userId, [observer](Outcome<juce::var> result) {
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

rxcpp::observable<juce::var> NetworkClient::getUserPostsObservable(const juce::String &userId, int limit, int offset) {
  auto source = rxcpp::sources::create<juce::var>([this, userId, limit, offset](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getUserPosts(userId, limit, offset, [observer](Outcome<juce::var> result) {
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

rxcpp::observable<juce::var> NetworkClient::getFollowersObservable(const juce::String &userId, int limit, int offset) {
  auto source = rxcpp::sources::create<juce::var>([this, userId, limit, offset](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getFollowers(userId, limit, offset, [observer](Outcome<juce::var> result) {
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

rxcpp::observable<juce::var> NetworkClient::getFollowingObservable(const juce::String &userId, int limit, int offset) {
  auto source = rxcpp::sources::create<juce::var>([this, userId, limit, offset](auto observer) {
    if (userId.isEmpty()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error("User ID is empty")));
      return;
    }

    getFollowing(userId, limit, offset, [observer](Outcome<juce::var> result) {
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

rxcpp::observable<juce::var> NetworkClient::changeUsernameObservable(const juce::String &newUsername) {
  auto source = rxcpp::sources::create<juce::var>([this, newUsername](auto observer) {
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
        observer.on_next(result.getValue());
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
