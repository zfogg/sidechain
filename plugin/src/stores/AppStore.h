#pragma once

#include "Store.h"
#include "app/AppState.h"
#include "../models/FeedResponse.h"
#include "../models/AggregatedFeedResponse.h"
#include "../network/NetworkClient.h"
#include "../util/cache/FileCache.h"
#include "../util/cache/ImageCache.h"
#include "../util/cache/AudioCache.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * AppStore - Unified reactive store for entire application
 *
 * Single Store<AppState> that manages all application state.
 * Methods organized in separate .cpp files like NetworkClient:
 *   - Auth.cpp - login, logout, 2FA, password reset
 *   - Feed.cpp - load feeds, like, save, repost, etc.
 *   - User.cpp - profile, settings, preferences
 *   - Chat.cpp - messaging, channels, typing indicators
 *   - Search.cpp - search posts/users, genres
 *   - Notifications.cpp - notifications, unread counts
 *   - Presence.cpp - online status, activity
 *   - Stories.cpp - stories, highlights, viewing
 *   - Upload.cpp - file uploads, progress tracking
 *   - Playlists.cpp - playlist management
 *   - Challenges.cpp - MIDI challenges
 *   - Sounds.cpp - sound pages
 *
 * Components inject single AppStore and subscribe to full state:
 *   auto& appStore = AppStore::getInstance();
 *   appStore.subscribe([this](const AppState& state) {
 *       // React to ANY state change
 *       updateUI(state);
 *   });
 *
 * Or subscribe to specific state slices:
 *   appStore.subscribeToAuth([this](const AuthState& auth) { ... });
 */
class AppStore : public Store<AppState> {
public:
  /**
   * Get singleton instance
   */
  static AppStore &getInstance() {
    static AppStore instance;
    return instance;
  }

  /**
   * Set the network client for API calls
   */
  void setNetworkClient(NetworkClient *client);

  //==============================================================================
  // Auth Methods (AppStore_Auth.cpp)

  void login(const juce::String &email, const juce::String &password);
  void registerAccount(const juce::String &email, const juce::String &username, const juce::String &password,
                       const juce::String &displayName);
  void verify2FA(const juce::String &code);
  void requestPasswordReset(const juce::String &email);
  void resetPassword(const juce::String &token, const juce::String &newPassword);
  void logout();
  void oauthCallback(const juce::String &provider, const juce::String &code);
  void setAuthToken(const juce::String &token);
  void refreshAuthToken();

  //==============================================================================
  // Feed/Posts Methods (AppStore_Feed.cpp)

  void loadFeed(FeedType feedType, bool forceRefresh = false);
  void refreshCurrentFeed();
  void loadMore();
  void switchFeedType(FeedType feedType);
  void toggleLike(const juce::String &postId);
  void toggleSave(const juce::String &postId);
  void toggleRepost(const juce::String &postId);
  void toggleMute(const juce::String &postId, bool isMuted);
  void togglePin(const juce::String &postId, bool pinned);
  void toggleFollow(const juce::String &postId, bool willFollow);
  void addReaction(const juce::String &postId, const juce::String &emoji);
  void loadSavedPosts();
  void loadMoreSavedPosts();
  void unsavePost(const juce::String &postId);
  void loadArchivedPosts();
  void loadMoreArchivedPosts();
  void restorePost(const juce::String &postId);

  //==============================================================================
  // Drafts Methods (Draft.cpp)

  void loadDrafts();
  void deleteDraft(const juce::String &draftId);
  void clearAutoRecoveryDraft();

  //==============================================================================
  // User/Profile Methods (User.cpp)

  void fetchUserProfile(bool forceRefresh = false);
  void updateProfile(const juce::String &username = "", const juce::String &displayName = "",
                     const juce::String &bio = "");
  void changeUsername(const juce::String &newUsername);
  void updateProfileComplete(const juce::String &displayName, const juce::String &bio, const juce::String &location,
                             const juce::String &genre, const juce::String &dawPreference, const juce::var &socialLinks,
                             bool isPrivate, const juce::String &profilePictureUrl = "");
  void uploadProfilePicture(const juce::File &imageFile);
  void setProfilePictureUrl(const juce::String &url);
  void setLocalPreviewImage(const juce::File &imageFile);
  void refreshProfileImage();
  void setNotificationSoundEnabled(bool enabled);
  void setOSNotificationsEnabled(bool enabled);
  void updateFollowerCount(int count);
  void updateFollowingCount(int count);
  void updatePostCount(int count);

  //==============================================================================
  // Chat Methods (Chat.cpp)

