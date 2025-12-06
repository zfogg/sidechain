
.. _program_listing_file_plugin_source_network_StreamChatClient.h:

Program Listing for File StreamChatClient.h
===========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_network_StreamChatClient.h>` (``plugin/source/network/StreamChatClient.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "NetworkClient.h"
   #include "../util/Result.h"
   #include <functional>
   #include <atomic>
   #include <vector>
   
   //==============================================================================
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
           juce::String backendBaseUrl;  
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
           juce::String type;        
           juce::String name;        
           juce::var members;        
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
           juce::var extraData;      
           bool isDeleted = false;   
       };
   
       struct UserPresence
       {
           juce::String userId;      
           bool online = false;      
           juce::String lastActive;  
           juce::String status;      
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
   
       void addMembers(const juce::String& channelType, const juce::String& channelId,
                      const std::vector<juce::String>& memberIds,
                      std::function<void(Outcome<void>)> callback);
   
       void removeMembers(const juce::String& channelType, const juce::String& channelId,
                         const std::vector<juce::String>& memberIds,
                         std::function<void(Outcome<void>)> callback);
   
       void updateChannel(const juce::String& channelType, const juce::String& channelId,
                         const juce::String& name, const juce::var& extraData,
                         std::function<void(Outcome<Channel>)> callback);
   
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
       // Real-time Updates (Polling-based until WebSocket is fixed)
       // Note: True WebSocket implementation blocked by ASIO/C++23 compatibility issues
   
       // Start/stop watching a specific channel for new messages (aggressive polling)
       void watchChannel(const juce::String& channelType, const juce::String& channelId);
       void unwatchChannel();
       bool isWatchingChannel() const { return !watchedChannelId.isEmpty(); }
   
       // Callbacks for real-time events
       void setMessageReceivedCallback(MessageReceivedCallback callback) { messageReceivedCallback = callback; }
       void setTypingCallback(TypingCallback callback) { typingCallback = callback; }
       void setPresenceChangedCallback(PresenceChangedCallback callback) { presenceChangedCallback = callback; }
       void setUnreadCountCallback(std::function<void(int totalUnread)> callback) { unreadCountCallback = callback; }
   
       // Send typing indicator via REST API event endpoint
       void sendTypingIndicator(const juce::String& channelType, const juce::String& channelId, bool isTyping);
   
       // WebSocket stubs (for future implementation when ASIO issues are resolved)
       void connectWebSocket();
       void disconnectWebSocket();
       bool isWebSocketConnected() const { return wsConnected.load(); }
   
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
       std::function<void(int)> unreadCountCallback;
   
       // Channel watching (polling-based real-time)
       juce::String watchedChannelType;
       juce::String watchedChannelId;
       juce::String lastSeenMessageId;  // Track newest message to detect new ones
       std::unique_ptr<juce::Timer> channelPollTimer;
       void pollWatchedChannel();
   
       // Unread count polling
       std::unique_ptr<juce::Timer> unreadPollTimer;
       void pollUnreadCount();
   
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
