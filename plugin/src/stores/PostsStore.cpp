#include "PostsStore.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

PostsStore::PostsStore(NetworkClient *client) : Store(PostsState()), networkClient(client) {
  Util::logInfo("PostsStore", "Initialized");
}

//==============================================================================
// Saved Posts Loading
//==============================================================================

void PostsStore::loadSavedPosts() {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot load saved posts - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Loading saved posts");

  updateState([](PostsState &state) {
    state.savedPosts.isLoading = true;
    state.savedPosts.offset = 0;
    state.savedPosts.posts.clear();
    state.savedPosts.error = "";
  });

  networkClient->getSavedPosts(20, 0, [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void PostsStore::loadMoreSavedPosts() {
  auto state = getState();
  if (!state.savedPosts.hasMore || state.savedPosts.isLoading || networkClient == nullptr) {
    return;
  }

  Util::logDebug("PostsStore", "Loading more saved posts");

  updateState([](PostsState &s) { s.savedPosts.isLoading = true; });

  networkClient->getSavedPosts(state.savedPosts.limit, state.savedPosts.offset,
                               [this](Outcome<juce::var> result) { handleSavedPostsLoaded(result); });
}

void PostsStore::refreshSavedPosts() {
  loadSavedPosts();
}

void PostsStore::handleSavedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateState([error = result.getError()](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateState([](PostsState &s) {
      s.savedPosts.isLoading = false;
      s.savedPosts.error = "Invalid saved posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateState([](PostsState &s) {
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

  updateState([loadedPosts, totalCount](PostsState &s) {
    s.savedPosts.posts = loadedPosts;
    s.savedPosts.isLoading = false;
    s.savedPosts.totalCount = totalCount;
    s.savedPosts.offset += loadedPosts.size();
    s.savedPosts.hasMore = s.savedPosts.offset < totalCount;
    s.savedPosts.error = "";
    s.savedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("PostsStore", "Loaded " + juce::String(loadedPosts.size()) + " saved posts");
}

//==============================================================================
// Archived Posts Loading
//==============================================================================

void PostsStore::loadArchivedPosts() {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot load archived posts - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Loading archived posts");

  updateState([](PostsState &state) {
    state.archivedPosts.isLoading = true;
    state.archivedPosts.offset = 0;
    state.archivedPosts.posts.clear();
    state.archivedPosts.error = "";
  });

  networkClient->getArchivedPosts(20, 0, [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void PostsStore::loadMoreArchivedPosts() {
  auto state = getState();
  if (!state.archivedPosts.hasMore || state.archivedPosts.isLoading || networkClient == nullptr) {
    return;
  }

  Util::logDebug("PostsStore", "Loading more archived posts");

  updateState([](PostsState &s) { s.archivedPosts.isLoading = true; });

  networkClient->getArchivedPosts(state.archivedPosts.limit, state.archivedPosts.offset,
                                  [this](Outcome<juce::var> result) { handleArchivedPostsLoaded(result); });
}

void PostsStore::refreshArchivedPosts() {
  loadArchivedPosts();
}

void PostsStore::handleArchivedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    updateState([error = result.getError()](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = error;
    });
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    updateState([](PostsState &s) {
      s.archivedPosts.isLoading = false;
      s.archivedPosts.error = "Invalid archived posts response";
    });
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    updateState([](PostsState &s) {
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

  updateState([loadedPosts, totalCount](PostsState &s) {
    s.archivedPosts.posts = loadedPosts;
    s.archivedPosts.isLoading = false;
    s.archivedPosts.totalCount = totalCount;
    s.archivedPosts.offset += loadedPosts.size();
    s.archivedPosts.hasMore = s.archivedPosts.offset < totalCount;
    s.archivedPosts.error = "";
    s.archivedPosts.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("PostsStore", "Loaded " + juce::String(loadedPosts.size()) + " archived posts");
}

//==============================================================================
// Saved Posts Operations
//==============================================================================

void PostsStore::unsavePost(const juce::String &postId) {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot unsave post - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Unsaving post: " + postId);

  // Optimistic removal
  removePostFromSaved(postId);

  // Send to server
  networkClient->unsavePost(postId, [this, postId](Outcome<juce::var> result) { handlePostUnsaved(postId, result); });
}

std::optional<FeedPost> PostsStore::getSavedPostById(const juce::String &postId) const {
  auto state = getState();
  for (const auto &post : state.savedPosts.posts) {
    if (post.id == postId) {
      return post;
    }
  }
  return std::nullopt;
}

void PostsStore::removePostFromSaved(const juce::String &postId) {
  auto state = getState();
  for (int i = 0; i < state.savedPosts.posts.size(); ++i) {
    if (state.savedPosts.posts[i].id == postId) {
      state.savedPosts.posts.remove(i);
      setState(state);
      return;
    }
  }
}

void PostsStore::updatePostInSaved(const FeedPost &updatedPost) {
  auto state = getState();
  for (int i = 0; i < state.savedPosts.posts.size(); ++i) {
    if (state.savedPosts.posts[i].id == updatedPost.id) {
      state.savedPosts.posts.set(i, updatedPost);
      setState(state);
      return;
    }
  }
}

void PostsStore::handlePostUnsaved([[maybe_unused]] const juce::String &postId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Refresh on error to restore the post
    Util::logError("PostsStore", "Failed to unsave post: " + result.getError());
    refreshSavedPosts();
  } else {
    Util::logDebug("PostsStore", "Post unsaved successfully");
  }
}

//==============================================================================
// Archived Posts Operations
//==============================================================================

void PostsStore::restorePost(const juce::String &postId) {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot restore post - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Restoring post: " + postId);

  // Optimistic removal (since restored posts go back to active)
  removePostFromArchived(postId);

  // Send to server
  networkClient->unarchivePost(postId,
                               [this, postId](Outcome<juce::var> result) { handlePostRestored(postId, result); });
}

std::optional<FeedPost> PostsStore::getArchivedPostById(const juce::String &postId) const {
  auto state = getState();
  for (const auto &post : state.archivedPosts.posts) {
    if (post.id == postId) {
      return post;
    }
  }
  return std::nullopt;
}

void PostsStore::removePostFromArchived(const juce::String &postId) {
  auto state = getState();
  for (int i = 0; i < state.archivedPosts.posts.size(); ++i) {
    if (state.archivedPosts.posts[i].id == postId) {
      state.archivedPosts.posts.remove(i);
      setState(state);
      return;
    }
  }
}

void PostsStore::updatePostInArchived(const FeedPost &updatedPost) {
  auto state = getState();
  for (int i = 0; i < state.archivedPosts.posts.size(); ++i) {
    if (state.archivedPosts.posts[i].id == updatedPost.id) {
      state.archivedPosts.posts.set(i, updatedPost);
      setState(state);
      return;
    }
  }
}

void PostsStore::handlePostRestored([[maybe_unused]] const juce::String &postId, Outcome<juce::var> result) {
  if (!result.isOk()) {
    // Refresh on error to restore the post to the list
    Util::logError("PostsStore", "Failed to restore post: " + result.getError());
    refreshArchivedPosts();
  } else {
    Util::logDebug("PostsStore", "Post restored successfully");
  }
}

//==============================================================================
// Feed Loading
//==============================================================================

void PostsStore::loadFeed(FeedType feedType, bool forceRefresh) {
  if (networkClient == nullptr) {
    Util::logWarning("PostsStore", "Cannot load feed - networkClient null");
    return;
  }

  Util::logInfo("PostsStore", "Loading feed: " + feedTypeToString(feedType));

  // Check cache first
  if (!forceRefresh && isCurrentFeedCached()) {
    auto state = getState();
    if (state.feeds.count(feedType) > 0 && !state.feeds.at(feedType).posts.isEmpty()) {
      currentFeedIsFromCache_ = true;
      return;
    }
  }

  // Update state to loading
  updateState([feedType](PostsState &state) {
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

void PostsStore::refreshCurrentFeed() {
  auto feedType = getState().currentFeedType;
  loadFeed(feedType, true);
}

void PostsStore::loadMore() {
  auto state = getState();
  auto feedType = state.currentFeedType;

  if (state.feeds.count(feedType) == 0) {
    Util::logWarning("PostsStore", "Cannot load more - feed not initialized");
    return;
  }

  auto &feedState = state.feeds.at(feedType);
  if (!feedState.hasMore || feedState.isLoading || networkClient == nullptr) {
    return;
  }

  updateState([feedType](PostsState &s) {
    if (s.feeds.count(feedType) > 0) {
      s.feeds[feedType].isLoading = true;
    }
  });

  performFetch(feedType, feedState.limit, feedState.offset);
}

void PostsStore::switchFeedType(FeedType feedType) {
  Util::logInfo("PostsStore", "Switching to feed: " + feedTypeToString(feedType));

  updateState([feedType](PostsState &state) { state.currentFeedType = feedType; });

  // Load the feed if not already loaded
  auto state = getState();
  if (state.feeds.count(feedType) == 0 || state.feeds.at(feedType).posts.isEmpty()) {
    loadFeed(feedType);
  }
}

void PostsStore::performFetch(FeedType feedType, int limit, int offset) {
  if (networkClient == nullptr) {
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
    Util::logWarning("PostsStore", "UserActivityAggregated requires userId - skipping");
  }
}

void PostsStore::handleFetchSuccess(FeedType feedType, const juce::var &data, [[maybe_unused]] int limit, int offset) {
  if (isAggregatedFeedType(feedType)) {
    auto response = parseAggregatedJsonResponse(data);

    updateState([feedType, response, offset](PostsState &s) {
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
  } else {
    auto response = parseJsonResponse(data);

    updateState([feedType, response, offset](PostsState &s) {
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
  }

  Util::logDebug("PostsStore", "Loaded " + feedTypeToString(feedType) + " feed");
}

void PostsStore::handleFetchError(FeedType feedType, const juce::String &error) {
  Util::logError("PostsStore", "Failed to load " + feedTypeToString(feedType) + ": " + error);

  updateState([feedType, error](PostsState &s) {
    if (isAggregatedFeedType(feedType)) {
      if (s.aggregatedFeeds.count(feedType) > 0) {
        s.aggregatedFeeds[feedType].isLoading = false;
        s.aggregatedFeeds[feedType].isRefreshing = false;
        s.aggregatedFeeds[feedType].error = error;
      }
    } else {
      if (s.feeds.count(feedType) > 0) {
        s.feeds[feedType].isLoading = false;
        s.feeds[feedType].isRefreshing = false;
        s.feeds[feedType].error = error;
      }
    }
  });
}

FeedResponse PostsStore::parseJsonResponse(const juce::var &json) {
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

AggregatedFeedResponse PostsStore::parseAggregatedJsonResponse(const juce::var &json) {
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
// Post Interactions (Optimistic Updates)
//==============================================================================

void PostsStore::toggleLike(const juce::String &postId) {
  if (networkClient == nullptr) {
    return;
  }

  // Check current like state to determine whether to like or unlike
  auto state = getState();
  bool isCurrentlyLiked = false;

  for (const auto &[feedType, feedState] : state.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        isCurrentlyLiked = post.isLiked;
        break;
      }
    }
  }

  // Optimistic update
  updateState([postId, isCurrentlyLiked](PostsState &state) {
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
        Util::logError("PostsStore", "Failed to unlike post: " + result.getError());
      }
    });
  } else {
    networkClient->likePost(postId, "", [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to like post: " + result.getError());
      }
    });
  }
}

void PostsStore::toggleSave(const juce::String &postId) {
  if (networkClient == nullptr) {
    return;
  }

  // Check current save state
  auto state = getState();
  bool isCurrentlySaved = false;

  for (const auto &[feedType, feedState] : state.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        isCurrentlySaved = post.isSaved;
        break;
      }
    }
  }

  // Optimistic update
  updateState([postId, isCurrentlySaved](PostsState &state) {
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
        Util::logError("PostsStore", "Failed to unsave post: " + result.getError());
      }
    });
  } else {
    networkClient->savePost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to save post: " + result.getError());
      }
    });
  }
}

void PostsStore::toggleRepost(const juce::String &postId) {
  if (networkClient == nullptr) {
    return;
  }

  // Check current repost state
  auto state = getState();
  bool isCurrentlyReposted = false;

  for (const auto &[feedType, feedState] : state.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        isCurrentlyReposted = post.isReposted;
        break;
      }
    }
  }

  // Optimistic update
  updateState([postId, isCurrentlyReposted](PostsState &state) {
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
        Util::logError("PostsStore", "Failed to undo repost: " + result.getError());
      }
    });
  } else {
    networkClient->repostPost(postId, "", [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to repost: " + result.getError());
      }
    });
  }
}

void PostsStore::addReaction(const juce::String &postId, const juce::String &emoji) {
  if (networkClient == nullptr) {
    return;
  }

  // Add a reaction by liking with an emoji
  networkClient->likePost(postId, emoji, [this](Outcome<juce::var> result) {
    if (!result.isOk()) {
      Util::logError("PostsStore", "Failed to add reaction: " + result.getError());
    }
  });
}

void PostsStore::toggleFollow(const juce::String &postId, bool willFollow) {
  if (networkClient == nullptr) {
    return;
  }

  // Extract user ID from post to follow the author
  auto state = getState();
  juce::String userId;

  for (const auto &[feedType, feedState] : state.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        userId = post.userId;
        break;
      }
    }
  }

  if (!userId.isEmpty()) {
    updateFollowStateByUserId(userId, willFollow);
  }
}

void PostsStore::updateFollowStateByUserId(const juce::String &userId, bool willFollow) {
  if (networkClient == nullptr) {
    return;
  }

  if (willFollow) {
    networkClient->followUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to follow user: " + result.getError());
      }
    });
  } else {
    networkClient->unfollowUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to unfollow user: " + result.getError());
      }
    });
  }
}