  void loadChannels();
  void selectChannel(const juce::String &channelId);
  void loadMessages(const juce::String &channelId, int limit = 50);
  void sendMessage(const juce::String &channelId, const juce::String &text);
  void startTyping(const juce::String &channelId);
  void stopTyping(const juce::String &channelId);
  void handleNewMessage(const juce::var &messageData);
  void handleTypingStart(const juce::String &userId);

  //==============================================================================
  // Search Methods (Search.cpp)

  void searchPosts(const juce::String &query);
  void searchUsers(const juce::String &query);
  void loadMoreSearchResults();
  void clearSearchResults();
  void loadGenres();
  void filterByGenre(const juce::String &genre);

  //==============================================================================
  // Notification Methods (Notifications.cpp)

  void loadNotifications();
  void loadMoreNotifications();
  void markNotificationsAsRead();
  void markNotificationsAsSeen();

  //==============================================================================
  // Presence Methods (Presence.cpp)

  void setPresenceStatus(PresenceStatus status);
  void setPresenceStatusMessage(const juce::String &message);
  void recordUserActivity();
  void connectPresence();
  void disconnectPresence();
  void handlePresenceUpdate(const juce::String &userId, const juce::var &presenceData);

  //==============================================================================
  // Stories Methods (Stories.cpp)

  void loadStoriesFeed();
  void loadMyStories();
  void markStoryAsViewed(const juce::String &storyId);
  void deleteStory(const juce::String &storyId);
  void createHighlight(const juce::String &name, const juce::Array<juce::String> &storyIds);

  //==============================================================================
  // Upload Methods (Upload.cpp)

  void uploadPost(const juce::var &postData, const juce::File &audioFile);
  void cancelUpload();

  //==============================================================================
  // Playlist Methods (Playlists.cpp)

  void loadPlaylists();
  void createPlaylist(const juce::String &name, const juce::String &description);
  void deletePlaylist(const juce::String &playlistId);
  void addPostToPlaylist(const juce::String &postId, const juce::String &playlistId);

  //==============================================================================
  // Challenge Methods (Challenges.cpp)

  void loadChallenges();
  void submitChallenge(const juce::String &challengeId, const juce::File &midiFile);

  //==============================================================================
  // Sound Methods (Sounds.cpp)

  void loadFeaturedSounds();
  void loadRecentSounds();
  void loadMoreSounds();
  void refreshSounds();

  //==============================================================================
  // Subscription helpers for state slices

  /**
   * Subscribe only to auth state changes
   */
  std::function<void()> subscribeToAuth(std::function<void(const AuthState &)> callback);

  /**
   * Subscribe only to feed state changes
   */
  std::function<void()> subscribeToFeed(std::function<void(const PostsState &)> callback);

  /**
   * Subscribe only to user state changes
   */
  std::function<void()> subscribeToUser(std::function<void(const UserState &)> callback);

  /**
   * Subscribe only to chat state changes
   */
  std::function<void()> subscribeToChat(std::function<void(const ChatState &)> callback);

  /**
   * Subscribe only to drafts state changes
   */
  std::function<void()> subscribeToDrafts(std::function<void(const DraftState &)> callback);

  /** Subscribe to challenges state slice (for MidiChallenges component) */
  std::function<void()> subscribeToChallenges(std::function<void(const ChallengeState &)> callback);

  /**
   * Subscribe only to stories state changes
   */
  std::function<void()> subscribeToStories(std::function<void(const StoriesState &)> callback);

  /**
   * Subscribe only to upload state changes
   */
  std::function<void()> subscribeToUploads(std::function<void(const UploadState &)> callback);

  /**
   * Subscribe only to notification state changes
   */
  std::function<void()> subscribeToNotifications(std::function<void(const NotificationState &)> callback);

  /**
   * Subscribe only to search state changes
   */
  std::function<void()> subscribeToSearch(std::function<void(const SearchState &)> callback);

  /**
   * Subscribe only to followers state changes
   */
  std::function<void()> subscribeToFollowers(std::function<void(const FollowersState &)> callback);

  /**
   * Subscribe only to playlist state changes
   */
  std::function<void()> subscribeToPlaylists(std::function<void(const PlaylistState &)> callback);

  /**
   * Subscribe only to sounds state changes
   */
  std::function<void()> subscribeToSounds(std::function<void(const SoundState &)> callback);

  //==============================================================================
  // Cache accessors

  /**
   * Get image cache for profile pictures, post thumbnails, etc.
   */
  SidechainImageCache &getImageCache() {
    return imageCache;
  }

