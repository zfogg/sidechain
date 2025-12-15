#pragma once

#include "../util/Result.h"
#include "NetworkClient.h"
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

// Define ASIO_STANDALONE before including websocketpp headers
#define ASIO_STANDALONE 1
#define ASIO_HAS_STD_INVOKE_RESULT 1

// websocketpp headers for WebSocket support (using TLS for wss://)
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

//==============================================================================
/**
 * StreamChatClient handles direct communication with getstream.io Chat API
 *
 * Architecture: Plugin talks directly to getstream.io (REST + WebSocket)
 * Backend only provides authentication tokens via GET /api/v1/auth/stream-token
 *
 * Features:
 * - Channel management (create, query, delete)
 * - Message operations (send, query, edit, delete, reactions)
 * - Real-time updates via WebSocket
 * - Typing indicators
 * - Read receipts
 * - Presence tracking (app-wide)
 * - Audio snippet sharing
 */
class StreamChatClient {
public:
  //==========================================================================
  // Connection status

  /** Connection status for getstream.io WebSocket */
  enum class ConnectionStatus {
    Disconnected, ///< Not connected to getstream.io
    Connecting,   ///< Currently attempting to connect
    Connected     ///< Successfully connected
  };

  //==========================================================================
  // Configuration

  /** Configuration for StreamChatClient */
  struct Config {
    juce::String backendBaseUrl; ///< Our backend URL for token fetching
    int timeoutMs = 30000;       ///< Request timeout in milliseconds
    int maxRetries = 3;          ///< Maximum number of retry attempts

    /** Create development configuration
     * @return Config with localhost backend URL
     */
    static Config development() {
      Config cfg;
      cfg.backendBaseUrl = "http://localhost:8787";
      return cfg;
    }

    /** Create production configuration
     * @return Config with production backend URL
     */
    static Config production() {
      Config cfg;
      cfg.backendBaseUrl = "https://api.sidechain.app";
      return cfg;
    }
  };

  //==========================================================================
  // Data structures

  /** Chat channel information */
  struct Channel {
    juce::String id;            ///< Unique channel identifier
    juce::String type;          ///< Channel type ("messaging" or "team")
    juce::String name;          ///< Channel display name
    juce::var members;          ///< Array of member objects
    juce::var lastMessage;      ///< Last message in the channel
    int unreadCount = 0;        ///< Number of unread messages
    juce::String lastMessageAt; ///< Timestamp of last message
    juce::var extraData;        ///< Additional channel metadata
  };

  /** Chat message information */
  struct Message {
    juce::String id;        ///< Unique message identifier
    juce::String text;      ///< Message text content
    juce::String userId;    ///< ID of the message author
    juce::String userName;  ///< Display name of the message author
    juce::String createdAt; ///< Message creation timestamp
    juce::var reactions;    ///< Message reactions (emoji, etc.)
    juce::var extraData;    ///< Additional data (audio_url, reply_to, etc.)
    bool isDeleted = false; ///< Whether the message has been deleted
  };

  /** User presence information */
  struct UserPresence {
    juce::String userId;     ///< User identifier
    bool online = false;     ///< Whether the user is currently online
    juce::String lastActive; ///< Last active timestamp
    juce::String status;     ///< Custom status (e.g., "in studio")
  };

  //==========================================================================
  // Callback types - using Outcome<T> for type-safe error handling

  /** Result structure for token fetch operations */
  struct TokenResult {
    juce::String token;  ///< getstream.io chat token
    juce::String apiKey; ///< getstream.io API key
    juce::String userId; ///< Current user ID
  };
  using TokenCallback = std::function<void(Outcome<TokenResult>)>;
  using ChannelsCallback = std::function<void(Outcome<std::vector<Channel>>)>;
  using MessagesCallback = std::function<void(Outcome<std::vector<Message>>)>;
  using MessageCallback = std::function<void(Outcome<Message>)>;
  using PresenceCallback = std::function<void(Outcome<std::vector<UserPresence>>)>;
  using ConnectionStatusCallback = std::function<void(ConnectionStatus status)>;
  using MessageReceivedCallback = std::function<void(const Message &message, const juce::String &channelId)>;
  using TypingCallback = std::function<void(const juce::String &userId, bool isTyping)>;
  using PresenceChangedCallback = std::function<void(const UserPresence &presence)>;
  using MessageNotificationCallback = std::function<void(const juce::String &title, const juce::String &message)>;

