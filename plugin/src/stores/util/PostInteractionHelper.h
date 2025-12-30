#pragma once

#include "../AppState.h"
#include "../app/AppState.h"
#include "StoreUtils.h"
#include "../../util/logging/Logger.h"
#include "../../util/Result.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>

namespace Sidechain {
namespace Stores {
namespace Utils {

// ==============================================================================
// PostInteractionHelper - Generic helper for post interactions
// ==============================================================================

/**
 * Helper class for toggle operations (like, save, repost) that:
 * 1. Find current state in all post collections
 * 2. Apply optimistic update across all collections
 * 3. Call the appropriate network API
 * 4. Rollback on error if needed
 *
 * This eliminates the massive code duplication in toggleLike, toggleSave, toggleRepost.
 */
class PostInteractionHelper {
public:
  using PostsStateRef = Rx::State<PostsState>;

  /**
   * Configuration for a toggle operation.
   */
  struct ToggleConfig {
    // Property accessors
    std::function<bool(const FeedPost &)> getIsActive;
    std::function<int(const FeedPost &)> getCount;
    std::function<void(FeedPost &, bool)> setIsActive;
    std::function<void(FeedPost &, int)> setCount;

    // API calls (isActive -> callback)
    std::function<void(const juce::String &, bool, std::function<void(Outcome<nlohmann::json>)>)> apiCall;

    // Logging context
    juce::String actionName;
  };

  /**
   * Find the current toggle state of a post across all feeds.
   *
   * @param state Current posts state
   * @param postId The post ID to find
   * @param getIsActive Function to get the active state from a post
   * @param getCount Function to get the count from a post
   * @return The current toggle state, or nullopt if not found
   */
  static std::optional<ToggleState> findCurrentState(const PostsState &state, const juce::String &postId,
                                                     std::function<bool(const FeedPost &)> getIsActive,
                                                     std::function<int(const FeedPost &)> getCount) {
    // Search in all feeds
    for (const auto &[feedType, feedState] : state.feeds) {
      for (const auto &post : feedState.posts) {
        if (post && post->id == postId) {
          return ToggleState::from(getIsActive(*post), getCount(*post));
        }
      }
    }

    // Search in saved posts
    for (const auto &post : state.savedPosts.posts) {
      if (post && post->id == postId) {
        return ToggleState::from(getIsActive(*post), getCount(*post));
      }
    }

    // Search in archived posts
    for (const auto &post : state.archivedPosts.posts) {
      if (post && post->id == postId) {
        return ToggleState::from(getIsActive(*post), getCount(*post));
      }
    }

    return std::nullopt;
  }

  /**
   * Apply an update to a post across all collections in the state.
   *
   * @param state State to modify
   * @param postId Post ID to update
   * @param updateFn Function to apply to matching posts
   */
  static void updatePostAcrossCollections(PostsState &state, const juce::String &postId,
                                          std::function<void(std::shared_ptr<FeedPost> &)> updateFn) {
    // Update in all feeds
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post && post->id == postId) {
          updateFn(post);
        }
      }
    }

    // Update in saved posts
    for (auto &post : state.savedPosts.posts) {
      if (post && post->id == postId) {
        updateFn(post);
      }
    }

    // Update in archived posts
    for (auto &post : state.archivedPosts.posts) {
      if (post && post->id == postId) {
        updateFn(post);
      }
    }
  }

