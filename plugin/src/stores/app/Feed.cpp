#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Helper Functions

static inline bool isAggregatedFeedType(FeedType feedType) {
  return feedType == FeedType::TimelineAggregated || feedType == FeedType::TrendingAggregated ||
         feedType == FeedType::NotificationAggregated || feedType == FeedType::UserActivityAggregated;
}

static inline juce::String feedTypeToString(FeedType feedType) {
  switch (feedType) {
  case FeedType::Timeline:
    return "Timeline";
  case FeedType::Global:
    return "Global";
  case FeedType::Trending:
    return "Trending";
  case FeedType::ForYou:
    return "ForYou";
  case FeedType::Popular:
    return "Popular";
  case FeedType::Latest:
    return "Latest";
  case FeedType::Discovery:
    return "Discovery";
  case FeedType::TimelineAggregated:
    return "TimelineAggregated";
  case FeedType::TrendingAggregated:
    return "TrendingAggregated";
  case FeedType::NotificationAggregated:
    return "NotificationAggregated";
  case FeedType::UserActivityAggregated:
    return "UserActivityAggregated";
  default:
    return "Unknown";
  }
}

// ==============================================================================
// Feed Loading

void AppStore::loadFeed(FeedType feedType, [[maybe_unused]] bool forceRefresh) {
  if (!networkClient) {
    PostsState newState = sliceManager.posts->getState();
    newState.currentFeedType = feedType;
    newState.feedError = "Network client not initialized";
    sliceManager.posts->setState(newState);
    return;
  }

  Util::logInfo("AppStore", "Loading feed", "feedType=" + feedTypeToString(feedType));

  // Note: We always fetch fresh data on network, don't use stale cache.
  // Removed cache check - always fetch from network for latest content.

  // Update state to loading
  PostsState loadingState = sliceManager.posts->getState();
  loadingState.currentFeedType = feedType;
  if (loadingState.feeds.count(feedType) == 0) {
    loadingState.feeds[feedType] = FeedState();
  }
  loadingState.feeds[feedType].isLoading = true;
  loadingState.feeds[feedType].error = "";
  sliceManager.posts->setState(loadingState);

  // Fetch from network
  performFetch(feedType, 20, 0);
}

void AppStore::refreshCurrentFeed() {
  auto currentState = sliceManager.posts->getState();
  loadFeed(currentState.currentFeedType, true);
}

void AppStore::loadMore() {
  auto currentState = sliceManager.posts->getState();
  auto feedType = currentState.currentFeedType;

  if (currentState.feeds.count(feedType) == 0) {
    Util::logWarning("AppStore", "Cannot load more - feed not initialized");
    return;
  }

  auto &feedState = currentState.feeds.at(feedType);
  if (!feedState.hasMore || feedState.isLoading || !networkClient) {
    return;
  }

  PostsState loadingState = sliceManager.posts->getState();
  if (loadingState.feeds.count(feedType) > 0) {
    loadingState.feeds[feedType].isLoading = true;
  }
  sliceManager.posts->setState(loadingState);

  performFetch(feedType, feedState.limit, feedState.offset);
}

void AppStore::switchFeedType(FeedType feedType) {
  PostsState newState = sliceManager.posts->getState();
  newState.currentFeedType = feedType;
  sliceManager.posts->setState(newState);

  // Load the new feed if not already loaded
  auto currentState = sliceManager.posts->getState();
  if (currentState.feeds[feedType].posts.empty()) {
    loadFeed(feedType, false);
  }
}

// ==============================================================================
// Saved Posts