  StreamChatClient(NetworkClient *networkClient, const Config &config = Config::development());
  ~StreamChatClient();

  /** Set the network client for HTTP requests
   * @param client Pointer to the NetworkClient instance
   */
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  //==========================================================================
  // Authentication

  /** Fetch a getstream.io chat token from the backend
   * @param backendAuthToken The backend authentication token
   * @param callback Called with token result or error
   */
  void fetchToken(const juce::String &backendAuthToken, TokenCallback callback);

  /** Set the chat token and API credentials directly
   * @param token The getstream.io chat token
   * @param apiKey The getstream.io API key
   * @param userId The current user ID
   */
  void setToken(const juce::String &token, const juce::String &apiKey, const juce::String &userId);

  /** Check if authenticated with getstream.io
   * @return true if token and API key are set
   */
  bool isAuthenticated() const {
    return !chatToken.isEmpty() && !apiKey.isEmpty();
  }

  //==========================================================================
  // Channel Management (REST API)

  /** Create a direct messaging channel with another user
   * @param targetUserId The user ID to message
   * @param callback Called with the created channel or error
   */
  void createDirectChannel(const juce::String &targetUserId, std::function<void(Outcome<Channel>)> callback);

  /** Create a group channel
   * @param channelId Unique channel identifier
   * @param name Channel display name
   * @param memberIds List of user IDs to add as members
   * @param callback Called with the created channel or error
   */
  void createGroupChannel(const juce::String &channelId, const juce::String &name,
                          const std::vector<juce::String> &memberIds, std::function<void(Outcome<Channel>)> callback);

  /** Query channels for the current user
   * @param callback Called with list of channels or error
   * @param limit Maximum number of channels to return
   * @param offset Pagination offset
   */
  void queryChannels(ChannelsCallback callback, int limit = 20, int offset = 0);

  /** Get a specific channel by type and ID
   * @param channelType Channel type (e.g., "messaging", "team")
   * @param channelId Channel identifier
   * @param callback Called with the channel or error
   */
  void getChannel(const juce::String &channelType, const juce::String &channelId,
                  std::function<void(Outcome<Channel>)> callback);

  /** Delete a channel
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param callback Called with result or error
   */
  void deleteChannel(const juce::String &channelType, const juce::String &channelId,
                     std::function<void(Outcome<void>)> callback);

  /** Leave a channel
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param callback Called with result or error
   */
  void leaveChannel(const juce::String &channelType, const juce::String &channelId,
                    std::function<void(Outcome<void>)> callback);

  /** Add members to a channel
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param memberIds List of user IDs to add
   * @param callback Called with result or error
   */
  void addMembers(const juce::String &channelType, const juce::String &channelId,
                  const std::vector<juce::String> &memberIds, std::function<void(Outcome<void>)> callback);

  /** Remove members from a channel
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param memberIds List of user IDs to remove
   * @param callback Called with result or error
   */
  void removeMembers(const juce::String &channelType, const juce::String &channelId,
                     const std::vector<juce::String> &memberIds, std::function<void(Outcome<void>)> callback);

  /** Update channel metadata
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param name New channel name
   * @param extraData Additional channel data
   * @param callback Called with updated channel or error
   */
  void updateChannel(const juce::String &channelType, const juce::String &channelId, const juce::String &name,
                     const juce::var &extraData, std::function<void(Outcome<Channel>)> callback);

  //==========================================================================
  // Message Operations (REST API)

  /** Send a text message to a channel
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param text Message text content
   * @param extraData Additional message data (e.g., reply_to, attachments)
   * @param callback Called with the sent message or error
   */
  void sendMessage(const juce::String &channelType, const juce::String &channelId, const juce::String &text,
                   const juce::var &extraData, MessageCallback callback);

  /** Query messages from a channel
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param limit Maximum number of messages to return
   * @param offset Pagination offset
   * @param callback Called with list of messages or error
   */
  void queryMessages(const juce::String &channelType, const juce::String &channelId, int limit, int offset,
                     MessagesCallback callback);

