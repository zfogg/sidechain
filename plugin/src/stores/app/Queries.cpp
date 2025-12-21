#include "../queries/AppStoreQueries.h"

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Feed Queries
// ==============================================================================

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getCurrentFeedPosts() const {
  auto currentFeed = state_.posts.getCurrentFeed();
  if (currentFeed) {
    return currentFeed->posts;
  }
  return {};
}

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getFeedPosts(FeedType feedType) const {
  auto it = state_.posts.feeds.find(feedType);
  if (it != state_.posts.feeds.end()) {
    return it->second.posts;
  }
  return {};
}

bool AppStoreQueries::isCurrentFeedLoading() const {
  auto currentFeed = state_.posts.getCurrentFeed();
  return currentFeed ? currentFeed->isLoading : false;
}

bool AppStoreQueries::isFeedLoading(FeedType feedType) const {
  auto it = state_.posts.feeds.find(feedType);
  return it != state_.posts.feeds.end() ? it->second.isLoading : false;
}

juce::String AppStoreQueries::getCurrentFeedError() const {
  auto currentFeed = state_.posts.getCurrentFeed();
  return currentFeed ? currentFeed->error : "";
}

bool AppStoreQueries::hasMoreCurrentFeedPosts() const {
  auto currentFeed = state_.posts.getCurrentFeed();
  return currentFeed ? currentFeed->hasMore : false;
}

int AppStoreQueries::getCurrentFeedTotal() const {
  auto currentFeed = state_.posts.getCurrentFeed();
  return currentFeed ? currentFeed->total : 0;
}

int AppStoreQueries::getCurrentFeedOffset() const {
  auto currentFeed = state_.posts.getCurrentFeed();
  return currentFeed ? currentFeed->offset : 0;
}

bool AppStoreQueries::isCurrentFeedSynced() const {
  auto currentFeed = state_.posts.getCurrentFeed();
  return currentFeed ? currentFeed->isSynced : false;
}

// ==============================================================================
// Saved Posts Queries
// ==============================================================================

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getSavedPosts() const {
  return state_.posts.savedPosts.posts;
}

bool AppStoreQueries::areSavedPostsLoading() const {
  return state_.posts.savedPosts.isLoading;
}

bool AppStoreQueries::hasMoreSavedPosts() const {
  return state_.posts.savedPosts.hasMore;
}

// ==============================================================================
// Archived Posts Queries
// ==============================================================================

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getArchivedPosts() const {
  return state_.posts.archivedPosts.posts;
}

bool AppStoreQueries::areArchivedPostsLoading() const {
  return state_.posts.archivedPosts.isLoading;
}

bool AppStoreQueries::hasMoreArchivedPosts() const {
  return state_.posts.archivedPosts.hasMore;
}

// ==============================================================================
// Auth Queries
// ==============================================================================

bool AppStoreQueries::isAuthenticated() const {
  return state_.auth.isLoggedIn;
}

juce::String AppStoreQueries::getCurrentUserId() const {
  return state_.auth.userId;
}

juce::String AppStoreQueries::getCurrentUserEmail() const {
  return state_.auth.email;
}

juce::String AppStoreQueries::getCurrentUsername() const {
  return state_.auth.username;
}

bool AppStoreQueries::isAuthenticating() const {
  return state_.auth.isAuthenticating;
}

bool AppStoreQueries::is2FARequired() const {
  return state_.auth.is2FARequired;
}

bool AppStoreQueries::isVerifying2FA() const {
  return state_.auth.isVerifying2FA;
}

juce::String AppStoreQueries::getAuthError() const {
  return state_.auth.authError;
}

bool AppStoreQueries::isAuthTokenExpired() const {
  return state_.auth.isTokenExpired();
}

bool AppStoreQueries::shouldRefreshAuthToken() const {
  return state_.auth.shouldRefreshToken();
}

// ==============================================================================
// User Profile Queries
// ==============================================================================

juce::String AppStoreQueries::getUserDisplayName() const {
  return state_.user.displayName;
}

juce::String AppStoreQueries::getUserBio() const {
  return state_.user.bio;
}

juce::Image AppStoreQueries::getUserProfileImage() const {
  return state_.user.profileImage;
}

bool AppStoreQueries::isUserProfileImageLoading() const {
  return state_.user.isLoadingImage;
}

