#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::setPresenceStatusMessage(const juce::String &message) {
  if (!streamChatClient) {
    Util::logError("AppStore", "Cannot update presence status message - StreamChatClient not initialized");
    return;
  }

  auto currentUser = stateManager.user->getState();

  // Build user data to upsert to GetStream.io
  nlohmann::json userData = {{"id", currentUser.userId.toStdString()},
                             {"name", currentUser.username.toStdString()},
                             {"status_message", message.toStdString()}};

  // Upsert user to GetStream.io
  streamChatClient->upsertUser(userData, [](Outcome<nlohmann::json> result) {
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

  auto presenceState = stateManager.presence;
  PresenceState newState = presenceState->getState();
  newState.isUpdatingPresence = true;
  presenceState->setState(newState);

  Util::logInfo("AppStore", "Connecting to GetStream.io presence");

  // GetStream.io automatically marks user as online when connected
  // The streamChatClient should be connected as part of normal chat initialization
  streamChatClient->subscribeToPresenceEvents([this](const nlohmann::json &event) {
    juce::String userId;
    if (event.contains("user") && event["user"].is_object()) {
      const auto &user = event["user"];
      if (user.contains("id") && user["id"].is_string()) {
        userId = juce::String(user["id"].get<std::string>());
      }
    }
    if (userId.isNotEmpty()) {
      handlePresenceUpdate(userId, event);
    }
  });

  PresenceState connectedState = presenceState->getState();
  connectedState.isUpdatingPresence = false;
  connectedState.isConnected = true;
  connectedState.currentUserStatus = PresenceStatus::Online;
  connectedState.currentUserLastActivity = juce::Time::getCurrentTime().toMilliseconds();
  presenceState->setState(connectedState);
  Util::logInfo("AppStore", "Connected to GetStream.io presence");
}

void AppStore::handlePresenceUpdate(const juce::String &userId, const nlohmann::json &presenceData) {
  // Extract presence information from the data
  bool isOnline = presenceData.value("online", false);

  Util::logInfo("AppStore", "Presence update for " + userId + ": " + (isOnline ? "online" : "offline"));

  // Update local state with user presence information
  // This is how followers see when users come online/offline
  PresenceState newState = stateManager.presence->getState();
  PresenceInfo info;
  info.userId = userId;
  info.status = isOnline ? PresenceStatus::Online : PresenceStatus::Offline;
  info.lastSeen = juce::Time::getCurrentTime().toMilliseconds();
  newState.userPresence[userId] = info;
  stateManager.presence->setState(newState);
}

void AppStore::disconnectPresence() {
  if (!streamChatClient)
    return;

  // Tell GetStream.io we're disconnecting
  // User will be marked offline after ~30 seconds of no activity
  streamChatClient->disconnect();

  auto presenceState = stateManager.presence;
  PresenceState disconnectState = presenceState->getState();
  disconnectState.isConnected = false;
  disconnectState.currentUserStatus = PresenceStatus::Offline;
  presenceState->setState(disconnectState);

  Util::logInfo("AppStore", "Disconnected from GetStream.io presence");
}

} // namespace Stores
} // namespace Sidechain
