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
 * Entity Bridge:
 * - EntitySlice: Bridges EntityStore (normalized data) with Redux slices
 *   See EntitySlice.h for entity-level caching and subscriptions
 *
 * Slices can be:
 * 1. Used independently for modular state management
 * 2. Composed into AppStore for monolithic access
 * 3. Tested independently without full app context
 * 4. Lazy-loaded to reduce startup overhead
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
 * Entity-level subscriptions (Phase 1):
 *   auto& entitySlice = EntitySlice::getInstance();
 *
 *   // Cache entities from network responses
 *   entitySlice.cachePosts(postsFromAPI);
 *
 *   // Subscribe to individual entity changes
 *   auto unsub = entitySlice.subscribeToPost(postId, [](const auto& post) {
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
 * AppSliceManager - Facade for managing all application slices
 *
 * Provides centralized access to all slices with:
 * - Lazy initialization (slices created on first access)
 * - Memoized selectors for derived state
 * - Unified dispatch for cross-slice operations
 * - Synchronized state across slices
 *
 * Usage:
 *   auto& manager = AppSliceManager::getInstance();
 *
 *   // Access individual slices
 *   auto& authSlice = manager.getAuthSlice();
 *   auto& feedSlice = manager.getFeedSlice();
 *
 *   // Subscribe to multiple slices for coordinated updates
 *   manager.onAuthChanged([](const AuthState& auth) { updateUI(); });
 *   manager.onFeedChanged([](const PostsState& feed) { updateFeed(); });
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

  // Getters for individual slices (lazy initialization)
  std::shared_ptr<AuthSlice> getAuthSlice() {
    if (!authSlice_) {
      authSlice_ = std::make_shared<AuthSlice>(AuthState());
    }
    return authSlice_;
  }

  std::shared_ptr<PostsSlice> getPostsSlice() {
    if (!postsSlice_) {
      postsSlice_ = std::make_shared<PostsSlice>(PostsState());
    }
    return postsSlice_;
  }

  std::shared_ptr<UserSlice> getUserSlice() {
    if (!userSlice_) {
      userSlice_ = std::make_shared<UserSlice>(UserState());
    }
    return userSlice_;
  }

  std::shared_ptr<ChatSlice> getChatSlice() {
    if (!chatSlice_) {
      chatSlice_ = std::make_shared<ChatSlice>(ChatState());
    }
    return chatSlice_;
  }

  std::shared_ptr<DraftSlice> getDraftSlice() {
    if (!draftSlice_) {
      draftSlice_ = std::make_shared<DraftSlice>(DraftState());
    }
    return draftSlice_;
  }

  std::shared_ptr<ChallengeSlice> getChallengeSlice() {
    if (!challengeSlice_) {
      challengeSlice_ = std::make_shared<ChallengeSlice>(ChallengeState());
    }
    return challengeSlice_;
  }

  std::shared_ptr<StoriesSlice> getStoriesSlice() {
    if (!storiesSlice_) {
      storiesSlice_ = std::make_shared<StoriesSlice>(StoriesState());
    }
    return storiesSlice_;
  }

  std::shared_ptr<UploadSlice> getUploadSlice() {
    if (!uploadSlice_) {
      uploadSlice_ = std::make_shared<UploadSlice>(UploadState());
    }
    return uploadSlice_;
  }

  std::shared_ptr<NotificationSlice> getNotificationSlice() {
    if (!notificationSlice_) {
      notificationSlice_ = std::make_shared<NotificationSlice>(NotificationState());
    }
    return notificationSlice_;
  }

  std::shared_ptr<CommentsSlice> getCommentsSlice() {
    if (!commentsSlice_) {
      commentsSlice_ = std::make_shared<CommentsSlice>(CommentsState());
    }
    return commentsSlice_;
  }

  std::shared_ptr<SearchSlice> getSearchSlice() {
    if (!searchSlice_) {
      searchSlice_ = std::make_shared<SearchSlice>(SearchState());
    }
    return searchSlice_;
  }

  std::shared_ptr<DiscoverySlice> getDiscoverySlice() {
    if (!discoverySlice_) {
      discoverySlice_ = std::make_shared<DiscoverySlice>(DiscoveryState());
    }
    return discoverySlice_;
  }

  std::shared_ptr<FollowersSlice> getFollowersSlice() {
    if (!followersSlice_) {
      followersSlice_ = std::make_shared<FollowersSlice>(FollowersState());
    }
    return followersSlice_;
  }

  std::shared_ptr<PlaylistSlice> getPlaylistSlice() {
    if (!playlistSlice_) {
      playlistSlice_ = std::make_shared<PlaylistSlice>(PlaylistState());
    }
    return playlistSlice_;
  }

  std::shared_ptr<SoundSlice> getSoundSlice() {
    if (!soundSlice_) {
      soundSlice_ = std::make_shared<SoundSlice>(SoundState());
    }
    return soundSlice_;
  }

  std::shared_ptr<PresenceSlice> getPresenceSlice() {
    if (!presenceSlice_) {
      presenceSlice_ = std::make_shared<PresenceSlice>(PresenceState());
    }
    return presenceSlice_;
  }

  /**
   * Reset all slices to initial state (immutable pattern)
   * Creates new state instances for each slice
   * Useful for logout or app reset
   */
  void resetAllSlices() {
    if (authSlice_)
      authSlice_->setState(AuthState());
    if (postsSlice_)
      postsSlice_->setState(PostsState());
    if (userSlice_)
      userSlice_->setState(UserState());
    if (chatSlice_)
      chatSlice_->setState(ChatState());
    if (draftSlice_)
      draftSlice_->setState(DraftState());
    if (challengeSlice_)
      challengeSlice_->setState(ChallengeState());
    if (storiesSlice_)
      storiesSlice_->setState(StoriesState());
    if (uploadSlice_)
      uploadSlice_->setState(UploadState());
    if (notificationSlice_)
      notificationSlice_->setState(NotificationState());
    if (commentsSlice_)
      commentsSlice_->setState(CommentsState());
    if (searchSlice_)
      searchSlice_->setState(SearchState());
    if (discoverySlice_)
      discoverySlice_->setState(DiscoveryState());
    if (followersSlice_)
      followersSlice_->setState(FollowersState());
    if (playlistSlice_)
      playlistSlice_->setState(PlaylistState());
    if (soundSlice_)
      soundSlice_->setState(SoundState());
    if (presenceSlice_)
      presenceSlice_->setState(PresenceState());
  }

  /**
   * Clear all slices (removes all data without resetting to defaults)
   */
  void clearAllSlices() {
    authSlice_ = nullptr;
    postsSlice_ = nullptr;
    userSlice_ = nullptr;
    chatSlice_ = nullptr;
    draftSlice_ = nullptr;
    challengeSlice_ = nullptr;
    storiesSlice_ = nullptr;
    uploadSlice_ = nullptr;
    notificationSlice_ = nullptr;
    commentsSlice_ = nullptr;
    searchSlice_ = nullptr;
    discoverySlice_ = nullptr;
    followersSlice_ = nullptr;
    playlistSlice_ = nullptr;
    soundSlice_ = nullptr;
    presenceSlice_ = nullptr;
  }

private:
  AppSliceManager() = default;

  // Slice instances (lazy-initialized)
  std::shared_ptr<AuthSlice> authSlice_;
  std::shared_ptr<PostsSlice> postsSlice_;
  std::shared_ptr<UserSlice> userSlice_;
  std::shared_ptr<ChatSlice> chatSlice_;
  std::shared_ptr<DraftSlice> draftSlice_;
  std::shared_ptr<ChallengeSlice> challengeSlice_;
  std::shared_ptr<StoriesSlice> storiesSlice_;
  std::shared_ptr<UploadSlice> uploadSlice_;
  std::shared_ptr<NotificationSlice> notificationSlice_;
  std::shared_ptr<CommentsSlice> commentsSlice_;
  std::shared_ptr<SearchSlice> searchSlice_;
  std::shared_ptr<DiscoverySlice> discoverySlice_;
  std::shared_ptr<FollowersSlice> followersSlice_;
  std::shared_ptr<PlaylistSlice> playlistSlice_;
  std::shared_ptr<SoundSlice> soundSlice_;
  std::shared_ptr<PresenceSlice> presenceSlice_;

  // Prevent copying
  AppSliceManager(const AppSliceManager &) = delete;
  AppSliceManager &operator=(const AppSliceManager &) = delete;
};

} // namespace Slices
} // namespace Stores
} // namespace Sidechain