int AppStoreQueries::getUserFollowerCount() const {
  return state_.user.followerCount;
}

int AppStoreQueries::getUserFollowingCount() const {
  return state_.user.followingCount;
}

int AppStoreQueries::getUserPostCount() const {
  return state_.user.postCount;
}

bool AppStoreQueries::isUserProfileFetching() const {
  return state_.user.isFetchingProfile;
}

juce::String AppStoreQueries::getUserProfileError() const {
  return state_.user.userError;
}

// ==============================================================================
// Chat Queries
// ==============================================================================

std::vector<juce::String> AppStoreQueries::getChatChannelIds() const {
  std::vector<juce::String> ids;
  for (const auto &channel : state_.chat.channels) {
    ids.push_back(channel.first);
  }
  return ids;
}

juce::String AppStoreQueries::getCurrentChatChannelId() const {
  return state_.chat.currentChannelId;
}

bool AppStoreQueries::areChatChannelsLoading() const {
  return state_.chat.isLoadingChannels;
}

int AppStoreQueries::getTotalUnreadChatCount() const {
  int total = 0;
  for (const auto &channel : state_.chat.channels) {
    total += channel.second.unreadCount;
  }
  return total;
}

bool AppStoreQueries::isChatConnected() const {
  return state_.chat.isAuthenticated;
}

juce::String AppStoreQueries::getChatError() const {
  return state_.chat.chatError;
}

// ==============================================================================
// Notification Queries
// ==============================================================================

std::vector<std::shared_ptr<Notification>> AppStoreQueries::getNotifications() const {
  return state_.notifications.notifications;
}

int AppStoreQueries::getUnreadNotificationCount() const {
  return state_.notifications.unreadCount;
}

int AppStoreQueries::getUnseenNotificationCount() const {
  return state_.notifications.unseenCount;
}

bool AppStoreQueries::areNotificationsLoading() const {
  return state_.notifications.isLoading;
}

juce::String AppStoreQueries::getNotificationError() const {
  return state_.notifications.notificationError;
}

// ==============================================================================
// Search Queries
// ==============================================================================

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getSearchResultPosts() const {
  return state_.search.results.posts;
}

std::vector<std::shared_ptr<User>> AppStoreQueries::getSearchResultUsers() const {
  return state_.search.results.users;
}

bool AppStoreQueries::isSearching() const {
  return state_.search.results.isSearching;
}

juce::String AppStoreQueries::getCurrentSearchQuery() const {
  return state_.search.results.searchQuery;
}

bool AppStoreQueries::hasMoreSearchResults() const {
  return state_.search.results.hasMoreResults;
}

juce::String AppStoreQueries::getSearchError() const {
  return state_.search.results.searchError;
}

// ==============================================================================
// Discovery Queries
// ==============================================================================

std::vector<std::shared_ptr<const User>> AppStoreQueries::getTrendingUsers() const {
  return state_.discovery.trendingUsers;
}

std::vector<std::shared_ptr<const User>> AppStoreQueries::getFeaturedProducers() const {
  return state_.discovery.featuredProducers;
}

std::vector<std::shared_ptr<const User>> AppStoreQueries::getSuggestedUsers() const {
  return state_.discovery.suggestedUsers;
}

bool AppStoreQueries::isDiscoveryLoading() const {
  return state_.discovery.isAnyLoading();
}

bool AppStoreQueries::areTrendingUsersLoading() const {
  return state_.discovery.isTrendingLoading;
}

juce::String AppStoreQueries::getDiscoveryError() const {
  return state_.discovery.discoveryError;
}

// ==============================================================================
// Presence Queries
// ==============================================================================

PresenceStatus AppStoreQueries::getCurrentPresenceStatus() const {
  return state_.presence.currentUserStatus;
}

bool AppStoreQueries::isCurrentUserOnline() const {
  return state_.presence.currentUserStatus == PresenceStatus::Online;
}

bool AppStoreQueries::isPresenceConnected() const {
  return state_.presence.isConnected;
}

juce::String AppStoreQueries::getPresenceError() const {
  return state_.presence.presenceError;
}

// ==============================================================================
// Comment Queries
// ==============================================================================

std::vector<std::shared_ptr<Comment>> AppStoreQueries::getCommentsForPost(const juce::String &postId) const {
  return state_.comments.getCommentsForPost(postId);
}

