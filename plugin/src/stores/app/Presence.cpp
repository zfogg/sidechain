#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::setPresenceStatus(PresenceStatus status) {
  updateState([status](AppState &state) {
    state.presence.currentUserStatus = status;
    state.presence.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
    Util::logInfo("AppStore", "Presence status set to: " + juce::String((int)status));
  });

  notifyObservers();
}

void AppStore::setPresenceStatusMessage(const juce::String &message) {
  Util::logInfo("AppStore", "Setting presence status message: " + message);
  // TODO: Send status message to server via presence endpoint
}

void AppStore::recordUserActivity() {
  updateState([](AppState &state) {
    state.presence.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
    // Set to Online if was Away/Offline
    if (state.presence.currentUserStatus != PresenceStatus::Online) {
      state.presence.currentUserStatus = PresenceStatus::Online;
    }
  });

  notifyObservers();
}

void AppStore::connectPresence() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot connect presence - network client not set");
    return;
  }

  updateState([](AppState &state) { state.presence.isUpdatingPresence = true; });

  Util::logInfo("AppStore", "Connecting to presence service");

  // TODO: Establish WebSocket connection to presence service
  // For now, just mark as connected
  updateState([](AppState &state) {
    state.presence.isUpdatingPresence = false;
    state.presence.isConnected = true;
    state.presence.currentUserStatus = PresenceStatus::Online;
    state.presence.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
  });

  notifyObservers();
}

void AppStore::disconnectPresence() {
  updateState([](AppState &state) {
    state.presence.isUpdatingPresence = false;
    state.presence.isConnected = false;
    state.presence.isReconnecting = false;
    state.presence.currentUserStatus = PresenceStatus::Offline;
    Util::logInfo("AppStore", "Disconnected from presence service");
  });

  notifyObservers();
}

void AppStore::handlePresenceUpdate(const juce::String &userId, const juce::var &presenceData) {
  if (!presenceData.isObject()) {
    return;
  }

  updateState([userId, presenceData](AppState &state) {
    PresenceInfo info;
    info.userId = userId;

    // Parse presence status from data
    juce::String status = presenceData.getProperty("status", "unknown").toString();
    if (status == "online") {
      info.status = PresenceStatus::Online;
    } else if (status == "away") {
      info.status = PresenceStatus::Away;
    } else if (status == "offline") {
      info.status = PresenceStatus::Offline;
    } else if (status == "dnd") {
      info.status = PresenceStatus::DoNotDisturb;
    } else {
      info.status = PresenceStatus::Unknown;
    }

    info.lastSeen = static_cast<int64_t>(static_cast<double>(presenceData.getProperty("last_seen", 0)));
    info.statusMessage = presenceData.getProperty("status_message", "").toString();

    state.presence.userPresence[userId] = info;
  });

  notifyObservers();
}

} // namespace Stores
} // namespace Sidechain
