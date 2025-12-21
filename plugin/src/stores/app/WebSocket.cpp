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
  PostsState newState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : newState.feeds) {
    feedState.isSynced = false; // Mark as out of sync, needs refresh
  }
  sliceManager.posts->setState(newState);
}

void AppStore::onWebSocketLikeCountUpdate(const juce::String &postId, int likeCount) {
  Util::logDebug("AppStore",
                 "WebSocket: Like count updated for post " + postId + " - new count: " + juce::String(likeCount));

  // Invalidate all feed caches since like counts affect post display/sorting
  PostsState newState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : newState.feeds) {
    feedState.isSynced = false; // Mark as out of sync, needs refresh
  }
  sliceManager.posts->setState(newState);
}

void AppStore::onWebSocketFollowerCountUpdate(const juce::String &userId, int followerCount) {
  Util::logDebug("AppStore", "WebSocket: Follower count updated for user " + userId +
                                 " - new count: " + juce::String(followerCount));

  // Update the current user's follower count in state if this is about us
  if (userId == sliceManager.user->getState().userId) {
    UserState newState = sliceManager.user->getState();
    newState.followerCount = followerCount;
    sliceManager.user->setState(newState);
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
  PostsState newState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : newState.feeds) {
    feedState.isSynced = false; // Mark as out of sync, needs refresh
  }
  sliceManager.posts->setState(newState);
}

void AppStore::onWebSocketUserUpdated(const juce::String &userId) {
  Util::logDebug("AppStore", "WebSocket: User profile updated - " + userId);

  // Invalidate search results cache since user data may have changed
  SearchState newState = sliceManager.search->getState();
  newState.results.lastSearchTime = 0; // Force refresh on next search
  sliceManager.search->setState(newState);
}

void AppStore::onWebSocketNewMessage(const juce::String &conversationId) {
  Util::logDebug("AppStore", "WebSocket: New message in conversation - " + conversationId);

  // Signal that channels need to be refreshed (new messages may affect unread counts and ordering)
  ChatState newState = sliceManager.chat->getState();
  newState.isLoadingChannels = true; // Trigger a refresh to update the channels list
  sliceManager.chat->setState(newState);
}

void AppStore::onWebSocketPresenceUpdate(const juce::String &userId, bool isOnline) {
  Util::logDebug("AppStore",
                 "WebSocket: Presence update - user " + userId + " is " + (isOnline ? "online" : "offline"));

  // Update dedicated presence state slice for efficient tracking of multiple users
  PresenceState newState = sliceManager.presence->getState();

  // Create or update presence info for the user
  PresenceInfo userPresence;
  userPresence.userId = userId;
  userPresence.status = isOnline ? PresenceStatus::Online : PresenceStatus::Offline;
  userPresence.lastSeen = juce::Time::getCurrentTime().toMilliseconds();

  newState.userPresence[userId] = userPresence;
  sliceManager.presence->setState(newState);

  Util::logDebug("AppStore", "Updated presence for user " + userId + " - status: " + (isOnline ? "Online" : "Offline"));
}

void AppStore::onWebSocketCommentCountUpdate(const juce::String &postId, int commentCount) {
  Util::logDebug("AppStore",
                 "WebSocket: Comment count updated for post " + postId + " - new count: " + juce::String(commentCount));

  // Invalidate comment cache for this post so UI refreshes with new count
  CommentsState newState = sliceManager.comments->getState();
  if (newState.totalCountByPostId.find(postId) != newState.totalCountByPostId.end()) {
    newState.lastUpdatedByPostId[postId] = 0; // Force refresh
  }
  sliceManager.comments->setState(newState);

  // Also invalidate feed caches since comment counts affect post display
  PostsState postState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : postState.feeds) {
    feedState.isSynced = false; // Mark as out of sync
  }
  sliceManager.posts->setState(postState);
}

void AppStore::onWebSocketNewComment(const juce::String &postId, const juce::String &commentId,
                                     const juce::String &username) {
  Util::logDebug("AppStore", "WebSocket: New comment " + commentId + " on post " + postId + " from " + username);

  // Invalidate comments for this post so UI refreshes with new comment
  CommentsState newState = sliceManager.comments->getState();
  newState.lastUpdatedByPostId[postId] = 0; // Force refresh on next load
  sliceManager.comments->setState(newState);

  // Also invalidate feed caches since new comments may affect post display
  PostsState postState = sliceManager.posts->getState();
  for (auto &[feedType, feedState] : postState.feeds) {
    feedState.isSynced = false; // Mark as out of sync
  }
  sliceManager.posts->setState(postState);
}

} // namespace Stores
} // namespace Sidechain
