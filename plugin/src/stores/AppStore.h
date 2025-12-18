#pragma once

#include "slices/AppSlices.h"
#include "../models/FeedResponse.h"
#include "../models/AggregatedFeedResponse.h"
#include "../network/NetworkClient.h"
#include "../network/StreamChatClient.h"
#include "../util/cache/FileCache.h"
#include "../util/cache/ImageCache.h"
#include "../util/cache/AudioCache.h"
#include "../util/cache/DraftCache.h"
#include <JuceHeader.h>
#include <chrono>
#include <functional>
#include <optional>
#include <mutex>
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

/**
 * AppStore - Pure orchestration and business logic layer
 *
 * Manages all application business logic by dispatching actions to independent slices.
 * Uses AppSliceManager to coordinate state across domains.
 *
 * Methods organized in separate .cpp files:
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
 * Components subscribe directly to slices:
 *   auto& manager = AppSliceManager::getInstance();
 *   manager.getAuthSlice()->subscribe([this](const AuthState& auth) {
 *       if (auth.isLoggedIn) updateUI();
 *   });
 *
 * Components dispatch actions via AppStore methods:
 *   AppStore::getInstance().login(email, password);
 *   AppStore::getInstance().loadFeed(FeedType::Timeline);
 */
class AppStore {
public:
  /**
   * Get singleton instance
   */
  static AppStore &getInstance() {
    static AppStore instance;
    return instance;
  }

  // Deleted copy constructor and assignment
  AppStore(const AppStore &) = delete;
  AppStore &operator=(const AppStore &) = delete;

  /**
   * Set the network client for API calls
   */
  void setNetworkClient(NetworkClient *client);
  void setStreamChatClient(StreamChatClient *client);

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

  /**
   * Follow a user by userId (not by post).
   * Invalidates user search cache so followers list updates immediately.
   *
   * @param userId User ID to follow
   */
  void followUser(const juce::String &userId);

  /**
   * Unfollow a user by userId.
   * Invalidates user search cache so followers list updates immediately.
   *
   * @param userId User ID to unfollow
   */
  void unfollowUser(const juce::String &userId);

  //==============================================================================
  // Chat Methods (Chat.cpp)

  void loadChannels();
  void selectChannel(const juce::String &channelId);
  void loadMessages(const juce::String &channelId, int limit = 50);
  void sendMessage(const juce::String &channelId, const juce::String &text);
  void editMessage(const juce::String &channelId, const juce::String &messageId, const juce::String &newText);
  void startTyping(const juce::String &channelId);
  void stopTyping(const juce::String &channelId);
  void handleNewMessage(const juce::var &messageData);
  void handleTypingStart(const juce::String &userId);

  /**
   * Add a channel to chat state (for network responses).
   * Used by PluginEditor when receiving channel data from network.
   *
   * @param channelId Channel ID
   * @param channelName Channel name/display name
   */
  void addChannelToState(const juce::String &channelId, const juce::String &channelName = "");

  /**
   * Add a message to a channel in chat state (for network responses).
   * Used by PluginEditor when receiving message data from network.
   *
   * @param channelId Channel ID to add message to
   * @param messageId Message ID
   * @param text Message text
   * @param userId User ID of sender
   * @param userName User name of sender
   * @param createdAt ISO 8601 timestamp
   */
  void addMessageToChannel(const juce::String &channelId, const juce::String &messageId, const juce::String &text,
                           const juce::String &userId, const juce::String &userName,
                           const juce::String &createdAt);

  //==============================================================================
  // Search Methods (Search.cpp)

  void searchPosts(const juce::String &query);
  void searchUsers(const juce::String &query);
  void loadMoreSearchResults();
  void clearSearchResults();
  void loadGenres();
  void filterByGenre(const juce::String &genre);
  void autocompleteUsers(const juce::String &query,
                         std::function<void(const juce::Array<juce::String> &suggestions)> callback);
  void autocompleteGenres(const juce::String &query,
                          std::function<void(const juce::Array<juce::String> &suggestions)> callback);

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

