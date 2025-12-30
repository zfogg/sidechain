// ==============================================================================
// SearchClient.cpp - Search operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../models/FeedPost.h"
#include "../../util/Async.h"
#include "../../util/Constants.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

// ==============================================================================
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

// ==============================================================================
// Reactive Observable Methods (Phase 5)
// ==============================================================================

rxcpp::observable<std::vector<Sidechain::FeedPost>> NetworkClient::searchPostsObservable(const juce::String &query,
                                                                                         int limit, int offset) {
  using ResultType = std::vector<Sidechain::FeedPost>;
  auto source = rxcpp::sources::create<ResultType>([this, query, limit, offset](auto observer) {
    // Build query string
    juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
    juce::String endpoint = buildApiPath("/search/posts") + "?q=" + encodedQuery + "&limit=" + juce::String(limit) +
                            "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, observer]() {
      auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

      juce::MessageManager::callAsync([result, observer]() {
        auto outcome = requestResultToOutcome(result);
        if (outcome.isOk()) {
          ResultType posts;
          auto data = outcome.getValue();
          // Parse posts array - check for "posts" or "results" field, or direct array
          juce::var postsArray = data;
          if (data.hasProperty("posts")) {
            postsArray = data["posts"];
          } else if (data.hasProperty("results")) {
            postsArray = data["results"];
          }

          if (postsArray.isArray()) {
            for (int i = 0; i < postsArray.size(); ++i) {
              try {
                auto jsonStr = juce::JSON::toString(postsArray[i]);
                auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                Sidechain::FeedPost post;
                from_json(jsonObj, post);
                posts.push_back(std::move(post));
              } catch (const std::exception &e) {
                Log::warn("NetworkClient: Failed to parse search post: " + juce::String(e.what()));
              }
            }
          }
          observer.on_next(std::move(posts));
          observer.on_completed();
        } else {
          observer.on_error(std::make_exception_ptr(std::runtime_error(outcome.getError().toStdString())));
        }
      });
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}
