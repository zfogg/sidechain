#pragma once

#include <JuceHeader.h>
#include "NetworkClient.h"
#include "../util/Result.h"
#include <functional>
#include <atomic>
#include <vector>

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
class StreamChatClient
{
public:
    //==========================================================================
    // Connection status
    enum class ConnectionStatus
    {
        Disconnected,
        Connecting,
        Connected
    };

    //==========================================================================
    // Configuration
    struct Config
    {
        juce::String backendBaseUrl;  // Our backend URL for token fetching
        int timeoutMs = 30000;
        int maxRetries = 3;
        
        static Config development()
        {
            Config cfg;
            cfg.backendBaseUrl = "http://localhost:8787";
            return cfg;
        }
        
        static Config production()
        {
            Config cfg;
            cfg.backendBaseUrl = "https://api.sidechain.app";
            return cfg;
        }
    };

    //==========================================================================
    // Data structures
    struct Channel
    {
        juce::String id;
        juce::String type;  // "messaging" or "team"
        juce::String name;
        juce::var members;  // Array of member objects
        juce::var lastMessage;
        int unreadCount = 0;
        juce::String lastMessageAt;
        juce::var extraData;
    };

    struct Message
    {
        juce::String id;
        juce::String text;
        juce::String userId;
        juce::String userName;
        juce::String createdAt;
        juce::var reactions;
        juce::var extraData;  // For audio_url, reply_to, etc.
        bool isDeleted = false;
    };

    struct UserPresence
    {
        juce::String userId;
        bool online = false;
        juce::String lastActive;
        juce::String status;  // Custom status like "in studio"
    };

    //==========================================================================
    // Callback types - using Outcome<T> for type-safe error handling
    struct TokenResult
    {
        juce::String token;
        juce::String apiKey;
        juce::String userId;
    };
    using TokenCallback = std::function<void(Outcome<TokenResult>)>;
    using ChannelsCallback = std::function<void(Outcome<std::vector<Channel>>)>;
    using MessagesCallback = std::function<void(Outcome<std::vector<Message>>)>;
    using MessageCallback = std::function<void(Outcome<Message>)>;
    using PresenceCallback = std::function<void(Outcome<std::vector<UserPresence>>)>;
    using ConnectionStatusCallback = std::function<void(ConnectionStatus status)>;
    using MessageReceivedCallback = std::function<void(const Message& message, const juce::String& channelId)>;
    using TypingCallback = std::function<void(const juce::String& userId, bool isTyping)>;
    using PresenceChangedCallback = std::function<void(const UserPresence& presence)>;

    StreamChatClient(NetworkClient* networkClient, const Config& config = Config::development());
    ~StreamChatClient();
    
    void setNetworkClient(NetworkClient* client) { networkClient = client; }

    //==========================================================================
    // Authentication
    void fetchToken(const juce::String& backendAuthToken, TokenCallback callback);
    void setToken(const juce::String& token, const juce::String& apiKey, const juce::String& userId);
    bool isAuthenticated() const { return !chatToken.isEmpty() && !apiKey.isEmpty(); }

    //==========================================================================
    // Channel Management (REST API)
    void createDirectChannel(const juce::String& targetUserId, std::function<void(Outcome<Channel>)> callback);
    void createGroupChannel(const juce::String& channelId, const juce::String& name, 
                           const std::vector<juce::String>& memberIds, 
                           std::function<void(Outcome<Channel>)> callback);
    void queryChannels(ChannelsCallback callback, int limit = 20, int offset = 0);
    void getChannel(const juce::String& channelType, const juce::String& channelId, 
                   std::function<void(Outcome<Channel>)> callback);
    void deleteChannel(const juce::String& channelType, const juce::String& channelId, 
                      std::function<void(Outcome<void>)> callback);
    void leaveChannel(const juce::String& channelType, const juce::String& channelId, 
                    std::function<void(Outcome<void>)> callback);

