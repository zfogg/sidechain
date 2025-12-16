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
  // Chat functionality is not yet implemented in AppStore
  // The ChatStore handles this separately
  Util::logWarning("AppStore", "sendMessage not implemented - use ChatStore");
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

} // namespace Stores
} // namespace Sidechain
