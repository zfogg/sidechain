#pragma once

#include "../network/StreamChatClient.h"
#include "../util/crdt/OperationalTransform.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>
#include <map>
#include <queue>
#include <vector>

namespace Sidechain {
namespace Stores {

/**
 * ChannelState - State for a single chat channel
 *
 * Supports collaborative editing of channel description using
 * OperationalTransform
 */
struct ChannelState {
  juce::String id;
  juce::String type;
  juce::String name;
  juce::Array<juce::String> memberIds;
  std::vector<StreamChatClient::Message> messages;
  int unreadCount = 0;
  juce::String lastMessageAt;
  bool isLoadingMessages = false;
  bool hasMoreMessages = true;
  juce::var extraData;

  // Typing indicators
  std::vector<juce::String> usersTyping;

  // Drafts
  juce::String draftText;

  // ========== Collaborative Channel Description Editing (Task 4.20) ==========

  // Channel description text
  juce::String description;

  // Operation history for collaborative editing
  std::vector<std::shared_ptr<Util::CRDT::OperationalTransform::Operation>> operationHistory;

  // Pending local operations waiting for server transform
  std::queue<std::shared_ptr<Util::CRDT::OperationalTransform::Operation>> pendingOperations;

  // Timestamp counter for operation ordering
  int operationTimestamp = 0;

  // Whether description is currently syncing
  bool isSyncingDescription = false;

  bool operator==(const ChannelState &other) const {
    return id == other.id && name == other.name && messages.size() == other.messages.size() &&
           unreadCount == other.unreadCount && isLoadingMessages == other.isLoadingMessages &&
           draftText == other.draftText;
  }
};

/**
 * ChatStoreState - Combined state for all chat channels and metadata
 */
struct ChatStoreState {
  // Channels mapped by ID
  std::map<juce::String, ChannelState> channels;

  // Ordered list of channel IDs (for display order)
  std::vector<juce::String> channelOrder;

  // Currently selected channel
  juce::String currentChannelId;

  // Loading states
  bool isLoadingChannels = false;
  bool isConnecting = false;
  StreamChatClient::ConnectionStatus connectionStatus = StreamChatClient::ConnectionStatus::Disconnected;

  // Authentication
  bool isAuthenticated = false;
  juce::String userId;
  juce::String chatToken;
  juce::String apiKey;

  // User presence (userId -> presence info)
  std::map<juce::String, StreamChatClient::UserPresence> userPresence;

  // Errors
  juce::String error;

  // Convenience accessors
  const ChannelState *getCurrentChannel() const {
    auto it = channels.find(currentChannelId);
    return it != channels.end() ? &it->second : nullptr;
  }

  ChannelState *getCurrentChannelMutable() {
    auto it = channels.find(currentChannelId);
    return it != channels.end() ? &it->second : nullptr;
  }

  bool operator==(const ChatStoreState &other) const {
    // Check top-level metadata
    if (channels.size() != other.channels.size() || currentChannelId != other.currentChannelId ||
        isLoadingChannels != other.isLoadingChannels || isConnecting != other.isConnecting ||
        connectionStatus != other.connectionStatus || isAuthenticated != other.isAuthenticated ||
        userId != other.userId) {
      return false;
    }

    // Check actual channel state (messages, typing, etc.)
    for (const auto &[channelId, channel] : channels) {
      auto it = other.channels.find(channelId);
      if (it == other.channels.end())
        return false;

      if (!(channel == it->second)) // Use ChannelState::operator==
        return false;
    }

    return true;
  }
};

/**
 * ChatStore - Reactive store for managing chat/messaging state
 *
 * Wraps StreamChatClient functionality with reactive subscriptions.
 *
 * Features:
 * - Reactive channel list management
 * - Real-time message updates via WebSocket
 * - Typing indicators
 * - Presence tracking
 * - Message drafts
 * - Optimistic message sending
 * - Read receipts
 *
 * Usage:
 *   // Get singleton instance
 *   auto& chatStore = ChatStore::getInstance();
 *   chatStore.setStreamChatClient(streamChatClient);
 *
 *   // Subscribe to state changes
 *   auto unsubscribe = chatStore.subscribe([](const ChatStoreState& state) {
 *       if (state.isAuthenticated) {
 *           displayChannels(state.channels);
 *           if (auto* channel = state.getCurrentChannel()) {
 *               displayMessages(channel->messages);
 *           }
 *       }
 *   });
 *
 *   // Load channels
 *   chatStore.loadChannels();
 *
 *   // Send message (optimistic)
 *   chatStore.sendMessage(channelId, "Hello!");
 */
class ChatStore : public Store<ChatStoreState> {
public:
  /**
   * Get singleton instance
   */
  static ChatStore &getInstance() {
    static ChatStore instance;
    return instance;
  }

