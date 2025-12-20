#include "../AppStore.h"
#include "../../util/Log.h"

namespace Sidechain {
namespace Stores {

void AppStore::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
  Log::info("AppStore::setStreamChatClient: StreamChatClient set: " +
            juce::String(client != nullptr ? "valid" : "null"));
}

void AppStore::loadChannels() {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Log::warn("AppStore::loadChannels: not implemented - use ChatStore");
}

void AppStore::selectChannel(const juce::String &channelId) {
  sliceManager.getChatSlice()->dispatch([channelId](ChatState &state) { state.currentChannelId = channelId; });
}

void AppStore::loadMessages(const juce::String &channelId, int limit) {
  if (!streamChatClient) {
    Log::warn("AppStore::loadMessages: StreamChatClient not available");
    return;
  }

  Log::info("AppStore::loadMessages: Loading " + juce::String(limit) + " messages for channel " + channelId);

  // Query messages from getstream.io
  streamChatClient->queryMessages(
      "messaging", channelId, limit, 0,
      [this, channelId](Outcome<std::vector<StreamChatClient::Message>> messagesResult) {
        if (!messagesResult.isOk()) {
          Log::error("AppStore::loadMessages: Failed to load messages - " + messagesResult.getError());
          return;
        }

        const auto &messages = messagesResult.getValue();
        Log::info("AppStore::loadMessages: Loaded " + juce::String(messages.size()) + " messages for " + channelId);

        // Add all messages to AppStore state
        for (const auto &msg : messages) {
          addMessageToChannel(channelId, msg.id, msg.text, msg.userId, msg.userName, msg.createdAt);
        }
      });
}

void AppStore::sendMessage(const juce::String &channelId, const juce::String &text) {
  Log::info("AppStore::sendMessage - 📨 CALLED with channelId: " + channelId);

  if (channelId.isEmpty()) {
    Log::error("AppStore::sendMessage - ERROR: channelId is empty!");
    return;
  }

  if (text.isEmpty()) {
    Log::error("AppStore::sendMessage - ERROR: text is empty!");
    return;
  }

  Log::info("AppStore::sendMessage - ✓ channelId and text are valid");

  // Verify channel exists in state
  const auto &chatState = sliceManager.getChatSlice()->getState();
  auto channelIt = chatState.channels.find(channelId);
  if (channelIt == chatState.channels.end()) {
    Log::error("AppStore::sendMessage - ERROR: Channel not found in state: " + channelId);
    return;
  }
  Log::info("AppStore::sendMessage - ✓ Channel found in state");

  if (!streamChatClient) {
    Log::error("AppStore::sendMessage - CRITICAL: StreamChatClient not available - cannot send message!");
    return;
  }

  Log::info("AppStore::sendMessage - Creating message object");

  juce::String userId = sliceManager.getUserSlice()->getState().userId;
  juce::String username = sliceManager.getUserSlice()->getState().username;

  Log::info("AppStore::sendMessage - ✓ Setting properties - userId: " + userId + ", username: " + username);

  juce::String messageId = juce::Uuid().toString();
  auto msg = std::make_shared<Message>();
  msg->id = messageId;
  msg->conversationId = channelId;
  msg->text = text;
  msg->senderId = userId;
  msg->senderUsername = username;
  msg->createdAt = juce::Time::getCurrentTime();

  Log::info("AppStore::sendMessage - ✓ Message object created successfully");

  Log::info("AppStore::sendMessage - Sending message to Stream.io via StreamChatClient");
  juce::var extraData;
  streamChatClient->sendMessage(
      "messaging", channelId, text, extraData,
      [this, channelId, msg, messageId](const Outcome<StreamChatClient::Message> &result) {
        if (!result.isOk()) {
          Log::error("AppStore::sendMessage - ERROR: Failed to send message to Stream.io: " + result.getError());
          // Still add to local state for optimistic UI
          sliceManager.getChatSlice()->dispatch([channelId, msg](ChatState &state) {
            auto channelIterator = state.channels.find(channelId);
            if (channelIterator != state.channels.end()) {
              channelIterator->second.messages.push_back(msg);
            }
          });
          return;
        }

        Log::info("AppStore::sendMessage - ✓ Message successfully sent to Stream.io");
        // Message was sent successfully, add to local state
        sliceManager.getChatSlice()->dispatch([channelId, msg](ChatState &state) {
          auto channelIterator = state.channels.find(channelId);
          if (channelIterator != state.channels.end()) {
            channelIterator->second.messages.push_back(msg);
            Log::info("AppStore::sendMessage - ✓ Added message to channel state with success status");
          } else {
            Log::error("AppStore::sendMessage callback - ERROR: Channel disappeared from state!");
          }
        });
      });
}

void AppStore::editMessage(const juce::String &channelId, const juce::String &messageId, const juce::String &newText) {
  if (!streamChatClient) {
    Log::error("AppStore::editMessage - CRITICAL: StreamChatClient not available");
    return;
  }

  Log::info("AppStore::editMessage - Editing message " + messageId + " in channel " + channelId);

  streamChatClient->updateMessage(
      "messaging", channelId, messageId, newText,
      [this, channelId, messageId, newText](const Outcome<StreamChatClient::Message> &result) {
        if (!result.isOk()) {
          Log::error("AppStore::editMessage - Failed to update message: " + result.getError());
          return;
        }

        Log::info("AppStore::editMessage - Message updated successfully");

        sliceManager.getChatSlice()->dispatch([channelId, messageId, newText](ChatState &state) {
          auto channelIt = state.channels.find(channelId);
          if (channelIt != state.channels.end()) {
            for (auto &msg : channelIt->second.messages) {
              if (msg && msg->id == messageId) {
                // For now, create a new message with the updated text
                // TODO: Implement proper message update in Message model
                msg->text = newText;
                Log::info("AppStore::editMessage - Updated message in state");
                return;
              }
            }
          }
          Log::warn("AppStore::editMessage - Message not found in state");
        });
      });
}

void AppStore::deleteMessage(const juce::String &channelId, const juce::String &messageId) {
  if (!streamChatClient) {
    Log::error("AppStore::deleteMessage - CRITICAL: StreamChatClient not available");
    return;
  }

  Log::info("AppStore::deleteMessage - Deleting message " + messageId + " from channel " + channelId);

  streamChatClient->deleteMessage(
      "messaging", channelId, messageId, [this, channelId, messageId](const Outcome<void> &result) {
        if (!result.isOk()) {
          Log::error("AppStore::deleteMessage - Failed to delete message: " + result.getError());
          return;
        }

        Log::info("AppStore::deleteMessage - Message deleted successfully");

        sliceManager.getChatSlice()->dispatch([channelId, messageId](ChatState &state) {
          auto channelIt = state.channels.find(channelId);
          if (channelIt != state.channels.end()) {
            auto &messages = channelIt->second.messages;
            for (auto it = messages.begin(); it != messages.end(); ++it) {
              if (*it && (*it)->id == messageId) {
                messages.erase(it);
                Log::info("AppStore::deleteMessage - Removed message from state");
                return;
              }
            }
          }
          Log::warn("AppStore::deleteMessage - Message not found in state");
        });
      });
}

void AppStore::startTyping(const juce::String &channelId) {
  if (channelId.isEmpty()) {
    Log::warn("AppStore::startTyping: Channel ID is empty");
    return;
  }
  Log::debug("AppStore::startTyping: User started typing in channel " + channelId);
  // The ChatStore handles actual WebSocket transmission of typing indicator
}

void AppStore::stopTyping(const juce::String &channelId) {
  if (channelId.isEmpty()) {
    Log::warn("AppStore::stopTyping: Channel ID is empty");
    return;
  }
  Log::debug("AppStore::stopTyping: User stopped typing in channel " + channelId);
  // The ChatStore handles actual WebSocket transmission of typing stop
}

void AppStore::handleNewMessage(const juce::var &messageData) {
  if (!messageData.isObject()) {
    Log::warn("AppStore::handleNewMessage: Message data is not an object");
    return;
  }

  const auto *obj = messageData.getDynamicObject();
  if (!obj) {
    Log::error("AppStore::handleNewMessage: Failed to extract dynamic object from messageData");
    return;
  }

  juce::String messageId = obj->getProperty("id").toString();
  juce::String channelId = obj->getProperty("channel_id").toString();
  juce::String text = obj->getProperty("text").toString();

  if (messageId.isEmpty() || channelId.isEmpty()) {
    Log::warn("AppStore::handleNewMessage: Message missing ID or channel ID");
    return;
  }

  Log::debug("AppStore::handleNewMessage: Received message " + messageId + " in channel " + channelId);

  // Forward to appropriate handler or state update
  // The ChatStore handles detailed message processing
}

void AppStore::handleTypingStart(const juce::String &userId) {
  if (userId.isEmpty()) {
    Log::warn("AppStore::handleTypingStart: User ID is empty");
    return;
  }
  Log::debug("AppStore::handleTypingStart: User " + userId + " is typing");
  // The ChatStore handles UI updates for typing indicators
}

void AppStore::addChannelToState(const juce::String &channelId, const juce::String &channelName) {
  Log::info("AppStore::addChannelToState: Adding channel " + channelId);

  sliceManager.getChatSlice()->dispatch([channelId, channelName](ChatState &state) {
    ChannelState channelState;
    channelState.id = channelId;
    channelState.name = channelName;
    state.channels[channelId] = channelState;
  });
}

void AppStore::addMessageToChannel(const juce::String &channelId, const juce::String &messageId,
                                   const juce::String &text, const juce::String &userId, const juce::String &userName,
                                   const juce::String &createdAt) {
  Log::info("AppStore::addMessageToChannel: Adding message " + messageId + " to channel " + channelId);

  sliceManager.getChatSlice()->dispatch([channelId, messageId, text, userId, userName, createdAt](ChatState &state) {
    auto channelIt = state.channels.find(channelId);
    if (channelIt != state.channels.end()) {
      // Create Message object
      auto msg = std::make_shared<Message>();
      msg->id = messageId;
      msg->conversationId = channelId;
      msg->text = text;
      msg->senderId = userId;
      msg->senderUsername = userName;
      msg->createdAt = juce::Time::fromISO8601(createdAt);

      channelIt->second.messages.push_back(msg);
      Log::info("AppStore::addMessageToChannel: Added message to channel state");
    } else {
      Log::warn("AppStore::addMessageToChannel: Channel not found in state - " + channelId);
    }
  });
}

} // namespace Stores
} // namespace Sidechain
