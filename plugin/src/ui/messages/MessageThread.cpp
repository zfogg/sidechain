#include "MessageThread.h"
#include "../../util/Log.h"
#include "AudioSnippetRecorder.h"
#include "UserPickerDialog.h"

MessageThread::MessageThread(Sidechain::Stores::AppStore *store)
    : Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChatState>(store), scrollBar(true) {
  Log::info("MessageThread: Initializing");
  addAndMakeVisible(scrollBar);
}

MessageThread::~MessageThread() {
  Log::debug("MessageThread: Destroying");
}

void MessageThread::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
  g.setColour(juce::Colours::white);
  g.drawText("Message Thread - TODO", getLocalBounds(), juce::Justification::centred);
}

void MessageThread::resized() {
  scrollBar.setBounds(getWidth() - scrollBar.getWidth(), 0, scrollBar.getWidth(), getHeight());
}

void MessageThread::mouseUp(const juce::MouseEvent &event) {}
void MessageThread::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {}
void MessageThread::textEditorReturnKeyPressed(juce::TextEditor &editor) {}
void MessageThread::textEditorTextChanged(juce::TextEditor &editor) {}

void MessageThread::setStreamChatClient(StreamChatClient *client) {}
void MessageThread::setNetworkClient(NetworkClient *client) {}
void MessageThread::setAudioProcessor(SidechainAudioProcessor *processor) {}
void MessageThread::loadChannel(const juce::String &type, const juce::String &id) {}

void MessageThread::onAppStateChanged(const Sidechain::Stores::ChatState &state) {
  repaint();
}

void MessageThread::drawMessages(juce::Graphics &g) {}
void MessageThread::drawMessageBubble(juce::Graphics &g, const StreamChatClient::Message &message, int &y, int width) {}
void MessageThread::drawMessageReactions(juce::Graphics &g, const StreamChatClient::Message &message, int &y, int x,
                                         int maxWidth) {}
void MessageThread::drawEmptyState(juce::Graphics &g) {}
void MessageThread::drawErrorState(juce::Graphics &g) {}
void MessageThread::drawInputArea(juce::Graphics &g) {}

void MessageThread::sendMessage() {}
void MessageThread::loadMessages() {}
void MessageThread::cancelReply() {}
void MessageThread::reportMessage(const StreamChatClient::Message &message) {}
void MessageThread::blockUser(const StreamChatClient::Message &message) {}

void MessageThread::showQuickReactionPicker(const StreamChatClient::Message &message,
                                            const juce::Point<int> &screenPos) {}
void MessageThread::addReaction(const juce::String &messageId, const juce::String &reactionType) {}
void MessageThread::removeReaction(const juce::String &messageId, const juce::String &reactionType) {}
void MessageThread::toggleReaction(const juce::String &messageId, const juce::String &reactionType) {}

bool MessageThread::hasUserReacted(const StreamChatClient::Message &message, const juce::String &reactionType) const {
  return false;
}
int MessageThread::getReactionCount(const StreamChatClient::Message &message, const juce::String &reactionType) const {
  return 0;
}
std::vector<juce::String> MessageThread::getReactionTypes(const StreamChatClient::Message &message) const {
  return {};
}

juce::Rectangle<int> MessageThread::getBackButtonBounds() const {
  return {};
}
juce::Rectangle<int> MessageThread::getSendButtonBounds() const {
  return {};
}
juce::Rectangle<int> MessageThread::getAudioButtonBounds() const {
  return {};
}
juce::Rectangle<int> MessageThread::getMessageBounds(const StreamChatClient::Message &message) const {
  return {};
}
juce::Rectangle<int> MessageThread::getHeaderMenuButtonBounds() const {
  return {};
}
juce::Rectangle<int> MessageThread::getSharedPostBounds(const StreamChatClient::Message &message) const {
  return {};
}
juce::Rectangle<int> MessageThread::getSharedStoryBounds(const StreamChatClient::Message &message) const {
  return {};
}
juce::Rectangle<int> MessageThread::getReplyPreviewBounds() const {
  return {};
}
juce::Rectangle<int> MessageThread::getCancelReplyButtonBounds() const {
  return {};
}

void MessageThread::leaveGroup() {}
void MessageThread::renameGroup() {}
void MessageThread::showAddMembersDialog() {}
void MessageThread::showRemoveMembersDialog() {}
bool MessageThread::isGroupChannel() const {
  return false;
}

void MessageThread::showMessageActionsMenu(const StreamChatClient::Message &message,
                                           const juce::Point<int> &screenPos) {}
void MessageThread::copyMessageText(const juce::String &text) {}
void MessageThread::editMessage(const StreamChatClient::Message &message) {}
void MessageThread::deleteMessage(const StreamChatClient::Message &message) {}
void MessageThread::replyToMessage(const StreamChatClient::Message &message) {}

juce::String MessageThread::formatTimestamp(const juce::String &timestamp) {
  return "";
}
int MessageThread::calculateMessageHeight(const StreamChatClient::Message &message, int maxWidth) const {
  return 50;
}
int MessageThread::calculateTotalMessagesHeight() {
  return 0;
}
bool MessageThread::isOwnMessage(const StreamChatClient::Message &message) const {
  return false;
}

juce::String MessageThread::getReplyToMessageId(const StreamChatClient::Message &message) const {
  return "";
}
std::optional<StreamChatClient::Message> MessageThread::findParentMessage(const juce::String &messageId) const {
  return std::nullopt;
}
void MessageThread::scrollToMessage(const juce::String &messageId) {}

bool MessageThread::hasSharedPost(const StreamChatClient::Message &message) const {
  return false;
}
bool MessageThread::hasSharedStory(const StreamChatClient::Message &message) const {
  return false;
}
void MessageThread::drawSharedPostPreview(juce::Graphics &g, const StreamChatClient::Message &message,
                                          juce::Rectangle<int> bounds) {}
void MessageThread::drawSharedStoryPreview(juce::Graphics &g, const StreamChatClient::Message &message,
                                           juce::Rectangle<int> bounds) {}
int MessageThread::getSharedContentHeight(const StreamChatClient::Message &message) const {
  return 0;
}

void MessageThread::timerCallback() {}
void MessageThread::drawHeader(juce::Graphics &g) {}
