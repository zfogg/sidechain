#pragma once

#include <JuceHeader.h>
#include <functional>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>

// Define ASIO_STANDALONE before including websocketpp headers
// This tells websocketpp to use standalone ASIO (not Boost.Asio)
#define ASIO_STANDALONE 1

// Include ASIO compatibility layer before websocketpp
// This provides io_service alias for io_context (websocketpp compatibility)
#include "asio_compat.h"

// websocketpp headers
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

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
class WebSocketClient : private juce::Thread
{
public:
    //==========================================================================
    // Connection state
    enum class ConnectionState
    {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting
    };

    //==========================================================================
    // Message types for routing
    enum class MessageType
    {
        Unknown,
        NewPost,           // New post in feed
        Like,              // Someone liked a post
        Follow,            // Someone followed a user
        Comment,           // New comment on a post
        Notification,      // Generic notification
        PresenceUpdate,    // User online/offline status
        PlayCount,         // Play count update
        LikeCountUpdate,   // Like count updated (5.5.3)
        FollowerCountUpdate, // Follower count updated (5.5.4)
        Heartbeat,         // Server heartbeat response
        Error              // Server error message
    };

    //==========================================================================
    // Parsed WebSocket message
    struct Message
    {
        MessageType type = MessageType::Unknown;
        juce::String typeString;    // Original type string from server
        juce::var data;             // Message payload
        juce::String rawJson;       // Raw JSON string (for debugging)

        // Helper to get nested data
        juce::var getProperty(const juce::String& key) const
        {
            if (data.isObject())
                return data.getProperty(key, juce::var());
            return juce::var();
        }
    };

    //==========================================================================
    // Configuration
    struct Config
    {
        juce::String host = "localhost";
        int port = 8787;
        juce::String path = "/api/v1/ws";
        bool useTLS = false;

        int connectTimeoutMs = 10000;
        int heartbeatIntervalMs = 30000;
        int reconnectBaseDelayMs = 1000;
        int reconnectMaxDelayMs = 30000;
        int maxReconnectAttempts = -1;  // -1 = unlimited
        int messageQueueMaxSize = 100;

        static Config development()
        {
            return { "localhost", 8787, "/api/v1/ws", false, 10000, 30000, 1000, 30000, -1, 100 };
        }

        static Config production()
        {
            return { "api.sidechain.app", 443, "/api/v1/ws", true, 15000, 30000, 2000, 60000, -1, 100 };
        }
    };

    //==========================================================================
    WebSocketClient(const Config& config = Config::development());
    ~WebSocketClient() override;

    //==========================================================================
    // Connection management
    void connect();
    void disconnect();
    bool isConnected() const { return state.load() == ConnectionState::Connected; }
    ConnectionState getState() const { return state.load(); }

    //==========================================================================
    // Authentication
    void setAuthToken(const juce::String& token);
    void clearAuthToken();
    bool hasAuthToken() const { return !authToken.isEmpty(); }

    //==========================================================================
    // Messaging
    bool send(const juce::var& message);
    bool send(const juce::String& type, const juce::var& data);

    //==========================================================================
    // Callbacks (called on message thread)
    std::function<void(const Message&)> onMessage;
    std::function<void(ConnectionState)> onStateChanged;
    std::function<void(const juce::String& error)> onError;

    //==========================================================================
    // Configuration
    void setConfig(const Config& newConfig);
    const Config& getConfig() const { return config; }

    //==========================================================================
    // Statistics
    struct Stats
    {
        int messagesReceived = 0;
        int messagesSent = 0;
        int reconnectAttempts = 0;
        int64_t lastMessageTime = 0;
        int64_t connectedTime = 0;
    };
    Stats getStats() const;

private:
    //==========================================================================
    // websocketpp types
    typedef websocketpp::client<websocketpp::config::asio_no_tls_client> wspp_client;
    typedef websocketpp::connection_hdl connection_hdl;
    typedef typename wspp_client::message_ptr message_ptr;
    typedef typename wspp_client::connection_ptr connection_ptr;

    //==========================================================================
    // Thread implementation
    void run() override;

    //==========================================================================
    // Connection helpers
    void attemptConnection();
    void handleDisconnect(const juce::String& reason);
    void scheduleReconnect();
    void cleanupClient();

    //==========================================================================
    // websocketpp event handlers
    void onWsOpen(connection_hdl hdl);
    void onWsClose(connection_hdl hdl);
    void onWsMessage(connection_hdl hdl, message_ptr msg);
    void onWsFail(connection_hdl hdl);
    void onWsPong(connection_hdl hdl, std::string appData);

    //==========================================================================
    // Message handling
    void processTextMessage(const juce::String& text);
    void dispatchMessage(const Message& message);
    MessageType parseMessageType(const juce::String& typeStr);

    //==========================================================================
    // Heartbeat
    void sendHeartbeat();
    bool isHeartbeatTimedOut() const;

    //==========================================================================
    // Message queue (for offline messages)
    void queueMessage(const juce::var& message);
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
    std::atomic<bool> connectionActive{ false };
    std::unique_ptr<std::thread> asioThread;  // ASIO event loop thread

    std::atomic<ConnectionState> state { ConnectionState::Disconnected };

    // Reconnection
    std::atomic<int> reconnectAttempts { 0 };
    std::atomic<bool> shouldReconnect { true };
    std::atomic<int64_t> nextReconnectTime { 0 };

    // Heartbeat
    std::atomic<int64_t> lastPingSentTime { 0 };
    std::atomic<int64_t> lastPongReceivedTime { 0 };
    std::atomic<int64_t> lastHeartbeatSent { 0 };

    // Message queue
    std::queue<juce::var> messageQueue;
    std::mutex queueMutex;

    // Statistics
    mutable std::mutex statsMutex;
    Stats stats;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebSocketClient)
};