bool AppStoreQueries::areCommentsLoading(const juce::String &postId) const {
  return state_.comments.isLoadingCommentsForPost(postId);
}

bool AppStoreQueries::hasMoreCommentsForPost(const juce::String &postId) const {
  auto it = state_.comments.hasMoreByPostId.find(postId);
  return it != state_.comments.hasMoreByPostId.end() ? it->second : false;
}

juce::String AppStoreQueries::getCommentError() const {
  return state_.comments.commentsError;
}

// ==============================================================================
// Story Queries
// ==============================================================================

std::vector<std::shared_ptr<Story>> AppStoreQueries::getFeedStories() const {
  return state_.stories.feedUserStories;
}

std::vector<std::shared_ptr<Story>> AppStoreQueries::getMyStories() const {
  return state_.stories.myStories;
}

bool AppStoreQueries::areFeedStoriesLoading() const {
  return state_.stories.isFeedLoading;
}

bool AppStoreQueries::areMyStoriesLoading() const {
  return state_.stories.isMyStoriesLoading;
}

juce::String AppStoreQueries::getStoryError() const {
  return state_.stories.storiesError;
}

// ==============================================================================
// Upload Queries
// ==============================================================================

bool AppStoreQueries::isUploading() const {
  return state_.uploads.isUploading;
}

int AppStoreQueries::getUploadProgress() const {
  return state_.uploads.progress;
}

juce::String AppStoreQueries::getUploadingFileName() const {
  return state_.uploads.currentFileName;
}

juce::String AppStoreQueries::getUploadError() const {
  return state_.uploads.uploadError;
}

// ==============================================================================
// Playlist Queries
// ==============================================================================

std::vector<std::shared_ptr<Playlist>> AppStoreQueries::getPlaylists() const {
  return state_.playlists.playlists;
}

bool AppStoreQueries::arePlaylistsLoading() const {
  return state_.playlists.isLoading;
}

juce::String AppStoreQueries::getPlaylistError() const {
  return state_.playlists.playlistError;
}

// ==============================================================================
// Challenge Queries
// ==============================================================================

std::vector<std::shared_ptr<MIDIChallenge>> AppStoreQueries::getChallenges() const {
  return state_.challenges.challenges;
}

bool AppStoreQueries::areChallengesLoading() const {
  return state_.challenges.isLoading;
}

juce::String AppStoreQueries::getChallengeError() const {
  return state_.challenges.challengeError;
}

// ==============================================================================
// Sound Queries
// ==============================================================================

std::vector<std::shared_ptr<Sound>> AppStoreQueries::getFeaturedSounds() const {
  return state_.sounds.featuredSounds;
}

std::vector<std::shared_ptr<Sound>> AppStoreQueries::getRecentSounds() const {
  return state_.sounds.recentSounds;
}

bool AppStoreQueries::areSoundsLoading() const {
  return state_.sounds.isLoading;
}

bool AppStoreQueries::hasMoreSounds() const {
  return state_.sounds.hasMoreRecent;
}

juce::String AppStoreQueries::getSoundError() const {
  return state_.sounds.soundError;
}

// ==============================================================================
// Draft Queries
// ==============================================================================

std::vector<std::shared_ptr<Draft>> AppStoreQueries::getDrafts() const {
  return state_.drafts.drafts;
}

bool AppStoreQueries::areDraftsLoading() const {
  return state_.drafts.isLoading;
}

juce::String AppStoreQueries::getDraftError() const {
  return state_.drafts.draftError;
}

// ==============================================================================
// Followers/Following Queries
// ==============================================================================

std::vector<std::shared_ptr<const User>> AppStoreQueries::getFollowers() const {
  return state_.followers.users;
}

std::vector<std::shared_ptr<const User>> AppStoreQueries::getFollowing() const {
  return state_.followers.users;
}

bool AppStoreQueries::areFollowersLoading() const {
  return state_.followers.isLoading;
}

juce::String AppStoreQueries::getFollowersTargetUserId() const {
  return juce::String(state_.followers.targetUserId);
}

int AppStoreQueries::getTotalFollowerCount() const {
  return state_.followers.totalCount;
}

} // namespace Stores
} // namespace Sidechain
