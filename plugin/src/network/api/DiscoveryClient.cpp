// ==============================================================================
// DiscoveryClient.cpp - User discovery operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/User.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

// ==============================================================================
// Helper function to parse user search results
static std::vector<Sidechain::User> parseSearchUsersResponse(const juce::var &json) {
  std::vector<Sidechain::User> users;

  if (!json.isArray()) {
    return users;
  }

  users.reserve(static_cast<size_t>(json.size()));
  for (int i = 0; i < json.size(); ++i) {
    try {
      auto jsonStr = juce::JSON::toString(json[i]);
      auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
      Sidechain::User user;
      from_json(jsonObj, user);
      if (user.isValid()) {
        users.push_back(std::move(user));
      }
    } catch (const std::exception &e) {
      Log::warn("DiscoveryClient: Failed to parse search user: " + juce::String(e.what()));
    }
  }

  Log::debug("DiscoveryClient: Parsed " + juce::String(static_cast<int>(users.size())) + " search users");
  return users;
}

// ==============================================================================
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
      auto outcome = extractProperty(requestResultToOutcome(result), "users");
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
      auto outcome = extractProperty(requestResultToOutcome(result), "users");
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

// ==============================================================================
// Reactive Observable Methods (Phase 5)
// ==============================================================================

rxcpp::observable<std::vector<Sidechain::User>> NetworkClient::searchUsersObservable(const juce::String &query,
                                                                                     int limit) {
  using ResultType = std::vector<Sidechain::User>;
  auto source = rxcpp::sources::create<ResultType>([this, query, limit](auto observer) {
    // URL-encode the query string
    juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
    juce::String endpoint =
        buildApiPath("/search/users") + "?q=" + encodedQuery + "&limit=" + juce::String(limit) + "&offset=0";

    Async::runVoid([this, endpoint, observer]() {
      auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

      juce::MessageManager::callAsync([result, observer]() {
        auto outcome = extractProperty(requestResultToOutcome(result), "users");
        if (outcome.isOk()) {
          auto users = parseSearchUsersResponse(outcome.getValue());
          observer.on_next(std::move(users));
          observer.on_completed();
        } else {
          observer.on_error(std::make_exception_ptr(std::runtime_error(outcome.getError().toStdString())));
        }
      });
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}
