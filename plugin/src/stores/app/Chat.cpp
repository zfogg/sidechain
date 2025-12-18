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
  updateChatState([channelId](ChatState &state) { state.currentChannelId = channelId; });
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
  if (!networkClient) {
    Log::error("AppStore::sendMessage: NetworkClient not available");
    return;
  }

  Log::info("AppStore::sendMessage: Sending message to channel " + channelId);

  // Make REST call to backend messaging API
  // This will eventually call streamChatClient->sendMessage internally
  // For now, we'll implement basic message storage

  // Create a new message object using direct construction
  auto obj = std::make_unique<juce::DynamicObject>();
  if (!obj) {
    Log::error("AppStore::sendMessage: Failed to create message object");
    return;
  }
  juce::var msgObj(obj.get());
  obj->setProperty("id", juce::Uuid().toString());
  obj->setProperty("text", text);
  obj->setProperty("user_id", getState().user.userId);
  obj->setProperty("user_name", getState().user.username);
  obj->setProperty("created_at", juce::Time::getCurrentTime().toISO8601(true));

  // Add message to chat state
  updateChatState([channelId, msgObj](ChatState &state) {
    auto channelIt = state.channels.find(channelId);
    if (channelIt != state.channels.end()) {
      channelIt->second.messages.push_back(msgObj);
      Log::info("AppStore::sendMessage: Added message to channel state");
    }
  });
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

  updateChatState([channelId, channelName](ChatState &state) {
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

  updateChatState([channelId, messageId, text, userId, userName, createdAt](ChatState &state) {
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
