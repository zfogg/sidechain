#include "PresenceStore.h"

namespace Sidechain {
namespace Stores {

void PresenceStore::setStatus(PresenceStatus status) {
  updateState([status](PresenceState &state) { state.currentUserStatus = status; });

  if (!networkClient) {
    updateState([](PresenceState &state) { state.error = "Network client not initialized"; });
    return;
  }

  // TODO: Send presence update to server
}

void PresenceStore::setStatusMessage(const juce::String &message) {
  // TODO: Send status message to server
}

void PresenceStore::recordActivity() {
  updateState(
      [](PresenceState &state) { state.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds(); });
}

PresenceStatus PresenceStore::getCurrentStatus() const {
  return getState().currentUserStatus;
}

const PresenceInfo *PresenceStore::getUserPresence(const juce::String &userId) const {
  auto state = getState();
  auto it = state.userPresence.find(userId);
  if (it != state.userPresence.end()) {
    return &it->second;
  }
  return nullptr;
}

std::function<void()> PresenceStore::subscribeToUserPresence(const juce::String &userId,
                                                             std::function<void(const PresenceInfo &)> callback) {
  presenceCallbacks[userId].push_back(callback);

  // Return unsubscriber
  return [this, userId, callback]() {
    auto &callbacks = presenceCallbacks[userId];
    auto it = std::find_if(callbacks.begin(), callbacks.end(), [callback](const auto &cb) {
      auto cbTarget = cb.template target<void(const PresenceInfo &)>();
      auto callbackTarget = callback.template target<void(const PresenceInfo &)>();
      return cbTarget != nullptr && callbackTarget != nullptr && cbTarget == callbackTarget;
    });
    if (it != callbacks.end()) {
      callbacks.erase(it);
    }
  };
}

void PresenceStore::connect() {
  if (!networkClient) {
    updateState([](PresenceState &state) { state.error = "Network client not initialized"; });
    return;
  }

  updateState([](PresenceState &state) { state.isReconnecting = true; });

  // TODO: Connect to presence WebSocket
  updateState([](PresenceState &state) {
    state.isReconnecting = false;
    state.isConnected = true;
  });
}

void PresenceStore::disconnect() {
  updateState([](PresenceState &state) { state.isConnected = false; });

  // TODO: Disconnect from presence WebSocket
}

void PresenceStore::reconnect() {
  disconnect();
  connect();
}

void PresenceStore::handlePresenceUpdate(const juce::String &userId, const juce::var &presenceData) {
  PresenceStatus status = PresenceStatus::Unknown;

  // Parse status from presence data
  juce::String statusStr = presenceData.getProperty("status", "unknown").toString();
  if (statusStr == "online") {
    status = PresenceStatus::Online;
  } else if (statusStr == "away") {
    status = PresenceStatus::Away;
  } else if (statusStr == "offline") {
    status = PresenceStatus::Offline;
  }

  PresenceInfo presenceInfo;
  presenceInfo.userId = userId;
  presenceInfo.status = status;
  presenceInfo.lastSeen = juce::Time::getCurrentTime().toMilliseconds();
  presenceInfo.statusMessage = presenceData.getProperty("statusMessage", "").toString();

  // Update state
  updateState([userId, presenceInfo](PresenceState &state) { state.userPresence[userId] = presenceInfo; });

  // Notify presence callbacks
  if (presenceCallbacks.count(userId) > 0) {
    for (const auto &callback : presenceCallbacks[userId]) {
      callback(presenceInfo);
    }
  }
}

} // namespace Stores
} // namespace Sidechain
