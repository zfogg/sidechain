#pragma once

#include "AppState.h"
#include "EntityStore.h"
#include "queries/AppStoreQueries.h"
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
 * Manages all application business logic using reactive state management.
 * Uses AppState (StateSubject-based) to coordinate state across domains.
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
 * Components subscribe directly to state:
 *   auto& state = AppState::getInstance();
 *   state.auth->subscribe([this](const AuthState& auth) {
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

  // ==============================================================================
  // Auth Methods (AppStore_Auth.cpp)

  void login(const juce::String &email, const juce::String &password);
  void registerAccount(const juce::String &email, const juce::String &username, const juce::String &password,
                       const juce::String &displayName);
  void verify2FA(const juce::String &code);
  void requestPasswordReset(const juce::String &email);
  void resetPassword(const juce::String &token, const juce::String &newPassword);
  void logout();
  void setAuthToken(const juce::String &token);
  void refreshAuthToken();
  void startTokenRefreshTimer();
  void stopTokenRefreshTimer();

  // Reactive Auth Methods (Phase 7)

  /**
   * Login result containing auth state on success
   */
  struct LoginResult {
    bool success = false;
    bool requires2FA = false;
    juce::String userId;
    juce::String username;
    juce::String token;
    juce::String errorMessage;
  };

  /**
   * Login with reactive observable pattern.
   * Returns LoginResult with auth state or error.
   *
   * @param email User email
   * @param password User password
   * @return Observable that emits LoginResult
   */
  rxcpp::observable<LoginResult> loginObservable(const juce::String &email, const juce::String &password);

  /**
   * Register account with reactive observable pattern.
   *
   * @param email User email
   * @param username Username
   * @param password Password
   * @param displayName Display name
   * @return Observable that emits LoginResult on success
   */
  rxcpp::observable<LoginResult> registerAccountObservable(const juce::String &email, const juce::String &username,
                                                           const juce::String &password,
                                                           const juce::String &displayName);

  /**
   * Verify 2FA code with reactive observable pattern.
   *
   * @param code 2FA verification code
   * @return Observable that emits LoginResult on success
   */
  rxcpp::observable<LoginResult> verify2FAObservable(const juce::String &code);

  /**
   * Request password reset with reactive observable pattern.
   *
   * @param email User email to send reset to
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> requestPasswordResetObservable(const juce::String &email);

  /**
   * Reset password with reactive observable pattern.
   *
   * @param token Reset token from email
   * @param newPassword New password
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> resetPasswordObservable(const juce::String &token, const juce::String &newPassword);

  /**
   * Refresh auth token with reactive observable pattern.
   *
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> refreshAuthTokenObservable();

  // ==============================================================================
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

  // ==============================================================================
  // Drafts Methods (Draft.cpp)

  void loadDrafts();
  void deleteDraft(const juce::String &draftId);
  void clearAutoRecoveryDraft();
  void saveDrafts();

  // ==============================================================================
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

  // Reactive User/Profile Methods (Phase 7)

  /**
   * Fetch user profile with reactive observable pattern.
   * Returns the current user's profile data.
   *
   * @param forceRefresh Force network request even if cached
   * @return Observable that emits User on success
   */
  rxcpp::observable<User> fetchUserProfileObservable(bool forceRefresh = false);

  /**
   * Update user profile with reactive observable pattern.
   *
   * @param username New username (empty to keep current)
   * @param displayName New display name (empty to keep current)
   * @param bio New bio (empty to keep current)
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> updateProfileObservable(const juce::String &username = "",
                                                 const juce::String &displayName = "", const juce::String &bio = "");

  /**
   * Change username with reactive observable pattern.
   *
   * @param newUsername New username
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> changeUsernameObservable(const juce::String &newUsername);

  /**
   * Upload profile picture with reactive observable pattern.
   *
   * @param imageFile Image file to upload
   * @return Observable that emits the new profile picture URL on success
   */
  rxcpp::observable<juce::String> uploadProfilePictureObservable(const juce::File &imageFile);

  // ==============================================================================
  // Discovery Methods (for UserDiscovery component)

  /**
   * Load trending users from the discovery API.
   * Updates SearchState.results.users with trending user list.
   */
  void loadTrendingUsers();

  /**
   * Load featured producers from the discovery API.
   * Updates SearchState.results.users with featured producer list.
   */
  void loadFeaturedProducers();

  /**
   * Load suggested users from the discovery API.
   * Updates SearchState.results.users with suggested user list.
   */
  void loadSuggestedUsers();

  // Reactive Discovery Methods (Phase 5)

  /**
   * Load trending users with reactive observable pattern.
   * Returns value copies of User structs (not shared_ptr).
   *
   * @param limit Maximum number of users to fetch (default 10)
   * @return Observable that emits vector of User
   */
  rxcpp::observable<std::vector<User>> loadTrendingUsersObservable(int limit = 10);

  /**
   * Load featured producers with reactive observable pattern.
   * Returns value copies of User structs.
   *
   * @param limit Maximum number of producers to fetch (default 10)
   * @return Observable that emits vector of User
   */
  rxcpp::observable<std::vector<User>> loadFeaturedProducersObservable(int limit = 10);

  /**
   * Load suggested users with reactive observable pattern.
   * Returns value copies of User structs.
   *
   * @param limit Maximum number of users to fetch (default 10)
   * @return Observable that emits vector of User
   */
  rxcpp::observable<std::vector<User>> loadSuggestedUsersObservable(int limit = 10);

  /**
   * Load all discovery data (trending, featured, suggested) in parallel.
   * Uses merge to combine all three streams.
   *
   * @return Observable that emits DiscoveryState when all data is loaded
   */
  rxcpp::observable<DiscoveryState> loadDiscoveryDataObservable();

  // ==============================================================================
  // Chat Methods (Chat.cpp)

  void loadChannels();
  void selectChannel(const juce::String &channelId);
  void loadMessages(const juce::String &channelId, int limit = 50);
  void sendMessage(const juce::String &channelId, const juce::String &text);
  void editMessage(const juce::String &channelId, const juce::String &messageId, const juce::String &newText);
  void deleteMessage(const juce::String &channelId, const juce::String &messageId);
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
                           const juce::String &userId, const juce::String &userName, const juce::String &createdAt);

  // Reactive Chat Methods (Phase 7)

  /**
   * Load messages for a channel with reactive observable pattern.
   * Returns value copies of Message structs.
   *
   * @param channelId Channel ID to load messages for
   * @param limit Number of messages to load (default 50)
   * @return Observable that emits vector of Message
   */
  rxcpp::observable<std::vector<Message>> loadMessagesObservable(const juce::String &channelId, int limit = 50);

  /**
   * Send message to channel with reactive observable pattern.
   *
   * @param channelId Channel ID to send to
   * @param text Message text
   * @return Observable that emits the sent Message on success
   */
  rxcpp::observable<Message> sendMessageObservable(const juce::String &channelId, const juce::String &text);

  /**
   * Edit message with reactive observable pattern.
   *
   * @param channelId Channel ID
   * @param messageId Message ID to edit
   * @param newText New message text
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> editMessageObservable(const juce::String &channelId, const juce::String &messageId,
                                               const juce::String &newText);

  /**
   * Delete message with reactive observable pattern.
   *
   * @param channelId Channel ID
   * @param messageId Message ID to delete
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> deleteMessageObservable(const juce::String &channelId, const juce::String &messageId);

  // ==============================================================================
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

  // Reactive Search Methods (Phase 2)

  /**
   * Search posts with reactive observable pattern.
   *
   * Use with debounce for live search:
   *   inputObservable
   *       .debounce(std::chrono::milliseconds(300))
   *       .distinct_until_changed()
   *       .flat_map([this](auto q) { return searchPostsObservable(q); })
   *       .subscribe([](auto posts) { ... });
   *
   * @param query Search query string
   * @return Observable that emits vector of FeedPost
   */
  rxcpp::observable<std::vector<FeedPost>> searchPostsObservable(const juce::String &query);

  /**
   * Search users with reactive observable pattern.
   *
   * Use with debounce for live search:
   *   inputObservable
   *       .debounce(std::chrono::milliseconds(300))
   *       .distinct_until_changed()
   *       .flat_map([this](auto q) { return searchUsersReactiveObservable(q); })
   *       .subscribe([](auto users) { ... });
   *
   * @param query Search query string
   * @return Observable that emits vector of User
   */
  rxcpp::observable<std::vector<User>> searchUsersReactiveObservable(const juce::String &query);

  /**
   * Autocomplete users with reactive observable pattern.
   *
   * Use with debounce for live autocomplete:
   *   inputObservable
   *       .debounce(std::chrono::milliseconds(200))
   *       .distinct_until_changed()
   *       .flat_map([this](auto q) { return autocompleteUsersObservable(q); })
   *       .subscribe([](auto suggestions) { ... });
   *
   * @param query Partial username to autocomplete
   * @return Observable that emits array of username suggestions
   */
  rxcpp::observable<juce::Array<juce::String>> autocompleteUsersObservable(const juce::String &query);

  /**
   * Autocomplete genres with reactive observable pattern.
   *
   * @param query Partial genre name to autocomplete
   * @return Observable that emits array of genre suggestions
   */
  rxcpp::observable<juce::Array<juce::String>> autocompleteGenresObservable(const juce::String &query);

  // ==============================================================================
  // Notification Methods (Notifications.cpp)

  void loadNotifications();
  void loadMoreNotifications();
  void markNotificationsAsRead();
  void markNotificationsAsSeen();

  // Reactive Notification Methods (Phase 6)

  /**
   * Load notifications with reactive observable pattern.
   * Returns value copies of Notification structs.
   *
   * @param limit Number of notifications to load (default 20)
   * @param offset Pagination offset (default 0)
   * @return Observable that emits vector of Notification
   */
  rxcpp::observable<std::vector<Notification>> loadNotificationsObservable(int limit = 20, int offset = 0);

  /**
   * Mark all notifications as read with reactive observable pattern.
   *
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> markNotificationsAsReadObservable();

  /**
   * Mark all notifications as seen with reactive observable pattern.
   *
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> markNotificationsAsSeenObservable();

  // ==============================================================================
  // Presence Methods (Presence.cpp)

  void setPresenceStatus(PresenceStatus status);
  void setPresenceStatusMessage(const juce::String &message);
  void recordUserActivity();
  void connectPresence();
  void disconnectPresence();
  void handlePresenceUpdate(const juce::String &userId, const juce::var &presenceData);

  // ==============================================================================
  // Stories Methods (Stories.cpp)

  void loadStoriesFeed();
  void loadMyStories();
  void markStoryAsViewed(const juce::String &storyId);
  void deleteStory(const juce::String &storyId);
  void createHighlight(const juce::String &name, const juce::Array<juce::String> &storyIds);

  // Reactive Stories Methods (Phase 6)

  /**
   * Load stories feed with reactive observable pattern.
   * Returns value copies of Story structs.
   *
   * @return Observable that emits vector of Story
   */
  rxcpp::observable<std::vector<Story>> loadStoriesFeedObservable();

  /**
   * Load user's own stories with reactive observable pattern.
   * Returns value copies of Story structs.
   *
   * @return Observable that emits vector of Story
   */
  rxcpp::observable<std::vector<Story>> loadMyStoriesObservable();

  /**
   * Mark a story as viewed with reactive observable pattern.
   *
   * @param storyId Story ID to mark as viewed
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> markStoryAsViewedObservable(const juce::String &storyId);

  /**
   * Delete a story with reactive observable pattern.
   *
   * @param storyId Story ID to delete
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> deleteStoryObservable(const juce::String &storyId);

  // ==============================================================================
  // Upload Methods (Upload.cpp)

  /**
   * Typed post upload metadata
   */
  struct PostUploadData {
    juce::String filename; // Display filename
    juce::String genre;    // Genre category
    juce::String key;      // Musical key (e.g., "C", "Am")
    double bpm = 0.0;      // Beats per minute
  };

  void uploadPost(const PostUploadData &postData, const juce::File &audioFile);
  void cancelUpload();

  // Reactive Upload Methods (Phase 7)

  /**
   * Upload progress result
   */
  struct UploadProgress {
    float progress = 0.0f; // 0.0 to 1.0
    bool isComplete = false;
    juce::String postId; // Populated on success
    juce::String error;  // Populated on failure
  };

  /**
   * Upload post with reactive observable pattern.
   * Emits progress updates and completes with the created post ID.
   *
   * @param postData Post metadata (filename, genre, key, bpm)
   * @param audioFile Audio file to upload
   * @return Observable that emits UploadProgress updates
   */
  rxcpp::observable<UploadProgress> uploadPostObservable(const PostUploadData &postData, const juce::File &audioFile);

  // ==============================================================================
  // Playlist Methods (Playlists.cpp)

  /**
   * Get user's playlists with reactive observable pattern.
   * Returns list of playlists from server.
   *
   * @return rxcpp::observable<std::vector<Playlist>> - emits typed playlist vector
   */
  rxcpp::observable<std::vector<Playlist>> getPlaylistsObservable();

  void loadPlaylists();
  void createPlaylist(const juce::String &name, const juce::String &description);
  void deletePlaylist(const juce::String &playlistId);
  void addPostToPlaylist(const juce::String &postId, const juce::String &playlistId);

  // Reactive Playlist Methods (Phase 7)

  /**
   * Load playlists with reactive observable pattern.
   * Returns value copies of Playlist structs.
   *
   * @return Observable that emits vector of Playlist
   */
  rxcpp::observable<std::vector<Playlist>> loadPlaylistsObservable();

  /**
   * Create playlist with reactive observable pattern.
   *
   * @param name Playlist name
   * @param description Playlist description
   * @return Observable that emits the created Playlist on success
   */
  rxcpp::observable<Playlist> createPlaylistObservable(const juce::String &name, const juce::String &description);

  /**
   * Delete playlist with reactive observable pattern.
   *
   * @param playlistId Playlist ID to delete
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> deletePlaylistObservable(const juce::String &playlistId);

  /**
   * Add post to playlist with reactive observable pattern.
   *
   * @param postId Post ID to add
   * @param playlistId Playlist ID to add to
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> addPostToPlaylistObservable(const juce::String &postId, const juce::String &playlistId);

  /**
   * Get a single playlist by ID with reactive observable pattern.
   *
   * @param playlistId Playlist ID to fetch
   * @return Observable that emits typed PlaylistDetailResult with playlist, entries, and collaborators
   */
  struct PlaylistDetailResult {
    Playlist playlist;
    std::vector<PlaylistEntry> entries;
    std::vector<PlaylistCollaborator> collaborators;
  };
  rxcpp::observable<PlaylistDetailResult> getPlaylistObservable(const juce::String &playlistId);

  /**
   * Remove an entry from a playlist with reactive observable pattern.
   *
   * @param playlistId Playlist ID
   * @param entryId Entry ID to remove
   * @return Observable that emits 0 on success
   */
  rxcpp::observable<int> removePlaylistEntryObservable(const juce::String &playlistId, const juce::String &entryId);

  // ==============================================================================
  // Challenge Methods (Challenges.cpp)

  void loadChallenges();
  void submitChallenge(const juce::String &challengeId, const juce::File &midiFile);

  // Reactive Challenge Methods (Phase 7)

  /**
   * Load challenges with reactive observable pattern.
   * Returns value copies of MIDIChallenge structs.
   *
   * @return Observable that emits vector of MIDIChallenge
   */
  rxcpp::observable<std::vector<MIDIChallenge>> loadChallengesObservable();

  /**
   * Submit challenge with reactive observable pattern.
   *
   * @param challengeId Challenge ID to submit to
   * @param midiFile MIDI file to submit
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> submitChallengeObservable(const juce::String &challengeId, const juce::File &midiFile);

  /**
   * Get a single MIDI challenge with entries.
   *
   * @param challengeId Challenge ID to fetch
   * @return Observable that emits typed MIDIChallengeDetailResult with challenge and entries
   */
  struct MIDIChallengeDetailResult {
    MIDIChallenge challenge;
    std::vector<MIDIChallengeEntry> entries;
  };
  rxcpp::observable<MIDIChallengeDetailResult> getMIDIChallengeObservable(const juce::String &challengeId);

  /**
   * Vote for a MIDI challenge entry.
   *
   * @param challengeId Challenge ID
   * @param entryId Entry ID to vote for
   * @return Observable that emits 0 on success
   */
  rxcpp::observable<int> voteMIDIChallengeEntryObservable(const juce::String &challengeId, const juce::String &entryId);

  // ==============================================================================
  // Sound Methods (Sounds.cpp)

  void loadFeaturedSounds();
  void loadRecentSounds();
  void loadMoreSounds();
  void refreshSounds();

  // Reactive Sound Methods (Phase 7)

  /**
   * Load featured sounds with reactive observable pattern.
   * Returns value copies of Sound structs.
   *
   * @return Observable that emits vector of Sound
   */
  rxcpp::observable<std::vector<Sound>> loadFeaturedSoundsObservable();

  /**
   * Load recent sounds with reactive observable pattern.
   * Returns value copies of Sound structs.
   *
   * @return Observable that emits vector of Sound
   */
  rxcpp::observable<std::vector<Sound>> loadRecentSoundsObservable();

  /**
   * Get a specific sound by ID with reactive observable pattern.
   *
   * @param soundId Sound ID to fetch
   * @return Observable that emits the Sound
   */
  rxcpp::observable<Sound> getSoundObservable(const juce::String &soundId);

  /**
   * Get the sound associated with a post with reactive observable pattern.
   *
   * @param postId Post ID to get sound for
   * @return Observable that emits the Sound
   */
  rxcpp::observable<Sound> getSoundForPostObservable(const juce::String &postId);

  /**
   * Get posts that use a specific sound with reactive observable pattern.
   *
   * @param soundId Sound ID to get posts for
   * @param limit Maximum number of posts to return
   * @param offset Pagination offset
   * @return Observable that emits vector of SoundPost
   */
  rxcpp::observable<std::vector<SoundPost>> getSoundPostsObservable(const juce::String &soundId, int limit = 50,
                                                                    int offset = 0);

  // ==============================================================================
  // Comment Methods (Comments.cpp)

  /**
   * Get comments for a post with pagination.
   * Uses observable pattern for reactive updates.
   *
   * @deprecated Use loadCommentsObservable() for typed Comment values
   * @param postId Post ID to load comments for
   * @param limit Number of comments to load (default 20)
   * @param offset Pagination offset (default 0)
   */
  rxcpp::observable<juce::Array<juce::var>> getCommentsObservable(const juce::String &postId, int limit = 20,
                                                                  int offset = 0);

  /**
   * Load comments for a post with reactive observable pattern.
   * Returns value copies of Comment structs (not shared_ptr).
   *
   * @param postId Post ID to load comments for
   * @param limit Number of comments to load (default 20)
   * @param offset Pagination offset (default 0)
   * @return Observable that emits vector of Comment
   */
  rxcpp::observable<std::vector<Comment>> loadCommentsObservable(const juce::String &postId, int limit = 20,
                                                                 int offset = 0);

  /**
   * Like a comment with reactive observable pattern.
   * Includes optimistic update with rollback on error.
   *
   * @param commentId Comment ID to like
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> likeCommentObservable(const juce::String &commentId);

  /**
   * Unlike a comment with reactive observable pattern.
   * Includes optimistic update with rollback on error.
   *
   * @param commentId Comment ID to unlike
   * @return Observable that emits 0 on success, errors on failure
   */
  rxcpp::observable<int> unlikeCommentObservable(const juce::String &commentId);

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

  // ==============================================================================
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

  /**
   * Flush all caches to persistent storage.
   * Called during shutdown to ensure cache state is preserved.
   */
  void flushCaches() {
    imageCache.flush();
    audioCache.flush();
    draftCache.flush();
  }

  // ==============================================================================
  // Image fetching with multi-level caching

  // Single unified interface for all image loading:
  // 1. Memory cache (fast, in-process, lost on app close)
  // 2. File cache (persistent, survives app restarts, on disk)
  // 3. HTTP download (network fetch if not cached)

  // getImage handles all three levels automatically:
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

  // ==============================================================================
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

  // ==============================================================================
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

  // ==============================================================================
  // UI Component Subscription Helpers (delegates to StateManager)
  // These are the recommended way for UI components to subscribe to state changes
  // Components should call these methods during setup to get reactive updates

  std::function<void()> subscribeToAuth(std::function<void(const AuthState &)> callback) {
    return stateManager.auth->subscribe(callback);
  }

  std::function<void()> subscribeToChat(std::function<void(const ChatState &)> callback) {
    return stateManager.chat->subscribe(callback);
  }

  std::function<void()> subscribeToChallenges(std::function<void(const ChallengeState &)> callback) {
    return stateManager.challenge->subscribe(callback);
  }

  std::function<void()> subscribeToNotifications(std::function<void(const NotificationState &)> callback) {
    return stateManager.notifications->subscribe(callback);
  }

  std::function<void()> subscribeToFollowers(std::function<void(const FollowersState &)> callback) {
    return stateManager.followers->subscribe(callback);
  }

  std::function<void()> subscribeToUser(std::function<void(const UserState &)> callback) {
    return stateManager.user->subscribe(callback);
  }

  std::function<void()> subscribeToFeed(std::function<void(const PostsState &)> callback) {
    return stateManager.posts->subscribe(callback);
  }

  std::function<void()> subscribeToPlaylists(std::function<void(const PlaylistState &)> callback) {
    return stateManager.playlists->subscribe(callback);
  }

  std::function<void()> subscribeToDrafts(std::function<void(const DraftState &)> callback) {
    return stateManager.draft->subscribe(callback);
  }

  std::function<void()> subscribeToUploads(std::function<void(const UploadState &)> callback) {
    return stateManager.uploads->subscribe(callback);
  }

  std::function<void()> subscribeSounds(std::function<void(const SoundState &)> callback) {
    return stateManager.sounds->subscribe(callback);
  }

  std::function<void()> subscribeToSearch(std::function<void(const SearchState &)> callback) {
    return stateManager.search->subscribe(callback);
  }

  std::function<void()> subscribeToSounds(std::function<void(const SoundState &)> callback) {
    return stateManager.sounds->subscribe(callback);
  }

  std::function<void()> subscribeToStories(std::function<void(const StoriesState &)> callback) {
    return stateManager.stories->subscribe(callback);
  }

  std::function<void()> subscribeToComments(std::function<void(const CommentsState &)> callback) {
    return stateManager.comments->subscribe(callback);
  }

  std::function<void()> subscribeToDiscovery(std::function<void(const DiscoveryState &)> callback) {
    return stateManager.discovery->subscribe(callback);
  }

  // Temporary accessor for UI components - to be removed
  const Stores::AuthState &getAuthState() const {
    return stateManager.auth->getState();
  }

  const Stores::PostsState &getPostsState() const {
    return stateManager.posts->getState();
  }

  const Stores::UserState &getUserState() const {
    return stateManager.user->getState();
  }

  const Stores::ChatState &getChatState() const {
    return stateManager.chat->getState();
  }

  const Stores::SearchState &getSearchState() const {
    return stateManager.search->getState();
  }

  const Stores::NotificationState &getNotificationState() const {
    return stateManager.notifications->getState();
  }

  // ==============================================================================
  // User Service Operations (File Cache: memory only with 5-min TTL)

  /**
   * Search for users by query string with memory caching (Reactive).
   *
   * @param query Search query (username, display name, etc)
   * @return rxcpp::observable<std::vector<User>> - emits typed User vector
   *
   * Caching strategy:
   * - Check memory cache for exact query (2 min TTL)
   * - If miss or expired, fetch from network
   * - Cache result for identical future queries
   */
  rxcpp::observable<std::vector<User>> searchUsersObservable(const juce::String &query);

  // ==============================================================================
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
   * Load multiple feed types in parallel using merge (Reactive).
   *
   * @param feedTypes Vector of feed types to load
   * @return rxcpp::observable<juce::var> - emits each feed as it arrives
   *
   * Usage:
   *   appStore.loadMultipleFeedsObservable({FeedType::Timeline, FeedType::Trending})
   *       .subscribe([](auto feedData) { processFeed(feedData); });
   */
  rxcpp::observable<juce::var> loadMultipleFeedsObservable(const std::vector<FeedType> &feedTypes);

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

  // ==============================================================================
  // WebSocket Event Handlers for Real-Time Cache Invalidation

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

  /**
   * Handle WebSocket comment count update for a post.
   * Invalidates post cache and updates comment count in state.
   *
   * @param postId ID of the post with updated comment count
   * @param commentCount New comment count
   */
  void onWebSocketCommentCountUpdate(const juce::String &postId, int commentCount);

  /**
   * Handle WebSocket new comment notification.
   * Invalidates comment cache for the post.
   *
   * @param postId ID of the post with new comment
   * @param commentId ID of the new comment
   * @param username Username of the comment author
   */
  void onWebSocketNewComment(const juce::String &postId, const juce::String &commentId, const juce::String &username);

  // ==============================================================================
  // Model-Level Subscription API (New Redux Pattern)
  // These methods provide strongly-typed access to models with automatic normalization
  // All components should use these instead of direct NetworkClient calls

  /**
   * Subscribe to a specific post by ID with strong typing.
   * Returns shared_ptr<FeedPost> from EntityStore (single source of truth).
   *
   * @param postId Post ID to subscribe to
   * @param callback Called whenever this post changes (likes, comments, etc.)
   * @return Unsubscriber function - call to stop receiving updates
   */
  std::function<void()> subscribeToPost(const juce::String &postId,
                                        std::function<void(const std::shared_ptr<FeedPost> &)> callback);

  /**
   * Subscribe to all feed posts as strongly-typed models.
   * Returns vector<shared_ptr<FeedPost>> from EntityStore.
   *
   * @param callback Called with current feed posts whenever feed changes
   * @return Unsubscriber function
   */
  std::function<void()> subscribeToPosts(std::function<void(const std::vector<std::shared_ptr<FeedPost>> &)> callback);

  /**
   * Subscribe to a specific user by ID with strong typing.
   * Returns shared_ptr<User> from EntityStore (single source of truth).
   *
   * @param userId User ID to subscribe to
   * @param callback Called whenever this user's profile changes
   * @return Unsubscriber function
   */
  std::function<void()> subscribeToUser(const juce::String &userId,
                                        std::function<void(const std::shared_ptr<User> &)> callback);

  /**
   * Subscribe to comments for a specific post with strong typing.
   * Returns vector<shared_ptr<Comment>> from EntityStore.
   *
   * @param postId Post ID to get comments for
   * @param callback Called with comment list whenever comments change
   * @return Unsubscriber function
   */
  std::function<void()>
  subscribeToPostComments(const juce::String &postId,
                          std::function<void(const std::vector<std::shared_ptr<Comment>> &)> callback);

  /**
   * Subscribe to a specific comment by ID with strong typing.
   * Returns shared_ptr<Comment> from EntityStore.
   *
   * @param commentId Comment ID to subscribe to
   * @param callback Called whenever this comment changes (reactions, edits)
   * @return Unsubscriber function
   */
  std::function<void()> subscribeToComment(const juce::String &commentId,
                                           std::function<void(const std::shared_ptr<Comment> &)> callback);

  /**
   * Load user profile from network and cache in EntityStore.
   * User will be available via subscribeToUser(userId, callback).
   * Waits for server confirmation before updating (pessimistic).
   *
   * @param userId User ID to load
   * @param forceRefresh Force network request even if cached
   */
  void loadUser(const juce::String &userId, bool forceRefresh = false);

  /**
   * Load posts for a user and cache in EntityStore.
   * Posts will be available via subscribeToPosts() or subscribeToPost(postId).
   *
   * @param userId User ID to load posts for
   * @param limit Number of posts to load
   * @param offset Pagination offset
   */
  void loadUserPosts(const juce::String &userId, int limit = 20, int offset = 0);

  /**
   * Load comments for a post and cache in EntityStore.
   * Comments will be available via subscribeToPostComments(postId) or subscribeToComment(commentId).
   *
   * @param postId Post ID to load comments for
   * @param limit Number of comments to load
   * @param offset Pagination offset
   */
  void loadPostComments(const juce::String &postId, int limit = 20, int offset = 0);

  /**
   * Load followers for a user and cache in EntityStore.
   *
   * @param userId User ID to load followers for
   * @param limit Number of followers to load
   * @param offset Pagination offset
   */
  void loadFollowers(const juce::String &userId, int limit = 20, int offset = 0);

  /**
   * Load following list for a user and cache in EntityStore.
   *
   * @param userId User ID to load following for
   * @param limit Number to load
   * @param offset Pagination offset
   */
  void loadFollowing(const juce::String &userId, int limit = 20, int offset = 0);

  /**
   * Search users and cache results in EntityStore.
   * Results available via subscribeToSearch().
   *
   * @param query Search query
   * @param limit Number of results
   * @param offset Pagination offset
   */
  void searchUsersAndCache(const juce::String &query, int limit = 20, int offset = 0);

  /**
   * Load trending users and cache in EntityStore.
   *
   * @param limit Number of trending users to load
   */
  void loadTrendingUsersAndCache(int limit = 20);

  /**
   * Load featured producers and cache in EntityStore.
   *
   * @param limit Number of featured producers to load
   */
  void loadFeaturedProducersAndCache(int limit = 20);

  /**
   * Load suggested users and cache in EntityStore.
   *
   * @param limit Number of suggested users to load
   */
  void loadSuggestedUsersAndCache(int limit = 20);

  /**
   * Get strongly-typed query interface for accessing derived/computed state.
   *
   * Components should use queries instead of directly accessing state structure.
   * This decouples UI from internal state organization.
   *
   * Example:
   *   auto queries = AppStore::getInstance().queries();
   *   auto posts = queries.getCurrentFeedPosts();
   *   if (queries.isCurrentFeedLoading()) { showSpinner(); }
   *
   * @return AppStoreQueries instance for reading state
   */
  AppStoreQueries queries() const {
    // Build composite AppState from all state subjects
    AppState combinedState;
    combinedState.auth = stateManager.auth->getState();
    combinedState.posts = stateManager.posts->getState();
    combinedState.user = stateManager.user->getState();
    combinedState.chat = stateManager.chat->getState();
    combinedState.notifications = stateManager.notifications->getState();
    combinedState.search = stateManager.search->getState();
    combinedState.comments = stateManager.comments->getState();
    combinedState.discovery = stateManager.discovery->getState();
    combinedState.presence = stateManager.presence->getState();
    combinedState.stories = stateManager.stories->getState();
    combinedState.uploads = stateManager.uploads->getState();
    combinedState.playlists = stateManager.playlists->getState();
    combinedState.challenges = stateManager.challenge->getState();
    combinedState.sounds = stateManager.sounds->getState();
    combinedState.drafts = stateManager.draft->getState();
    combinedState.followers = stateManager.followers->getState();

    return AppStoreQueries(combinedState);
  }

  /**
   * Get EntityStore instance for direct normalized access to all cached models.
   * Use for read-only access. Use AppStore methods for mutations.
   */
  EntityStore &getEntityStore() {
    return EntityStore::getInstance();
  }

private:
  NetworkClient *networkClient = nullptr;
  StreamChatClient *streamChatClient = nullptr;

  // ==============================================================================
  // State Management
  // AppStore is a pure orchestration/service layer
  // All state is managed by reactive StateSubjects via StateManager
  StateManager &stateManager = StateManager::getInstance();

  // File caching (for binary assets: images, audio, MIDI, drafts)
  SidechainImageCache imageCache{500 * 1024 * 1024};        // 500MB
  SidechainAudioCache audioCache{5LL * 1024 * 1024 * 1024}; // 5GB
  SidechainDraftCache draftCache{100 * 1024 * 1024};        // 100MB

  // Feed helpers
  void performFetch(FeedType feedType, int limit, int offset);
  void handleFetchSuccess(FeedType feedType, const juce::var &data, int limit, int offset);
  void handleTypedFetchSuccess(FeedType feedType, const NetworkClient::FeedResult &result, int limit, int offset);
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

  // Discovery handlers
  void handleTrendingUsersSuccess(const juce::var &data);
  void handleTrendingUsersError(const juce::String &error);
  void handleFeaturedProducersSuccess(const juce::var &data);
  void handleFeaturedProducersError(const juce::String &error);
  void handleSuggestedUsersSuccess(const juce::var &data);
  void handleSuggestedUsersError(const juce::String &error);

  // Token refresh timer
  class TokenRefreshTimer : public juce::Timer {
  public:
    TokenRefreshTimer(AppStore *store) : store_(store) {}

    void timerCallback() override {
      if (store_) {
        store_->checkAndRefreshToken();
      }
    }

  private:
    AppStore *store_;
  };

  std::unique_ptr<TokenRefreshTimer> tokenRefreshTimer_;
  void checkAndRefreshToken();

  // Private constructor for singleton pattern
  AppStore();
};

} // namespace Stores
} // namespace Sidechain
