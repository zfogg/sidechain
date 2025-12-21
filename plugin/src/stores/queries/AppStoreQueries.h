#pragma once

#include "../app/AppState.h"
#include "../../models/FeedPost.h"
#include "../../models/User.h"
#include <memory>
#include <vector>

namespace Sidechain {
namespace Stores {

/**
 * AppStoreQueries - Typed query interface for accessing derived/computed state
 *
 * Components should use queries instead of directly accessing state structure.
 * This decouples UI from internal state organization.
 *
 * Example:
 *   auto queries = AppStore::getInstance().queries();
 *   auto posts = queries.getCurrentFeedPosts();
 *   if (queries.isCurrentFeedLoading()) { showSpinner(); }
 *
 * Queries are computed from state on-demand (no caching).
 * For expensive computations, cache results in state (AppState) instead.
 */
class AppStoreQueries {
public:
  explicit AppStoreQueries(const AppState &state) : state_(state) {}

  // ==============================================================================
  // Feed Queries
  // ==============================================================================

  /**
   * Get posts for the current feed type
   * Returns empty vector if current feed not loaded yet
   */
  std::vector<std::shared_ptr<FeedPost>> getCurrentFeedPosts() const;

  /**
   * Get posts for a specific feed type
   */
  std::vector<std::shared_ptr<FeedPost>> getFeedPosts(FeedType feedType) const;

  /**
   * Check if the current feed is loading
   */
  bool isCurrentFeedLoading() const;

  /**
   * Check if a specific feed is loading
   */
  bool isFeedLoading(FeedType feedType) const;

  /**
   * Get current feed error message (empty if no error)
   */
  juce::String getCurrentFeedError() const;

  /**
   * Check if current feed has more posts to load
   */
  bool hasMoreCurrentFeedPosts() const;

  /**
   * Get total post count for current feed
   */
  int getCurrentFeedTotal() const;

  /**
   * Get current pagination offset
   */
  int getCurrentFeedOffset() const;

  /**
   * Check if current feed has been synced with server at least once
   */
  bool isCurrentFeedSynced() const;

  // ==============================================================================
  // Saved Posts Queries
  // ==============================================================================

  /**
   * Get all saved posts
   */
  std::vector<std::shared_ptr<FeedPost>> getSavedPosts() const;

  /**
   * Check if saved posts are loading
   */
  bool areSavedPostsLoading() const;

  /**
   * Check if there are more saved posts to load
   */
  bool hasMoreSavedPosts() const;

  // ==============================================================================
  // Archived Posts Queries
  // ==============================================================================

  /**
   * Get all archived posts
   */
  std::vector<std::shared_ptr<FeedPost>> getArchivedPosts() const;

  /**
   * Check if archived posts are loading
   */
  bool areArchivedPostsLoading() const;

  /**
   * Check if there are more archived posts to load
   */
  bool hasMoreArchivedPosts() const;

  // ==============================================================================
  // Auth Queries
  // ==============================================================================

  /**
   * Check if user is currently authenticated
   */
  bool isAuthenticated() const;

  /**
   * Get current user ID (empty if not authenticated)
   */
  juce::String getCurrentUserId() const;

  /**
   * Get current user email
   */
  juce::String getCurrentUserEmail() const;

  /**
   * Get current user username
   */
  juce::String getCurrentUsername() const;

  /**
   * Check if authentication is in progress
   */
  bool isAuthenticating() const;

  /**
   * Check if 2FA is required
   */
  bool is2FARequired() const;

  /**
   * Check if 2FA verification is in progress
   */
  bool isVerifying2FA() const;

  /**
   * Get auth error message (empty if no error)
   */
  juce::String getAuthError() const;

  /**
   * Check if authentication token is expired
   */
  bool isAuthTokenExpired() const;

  /**
   * Check if authentication token should be refreshed soon
   */
  bool shouldRefreshAuthToken() const;

