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
  auto currentState = getState();
  for (const auto &[id, callback] : feedSubscribers) {
    try {
      callback(currentState.posts);
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Feed subscriber threw exception", e.what());
    }
  }
}

void AppStore::updateUserState(std::function<void(UserState &)> updater) {
  updateState([updater](AppState &state) { updater(state.user); });

  // Notify user subscribers
  auto currentState = getState();
  for (const auto &[id, callback] : userSubscribers) {
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
  auto currentState = getState();
  for (const auto &[id, callback] : chatSubscribers) {
    try {
      callback(currentState.chat);
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Chat subscriber threw exception", e.what());
    }
  }
}

} // namespace Stores
} // namespace Sidechain