  /**
   * Perform a toggle operation with optimistic update and rollback on error.
   *
   * @param state The posts state state
   * @param postId The post to toggle
   * @param config Configuration for the toggle operation
   */
  static void performToggle(PostsStateRef state, const juce::String &postId, const ToggleConfig &config) {
    // 1. Find current state
    auto currentState = state->getState();
    auto toggleState = findCurrentState(currentState, postId, config.getIsActive, config.getCount);

    if (!toggleState.has_value()) {
      Util::logWarning("PostInteractionHelper", "Post not found for " + config.actionName + ": " + postId);
      return;
    }

    bool wasActive = toggleState->isActive;
    auto newToggleState = toggleState->toggle();

    // 2. Apply optimistic update
    PostsState newState = state->getState();
    updatePostAcrossCollections(newState, postId, [&config, &newToggleState](std::shared_ptr<FeedPost> &post) {
      config.setIsActive(*post, newToggleState.isActive);
      config.setCount(*post, newToggleState.count);
    });
    state->setState(newState);

    Util::logDebug("PostInteractionHelper", config.actionName + " optimistic update: " + postId);

    // 3. Call API
    config.apiCall(postId, wasActive, [state, postId, wasActive, toggleState, config](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        Util::logInfo("PostInteractionHelper",
                      config.actionName + " " + (wasActive ? "undone" : "applied") + " successfully: " + postId);
      } else {
        Util::logError("PostInteractionHelper", "Failed to " + config.actionName + " post: " + result.getError());

        // Rollback optimistic update
        PostsState rollbackState = state->getState();
        updatePostAcrossCollections(rollbackState, postId, [&config, &toggleState](std::shared_ptr<FeedPost> &post) {
          config.setIsActive(*post, toggleState->isActive);
          config.setCount(*post, toggleState->count);
        });
        state->setState(rollbackState);
      }
    });
  }

  /**
   * Create a ToggleConfig for like operations.
   */
  static ToggleConfig createLikeConfig(
      std::function<void(const juce::String &, bool, std::function<void(Outcome<nlohmann::json>)>)> apiCall) {
    return ToggleConfig{.getIsActive = [](const FeedPost &p) { return p.isLiked; },
                        .getCount = [](const FeedPost &p) { return p.likeCount; },
                        .setIsActive = [](FeedPost &p, bool active) { p.isLiked = active; },
                        .setCount = [](FeedPost &p, int count) { p.likeCount = count; },
                        .apiCall = std::move(apiCall),
                        .actionName = "like"};
  }

  /**
   * Create a ToggleConfig for save operations.
   */
  static ToggleConfig createSaveConfig(
      std::function<void(const juce::String &, bool, std::function<void(Outcome<nlohmann::json>)>)> apiCall) {
    return ToggleConfig{.getIsActive = [](const FeedPost &p) { return p.isSaved; },
                        .getCount = [](const FeedPost &p) { return p.saveCount; },
                        .setIsActive = [](FeedPost &p, bool active) { p.isSaved = active; },
                        .setCount = [](FeedPost &p, int count) { p.saveCount = count; },
                        .apiCall = std::move(apiCall),
                        .actionName = "save"};
  }

  /**
   * Create a ToggleConfig for repost operations.
   */
  static ToggleConfig createRepostConfig(
      std::function<void(const juce::String &, bool, std::function<void(Outcome<nlohmann::json>)>)> apiCall) {
    return ToggleConfig{.getIsActive = [](const FeedPost &p) { return p.isReposted; },
                        .getCount = [](const FeedPost &p) { return p.repostCount; },
                        .setIsActive = [](FeedPost &p, bool active) { p.isReposted = active; },
                        .setCount = [](FeedPost &p, int count) { p.repostCount = count; },
                        .apiCall = std::move(apiCall),
                        .actionName = "repost"};
  }
};

// ==============================================================================
// FollowHelper - Helper for follow/unfollow operations
// ==============================================================================

/**
 * Helper for follow operations on posts.
 */
class FollowHelper {
public:
  using PostsStateRef = Rx::State<PostsState>;

  /**
   * Extract user ID from a post.
   */
  static std::optional<std::pair<juce::String, bool>> findUserAndFollowState(const PostsState &state,
                                                                             const juce::String &postId) {
    for (const auto &[feedType, feedState] : state.feeds) {
      for (const auto &post : feedState.posts) {
        if (post && post->id == postId) {
          return std::make_pair(post->userId, post->isFollowing);
        }
      }
    }
    return std::nullopt;
  }

  /**
   * Update follow state for a post across all collections.
   */
  static void updateFollowState(PostsState &state, const juce::String &postId, bool isFollowing) {
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post && post->id == postId) {
          post->isFollowing = isFollowing;
        }
      }
    }
  }
};

} // namespace Utils
} // namespace Stores
} // namespace Sidechain
