#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
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

//==============================================================================
// Feed Loading

void AppStore::loadFeed(FeedType feedType, bool forceRefresh) {
  if (!networkClient) {
    updateFeedState([feedType](PostsState &state) {
      state.currentFeedType = feedType;
      state.feedError = "Network client not initialized";
    });
    return;
  }

  Util::logInfo("AppStore", "Loading feed", "feedType=" + feedTypeToString(feedType));

  // Check memory cache first (if not force refreshing)
  if (!forceRefresh) {
    // Try memory cache with 5-minute TTL for feed data
    auto cacheKey = "feed:" + feedTypeToString(feedType).toLowerCase();
    auto cachedFeed = getCached<FeedResponse>(cacheKey);
    if (cachedFeed.has_value()) {
      Util::logDebug("AppStore", "Using cached feed from memory", "feedType=" + feedTypeToString(feedType));
      handleFetchSuccess(feedType, juce::var(), 0, 0);
      return;
    }

    // Also check state-based cache as fallback
    if (isCurrentFeedCached()) {
      auto state = getState();
      if (state.posts.feeds.count(feedType) > 0 && !state.posts.feeds.at(feedType).posts.isEmpty()) {
        currentFeedIsFromCache_ = true;
        Util::logDebug("AppStore", "Using cached feed from state", "feedType=" + feedTypeToString(feedType));
        return;
      }
    }
  }

  // Update state to loading
  updateFeedState([feedType](PostsState &state) {
    state.currentFeedType = feedType;
    if (state.feeds.count(feedType) == 0) {
      state.feeds[feedType] = FeedState();
    }
    state.feeds[feedType].isLoading = true;
    state.feeds[feedType].error = "";
  });

  // Fetch from network
  performFetch(feedType, 20, 0);
}

void AppStore::refreshCurrentFeed() {
  auto currentState = getState();
  loadFeed(currentState.posts.currentFeedType, true);
}

void AppStore::loadMore() {
  auto currentState = getState();
  auto feedType = currentState.posts.currentFeedType;

  if (currentState.posts.feeds.count(feedType) == 0) {
    Util::logWarning("AppStore", "Cannot load more - feed not initialized");
    return;
  }

  auto &feedState = currentState.posts.feeds.at(feedType);
  if (!feedState.hasMore || feedState.isLoading || !networkClient) {
    return;
  }

  updateFeedState([feedType](PostsState &state) {
    if (state.feeds.count(feedType) > 0) {
      state.feeds[feedType].isLoading = true;
    }
  });

  performFetch(feedType, feedState.limit, feedState.offset);
}

void AppStore::switchFeedType(FeedType feedType) {
  updateFeedState([feedType](PostsState &state) { state.currentFeedType = feedType; });

  // Load the new feed if not already loaded
  auto currentState = getState();
  if (currentState.posts.feeds[feedType].posts.isEmpty()) {
    loadFeed(feedType, false);
  }
}

//==============================================================================
// Saved Posts