void PostsStore::toggleMute(const juce::String &userId, bool willMute) {
  if (networkClient == nullptr) {
    return;
  }

  if (willMute) {
    networkClient->muteUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to mute user: " + result.getError());
      }
    });
  } else {
    networkClient->unmuteUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to unmute user: " + result.getError());
      }
    });
  }
}

void PostsStore::toggleBlock(const juce::String &userId, bool willBlock) {
  if (networkClient == nullptr) {
    return;
  }

  if (willBlock) {
    networkClient->blockUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to block user: " + result.getError());
      }
    });
  } else {
    networkClient->unblockUser(userId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to unblock user: " + result.getError());
      }
    });
  }
}

void PostsStore::toggleArchive(const juce::String &postId, bool archived) {
  if (networkClient == nullptr) {
    return;
  }

  // Archived posts don't have a specific state on FeedPost - they're managed in separate collections
  // Just send the request to the server and let the server response update the state
  if (archived) {
    networkClient->archivePost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to archive post: " + result.getError());
      } else {
        // Refresh archived posts list after archiving
        loadArchivedPosts();
      }
    });
  } else {
    networkClient->unarchivePost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to unarchive post: " + result.getError());
      } else {
        // Refresh feed after unarchiving
        refreshCurrentFeed();
      }
    });
  }
}

