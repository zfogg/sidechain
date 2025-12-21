#include "MessageThread.h"
#include "../../util/Log.h"
#include "AudioSnippetRecorder.h"
#include "UserPickerDialog.h"

MessageThread::MessageThread(Sidechain::Stores::AppStore *store)
    : Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChatState>(
          store, [store](auto cb) { return store ? store->subscribeToChat(cb) : std::function<void()>([]() {}); }),
      scrollBar(true) {
  Log::info("MessageThread: Initializing");
  addAndMakeVisible(scrollBar);

  // Start timer for typing indicator animation (update every 250ms)
  startTimer(250);
}

MessageThread::~MessageThread() {
  stopTimer();
  Log::debug("MessageThread: Destroying");
}

void MessageThread::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
  drawHeader(g);
  drawMessages(g);
  drawTypingIndicator(g);
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

void MessageThread::drawTypingIndicator(juce::Graphics &g) {
  // Get current channel state from store
  if (!appStore)
    return;

  auto state = appStore->getChatState();
  if (state.currentChannelId.isEmpty())
    return;

  auto it = state.channels.find(state.currentChannelId);
  if (it == state.channels.end() || it->second.usersTyping.empty())
    return;

  // Position typing indicator above input area
  int inputHeight = INPUT_HEIGHT;
  int replyHeight = replyingToMessageId.isNotEmpty() ? REPLY_PREVIEW_HEIGHT : 0;
  int typingY = getHeight() - inputHeight - replyHeight - 40;

  // Draw typing indicator with animated dots
  auto typingBounds = juce::Rectangle<int>(12, typingY, getWidth() - 24, 30);

  g.setColour(juce::Colour(0xff888888));
  g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));

  // Build typing user names
  juce::String typingText;
  const auto &usersTyping = it->second.usersTyping;

  if (usersTyping.size() == 1) {
    typingText = usersTyping[0] + " is typing";
  } else if (usersTyping.size() == 2) {
    typingText = usersTyping[0] + " and " + usersTyping[1] + " are typing";
  } else if (usersTyping.size() > 2) {
    typingText = juce::String(usersTyping.size()) + " users are typing";
  }

  // Draw animated dots
  int64_t currentTime = juce::Time::currentTimeMillis();
  int dotPhase = static_cast<int>((currentTime / 250) % 4); // 4 phases: dot1, dot2, dot3, none

  juce::String dots;
  for (int i = 1; i <= 3; ++i) {
    dots += (i <= dotPhase) ? "•" : " ";
  }

  g.drawText(typingText + " " + dots, typingBounds, juce::Justification::centredLeft);
}

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

