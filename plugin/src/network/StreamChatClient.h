#pragma once

#include "../util/Result.h"
#include "../util/rx/JuceScheduler.h"
#include "NetworkClient.h"
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include <rxcpp/rx.hpp>
#include <thread>
#include <vector>

// Define ASIO_STANDALONE before including websocketpp headers
#define ASIO_STANDALONE 1
#define ASIO_HAS_STD_INVOKE_RESULT 1

// websocketpp headers for WebSocket support (using TLS for wss://)
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

// ==============================================================================
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
  // ==========================================================================
  // Connection status

  /** Connection status for getstream.io WebSocket */
  enum class ConnectionStatus {
    Disconnected, // /< Not connected to getstream.io
    Connecting,   // /< Currently attempting to connect
    Connected     // /< Successfully connected
  };

  // ==========================================================================
  // Configuration

  /** Configuration for StreamChatClient */
  struct Config {
    juce::String backendBaseUrl; // /< Our backend URL for token fetching
    int timeoutMs = 30000;       // /< Request timeout in milliseconds
    int maxRetries = 3;          // /< Maximum number of retry attempts

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

  // ==========================================================================
  // Data structures

  /** Chat channel information */
  struct Channel {
    juce::String id;            // /< Unique channel identifier
    juce::String type;          // /< Channel type ("messaging" or "team")
    juce::String name;          // /< Channel display name
    juce::var members;          // /< Array of member objects
    juce::var lastMessage;      // /< Last message in the channel
    int unreadCount = 0;        // /< Number of unread messages
    juce::String lastMessageAt; // /< Timestamp of last message
    juce::var extraData;        // /< Additional channel metadata

    // Equality comparison (required for RxCpp)
    bool operator==(const Channel &other) const {
      return id == other.id;
    }
    bool operator!=(const Channel &other) const {
      return id != other.id;
    }
  };

  /** Chat message information */
  struct Message {
    juce::String id;        // /< Unique message identifier
    juce::String text;      // /< Message text content
    juce::String userId;    // /< ID of the message author
    juce::String userName;  // /< Display name of the message author
    juce::String createdAt; // /< Message creation timestamp
    juce::var reactions;    // /< Message reactions (emoji, etc.)
    juce::var extraData;    // /< Additional data (audio_url, reply_to, etc.)
    bool isDeleted = false; // /< Whether the message has been deleted

    // Equality comparison (required for RxCpp)
    bool operator==(const Message &other) const {
      return id == other.id;
    }
    bool operator!=(const Message &other) const {
      return id != other.id;
    }
  };

  /** User presence information */
  struct UserPresence {
    juce::String userId;     // /< User identifier
    bool online = false;     // /< Whether the user is currently online
    juce::String lastActive; // /< Last active timestamp
    juce::String status;     // /< Custom status (e.g., "in studio")

    // Equality comparison (required for RxCpp)
    bool operator==(const UserPresence &other) const {
      return userId == other.userId;
    }
    bool operator!=(const UserPresence &other) const {
      return userId != other.userId;
    }
  };

  /** Typing event information */
  struct TypingEvent {
    juce::String userId;    // /< User who is typing
    juce::String channelId; // /< Channel where typing is occurring
    bool isTyping = false;  // /< Whether user is currently typing

    // Equality comparison (required for RxCpp)
    bool operator==(const TypingEvent &other) const {
      return userId == other.userId && channelId == other.channelId;
    }
    bool operator!=(const TypingEvent &other) const {
      return !(*this == other);
    }
  };

  /** Message event with channel context */
  struct MessageEvent {
    Message message;        // /< The message
    juce::String channelId; // /< Channel where message was sent

    // Equality comparison (required for RxCpp)
    bool operator==(const MessageEvent &other) const {
      return message.id == other.message.id;
    }
    bool operator!=(const MessageEvent &other) const {
      return message.id != other.message.id;
    }
  };

  // ==========================================================================
  // Callback types - using Outcome<T> for type-safe error handling

  /** Result structure for token fetch operations */
  struct TokenResult {
    juce::String token;  // /< getstream.io chat token
    juce::String apiKey; // /< getstream.io API key
    juce::String userId; // /< Current user ID
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

  // ==========================================================================
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

  // ==========================================================================
  // Channel Management (REST API)
  // NOTE: Callback-based methods are deprecated. Use observable versions instead.

  /** Create a direct messaging channel with another user
   * @deprecated Use createDirectChannelObservable() instead
   * @param targetUserId The user ID to message
   * @param callback Called with the created channel or error
   */
  [[deprecated("Use createDirectChannelObservable() instead")]]
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
   * @deprecated Use queryChannelsObservable() instead
   * @param callback Called with list of channels or error
   * @param limit Maximum number of channels to return
   * @param offset Pagination offset
   */
  [[deprecated("Use queryChannelsObservable() instead")]]
  void queryChannels(ChannelsCallback callback, int limit = 20, int offset = 0);

  /** Get a specific channel by type and ID
   * @deprecated Use getChannelObservable() instead
   * @param channelType Channel type (e.g., "messaging", "team")
   * @param channelId Channel identifier
   * @param callback Called with the channel or error
   */
  [[deprecated("Use getChannelObservable() instead")]]
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

  // ==========================================================================
  // Message Operations (REST API)
  // NOTE: Callback-based methods are deprecated. Use observable versions instead.

  /** Send a text message to a channel
   * @deprecated Use sendMessageObservable() instead
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param text Message text content
   * @param extraData Additional message data (e.g., reply_to, attachments)
   * @param callback Called with the sent message or error
   */
  [[deprecated("Use sendMessageObservable() instead")]]
  void sendMessage(const juce::String &channelType, const juce::String &channelId, const juce::String &text,
                   const juce::var &extraData, MessageCallback callback);

  /** Query messages from a channel
   * @deprecated Use queryMessagesObservable() instead
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param limit Maximum number of messages to return
   * @param offset Pagination offset
   * @param callback Called with list of messages or error
   */
  [[deprecated("Use queryMessagesObservable() instead")]]
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

  // ==========================================================================
  // Read Receipts

  /** Mark a channel as read
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param callback Called with result or error
   */
  void markChannelRead(const juce::String &channelType, const juce::String &channelId,
                       std::function<void(Outcome<void>)> callback);

  // ==========================================================================
  // Audio Snippet Sharing

  /** Result structure for audio snippet uploads */
  struct AudioSnippetResult {
    juce::String audioUrl; // /< CDN URL of uploaded audio
    double duration = 0.0; // /< Duration in seconds
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

  // ==========================================================================
  // Message Search
  // NOTE: Callback-based methods are deprecated. Use observable versions instead.

  /** Search messages
   * @deprecated Use searchMessagesObservable() instead
   */
  [[deprecated("Use searchMessagesObservable() instead")]]
  void searchMessages(const juce::String &query, const juce::var &channelFilters, int limit, int offset,
                      MessagesCallback callback);

  // ==========================================================================
  // Presence (App-Wide) - GetStream.io handles presence automatically
  // NOTE: Callback-based methods are deprecated. Use observable versions instead.

  /** Query presence status for multiple users
   * @deprecated Use queryPresenceObservable() instead
   * @param userIds List of user IDs to query
   * @param callback Called with list of presence info or error
   */
  [[deprecated("Use queryPresenceObservable() instead")]]
  void queryPresence(const std::vector<juce::String> &userIds, PresenceCallback callback);

  /** Update current user's status in GetStream.io
   * @param status Custom status message (e.g., "jamming")
   * @param extraData Additional user data
   * @param callback Called with result or error
   */
  void updateStatus(const juce::String &status, const juce::var &extraData,
                    std::function<void(Outcome<void>)> callback);

  /** Upsert user data to GetStream.io (for presence updates)
   * @param userData User data including id, name, status, invisible flag
   * @param callback Called with result or error
   */
  void upsertUser(std::shared_ptr<juce::DynamicObject> userData, std::function<void(Outcome<juce::var>)> callback);

  /** Subscribe to presence change events from GetStream.io
   * @param callback Called whenever a presence event occurs (user comes online/offline)
   */
  void subscribeToPresenceEvents(std::function<void(const juce::var &event)> callback);

  // ==========================================================================
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
  // NOTE: These callback setters are deprecated. Use the observable subjects instead:
  //   - messages() for new message events
  //   - typingEvents() for typing indicators
  //   - presenceChanges() for presence updates
  //   - pollUnreadCountObservable() for unread counts

  [[deprecated("Use messages() observable instead")]]
  void setMessageReceivedCallback(MessageReceivedCallback callback) {
    messageReceivedCallback = callback;
  }
  [[deprecated("Use typingEvents() observable instead")]]
  void setTypingCallback(TypingCallback callback) {
    typingCallback = callback;
  }
  [[deprecated("Use presenceChanges() observable instead")]]
  void setPresenceChangedCallback(PresenceChangedCallback callback) {
    presenceChangedCallback = callback;
  }
  [[deprecated("Use pollUnreadCountObservable() instead")]]
  void setUnreadCountCallback(std::function<void(int totalUnread)> callback) {
    unreadCountCallback = callback;
  }
  void setMessageNotificationCallback(MessageNotificationCallback callback) {
    onMessageNotificationRequested = callback;
  }

  // ==========================================================================
  // Reactive Observables (RxCpp subjects for real-time events)
  //
  // These provide hot observables for WebSocket events. Multiple UI components
  // can subscribe to the same event stream without affecting each other.
  //
  // Usage:
  //   chatClient->messages().subscribe([](const MessageEvent& evt) {
  //       updateUI(evt.message, evt.channelId);
  //   });

  /**
   * Observable stream of new messages from WebSocket.
   * Emits MessageEvent containing the message and channel context.
   * @return Hot observable that emits on JUCE message thread
   */
  rxcpp::observable<MessageEvent> messages() const {
    return messageSubject_.get_observable().observe_on(Sidechain::Rx::observe_on_juce_thread());
  }

  /**
   * Observable stream of typing events from WebSocket.
   * Emits TypingEvent when users start/stop typing.
   * @return Hot observable that emits on JUCE message thread
   */
  rxcpp::observable<TypingEvent> typingEvents() const {
    return typingSubject_.get_observable().observe_on(Sidechain::Rx::observe_on_juce_thread());
  }

  /**
   * Observable stream of presence changes from WebSocket.
   * Emits UserPresence when users come online/offline.
   * @return Hot observable that emits on JUCE message thread
   */
  rxcpp::observable<UserPresence> presenceChanges() const {
    return presenceSubject_.get_observable().observe_on(Sidechain::Rx::observe_on_juce_thread());
  }

  // Send typing indicator via REST API event endpoint
  void sendTypingIndicator(const juce::String &channelType, const juce::String &channelId, bool isTyping);

  // ==========================================================================
  // Reactive Observable Methods (Phase 6)
  //
  // These observables wrap callback-based APIs for easier composition with
  // RxCpp operators like debounce, merge, zip, etc.
  //
  // Channel Operations
  //

  /**
   * Query channels with reactive observable pattern.
   * @param limit Maximum channels to return (default 20)
   * @param offset Pagination offset (default 0)
   * @return Observable that emits vector of Channel
   */
  rxcpp::observable<std::vector<Channel>> queryChannelsObservable(int limit = 20, int offset = 0);

  /**
   * Create a direct messaging channel with another user.
   * @param targetUserId User ID to create DM with
   * @return Observable that emits the created Channel
   */
  rxcpp::observable<Channel> createDirectChannelObservable(const juce::String &targetUserId);

  /**
   * Get a specific channel by type and ID.
   * @param channelType Channel type (messaging, team, etc.)
   * @param channelId Channel identifier
   * @return Observable that emits the Channel
   */
  rxcpp::observable<Channel> getChannelObservable(const juce::String &channelType, const juce::String &channelId);

  //
  // Message Operations
  //

  /**
   * Query messages from a channel with reactive observable pattern.
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param limit Maximum messages to return (default 50)
   * @param offset Pagination offset (default 0)
   * @return Observable that emits vector of Message
   */
  rxcpp::observable<std::vector<Message>> queryMessagesObservable(const juce::String &channelType,
                                                                  const juce::String &channelId, int limit = 50,
                                                                  int offset = 0);

  /**
   * Send a message to a channel with reactive observable pattern.
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param text Message text
   * @param extraData Additional message data
   * @return Observable that emits the sent Message
   */
  rxcpp::observable<Message> sendMessageObservable(const juce::String &channelType, const juce::String &channelId,
                                                   const juce::String &text, const juce::var &extraData = juce::var());

  /**
   * Search messages with reactive observable pattern.
   * @param query Search query
   * @param channelFilters Optional channel filter conditions
   * @param limit Maximum results (default 20)
   * @param offset Pagination offset (default 0)
   * @return Observable that emits vector of Message
   */
  rxcpp::observable<std::vector<Message>> searchMessagesObservable(const juce::String &query,
                                                                   const juce::var &channelFilters = juce::var(),
                                                                   int limit = 20, int offset = 0);

  //
  // Presence Operations
  //

  /**
   * Query presence status for multiple users.
   * @param userIds Vector of user IDs to query
   * @return Observable that emits vector of UserPresence
   */
  rxcpp::observable<std::vector<UserPresence>> queryPresenceObservable(const std::vector<juce::String> &userIds);

  //
  // Interval-based Polling Observables
  //
  // These replace juce::Timer with RxCpp intervals for cleaner composition
  // with other reactive streams. The observables emit periodically until
  // unsubscribed.
  //

  /**
   * Create an observable that polls a channel for new messages at regular intervals.
   * Emits vector of new Messages when detected.
   *
   * @param channelType Channel type
   * @param channelId Channel identifier
   * @param intervalMs Polling interval in milliseconds (default 2000)
   * @return Observable that emits new messages periodically
   */
  rxcpp::observable<std::vector<Message>>
  pollChannelMessagesObservable(const juce::String &channelType, const juce::String &channelId, int intervalMs = 2000);

  /**
   * Create an observable that polls total unread count at regular intervals.
   * Emits int representing total unread messages across all channels.
   *
   * @param intervalMs Polling interval in milliseconds (default 30000)
   * @return Observable that emits unread count periodically
   */
  rxcpp::observable<int> pollUnreadCountObservable(int intervalMs = 30000);

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
   * @see disconnect() or disconnectWebSocket() To close the connection
   * @see watchChannel() For polling-based fallback if WebSocket is unavailable
   */
  void connectWebSocket();

  /**
   * Disconnect from getstream.io WebSocket and mark user offline.
   *
   * Closes the WebSocket connection and cleans up resources.
   * The ASIO event loop thread is stopped and joined.
   * User will be marked offline on GetStream.io after ~30 seconds.
   */
  void disconnect() {
    disconnectWebSocket();
  }

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

  // ==========================================================================
  // Connection Status
  void setConnectionStatusCallback(ConnectionStatusCallback callback) {
    connectionStatusCallback = callback;
  }
  ConnectionStatus getConnectionStatus() const {
    return connectionStatus.load();
  }

private:
  // ==========================================================================
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

  // RxCpp subjects for reactive event streams
  // These are mutable to allow const observable getters
  mutable rxcpp::subjects::subject<MessageEvent> messageSubject_;
  mutable rxcpp::subjects::subject<TypingEvent> typingSubject_;
  mutable rxcpp::subjects::subject<UserPresence> presenceSubject_;

  // Channel watching (polling-based real-time)
  juce::String watchedChannelType;
  juce::String watchedChannelId;
  juce::String lastSeenMessageId; // Track newest message to detect new ones
  std::unique_ptr<juce::Timer> channelPollTimer;
  void pollWatchedChannel();

  // Unread count polling
  std::unique_ptr<juce::Timer> unreadPollTimer;
  void pollUnreadCount();

  // ==========================================================================
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