void PostsStore::togglePin(const juce::String &postId, bool pinned) {
  if (networkClient == nullptr) {
    return;
  }

  // Check current pin state
  auto state = getState();
  bool isCurrentlyPinned = false;

  for (const auto &[feedType, feedState] : state.feeds) {
    for (const auto &post : feedState.posts) {
      if (post.id == postId) {
        isCurrentlyPinned = post.isPinned;
        break;
      }
    }
  }

  // Optimistic update
  updateState([postId, pinned](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post.id == postId) {
          post.isPinned = pinned;
        }
      }
    }

    for (auto &post : state.savedPosts.posts) {
      if (post.id == postId) {
        post.isPinned = pinned;
      }
    }

    for (auto &post : state.archivedPosts.posts) {
      if (post.id == postId) {
        post.isPinned = pinned;
      }
    }
  });

  // Send to server
  if (pinned) {
    networkClient->pinPost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to pin post: " + result.getError());
      }
    });
  } else {
    networkClient->unpinPost(postId, [this](Outcome<juce::var> result) {
      if (!result.isOk()) {
        Util::logError("PostsStore", "Failed to unpin post: " + result.getError());
      }
    });
  }
}

void PostsStore::updatePlayCount(const juce::String &postId, int newCount) {
  updateState([postId, newCount](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post.id == postId) {
          post.playCount = newCount;
        }
      }
    }

    for (auto &post : state.savedPosts.posts) {
      if (post.id == postId) {
        post.playCount = newCount;
      }
    }

    for (auto &post : state.archivedPosts.posts) {
      if (post.id == postId) {
        post.playCount = newCount;
      }
    }
  });
}

