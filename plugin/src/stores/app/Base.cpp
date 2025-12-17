#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

AppStore::AppStore() : Store<AppState>() {
  Util::logInfo("AppStore", "Initialized unified reactive app store");
}

void AppStore::setNetworkClient(NetworkClient *client) {
  networkClient = client;
  Util::logInfo("AppStore", "Network client configured");
}

//==============================================================================
// State Slice Subscriptions

std::function<void()> AppStore::subscribeToAuth(std::function<void(const AuthState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  authSubscribers[subscriberId] = callback;

  // Call immediately with current auth state
  callback(getState().auth);

  // Return unsubscriber
  return [this, subscriberId]() { authSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToFeed(std::function<void(const PostsState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  feedSubscribers[subscriberId] = callback;

  // Call immediately with current feed state
  callback(getState().posts);

  // Return unsubscriber
  return [this, subscriberId]() { feedSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToUser(std::function<void(const UserState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  userSubscribers[subscriberId] = callback;

  // Call immediately with current user state
  callback(getState().user);

  // Return unsubscriber
  return [this, subscriberId]() { userSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToChat(std::function<void(const ChatState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  chatSubscribers[subscriberId] = callback;

  // Call immediately with current chat state
  callback(getState().chat);

  // Return unsubscriber
  return [this, subscriberId]() { chatSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToDrafts(std::function<void(const DraftState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  draftSubscribers[subscriberId] = callback;

  // Call immediately with current drafts state
  callback(getState().drafts);

  // Return unsubscriber
  return [this, subscriberId]() { draftSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToChallenges(std::function<void(const ChallengeState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  challengeSubscribers[subscriberId] = callback;

  // Call immediately with current challenges state
  callback(getState().challenges);

  // Return unsubscriber
  return [this, subscriberId]() { challengeSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToStories(std::function<void(const StoriesState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  storiesSubscribers[subscriberId] = callback;

  // Call immediately with current stories state
  callback(getState().stories);

  // Return unsubscriber
  return [this, subscriberId]() { storiesSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToUploads(std::function<void(const UploadState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  uploadSubscribers[subscriberId] = callback;

  // Call immediately with current upload state
  callback(getState().uploads);

  // Return unsubscriber
  return [this, subscriberId]() { uploadSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToNotifications(std::function<void(const NotificationState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  notificationSubscribers[subscriberId] = callback;

  // Call immediately with current notification state
  callback(getState().notifications);

  // Return unsubscriber
  return [this, subscriberId]() { notificationSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToSearch(std::function<void(const SearchState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  searchSubscribers[subscriberId] = callback;

  // Call immediately with current search state
  callback(getState().search);

  // Return unsubscriber
  return [this, subscriberId]() { searchSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToFollowers(std::function<void(const FollowersState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  followersSubscribers[subscriberId] = callback;

  // Call immediately with current followers state
  callback(getState().followers);

  // Return unsubscriber
  return [this, subscriberId]() { followersSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToPlaylists(std::function<void(const PlaylistState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  playlistSubscribers[subscriberId] = callback;

  // Call immediately with current playlist state
  callback(getState().playlists);

  // Return unsubscriber
  return [this, subscriberId]() { playlistSubscribers.erase(subscriberId); };
}

std::function<void()> AppStore::subscribeToSounds(std::function<void(const SoundState &)> callback) {
  if (!callback)
    return []() {};

  auto subscriberId = nextSliceSubscriberId++;
  soundSubscribers[subscriberId] = callback;

  // Call immediately with current sounds state
  callback(getState().sounds);

  // Return unsubscriber
  return [this, subscriberId]() { soundSubscribers.erase(subscriberId); };
}

//==============================================================================
// Helper Methods for State Updates

void AppStore::updateAuthState(std::function<void(AuthState &)> updater) {
  updateState([updater](AppState &state) { updater(state.auth); });

  // Notify auth subscribers
  auto currentState = getState();
  for (const auto &[id, callback] : authSubscribers) {
    try {
      callback(currentState.auth);
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Auth subscriber threw exception", e.what());
    }
  }
}

void AppStore::updateFeedState(std::function<void(PostsState &)> updater) {
  updateState([updater](AppState &state) { updater(state.posts); });

  // Notify feed subscribers
  // Copy subscribers map before iterating to avoid iterator corruption if callbacks modify it
  auto currentState = getState();
  auto subscribersCopy = feedSubscribers;
  Util::logDebug("AppStore", "updateFeedState: " + juce::String(subscribersCopy.size()) + " feed subscribers to notify");
  for (const auto &[id, callback] : subscribersCopy) {
    try {
      Util::logDebug("AppStore", "updateFeedState: Calling subscriber " + juce::String((int)id));
      callback(currentState.posts);
      Util::logDebug("AppStore", "updateFeedState: Subscriber " + juce::String((int)id) + " returned");
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Feed subscriber threw exception", e.what());
    }
  }
}

void AppStore::updateUserState(std::function<void(UserState &)> updater) {
  updateState([updater](AppState &state) { updater(state.user); });

  // Notify user subscribers
  // Copy subscribers map before iterating to avoid iterator corruption if callbacks modify it
  auto currentState = getState();
  auto subscribersCopy = userSubscribers;
  for (const auto &[id, callback] : subscribersCopy) {
    try {
      callback(currentState.user);
    } catch (const std::exception &e) {
      Util::logError("AppStore", "User subscriber threw exception", e.what());
    }
  }
}

void AppStore::updateChatState(std::function<void(ChatState &)> updater) {
  updateState([updater](AppState &state) { updater(state.chat); });

  // Notify chat subscribers
  // Copy subscribers map before iterating to avoid iterator corruption if callbacks modify it
  auto currentState = getState();
  auto subscribersCopy = chatSubscribers;
  for (const auto &[id, callback] : subscribersCopy) {
    try {
      callback(currentState.chat);
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Chat subscriber threw exception", e.what());
    }
  }
}

void AppStore::updateUploadState(std::function<void(UploadState &)> updater) {
  updateState([updater](AppState &state) { updater(state.uploads); });

  // Notify all observers of state change
  notifyObservers();
}

} // namespace Stores
} // namespace Sidechain
