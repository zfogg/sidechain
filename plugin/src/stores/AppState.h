#pragma once

#include "../util/rx/StateSubject.h"
#include "app/AppState.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * StateManager - Centralized reactive state management
 *
 * All application state is held in Rx::State<T> (StateSubject) instances that provide:
 * - Thread-safe read/write
 * - Reactive subscriptions with selector support
 * - Composable with RxCpp observables via asObservable()
 *
 * Usage:
 *   auto& state = StateManager::getInstance();
 *
 *   // Read current state
 *   auto authState = state.auth->getValue();
 *
 *   // Update state
 *   AuthState newAuth = state.auth->getValue();
 *   newAuth.isLoggedIn = true;
 *   state.auth->next(newAuth);
 *
 *   // Subscribe to changes
 *   auto unsub = state.auth->subscribe([](const AuthState& s) {
 *       updateUI(s);
 *   });
 *
 *   // Subscribe to derived value (selector pattern)
 *   auto unsub2 = state.auth->select(
 *       [](const AuthState& s) { return s.isLoggedIn; },
 *       [](bool loggedIn) { showLoginButton(!loggedIn); }
 *   );
 *
 *   // Cleanup
 *   unsub();
 *   unsub2();
 */
class StateManager {
public:
  static StateManager &getInstance() {
    static StateManager instance;
    return instance;
  }

  // All state subjects - direct public access
  Rx::State<AuthState> auth;
  Rx::State<PostsState> posts;
  Rx::State<UserState> user;
  Rx::State<ChatState> chat;
  Rx::State<DraftState> draft;
  Rx::State<ChallengeState> challenge;
  Rx::State<StoriesState> stories;
  Rx::State<UploadState> uploads;
  Rx::State<NotificationState> notifications;
  Rx::State<CommentsState> comments;
  Rx::State<SearchState> search;
  Rx::State<DiscoveryState> discovery;
  Rx::State<FollowersState> followers;
  Rx::State<PlaylistState> playlists;
  Rx::State<SoundState> sounds;
  Rx::State<PresenceState> presence;

  /**
   * Reset all state to initial values
   * Useful for logout or app reset
   */
  void resetAll() {
    auth->next(AuthState());
    posts->next(PostsState());
    user->next(UserState());
    chat->next(ChatState());
    draft->next(DraftState());
    challenge->next(ChallengeState());
    stories->next(StoriesState());
    uploads->next(UploadState());
    notifications->next(NotificationState());
    comments->next(CommentsState());
    search->next(SearchState());
    discovery->next(DiscoveryState());
    followers->next(FollowersState());
    playlists->next(PlaylistState());
    sounds->next(SoundState());
    presence->next(PresenceState());
  }

private:
  StateManager()
      : auth(Rx::makeState<AuthState>()), posts(Rx::makeState<PostsState>()), user(Rx::makeState<UserState>()),
        chat(Rx::makeState<ChatState>()), draft(Rx::makeState<DraftState>()),
        challenge(Rx::makeState<ChallengeState>()), stories(Rx::makeState<StoriesState>()),
        uploads(Rx::makeState<UploadState>()), notifications(Rx::makeState<NotificationState>()),
        comments(Rx::makeState<CommentsState>()), search(Rx::makeState<SearchState>()),
        discovery(Rx::makeState<DiscoveryState>()), followers(Rx::makeState<FollowersState>()),
        playlists(Rx::makeState<PlaylistState>()), sounds(Rx::makeState<SoundState>()),
        presence(Rx::makeState<PresenceState>()) {}

  // Prevent copying
  StateManager(const StateManager &) = delete;
  StateManager &operator=(const StateManager &) = delete;
};

} // namespace Stores
} // namespace Sidechain
