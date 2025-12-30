// ==============================================================================
// StoriesClient.cpp - Stories operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::getStoriesFeed(ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/stories"), "GET", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::viewStory(const juce::String &storyId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, storyId, callback]() {
    juce::String endpoint = buildApiPath("/stories/") + storyId + "/view";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::deleteStory(const juce::String &storyId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, storyId, callback]() {
    juce::String endpoint = buildApiPath("/stories/") + storyId;
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::uploadStory(const juce::AudioBuffer<float> &audioBuffer, double sampleRate,
                                const nlohmann::json &midiData, int bpm, const juce::String &key,
                                const juce::StringArray &genres, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  // Upload rate limiting check
  if (uploadRateLimiter) {
    juce::String identifier = currentUserId.isEmpty() ? "anonymous" : currentUserId;
    auto rateLimitStatus = uploadRateLimiter->tryConsume(identifier, 1);

    if (!rateLimitStatus.allowed) {
      int retrySeconds =
          rateLimitStatus.retryAfterSeconds > 0 ? rateLimitStatus.retryAfterSeconds : rateLimitStatus.resetInSeconds;

      juce::String retryMsg = retrySeconds > 0 ? " You can upload again in " + juce::String(retrySeconds) + " seconds."
                                               : " Please try again later.";

      juce::String errorMsg = "Upload limit exceeded." + retryMsg;
      Log::warn("Story upload rate limit exceeded for " + identifier + ": " + errorMsg);

      if (callback) {
        juce::MessageManager::callAsync([callback, errorMsg]() { callback(Outcome<nlohmann::json>::error(errorMsg)); });
      }
      return;
    }

    Log::debug("Story upload rate limit OK for " + identifier +
               " - remaining: " + juce::String(rateLimitStatus.remaining) + "/" + juce::String(rateLimitStatus.limit));
  }

  Async::runVoid([this, audioBuffer, sampleRate, midiData, bpm, key, genres, callback]() {
    // Encode audio to MP3
    juce::MemoryBlock mp3Data = encodeAudioToMP3(audioBuffer, sampleRate);

    if (mp3Data.isEmpty()) {
      Log::error("NetworkClient::uploadStory: Failed to encode audio");
      if (callback) {
        juce::MessageManager::callAsync(
            [callback]() { callback(Outcome<nlohmann::json>::error("Failed to encode audio")); });
      }
      return;
    }

    // Build request with audio and MIDI data
    std::map<juce::String, juce::String> extraFields;
    if (midiData.is_object() && !midiData.empty()) {
      extraFields["midi_data"] = midiData.dump();
    }

    // Calculate duration
    double durationSeconds = audioBuffer.getNumSamples() / sampleRate;
    extraFields["duration"] = juce::String(durationSeconds);

    // Add metadata if provided
    if (bpm > 0)
      extraFields["bpm"] = juce::String(bpm);
    if (key.isNotEmpty())
      extraFields["key"] = key;
    if (genres.size() > 0)
      extraFields["genre"] = genres.joinIntoString(",");

    auto result =
        uploadMultipartData(buildApiPath("/stories"), "audio", mp3Data, "story.mp3", "audio/mpeg", extraFields);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getStoryViews(const juce::String &storyId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, storyId, callback]() {
    juce::String path = "/stories/" + storyId + "/views";
    auto result = makeRequestWithRetry(buildApiPath(path.toRawUTF8()), "GET", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getStoryDownloadInfo(const juce::String &storyId, DownloadInfoCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<DownloadInfo>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, storyId, callback]() {
    juce::String endpoint = "/stories/" + storyId + "/download";
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "POST", nlohmann::json(), true);
    Log::debug("Get story download info response: " + juce::String(result.data.dump()));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        if (result.success && result.data.is_object()) {
          DownloadInfo info;
          if (result.data.contains("audio_url")) {
            info.downloadUrl = juce::String(result.data["audio_url"].get<std::string>());
          }
          if (result.data.contains("audio_filename")) {
            info.filename = juce::String(result.data["audio_filename"].get<std::string>());
          }
          if (result.data.contains("metadata")) {
            info.metadata = result.data["metadata"];
          }
          // Note: download_count not tracked in response, but incremented server-side
          callback(Outcome<DownloadInfo>::ok(info));
        } else {
          auto outcome = requestResultToOutcome(result);
          callback(Outcome<DownloadInfo>::error(outcome.getError()));
        }
      });
    }
  });
}

// ==============================================================================
// Story Highlights operations

void NetworkClient::getHighlights(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = "/users/" + userId + "/highlights";
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "GET", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getHighlight(const juce::String &highlightId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, callback]() {
    juce::String endpoint = "/highlights/" + highlightId;
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "GET", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::createHighlight(const juce::String &name, const juce::String &description,
                                    ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, name, description, callback]() {
    nlohmann::json body;
    body["name"] = name.toStdString();
    if (description.isNotEmpty())
      body["description"] = description.toStdString();

    auto result = makeRequestWithRetry(buildApiPath("/highlights"), "POST", body, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::updateHighlight(const juce::String &highlightId, const juce::String &name,
                                    const juce::String &description, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, name, description, callback]() {
    nlohmann::json body;
    if (name.isNotEmpty())
      body["name"] = name.toStdString();
    if (description.isNotEmpty())
      body["description"] = description.toStdString();

    juce::String endpoint = "/highlights/" + highlightId;
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "PUT", body, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::deleteHighlight(const juce::String &highlightId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, callback]() {
    juce::String endpoint = "/highlights/" + highlightId;
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "DELETE", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::addStoryToHighlight(const juce::String &highlightId, const juce::String &storyId,
                                        ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, storyId, callback]() {
    nlohmann::json body = {{"story_id", storyId.toStdString()}};

    juce::String endpoint = "/highlights/" + highlightId + "/stories";
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "POST", body, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::removeStoryFromHighlight(const juce::String &highlightId, const juce::String &storyId,
                                             ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, storyId, callback]() {
    juce::String endpoint = "/highlights/" + highlightId + "/stories/" + storyId;
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "DELETE", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}
