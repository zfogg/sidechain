#pragma once

#include "ReactiveStore.h"
#include "app/AppState.h"
#include <JuceHeader.h>
#include <memory>

namespace Sidechain {
namespace Stores {

/**
 * RootStore - Unified application state management
 *
 * Replaces the 15-slice AppSliceManager pattern with a single ReactiveStore<AppState>
 * that manages all application state at once.
 *
 * BENEFITS:
 * - Single store instead of 15 separate facades
 * - Simpler dependency injection
 * - Clearer state relationships
 * - Easier testing (just pass RootStore*)
 * - No more AppSliceManager::getInstance() scattered everywhere
 *
 * Architecture:
 * - RootStore holds one ReactiveStore<AppState>
 * - AppState contains all domain state (auth, posts, user, chat, etc.)
 * - Subscribe to entire AppState or use selectors for specific parts
 * - All state updates go through setState(newAppState)
 *
 * Usage:
 *   // Create root store
 *   auto rootStore = std::make_unique<RootStore>();
 *
 *   // Subscribe to entire state
 *   rootStore->subscribe([this](const AppState& state) {
 *       updateUI(state);
 *   });
 *
 *   // Subscribe to specific part (selector pattern)
 *   rootStore->subscribeToAuth([this](const AuthState& auth) {
 *       if (auth.isLoggedIn) { ... }
 *   });
 *
 *   // Update state
 *   auto newState = rootStore->getState();
 *   newState.auth.isLoggedIn = true;
 *   rootStore->setState(newState);
 *
 * Convenience methods for common subscriptions:
 *   rootStore->subscribeToAuth(callback);      // Only AuthState
 *   rootStore->subscribeToPosts(callback);     // Only PostsState
 *   rootStore->subscribeToUser(callback);      // Only UserState
 *   rootStore->subscribeToChat(callback);      // Only ChatState
 *   // ... and so on for all state types
 */
class RootStore {
public:
  using Unsubscriber = std::function<void()>;

  RootStore() = default;
  ~RootStore() = default;

  // ============================================================================
  // Core Store Access
  // ============================================================================

  /**
   * Get current complete application state
   */
  const AppState &getState() const {
    return store_.getState();
  }

  /**
   * Copy current state (for modification)
   */
  AppState copyState() const {
    return store_.copyState();
  }

  /**
   * Set new complete application state
   * Triggers all subscribers
   */
  void setState(const AppState &newState) {
    store_.setState(newState);
  }

  /**
   * Update state via updater function
   */
  void updateState(std::function<void(AppState &)> updater) {
    store_.updateState(updater);
  }

  // ============================================================================
  // Full State Subscription
  // ============================================================================

  /**
   * Subscribe to all state changes
   */
  Unsubscriber subscribe(std::function<void(const AppState &)> callback) {
    return store_.subscribe(callback);
  }

  // ============================================================================
  // Auth State Subscriptions
  // ============================================================================

  /**
   * Subscribe to auth state changes (selector pattern)
   */
  Unsubscriber subscribeToAuth(std::function<void(const AuthState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.auth; }, callback);
  }

