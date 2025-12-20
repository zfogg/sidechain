#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::setPresenceStatusMessage(const juce::String &message) {
  if (!streamChatClient) {
    Util::logError("AppStore", "Cannot update presence status message - StreamChatClient not initialized");
    return;
  }

  auto currentUser = sliceManager.getUserSlice()->getState();

  // Build user data to upsert to GetStream.io
  auto userData = std::make_shared<juce::DynamicObject>();
  userData->setProperty("id", currentUser.userId);
  userData->setProperty("name", currentUser.username);
  userData->setProperty("status_message", message);

  // Upsert user to GetStream.io
  streamChatClient->upsertUser(userData, [this, message](Outcome<juce::var> result) {
    if (result.isError()) {
      Util::logError("AppStore", "Failed to update presence status message: " + result.getError());
      return;
    }

    Util::logInfo("AppStore", "Presence status message updated successfully");
  });
}

void AppStore::connectPresence() {
  if (!streamChatClient) {
    Util::logError("AppStore", "Cannot connect presence - StreamChatClient not initialized");
    return;
  }

  sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) { state.isUpdatingPresence = true; });

  Util::logInfo("AppStore", "Connecting to GetStream.io presence");

  // GetStream.io automatically marks user as online when connected
  // The streamChatClient should be connected as part of normal chat initialization
  streamChatClient->subscribeToPresenceEvents([this](const juce::var &event) {
    auto user = event.getProperty("user", juce::var());
    auto userId = user.getProperty("id", juce::var()).toString();
    if (userId.isNotEmpty()) {
      handlePresenceUpdate(userId, event);
    }
  });

  sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) {
    state.isUpdatingPresence = false;
    state.isConnected = true;
    state.currentUserStatus = PresenceStatus::Online;
    state.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
    Util::logInfo("AppStore", "Connected to GetStream.io presence");
  });
}

void AppStore::handlePresenceUpdate(const juce::String &userId, const juce::var &presenceData) {
  // Extract presence information from the data
  bool isOnline = presenceData.getProperty("online", false);

  Util::logInfo("AppStore", "Presence update for " + userId + ": " + (isOnline ? "online" : "offline"));

  // Update local state with user presence information
  // This is how followers see when users come online/offline
  sliceManager.getPresenceSlice()->dispatch([userId, isOnline](PresenceState &state) {
    PresenceInfo info;
    info.userId = userId;
    info.status = isOnline ? PresenceStatus::Online : PresenceStatus::Offline;
    info.lastSeen = juce::Time::getCurrentTime().toMilliseconds();
    state.userPresence[userId] = info;
  });
}

void AppStore::disconnectPresence() {
  if (!streamChatClient)
    return;

  // Tell GetStream.io we're disconnecting
  // User will be marked offline after ~30 seconds of no activity
  streamChatClient->disconnect();

  sliceManager.getPresenceSlice()->dispatch([](PresenceState &state) {
    state.isConnected = false;
    state.currentUserStatus = PresenceStatus::Offline;
  });

  Util::logInfo("AppStore", "Disconnected from GetStream.io presence");
}

} // namespace Stores
} // namespace Sidechain
