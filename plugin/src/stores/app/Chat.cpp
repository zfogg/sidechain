#include "../AppStore.h"
#include "../../util/Log.h"

namespace Sidechain {
namespace Stores {

void AppStore::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
  Log::info("AppStore::setStreamChatClient: StreamChatClient set: " + juce::String(client != nullptr ? "valid" : "null"));
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
  streamChatClient->queryMessages("messaging", channelId, limit, 0,
                                  [this, channelId](Outcome<std::vector<StreamChatClient::Message>> messagesResult) {
                                    if (!messagesResult.isOk()) {
                                      Log::error("AppStore::loadMessages: Failed to load messages - " +
                                                 messagesResult.getError());
                                      return;
                                    }

                                    const auto &messages = messagesResult.getValue();
                                    Log::info("AppStore::loadMessages: Loaded " + juce::String(messages.size()) +
                                              " messages for " + channelId);

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
  const auto &chatState = getState().chat;
  auto channelIt = chatState.channels.find(channelId);
  if (channelIt == chatState.channels.end()) {
    Log::error("AppStore::sendMessage - ERROR: Channel not found in state: " + channelId);
    return;
  }
  Log::info("AppStore::sendMessage - ✓ Channel found in state");

  if (!networkClient) {
    Log::warn("AppStore::sendMessage - WARNING: NetworkClient not available, creating local message only");
  }

  Log::info("AppStore::sendMessage - Creating message object");

  // Create message using new with proper lifecycle management
  auto obj = new juce::DynamicObject();
  if (!obj) {
    Log::error("AppStore::sendMessage - SEGFAULT RISK: Failed to allocate DynamicObject!");
    return;
  }

  juce::String userId = getState().user.userId;
  juce::String username = getState().user.username;

  Log::info("AppStore::sendMessage - ✓ Setting properties - userId: " + userId + ", username: " + username);

  juce::var msgObj(obj);
  obj->setProperty("id", juce::Uuid().toString());
  obj->setProperty("text", text);
  obj->setProperty("user_id", userId);
  obj->setProperty("user_name", username);
  obj->setProperty("created_at", juce::Time::getCurrentTime().toISO8601(true));

  Log::info("AppStore::sendMessage - ✓ Message object created successfully");

  // Add message to chat state
  Log::info("AppStore::sendMessage - Calling updateChatState to add message");
  sliceManager.getChatSlice()->dispatch([channelId, msgObj](ChatState &state) {
    Log::debug("AppStore::sendMessage lambda - Inside updateChatState lambda");
    auto channelIt = state.channels.find(channelId);
    if (channelIt != state.channels.end()) {
      Log::debug("AppStore::sendMessage lambda - Channel found, adding message");
      channelIt->second.messages.push_back(msgObj);
      Log::info("AppStore::sendMessage - ✓ Added message to channel state");
    } else {
      Log::error("AppStore::sendMessage lambda - ERROR: Channel disappeared from state!");
    }
  });

  Log::info("AppStore::sendMessage - ✓ Message send complete");
}

void AppStore::startTyping(const juce::String &channelId) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Log::debug("AppStore::startTyping: not implemented - use ChatStore");
}

void AppStore::stopTyping(const juce::String &channelId) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Log::debug("AppStore::stopTyping: not implemented - use ChatStore");
}

void AppStore::handleNewMessage(const juce::var &messageData) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Log::debug("AppStore::handleNewMessage: not implemented - use ChatStore");
}

void AppStore::handleTypingStart(const juce::String &userId) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Log::debug("AppStore::handleTypingStart: not implemented - use ChatStore");
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
                                   const juce::String &text, const juce::String &userId,
                                   const juce::String &userName, const juce::String &createdAt) {
  Log::info("AppStore::addMessageToChannel: Adding message " + messageId + " to channel " + channelId);

  sliceManager.getChatSlice()->dispatch([channelId, messageId, text, userId, userName, createdAt](ChatState &state) {
    auto channelIt = state.channels.find(channelId);
    if (channelIt != state.channels.end()) {
      // Create message object
      juce::var msgObj(new juce::DynamicObject());
      auto *obj = msgObj.getDynamicObject();
      obj->setProperty("id", messageId);
      obj->setProperty("text", text);
      obj->setProperty("user_id", userId);
      obj->setProperty("user_name", userName);
      obj->setProperty("created_at", createdAt);

      channelIt->second.messages.push_back(msgObj);
      Log::info("AppStore::addMessageToChannel: Added message to channel state");
    } else {
      Log::warn("AppStore::addMessageToChannel: Channel not found in state - " + channelId);
    }
  });
}

} // namespace Stores
} // namespace Sidechain
