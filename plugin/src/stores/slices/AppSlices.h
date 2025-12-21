#pragma once

#include "../StateSlice.h"
#include "../app/AppState.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {
namespace Slices {

/**
 * AppSlices - Concrete slice implementations for application state domains
 *
 * Each slice manages a specific domain of application state:
 * - AuthSlice: Login, tokens, user identity
 * - FeedSlice: Posts, feeds, feed navigation
 * - UserSlice: Profile, settings, preferences
 * - ChatSlice: Messages, conversations
 * - NotificationSlice: Notifications, alerts
 * - SearchSlice: Search results, queries
 * - UploadSlice: Upload progress, status
 *
 * Entity Caching:
 * - EntityStore: Centralized cache for all normalized entities (posts, users, etc.)
 *   All entity caching and subscriptions are handled directly through EntityStore.
 *   Entity updates flow through the Redux state system for reactive propagation.
 *
 * Slices can be:
 * 1. Used independently for modular state management
 * 2. Composed into AppStore for monolithic access
 * 3. Tested independently without full app context
 * 4. Eager-initialized to ensure all slices are ready at startup
 *
 * Usage:
 *   // Create a slice
 *   auto authSlice = std::make_shared<AuthSlice>();
 *
 *   // Subscribe to slice changes
 *   authSlice->subscribe([this](const AuthState& auth) {
 *       if (auth.isLoggedIn) {
 *           enableUIControls();
 *       }
 *   });
 *
 *   // Update state (immutable - create new instances)
 *   AuthState newAuth = authSlice->getState();
 *   newAuth.isLoggedIn = true;
 *   newAuth.userId = "user_123";
 *   authSlice->setState(newAuth);  // Atomic replace - all subscribers notified
 *
 *   // Subscribe to derived state (selector pattern)
 *   authSlice->subscribeToSelection(
 *       [](const AuthState& auth) { return auth.isLoggedIn; },
 *       [this](const bool& isLoggedIn) { updateLoginUI(isLoggedIn); }
 *   );
 *
 * Entity-level caching (using EntityStore directly):
 *   auto& entityStore = EntityStore::getInstance();
 *
 *   // Cache entities from network responses
 *   entityStore.posts().setMany(postsFromAPI);
 *
 *   // Subscribe to individual entity changes
 *   auto unsub = entityStore.posts().subscribe(postId, [](const auto& post) {
 *       updateUI(*post);
 *   });
 */

// ==============================================================================
// Auth Slice
// ==============================================================================

using AuthSlice = ImmutableSlice<AuthState>;

// ==============================================================================
// Feed/Posts Slice
// ==============================================================================

using PostsSlice = ImmutableSlice<PostsState>;

// ==============================================================================
// User/Profile Slice
// ==============================================================================

using UserSlice = ImmutableSlice<UserState>;

// ==============================================================================
// Chat Slice
// ==============================================================================

using ChatSlice = ImmutableSlice<ChatState>;

// ==============================================================================
// Draft Slice
// ==============================================================================

using DraftSlice = ImmutableSlice<DraftState>;

// ==============================================================================
// Challenge Slice
// ==============================================================================

using ChallengeSlice = ImmutableSlice<ChallengeState>;

// ==============================================================================
// Stories Slice
// ==============================================================================

using StoriesSlice = ImmutableSlice<StoriesState>;

// ==============================================================================
// Upload Slice
// ==============================================================================

using UploadSlice = ImmutableSlice<UploadState>;

// ==============================================================================
// Notification Slice
// ==============================================================================

using NotificationSlice = ImmutableSlice<NotificationState>;

// ==============================================================================
// Comments Slice
// ==============================================================================

using CommentsSlice = ImmutableSlice<CommentsState>;

// ==============================================================================
// Search Slice
// ==============================================================================

using SearchSlice = ImmutableSlice<SearchState>;

// ==============================================================================
// Discovery Slice
// ==============================================================================

using DiscoverySlice = ImmutableSlice<DiscoveryState>;

// ==============================================================================
// Followers Slice
// ==============================================================================

using FollowersSlice = ImmutableSlice<FollowersState>;

// ==============================================================================
// Playlist Slice
// ==============================================================================

using PlaylistSlice = ImmutableSlice<PlaylistState>;

// ==============================================================================
// Sound Slice
// ==============================================================================

using SoundSlice = ImmutableSlice<SoundState>;

// ==============================================================================
// Presence Slice
// ==============================================================================

using PresenceSlice = ImmutableSlice<PresenceState>;

/**
 * AppSliceManager - Centralized accessor for all application slices
 *
 * Provides singleton access to all domain-specific slices with:
 * - Eager initialization on construction (all slices ready at startup)
 * - Direct public member access (no lazy getter boilerplate)
 * - Unified state reset for logout/app reset
 *
 * Usage:
 *   auto& manager = AppSliceManager::getInstance();
 *
 *   // Direct access to slices (no getter boilerplate)
 *   manager.auth->setState(newAuthState);
 *   auto currentFeed = manager.posts->getState();
 *
 *   // Subscribe to slice changes
 *   manager.auth->subscribe([](const AuthState& state) { updateUI(); });
 *
 *   // Reset all slices on logout
 *   manager.resetAllSlices();
 */
class AppSliceManager {
public:
  static AppSliceManager &getInstance() {
    static AppSliceManager instance;
    return instance;
  }

