#pragma once

#include "../../models/FeedPost.h"
#include "../../models/AggregatedFeedGroup.h"
#include "../../models/Story.h"
#include "../../models/User.h"
#include "../../models/Notification.h"
#include "../../models/Message.h"
#include "../../models/Conversation.h"
#include "../../models/Playlist.h"
#include "../../models/MidiChallenge.h"
#include "../../models/Draft.h"
#include "../../models/Sound.h"
#include "../../util/logging/Logger.h"
#include <JuceHeader.h>
#include <map>
#include <memory>
#include <vector>

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Auth State
// ==============================================================================

struct AuthState {
  bool isLoggedIn = false;
  juce::String userId;
  juce::String username;
  juce::String email;
  juce::String authToken;
  juce::String refreshToken;
  bool isAuthenticating = false;
  bool is2FARequired = false;
  bool isVerifying2FA = false;
  juce::String twoFactorUserId;
  bool isResettingPassword = false;
  juce::String authError;
  int64_t lastAuthTime = 0;
};

// ==============================================================================
// Feed/Posts State
// ==============================================================================

enum class FeedType {
  Timeline,
  Global,
  Trending,
  ForYou,
  Popular,
  Latest,
  Discovery,
  TimelineAggregated,
  TrendingAggregated,
  NotificationAggregated,
  UserActivityAggregated
};

struct FeedState {
  // Store shared_ptr to posts - same memory shared across app, freed when all references drop (RAII)
  std::vector<std::shared_ptr<Sidechain::FeedPost>> posts;
  bool isLoading = false;
  bool isRefreshing = false;
  bool hasMore = true;
  bool isSynced = false;
  int offset = 0;
  int limit = 20;
  int total = 0;
  juce::String error;
  int64_t lastUpdated = 0;
};

struct SavedPostsState {
  juce::Array<FeedPost> posts;
  bool isLoading = false;
  juce::String error;
  int totalCount = 0;
  int offset = 0;
  int limit = 20;
  bool hasMore = true;
  int64_t lastUpdated = 0;
};

struct ArchivedPostsState {
  std::vector<std::shared_ptr<Sidechain::FeedPost>> posts;
  bool isLoading = false;
  juce::String error;
  int totalCount = 0;
  int offset = 0;
  int limit = 20;
  bool hasMore = true;
  int64_t lastUpdated = 0;
};

struct AggregatedFeedState {
  juce::Array<AggregatedFeedGroup> groups;
  bool isLoading = false;
  juce::String error;
  int offset = 0;
  int limit = 20;
  int total = 0;
  bool hasMore = true;
  int64_t lastUpdated = 0;
};

struct PostsState {
  std::map<FeedType, FeedState> feeds;
  std::map<FeedType, AggregatedFeedState> aggregatedFeeds;
  FeedType currentFeedType = FeedType::Timeline;
  SavedPostsState savedPosts;
  ArchivedPostsState archivedPosts;
  juce::String feedError;
  int64_t lastFeedUpdate = 0;

  // Helper to get current feed state
  FeedState *getCurrentFeed() const {
    auto it = feeds.find(currentFeedType);
    if (it != feeds.end()) {
      return const_cast<FeedState *>(&it->second);
    }
    return nullptr;
  }

  // Helper to get current aggregated feed state
  AggregatedFeedState *getCurrentAggregatedFeed() const {
    auto it = aggregatedFeeds.find(currentFeedType);
    if (it != aggregatedFeeds.end()) {
      return const_cast<AggregatedFeedState *>(&it->second);
    }
    return nullptr;
  }
};

// ==============================================================================
// User State
// ==============================================================================

struct UserState {
  juce::String userId;
  juce::String username;
  juce::String email;
  juce::String displayName;
  juce::String bio;
  juce::String location;
  juce::String genre;
  juce::String dawPreference;
  bool isPrivate = false;
  juce::var socialLinks;
  juce::String profilePictureUrl;
  juce::Image profileImage;
  bool isLoadingImage = false;
  bool notificationSoundEnabled = true;
  bool osNotificationsEnabled = true;
  int followerCount = 0;
  int followingCount = 0;
  int postCount = 0;
  bool isFetchingProfile = false;
  juce::String userError;
  int64_t lastProfileUpdate = 0;
};

// ==============================================================================
// Chat State
// ==============================================================================

struct ChannelState {
  juce::String id;
  juce::String name;
  std::vector<std::shared_ptr<Sidechain::Message>> messages;
  std::vector<juce::String> usersTyping;
  bool isLoadingMessages = false;
  int unreadCount = 0;
};

struct ChatState {
  std::map<juce::String, ChannelState> channels;
  std::vector<std::shared_ptr<Sidechain::Conversation>> conversations;
  std::vector<juce::String> channelOrder;
  juce::String currentChannelId;
  bool isLoadingChannels = false;
  bool isConnecting = false;
  bool isAuthenticated = false;
  juce::String chatUserId;
  juce::String chatError;
};

