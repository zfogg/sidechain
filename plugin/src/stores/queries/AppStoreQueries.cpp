#include "AppStoreQueries.h"

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Feed Queries
// ==============================================================================

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getCurrentFeedPosts() const {
  auto feedType = state_.posts.currentFeedType;
  if (state_.posts.feeds.count(feedType) == 0) {
    return {};
  }
  const auto &feed = state_.posts.feeds.at(feedType);
  std::vector<std::shared_ptr<FeedPost>> result;
  for (const auto &post : feed.posts) {
    if (post) {
      result.push_back(post);
    }
  }
  return result;
}

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getFeedPosts(FeedType feedType) const {
  if (state_.posts.feeds.count(feedType) == 0) {
    return {};
  }
  const auto &feed = state_.posts.feeds.at(feedType);
  std::vector<std::shared_ptr<FeedPost>> result;
  for (const auto &post : feed.posts) {
    if (post) {
      result.push_back(post);
    }
  }
  return result;
}

bool AppStoreQueries::isCurrentFeedLoading() const {
  auto feedType = state_.posts.currentFeedType;
  if (state_.posts.feeds.count(feedType) == 0) {
    return false;
  }
  return state_.posts.feeds.at(feedType).isLoading;
}

bool AppStoreQueries::isFeedLoading(FeedType feedType) const {
  if (state_.posts.feeds.count(feedType) == 0) {
    return false;
  }
  return state_.posts.feeds.at(feedType).isLoading;
}

juce::String AppStoreQueries::getCurrentFeedError() const {
  auto feedType = state_.posts.currentFeedType;
  if (state_.posts.feeds.count(feedType) == 0) {
    return {};
  }
  return state_.posts.feeds.at(feedType).error;
}

bool AppStoreQueries::hasMoreCurrentFeedPosts() const {
  auto feedType = state_.posts.currentFeedType;
  if (state_.posts.feeds.count(feedType) == 0) {
    return false;
  }
  return state_.posts.feeds.at(feedType).hasMore;
}

int AppStoreQueries::getCurrentFeedTotal() const {
  auto feedType = state_.posts.currentFeedType;
  if (state_.posts.feeds.count(feedType) == 0) {
    return 0;
  }
  return state_.posts.feeds.at(feedType).total;
}

int AppStoreQueries::getCurrentFeedOffset() const {
  auto feedType = state_.posts.currentFeedType;
  if (state_.posts.feeds.count(feedType) == 0) {
    return 0;
  }
  return state_.posts.feeds.at(feedType).offset;
}

bool AppStoreQueries::isCurrentFeedSynced() const {
  auto feedType = state_.posts.currentFeedType;
  if (state_.posts.feeds.count(feedType) == 0) {
    return false;
  }
  return state_.posts.feeds.at(feedType).isSynced;
}

// ==============================================================================
// Saved Posts Queries
// ==============================================================================

