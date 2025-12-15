//==============================================================================
// StoriesClient.cpp - Stories operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"

//==============================================================================
// Helper to build API endpoint paths consistently
static juce::String buildApiPath(const char *path) {
  return juce::String("/api/v1") + path;
}

// Helper to convert RequestResult to Outcome<juce::var>
static Outcome<juce::var> requestResultToOutcome(const NetworkClient::RequestResult &result) {
  if (result.success && result.isSuccess()) {
    return Outcome<juce::var>::ok(result.data);
  } else {
    juce::String errorMsg = result.getUserFriendlyError();
    if (errorMsg.isEmpty())
      errorMsg = "Request failed (HTTP " + juce::String(result.httpStatus) + ")";
    return Outcome<juce::var>::error(errorMsg);
  }
}

//==============================================================================
void NetworkClient::getStoriesFeed(ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/stories"), "GET", juce::var(), true);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, storyId, callback]() {
    juce::String endpoint = buildApiPath("/stories/") + storyId + "/view";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, storyId, callback]() {
    juce::String endpoint = buildApiPath("/stories/") + storyId;
    auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::uploadStory(const juce::AudioBuffer<float> &audioBuffer, double sampleRate,
                                const juce::var &midiData, int bpm, const juce::String &key,
                                const juce::StringArray &genres, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  // Upload rate limiting check (Task 4.18)
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
        juce::MessageManager::callAsync([callback, errorMsg]() { callback(Outcome<juce::var>::error(errorMsg)); });
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
            [callback]() { callback(Outcome<juce::var>::error("Failed to encode audio")); });
      }
      return;
    }

    // Build request with audio and MIDI data
    std::map<juce::String, juce::String> extraFields;
    if (midiData.isObject()) {
      extraFields["midi_data"] = juce::JSON::toString(midiData);
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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, storyId, callback]() {
    juce::String path = "/stories/" + storyId + "/views";
    auto result = makeRequestWithRetry(buildApiPath(path.toRawUTF8()), "GET", juce::var(), true);

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
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "POST", juce::var(), true);
    Log::debug("Get story download info response: " + juce::JSON::toString(result.data));

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        if (result.success && result.data.isObject()) {
          DownloadInfo info;
          auto *obj = result.data.getDynamicObject();
          if (obj != nullptr) {
            info.downloadUrl = obj->getProperty("audio_url").toString();
            info.filename = obj->getProperty("audio_filename").toString();
            info.metadata = obj->getProperty("metadata");
            // Note: download_count not tracked in response, but incremented
            // server-side
          }
          callback(Outcome<DownloadInfo>::ok(info));
        } else {
          auto outcome = requestResultToOutcome(result);
          callback(Outcome<DownloadInfo>::error(outcome.getError()));
        }
      });
    }
  });
}

//==============================================================================
// Story Highlights operations

void NetworkClient::getHighlights(const juce::String &userId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, userId, callback]() {
    juce::String endpoint = "/users/" + userId + "/highlights";
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "GET", juce::var(), true);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, callback]() {
    juce::String endpoint = "/highlights/" + highlightId;
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "GET", juce::var(), true);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, name, description, callback]() {
    auto *obj = new juce::DynamicObject();
    obj->setProperty("name", name);
    if (description.isNotEmpty())
      obj->setProperty("description", description);
    juce::var body(obj);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, name, description, callback]() {
    auto *obj = new juce::DynamicObject();
    if (name.isNotEmpty())
      obj->setProperty("name", name);
    if (description.isNotEmpty())
      obj->setProperty("description", description);
    juce::var body(obj);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, callback]() {
    juce::String endpoint = "/highlights/" + highlightId;
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "DELETE", juce::var(), true);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, storyId, callback]() {
    auto *obj = new juce::DynamicObject();
    obj->setProperty("story_id", storyId);
    juce::var body(obj);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, highlightId, storyId, callback]() {
    juce::String endpoint = "/highlights/" + highlightId + "/stories/" + storyId;
    auto result = makeRequestWithRetry(buildApiPath(endpoint.toRawUTF8()), "DELETE", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}