  /** Update an existing message
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param messageId Message identifier
   * @param newText Updated message text
   * @param callback Called with updated message or error
   */
  void updateMessage(const juce::String &channelType, const juce::String &channelId, const juce::String &messageId,
                     const juce::String &newText, MessageCallback callback);

  /** Delete a message
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param messageId Message identifier
   * @param callback Called with result or error
   */
  void deleteMessage(const juce::String &channelType, const juce::String &channelId, const juce::String &messageId,
                     std::function<void(Outcome<void>)> callback);

  /** Add a reaction to a message
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param messageId Message identifier
   * @param reactionType Reaction emoji or type
   * @param callback Called with result or error
   */
  void addReaction(const juce::String &channelType, const juce::String &channelId, const juce::String &messageId,
                   const juce::String &reactionType, std::function<void(Outcome<void>)> callback);

  /** Remove a reaction from a message
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param messageId Message identifier
   * @param reactionType Reaction emoji or type to remove
   * @param callback Called with result or error
   */
  void removeReaction(const juce::String &channelType, const juce::String &channelId, const juce::String &messageId,
                      const juce::String &reactionType, std::function<void(Outcome<void>)> callback);

  //==========================================================================
  // Read Receipts

  /** Mark a channel as read
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param callback Called with result or error
   */
  void markChannelRead(const juce::String &channelType, const juce::String &channelId,
                       std::function<void(Outcome<void>)> callback);

  //==========================================================================
  // Audio Snippet Sharing

  /** Result structure for audio snippet uploads */
  struct AudioSnippetResult {
    juce::String audioUrl; ///< CDN URL of uploaded audio
    double duration = 0.0; ///< Duration in seconds
  };
  using AudioSnippetCallback = std::function<void(Outcome<AudioSnippetResult>)>;

  /** Upload an audio snippet for sharing in messages
   * @param audioBuffer The audio data to upload
   * @param sampleRate Sample rate of the audio
   * @param callback Called with upload result (URL, duration) or error
   */
  void uploadAudioSnippet(const juce::AudioBuffer<float> &audioBuffer, double sampleRate,
                          AudioSnippetCallback callback);

  /** Send a message with an audio attachment
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param text Optional message text
   * @param audioBuffer The audio data to attach
   * @param sampleRate Sample rate of the audio
   * @param callback Called with the sent message or error
   */
  void sendMessageWithAudio(const juce::String &channelType, const juce::String &channelId, const juce::String &text,
                            const juce::AudioBuffer<float> &audioBuffer, double sampleRate, MessageCallback callback);

  //==========================================================================
  // Message Search
  void searchMessages(const juce::String &query, const juce::var &channelFilters, int limit, int offset,
                      MessagesCallback callback);

  //==========================================================================
  // Presence (App-Wide)
  void queryPresence(const std::vector<juce::String> &userIds, PresenceCallback callback);
  void updateStatus(const juce::String &status, const juce::var &extraData,
                    std::function<void(Outcome<void>)> callback);

  //==========================================================================
  // Real-time Updates (Polling-based until WebSocket is fixed)
  // Note: True WebSocket implementation blocked by ASIO/C++23 compatibility
  // issues

  // Start/stop watching a specific channel for new messages (aggressive
  // polling)
  void watchChannel(const juce::String &channelType, const juce::String &channelId);
  void unwatchChannel();
  bool isWatchingChannel() const {
    return !watchedChannelId.isEmpty();
  }

  // Callbacks for real-time events
  void setMessageReceivedCallback(MessageReceivedCallback callback) {
    messageReceivedCallback = callback;
  }
  void setTypingCallback(TypingCallback callback) {
    typingCallback = callback;
  }
  void setPresenceChangedCallback(PresenceChangedCallback callback) {
    presenceChangedCallback = callback;
  }
  void setUnreadCountCallback(std::function<void(int totalUnread)> callback) {
    unreadCountCallback = callback;
  }
  void setMessageNotificationCallback(MessageNotificationCallback callback) {
    onMessageNotificationRequested = callback;
  }

