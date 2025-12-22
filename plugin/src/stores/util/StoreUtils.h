#pragma once

#include "../../util/Result.h"
#include "../../util/SerializableModel.h"
#include "../../util/Log.h"
#include "../../util/logging/Logger.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace Sidechain {
namespace Stores {
namespace Utils {

// ==============================================================================
// JsonArrayParser - Generic JSON array to model list parsing
// ==============================================================================

/**
 * Parses a juce::var JSON array into a vector of shared_ptr<T>.
 * Uses SerializableModel<T>::createFromJson for deserialization.
 *
 * @tparam T The model type (must extend SerializableModel<T>)
 * @param jsonArray The juce::var containing the JSON array
 * @param logContext Optional context string for error logging
 * @return Vector of shared_ptr<T> for successfully parsed items
 *
 * Usage:
 *   auto posts = JsonArrayParser<FeedPost>::parse(postsArray, "FeedPost");
 */
template <typename T> class JsonArrayParser {
public:
  static std::vector<std::shared_ptr<T>> parse(const juce::var &jsonArray, const juce::String &logContext = "") {
    std::vector<std::shared_ptr<T>> result;

    if (!jsonArray.isArray()) {
      return result;
    }

    result.reserve(static_cast<size_t>(jsonArray.size()));

    for (int i = 0; i < jsonArray.size(); ++i) {
      try {
        auto jsonStr = juce::JSON::toString(jsonArray[i]);
        auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
        auto modelResult = SerializableModel<T>::createFromJson(jsonObj);

        if (modelResult.isOk()) {
          result.push_back(modelResult.getValue());
        } else if (logContext.isNotEmpty()) {
          Log::debug("JsonArrayParser: Failed to parse " + logContext + ": " + modelResult.getError());
        }
      } catch (const std::exception &e) {
        if (logContext.isNotEmpty()) {
          Log::debug("JsonArrayParser: Exception parsing " + logContext + ": " + juce::String(e.what()));
        }
      }
    }

    return result;
  }

  /**
   * Parses with validation - only includes items where validator returns true.
   */
  static std::vector<std::shared_ptr<T>> parseWithValidation(const juce::var &jsonArray,
                                                             std::function<bool(const T &)> validator,
                                                             const juce::String &logContext = "") {
    std::vector<std::shared_ptr<T>> result;

    if (!jsonArray.isArray()) {
      return result;
    }

    result.reserve(static_cast<size_t>(jsonArray.size()));

    for (int i = 0; i < jsonArray.size(); ++i) {
      try {
        auto jsonStr = juce::JSON::toString(jsonArray[i]);
        auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
        auto modelResult = SerializableModel<T>::createFromJson(jsonObj);

        if (modelResult.isOk() && validator(*modelResult.getValue())) {
          result.push_back(modelResult.getValue());
        }
      } catch (const std::exception &e) {
        if (logContext.isNotEmpty()) {
          Log::debug("JsonArrayParser: Exception parsing " + logContext + ": " + juce::String(e.what()));
        }
      }
    }

    return result;
  }
};

// ==============================================================================
// NlohmannJsonArrayParser - Parse from nlohmann::json arrays directly
// ==============================================================================

/**
 * Parses a nlohmann::json array into a vector of shared_ptr<T>.
 * For when data is already in nlohmann::json format.
 */
template <typename T> class NlohmannJsonArrayParser {
public:
  static std::vector<std::shared_ptr<T>> parse(const nlohmann::json &jsonArray, const juce::String &logContext = "") {
    std::vector<std::shared_ptr<T>> result;

    if (!jsonArray.is_array()) {
      return result;
    }

    result.reserve(jsonArray.size());

    for (const auto &item : jsonArray) {
      try {
        auto modelResult = SerializableModel<T>::createFromJson(item);

        if (modelResult.isOk()) {
          result.push_back(modelResult.getValue());
        } else if (logContext.isNotEmpty()) {
          Log::debug("NlohmannJsonArrayParser: Failed to parse " + logContext + ": " + modelResult.getError());
        }
      } catch (const std::exception &e) {
        if (logContext.isNotEmpty()) {
          Log::debug("NlohmannJsonArrayParser: Exception parsing " + logContext + ": " + juce::String(e.what()));
        }
      }
    }

    return result;
  }
};

// ==============================================================================
// PostUpdater - Generic post update utility for optimistic updates
// ==============================================================================

/**
 * Generic utility for updating posts across multiple collections.
 * Useful for optimistic updates of like/save/repost state.
 *
 * @tparam PostType The post type (typically FeedPost)
 *
 * Usage:
 *   PostUpdater<FeedPost>::updateInCollection(posts, postId,
 *     [](auto& post) { post->isLiked = true; post->likeCount++; });
 */
template <typename PostType> class PostUpdater {
public:
  using PostPtr = std::shared_ptr<PostType>;
  using PostCollection = std::vector<PostPtr>;
  using UpdateFn = std::function<void(PostPtr &)>;
  using FinderFn = std::function<bool(const PostPtr &)>;

  /**
   * Update a post in a collection by ID.
   */
  static bool updateInCollection(PostCollection &collection, const juce::String &postId, UpdateFn updateFn) {
    for (auto &post : collection) {
      if (post && post->id == postId) {
        updateFn(post);
        return true;
      }
    }
    return false;
  }

  /**
   * Update a post in a collection using a custom finder.
   */
  static bool updateInCollectionWhere(PostCollection &collection, FinderFn finder, UpdateFn updateFn) {
    for (auto &post : collection) {
      if (post && finder(post)) {
        updateFn(post);
        return true;
      }
    }
    return false;
  }

  /**
   * Update a post across multiple collections.
   */
  static void updateAcrossCollections(std::initializer_list<PostCollection *> collections, const juce::String &postId,
                                      UpdateFn updateFn) {
    for (auto *collection : collections) {
      if (collection) {
        updateInCollection(*collection, postId, updateFn);
      }
    }
  }

  /**
   * Find a post's current state by ID.
   */
  template <typename ResultType>
  static std::optional<ResultType> findInCollection(const PostCollection &collection, const juce::String &postId,
                                                    std::function<ResultType(const PostPtr &)> extractor) {
    for (const auto &post : collection) {
      if (post && post->id == postId) {
        return extractor(post);
      }
    }
    return std::nullopt;
  }
};

// ==============================================================================
// ToggleState - Value types for optimistic updates
// ==============================================================================

/**
 * Represents a toggleable boolean property with a count.
 * Used for like/save/repost operations.
 */
struct ToggleState {
  bool isActive = false;
  int count = 0;