void AppStore::loadSavedPosts() {
  if (!networkClient) {
    updateFeedState([](PostsState &state) { state.savedPosts.error = "Network client not initialized"; });
    return;
  }

  Util::logInfo("AppStore", "Loading saved posts");

  updateFeedState([](PostsState &state) {
    state.savedPosts.isLoading = true;
    state.savedPosts.offset = 0;
    state.savedPosts.posts.clear();
    state.savedPosts.error = "";
  });

  networkClient->getSavedPosts(20, 0, [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void AppStore::loadMoreSavedPosts() {
  auto currentState = getState();
  if (!currentState.posts.savedPosts.hasMore || currentState.posts.savedPosts.isLoading || !networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Loading more saved posts");

  updateFeedState([](PostsState &state) { state.savedPosts.isLoading = true; });

  networkClient->getSavedPosts(currentState.posts.savedPosts.limit, currentState.posts.savedPosts.offset,
                               [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void AppStore::unsavePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Unsaving post: " + postId);

  // Optimistic removal from saved posts
  updateFeedState([postId](PostsState &state) {
    for (int i = 0; i < state.savedPosts.posts.size(); ++i) {
      if (state.savedPosts.posts[i].id == postId) {
        state.savedPosts.posts.remove(i);
        break;
      }
    }
  });

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

//==============================================================================
// Archived Posts

void AppStore::loadArchivedPosts() {
  if (!networkClient) {
    updateFeedState([](PostsState &state) { state.archivedPosts.error = "Network client not initialized"; });
    return;
  }

  Util::logInfo("AppStore", "Loading archived posts");

  updateFeedState([](PostsState &state) {
    state.archivedPosts.isLoading = true;
    state.archivedPosts.offset = 0;
    state.archivedPosts.posts.clear();
    state.archivedPosts.error = "";
  });

  networkClient->getArchivedPosts(20, 0, [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void AppStore::loadMoreArchivedPosts() {
  auto currentState = getState();
  if (!currentState.posts.archivedPosts.hasMore || currentState.posts.archivedPosts.isLoading || !networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Loading more archived posts");

  updateFeedState([](PostsState &state) { state.archivedPosts.isLoading = true; });

  // TODO: Implement archived posts loading via AppStore
  // networkClient->getArchivedPosts(20, currentState.posts.archivedPosts.offset,
  //                                 [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void AppStore::restorePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Restoring post: " + postId);

  // Optimistic removal from archived posts
  updateFeedState([postId](PostsState &state) {
    for (int i = 0; i < state.archivedPosts.posts.size(); ++i) {
      if (state.archivedPosts.posts[i].id == postId) {
        state.archivedPosts.posts.remove(i);
        break;
      }
    }
  });

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

//==============================================================================
// Post Interactions

void AppStore::toggleLike(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  // Check current like state to determine whether to like or unlike
  auto state = getState();
  bool isCurrentlyLiked = false;

  for (const auto &[feedType, feedState] : state.posts.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        isCurrentlyLiked = post.isLiked;
        break;
      }
    }
  }

  // Optimistic update
  updateFeedState([postId, isCurrentlyLiked](PostsState &state) {
    // Update all occurrences of the post across all feeds
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post.id == postId) {
          post.isLiked = !post.isLiked;
          post.likeCount += post.isLiked ? 1 : -1;
        }
      }
    }

    for (auto &post : state.savedPosts.posts) {
      if (post.id == postId) {
        post.isLiked = !post.isLiked;
        post.likeCount += post.isLiked ? 1 : -1;
      }
    }

    for (auto &post : state.archivedPosts.posts) {
      if (post.id == postId) {
        post.isLiked = !post.isLiked;
        post.likeCount += post.isLiked ? 1 : -1;
      }
    }
  });

  // Send to server - like or unlike based on previous state
  if (isCurrentlyLiked) {
    networkClient->unlikePost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to unlike post: " + result.getError());
      } else {
        // Invalidate feed caches on successful like/unlike
        invalidateCachePattern("feed:*");
      }
    });
  } else {
    networkClient->likePost(postId, "", [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to like post: " + result.getError());
      } else {
        // Invalidate feed caches on successful like/unlike
        invalidateCachePattern("feed:*");
      }
    });
  }
}

void AppStore::toggleSave(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  // Check current save state
  auto state = getState();
  bool isCurrentlySaved = false;

  for (const auto &[feedType, feedState] : state.posts.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        isCurrentlySaved = post.isSaved;
        break;
      }
    }
  }

  // Optimistic update
  updateFeedState([postId, isCurrentlySaved](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post.id == postId) {
          post.isSaved = !post.isSaved;
          post.saveCount += post.isSaved ? 1 : -1;
        }
      }
    }

    for (auto &post : state.savedPosts.posts) {
      if (post.id == postId) {
        post.isSaved = !post.isSaved;
        post.saveCount += post.isSaved ? 1 : -1;
      }
    }

    for (auto &post : state.archivedPosts.posts) {
      if (post.id == postId) {
        post.isSaved = !post.isSaved;
        post.saveCount += post.isSaved ? 1 : -1;
      }
    }
  });

  // Send to server
  if (isCurrentlySaved) {
    networkClient->unsavePost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to unsave post: " + result.getError());
      } else {
        // Invalidate feed caches on successful save/unsave
        invalidateCachePattern("feed:*");
      }
    });
  } else {
    networkClient->savePost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to save post: " + result.getError());
      } else {
        // Invalidate feed caches on successful save/unsave
        invalidateCachePattern("feed:*");
      }
    });
  }
}