  // Send typing indicator via REST API event endpoint
  void sendTypingIndicator(const juce::String &channelType, const juce::String &channelId, bool isTyping);

  /**
   * Connect to getstream.io WebSocket for real-time updates.
   *
   * Establishes a WebSocket connection to getstream.io's chat service using
   * websocketpp. The connection runs in a background thread using ASIO for
   * event handling.
   *
   * @note Requires OpenSSL for TLS support (wss:// connections).
   * @note The WebSocket URL is built automatically from the token, API key, and
   * user ID.
   *
   * Events handled:
   * - message.new: New messages in channels
   * - typing.start/typing.stop: Typing indicators
   * - user.presence.changed: User online/offline status
   *
   * @see disconnectWebSocket() To close the connection
   * @see watchChannel() For polling-based fallback if WebSocket is unavailable
   */
  void connectWebSocket();

  /**
   * Disconnect from getstream.io WebSocket.
   *
   * Closes the WebSocket connection and cleans up resources.
   * The ASIO event loop thread is stopped and joined.
   */
  void disconnectWebSocket();
  bool isWebSocketConnected() const {
    return wsConnected.load();
  }

  //==========================================================================
  // Connection Status
  void setConnectionStatusCallback(ConnectionStatusCallback callback) {
    connectionStatusCallback = callback;
  }
  ConnectionStatus getConnectionStatus() const {
    return connectionStatus.load();
  }

private:
  //==========================================================================
  NetworkClient *networkClient = nullptr;
  Config config;
  juce::String chatToken;
  juce::String apiKey;
  juce::String currentUserId;
  juce::String backendAuthToken; // Token for our backend API

  // WebSocket (using websocketpp)
  typedef websocketpp::client<websocketpp::config::asio_client> wspp_client;
  typedef websocketpp::connection_hdl connection_hdl;
  typedef typename wspp_client::message_ptr message_ptr;
  typedef typename wspp_client::connection_ptr connection_ptr;

  std::unique_ptr<wspp_client> wsClient;
  connection_hdl wsConnection;
  std::unique_ptr<std::thread> wsAsioThread;
  std::atomic<bool> wsConnected{false};
  std::atomic<bool> wsConnectionActive{false};
  juce::String wsUrl;

  // Status
  std::atomic<ConnectionStatus> connectionStatus{ConnectionStatus::Disconnected};

  // Callbacks
  ConnectionStatusCallback connectionStatusCallback;
  MessageReceivedCallback messageReceivedCallback;
  TypingCallback typingCallback;
  PresenceChangedCallback presenceChangedCallback;
  std::function<void(int)> unreadCountCallback;
  MessageNotificationCallback onMessageNotificationRequested;

  // Channel watching (polling-based real-time)
  juce::String watchedChannelType;
  juce::String watchedChannelId;
  juce::String lastSeenMessageId; // Track newest message to detect new ones
  std::unique_ptr<juce::Timer> channelPollTimer;
  void pollWatchedChannel();

  // Unread count polling
  std::unique_ptr<juce::Timer> unreadPollTimer;
  void pollUnreadCount();

  //==========================================================================
  // Internal helpers
  juce::String getStreamBaseUrl() const {
    return "https://chat.stream-io-api.com";
  }
  juce::String buildAuthHeaders() const;
  juce::var makeStreamRequest(const juce::String &endpoint, const juce::String &method,
                              const juce::var &data = juce::var());
  void updateConnectionStatus(ConnectionStatus status);
  void handleWebSocketMessage(const juce::String &message);
  void parseWebSocketEvent(const juce::var &event);

  // WebSocket event handlers (websocketpp callbacks)
  void onWsOpen(connection_hdl hdl);
  void onWsClose(connection_hdl hdl);
  void onWsMessage(connection_hdl hdl, message_ptr msg);
  void onWsFail(connection_hdl hdl);
  void cleanupWebSocket();

  // Channel ID helpers
  juce::String generateDirectChannelId(const juce::String &userId1, const juce::String &userId2);

  // Parsing helpers
  Channel parseChannel(const juce::var &channelData);
  Message parseMessage(const juce::var &messageData);
  UserPresence parsePresence(const juce::var &userData);
};
