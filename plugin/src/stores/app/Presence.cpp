#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::setPresenceStatus(PresenceStatus status) {
  sliceManager.getPresenceSlice()->dispatch([status](PresenceState &state) {
    state.currentUserStatus = status;
    state.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
    Util::logInfo("AppStore", "Presence status set to: " + juce::String((int)status));
  });
}

void AppStore::setPresenceStatusMessage(const juce::String &message) {
  Util::logInfo("AppStore", "Setting presence status message: " + message);
  // TODO: Send status message to server via presence endpoint
}

void AppStore::recordUserActivity() {
  sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) {
    state.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
    // Set to Online if was Away/Offline
    if (state.currentUserStatus != PresenceStatus::Online) {
      state.currentUserStatus = PresenceStatus::Online;
    }
  });
}

void AppStore::connectPresence() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot connect presence - network client not set");
    return;
  }

  sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) { state.isUpdatingPresence = true; });

  Util::logInfo("AppStore", "Connecting to presence service");

  // TODO: Establish WebSocket connection to presence service
  // For now, just mark as connected
  sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) {
    state.isUpdatingPresence = false;
    state.isConnected = true;
    state.currentUserStatus = PresenceStatus::Online;
    state.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
  });
}

void AppStore::disconnectPresence() {
  sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) {
    state.isUpdatingPresence = false;
    state.isConnected = false;
    state.isReconnecting = false;
    state.currentUserStatus = PresenceStatus::Offline;
    Util::logInfo("AppStore", "Disconnected from presence service");
  });
}

void AppStore::handlePresenceUpdate(const juce::String &userId, const juce::var &presenceData) {
  if (!presenceData.isObject()) {
    return;
  }

  sliceManager.getPresenceSlice()->dispatch([userId, presenceData](PresenceState &state) {
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

    state.userPresence[userId] = info;
  });
}

} // namespace Stores
} // namespace Sidechain