//==============================================================================
// Real-Time Updates
//==============================================================================

void PostsStore::handleNewPostNotification(const juce::var &postData) {
  auto post = FeedPost::fromJson(postData);
  if (!post.isValid()) {
    return;
  }

  auto feedType = getState().currentFeedType;

  updateState([feedType, post](PostsState &state) {
    if (!isAggregatedFeedType(feedType) && state.feeds.count(feedType) > 0) {
      state.feeds[feedType].posts.insert(0, post);
      state.feeds[feedType].total++;
    }
  });

  Util::logInfo("PostsStore", "New post notification received: " + post.id);
}

void PostsStore::handleLikeCountUpdate(const juce::String &postId, int likeCount) {
  updateState([postId, likeCount](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      for (auto &post : feedState.posts) {
        if (post.id == postId) {
          post.likeCount = likeCount;
        }
      }
    }

    for (auto &post : state.savedPosts.posts) {
      if (post.id == postId) {
        post.likeCount = likeCount;
      }
    }

    for (auto &post : state.archivedPosts.posts) {
      if (post.id == postId) {
        post.likeCount = likeCount;
      }
    }
  });
}

void PostsStore::updateUserPresence(const juce::String &userId, bool isOnline, const juce::String &status) {
  // This would be used to update user presence indicators in the UI
  // Stored in a separate presence state if needed
  Util::logDebug("PostsStore", "User presence updated: " + userId + " - " + (isOnline ? "online" : "offline"));
}