void AppStore::loadSavedPosts() {
  if (!networkClient) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.savedPosts.error = "Network client not initialized";
    sliceManager.posts->setState(errorState);
    return;
  }

  Util::logInfo("AppStore", "Loading saved posts");

  PostsState loadingState = sliceManager.posts->getState();
  loadingState.savedPosts.isLoading = true;
  loadingState.savedPosts.offset = 0;
  loadingState.savedPosts.posts.clear();
  loadingState.savedPosts.error = "";
  sliceManager.posts->setState(loadingState);

  networkClient->getSavedPosts(20, 0, [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void AppStore::loadMoreSavedPosts() {
  auto currentState = sliceManager.posts->getState();
  if (!currentState.savedPosts.hasMore || currentState.savedPosts.isLoading || !networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Loading more saved posts");

  PostsState loadingState = sliceManager.posts->getState();
  loadingState.savedPosts.isLoading = true;
  sliceManager.posts->setState(loadingState);

  networkClient->getSavedPosts(currentState.savedPosts.limit, currentState.savedPosts.offset,
                               [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void AppStore::unsavePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Unsaving post: " + postId);

  // Optimistic removal from saved posts
  PostsState newState = sliceManager.posts->getState();
  auto it = std::find_if(newState.savedPosts.posts.begin(), newState.savedPosts.posts.end(),
                         [&postId](const auto &post) { return post->id == postId; });
  if (it != newState.savedPosts.posts.end()) {
    newState.savedPosts.posts.erase(it);
  }
  sliceManager.posts->setState(newState);

  // Send to server
  networkClient->unsavePost(postId, [this, postId](Outcome<juce::var> result) {
    if (!result.isOk()) {
      // Refresh on error to restore the post
      Util::logError("AppStore", "Failed to unsave post: " + result.getError());
      loadSavedPosts();
    } else {
      Util::logDebug("AppStore", "Post unsaved successfully");
    }
  });
}

// ==============================================================================
// Archived Posts

void AppStore::loadArchivedPosts() {
  if (!networkClient) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.archivedPosts.error = "Network client not initialized";
    sliceManager.posts->setState(errorState);
    return;
  }

  Util::logInfo("AppStore", "Loading archived posts");

  PostsState loadingState = sliceManager.posts->getState();
  loadingState.archivedPosts.isLoading = true;
  loadingState.archivedPosts.offset = 0;
  loadingState.archivedPosts.posts.clear();
  loadingState.archivedPosts.error = "";
  sliceManager.posts->setState(loadingState);

  networkClient->getArchivedPosts(20, 0, [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void AppStore::loadMoreArchivedPosts() {
  auto currentState = sliceManager.posts->getState();
  if (!currentState.archivedPosts.hasMore || currentState.archivedPosts.isLoading || !networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Loading more archived posts");

  PostsState loadingState = sliceManager.posts->getState();
  loadingState.archivedPosts.isLoading = true;
  sliceManager.posts->setState(loadingState);

  // Fetch archived posts from NetworkClient
  int offset = currentState.archivedPosts.offset + currentState.archivedPosts.posts.size();
  networkClient->getArchivedPosts(20, offset, [this, offset](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Util::logError("AppStore", "Failed to load archived posts: " + result.getError());
      PostsState errorState = sliceManager.posts->getState();
      errorState.archivedPosts.isLoading = false;
      errorState.archivedPosts.error = result.getError();
      sliceManager.posts->setState(errorState);
      return;
    }

    // Parse the response
    const auto &data = result.getValue();
    if (!data.isArray()) {
      Util::logError("AppStore", "Invalid archived posts response format");
      PostsState errorState = sliceManager.posts->getState();
      errorState.archivedPosts.isLoading = false;
      sliceManager.posts->setState(errorState);
      return;
    }

    // Update state with fetched archived posts
    PostsState newState = sliceManager.posts->getState();
    newState.archivedPosts.isLoading = false;

    // Parse each archived post from the response
    for (int i = 0; i < data.size(); ++i) {
      auto post = FeedPost::fromJSON(data[i]);
      if (post) {
        newState.archivedPosts.posts.push_back(post);
      }
    }

    // Update pagination info
    newState.archivedPosts.offset = offset;
    newState.archivedPosts.hasMore = (data.size() >= 20); // Has more if got full page

    sliceManager.posts->setState(newState);
    Util::logDebug("AppStore", "Loaded " + juce::String(data.size()) + " archived posts");
  });
}

void AppStore::restorePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Restoring post: " + postId);

  // Optimistic removal from archived posts
  PostsState newState = sliceManager.posts->getState();
  auto it = std::find_if(newState.archivedPosts.posts.begin(), newState.archivedPosts.posts.end(),
                         [&postId](const auto &post) { return post->id == postId; });
  if (it != newState.archivedPosts.posts.end()) {
    newState.archivedPosts.posts.erase(it);
  }
  sliceManager.posts->setState(newState);

  // Send to server
  networkClient->unarchivePost(postId, [this, postId](Outcome<juce::var> result) {
    if (!result.isOk()) {
      // Refresh on error to restore the post to the list
      Util::logError("AppStore", "Failed to restore post: " + result.getError());
      loadArchivedPosts();
    } else {
      Util::logDebug("AppStore", "Post restored successfully");
    }
  });
}

// ==============================================================================
// Post Interactions

void AppStore::toggleLike(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  // Check current like state to determine whether to like or unlike
  auto currentPostsState = sliceManager.posts->getState();
  bool isCurrentlyLiked = false;

  for (const auto &[feedType, feedState] : currentPostsState.feeds) {
    for (const auto &post : feedState.posts) {
      if (post->id == postId) {
        isCurrentlyLiked = post->isLiked;
        break;
      }
    }
  }

  // Optimistic update
  PostsState newState = sliceManager.posts->getState();
  // Update all occurrences of the post across all feeds
  for (auto &[feedType, feedState] : newState.feeds) {
    for (auto &post : feedState.posts) {
      if (post->id == postId) {
        post->isLiked = !post->isLiked;
        post->likeCount += post->isLiked ? 1 : -1;
      }
    }
  }

  for (auto &post : newState.savedPosts.posts) {
    if (post->id == postId) {
      post->isLiked = !post->isLiked;
      post->likeCount += post->isLiked ? 1 : -1;
    }
  }

  for (auto &post : newState.archivedPosts.posts) {
    if (post->id == postId) {
      post->isLiked = !post->isLiked;
      post->likeCount += post->isLiked ? 1 : -1;
    }
  }
  sliceManager.posts->setState(newState);

  // Send to server - like or unlike based on previous state
  if (isCurrentlyLiked) {
    networkClient->unlikePost(postId, [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to unlike post: " + result.getError());
      } else {
        // Invalidate feed caches on successful like/unlike
      }
    });
  } else {
    networkClient->likePost(postId, "", [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to like post: " + result.getError());
      } else {
        // Invalidate feed caches on successful like/unlike
      }
    });
  }
}

void AppStore::toggleSave(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  // Check current save state
  auto currentPostsState = sliceManager.posts->getState();
  bool isCurrentlySaved = false;

  for (const auto &[feedType, feedState] : currentPostsState.feeds) {
    for (const auto &post : feedState.posts) {
      if (post->id == postId) {
        isCurrentlySaved = post->isSaved;
        break;
      }
    }
  }

  // Optimistic update
  PostsState newState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : newState.feeds) {
    for (auto &post : feedState.posts) {
      if (post->id == postId) {
        post->isSaved = !post->isSaved;
        post->saveCount += post->isSaved ? 1 : -1;
      }
    }
  }

  for (auto &post : newState.savedPosts.posts) {
    if (post->id == postId) {
      post->isSaved = !post->isSaved;
      post->saveCount += post->isSaved ? 1 : -1;
    }
  }

  for (auto &post : newState.archivedPosts.posts) {
    if (post->id == postId) {
      post->isSaved = !post->isSaved;
      post->saveCount += post->isSaved ? 1 : -1;
    }
  }
  sliceManager.posts->setState(newState);

  // Send to server
  if (isCurrentlySaved) {
    networkClient->unsavePost(postId, [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to unsave post: " + result.getError());
      } else {
        // Invalidate feed caches on successful save/unsave
      }
    });
  } else {
    networkClient->savePost(postId, [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to save post: " + result.getError());
      } else {
        // Invalidate feed caches on successful save/unsave
      }
    });
  }
}

void AppStore::toggleRepost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  // Check current repost state
  auto currentPostsState = sliceManager.posts->getState();
  bool isCurrentlyReposted = false;

  for (const auto &[feedType, feedState] : currentPostsState.feeds) {
    for (const auto &post : feedState.posts) {
      if (post->id == postId) {
        isCurrentlyReposted = post->isReposted;
        break;
      }
    }
  }

  // Optimistic update
  PostsState newState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : newState.feeds) {
    for (auto &post : feedState.posts) {
      if (post->id == postId) {
        post->isReposted = !post->isReposted;
        post->repostCount += post->isReposted ? 1 : -1;
      }
    }
  }

  for (auto &post : newState.savedPosts.posts) {
    if (post->id == postId) {
      post->isReposted = !post->isReposted;
      post->repostCount += post->isReposted ? 1 : -1;
    }
  }

  for (auto &post : newState.archivedPosts.posts) {
    if (post->id == postId) {
      post->isReposted = !post->isReposted;
      post->repostCount += post->isReposted ? 1 : -1;
    }
  }
  sliceManager.posts->setState(newState);

  // Send to server
  if (isCurrentlyReposted) {
    networkClient->undoRepost(postId, [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to undo repost: " + result.getError());
      } else {
        // Invalidate feed caches on successful repost/undo repost
      }
    });
  } else {
    networkClient->repostPost(postId, "", [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to repost: " + result.getError());
      } else {
        // Invalidate feed caches on successful repost/undo repost
      }
    });
  }
}

