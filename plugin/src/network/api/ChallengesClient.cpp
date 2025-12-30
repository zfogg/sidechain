// ==============================================================================
// ChallengesClient.cpp - MIDI Challenge operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::getMIDIChallenges(const juce::String &status, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, status, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges");
    if (status.isNotEmpty())
      endpoint += "?status=" + status;

    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getMIDIChallenge(const juce::String &challengeId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges") + "/" + challengeId;
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::submitMIDIChallengeEntry(const juce::String &challengeId, const juce::String &audioUrl,
                                             const juce::String &postId, const nlohmann::json &midiData,
                                             const juce::String &midiPatternId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, audioUrl, postId, midiData, midiPatternId, callback]() {
    nlohmann::json data;
    data["audio_url"] = audioUrl.toStdString();

    if (postId.isNotEmpty())
      data["post_id"] = postId.toStdString();

    if (midiPatternId.isNotEmpty())
      data["midi_pattern_id"] = midiPatternId.toStdString();
    else if (!midiData.is_null() && midiData.contains("events")) {
      // Include MIDI data if provided
      data["midi_data"] = midiData;
    }

    juce::String endpoint = buildApiPath("/midi-challenges") + "/" + challengeId + "/entries";
    auto result = makeRequestWithRetry(endpoint, "POST", data, true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getMIDIChallengeEntries(const juce::String &challengeId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges") + "/" + challengeId + "/entries";
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::voteMIDIChallengeEntry(const juce::String &challengeId, const juce::String &entryId,
                                           ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<nlohmann::json>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, entryId, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges") + "/" + challengeId + "/entries/" + entryId + "/vote";
    auto result = makeRequestWithRetry(endpoint, "POST", nlohmann::json(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}