void AppStore::toggleRepost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  // Check current repost state
  auto state = getState();
  bool isCurrentlyReposted = false;

  for (const auto &[feedType, feedState] : state.posts.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        isCurrentlyReposted = post.isReposted;
        break;
      }
    }
  }

  // Optimistic update
  updateFeedState([postId, isCurrentlyReposted](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post.id == postId) {
          post.isReposted = !post.isReposted;
          post.repostCount += post.isReposted ? 1 : -1;
        }
      }
    }

    for (auto &post : state.savedPosts.posts) {
      if (post.id == postId) {
        post.isReposted = !post.isReposted;
        post.repostCount += post.isReposted ? 1 : -1;
      }
    }

    for (auto &post : state.archivedPosts.posts) {
      if (post.id == postId) {
        post.isReposted = !post.isReposted;
        post.repostCount += post.isReposted ? 1 : -1;
      }
    }
  });

  // Send to server
  if (isCurrentlyReposted) {
    networkClient->undoRepost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to undo repost: " + result.getError());
      } else {
        // Invalidate feed caches on successful repost/undo repost
        invalidateCachePattern("feed:*");
      }
    });
  } else {
    networkClient->repostPost(postId, "", [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to repost: " + result.getError());
      } else {
        // Invalidate feed caches on successful repost/undo repost
        invalidateCachePattern("feed:*");
      }
    });
  }
}

void AppStore::addReaction(const juce::String &postId, const juce::String &emoji) {
  if (!networkClient) {
    return;
  }

  // Add a reaction by liking with an emoji
  networkClient->likePost(postId, emoji, [this](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Util::logError("AppStore", "Failed to add reaction: " + result.getError());
    }
  });
}

void AppStore::toggleFollow(const juce::String &postId, bool willFollow) {
  if (!networkClient) {
    return;
  }

  // Extract user ID from post to follow the author
  auto state = getState();
  juce::String userId;

  for (const auto &[feedType, feedState] : state.posts.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        userId = post.userId;
        break;
      }
    }
  }

  if (!userId.isEmpty()) {
    if (willFollow) {
      networkClient->followUser(userId, [this](Outcome<juce::var> result) {
        if (!result.isOk()) {
          Util::logError("AppStore", "Failed to follow user: " + result.getError());
        }
      });
    } else {
      networkClient->unfollowUser(userId, [this](Outcome<juce::var> result) {
        if (!result.isOk()) {
          Util::logError("AppStore", "Failed to unfollow user: " + result.getError());
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
    networkClient->muteUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to mute user: " + result.getError());
      } else {
        // Invalidate feed caches on successful mute (feed may change)
        invalidateCachePattern("feed:*");
      }
    });
  } else {
    networkClient->unmuteUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("AppStore", "Failed to unmute user: " + result.getError());
      } else {
        // Invalidate feed caches on successful unmute (feed may change)
        invalidateCachePattern("feed:*");
      }
    });
  }
}

void AppStore::togglePin(const juce::String &postId, bool pinned) {
  if (!networkClient) {
    return;
  }

  // Update UI optimistically
  updateFeedState([postId, pinned](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post.id == postId) {
          post.isPinned = pinned;
        }
      }
    }
  });

  // TODO: Call actual API when NetworkClient has pin/unpin methods
  Util::logInfo("AppStore", pinned ? "Pin post: " + postId : "Unpin post: " + postId);
}

//==============================================================================
// Helper Methods

