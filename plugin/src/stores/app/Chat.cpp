#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadChannels() {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Util::logWarning("AppStore", "loadChannels not implemented - use ChatStore");
}

void AppStore::selectChannel(const juce::String &channelId) {
  updateChatState([channelId](ChatState &state) { state.currentChannelId = channelId; });
}

void AppStore::loadMessages(const juce::String &channelId, int limit) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Util::logWarning("AppStore", "loadMessages not implemented - use ChatStore");
}

void AppStore::sendMessage(const juce::String &channelId, const juce::String &text) {
  if (!networkClient) {
    Util::logError("AppStore", "sendMessage: NetworkClient not available");
    return;
  }

  Util::logInfo("AppStore", "sendMessage: Sending message to channel " + channelId);

  // Make REST call to backend messaging API
  // This will eventually call streamChatClient->sendMessage internally
  // For now, we'll implement basic message storage

  // Create a new message object
  juce::var msgObj(new juce::DynamicObject());
  auto *obj = msgObj.getDynamicObject();
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
      Util::logInfo("AppStore", "sendMessage: Added message to channel state");
    }
  });
}

void AppStore::startTyping(const juce::String &channelId) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Util::logDebug("AppStore", "startTyping not implemented - use ChatStore");
}

void AppStore::stopTyping(const juce::String &channelId) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Util::logDebug("AppStore", "stopTyping not implemented - use ChatStore");
}

void AppStore::handleNewMessage(const juce::var &messageData) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Util::logDebug("AppStore", "handleNewMessage not implemented - use ChatStore");
}

void AppStore::handleTypingStart(const juce::String &userId) {
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Util::logDebug("AppStore", "handleTypingStart not implemented - use ChatStore");
}

void AppStore::addChannelToState(const juce::String &channelId, const juce::String &channelName) {
  Util::logInfo("AppStore", "addChannelToState: Adding channel " + channelId);

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
  Util::logInfo("AppStore", "addMessageToChannel: Adding message " + messageId + " to channel " + channelId);

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
      Util::logInfo("AppStore", "addMessageToChannel: Added message to channel state");
    } else {
      Util::logWarning("AppStore", "addMessageToChannel: Channel not found in state - " + channelId);
    }
  });
}

} // namespace Stores
} // namespace Sidechain
