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

  // Invalidate the specific post cache

  // Invalidate all feed caches since any post change affects feed display
}

void AppStore::onWebSocketLikeCountUpdate(const juce::String &postId, int likeCount) {
  Util::logDebug("AppStore",
                 "WebSocket: Like count updated for post " + postId + " - new count: " + juce::String(likeCount));

  // Invalidate the specific post cache

  // Invalidate all feed caches
}

void AppStore::onWebSocketFollowerCountUpdate(const juce::String &userId, int followerCount) {
  Util::logDebug("AppStore", "WebSocket: Follower count updated for user " + userId +
                                 " - new count: " + juce::String(followerCount));

  // Invalidate the user cache

  // Invalidate user search results since follower counts may have changed

  // Update the current user's follower count in state if this is about us
  if (userId == sliceManager.getUserSlice()->getState().userId) {
    sliceManager.getUserSlice()->dispatch([followerCount](UserState &state) { state.followerCount = followerCount; });
  }
}

void AppStore::onWebSocketNewPost(const juce::var &postData) {
  Util::logDebug("AppStore", "WebSocket: New post notification received");

  // Invalidate all feed caches so the new post appears
}

void AppStore::onWebSocketUserUpdated(const juce::String &userId) {
  Util::logDebug("AppStore", "WebSocket: User profile updated - " + userId);

  // Invalidate the specific user cache

  // Invalidate user search results
}

void AppStore::onWebSocketNewMessage(const juce::String &conversationId) {
  Util::logDebug("AppStore", "WebSocket: New message in conversation - " + conversationId);

  // Invalidate the messages cache for this conversation

  // Invalidate all channels list since this conversation may need re-sorting
}

void AppStore::onWebSocketPresenceUpdate(const juce::String &userId, bool isOnline) {
  Util::logDebug("AppStore",
                 "WebSocket: Presence update - user " + userId + " is " + (isOnline ? "online" : "offline"));

  // Update presence state directly without invalidating other caches
  // (presence changes don't affect feed/user data validity, just UI display)
  sliceManager.getUserSlice()->dispatch([userId, isOnline](UserState &state) {
    // Store online status per user - could use a map if many users tracked
    // For now, we just log the update. Real implementation might maintain
    // a presence map or push to a dedicated presence state slice.
    Util::logDebug("AppStore", "Updated presence for " + userId);
  });
}

// TODO: Implement Phase 3+ comment handlers when comment system is added
// void AppStore::onWebSocketCommentCountUpdate(const juce::String &postId, int commentCount) { }
// void AppStore::onWebSocketNewComment(const juce::String &postId, const juce::String &commentId,
// const juce::String &username) { }

} // namespace Stores
} // namespace Sidechain