void AppStore::addReaction(const juce::String &postId, const juce::String &emoji) {
  if (!networkClient) {
    return;
  }

  // Add a reaction by liking with an emoji
  networkClient->likePost(postId, emoji, [](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Util::logError("AppStore", "Failed to add reaction: " + result.getError());
    }
  });
}

void AppStore::toggleFollow(const juce::String &postId, bool willFollow) {
  if (!networkClient) {
    return;
  }

  // Extract user ID and current follow state from post
  auto currentPostsState = sliceManager.posts->getState();
  juce::String userId;
  bool previousFollowState = false;

  for (const auto &[feedType, feedState] : currentPostsState.feeds) {
    for (const auto &post : feedState.posts) {
      if (post->id == postId) {
        userId = post->userId;
        previousFollowState = post->isFollowing;
        break;
      }
    }
  }

  if (userId.isNotEmpty()) {
    // Apply optimistic update - toggle follow state immediately
    PostsState newState = sliceManager.posts->getState();
    for (auto &[feedType, feedState] : newState.feeds) {
      for (auto &post : feedState.posts) {
        if (post->id == postId) {
          post->isFollowing = willFollow;
        }
      }
    }
    sliceManager.posts->setState(newState);

    Util::logDebug("AppStore", "Follow post optimistic update: " + postId + " - " +
                                   juce::String(willFollow ? "follow" : "unfollow"));

    if (willFollow) {
      networkClient->followUser(userId, [this, postId, previousFollowState](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "User followed successfully: " + postId);
          // Invalidate feed caches on successful follow
        } else {
          Util::logError("AppStore", "Failed to follow user: " + result.getError());
          // Rollback optimistic update on error
          PostsState rollbackState = sliceManager.posts->getState();
          for (auto &[feedType, feedState] : rollbackState.feeds) {
            for (auto &post : feedState.posts) {
              if (post->id == postId) {
                post->isFollowing = previousFollowState;
              }
            }
          }
          sliceManager.posts->setState(rollbackState);
        }
      });
    } else {
      networkClient->unfollowUser(userId, [this, postId, previousFollowState](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "User unfollowed successfully: " + postId);
          // Invalidate feed caches on successful unfollow
        } else {
          Util::logError("AppStore", "Failed to unfollow user: " + result.getError());
          // Rollback optimistic update on error
          PostsState rollbackState = sliceManager.posts->getState();
          for (auto &[feedType, feedState] : rollbackState.feeds) {
            for (auto &post : feedState.posts) {
              if (post->id == postId) {
                post->isFollowing = previousFollowState;
              }
            }
          }
          sliceManager.posts->setState(rollbackState);
        }
      });
    }
  }
}