void AppStore::performFetch(FeedType feedType, int limit, int offset) {
  if (!networkClient) {
    return;
  }

  auto callback = [this, feedType, limit, offset](Outcome<juce::var> result) {
    if (result.isOk()) {
      handleFetchSuccess(feedType, result.getValue(), limit, offset);
    } else {
      handleFetchError(feedType, result.getError());
    }
  };

  // Call the appropriate feed method based on feed type
  if (feedType == FeedType::Timeline) {
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

void AppStore::handleFetchSuccess(FeedType feedType, const juce::var &data, [[maybe_unused]] int limit, int offset) {
  // TODO: Implement aggregated feed handling
  // For now, skip aggregated feed types
  /*
  if (isAggregatedFeedType(feedType)) {
    auto response = parseAggregatedJsonResponse(data);

    updateFeedState([feedType, response, offset](PostsState &s) {
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
  auto response = parseJsonResponse(data);

  // Cache the feed data in memory cache (5-minute TTL)
  auto cacheKey = "feed:" + feedTypeToString(feedType).toLowerCase();
  setCached<FeedResponse>(cacheKey, response, 300);

  updateFeedState([feedType, response, offset](PostsState &s) {
    if (s.feeds.count(feedType) == 0) {
      s.feeds[feedType] = FeedState();
    }

    auto &feedState = s.feeds[feedType];
    if (offset == 0) {
      feedState.posts = response.posts;
    } else {
      for (const auto &post : response.posts) {
        feedState.posts.add(post);
      }
    }

    feedState.isLoading = false;
    feedState.isRefreshing = false;
    feedState.offset = offset + response.posts.size();
    feedState.total = response.total;
    feedState.hasMore = feedState.offset < feedState.total;
    feedState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
    feedState.error = "";
    feedState.isSynced = true;
  });
  //}

  Util::logDebug("AppStore", "Loaded feed", "feedType=" + feedTypeToString(feedType));
}

void AppStore::handleFetchError(FeedType feedType, const juce::String &error) {
  Util::logError("AppStore", "Failed to load feed: " + error);

  updateFeedState([feedType, error](PostsState &s) {
    // TODO: Handle aggregated feed types
    // For now, only handle regular feeds
    if (s.feeds.count(feedType) > 0) {
      s.feeds[feedType].isLoading = false;
      s.feeds[feedType].isRefreshing = false;
      s.feeds[feedType].error = error;
    }
  });
}

void AppStore::handleSavedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateFeedState([error = result.getError()](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateFeedState([](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = "Invalid saved posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateFeedState([](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = "Invalid posts array in response";
    });
    return;
  }

  juce::Array<FeedPost> loadedPosts;
  for (int i = 0; i < postsArray.size(); ++i) {
    auto post = FeedPost::fromJson(postsArray[i]);
    if (post.isValid()) {
      loadedPosts.add(post);
    }
  }

  updateFeedState([loadedPosts, totalCount](PostsState &s) {
    s.savedPosts.posts = loadedPosts;
    s.savedPosts.isLoading = false;
    s.savedPosts.totalCount = totalCount;
    s.savedPosts.offset += loadedPosts.size();
    s.savedPosts.hasMore = s.savedPosts.offset < totalCount;
    s.savedPosts.error = "";
    s.savedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("AppStore", "Loaded " + juce::String(loadedPosts.size()) + " saved posts");
}

void AppStore::handleArchivedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateFeedState([error = result.getError()](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateFeedState([](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = "Invalid archived posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateFeedState([](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = "Invalid posts array in response";
    });
    return;
  }

  juce::Array<FeedPost> loadedPosts;
  for (int i = 0; i < postsArray.size(); ++i) {
    auto post = FeedPost::fromJson(postsArray[i]);
    if (post.isValid()) {
      loadedPosts.add(post);
    }
  }

  updateFeedState([loadedPosts, totalCount](PostsState &s) {
    s.archivedPosts.posts = loadedPosts;
    s.archivedPosts.isLoading = false;
    s.archivedPosts.totalCount = totalCount;
    s.archivedPosts.offset += loadedPosts.size();
    s.archivedPosts.hasMore = s.archivedPosts.offset < totalCount;
    s.archivedPosts.error = "";
    s.archivedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("AppStore", "Loaded " + juce::String(loadedPosts.size()) + " archived posts");
}

bool AppStore::isCurrentFeedCached() const {
  auto state = getState();
  auto feedType = state.posts.currentFeedType;
  auto now = juce::Time::getCurrentTime().toMilliseconds();
  const int cacheTTLSeconds = 300; // 5 minutes

  if (isAggregatedFeedType(feedType)) {
    // TODO: Implement proper aggregated feed caching with timestamp tracking
    // For now, aggregated feeds are not cached
    return false;
  } else {
    if (state.posts.feeds.count(feedType) > 0) {
      auto &feedState = state.posts.feeds.at(feedType);
      auto ageMs = now - feedState.lastUpdated;
      auto ageSecs = ageMs / 1000;
      return !feedState.posts.isEmpty() && ageSecs < cacheTTLSeconds;
    }
  }

  return false;
}

FeedResponse AppStore::parseJsonResponse(const juce::var &json) {
  FeedResponse response;

  if (!json.isObject()) {
    return response;
  }

  auto postsArray = json.getProperty("posts", juce::var());
  response.total = static_cast<int>(json.getProperty("total", 0));

  if (postsArray.isArray()) {
    for (int i = 0; i < postsArray.size(); ++i) {
      auto post = FeedPost::fromJson(postsArray[i]);
      if (post.isValid()) {
        response.posts.add(post);
      }
    }
  }

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

//==============================================================================
// Phase 4: Reactive Feed Service Operations with Memory Caching
//
// These methods implement loadFeedObservable() and likePostObservable()
// using RxCpp observables with automatic cache invalidation strategies.

rxcpp::observable<juce::var> AppStore::loadFeedObservable(FeedType feedType) {
  return rxcpp::sources::create<juce::var>([this, feedType](auto observer) {
    // Check memory cache first (30-second TTL for feeds)
    auto cacheKey = "feed:" + feedTypeToString(feedType).toLowerCase();
    if (auto cached = getCached<juce::var>(cacheKey)) {
      Util::logDebug("AppStore", "Feed cache hit: " + cacheKey);
      observer.on_next(*cached);
      observer.on_completed();
      return;
    }

    // Cache miss - fetch from network
    if (!networkClient) {
      Util::logError("AppStore", "Network client not initialized");
      observer.on_next(juce::var());
      observer.on_completed();
      return;
    }

    Util::logDebug("AppStore", "Loading feed from network: " + feedTypeToString(feedType));

    // Create callback to handle network response
    auto callback = [this, feedType, cacheKey, observer](Outcome<juce::var> result) {
      if (result.isOk()) {
        // Cache the response (30 seconds for feeds - they update frequently)
        auto data = result.getValue();
        setCached<juce::var>(cacheKey, data, 30);
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
    auto state = getState();
    bool isCurrentlyLiked = false;

    for (const auto &[feedType, feedState] : state.posts.feeds) {
      for (const auto &post : feedState.posts) {
        if (post.id == postId) {
          isCurrentlyLiked = post.isLiked;
          break;
        }
      }
    }

    // Store previous state for rollback on error
    auto previousState = isCurrentlyLiked;

    // Apply optimistic update
    updateFeedState([postId, isCurrentlyLiked](PostsState &state) {
      for (auto &[feedType, feedState] : state.feeds) {
        for (auto &post : feedState.posts) {
          if (post.id == postId) {
            post.isLiked = !post.isLiked;
            post.likeCount += post.isLiked ? 1 : -1;
          }
        }
      }
    });

    Util::logDebug("AppStore", "Like post optimistic update: " + postId);

    // Send to server - like or unlike based on previous state
    if (isCurrentlyLiked) {
      networkClient->unlikePost(postId, [this, postId, previousState, observer](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "Post unliked successfully: " + postId);
          // Invalidate feed caches on successful unlike
          invalidateCachePattern("feed:*");
          observer.on_next(0);
          observer.on_completed();
        } else {
          Util::logError("AppStore", "Failed to unlike post: " + result.getError());
          // Rollback optimistic update on error
          updateFeedState([postId, previousState](PostsState &state) {
            for (auto &[feedType, feedState] : state.feeds) {
              for (auto &post : feedState.posts) {
                if (post.id == postId) {
                  post.isLiked = previousState;
                  post.likeCount += previousState ? 1 : -1;
                }
              }
            }
          });
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
        }
      });
    } else {
      networkClient->likePost(postId, "", [this, postId, previousState, observer](Outcome<juce::var> result) {
        if (result.isOk()) {
          Util::logInfo("AppStore", "Post liked successfully: " + postId);
          // Invalidate feed caches on successful like
          invalidateCachePattern("feed:*");
          observer.on_next(0);
          observer.on_completed();
        } else {
          Util::logError("AppStore", "Failed to like post: " + result.getError());
          // Rollback optimistic update on error
          updateFeedState([postId, previousState](PostsState &state) {
            for (auto &[feedType, feedState] : state.feeds) {
              for (auto &post : feedState.posts) {
                if (post.id == postId) {
                  post.isLiked = previousState;
                  post.likeCount += previousState ? 1 : -1;
                }
              }
            }
          });
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
        }
      });
    }
  });
}

} // namespace Stores
} // namespace Sidechain
