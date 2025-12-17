#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

// Forward declaration
class WebSocketClient;

/**
 * PresenceManager - Manages user presence (online/in_studio status with DAW detection)
 *
 * Features:
 * - Detects DAW on initialization (Ableton Live, Logic Pro, FL Studio, etc.)
 * - Periodically reports presence status via WebSocket
 * - Handles online/offline transitions
 * - Persists DAW information for followers to see
 *
 * Usage:
 *   auto presence = std::make_unique<PresenceManager>(webSocketClient.get());
 *   presence->start(); // Begins sending presence updates
 */
class PresenceManager : private juce::Thread {
public:
  /**
   * Create a new presence manager
   * @param webSocket WebSocket client for sending presence updates
   */
  explicit PresenceManager(WebSocketClient *webSocket);

  /**
   * Destructor - cleans up and marks user as offline
   */
  ~PresenceManager() override;

  /**
   * Start reporting presence to the server
   * Detects DAW and begins periodic updates
   */
  void start();

  /**
   * Stop reporting presence and mark user as offline
   */
  void stop();

  /**
   * Get the detected DAW name
   * @return DAW name (e.g., "Ableton Live", "Logic Pro") or "Unknown"
   */
  juce::String getDetectedDAW() const;

  /**
   * Get current presence status
   * @return "online" or "in_studio"
   */
  juce::String getCurrentStatus() const;

  /**
   * Manually set presence status
   * @param status "online" or "in_studio"
   */
  void setStatus(const juce::String &status);

private:
  // Thread implementation
  void run() override;

  // Send presence update via WebSocket
  void sendPresenceUpdate();

  // Detect DAW name at initialization
  void detectDAW();

  WebSocketClient *webSocket;
  juce::String detectedDAW;
  juce::String currentStatus = "online";  // Changed from atomic - juce::String not trivially copyable

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresenceManager)
};