  /**
   * Get user's playlists with reactive observable pattern.
   * Returns list of playlists from server.
   *
   * @return rxcpp::observable<juce::Array<juce::var>> - emits playlist array when available
   */
  rxcpp::observable<juce::Array<juce::var>> getPlaylistsObservable();

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
  // Comment Methods (Comments.cpp)

  /**
   * Get comments for a post with pagination.
   * Uses observable pattern for reactive updates.
   *
   * @param postId Post ID to load comments for
   * @param limit Number of comments to load (default 20)
   * @param offset Pagination offset (default 0)
   */
  rxcpp::observable<juce::Array<juce::var>> getCommentsObservable(const juce::String &postId, int limit = 20,
                                                                  int offset = 0);

  /**
   * Create a new comment on a post.
   *
   * @param postId Post ID to comment on
   * @param content Comment text
   * @param parentId Optional parent comment ID for replies
   */
  void createComment(const juce::String &postId, const juce::String &content, const juce::String &parentId = "");

  /**
   * Delete a comment.
   *
   * @param commentId Comment ID to delete
   */
  void deleteComment(const juce::String &commentId);

  /**
   * Like a comment.
   *
   * @param commentId Comment ID to like
   */
  void likeComment(const juce::String &commentId);

  /**
   * Unlike a comment.
   *
   * @param commentId Comment ID to unlike
   */
  void unlikeComment(const juce::String &commentId);

  /**
   * Update an existing comment.
   *
   * @param commentId Comment ID to update
   * @param content New comment text
   */
  void updateComment(const juce::String &commentId, const juce::String &content);

  /**
   * Report a comment.
   *
   * @param commentId Comment ID to report
   * @param reason Report reason
   * @param description Detailed report description
   */
  void reportComment(const juce::String &commentId, const juce::String &reason, const juce::String &description);

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


  //==============================================================================
  // Image Service Operations (File Cache: memory → file → network)

  /**
   * Load image from URL asynchronously with automatic multi-tier caching (Reactive).
   *
   * @param url Image URL
   * @return rxcpp::observable<Image> - emits loaded image when available
   */
  rxcpp::observable<juce::Image> loadImageObservable(const juce::String &url);

  /**
   * Get image from cache synchronously (no network).
   * Uses existing SidechainImageCache (memory → file hierarchy).
   */
  juce::Image getCachedImage(const juce::String &url);

  //==============================================================================
  // Audio Service Operations (File Cache: file → network)

  /**
   * Load audio file from URL asynchronously with disk caching (Reactive).
   *
   * @param url Audio URL
   * @return rxcpp::observable<File> - emits file path when available
   */
  rxcpp::observable<juce::File> loadAudioObservable(const juce::String &url);

  /**
   * Get audio file from cache synchronously (no network).
   * Uses existing SidechainAudioCache (file cache only).
   */
  juce::File getCachedAudio(const juce::String &url);

  //==============================================================================
  // UI Component Subscription Helpers (Thin delegates to slices)
  // TODO: Phase 5 - Remove these and update UI components to subscribe directly to slices