    //==========================================================================
    // Message Operations (REST API)
    void sendMessage(const juce::String& channelType, const juce::String& channelId,
                    const juce::String& text, const juce::var& extraData,
                    MessageCallback callback);
    void queryMessages(const juce::String& channelType, const juce::String& channelId,
                      int limit, int offset, MessagesCallback callback);
    void updateMessage(const juce::String& channelType, const juce::String& channelId,
                      const juce::String& messageId, const juce::String& newText,
                      MessageCallback callback);
    void deleteMessage(const juce::String& channelType, const juce::String& channelId,
                      const juce::String& messageId, std::function<void(Outcome<void>)> callback);
    void addReaction(const juce::String& channelType, const juce::String& channelId,
                    const juce::String& messageId, const juce::String& reactionType,
                    std::function<void(Outcome<void>)> callback);
    void removeReaction(const juce::String& channelType, const juce::String& channelId,
                       const juce::String& messageId, const juce::String& reactionType,
                       std::function<void(Outcome<void>)> callback);

    //==========================================================================
    // Read Receipts
    void markChannelRead(const juce::String& channelType, const juce::String& channelId,
                        std::function<void(Outcome<void>)> callback);

    //==========================================================================
    // Audio Snippet Sharing
    struct AudioSnippetResult
    {
        juce::String audioUrl;
        double duration = 0.0;
    };
    using AudioSnippetCallback = std::function<void(Outcome<AudioSnippetResult>)>;
    void uploadAudioSnippet(const juce::AudioBuffer<float>& audioBuffer, double sampleRate,
                           AudioSnippetCallback callback);
    void sendMessageWithAudio(const juce::String& channelType, const juce::String& channelId,
                             const juce::String& text, const juce::AudioBuffer<float>& audioBuffer,
                             double sampleRate, MessageCallback callback);

    //==========================================================================
    // Message Search
    void searchMessages(const juce::String& query, const juce::var& channelFilters,
                       int limit, int offset, MessagesCallback callback);

    //==========================================================================
    // Presence (App-Wide)
    void queryPresence(const std::vector<juce::String>& userIds, PresenceCallback callback);
    void updateStatus(const juce::String& status, const juce::var& extraData, 
                     std::function<void(Outcome<void>)> callback);

    //==========================================================================
    // Real-time Updates (WebSocket)
    void connectWebSocket();
    void disconnectWebSocket();
    bool isWebSocketConnected() const { return wsConnected.load(); }
    void setMessageReceivedCallback(MessageReceivedCallback callback) { messageReceivedCallback = callback; }
    void setTypingCallback(TypingCallback callback) { typingCallback = callback; }
    void setPresenceChangedCallback(PresenceChangedCallback callback) { presenceChangedCallback = callback; }
    void sendTypingIndicator(const juce::String& channelType, const juce::String& channelId, bool isTyping);

    //==========================================================================
    // Connection Status
    void setConnectionStatusCallback(ConnectionStatusCallback callback) { connectionStatusCallback = callback; }
    ConnectionStatus getConnectionStatus() const { return connectionStatus.load(); }

private:
    //==========================================================================
    NetworkClient* networkClient = nullptr;
    Config config;
    juce::String chatToken;
    juce::String apiKey;
    juce::String currentUserId;
    juce::String backendAuthToken;  // Token for our backend API

    // WebSocket
    std::unique_ptr<juce::StreamingSocket> webSocket;
    std::atomic<bool> wsConnected{ false };
    juce::String wsUrl;

    // Status
    std::atomic<ConnectionStatus> connectionStatus{ ConnectionStatus::Disconnected };

    // Callbacks
    ConnectionStatusCallback connectionStatusCallback;
    MessageReceivedCallback messageReceivedCallback;
    TypingCallback typingCallback;
    PresenceChangedCallback presenceChangedCallback;

    //==========================================================================
    // Internal helpers
    juce::String getStreamBaseUrl() const { return "https://chat.stream-io-api.com"; }
    juce::String buildAuthHeaders() const;
    juce::var makeStreamRequest(const juce::String& endpoint, const juce::String& method, 
                                const juce::var& data = juce::var());
    void updateConnectionStatus(ConnectionStatus status);
    void handleWebSocketMessage(const juce::String& message);
    void parseWebSocketEvent(const juce::var& event);
    
    // Channel ID helpers
    juce::String generateDirectChannelId(const juce::String& userId1, const juce::String& userId2);
    
    // Parsing helpers
    Channel parseChannel(const juce::var& channelData);
    Message parseMessage(const juce::var& messageData);
    UserPresence parsePresence(const juce::var& userData);
};
