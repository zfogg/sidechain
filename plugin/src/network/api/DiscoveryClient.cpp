//==============================================================================
// DiscoveryClient.cpp - User discovery operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

//==============================================================================
void NetworkClient::searchUsers(const juce::String &query, int limit, int offset, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  // URL-encode the query string
  juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
  juce::String endpoint = buildApiPath("/search/users") + "?q=" + encodedQuery + "&limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);

      // Extract the "users" array from the response
      if (outcome.isOk()) {
        auto data = outcome.getValue();
        if (data.isObject() && data.hasProperty("users")) {
          outcome = Outcome<juce::var>::ok(data.getProperty("users", juce::var()));
        }
      }

      callback(outcome);
    });
  });
}

void NetworkClient::getTrendingUsers(int limit, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/discover/trending") + "?limit=" + juce::String(limit);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getFeaturedProducers(int limit, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/discover/featured") + "?limit=" + juce::String(limit);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getSuggestedUsers(int limit, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/discover/suggested") + "?limit=" + juce::String(limit);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);

      // Extract the "users" array from the response
      if (outcome.isOk()) {
        auto data = outcome.getValue();
        if (data.isObject() && data.hasProperty("users")) {
          outcome = Outcome<juce::var>::ok(data.getProperty("users", juce::var()));
        }
      }

      callback(outcome);
    });
  });
}

void NetworkClient::getUsersByGenre(const juce::String &genre, int limit, int offset, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  // URL-encode the genre
  juce::String encodedGenre = juce::URL::addEscapeChars(genre, true);
  juce::String endpoint = buildApiPath("/discover/genre") + "/" + encodedGenre + "?limit=" + juce::String(limit) +
                          "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getAvailableGenres(ResponseCallback callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/discover/genres"), "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getSimilarUsers(const juce::String &userId, int limit, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint = buildApiPath("/users") + "/" + userId + "/similar?limit=" + juce::String(limit);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}

void NetworkClient::getRecommendedUsersToFollow(int limit, int offset, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint =
      buildApiPath("/users/recommended") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    juce::MessageManager::callAsync([callback, result]() {
      auto outcome = requestResultToOutcome(result);
      callback(outcome);
    });
  });
}
