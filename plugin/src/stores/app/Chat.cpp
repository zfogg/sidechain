#include "../AppStore.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadChannels() {
  // TODO: Implement load channels
}

void AppStore::selectChannel(const juce::String &channelId) {
  updateChatState([channelId](ChatState &state) { state.currentChannelId = channelId; });
}

void AppStore::loadMessages(const juce::String &channelId, int limit) {
  // TODO: Implement load messages
}

void AppStore::sendMessage(const juce::String &channelId, const juce::String &text) {
  // TODO: Implement send message
}

void AppStore::startTyping(const juce::String &channelId) {
  // TODO: Implement start typing
}

void AppStore::stopTyping(const juce::String &channelId) {
  // TODO: Implement stop typing
}

void AppStore::handleNewMessage(const juce::var &messageData) {
  // TODO: Implement handle new message
}

void AppStore::handleTypingStart(const juce::String &userId) {
  // TODO: Implement handle typing start
}

} // namespace Stores
} // namespace Sidechain