void AppStore::toggleMute(const juce::String &userId, bool willMute) {
  if (!networkClient) {
    return;
  }

  if (willMute) {
    networkClient->muteUser(userId, [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to mute user: " + result.getError());
      } else {
        // Invalidate feed caches on successful mute (feed may change)
      }
    });
  } else {
    networkClient->unmuteUser(userId, [](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to unmute user: " + result.getError());
      } else {
        // Invalidate feed caches on successful unmute (feed may change)
      }
    });
  }
}

void AppStore::togglePin(const juce::String &postId, bool pinned) {
  if (!networkClient) {
    return;
  }

  // Update UI optimistically
  PostsState newState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : newState.feeds) {
    for (auto &post : feedState.posts) {
      if (post->id == postId) {
        post->isPinned = pinned;
      }
    }
  }
  sliceManager.posts->setState(newState);

  // Call actual API to persist pin/unpin state
  if (pinned) {
    networkClient->pinPost(postId, nullptr);
  } else {
    networkClient->unpinPost(postId, nullptr);
  }

  Util::logInfo("AppStore", pinned ? "Pin post: " + postId : "Unpin post: " + postId);
}

// ==============================================================================
// Helper Methods

void AppStore::performFetch(FeedType feedType, int limit, int offset) {
  Util::logDebug("AppStore", "performFetch called for feedType=" + feedTypeToString(feedType) +
                                 ", limit=" + juce::String(limit) + ", offset=" + juce::String(offset));

  if (!networkClient) {
    Util::logError("AppStore", "performFetch: networkClient is null!");
    return;
  }

  auto callback = [this, feedType, limit, offset](Outcome<juce::var> result) {
    Log::debug("AppStore: performFetch callback invoked for feedType=" + feedTypeToString(feedType));
    if (result.isOk()) {
      Log::debug("AppStore: Feed response received, parsing...");
      handleFetchSuccess(feedType, result.getValue(), limit, offset);
    } else {
      Log::debug("AppStore: Feed request failed: " + result.getError());
      handleFetchError(feedType, result.getError());
    }
  };

  // Call the appropriate feed method based on feed type
  if (feedType == FeedType::Timeline) {
    Util::logDebug("AppStore", "performFetch: calling getTimelineFeed");
    networkClient->getTimelineFeed(limit, offset, callback);
  } else if (feedType == FeedType::Trending) {
    networkClient->getTrendingFeed(limit, offset, callback);
  } else if (feedType == FeedType::Global) {
    networkClient->getGlobalFeed(limit, offset, callback);
  } else if (feedType == FeedType::ForYou) {
    networkClient->getForYouFeed(limit, offset, callback);
  } else if (feedType == FeedType::Popular) {
    networkClient->getPopularFeed(limit, offset, callback);
  } else if (feedType == FeedType::Latest) {
    networkClient->getLatestFeed(limit, offset, callback);
  } else if (feedType == FeedType::Discovery) {
    networkClient->getDiscoveryFeed(limit, offset, callback);
  } else if (feedType == FeedType::TimelineAggregated) {
    networkClient->getAggregatedTimeline(limit, offset, callback);
  } else if (feedType == FeedType::TrendingAggregated) {
    networkClient->getTrendingFeedGrouped(limit, offset, callback);
  } else if (feedType == FeedType::NotificationAggregated) {
    networkClient->getNotificationsAggregated(limit, offset, callback);
  } else if (feedType == FeedType::UserActivityAggregated) {
    // For user activity, we would need the user ID - for now, skip
    Util::logWarning("AppStore", "UserActivityAggregated requires userId - skipping");
  }
}