bool MessageThread::hasUserReacted(const StreamChatClient::Message &message, const juce::String &reactionType) const {
  // Check if current user has reacted with this reaction type
  // Reactions structure: { "👍": ["user1", "user2"], "❤️": ["user3"] }
  if (!message.reactions.isObject()) {
    return false;
  }

  auto reactionArray = message.reactions.getProperty(reactionType, juce::var());
  if (!reactionArray.isArray()) {
    return false;
  }

  // Check if currentUserId is in the reaction array
  for (int i = 0; i < reactionArray.size(); ++i) {
    if (reactionArray[i].toString() == currentUserId) {
      return true;
    }
  }

  return false;
}
int MessageThread::getReactionCount(const StreamChatClient::Message &message, const juce::String &reactionType) const {
  // Return count of users who reacted with this reaction type
  if (!message.reactions.isObject()) {
    return 0;
  }

  auto reactionArray = message.reactions.getProperty(reactionType, juce::var());
  if (!reactionArray.isArray()) {
    return 0;
  }

  return reactionArray.size();
}
std::vector<juce::String> MessageThread::getReactionTypes(const StreamChatClient::Message &message) const {
  // Return all reaction types (emoji) on this message
  std::vector<juce::String> types;

  if (!message.reactions.isObject()) {
    return types;
  }

  // Iterate through all properties in the reactions object to get reaction types
  auto &properties = message.reactions.getDynamicObject()->getProperties();
  for (int i = 0; i < properties.size(); ++i) {
    types.push_back(properties.getName(i).toString());
  }

  return types;
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
  return juce::Rectangle<int>(getWidth() - (BUTTON_SIZE * 2) - (PADDING * 2), inputAreaY + PADDING, BUTTON_SIZE,
                              BUTTON_SIZE);
}
juce::Rectangle<int> MessageThread::getMessageBounds(const StreamChatClient::Message & /*message*/) const {
  // Placeholder: Return bounds for message in the messages area
  // In a full implementation, this would track actual message positions from layout
  constexpr int PADDING = MESSAGE_BUBBLE_PADDING;
  int messagesAreaY = HEADER_HEIGHT + MESSAGE_TOP_PADDING;

  // Return a default bounds that spans the messages area width
  // Actual height/position would require full message layout tracking
  return juce::Rectangle<int>(PADDING, messagesAreaY, getWidth() - (PADDING * 2), MESSAGE_BUBBLE_MIN_HEIGHT);
}
juce::Rectangle<int> MessageThread::getHeaderMenuButtonBounds() const {
  // Menu button in top-right corner of header
  constexpr int BUTTON_SIZE = 40;
  constexpr int PADDING = 10;
  return juce::Rectangle<int>(getWidth() - BUTTON_SIZE - PADDING, PADDING, BUTTON_SIZE, BUTTON_SIZE);
}
juce::Rectangle<int>
MessageThread::getSharedPostBounds([[maybe_unused]] const StreamChatClient::Message &message) const {
  // Post preview bounds within message bubble
  constexpr int PADDING = 10;
  constexpr int PREVIEW_HEIGHT = 100;
  return juce::Rectangle<int>(PADDING, PADDING, MESSAGE_MAX_WIDTH - (PADDING * 2), PREVIEW_HEIGHT);
}
juce::Rectangle<int>
MessageThread::getSharedStoryBounds([[maybe_unused]] const StreamChatClient::Message &message) const {
  // Story preview bounds within message bubble (square thumbnail for story)
  constexpr int PADDING = 10;
  constexpr int PREVIEW_SIZE = 80;
  return juce::Rectangle<int>(PADDING, PADDING, PREVIEW_SIZE, PREVIEW_SIZE);
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

// ==============================================================================
// Audio Attachment Playback

bool MessageThread::hasAudioAttachment(const StreamChatClient::Message & /* message */) const {
  // Check if message has audio attachments
  // Note: Will be populated from Message.attachments when integrated
  return false; // TODO: Check message.attachments for type == "audio"
}

void MessageThread::drawAudioAttachment(juce::Graphics &g, const StreamChatClient::Message & /*message*/,
                                        juce::Rectangle<int> bounds) {
  // Draw audio player control within message bubble
  // Layout:
  // - Play/pause button (left)
  // - Progress bar (center)
  // - Duration label (right)

  g.setColour(juce::Colour(0xff333333));
  g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

  constexpr int BUTTON_SIZE = 30;
  constexpr int PADDING = 8;

  // Play/pause button
  bounds.removeFromLeft(BUTTON_SIZE + PADDING).withTrimmedRight(PADDING);
  auto isPlaying = playingAudioId.isNotEmpty();

  // Draw play button or pause icon
  juce::Path playPath;
  if (isPlaying) {
    // Draw pause icon (two vertical bars)
    playPath.addRectangle(2, 2, 4, 10);
    playPath.addRectangle(7, 2, 4, 10);
  } else {
    // Draw play icon (triangle)
    playPath.startNewSubPath(2, 2);
    playPath.lineTo(2, 12);
    playPath.lineTo(10, 7);
    playPath.closeSubPath();
  }

  g.setColour(juce::Colour(0xffffffff));
  auto transform = juce::AffineTransform::scale(1.2f);
  g.fillPath(playPath, transform);

  // Progress bar
  auto progressBounds = bounds.removeFromLeft(bounds.getWidth() - 50);
  g.setColour(juce::Colour(0xff555555));
  g.fillRoundedRectangle(progressBounds.reduced(2).toFloat(), 3.0f);

  // Playback progress
  if (audioPlaybackProgress > 0.0) {
    auto fillBounds = progressBounds.reduced(2);
    fillBounds.setWidth(static_cast<int>(fillBounds.getWidth() * audioPlaybackProgress));
    g.setColour(juce::Colour(0xff1DB954)); // Spotify green
    g.fillRoundedRectangle(fillBounds.toFloat(), 3.0f);
  }

  // Duration label
  g.setColour(juce::Colour(0xffcccccc));
  g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
  g.drawText("0:00", bounds, juce::Justification::centredRight);
}

void MessageThread::playAudioAttachment(const StreamChatClient::Message &message) {
  // Play audio attachment from message
  if (!audioPlayer)
    return;

  // Find audio attachment URL
  // TODO: Integrate with Message.attachments when available
  // For now, just mark as playing
  playingAudioId = message.id;
  repaint();
}

void MessageThread::pauseAudioPlayback() {
  // Note: audioPlayer is forward-declared, so we can't call methods on it directly
  // This is intentional - the actual pause() call will be in the derived implementation
  // when HttpAudioPlayer is fully integrated
  playingAudioId = "";
  repaint();
}

int MessageThread::getAudioAttachmentHeight() const {
  return 40; // Height of audio player control
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
std::optional<StreamChatClient::Message> MessageThread::findParentMessage(const juce::String &parentId) const {
  // Search through message history to find the parent message by ID
  if (parentId.isEmpty()) {
    return std::nullopt;
  }

  if (!appStore) {
    return std::nullopt;
  }

  // Get current ChatState from AppStore
  const auto &chatState = appStore->getChatState();

  // Find the current channel in the state
  auto channelIt = chatState.channels.find(channelId);
  if (channelIt == chatState.channels.end()) {
    return std::nullopt;
  }

  // Search through messages in the current channel
  const auto &channelState = channelIt->second;
  for (const auto &message : channelState.messages) {
    if (message && message->id == parentId) {
      // Convert Sidechain::Message to StreamChatClient::Message
      StreamChatClient::Message result;
      result.id = message->id;
      result.userId = message->senderId;
      result.userName = message->senderUsername;
      result.text = message->text;
      result.createdAt = message->createdAt.toString(true, true, true);
      result.isDeleted = message->isDeleted;

      // Store reply info in extraData if this message itself is a reply
      if (message->replyToId.isNotEmpty()) {
        auto *extraObj = new juce::DynamicObject();
        extraObj->setProperty("reply_to_id", message->replyToId);
        extraObj->setProperty("reply_to_sender", message->replyToSenderId);
        result.extraData = juce::var(extraObj);
      }

      return result;
    }
  }

  return std::nullopt;
}
void MessageThread::scrollToMessage(const juce::String &messageId) {
  // Scroll scrollbar to show message with given ID
  if (messageId.isEmpty() || !appStore) {
    return;
  }

  // Get current ChatState
  const auto &chatState = appStore->getChatState();

  // Find the current channel
  auto channelIt = chatState.channels.find(channelId);
  if (channelIt == chatState.channels.end()) {
    return;
  }

  // Calculate position of message in the message list
  int messagePosition = 0;
  const auto &channelState = channelIt->second;

  for (size_t i = 0; i < channelState.messages.size(); ++i) {
    if (channelState.messages[i] && channelState.messages[i]->id == messageId) {
      // Found the message - calculate its Y position
      // Add heights of all non-null messages before this one
      for (size_t j = 0; j < i; ++j) {
        if (channelState.messages[j]) {
          // Calculate approximate message height based on content
          int messageHeight = 60; // Base height for single-line message

          // Add extra height for multi-line text
          const auto &msg = channelState.messages[j];
          if (msg && msg->text.length() > 50) {
            // Estimate ~2 lines per 50 characters, ~25 pixels per line
            messageHeight += ((msg->text.length() / 50) * 25);
          }

          // Add height for attachments
          if (!msg->attachments.empty()) {
            messageHeight += (msg->attachments.size() * 100); // ~100px per attachment
          }

          messagePosition += messageHeight;
        }
      }
      // Scroll to this position
      scrollBar.setCurrentRangeStart(messagePosition, juce::sendNotification);
      repaint();
      return;
    }
  }
}

bool MessageThread::hasSharedPost(const StreamChatClient::Message &message) const {
  return message.extraData.hasProperty("post_id");
}
bool MessageThread::hasSharedStory(const StreamChatClient::Message &message) const {
  return message.extraData.hasProperty("story_id");
}

void MessageThread::drawSharedPostPreview(juce::Graphics &g, const StreamChatClient::Message &message,
                                          juce::Rectangle<int> bounds) {
  // Draw post preview within bounds - includes thumbnail, title, artist
  if (!message.extraData.hasProperty("post_id")) {
    return;
  }

  // Background
  g.setColour(juce::Colour(0xff2a2a2a));
  g.fillRect(bounds);

  // Border
  g.setColour(juce::Colour(0xff444444));
  g.drawRect(bounds, 1);

  // Extract post data from extraData
  juce::String postId = message.extraData.getProperty("post_id", "").toString();
  juce::String postTitle = message.extraData.getProperty("post_title", "").toString();
  juce::String artistName = message.extraData.getProperty("artist_name", "").toString();

  // Draw icon (♪ music note)
  auto iconBounds = bounds.removeFromLeft(bounds.getHeight()).reduced(8);
  g.setColour(juce::Colour(0xff555555));
  g.drawText(juce::String::fromUTF8("\xe2\x99\xaa"), iconBounds, juce::Justification::centred);

  // Draw title and artist
  auto textBounds = bounds.reduced(8, 4);
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
  g.drawText(postTitle.isEmpty() ? "Post" : postTitle, textBounds.removeFromTop(14), juce::Justification::topLeft);

  g.setColour(juce::Colour(0xffaaaaaa));
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
  g.drawText(artistName.isEmpty() ? "Unknown Artist" : artistName, textBounds, juce::Justification::topLeft);
}

void MessageThread::drawSharedStoryPreview(juce::Graphics &g, const StreamChatClient::Message &message,
                                           juce::Rectangle<int> bounds) {
  // Draw story preview within bounds - includes thumbnail with story gradient
  if (!message.extraData.hasProperty("story_id")) {
    return;
  }

  // Background with gradient
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5500ff), bounds.getTopLeft().toFloat(),
                                         juce::Colour(0xff0099ff), bounds.getBottomRight().toFloat(), false));
  g.fillRect(bounds);

  // Border
  g.setColour(juce::Colour(0xffffffff));
  g.drawRect(bounds, 2);

  // Extract story data
  juce::String storyId = message.extraData.getProperty("story_id", "").toString();
  juce::String storyAuthor = message.extraData.getProperty("story_author", "").toString();

  // Draw story label
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
  g.drawText("Story", bounds.reduced(8), juce::Justification::topLeft);

  // Draw author name at bottom
  if (!storyAuthor.isEmpty()) {
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText(storyAuthor, bounds.reduced(8), juce::Justification::bottomLeft);
  }
}