//==============================================================================
// Cache Management
//==============================================================================

void PostsStore::clearCache() {
  updateState([](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      feedState.posts.clear();
      feedState.offset = 0;
      feedState.total = 0;
      feedState.hasMore = true;
      feedState.lastUpdated = 0;
    }

    for (auto &[feedType, aggState] : state.aggregatedFeeds) {
      aggState.groups.clear();
      aggState.offset = 0;
      aggState.total = 0;
      aggState.hasMore = true;
      aggState.lastUpdated = 0;
    }
  });

  Util::logInfo("PostsStore", "All caches cleared");
}

void PostsStore::clearCache(FeedType feedType) {
  updateState([feedType](PostsState &state) {
    if (isAggregatedFeedType(feedType)) {
      if (state.aggregatedFeeds.count(feedType) > 0) {
        state.aggregatedFeeds[feedType].groups.clear();
        state.aggregatedFeeds[feedType].offset = 0;
        state.aggregatedFeeds[feedType].total = 0;
        state.aggregatedFeeds[feedType].hasMore = true;
        state.aggregatedFeeds[feedType].lastUpdated = 0;
      }
    } else {
      if (state.feeds.count(feedType) > 0) {
        state.feeds[feedType].posts.clear();
        state.feeds[feedType].offset = 0;
        state.feeds[feedType].total = 0;
        state.feeds[feedType].hasMore = true;
        state.feeds[feedType].lastUpdated = 0;
      }
    }
  });

  Util::logInfo("PostsStore", "Cache cleared for feed: " + feedTypeToString(feedType));
}

//==============================================================================
// Cache Warming & Offline Support
//==============================================================================

void PostsStore::startCacheWarming() {
  if (cacheWarmer != nullptr) {
    cacheWarmer->start();
    Util::logInfo("PostsStore", "Cache warming started");
  }
}

void PostsStore::stopCacheWarming() {
  if (cacheWarmer != nullptr) {
    cacheWarmer->stop();
    Util::logInfo("PostsStore", "Cache warming stopped");
  }
}

void PostsStore::setOnlineStatus(bool isOnline) {
  isOnlineStatus_ = isOnline;
  Util::logInfo("PostsStore", "Online status: " + juce::String(isOnline ? "online" : "offline"));
}

bool PostsStore::isOnline() const {
  return isOnlineStatus_;
}

bool PostsStore::isCurrentFeedCached() const {
  auto state = getState();
  auto feedType = state.currentFeedType;
  auto now = juce::Time::getCurrentTime().toMilliseconds();

  if (isAggregatedFeedType(feedType)) {
    if (state.aggregatedFeeds.count(feedType) > 0) {
      auto &aggState = state.aggregatedFeeds.at(feedType);
      auto ageMs = now - aggState.lastUpdated;
      auto ageSecs = ageMs / 1000;
      return !aggState.groups.isEmpty() && ageSecs < cacheTTLSeconds;
    }
  } else {
    if (state.feeds.count(feedType) > 0) {
      auto &feedState = state.feeds.at(feedType);
      auto ageMs = now - feedState.lastUpdated;
      auto ageSecs = ageMs / 1000;
      return !feedState.posts.isEmpty() && ageSecs < cacheTTLSeconds;
    }
  }

  return false;
}

//==============================================================================
// Real-Time Synchronization
//==============================================================================

void PostsStore::enableRealtimeSync() {
  if (realtimeSync != nullptr) {
    // TODO: Call the actual start method once RealtimeSync API is finalized
    Util::logInfo("PostsStore", "Real-time sync enabled");
  }
}