void AppStore::handleFetchSuccess(FeedType feedType, const juce::var &data, int limit, int offset) {
  // Log pagination info for debugging
  Log::info("========== handleFetchSuccess ENTRY ==========");
  Log::info("handleFetchSuccess called for feedType=" + feedTypeToString(feedType));
  Log::debug("handleFetchSuccess parsing response");

  try {
    // Log pagination info for debugging
    Log::debug("handleFetchSuccess: feedType=" + feedTypeToString(feedType) + ", offset=" + juce::String(offset) +
               ", limit=" + juce::String(limit));

    // TODO: Implement aggregated feed handling
    // For now, skip aggregated feed types
    /*
    if (isAggregatedFeedType(feedType)) {
      auto response = parseAggregatedJsonResponse(data);

      sliceManager.posts->dispatch([feedType, response, offset](PostsState &s) {
        if (s.aggregatedFeeds.count(feedType) == 0) {
          s.aggregatedFeeds[feedType] = AggregatedFeedState();
        }

        auto &feedState = s.aggregatedFeeds[feedType];
        if (offset == 0) {
          feedState.groups = response.groups;
        } else {
          for (const auto &group : response.groups) {
            feedState.groups.add(group);
          }
        }

        feedState.isLoading = false;
        feedState.isRefreshing = false;
        feedState.offset = offset + response.groups.size();
        feedState.total = response.total;
        feedState.hasMore = feedState.offset < feedState.total;
        feedState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
        feedState.error = "";
        feedState.isSynced = true;
      });
    } else {*/

    Log::debug("========== About to call parseJsonResponse ==========");
    auto response = parseJsonResponse(data);
    Log::debug("========== parseJsonResponse COMPLETE, got " + juce::String(response.posts.size()) +
               " posts ==========");

    // Validate response size against requested limit
    const size_t responseSize = static_cast<size_t>(response.posts.size());
    const size_t expectedLimit = static_cast<size_t>(limit);
    if (responseSize > expectedLimit) {
      Log::warn("AppStore: Response size (" + juce::String(static_cast<int>(responseSize)) +
                ") exceeds requested limit (" + juce::String(limit) + ")");
    }

    // Cache the feed data in memory cache (5-minute TTL)
    Log::debug("========== About to cache feed data ==========");
    auto cacheKey = "feed:" + feedTypeToString(feedType).toLowerCase();
    Log::debug("========== Feed cached successfully ==========");

    Log::debug("========== About to call updateFeedState ==========");
    PostsState newState = sliceManager.posts->getState();
    if (newState.feeds.count(feedType) == 0) {
      newState.feeds[feedType] = FeedState();
    }

    auto &feedState = newState.feeds[feedType];
    if (offset == 0) {
      feedState.posts.clear();
    }
    // Convert juce::Array<FeedPost> to vector<shared_ptr<FeedPost>>
    for (const auto &post : response.posts) {
      feedState.posts.push_back(std::make_shared<FeedPost>(post));
    }

    feedState.isLoading = false;
    feedState.isRefreshing = false;
    feedState.offset = offset + response.posts.size();
    feedState.total = response.total;
    feedState.hasMore = response.hasMore; // Use has_more from API response
    feedState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
    feedState.error = "";
    feedState.isSynced = true;
    sliceManager.posts->setState(newState);
    Log::debug("========== updateFeedState COMPLETE ==========");
    // }

    Log::info("========== handleFetchSuccess COMPLETE ==========");
    Log::debug("Loaded feed for feedType=" + feedTypeToString(feedType));
  } catch (const std::exception &e) {
    Log::error("========== EXCEPTION in handleFetchSuccess: " + juce::String(e.what()) + " ==========");
    Log::error("Exception in handleFetchSuccess: " + juce::String(e.what()));
  } catch (...) {
    Log::error("========== UNKNOWN EXCEPTION in handleFetchSuccess ==========");
    Log::error("Unknown exception in handleFetchSuccess");
  }
}

void AppStore::handleFetchError(FeedType feedType, const juce::String &error) {
  Util::logError("AppStore", "Failed to load feed: " + error);

  PostsState newState = sliceManager.posts->getState();
  // TODO: Handle aggregated feed types
  // For now, only handle regular feeds
  if (newState.feeds.count(feedType) > 0) {
    newState.feeds[feedType].isLoading = false;
    newState.feeds[feedType].isRefreshing = false;
    newState.feeds[feedType].error = error;
  }
  sliceManager.posts->setState(newState);
}

void AppStore::handleSavedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.savedPosts.isLoading = false;
    errorState.savedPosts.error = result.getError();
    sliceManager.posts->setState(errorState);
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.savedPosts.isLoading = false;
    errorState.savedPosts.error = "Invalid saved posts response";
    sliceManager.posts->setState(errorState);
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.savedPosts.isLoading = false;
    errorState.savedPosts.error = "Invalid posts array in response";
    sliceManager.posts->setState(errorState);
    return;
  }

  juce::Array<FeedPost> loadedPosts;
  for (int i = 0; i < postsArray.size(); ++i) {
    try {
      auto jsonStr = juce::JSON::toString(postsArray[i]);
      auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
      auto postResult = SerializableModel<FeedPost>::createFromJson(jsonObj);
      if (postResult.isOk()) {
        auto post = *postResult.getValue();
        if (post.isValid()) {
          loadedPosts.add(post);
        }
      }
    } catch (const std::exception &) {
      // Skip invalid posts
    }
  }

  PostsState successState = sliceManager.posts->getState();
  // Convert juce::Array<FeedPost> to vector<shared_ptr<FeedPost>>
  successState.savedPosts.posts.clear();
  for (const auto &post : loadedPosts) {
    successState.savedPosts.posts.push_back(std::make_shared<FeedPost>(post));
  }
  successState.savedPosts.isLoading = false;
  successState.savedPosts.totalCount = totalCount;
  successState.savedPosts.offset += loadedPosts.size();
  successState.savedPosts.hasMore = successState.savedPosts.offset < totalCount;
  successState.savedPosts.error = "";
  successState.savedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  sliceManager.posts->setState(successState);

  Util::logDebug("AppStore", "Loaded " + juce::String(loadedPosts.size()) + " saved posts");
}

