#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

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

  updateFeedState([feedType](PostsState &state) {
    state.currentFeedType = feedType;
    state.feeds[feedType].isLoading = true;
    state.feedError = "";
  });

  Util::logInfo("AppStore", "Loading feed");
  // TODO: Implement actual feed loading from network
}

void AppStore::refreshCurrentFeed() {
  auto currentState = getState();
  loadFeed(currentState.posts.currentFeedType, true);
}

void AppStore::loadMore() {
  auto currentState = getState();
  auto &feedState = currentState.posts.feeds[currentState.posts.currentFeedType];

  if (!feedState.hasMore) {
    Util::logDebug("AppStore", "No more posts to load");
    return;
  }

  updateFeedState([](PostsState &state) { state.feeds[state.currentFeedType].isLoading = true; });

  Util::logInfo("AppStore", "Loading more posts");
  // TODO: Implement load more
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

  updateFeedState([](PostsState &state) {
    state.savedPosts.isLoading = true;
    state.savedPosts.error = "";
  });

  Util::logInfo("AppStore", "Loading saved posts");
  // TODO: Implement load saved posts
}

void AppStore::loadMoreSavedPosts() {
  auto currentState = getState();
  if (!currentState.posts.savedPosts.hasMore) {
    return;
  }

  updateFeedState([](PostsState &state) { state.savedPosts.isLoading = true; });

  Util::logInfo("AppStore", "Loading more saved posts");
  // TODO: Implement load more saved posts
}

void AppStore::unsavePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Unsaving post", "postId=" + postId);

  // Optimistic removal from saved posts
  updateFeedState([postId](PostsState &state) {
    for (int i = 0; i < state.savedPosts.posts.size(); ++i) {
      if (state.savedPosts.posts[i].id == postId) {
        state.savedPosts.posts.remove(i);
        break;
      }
    }
  });

  // TODO: Implement network call to unsave
}

//==============================================================================
// Archived Posts

void AppStore::loadArchivedPosts() {
  if (!networkClient) {
    updateFeedState([](PostsState &state) { state.archivedPosts.error = "Network client not initialized"; });
    return;
  }

  updateFeedState([](PostsState &state) {
    state.archivedPosts.isLoading = true;
    state.archivedPosts.error = "";
  });

  Util::logInfo("AppStore", "Loading archived posts");
  // TODO: Implement load archived posts
}

void AppStore::loadMoreArchivedPosts() {
  auto currentState = getState();
  if (!currentState.posts.archivedPosts.hasMore) {
    return;
  }

  updateFeedState([](PostsState &state) { state.archivedPosts.isLoading = true; });

  Util::logInfo("AppStore", "Loading more archived posts");
  // TODO: Implement load more archived posts
}

void AppStore::restorePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Restoring post", "postId=" + postId);

  // Optimistic removal from archived posts
  updateFeedState([postId](PostsState &state) {
    for (int i = 0; i < state.archivedPosts.posts.size(); ++i) {
      if (state.archivedPosts.posts[i].id == postId) {
        state.archivedPosts.posts.remove(i);
        break;
      }
    }
  });

  // TODO: Implement network call to restore
}

//==============================================================================
// Post Interactions

void AppStore::toggleLike(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Toggling like", "postId=" + postId);
  // TODO: Implement toggle like with optimistic update
}

void AppStore::toggleSave(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Toggling save", "postId=" + postId);
  // TODO: Implement toggle save with optimistic update
}

void AppStore::toggleRepost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Toggling repost", "postId=" + postId);
  // TODO: Implement toggle repost with optimistic update
}

void AppStore::addReaction(const juce::String &postId, const juce::String &emoji) {
  if (!networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Adding reaction", "postId=" + postId + ", emoji=" + emoji);
  // TODO: Implement add reaction with optimistic update
}

void AppStore::toggleFollow(const juce::String &postId, bool willFollow) {
  if (!networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Toggling follow", "postId=" + postId);
  // TODO: Implement toggle follow with optimistic update
}

void AppStore::toggleMute(const juce::String &postId, bool isMuted) {
  if (!networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Toggling mute", "postId=" + postId);
  // TODO: Implement toggle mute
}

} // namespace Stores
} // namespace Sidechain