  /**
   * Set the Stream Chat client
   */
  void setStreamChatClient(StreamChatClient *client);

  /**
   * Get the Stream Chat client
   */
  StreamChatClient *getStreamChatClient() const {
    return streamChatClient;
  }

  //==========================================================================
  // Authentication

  /**
   * Set authentication token and connect to Stream Chat
   * @param token Chat token from getstream.io
   * @param apiKey API key for getstream.io
   * @param userId Current user ID
   */
  void setAuthentication(const juce::String &token, const juce::String &apiKey, const juce::String &userId);

  /**
   * Clear authentication and disconnect
   */
  void clearAuthentication();

  /**
   * Check if authenticated
   */
  bool isAuthenticated() const {
    return getState().isAuthenticated;
  }

  //==========================================================================
  // Channel Management

  /**
   * Load list of channels for current user
   * @param forceRefresh If true, bypass cache
   */
  void loadChannels(bool forceRefresh = false);

  /**
   * Create a direct message channel with another user
   * @param targetUserId User ID to message
   */
  void createDirectChannel(const juce::String &targetUserId);

  /**
   * Create a group channel
   * @param channelId Unique channel ID
   * @param name Channel display name
   * @param memberIds List of user IDs to add
   */
  void createGroupChannel(const juce::String &channelId, const juce::String &name,
                          const std::vector<juce::String> &memberIds);

  /**
   * Delete a channel
   * @param channelId Channel to delete
   */
  void deleteChannel(const juce::String &channelId);

  /**
   * Leave a channel
   * @param channelId Channel to leave
   */
  void leaveChannel(const juce::String &channelId);

  /**
   * Select a channel to view
   * @param channelId Channel to select
   */
  void selectChannel(const juce::String &channelId);

  /**
   * Get currently selected channel ID
   */
  juce::String getCurrentChannelId() const {
    return getState().currentChannelId;
  }

  //==========================================================================
  // Message Management

  /**
   * Load messages for a channel
   * @param channelId Channel to load messages for
   * @param limit Number of messages to load
   */
  void loadMessages(const juce::String &channelId, int limit = 50);

  /**
   * Load more (older) messages for pagination
   * @param channelId Channel to load more messages for
   */
  void loadMoreMessages(const juce::String &channelId);

  /**
   * Send a text message (optimistic update)
   * @param channelId Channel to send message to
   * @param text Message text
   */
  void sendMessage(const juce::String &channelId, const juce::String &text);

  /**
   * Send a message with audio snippet
   * @param channelId Channel to send to
   * @param text Message text
   * @param audioBuffer Audio data to attach
   * @param sampleRate Sample rate of audio
   */
  void sendMessageWithAudio(const juce::String &channelId, const juce::String &text,
                            const juce::AudioBuffer<float> &audioBuffer, double sampleRate);

  /**
   * Delete a message
   * @param channelId Channel containing the message
   * @param messageId Message to delete
   */
  void deleteMessage(const juce::String &channelId, const juce::String &messageId);

  /**
   * Add reaction to a message
   * @param channelId Channel containing the message
   * @param messageId Message to react to
   * @param reaction Reaction emoji/type
   */
  void addReaction(const juce::String &channelId, const juce::String &messageId, const juce::String &reaction);

  //==========================================================================
  // Sharing Content to Channels (Task 2.5)

  /**
   * Share a feed post to one or more channels
   * @param postId The post ID to share (used to fetch post data)
   * @param channelIds List of channel IDs to share to
   * @param optionalMessage Optional message to include with the share
   */
  void sharePostToChannels(const juce::String &postId, const std::vector<juce::String> &channelIds,
                           const juce::String &optionalMessage = "");

  /**
   * Share a story to one or more channels
   * @param storyId The story ID to share
   * @param channelIds List of channel IDs to share to
   * @param optionalMessage Optional message to include with the share
   */
  void shareStoryToChannels(const juce::String &storyId, const std::vector<juce::String> &channelIds,
                            const juce::String &optionalMessage = "");

  /**
   * Send a message with embedded post/story preview
   * This is a lower-level method used by sharePostToChannels and
   * shareStoryToChannels
   * @param channelId Channel to send message to
   * @param text Message text
   * @param sharedContent JSON object with post/story data {type:
   * "post"|"story", id, data}
   */
  void sendMessageWithSharedContent(const juce::String &channelId, const juce::String &text,
                                    const juce::var &sharedContent);

  //==========================================================================
  // Typing Indicators

  /**
   * Send typing indicator
   * @param channelId Channel user is typing in
   */
  void startTyping(const juce::String &channelId);

  /**
   * Stop typing indicator
   * @param channelId Channel to stop typing in
   */
  void stopTyping(const juce::String &channelId);