void AppStore::handleArchivedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.archivedPosts.isLoading = false;
    errorState.archivedPosts.error = result.getError();
    sliceManager.posts->setState(errorState);
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.archivedPosts.isLoading = false;
    errorState.archivedPosts.error = "Invalid archived posts response";
    sliceManager.posts->setState(errorState);
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    PostsState errorState = sliceManager.posts->getState();
    errorState.archivedPosts.isLoading = false;
    errorState.archivedPosts.error = "Invalid posts array in response";
    sliceManager.posts->setState(errorState);
    return;
  }

  juce::Array<FeedPost> loadedPosts;
  for (int i = 0; i < postsArray.size(); ++i) {
    try {
      auto jsonStr = juce::JSON::toString(postsArray[i]);
      auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
      auto postResult = SerializableModel<FeedPost>::createFromJson(jsonObj);
      if (postResult.isOk()) {
        auto post = *postResult.getValue();
        if (post.isValid()) {
          loadedPosts.add(post);
        }
      }
    } catch (const std::exception &) {
      // Skip invalid posts
    }
  }

  PostsState successState = sliceManager.posts->getState();
  // Convert juce::Array<FeedPost> to vector<shared_ptr<FeedPost>>
  successState.archivedPosts.posts.clear();
  for (const auto &post : loadedPosts) {
    successState.archivedPosts.posts.push_back(std::make_shared<FeedPost>(post));
  }
  successState.archivedPosts.isLoading = false;
  successState.archivedPosts.totalCount = totalCount;
  successState.archivedPosts.offset += loadedPosts.size();
  successState.archivedPosts.hasMore = successState.archivedPosts.offset < totalCount;
  successState.archivedPosts.error = "";
  successState.archivedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  sliceManager.posts->setState(successState);

  Util::logDebug("AppStore", "Loaded " + juce::String(loadedPosts.size()) + " archived posts");
}

bool AppStore::isCurrentFeedCached() const {
  auto state = sliceManager.posts->getState();
  auto feedType = state.currentFeedType;
  auto now = juce::Time::getCurrentTime().toMilliseconds();
  const int cacheTTLSeconds = 300; // 5 minutes

  if (isAggregatedFeedType(feedType)) {
    // TODO: Implement proper aggregated feed caching with timestamp tracking
    // For now, aggregated feeds are not cached
    return false;
  } else {
    if (state.feeds.count(feedType) > 0) {
      auto &feedState = state.feeds.at(feedType);
      auto ageMs = now - feedState.lastUpdated;
      auto ageSecs = ageMs / 1000;
      return !feedState.posts.empty() && ageSecs < cacheTTLSeconds;
    }
  }

  return false;
}

FeedResponse AppStore::parseJsonResponse(const juce::var &json) {
  FeedResponse response;

  if (!json.isObject()) {
    return response;
  }

  // Try "activities" first (unified feed format), then "posts" (fallback for other endpoints)
  auto postsArray = json.getProperty("activities", juce::var());
  if (!postsArray.isArray()) {
    postsArray = json.getProperty("posts", juce::var());
  }

  // Extract total from meta.count or total field
  auto metaObj = json.getProperty("meta", juce::var());
  if (metaObj.isObject() && metaObj.hasProperty("count")) {
    response.total = static_cast<int>(metaObj.getProperty("count", 0));
  } else {
    response.total = static_cast<int>(json.getProperty("total", 0));
  }

  // Extract has_more flag for pagination
  if (metaObj.isObject()) {
    response.hasMore = metaObj.getProperty("has_more", false);
  }

  if (postsArray.isArray()) {
    for (int i = 0; i < postsArray.size(); ++i) {
      try {
        auto jsonStr = juce::JSON::toString(postsArray[i]);
        auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
        auto result = SerializableModel<FeedPost>::createFromJson(jsonObj);
        if (result.isOk()) {
          auto post = *result.getValue();
          if (post.isValid()) {
            response.posts.add(post);
          }
        }
      } catch (const std::exception &) {
        // Skip invalid posts
      }
    }
  }

  Util::logDebug("AppStore", "Parsed " + juce::String(response.posts.size()) + " posts from feed response");
  return response;
}

AggregatedFeedResponse AppStore::parseAggregatedJsonResponse(const juce::var &json) {
  AggregatedFeedResponse response;

  if (!json.isObject()) {
    return response;
  }

  auto groupsArray = json.getProperty("groups", juce::var());
  response.total = static_cast<int>(json.getProperty("total", 0));

  if (groupsArray.isArray()) {
    for (int i = 0; i < groupsArray.size(); ++i) {
      auto group = AggregatedFeedGroup::fromJson(groupsArray[i]);
      response.groups.add(group);
    }
  }

  return response;
}

// ==============================================================================
// Phase 4: Reactive Feed Service Operations with Memory Caching

