#pragma once

#include "../util/Constants.h"
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

// Define ASIO_STANDALONE before including websocketpp headers
// This tells websocketpp to use standalone ASIO (not Boost.Asio)
#define ASIO_STANDALONE 1
// Use std::invoke_result instead of deprecated std::result_of (removed in
// C++20)
#define ASIO_HAS_STD_INVOKE_RESULT 1

// websocketpp headers (ASIO 1.14.1 provides native io_service support)
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

//==============================================================================
/**
 * WebSocketClient provides real-time communication with the Sidechain backend.
 *
 * Features:
 * - WebSocket RFC 6455 compliant framing (via websocketpp)
 * - Automatic reconnection with exponential backoff
 * - Heartbeat/ping-pong for connection health
 * - Message queueing when disconnected
 * - Thread-safe callbacks on message thread
 * - JWT authentication via query parameter
 *
 * Usage:
 *   webSocket->setAuthToken(token);
 *   webSocket->connect();
 *   webSocket->onMessage = [](const juce::var& msg) { ... };
 */
class WebSocketClient : private juce::Thread {
public:
  //==========================================================================
  // Connection state
  enum class ConnectionState { Disconnected, Connecting, Connected, Reconnecting };

  //==========================================================================
  // Message types for routing
  enum class MessageType {
    Unknown,
    NewPost,             // New post in feed
    Like,                // Someone liked a post
    Follow,              // Someone followed a user
    Comment,             // New comment on a post
    Notification,        // Generic notification
    PresenceUpdate,      // User online/offline status
    PlayCount,           // Play count update
    LikeCountUpdate,     // Like count updated (5.5.3)
    FollowerCountUpdate, // Follower count updated (5.5.4)
    Heartbeat,           // Server heartbeat response
    Error                // Server error message
  };

  //==========================================================================
  // Parsed WebSocket message
  struct Message {
    MessageType type = MessageType::Unknown;
    juce::String typeString; // Original type string from server
    juce::var data;          // Message payload
    juce::String rawJson;    // Raw JSON string (for debugging)

    // Helper to get nested data
    juce::var getProperty(const juce::String &key) const {
      if (data.isObject())
        return data.getProperty(key, juce::var());
      return juce::var();
    }
  };

  //==========================================================================
  // Configuration
  struct Config {
    juce::String host = Constants::Endpoints::DEV_WS_HOST;
    int port = Constants::Endpoints::DEV_WS_PORT;
    juce::String path = Constants::Endpoints::WS_PATH;
    bool useTLS = false;

    int connectTimeoutMs = Constants::Api::WEBSOCKET_TIMEOUT_MS;
    int heartbeatIntervalMs = 30000;
    int reconnectBaseDelayMs = Constants::Api::RETRY_DELAY_BASE_MS;
    int reconnectMaxDelayMs = 30000;
    int maxReconnectAttempts = -1; // -1 = unlimited
    int messageQueueMaxSize = 100;

    static Config development() {
      return {Constants::Endpoints::DEV_WS_HOST,
              Constants::Endpoints::DEV_WS_PORT,
              Constants::Endpoints::WS_PATH,
              false,
              Constants::Api::WEBSOCKET_TIMEOUT_MS,
              30000,
              Constants::Api::RETRY_DELAY_BASE_MS,
              30000,
              -1,
              100};
    }

    static Config production() {
      return {Constants::Endpoints::PROD_WS_HOST,
              Constants::Endpoints::PROD_WS_PORT,
              Constants::Endpoints::WS_PATH,
              true,
              15000,
              30000,
              2000,
              60000,
              -1,
              100};
    }
  };

  //==========================================================================
  WebSocketClient(const Config &config = Config::development());
  ~WebSocketClient() override;

  //==========================================================================
  // Connection management

  /** Connect to the WebSocket server */
  void connect();

  /** Disconnect from the WebSocket server */
  void disconnect();

  /** Check if currently connected
   *  @return true if connected, false otherwise
   */
  bool isConnected() const {
    return state.load() == ConnectionState::Connected;
  }

  /** Get current connection state
   *  @return Current connection state
   */
  ConnectionState getState() const {
    return state.load();
  }

  //==========================================================================
  // Authentication

  /** Set authentication token for WebSocket connection
   *  @param token JWT authentication token
   */
  void setAuthToken(const juce::String &token);

