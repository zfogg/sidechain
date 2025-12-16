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
#include <chrono>
#include <any>
#include <functional>
#include <rxcpp/rx.hpp>

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

  //==============================================================================
  // Memory Cache for Ephemeral Data
  //
  // Time-to-live (TTL) based expiration for non-persistent data.
  // Used for: users, posts, messages, search results, comments
  // NOT used for: binary assets (images, audio use file cache instead)
  //
  // Cache is session-only and cleared when app closes.
  // Real-time invalidation via WebSocket events.

  /**
   * Get cached value by key with type safety.
   * Returns std::optional - empty if not found or expired.
   * Automatically removes expired entries.
   *
   * @param key Cache key (e.g., "user:123", "feed:home")
   * @return std::optional<T> - cached value or std::nullopt if not cached
   */
  template <typename T> std::optional<T> getCached(const juce::String &key) {
    std::lock_guard<std::mutex> lock(memoryCacheLock);

    auto it = memoryCache.find(key);
    if (it == memoryCache.end()) {
      Util::logDebug("AppStore", "Cache miss: " + key);
      return std::nullopt;
    }

    // Check if expired
    if (isCacheExpired(it->second)) {
      Util::logDebug("AppStore", "Cache expired: " + key);
      memoryCache.erase(it);
      return std::nullopt;
    }

    // Try to cast to requested type
    try {
      auto result = std::any_cast<T>(it->second.value);
      Util::logDebug("AppStore", "Cache hit: " + key);
      return result;
    } catch (const std::bad_any_cast &) {
      Util::logWarning("AppStore", "Cache type mismatch for key: " + key);
      return std::nullopt;
    }
  }

  /**
   * Store value in memory cache with TTL.
   *
   * @param key Cache key (e.g., "user:123", "feed:home")
   * @param value Value to cache
   * @param ttlSeconds Time-to-live in seconds (default 300 = 5 min)
   */
  template <typename T> void setCached(const juce::String &key, const T &value, int ttlSeconds = 300) {
    std::lock_guard<std::mutex> lock(memoryCacheLock);
    memoryCache[key] = CacheEntry{std::any(value), std::chrono::steady_clock::now(), ttlSeconds};
    Util::logDebug("AppStore", "Cache set: " + key + " (TTL: " + juce::String(ttlSeconds) + "s)");
  }

  /**
   * Remove specific cache entry immediately.
   *
   * @param key Cache key to invalidate
   */
  void invalidateCache(const juce::String &key);

  /**
   * Remove all cache entries matching wildcard pattern.
   * Pattern examples: "feed:*", "user:*", "search:*"
   *
   * @param pattern Pattern with trailing * to match prefix
   */
  void invalidateCachePattern(const juce::String &pattern);

  /**
   * Clear all memory caches (for logout or app reset).
   */
  void clearMemoryCaches();

  /**
   * Get current memory cache size in bytes (for monitoring).
   */
  size_t getMemoryCacheSize() const;

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

  // File caching (for binary assets: images, audio, MIDI)
  SidechainImageCache imageCache{500 * 1024 * 1024};        // 500MB
  SidechainAudioCache audioCache{5LL * 1024 * 1024 * 1024}; // 5GB

  // Memory cache (for ephemeral data: users, posts, messages, search results)
  struct CacheEntry {
    std::any value;
    std::chrono::steady_clock::time_point timestamp;
    int ttlSeconds;
  };

  std::map<juce::String, CacheEntry> memoryCache;
  mutable std::mutex memoryCacheLock;

  // Helper to check if cache entry has expired
  bool isCacheExpired(const CacheEntry &entry) const {
    auto elapsed = std::chrono::steady_clock::now() - entry.timestamp;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= entry.ttlSeconds;
  }

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