// These methods implement loadFeedObservable and likePostObservable
// using RxCpp observables with automatic cache invalidation strategies.

rxcpp::observable<juce::var> AppStore::loadFeedObservable(FeedType feedType) {
  return rxcpp::sources::create<juce::var>([this, feedType](auto observer) {
    // Fetch from network
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_next(juce::var());
      observer.on_completed();
      return;
    }

    Util::logDebug("AppStore", "Loading feed from network: " + feedTypeToString(feedType));

    // Create callback to handle network response
    auto callback = [feedType, observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        // Cache the response (30 seconds for feeds - they update frequently)
        auto data = result.getValue();
        Util::logInfo("AppStore", "Feed loaded and cached: " + feedTypeToString(feedType));
        observer.on_next(data);
      } else {
        Util::logError("AppStore", "Feed load failed: " + result.getError());
        observer.on_next(juce::var());
      }
      observer.on_completed();
    };

    // Call the appropriate feed method based on feed type
    if (feedType == FeedType::Timeline) {
      networkClient->getTimelineFeed(20, 0, callback);
    } else if (feedType == FeedType::Trending) {
      networkClient->getTrendingFeed(20, 0, callback);
    } else if (feedType == FeedType::Global) {
      networkClient->getGlobalFeed(20, 0, callback);
    } else if (feedType == FeedType::ForYou) {
      networkClient->getForYouFeed(20, 0, callback);
    } else if (feedType == FeedType::Popular) {
      networkClient->getPopularFeed(20, 0, callback);
    } else if (feedType == FeedType::Latest) {
      networkClient->getLatestFeed(20, 0, callback);
    } else if (feedType == FeedType::Discovery) {
      networkClient->getDiscoveryFeed(20, 0, callback);
    } else if (feedType == FeedType::TimelineAggregated) {
      networkClient->getAggregatedTimeline(20, 0, callback);
    } else if (feedType == FeedType::TrendingAggregated) {
      networkClient->getTrendingFeedGrouped(20, 0, callback);
    } else if (feedType == FeedType::NotificationAggregated) {
      networkClient->getNotificationsAggregated(20, 0, callback);
    } else if (feedType == FeedType::UserActivityAggregated) {
      Util::logWarning("AppStore", "UserActivityAggregated requires userId - skipping");
      observer.on_next(juce::var());
      observer.on_completed();
    } else {
      Util::logError("AppStore", "Unknown feed type");
      observer.on_next(juce::var());
      observer.on_completed();
    }
  });
}

rxcpp::observable<int> AppStore::likePostObservable(const juce::String &postId) {
  return rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
      return;
    }

    // Check current like state from app state
    auto currentPostsState = sliceManager.posts->getState();
    bool isCurrentlyLiked = false;

    for (const auto &[feedType, feedState] : currentPostsState.feeds) {
      for (const auto &post : feedState.posts) {
        if (post->id == postId) {
          isCurrentlyLiked = post->isLiked;
          break;
        }
      }
    }

    // Store previous state for rollback on error
    auto previousState = isCurrentlyLiked;

    // Apply optimistic update
    PostsState newState = sliceManager.posts->getState();
    for (auto &[feedType, feedState] : newState.feeds) {
      for (auto &post : feedState.posts) {
        if (post->id == postId) {
          post->isLiked = !post->isLiked;
          post->likeCount += post->isLiked ? 1 : -1;
        }
      }
    }
    sliceManager.posts->setState(newState);

    Util::logDebug("AppStore", "Like post optimistic update: " + postId);

    // Send to server - like or unlike based on previous state
    if (isCurrentlyLiked) {
      networkClient->unlikePost(postId, [this, postId, previousState, observer](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "Post unliked successfully: " + postId);
          // Invalidate feed caches on successful unlike
          observer.on_next(0);
          observer.on_completed();
        } else {
          Util::logError("AppStore", "Failed to unlike post: " + result.getError());
          // Rollback optimistic update on error
          PostsState rollbackState = sliceManager.posts->getState();
          for (auto &[feedType, feedState] : rollbackState.feeds) {
            for (auto &post : feedState.posts) {
              if (post->id == postId) {
                post->isLiked = previousState;
                post->likeCount += previousState ? 1 : -1;
              }
            }
          }
          sliceManager.posts->setState(rollbackState);
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
        }
      });
    } else {
      networkClient->likePost(postId, "", [this, postId, previousState, observer](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "Post liked successfully: " + postId);
          // Invalidate feed caches on successful like
          observer.on_next(0);
          observer.on_completed();
        } else {
          Util::logError("AppStore", "Failed to like post: " + result.getError());
          // Rollback optimistic update on error
          PostsState rollbackState = sliceManager.posts->getState();
          for (auto &[feedType, feedState] : rollbackState.feeds) {
            for (auto &post : feedState.posts) {
              if (post->id == postId) {
                post->isLiked = previousState;
                post->likeCount += previousState ? 1 : -1;
              }
            }
          }
          sliceManager.posts->setState(rollbackState);
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
        }
      });
    }
  });
}

// ==============================================================================
// Additional Reactive Observable Methods