  // Direct public access to slices (eagerly initialized)
  std::shared_ptr<AuthSlice> auth;
  std::shared_ptr<PostsSlice> posts;
  std::shared_ptr<UserSlice> user;
  std::shared_ptr<ChatSlice> chat;
  std::shared_ptr<DraftSlice> draft;
  std::shared_ptr<ChallengeSlice> challenge;
  std::shared_ptr<StoriesSlice> stories;
  std::shared_ptr<UploadSlice> uploads;
  std::shared_ptr<NotificationSlice> notifications;
  std::shared_ptr<CommentsSlice> comments;
  std::shared_ptr<SearchSlice> search;
  std::shared_ptr<DiscoverySlice> discovery;
  std::shared_ptr<FollowersSlice> followers;
  std::shared_ptr<PlaylistSlice> playlists;
  std::shared_ptr<SoundSlice> sounds;
  std::shared_ptr<PresenceSlice> presence;

  /**
   * Reset all slices to initial state (immutable pattern)
   * Creates new state instances for each slice
   * Useful for logout or app reset
   */
  void resetAllSlices() {
    auth->setState(AuthState());
    posts->setState(PostsState());
    user->setState(UserState());
    chat->setState(ChatState());
    draft->setState(DraftState());
    challenge->setState(ChallengeState());
    stories->setState(StoriesState());
    uploads->setState(UploadState());
    notifications->setState(NotificationState());
    comments->setState(CommentsState());
    search->setState(SearchState());
    discovery->setState(DiscoveryState());
    followers->setState(FollowersState());
    playlists->setState(PlaylistState());
    sounds->setState(SoundState());
    presence->setState(PresenceState());
  }

private:
  AppSliceManager()
      : auth(std::make_shared<AuthSlice>(AuthState())), posts(std::make_shared<PostsSlice>(PostsState())),
        user(std::make_shared<UserSlice>(UserState())), chat(std::make_shared<ChatSlice>(ChatState())),
        draft(std::make_shared<DraftSlice>(DraftState())),
        challenge(std::make_shared<ChallengeSlice>(ChallengeState())),
        stories(std::make_shared<StoriesSlice>(StoriesState())), uploads(std::make_shared<UploadSlice>(UploadState())),
        notifications(std::make_shared<NotificationSlice>(NotificationState())),
        comments(std::make_shared<CommentsSlice>(CommentsState())),
        search(std::make_shared<SearchSlice>(SearchState())),
        discovery(std::make_shared<DiscoverySlice>(DiscoveryState())),
        followers(std::make_shared<FollowersSlice>(FollowersState())),
        playlists(std::make_shared<PlaylistSlice>(PlaylistState())), sounds(std::make_shared<SoundSlice>(SoundState())),
        presence(std::make_shared<PresenceSlice>(PresenceState())) {}

  // Prevent copying
  AppSliceManager(const AppSliceManager &) = delete;
  AppSliceManager &operator=(const AppSliceManager &) = delete;
};

} // namespace Slices
} // namespace Stores
} // namespace Sidechain
