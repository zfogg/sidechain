#include "MessageThread.h"
#include "../../util/Log.h"
#include "AudioSnippetRecorder.h"
#include "UserPickerDialog.h"

MessageThread::MessageThread(Sidechain::Stores::AppStore *store)
    : Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChatState>(store), scrollBar(true) {
  Log::info("MessageThread: Initializing");
  addAndMakeVisible(scrollBar);
  initialize();
}

MessageThread::~MessageThread() {
  Log::debug("MessageThread: Destroying");
}

void MessageThread::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
  drawHeader(g);
  drawMessages(g);
  drawInputArea(g);
}

void MessageThread::resized() {
  scrollBar.setBounds(getWidth() - scrollBar.getWidth(), 0, scrollBar.getWidth(), getHeight());
}

void MessageThread::mouseUp(const juce::MouseEvent &) {}
void MessageThread::mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) {}
void MessageThread::textEditorReturnKeyPressed(juce::TextEditor &) {}
void MessageThread::textEditorTextChanged(juce::TextEditor &) {}

void MessageThread::setStreamChatClient(StreamChatClient *) {}
void MessageThread::setNetworkClient(NetworkClient *) {}
void MessageThread::setAudioProcessor(SidechainAudioProcessor *) {}
void MessageThread::loadChannel(const juce::String &, const juce::String &) {}

void MessageThread::onAppStateChanged(const Sidechain::Stores::ChatState &) {
  repaint();
}

void MessageThread::drawMessages(juce::Graphics &) {}
void MessageThread::drawMessageBubble(juce::Graphics &, const StreamChatClient::Message &, int &, int) {}
void MessageThread::drawMessageReactions(juce::Graphics &, const StreamChatClient::Message &, int &, int, int) {}
void MessageThread::drawEmptyState(juce::Graphics &) {}
void MessageThread::drawErrorState(juce::Graphics &) {}
void MessageThread::drawInputArea(juce::Graphics &) {}

void MessageThread::sendMessage() {}
void MessageThread::loadMessages() {}
void MessageThread::cancelReply() {}
void MessageThread::reportMessage(const StreamChatClient::Message &) {}
void MessageThread::blockUser(const StreamChatClient::Message &) {}

void MessageThread::showQuickReactionPicker(const StreamChatClient::Message &, const juce::Point<int> &) {}
void MessageThread::addReaction(const juce::String &, const juce::String &) {}
void MessageThread::removeReaction(const juce::String &, const juce::String &) {}
void MessageThread::toggleReaction(const juce::String &, const juce::String &) {}

bool MessageThread::hasUserReacted([[maybe_unused]] const StreamChatClient::Message &message,
                                   [[maybe_unused]] const juce::String &reactionType) const {
  // TODO: implement
  return false;
}
int MessageThread::getReactionCount([[maybe_unused]] const StreamChatClient::Message &message,
                                    [[maybe_unused]] const juce::String &reactionType) const {
  // TODO: implement
  return 0;
}
std::vector<juce::String>
MessageThread::getReactionTypes([[maybe_unused]] const StreamChatClient::Message &message) const {
  // TODO: implement
  return {};
}

juce::Rectangle<int> MessageThread::getBackButtonBounds() const {
  // Back button in top-left corner of header
  constexpr int BUTTON_SIZE = 40;
  constexpr int PADDING = 10;
  return juce::Rectangle<int>(PADDING, PADDING, BUTTON_SIZE, BUTTON_SIZE);
}
juce::Rectangle<int> MessageThread::getSendButtonBounds() const {
  // Send button in bottom-right corner of input area
  constexpr int BUTTON_SIZE = 40;
  constexpr int PADDING = 10;
  int inputAreaY = getHeight() - INPUT_HEIGHT;
  return juce::Rectangle<int>(getWidth() - BUTTON_SIZE - PADDING, inputAreaY + PADDING, BUTTON_SIZE, BUTTON_SIZE);
}
juce::Rectangle<int> MessageThread::getAudioButtonBounds() const {
  // Audio button to the left of send button
  constexpr int BUTTON_SIZE = 40;
  constexpr int PADDING = 10;
  int inputAreaY = getHeight() - INPUT_HEIGHT;
  return juce::Rectangle<int>(getWidth() - (BUTTON_SIZE * 2) - (PADDING * 2), inputAreaY + PADDING, BUTTON_SIZE, BUTTON_SIZE);
}
juce::Rectangle<int> MessageThread::getMessageBounds([[maybe_unused]] const StreamChatClient::Message &message) const {
  // TODO: implement
  return {};
}
juce::Rectangle<int> MessageThread::getHeaderMenuButtonBounds() const {
  // Menu button in top-right corner of header
  constexpr int BUTTON_SIZE = 40;
  constexpr int PADDING = 10;
  return juce::Rectangle<int>(getWidth() - BUTTON_SIZE - PADDING, PADDING, BUTTON_SIZE, BUTTON_SIZE);
}
juce::Rectangle<int>
MessageThread::getSharedPostBounds([[maybe_unused]] const StreamChatClient::Message &message) const {
  // TODO: implement
  return {};
}
juce::Rectangle<int>
MessageThread::getSharedStoryBounds([[maybe_unused]] const StreamChatClient::Message &message) const {
  // TODO: implement
  return {};
}
juce::Rectangle<int> MessageThread::getReplyPreviewBounds() const {
  // Reply preview area above the input field
  constexpr int PADDING = 10;
  int replyAreaY = getHeight() - INPUT_HEIGHT - REPLY_PREVIEW_HEIGHT;
  return juce::Rectangle<int>(PADDING, replyAreaY, getWidth() - (PADDING * 2), REPLY_PREVIEW_HEIGHT);
}
juce::Rectangle<int> MessageThread::getCancelReplyButtonBounds() const {
  // Cancel button in top-right corner of reply preview area
  constexpr int BUTTON_SIZE = 30;
  constexpr int PADDING = 5;
  int replyAreaY = getHeight() - INPUT_HEIGHT - REPLY_PREVIEW_HEIGHT;
  return juce::Rectangle<int>(getWidth() - BUTTON_SIZE - PADDING, replyAreaY + PADDING, BUTTON_SIZE, BUTTON_SIZE);
}