  /** Clear authentication token */
  void clearAuthToken();

  /** Check if authentication token is set
   *  @return true if token is set, false otherwise
   */
  bool hasAuthToken() const {
    return !authToken.isEmpty();
  }

  //==========================================================================
  // Messaging

  /** Send a message to the server
   *  @param message JSON message object
   *  @return true if message was queued/sent, false if connection is down
   */
  bool send(const juce::var &message);

  /** Send a typed message to the server
   *  @param type Message type string
   *  @param data Message data payload
   *  @return true if message was queued/sent, false if connection is down
   */
  bool send(const juce::String &type, const juce::var &data);

  //==========================================================================
  // Callbacks (called on message thread)
  std::function<void(const Message &)> onMessage;
  std::function<void(ConnectionState)> onStateChanged;
  std::function<void(const juce::String &error)> onError;

  //==========================================================================
  // Configuration

  /** Update WebSocket configuration
   *  @param newConfig New configuration settings
   */
  void setConfig(const Config &newConfig);

  /** Get current configuration
   *  @return Current configuration
   */
  const Config &getConfig() const {
    return config;
  }

  //==========================================================================
  // Statistics

  /** Statistics structure for connection metrics */
  struct Stats {
    int messagesReceived = 0;    ///< Total messages received
    int messagesSent = 0;        ///< Total messages sent
    int reconnectAttempts = 0;   ///< Number of reconnection attempts
    int64_t lastMessageTime = 0; ///< Timestamp of last message (milliseconds)
    int64_t connectedTime = 0;   ///< Timestamp when connected (milliseconds)
  };

  /** Get connection statistics
   *  @return Current connection statistics
   */
  Stats getStats() const;

private:
  //==========================================================================
  // websocketpp types
  typedef websocketpp::client<websocketpp::config::asio_client> wspp_client;
  typedef websocketpp::connection_hdl connection_hdl;
  typedef typename wspp_client::message_ptr message_ptr;
  typedef typename wspp_client::connection_ptr connection_ptr;

  //==========================================================================
  // Thread implementation
  void run() override;

  //==========================================================================
  // Connection helpers
  void attemptConnection();
  void handleDisconnect(const juce::String &reason);
  void scheduleReconnect();
  void cleanupClient();
  void connectionLoop();

  //==========================================================================
  // websocketpp event handlers
  void onWsOpen(connection_hdl hdl);
  void onWsClose(connection_hdl hdl);
  void onWsMessage(connection_hdl hdl, message_ptr msg);
  void onWsFail(connection_hdl hdl);
  void onWsPong(connection_hdl hdl, std::string appData);

  //==========================================================================
  // Message handling
  void processTextMessage(const juce::String &text);
  void dispatchMessage(const Message &message);
  MessageType parseMessageType(const juce::String &typeStr);

  //==========================================================================
  // Heartbeat
  void sendHeartbeat();
  bool isHeartbeatTimedOut() const;

  //==========================================================================
  // Message queue (for offline messages)
  void queueMessage(const juce::var &message);
  void flushMessageQueue();
  void clearMessageQueue();

  //==========================================================================
  // State management
  void setState(ConnectionState newState);

  //==========================================================================
  // Build WebSocket URI
  juce::String buildUri() const;

  //==========================================================================
  Config config;
  juce::String authToken;

  // websocketpp client
  std::unique_ptr<wspp_client> client;
  connection_hdl currentConnection;
  std::atomic<bool> connectionActive{false};
  std::unique_ptr<std::thread> asioThread; // ASIO event loop thread

  std::atomic<ConnectionState> state{ConnectionState::Disconnected};

  // Reconnection
  std::atomic<int> reconnectAttempts{0};
  std::atomic<bool> shouldReconnect{true};
  std::atomic<int64_t> nextReconnectTime{0};

  // Heartbeat
  std::atomic<int64_t> lastPingSentTime{0};
  std::atomic<int64_t> lastPongReceivedTime{0};
  std::atomic<int64_t> lastHeartbeatSent{0};

  // Message queue
  std::queue<juce::var> messageQueue;
  std::mutex queueMutex;

  // Statistics
  mutable std::mutex statsMutex;
  Stats stats;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebSocketClient)
};