  /**
   * Get audio cache for downloaded audio clips, stems, etc.
   */
  SidechainAudioCache &getAudioCache() {
    return audioCache;
  }

  //==============================================================================
  // Image fetching with multi-level caching
  //
  // Single unified interface for all image loading:
  // 1. Memory cache (fast, in-process, lost on app close)
  // 2. File cache (persistent, survives app restarts, on disk)
  // 3. HTTP download (network fetch if not cached)
  //
  // getImage() handles all three levels automatically:
  // - Returns immediately from memory cache
  // - Falls back to file cache, loads to memory
  // - Falls back to HTTP download if not in any cache
  // - Automatically stores in both caches when downloading or loading from file

  /**
   * Get image from URL with automatic multi-level caching.
   *
   * Handles all three cache levels: memory -> file -> network
   * 1. Check memory cache first (returns immediately if found)
   * 2. Check file cache if not in memory
   * 3. Download from HTTP if not in either cache
   * 4. Store in both caches when downloading or loading from file
   *
   * Callback emission timeline:
   * - Memory hit: Calls callback immediately on same thread
   * - File hit: Loads from file on background thread, calls callback on message thread
   * - Network: Downloads on background thread, stores in both caches, calls callback on message thread
   *
   * Usage:
   *   appStore.getImage(url, [this](const juce::Image &img) {
   *       if (img.isValid()) {
   *           setImage(img);
   *           repaint();
   *       }
   *   });
   *
   * @param url Image URL to fetch
   * @param callback Called with image when available (may be null if fetch fails)
   */
  void getImage(const juce::String &url, std::function<void(const juce::Image &)> callback);

protected:
  /**
   * Constructor
   */
  AppStore();

  /**
   * Helper to update auth state
   */
  void updateAuthState(std::function<void(AuthState &)> updater);

  /**
   * Helper to update feed state
   */
  void updateFeedState(std::function<void(PostsState &)> updater);

  /**
   * Helper to update user state
   */
  void updateUserState(std::function<void(UserState &)> updater);

  /**
   * Helper to update chat state
   */
  void updateChatState(std::function<void(ChatState &)> updater);

  /**
   * Helper to update upload state
   */
  void updateUploadState(std::function<void(UploadState &)> updater);

private:
  NetworkClient *networkClient = nullptr;

  // File caching
  SidechainImageCache imageCache{500 * 1024 * 1024};        // 500MB
  SidechainAudioCache audioCache{5LL * 1024 * 1024 * 1024}; // 5GB

  // Feed helpers
  void performFetch(FeedType feedType, int limit, int offset);
  void handleFetchSuccess(FeedType feedType, const juce::var &data, int limit, int offset);
  void handleFetchError(FeedType feedType, const juce::String &error);
  void handleSavedPostsLoaded(Outcome<juce::var> result);
  void handleArchivedPostsLoaded(Outcome<juce::var> result);
  FeedResponse parseJsonResponse(const juce::var &json);
  AggregatedFeedResponse parseAggregatedJsonResponse(const juce::var &json);
  bool isCurrentFeedCached() const;
  bool currentFeedIsFromCache_ = false;

  // User helpers
  void downloadProfileImage(const juce::String &url);
  void handleProfileFetchSuccess(const juce::var &data);
  void handleProfileFetchError(const juce::String &error);

  // Subscription storage for state slice subscriptions
  std::map<uint64_t, std::function<void(const AuthState &)>> authSubscribers;
  std::map<uint64_t, std::function<void(const PostsState &)>> feedSubscribers;
  std::map<uint64_t, std::function<void(const UserState &)>> userSubscribers;
  std::map<uint64_t, std::function<void(const ChatState &)>> chatSubscribers;
  std::map<uint64_t, std::function<void(const DraftState &)>> draftSubscribers;
  std::map<uint64_t, std::function<void(const ChallengeState &)>> challengeSubscribers;
  std::map<uint64_t, std::function<void(const StoriesState &)>> storiesSubscribers;
  std::map<uint64_t, std::function<void(const UploadState &)>> uploadSubscribers;
  std::map<uint64_t, std::function<void(const NotificationState &)>> notificationSubscribers;
  std::map<uint64_t, std::function<void(const SearchState &)>> searchSubscribers;
  std::map<uint64_t, std::function<void(const FollowersState &)>> followersSubscribers;
  std::map<uint64_t, std::function<void(const PlaylistState &)>> playlistSubscribers;
  std::map<uint64_t, std::function<void(const SoundState &)>> soundSubscribers;
  std::atomic<uint64_t> nextSliceSubscriberId{1};
};

} // namespace Stores
} // namespace Sidechain