int MessageThread::getSharedContentHeight(const StreamChatClient::Message &message) const {
  if (message.extraData.hasProperty("post_id") || message.extraData.hasProperty("story_id")) {
    return 100; // Standard preview height
  }
  return 0;
}

void MessageThread::timerCallback() {
  // Handle periodic updates - repaint for animations and typing indicators
  // Check if any users are typing and update display
  if (appStore) {
    const auto &chatState = appStore->getChatState();
    auto channelIt = chatState.channels.find(channelId);
    if (channelIt != chatState.channels.end() && !channelIt->second.usersTyping.empty()) {
      repaint(); // Animate typing indicator
    }
  }
}

void MessageThread::drawHeader(juce::Graphics &g) {
  // Draw channel header with back button, channel name/avatar, and menu button
  auto headerBounds = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);

  // Background
  g.setColour(juce::Colour(0xff0a0a0a));
  g.fillRect(headerBounds);

  // Border
  g.setColour(juce::Colour(0xff333333));
  g.drawLine(0.0f, static_cast<float>(HEADER_HEIGHT), static_cast<float>(getWidth()), static_cast<float>(HEADER_HEIGHT),
             1.0f);

  // Back button bounds
  auto backButtonBounds = getBackButtonBounds();
  g.setColour(juce::Colour(0xff666666));
  g.drawText("<", backButtonBounds, juce::Justification::centred);

  // Channel name/info
  auto nameBounds = headerBounds.reduced(50, 0);
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));
  g.drawText(channelName, nameBounds, juce::Justification::centredLeft);

  // Typing indicator
  if (appStore) {
    const auto &chatState = appStore->getChatState();
    auto channelIt = chatState.channels.find(channelId);
    if (channelIt != chatState.channels.end() && !channelIt->second.usersTyping.empty()) {
      g.setColour(juce::Colour(0xff888888));
      g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
      g.drawText("typing...", nameBounds.withTop(nameBounds.getCentreY()), juce::Justification::centredLeft);
    }
  }

  // Menu button bounds (⋮ vertical ellipsis)
  auto menuButtonBounds = getHeaderMenuButtonBounds();
  g.setColour(juce::Colour(0xff666666));
  g.drawText(juce::String::fromUTF8("\xe2\x8b\xae"), menuButtonBounds, juce::Justification::centred);
}