  ToggleState toggle() const { return {!isActive, isActive ? std::max(0, count - 1) : count + 1}; }

  static ToggleState from(bool active, int cnt) { return {active, cnt}; }
};

// ==============================================================================
// OptimisticToggle - Encapsulates optimistic update logic
// ==============================================================================

/**
 * Utility for performing optimistic toggle operations with rollback capability.
 *
 * Usage:
 *   OptimisticToggle toggle(currentlyLiked, likeCount);
 *   // Apply optimistic update
 *   post->isLiked = toggle.newState();
 *   post->likeCount = toggle.newCount();
 *
 *   // On error, rollback
 *   post->isLiked = toggle.originalState();
 *   post->likeCount = toggle.originalCount();
 */
class OptimisticToggle {
public:
  OptimisticToggle(bool currentState, int currentCount)
      : originalState_(currentState), originalCount_(currentCount), newState_(!currentState),
        newCount_(currentState ? std::max(0, currentCount - 1) : currentCount + 1) {}

  bool originalState() const { return originalState_; }
  int originalCount() const { return originalCount_; }
  bool newState() const { return newState_; }
  int newCount() const { return newCount_; }

  // For determining which API to call
  bool wasActive() const { return originalState_; }
  bool willBeActive() const { return newState_; }

private:
  bool originalState_;
  int originalCount_;
  bool newState_;
  int newCount_;
};

// ==============================================================================
// StateHelpers - Generic state manipulation utilities
// ==============================================================================

/**
 * Helper class for common state operations.
 */
class StateHelpers {
public:
  /**
   * Create a timestamp for last updated tracking.
   */
  static int64_t now() { return juce::Time::getCurrentTime().toMilliseconds(); }

  /**
   * Check if a cache is stale based on TTL.
   */
  static bool isStale(int64_t lastUpdated, int ttlSeconds) {
    auto age = (now() - lastUpdated) / 1000;
    return age >= ttlSeconds;
  }

  /**
   * Check if should load more (pagination helper).
   */
  static bool shouldLoadMore(bool hasMore, bool isLoading) { return hasMore && !isLoading; }
};

// ==============================================================================
// NetworkClientGuard - RAII guard for network client null checks
// ==============================================================================

/**
 * Checks if network client is available and logs error if not.
 * Returns false if network client is null.
 *
 * Usage:
 *   if (!NetworkClientGuard::check(networkClient, "loadFeed")) return;
 */
class NetworkClientGuard {
public:
  template <typename NetworkClientType>
  static bool check(NetworkClientType *client, const juce::String &operation,
                    const juce::String &logTag = "AppStore") {
    if (!client) {
      Util::logError(logTag, "Cannot " + operation + " - network client not set");
      return false;
    }
    return true;
  }

  template <typename NetworkClientType>
  static bool checkSilent(NetworkClientType *client) {
    return client != nullptr;
  }
};

// ==============================================================================
// LoadingStateScope - RAII for loading state management
// ==============================================================================

/**
 * RAII helper that sets loading state on construction and clears on destruction.
 * Useful for ensuring loading state is always cleared, even on exceptions.
 *
 * Usage:
 *   {
 *     LoadingStateScope scope(state.isLoading);
 *     // ... do work ...
 *   } // isLoading automatically set to false
 */
class LoadingStateScope {
public:
  explicit LoadingStateScope(bool &loadingFlag) : loadingFlag_(loadingFlag) { loadingFlag_ = true; }

  ~LoadingStateScope() { loadingFlag_ = false; }

  // Non-copyable
  LoadingStateScope(const LoadingStateScope &) = delete;
  LoadingStateScope &operator=(const LoadingStateScope &) = delete;

private:
  bool &loadingFlag_;
};

} // namespace Utils
} // namespace Stores
} // namespace Sidechain