  /**
   * Subscribe to login status changes only
   */
  Unsubscriber subscribeToLoginStatus(std::function<void(bool)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.auth.isLoggedIn; }, callback);
  }

  /**
   * Subscribe to auth token changes
   */
  Unsubscriber subscribeToAuthToken(std::function<void(const juce::String &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.auth.authToken; }, callback);
  }

  // ============================================================================
  // Posts/Feed State Subscriptions
  // ============================================================================

  /**
   * Subscribe to posts state changes
   */
  Unsubscriber subscribeToPosts(std::function<void(const PostsState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.posts; }, callback);
  }

  /**
   * Subscribe to current feed changes
   */
  Unsubscriber subscribeToCurrentFeed(std::function<void(const FeedState &)> callback) {
    return store_.subscribeToSelection(
        [](const AppState &s) {
          auto *feed = s.posts.getCurrentFeed();
          return feed ? *feed : FeedState();
        },
        callback);
  }

  /**
   * Subscribe to feed loading state
   */
  Unsubscriber subscribeToFeedLoading(std::function<void(bool)> callback) {
    return store_.subscribeToSelection(
        [](const AppState &s) {
          auto *feed = s.posts.getCurrentFeed();
          return feed ? feed->isLoading : false;
        },
        callback);
  }

  // ============================================================================
  // User State Subscriptions
  // ============================================================================

  /**
   * Subscribe to user profile state changes
   */
  Unsubscriber subscribeToUser(std::function<void(const UserState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.user; }, callback);
  }

  /**
   * Subscribe to user profile picture changes
   */
  Unsubscriber subscribeToUserProfilePicture(std::function<void(const juce::String &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.user.profilePictureUrl; }, callback);
  }

  // ============================================================================
  // Chat State Subscriptions
  // ============================================================================

  /**
   * Subscribe to chat state changes
   */
  Unsubscriber subscribeToChat(std::function<void(const ChatState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.chat; }, callback);
  }

  /**
   * Subscribe to conversations list
   */
  Unsubscriber
  subscribeToConversations(std::function<void(const std::vector<std::shared_ptr<Conversation>> &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.chat.conversations; }, callback);
  }

  // ============================================================================
  // Notification State Subscriptions
  // ============================================================================

  /**
   * Subscribe to notification state changes
   */
  Unsubscriber subscribeToNotifications(std::function<void(const NotificationState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.notifications; }, callback);
  }

  /**
   * Subscribe to unread count changes
   */
  Unsubscriber subscribeToUnreadCount(std::function<void(int)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.notifications.unreadCount; }, callback);
  }

  // ============================================================================
  // Search State Subscriptions
  // ============================================================================

  /**
   * Subscribe to search state changes
   */
  Unsubscriber subscribeToSearch(std::function<void(const SearchState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.search; }, callback);
  }

  /**
   * Subscribe to search query changes
   */
  Unsubscriber subscribeToSearchQuery(std::function<void(const juce::String &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.search.results.searchQuery; }, callback);
  }

  // ============================================================================
  // Discovery State Subscriptions
  // ============================================================================

  /**
   * Subscribe to discovery state changes
   */
  Unsubscriber subscribeToDiscovery(std::function<void(const DiscoveryState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.discovery; }, callback);
  }

  /**
   * Subscribe to trending users changes
   */
  Unsubscriber
  subscribeToTrendingUsers(std::function<void(const std::vector<std::shared_ptr<const User>> &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.discovery.trendingUsers; }, callback);
  }

  // ============================================================================
  // Presence State Subscriptions
  // ============================================================================

  /**
   * Subscribe to presence state changes
   */
  Unsubscriber subscribeToPresence(std::function<void(const PresenceState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.presence; }, callback);
  }

  /**
   * Subscribe to connection status
   */
  Unsubscriber subscribeToConnectionStatus(std::function<void(bool)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.presence.isConnected; }, callback);
  }

  // ============================================================================
  // Stories State Subscriptions
  // ============================================================================

  /**
   * Subscribe to stories state changes
   */
  Unsubscriber subscribeToStories(std::function<void(const StoriesState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.stories; }, callback);
  }

  // ============================================================================
  // Upload State Subscriptions
  // ============================================================================

  /**
   * Subscribe to upload state changes
   */
  Unsubscriber subscribeToUpload(std::function<void(const UploadState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.uploads; }, callback);
  }

  /**
   * Subscribe to upload progress
   */
  Unsubscriber subscribeToUploadProgress(std::function<void(int)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.uploads.progress; }, callback);
  }

  // ============================================================================
  // Playlist State Subscriptions
  // ============================================================================

  /**
   * Subscribe to playlist state changes
   */
  Unsubscriber subscribeToPlaylists(std::function<void(const PlaylistState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.playlists; }, callback);
  }

  // ============================================================================
  // Challenge State Subscriptions
  // ============================================================================

  /**
   * Subscribe to challenge state changes
   */
  Unsubscriber subscribeToChallenges(std::function<void(const ChallengeState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.challenges; }, callback);
  }

  // ============================================================================
  // Sound State Subscriptions
  // ============================================================================

  /**
   * Subscribe to sound state changes
   */
  Unsubscriber subscribeToSounds(std::function<void(const SoundState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.sounds; }, callback);
  }

  // ============================================================================
  // Draft State Subscriptions
  // ============================================================================

  /**
   * Subscribe to draft state changes
   */
  Unsubscriber subscribeToDrafts(std::function<void(const DraftState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.drafts; }, callback);
  }

  // ============================================================================
  // Comments State Subscriptions
  // ============================================================================

  /**
   * Subscribe to comments state changes
   */
  Unsubscriber subscribeToComments(std::function<void(const CommentsState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.comments; }, callback);
  }

  // ============================================================================
  // Followers State Subscriptions
  // ============================================================================

  /**
   * Subscribe to followers state changes
   */
  Unsubscriber subscribeToFollowers(std::function<void(const FollowersState &)> callback) {
    return store_.subscribeToSelection([](const AppState &s) { return s.followers; }, callback);
  }

  // ============================================================================
  // Optimistic Updates
  // ============================================================================

  /**
   * Perform optimistic update with rollback on error
   */
  template <typename AsyncOp>
  void optimisticUpdate(std::function<void(AppState &)> optimisticUpdate, AsyncOp asyncOperation,
                        std::function<void(const juce::String &)> onError = nullptr) {
    store_.optimisticUpdate(optimisticUpdate, asyncOperation, onError);
  }

  // ============================================================================
  // Utilities
  // ============================================================================

  /**
   * Reset all state to defaults (useful for logout)
   */
  void reset() {
    store_.setState(AppState());
  }

  /**
   * Get number of subscribers
   */
  size_t getSubscriberCount() const {
    return store_.getSubscriberCount();
  }

  /**
   * Check if store has active subscribers
   */
  bool hasSubscribers() const {
    return store_.hasSubscribers();
  }

private:
  ReactiveStore<AppState> store_;

  // Prevent copying
  RootStore(const RootStore &) = delete;
  RootStore &operator=(const RootStore &) = delete;
  RootStore(RootStore &&) = delete;
  RootStore &operator=(RootStore &&) = delete;
};

} // namespace Stores
} // namespace Sidechain