  // ==============================================================================
  // User Profile Queries
  // ==============================================================================

  /**
   * Get current user's display name
   */
  juce::String getUserDisplayName() const;

  /**
   * Get current user's bio
   */
  juce::String getUserBio() const;

  /**
   * Get current user's profile picture
   */
  juce::Image getUserProfileImage() const;

  /**
   * Check if user profile image is loading
   */
  bool isUserProfileImageLoading() const;

  /**
   * Get current user's follower count
   */
  int getUserFollowerCount() const;

  /**
   * Get current user's following count
   */
  int getUserFollowingCount() const;

  /**
   * Get current user's post count
   */
  int getUserPostCount() const;

  /**
   * Check if user profile is being fetched
   */
  bool isUserProfileFetching() const;

  /**
   * Get user profile error (empty if no error)
   */
  juce::String getUserProfileError() const;

  // ==============================================================================
  // Chat Queries
  // ==============================================================================

  /**
   * Get all chat channels
   */
  std::vector<juce::String> getChatChannelIds() const;

  /**
   * Get current selected channel ID
   */
  juce::String getCurrentChatChannelId() const;

  /**
   * Check if channels are loading
   */
  bool areChatChannelsLoading() const;

  /**
   * Get total unread message count across all channels
   */
  int getTotalUnreadChatCount() const;

  /**
   * Check if chat is connected
   */
  bool isChatConnected() const;

  /**
   * Get chat error (empty if no error)
   */
  juce::String getChatError() const;

  // ==============================================================================
  // Notification Queries
  // ==============================================================================

  /**
   * Get all notifications
   */
  std::vector<std::shared_ptr<Notification>> getNotifications() const;

  /**
   * Get unread notification count
   */
  int getUnreadNotificationCount() const;

  /**
   * Get unseen notification count
   */
  int getUnseenNotificationCount() const;

  /**
   * Check if notifications are loading
   */
  bool areNotificationsLoading() const;

  /**
   * Get notification error (empty if no error)
   */
  juce::String getNotificationError() const;

  // ==============================================================================
  // Search Queries
  // ==============================================================================

  /**
   * Get search result posts
   */
  std::vector<std::shared_ptr<FeedPost>> getSearchResultPosts() const;

  /**
   * Get search result users
   */
  std::vector<std::shared_ptr<User>> getSearchResultUsers() const;

  /**
   * Check if search is in progress
   */
  bool isSearching() const;

  /**
   * Get current search query
   */
  juce::String getCurrentSearchQuery() const;

  /**
   * Check if search has more results
   */
  bool hasMoreSearchResults() const;

  /**
   * Get search error (empty if no error)
   */
  juce::String getSearchError() const;

  // ==============================================================================
  // Discovery Queries
  // ==============================================================================

  /**
   * Get trending users
   */
  std::vector<std::shared_ptr<const User>> getTrendingUsers() const;

  /**
   * Get featured producers
   */
  std::vector<std::shared_ptr<const User>> getFeaturedProducers() const;

  /**
   * Get suggested users
   */
  std::vector<std::shared_ptr<const User>> getSuggestedUsers() const;

  /**
   * Check if any discovery section is loading
   */
  bool isDiscoveryLoading() const;

  /**
   * Check specifically if trending users are loading
   */
  bool areTrendingUsersLoading() const;

  /**
   * Get discovery error (empty if no error)
   */
  juce::String getDiscoveryError() const;

  // ==============================================================================
  // Presence Queries
  // ==============================================================================

  /**
   * Get current user's presence status
   */
  PresenceStatus getCurrentPresenceStatus() const;

  /**
   * Check if user is online
   */
  bool isCurrentUserOnline() const;

  /**
   * Check if presence is connected
   */
  bool isPresenceConnected() const;

  /**
   * Get presence error (empty if no error)
   */
  juce::String getPresenceError() const;

  // ==============================================================================
  // Comment Queries
  // ==============================================================================