rxcpp::observable<int> AppStore::toggleSaveObservable(const juce::String &postId) {
  return rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
      return;
    }

    // Check current save state
    auto currentPostsState = sliceManager.posts->getState();
    bool isCurrentlySaved = false;

    for (const auto &[feedType, feedState] : currentPostsState.feeds) {
      for (const auto &post : feedState.posts) {
        if (post->id == postId) {
          isCurrentlySaved = post->isSaved;
          break;
        }
      }
    }

    // Store previous state for rollback on error
    auto previousState = isCurrentlySaved;

    // Apply optimistic update
    PostsState newState = sliceManager.posts->getState();
    for (auto &[feedType, feedState] : newState.feeds) {
      for (auto &post : feedState.posts) {
        if (post->id == postId) {
          post->isSaved = !post->isSaved;
          post->saveCount += post->isSaved ? 1 : -1;
        }
      }
    }

    for (auto &post : newState.savedPosts.posts) {
      if (post->id == postId) {
        post->isSaved = !post->isSaved;
        post->saveCount += post->isSaved ? 1 : -1;
      }
    }

    for (auto &post : newState.archivedPosts.posts) {
      if (post->id == postId) {
        post->isSaved = !post->isSaved;
        post->saveCount += post->isSaved ? 1 : -1;
      }
    }
    sliceManager.posts->setState(newState);

    Util::logDebug("AppStore", "Toggle save optimistic update: " + postId);

    // Send to server
    if (previousState) {
      networkClient->unsavePost(postId, [this, postId, previousState, observer](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "Post unsaved successfully: " + postId);
          observer.on_next(0);
          observer.on_completed();
        } else {
          Util::logError("AppStore", "Failed to unsave post: " + result.getError());
          // Rollback optimistic update on error
          PostsState rollbackState = sliceManager.posts->getState();
          for (auto &[feedType, feedState] : rollbackState.feeds) {
            for (auto &post : feedState.posts) {
              if (post->id == postId) {
                post->isSaved = previousState;
                post->saveCount += previousState ? 1 : -1;
              }
            }
          }
          for (auto &post : rollbackState.savedPosts.posts) {
            if (post->id == postId) {
              post->isSaved = previousState;
              post->saveCount += previousState ? 1 : -1;
            }
          }
          for (auto &post : rollbackState.archivedPosts.posts) {
            if (post->id == postId) {
              post->isSaved = previousState;
              post->saveCount += previousState ? 1 : -1;
            }
          }
          sliceManager.posts->setState(rollbackState);
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
        }
      });
    } else {
      networkClient->savePost(postId, [this, postId, previousState, observer](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "Post saved successfully: " + postId);
          observer.on_next(0);
          observer.on_completed();
        } else {
          Util::logError("AppStore", "Failed to save post: " + result.getError());
          // Rollback optimistic update on error
          PostsState rollbackState = sliceManager.posts->getState();
          for (auto &[feedType, feedState] : rollbackState.feeds) {
            for (auto &post : feedState.posts) {
              if (post->id == postId) {
                post->isSaved = previousState;
                post->saveCount += previousState ? 1 : -1;
              }
            }
          }
          for (auto &post : rollbackState.savedPosts.posts) {
            if (post->id == postId) {
              post->isSaved = previousState;
              post->saveCount += previousState ? 1 : -1;
            }
          }
          for (auto &post : rollbackState.archivedPosts.posts) {
            if (post->id == postId) {
              post->isSaved = previousState;
              post->saveCount += previousState ? 1 : -1;
            }
          }
          sliceManager.posts->setState(rollbackState);
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
        }
      });
    }
  });
}

rxcpp::observable<int> AppStore::toggleRepostObservable(const juce::String &postId) {
  return rxcpp::sources::create<int>([this, postId](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
      return;
    }

    // Call the existing callback-based method for now - it handles optimistic updates and cache invalidation
    toggleRepost(postId);
    observer.on_next(0);
    observer.on_completed();
  });
}

rxcpp::observable<int> AppStore::togglePinObservable(const juce::String &postId, bool pinned) {
  return rxcpp::sources::create<int>([this, postId, pinned](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
      return;
    }

    // Call the existing callback-based method for now - it handles optimistic updates and cache invalidation
    togglePin(postId, pinned);
    observer.on_next(0);
    observer.on_completed();
  });
}

rxcpp::observable<int> AppStore::toggleFollowObservable(const juce::String &postId, bool willFollow) {
  return rxcpp::sources::create<int>([this, postId, willFollow](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
      return;
    }

    // Call the existing callback-based method for now - it handles optimistic updates and cache invalidation
    toggleFollow(postId, willFollow);
    observer.on_next(0);
    observer.on_completed();
  });
}

rxcpp::observable<int> AppStore::addReactionObservable(const juce::String &postId, const juce::String &emoji) {
  return rxcpp::sources::create<int>([this, postId, emoji](auto observer) {
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
      return;
    }

    // Call the existing callback-based method for now - it handles optimistic updates and cache invalidation
    addReaction(postId, emoji);
    observer.on_next(0);
    observer.on_completed();
  });
}

} // namespace Stores
} // namespace Sidechain