std::vector<std::shared_ptr<FeedPost>> AppStoreQueries::getSavedPosts() const {
  std::vector<std::shared_ptr<FeedPost>> result;
  for (const auto &post : state_.posts.savedPosts.posts) {
    if (post) {
      result.push_back(post);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<FeedPost>> result;
  for (const auto &post : state_.posts.archivedPosts.posts) {
    if (post) {
      result.push_back(post);
    }
  }
  return result;
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
  std::vector<juce::String> result;
  for (const auto &channelId : state_.chat.channelOrder) {
    result.push_back(channelId);
  }
  return result;
}

juce::String AppStoreQueries::getCurrentChatChannelId() const {
  return state_.chat.currentChannelId;
}

bool AppStoreQueries::areChatChannelsLoading() const {
  return state_.chat.isLoadingChannels;
}

int AppStoreQueries::getTotalUnreadChatCount() const {
  int total = 0;
  for (const auto &[_, channel] : state_.chat.channels) {
    total += channel.unreadMessageCount;
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
  std::vector<std::shared_ptr<Notification>> result;
  for (const auto &notif : state_.notifications.notifications) {
    if (notif) {
      result.push_back(notif);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<FeedPost>> result;
  for (const auto &post : state_.search.results.posts) {
    if (post) {
      result.push_back(post);
    }
  }
  return result;
}

std::vector<std::shared_ptr<User>> AppStoreQueries::getSearchResultUsers() const {
  std::vector<std::shared_ptr<User>> result;
  for (const auto &user : state_.search.results.users) {
    if (user) {
      result.push_back(user);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<const User>> result;
  for (const auto &user : state_.discovery.trendingUsers) {
    if (user) {
      result.push_back(user);
    }
  }
  return result;
}

std::vector<std::shared_ptr<const User>> AppStoreQueries::getFeaturedProducers() const {
  std::vector<std::shared_ptr<const User>> result;
  for (const auto &user : state_.discovery.featuredProducers) {
    if (user) {
      result.push_back(user);
    }
  }
  return result;
}

std::vector<std::shared_ptr<const User>> AppStoreQueries::getSuggestedUsers() const {
  std::vector<std::shared_ptr<const User>> result;
  for (const auto &user : state_.discovery.suggestedUsers) {
    if (user) {
      result.push_back(user);
    }
  }
  return result;
}

bool AppStoreQueries::isDiscoveryLoading() const {
  return state_.discovery.isTrendingLoading || state_.discovery.isFeaturedLoading ||
         state_.discovery.isSuggestedLoading;
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
  if (state_.comments.commentsByPostId.count(postId) == 0) {
    return {};
  }

  const auto &commentList = state_.comments.commentsByPostId.at(postId);
  std::vector<std::shared_ptr<Comment>> result;
  for (const auto &comment : commentList) {
    if (comment) {
      result.push_back(comment);
    }
  }
  return result;
}

bool AppStoreQueries::areCommentsLoading(const juce::String &postId) const {
  if (state_.comments.isLoadingByPostId.count(postId) == 0) {
    return false;
  }
  return state_.comments.isLoadingByPostId.at(postId);
}

bool AppStoreQueries::hasMoreCommentsForPost(const juce::String &postId) const {
  if (state_.comments.hasMoreByPostId.count(postId) == 0) {
    return false;
  }
  return state_.comments.hasMoreByPostId.at(postId);
}

juce::String AppStoreQueries::getCommentError() const {
  return state_.comments.commentsError;
}

// ==============================================================================
// Story Queries
// ==============================================================================

std::vector<std::shared_ptr<Story>> AppStoreQueries::getFeedStories() const {
  std::vector<std::shared_ptr<Story>> result;
  for (const auto &story : state_.stories.feedUserStories) {
    if (story) {
      result.push_back(story);
    }
  }
  return result;
}

std::vector<std::shared_ptr<Story>> AppStoreQueries::getMyStories() const {
  std::vector<std::shared_ptr<Story>> result;
  for (const auto &story : state_.stories.myStories) {
    if (story) {
      result.push_back(story);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<Playlist>> result;
  for (const auto &playlist : state_.playlists.playlists) {
    if (playlist) {
      result.push_back(playlist);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<MIDIChallenge>> result;
  for (const auto &challenge : state_.challenges.challenges) {
    if (challenge) {
      result.push_back(challenge);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<Sound>> result;
  for (const auto &sound : state_.sounds.featuredSounds) {
    if (sound) {
      result.push_back(sound);
    }
  }
  return result;
}

std::vector<std::shared_ptr<Sound>> AppStoreQueries::getRecentSounds() const {
  std::vector<std::shared_ptr<Sound>> result;
  for (const auto &sound : state_.sounds.recentSounds) {
    if (sound) {
      result.push_back(sound);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<Draft>> result;
  for (const auto &draft : state_.drafts.drafts) {
    if (draft) {
      result.push_back(draft);
    }
  }
  return result;
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
  std::vector<std::shared_ptr<const User>> result;
  for (const auto &user : state_.followers.users) {
    if (user) {
      result.push_back(user);
    }
  }
  return result;
}

std::vector<std::shared_ptr<const User>> AppStoreQueries::getFollowing() const {
  std::vector<std::shared_ptr<const User>> result;
  for (const auto &user : state_.followers.users) {
    if (user) {
      result.push_back(user);
    }
  }
  return result;
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