  /**
   * Get comments for a specific post
   */
  std::vector<std::shared_ptr<Comment>> getCommentsForPost(const juce::String &postId) const;

  /**
   * Check if comments are loading for a post
   */
  bool areCommentsLoading(const juce::String &postId) const;

  /**
   * Check if there are more comments to load for a post
   */
  bool hasMoreCommentsForPost(const juce::String &postId) const;

  /**
   * Get comment error (empty if no error)
   */
  juce::String getCommentError() const;

  // ==============================================================================
  // Story Queries
  // ==============================================================================

  /**
   * Get feed stories (from other users)
   */
  std::vector<std::shared_ptr<Story>> getFeedStories() const;

  /**
   * Get my stories
   */
  std::vector<std::shared_ptr<Story>> getMyStories() const;

  /**
   * Check if feed stories are loading
   */
  bool areFeedStoriesLoading() const;

  /**
   * Check if my stories are loading
   */
  bool areMyStoriesLoading() const;

  /**
   * Get story error (empty if no error)
   */
  juce::String getStoryError() const;

  // ==============================================================================
  // Upload Queries
  // ==============================================================================

  /**
   * Check if upload is in progress
   */
  bool isUploading() const;

  /**
   * Get upload progress (0-100)
   */
  int getUploadProgress() const;

  /**
   * Get uploading file name
   */
  juce::String getUploadingFileName() const;

  /**
   * Get upload error (empty if no error)
   */
  juce::String getUploadError() const;

  // ==============================================================================
  // Playlist Queries
  // ==============================================================================

  /**
   * Get all playlists
   */
  std::vector<std::shared_ptr<Playlist>> getPlaylists() const;

  /**
   * Check if playlists are loading
   */
  bool arePlaylistsLoading() const;

  /**
   * Get playlist error (empty if no error)
   */
  juce::String getPlaylistError() const;

  // ==============================================================================
  // Challenge Queries
  // ==============================================================================

  /**
   * Get all MIDI challenges
   */
  std::vector<std::shared_ptr<MIDIChallenge>> getChallenges() const;

  /**
   * Check if challenges are loading
   */
  bool areChallengesLoading() const;

  /**
   * Get challenge error (empty if no error)
   */
  juce::String getChallengeError() const;

  // ==============================================================================
  // Sound Queries
  // ==============================================================================

  /**
   * Get featured sounds
   */
  std::vector<std::shared_ptr<Sound>> getFeaturedSounds() const;

  /**
   * Get recent sounds
   */
  std::vector<std::shared_ptr<Sound>> getRecentSounds() const;

  /**
   * Check if sounds are loading
   */
  bool areSoundsLoading() const;

  /**
   * Check if there are more sounds to load
   */
  bool hasMoreSounds() const;

  /**
   * Get sound error (empty if no error)
   */
  juce::String getSoundError() const;

  // ==============================================================================
  // Draft Queries
  // ==============================================================================

  /**
   * Get all drafts
   */
  std::vector<std::shared_ptr<Draft>> getDrafts() const;

  /**
   * Check if drafts are loading
   */
  bool areDraftsLoading() const;

  /**
   * Get draft error (empty if no error)
   */
  juce::String getDraftError() const;

  // ==============================================================================
  // Followers/Following Queries
  // ==============================================================================

  /**
   * Get followers list
   */
  std::vector<std::shared_ptr<const User>> getFollowers() const;

  /**
   * Get following list
   */
  std::vector<std::shared_ptr<const User>> getFollowing() const;

  /**
   * Check if followers are loading
   */
  bool areFollowersLoading() const;

  /**
   * Get followers/following target user ID
   */
  juce::String getFollowersTargetUserId() const;

  /**
   * Get total follower count
   */
  int getTotalFollowerCount() const;

private:
  const AppState &state_;
};

} // namespace Stores
} // namespace Sidechain
