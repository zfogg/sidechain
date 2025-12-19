//==============================================================================
// SearchClient.cpp - Search operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

//==============================================================================
void NetworkClient::searchPosts(const juce::String &query, const juce::String &genre, int bpmMin, int bpmMax,
                                const juce::String &key, int limit, int offset, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  // Build query string with filters
  juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
  juce::String endpoint = buildApiPath("/search/posts") + "?q=" + encodedQuery + "&limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  if (!genre.isEmpty()) {
    juce::String encodedGenre = juce::URL::addEscapeChars(genre, true);
    endpoint += "&genre=" + encodedGenre;
  }

  if (bpmMin > 0)
    endpoint += "&bpm_min=" + juce::String(bpmMin);

  if (bpmMax < 200)
    endpoint += "&bpm_max=" + juce::String(bpmMax);

  if (!key.isEmpty()) {
    juce::String encodedKey = juce::URL::addEscapeChars(key, true);
    endpoint += "&key=" + encodedKey;
  }

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::autocompleteUsers(const juce::String &query, int limit, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
  juce::String endpoint =
      buildApiPath("/search/autocomplete/users") + "?q=" + encodedQuery + "&limit=" + juce::String(limit);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);

      if (outcome.isOk()) {
        auto data = outcome.getValue();
        if (data.isObject() && data.hasProperty("suggestions")) {
          outcome = Outcome<juce::var>::ok(data.getProperty("suggestions", juce::var()));
        }
      }

      callback(outcome);
    });
  });
}

void NetworkClient::autocompleteGenres(const juce::String &query, int limit, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
  juce::String endpoint =
      buildApiPath("/search/autocomplete/genres") + "?q=" + encodedQuery + "&limit=" + juce::String(limit);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);

      if (outcome.isOk()) {
        auto data = outcome.getValue();
        if (data.isObject() && data.hasProperty("suggestions")) {
          outcome = Outcome<juce::var>::ok(data.getProperty("suggestions", juce::var()));
        }
      }

      callback(outcome);
    });
  });
}

void NetworkClient::getSearchSuggestions(const juce::String &query, int limit, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
  juce::String endpoint = buildApiPath("/search/suggestions") + "?q=" + encodedQuery + "&limit=" + juce::String(limit);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}