  //==========================================================================
  // Drafts

  /**
   * Update draft text for a channel
   * @param channelId Channel to update draft for
   * @param text Draft text
   */
  void updateDraft(const juce::String &channelId, const juce::String &text);

  /**
   * Clear draft for a channel
   * @param channelId Channel to clear draft for
   */
  void clearDraft(const juce::String &channelId);

  //==========================================================================
  // Read Receipts

  /**
   * Mark messages as read in a channel
   * @param channelId Channel to mark as read
   */
  void markAsRead(const juce::String &channelId);

  //==========================================================================
  // Presence

  /**
   * Update user presence information
   * @param userId User ID
   * @param presence Presence info
   */
  void updateUserPresence(const juce::String &userId, const StreamChatClient::UserPresence &presence);

  /**
   * Query presence for a list of users
   * @param userIds List of user IDs to query
   */
  void queryPresence(const std::vector<juce::String> &userIds);

  //==========================================================================
  // Collaborative Channel Description Editing (Task 4.20)

  /**
   * Edit channel description with OperationalTransform conflict resolution
   *
   * When user edits the description, this creates an Insert/Delete operation
   * and sends it to the server. The server applies OT transformations to
   * handle concurrent edits from multiple users.
   *
   * @param channelId Channel to edit description for
   * @param newDescription New description text
   */
  void editChannelDescription(const juce::String &channelId, const juce::String &newDescription);

  /**
   * Apply server-transformed operation to local description
   *
   * Called when server sends back a transformed operation after handling
   * concurrent edits. This ensures all clients converge to the same state.
   *
   * @param channelId Channel to apply operation to
   * @param operation Transformed operation from server
   */
  void applyServerOperation(const juce::String &channelId,
                            const std::shared_ptr<Util::CRDT::OperationalTransform::Operation> &operation);

  /**
   * Handle concurrent edit from another user
   *
   * @param channelId Channel where edit occurred
   * @param operation Remote operation from other user
   * @param clientId ID of client that sent the operation
   */
  void handleRemoteOperation(const juce::String &channelId,
                             const std::shared_ptr<Util::CRDT::OperationalTransform::Operation> &operation,
                             int clientId);

  //==========================================================================
  // Real-time Events

  /**
   * Handle incoming message from WebSocket
   * @param message Message received
   * @param channelId Channel message was sent to
   */
  void handleMessageReceived(const StreamChatClient::Message &message, const juce::String &channelId);

  /**
   * Handle typing event
   * @param channelId Channel where user is typing
   * @param userId User who is typing
   * @param isTyping Whether user is typing
   */
  void handleTypingEvent(const juce::String &channelId, const juce::String &userId, bool isTyping);

  /**
   * Handle connection status change
   * @param status New connection status
   */
  void handleConnectionStatusChanged(StreamChatClient::ConnectionStatus status);

private:
  ChatStore();
  ~ChatStore() override = default;

  // Stream Chat client (not owned)
  StreamChatClient *streamChatClient = nullptr;

  // Client ID for this plugin instance (used for OT conflict resolution)
  // Generated on first use using hash of user ID
  int clientId = -1;

  // Internal helpers
  void setupEventHandlers();
  void handleChannelsLoaded(const std::vector<StreamChatClient::Channel> &channels);
  void handleChannelsError(const juce::String &error);
  void handleMessagesLoaded(const juce::String &channelId, const std::vector<StreamChatClient::Message> &messages);
  void handleMessagesError(const juce::String &channelId, const juce::String &error);
  void handleMessageSent(const juce::String &channelId, const StreamChatClient::Message &message);
  void handleMessageSendError(const juce::String &channelId, const juce::String &tempId, const juce::String &error);

  // Generate temporary message ID for optimistic updates
  juce::String generateTempMessageId() const;

  // ========== Collaborative Editing Helpers (Task 4.20) ==========

  /**
   * Get or initialize client ID for OT operations
   * Uses hash of user ID to generate stable client ID
   */
  int getClientId();

  /**
   * Generate operation comparing old and new description text
   * Creates Insert/Delete operations as needed
   */
  std::shared_ptr<Util::CRDT::OperationalTransform::Operation>
  generateDescriptionOperation(const juce::String &oldDescription, const juce::String &newDescription);

  /**
   * Send operation to server for OT transformation
   * @param channelId Channel ID
   * @param operation Operation to send
   */
  void sendOperationToServer(const juce::String &channelId,
                             const std::shared_ptr<Util::CRDT::OperationalTransform::Operation> &operation);

  // Singleton enforcement
  ChatStore(const ChatStore &) = delete;
  ChatStore &operator=(const ChatStore &) = delete;
};

} // namespace Stores
} // namespace Sidechain
