#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

// ==============================================================================
// WebSocket Event Handlers for Real-Time Cache Invalidation

// When WebSocket events arrive from the backend, these methods ensure
// the in-memory cache stays fresh by invalidating affected entries.

void AppStore::onWebSocketPostUpdated(const juce::String &postId) {
  Util::logDebug("AppStore", "WebSocket: Post updated - " + postId);

  // Invalidate all feed caches since any post change affects feed display
  sliceManager.getPostsSlice()->dispatch([](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      feedState.isSynced = false; // Mark as out of sync, needs refresh
    }
  });
}

void AppStore::onWebSocketLikeCountUpdate(const juce::String &postId, int likeCount) {
  Util::logDebug("AppStore",
                 "WebSocket: Like count updated for post " + postId + " - new count: " + juce::String(likeCount));

  // Invalidate all feed caches since like counts affect post display/sorting
  sliceManager.getPostsSlice()->dispatch([](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      feedState.isSynced = false; // Mark as out of sync, needs refresh
    }
  });
}

void AppStore::onWebSocketFollowerCountUpdate(const juce::String &userId, int followerCount) {
  Util::logDebug("AppStore", "WebSocket: Follower count updated for user " + userId +
                                 " - new count: " + juce::String(followerCount));

  // Update the current user's follower count in state if this is about us
  if (userId == sliceManager.getUserSlice()->getState().userId) {
    sliceManager.getUserSlice()->dispatch([followerCount](UserState &state) { state.followerCount = followerCount; });
  }
}

void AppStore::onWebSocketNewPost(const juce::var &postData) {
  juce::String postId;
  if (postData.isObject()) {
    const auto *obj = postData.getDynamicObject();
    if (obj) {
      postId = obj->getProperty("id").toString();
      Util::logDebug("AppStore", "WebSocket: New post notification received - post ID: " + postId);
    }
  }

  // Invalidate all feed caches so the new post appears in feeds on next load
  sliceManager.getPostsSlice()->dispatch([](PostsState &state) {
    for (auto &[feedType, feedState] : state.feeds) {
      feedState.isSynced = false; // Mark as out of sync, needs refresh
    }
  });
}

void AppStore::onWebSocketUserUpdated(const juce::String &userId) {
  Util::logDebug("AppStore", "WebSocket: User profile updated - " + userId);

  // Invalidate search results cache since user data may have changed
  sliceManager.getSearchSlice()->dispatch([](SearchState &state) {
    state.results.lastSearchTime = 0; // Force refresh on next search
  });
}

void AppStore::onWebSocketNewMessage(const juce::String &conversationId) {
  Util::logDebug("AppStore", "WebSocket: New message in conversation - " + conversationId);

  // Signal that channels need to be refreshed (new messages may affect unread counts and ordering)
  sliceManager.getChatSlice()->dispatch([conversationId](ChatState &state) {
    state.isLoadingChannels = true; // Trigger a refresh to update the channels list
  });
}

void AppStore::onWebSocketPresenceUpdate(const juce::String &userId, bool isOnline) {
  Util::logDebug("AppStore",
                 "WebSocket: Presence update - user " + userId + " is " + (isOnline ? "online" : "offline"));

  // Update presence state directly without invalidating other caches
  // (presence changes don't affect feed/user data validity, just UI display)
  sliceManager.getUserSlice()->dispatch([userId](UserState &state [[maybe_unused]]) {
    // Store online status per user - could use a map if many users tracked
    // For now, we just log the update. Real implementation might maintain
    // presence map or push to a dedicated presence state slice. TODO revisit this.
    Util::logDebug("AppStore", "Updated presence for user " + userId + " - online status changed");
  });
}

// TODO: Implement comment handlers when comment system is added
// void AppStore::onWebSocketCommentCountUpdate(const juce::String &postId, int commentCount) { }
// void AppStore::onWebSocketNewComment(const juce::String &postId, const juce::String &commentId,
// const juce::String &username) { }

} // namespace Stores
} // namespace Sidechain