  std::function<void()> subscribeToAuth(std::function<void(const AuthState &)> callback) {
    sliceManager.getAuthSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToChat(std::function<void(const ChatState &)> callback) {
    sliceManager.getChatSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToChallenges(std::function<void(const ChallengeState &)> callback) {
    sliceManager.getChallengeSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToNotifications(std::function<void(const NotificationState &)> callback) {
    sliceManager.getNotificationSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToFollowers(std::function<void(const FollowersState &)> callback) {
    sliceManager.getFollowersSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToUser(std::function<void(const UserState &)> callback) {
    sliceManager.getUserSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToFeed(std::function<void(const PostsState &)> callback) {
    sliceManager.getPostsSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToPlaylists(std::function<void(const PlaylistState &)> callback) {
    sliceManager.getPlaylistSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToDrafts(std::function<void(const DraftState &)> callback) {
    sliceManager.getDraftSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToUploads(std::function<void(const UploadState &)> callback) {
    sliceManager.getUploadSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeSounds(std::function<void(const SoundState &)> callback) {
    sliceManager.getSoundSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToSearch(std::function<void(const SearchState &)> callback) {
    sliceManager.getSearchSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToSounds(std::function<void(const SoundState &)> callback) {
    sliceManager.getSoundSlice()->subscribe(callback);
    return []() {};
  }

  std::function<void()> subscribeToStories(std::function<void(const StoriesState &)> callback) {
    sliceManager.getStoriesSlice()->subscribe(callback);
    return []() {};
  }

  // Temporary accessor for UI components - to be removed in Phase 5
  const Stores::AuthState &getAuthState() const {
    return sliceManager.getAuthSlice()->getState();
  }

  const Stores::PostsState &getPostsState() const {
    return sliceManager.getPostsSlice()->getState();
  }

  const Stores::UserState &getUserState() const {
    return sliceManager.getUserSlice()->getState();
  }

  const Stores::ChatState &getChatState() const {
    return sliceManager.getChatSlice()->getState();
  }

  const Stores::SearchState &getSearchState() const {
    return sliceManager.getSearchSlice()->getState();
  }

  const Stores::NotificationState &getNotificationState() const {
    return sliceManager.getNotificationSlice()->getState();
  }

  //==============================================================================
  // User Service Operations (File Cache: memory only with 5-min TTL)

  /**
   * Search for users by query string with memory caching (Reactive).
   *
   * @param query Search query (username, display name, etc)
   * @return rxcpp::observable<juce::Array<juce::var>> - emits array of matching users
   *
   * Caching strategy:
   * - Check memory cache for exact query (2 min TTL)
   * - If miss or expired, fetch from network
   * - Cache result for identical future queries
   */
  rxcpp::observable<juce::Array<juce::var>> searchUsersObservable(const juce::String &query);

  //==============================================================================
  // Feed Service Operations (Memory Cache: 30-second TTL for frequent updates)

  /**
   * Load a feed by type with memory caching (Reactive).
   *
   * @param feedType Feed type (Timeline, Global, Trending, ForYou, etc.)
   * @return rxcpp::observable<juce::var> - emits feed data when available
   *
   * Caching strategy:
   * - Check memory cache for exact feed type (30 sec TTL)
   * - If miss or expired, fetch from network
   * - Cache result for 30 seconds (feeds update frequently)
   * - Automatically invalidated by likePost/unlikePost/savePost mutations
   */
  rxcpp::observable<juce::var> loadFeedObservable(FeedType feedType);

  /**
   * Like a post with automatic cache invalidation (Reactive).
   *
   * @param postId Post ID to like
   * @return rxcpp::observable<int> - emits 0 on success, completes with error on failure
   *
   * Side effects:
   * - Optimistic update applied immediately
   * - On success: Invalidates "feed:*" cache pattern
   * - On error: Reverts optimistic update
   *
   * Cache invalidation strategy:
   * - Invalidates "feed:*" pattern to refresh all feed caches
   * - Allows WebSocket to repopulate with fresh data
   */
  rxcpp::observable<int> likePostObservable(const juce::String &postId);

  /**
   * Save/unsave a post reactively with optimistic update.
   *
   * - On success: Invalidates "feed:*" cache pattern
   * - On error: Reverts optimistic update
   */
  rxcpp::observable<int> toggleSaveObservable(const juce::String &postId);

  /**
   * Repost a post reactively with optimistic update.
   *
   * - On success: Invalidates "feed:*" cache pattern
   * - On error: Reverts optimistic update
   */
  rxcpp::observable<int> toggleRepostObservable(const juce::String &postId);

  /**
   * Pin/unpin a post reactively with optimistic update.
   *
   * - On success: Invalidates "feed:*" cache pattern
   * - On error: Reverts optimistic update
   */
  rxcpp::observable<int> togglePinObservable(const juce::String &postId, bool pinned);

  /**
   * Follow/unfollow a user reactively with optimistic update.
   *
   * - On success: Invalidates user search cache
   * - On error: Reverts optimistic update
   */
  rxcpp::observable<int> toggleFollowObservable(const juce::String &postId, bool willFollow);

  /**
   * Add a reaction to a post reactively with optimistic update.
   *
   * - On success: Invalidates "feed:*" cache pattern
   * - On error: Reverts optimistic update
   */
  rxcpp::observable<int> addReactionObservable(const juce::String &postId, const juce::String &emoji);

  /**
   * Follow a user by userId reactively.
   *
   * - On success: Invalidates user search cache
   * - Invalidates followers list cache
   */
  rxcpp::observable<int> followUserObservable(const juce::String &userId);

  /**
   * Unfollow a user by userId reactively.
   *
   * - On success: Invalidates user search cache
   * - Invalidates followers list cache
   */
  rxcpp::observable<int> unfollowUserObservable(const juce::String &userId);

  //==============================================================================
  // WebSocket Event Handlers for Real-Time Cache Invalidation (Phase 5)
  //
  // Called by PluginEditor when WebSocket messages arrive from backend.
  // Each handler invalidates relevant cache entries to keep data fresh in real-time.

  /**
   * Handle WebSocket post update (like, repost, comment added to post).
   * Invalidates post cache and feed caches.
   *
   * @param postId ID of the post that was updated
   */
  void onWebSocketPostUpdated(const juce::String &postId);

  /**
   * Handle WebSocket like count update for a post.
   * Invalidates post cache and updates state if needed.
   *
   * @param postId ID of the post with updated like count
   * @param likeCount New like count
   */
  void onWebSocketLikeCountUpdate(const juce::String &postId, int likeCount);

  /**
   * Handle WebSocket follower count update for a user.
   * Invalidates user cache and updates state if needed.
   *
   * @param userId ID of the user with updated follower count
   * @param followerCount New follower count
   */
  void onWebSocketFollowerCountUpdate(const juce::String &userId, int followerCount);

  /**
   * Handle WebSocket new post notification.
   * Invalidates all feed caches so new post appears.
   *
   * @param postData Serialized post data from WebSocket
   */
  void onWebSocketNewPost(const juce::var &postData);

  /**
   * Handle WebSocket user profile update.
   * Invalidates user cache.
   *
   * @param userId ID of the user with updated profile
   */
  void onWebSocketUserUpdated(const juce::String &userId);

  /**
   * Handle WebSocket new message notification.
   * Invalidates message cache for the conversation.
   *
   * @param conversationId ID of the conversation with new message
   */
  void onWebSocketNewMessage(const juce::String &conversationId);

  /**
   * Handle WebSocket presence update (user online/offline).
   * Updates presence state without full cache invalidation.
   *
   * @param userId ID of the user with updated presence
   * @param isOnline Whether user is now online
   */
  void onWebSocketPresenceUpdate(const juce::String &userId, bool isOnline);

private:
  NetworkClient *networkClient = nullptr;
  StreamChatClient *streamChatClient = nullptr;

  //==============================================================================
  // Slice Architecture (Phase 3 refactoring)
  // AppStore is now a pure orchestration/service layer
  // All state is managed by independent slices via AppSliceManager
  Slices::AppSliceManager &sliceManager = Slices::AppSliceManager::getInstance();

  // File caching (for binary assets: images, audio, MIDI, drafts)
  SidechainImageCache imageCache{500 * 1024 * 1024};        // 500MB
  SidechainAudioCache audioCache{5LL * 1024 * 1024 * 1024}; // 5GB
  SidechainDraftCache draftCache{100 * 1024 * 1024};        // 100MB

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
  void downloadProfileImage(const juce::String &userId, const juce::String &url);
  void handleProfileFetchSuccess(const juce::var &data);
  void handleProfileFetchError(const juce::String &error);

  // Private constructor for singleton pattern
  AppStore();
};

} // namespace Stores
} // namespace Sidechain