void MessageThread::leaveGroup() {}
void MessageThread::renameGroup() {}
void MessageThread::showAddMembersDialog() {}
void MessageThread::showRemoveMembersDialog() {}
bool MessageThread::isGroupChannel() const {
  return false;
}

void MessageThread::showMessageActionsMenu(const StreamChatClient::Message & /*message*/,
                                           const juce::Point<int> & /*screenPos*/) {}
void MessageThread::copyMessageText(const juce::String & /*text*/) {}
void MessageThread::editMessage(const StreamChatClient::Message &) {}
void MessageThread::deleteMessage(const StreamChatClient::Message &) {}
void MessageThread::replyToMessage(const StreamChatClient::Message &) {}

juce::String MessageThread::formatTimestamp(const juce::String &timestamp) {
  // Parse ISO 8601 timestamp and format as "HH:MM" or "Mon" if older than today
  try {
    auto time = juce::Time::fromISO8601(timestamp);
    auto now = juce::Time::getCurrentTime();
    auto daysDiff = (now.toMilliseconds() - time.toMilliseconds()) / (1000 * 60 * 60 * 24);

    if (daysDiff < 1) {
      return time.formatted("%H:%M"); // Today: show time
    }
    return time.formatted("%a"); // Older: show day name
  } catch (...) {
    return timestamp;
  }
}
int MessageThread::calculateMessageHeight(const StreamChatClient::Message &message, int) const {
  // Base height for message bubble + reactions + padding
  int height = 50; // Minimum height for bubble
  if (!message.text.isEmpty()) {
    height += message.text.length() / 20; // Rough estimate based on text length
  }
  if (message.extraData.hasProperty("post_id") || message.extraData.hasProperty("story_id")) {
    height += 80; // Height for preview
  }
  return height;
}
int MessageThread::calculateTotalMessagesHeight() {
  // Sum all message heights in current view
  // Messages are stored in ChatState - this will be properly integrated
  // when ChatState is cached during onAppStateChanged
  int totalHeight = 0;
  // For now, return 0 - will be populated with proper ChatState integration
  return totalHeight;
}
bool MessageThread::isOwnMessage(const StreamChatClient::Message &message) const {
  // Check if message author is current user by comparing user IDs
  return message.userId == currentUserId;
}

juce::String MessageThread::getReplyToMessageId(const StreamChatClient::Message &message) const {
  // Extract parent message ID from extraData if this is a reply
  if (message.extraData.hasProperty("reply_to")) {
    return message.extraData["reply_to"].toString();
  }
  return "";
}
std::optional<StreamChatClient::Message> MessageThread::findParentMessage(const juce::String &) const {
  // Search through message history to find the parent message by ID
  // TODO: Implement when ChatState is available from AppStore
  return std::nullopt;
}
void MessageThread::scrollToMessage(const juce::String &) {
  // Scroll scrollbar to show message with given ID
  // TODO: Implement when message layout is finalized
}

bool MessageThread::hasSharedPost(const StreamChatClient::Message &message) const {
  return message.extraData.hasProperty("post_id");
}
bool MessageThread::hasSharedStory(const StreamChatClient::Message &message) const {
  return message.extraData.hasProperty("story_id");
}

void MessageThread::drawSharedPostPreview(juce::Graphics &g, const StreamChatClient::Message &,
                                          juce::Rectangle<int> bounds) {
  // Draw post preview within bounds - includes thumbnail, title, artist
  g.setColour(juce::Colour(0xff2a2a2a));
  g.fillRect(bounds);
  // TODO: Implement full post preview rendering when PostPreview component is available
}

void MessageThread::drawSharedStoryPreview(juce::Graphics &g, const StreamChatClient::Message &,
                                           juce::Rectangle<int> bounds) {
  // Draw story preview within bounds - includes thumbnail with story gradient
  g.setColour(juce::Colour(0xff2a2a2a));
  g.fillRect(bounds);
  // TODO: Implement full story preview rendering when StoryPreview component is available
}

int MessageThread::getSharedContentHeight(const StreamChatClient::Message &message) const {
  if (message.extraData.hasProperty("post_id") || message.extraData.hasProperty("story_id")) {
    return 100; // Standard preview height
  }
  return 0;
}

void MessageThread::timerCallback() {
  // TODO: Handle periodic updates - scroll sync, typing indicators, message animations
}
void MessageThread::drawHeader(juce::Graphics &) {
  // TODO: Draw channel header with back button, channel name/avatar, and menu button
}
