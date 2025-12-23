// ==============================================================================
// ProfileClient.cpp - Profile operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

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
