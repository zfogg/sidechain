//==============================================================================
// ChallengesClient.cpp - MIDI Challenge operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

//==============================================================================
void NetworkClient::getMIDIChallenges(const juce::String &status, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, status, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges");
    if (status.isNotEmpty())
      endpoint += "?status=" + status;

    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges") + "/" + challengeId;
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::submitMIDIChallengeEntry(const juce::String &challengeId, const juce::String &audioUrl,
                                             const juce::String &postId, const juce::var &midiData,
                                             const juce::String &midiPatternId, ResponseCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, audioUrl, postId, midiData, midiPatternId, callback]() {
    auto *requestObj = new juce::DynamicObject();
    requestObj->setProperty("audio_url", audioUrl);

    if (postId.isNotEmpty())
      requestObj->setProperty("post_id", postId);

    if (midiPatternId.isNotEmpty())
      requestObj->setProperty("midi_pattern_id", midiPatternId);
    else if (!midiData.isVoid() && midiData.hasProperty("events")) {
      // Include MIDI data if provided
      requestObj->setProperty("midi_data", midiData);
    }

    juce::var data(requestObj);
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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges") + "/" + challengeId + "/entries";
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

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
      callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
    return;
  }

  Async::runVoid([this, challengeId, entryId, callback]() {
    juce::String endpoint = buildApiPath("/midi-challenges") + "/" + challengeId + "/entries/" + entryId + "/vote";
    auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);

    if (callback) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}