void PostsStore::disableRealtimeSync() {
  if (realtimeSync != nullptr) {
    // TODO: Call the actual stop method once RealtimeSync API is finalized
    Util::logInfo("PostsStore", "Real-time sync disabled");
  }
}

bool PostsStore::isRealtimeSyncEnabled() const {
  // TODO: Query the actual running state once RealtimeSync API is finalized
  return realtimeSync != nullptr;
}

bool PostsStore::isCurrentFeedSynced() const {
  auto state = getState();
  auto feedType = state.currentFeedType;

  if (isAggregatedFeedType(feedType)) {
    if (state.aggregatedFeeds.count(feedType) > 0) {
      return state.aggregatedFeeds.at(feedType).isSynced;
    }
  } else {
    if (state.feeds.count(feedType) > 0) {
      return state.feeds.at(feedType).isSynced;
    }
  }

  return false;
}

//==============================================================================
// Helpers
//==============================================================================

const FeedPost *PostsStore::findPost(const juce::String &postId) const {
  auto state = getState();

  // Search in current feed
  auto feedType = state.currentFeedType;
  if (state.feeds.count(feedType) > 0) {
    for (const auto &post : state.feeds.at(feedType).posts) {
      if (post.id == postId) {
        return &post;
      }
    }
  }

  // Search in saved posts
  for (const auto &post : state.savedPosts.posts) {
    if (post.id == postId) {
      return &post;
    }
  }

  // Search in archived posts
  for (const auto &post : state.archivedPosts.posts) {
    if (post.id == postId) {
      return &post;
    }
  }

  return nullptr;
}

std::optional<std::pair<FeedType, int>> PostsStore::findPostLocation(const juce::String &postId) const {
  auto state = getState();

  for (const auto &[feedType, feedState] : state.feeds) {
    for (int i = 0; i < feedState.posts.size(); ++i) {
      if (feedState.posts[i].id == postId) {
        return std::make_pair(feedType, i);
      }
    }
  }

  return std::nullopt;
}

//==============================================================================
// Private Helpers
//==============================================================================

void PostsStore::timerCallback() {
  // Timer callback for cache expiration checks
  // Could be extended to periodically clean up expired cache entries
}

juce::String PostsStore::feedTypeToCacheKey(FeedType feedType) const {
  return "feed_" + feedTypeToString(feedType);
}

void PostsStore::schedulePopularFeedWarmup() {
  // Schedule periodic warmup of popular feeds
  // Would be called by cache warmer
}

void PostsStore::warmTimeline() {
  loadFeed(FeedType::Timeline);
}

void PostsStore::warmTrending() {
  loadFeed(FeedType::Trending);
}

void PostsStore::warmUserPosts() {
  // Load user's own posts
}

juce::File PostsStore::getCacheFile(FeedType feedType) const {
  auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("Sidechain")
                 .getChildFile("Cache");
  return dir.getChildFile("feed_" + feedTypeToString(feedType) + ".json");
}

void PostsStore::loadCacheFromDisk(FeedType feedType) {
  auto file = getCacheFile(feedType);
  if (!file.exists()) {
    return;
  }

  auto json = juce::JSON::parse(file.loadFileAsString());
  if (json.isObject()) {
    handleFetchSuccess(feedType, json, 20, 0);
  }
}

void PostsStore::saveCacheToDisk(FeedType feedType, const CacheEntry &entry) {
  auto file = getCacheFile(feedType);
  file.getParentDirectory().createDirectory();
  // Would serialize FeedResponse to JSON and save
}

void PostsStore::updatePostInAllFeeds(const juce::String &postId, std::function<void(FeedPost &)> updater) {
  // Helper function that updates a post in all feeds
  // Must be called from within an updateState lambda
  auto state = getState();

  for (auto &[feedType, feedState] : state.feeds) {
    for (auto &post : feedState.posts) {
      if (post.id == postId) {
        updater(post);
      }
    }
  }

  for (auto &post : state.savedPosts.posts) {
    if (post.id == postId) {
      updater(post);
    }
  }

  for (auto &post : state.archivedPosts.posts) {
    if (post.id == postId) {
      updater(post);
    }
  }

  setState(state);
}

PostsStore::~PostsStore() {
  stopTimer();
  stopCacheWarming();
  disableRealtimeSync();
}

} // namespace Stores
} // namespace Sidechain