// ==============================================================================
// Notification State
// ==============================================================================

struct NotificationState {
  std::vector<std::shared_ptr<Sidechain::Notification>> notifications;
  int unreadCount = 0;
  int unseenCount = 0;
  bool isLoading = false;
  juce::String notificationError;
};

// ==============================================================================
// Search State
// ==============================================================================

struct SearchResultsState {
  std::vector<std::shared_ptr<Sidechain::FeedPost>> posts;
  std::vector<std::shared_ptr<Sidechain::User>> users;
  juce::String searchQuery;
  juce::String currentGenre;
  bool isSearching = false;
  bool hasMoreResults = false;
  int totalResults = 0;
  int offset = 0;
  int limit = 20;
  juce::String searchError;
  int64_t lastSearchTime = 0;
};

struct GenresState {
  juce::StringArray genres;
  bool isLoading = false;
  juce::String genresError;
};

struct SearchState {
  SearchResultsState results;
  GenresState genres;
};

// ==============================================================================
// Presence State
// ==============================================================================

enum class PresenceStatus { Unknown, Online, Away, Offline, DoNotDisturb };

struct PresenceInfo {
  juce::String userId;
  PresenceStatus status = PresenceStatus::Unknown;
  int64_t lastSeen = 0;
  juce::String statusMessage;
};

struct PresenceState {
  PresenceStatus currentUserStatus = PresenceStatus::Offline;
  int64_t currentUserLastActivity = 0;
  bool isUpdatingPresence = false;
  bool isConnected = false;
  bool isReconnecting = false;
  std::map<juce::String, PresenceInfo> userPresence;
  juce::String presenceError;
};

// ==============================================================================
// Stories State
// ==============================================================================

struct StoriesState {
  std::vector<std::shared_ptr<Sidechain::Story>> feedUserStories;
  std::vector<std::shared_ptr<Sidechain::Story>> myStories;
  std::vector<std::shared_ptr<Sidechain::Story>> highlights;
  bool isFeedLoading = false;
  bool isMyStoriesLoading = false;
  juce::String storiesError;
};

// ==============================================================================
// Upload State
// ==============================================================================

struct UploadState {
  bool isUploading = false;
  int progress = 0;
  juce::String currentFileName;
  juce::String uploadError;
  int64_t startTime = 0;
};

// ==============================================================================
// Playlists State
// ==============================================================================

struct PlaylistState {
  std::vector<std::shared_ptr<Sidechain::Playlist>> playlists;
  bool isLoading = false;
  juce::String playlistError;
};

// ==============================================================================
// Challenges State
// ==============================================================================

struct ChallengeState {
  std::vector<std::shared_ptr<Sidechain::MIDIChallenge>> challenges;
  bool isLoading = false;
  juce::String challengeError;
};

// ==============================================================================
// Sound State
// ==============================================================================

struct SoundState {
  std::shared_ptr<Sidechain::Sound> soundData;
  bool isLoading = false;
  bool isRefreshing = false;
  std::vector<std::shared_ptr<Sidechain::Sound>> featuredSounds;
  bool isFeaturedLoading = false;
  std::vector<std::shared_ptr<Sidechain::Sound>> recentSounds;
  int recentOffset = 0;
  bool hasMoreRecent = true;
  int offset = 0;
  int limit = 20;
  int totalCount = 0;
  juce::String soundError;
  int64_t lastUpdated = 0;
};

// ==============================================================================
// Draft State
// ==============================================================================

struct DraftState {
  std::vector<std::shared_ptr<Sidechain::Draft>> drafts;
  bool isLoading = false;
  juce::String draftError;
};

// ==============================================================================
// Followers State
// ==============================================================================

struct FollowersState {
  std::vector<std::shared_ptr<Sidechain::User>> followers;
  std::vector<std::shared_ptr<Sidechain::User>> following;
  bool isLoading = false;
  juce::String followersError;
};

// ==============================================================================
// Complete App State - Single Source of Truth
// ==============================================================================

/**
 * AppState - Unified, immutable state for entire application
 *
 * Single source of truth containing all app state:
 * - Authentication (login/logout/2FA)
 * - Feed/Posts (all feeds, saved, archived)
 * - User profile
 * - Chat/Messages
 * - Notifications
 * - Search results
 * - User presence
 * - Stories
 * - Uploads
 * - Playlists
 * - Challenges
 * - Sounds
 * - Drafts
 * - Followers
 *
 * AppStore manages this state and provides methods for all operations.
 * Components subscribe to AppStore and react to state changes.
 */
struct AppState {
  AuthState auth;
  PostsState posts;
  UserState user;
  ChatState chat;
  NotificationState notifications;
  SearchState search;
  PresenceState presence;
  StoriesState stories;
  UploadState uploads;
  PlaylistState playlists;
  ChallengeState challenges;
  SoundState sounds;
  DraftState drafts;
  FollowersState followers;

  // Global error for critical failures
  juce::String globalError;
};

} // namespace Stores
} // namespace Sidechain
