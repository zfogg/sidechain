#pragma once

#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>
#include <map>

namespace Sidechain {
namespace Stores {

/**
 * PresenceStatus - User online/offline status
 */
enum class PresenceStatus {
  Unknown,     // Status unknown
  Online,      // User is online
  Away,        // User is away (idle)
  Offline,     // User is offline
  DoNotDisturb // User is in DND mode
};

/**
 * PresenceInfo - Information about a user's presence
 */
struct PresenceInfo {
  juce::String userId;
  PresenceStatus status = PresenceStatus::Unknown;
  int64_t lastSeen = 0;       // Last activity timestamp
  juce::String statusMessage; // Custom status message
  int idleTime = 0;           // Seconds idle

  bool operator==(const PresenceInfo &other) const {
    return userId == other.userId && status == other.status && lastSeen == other.lastSeen;
  }
};

/**
 * PresenceState - Immutable presence tracking state
 */
struct PresenceState {
  // Current user's presence
  PresenceStatus currentUserStatus = PresenceStatus::Offline;
  int64_t currentUserLastActivity = 0;
  bool isUpdatingPresence = false;

  // Connection status
  bool isConnected = false;
  bool isReconnecting = false;

  // Other users' presence (cached)
  std::map<juce::String, PresenceInfo> userPresence;

  // Error handling
  juce::String error;

  bool operator==(const PresenceState &other) const {
    return currentUserStatus == other.currentUserStatus && isConnected == other.isConnected;
  }
};

/**
 * PresenceStore - Reactive store for user presence/online status tracking
 *
 * Handles:
 * - Current user's online/away/offline status
 * - Other users' presence (via WebSocket)
 * - Idle time detection
 * - Connection status
 * - Presence updates
 *
 * Usage:
 *   auto& presenceStore = PresenceStore::getInstance();
 *   presenceStore.setNetworkClient(networkClient);
 *
 *   auto unsubscribe = presenceStore.subscribe([](const PresenceState& state) {
 *       if (state.isConnected) {
 *           showConnectedIndicator();
 *       } else {
 *           showDisconnectedIndicator();
 *       }
 *
 *       // Show other users' presence
 *       for (const auto& [userId, presence] : state.userPresence) {
 *           updateUserStatus(userId, presence.status);
 *       }
 *   });
 *
 *   // Set your status
 *   presenceStore.setStatus(PresenceStatus::Online);
 */
class PresenceStore : public Store<PresenceState> {
public:
  /**
   * Get singleton instance
   */
  static PresenceStore &getInstance() {
    static PresenceStore instance;
    return instance;
  }

  /**
   * Set the network client for API calls
   */
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  //==============================================================================
  // Presence Management

  /**
   * Set current user's status
   */
  void setStatus(PresenceStatus status);

  /**
   * Set custom status message
   */
  void setStatusMessage(const juce::String &message);

  /**
   * Update last activity (call on user interaction)
   */
  void recordActivity();

  /**
   * Get current user's presence status
   */
  PresenceStatus getCurrentStatus() const;

  /**
   * Get another user's presence
   */
  const PresenceInfo *getUserPresence(const juce::String &userId) const;

  /**
   * Subscribe to specific user's presence changes
   */
  std::function<void()> subscribeToUserPresence(const juce::String &userId,
                                                std::function<void(const PresenceInfo &)> callback);

  //==============================================================================
  // Connection Management

  /**
   * Connect to presence WebSocket
   */
  void connect();

  /**
   * Disconnect from presence WebSocket
   */
  void disconnect();

  /**
   * Reconnect if disconnected
   */
  void reconnect();

  /**
   * Handle presence update from WebSocket
   */
  void handlePresenceUpdate(const juce::String &userId, const juce::var &presenceData);

private:
  PresenceStore() : Store<PresenceState>() {}

  NetworkClient *networkClient = nullptr;
  std::map<juce::String, std::vector<std::function<void(const PresenceInfo &)>>> presenceCallbacks;
};

} // namespace Stores
} // namespace Sidechain
