#include "MessageThread.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringFormatter.h"
#include "AudioSnippetRecorder.h"
#include "UserPickerDialog.h"
#include "core/PluginProcessor.h"
#include <algorithm>
#include <map>

//==============================================================================
MessageThread::MessageThread(Sidechain::Stores::AppStore *store)
    : Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChatState>(store), scrollBar(true) {
  Log::info("MessageThread: Initializing");

  addAndMakeVisible(scrollBar);
  scrollBar.setRangeLimits(0.0, 0.0);
  scrollBar.addListener(this);

  // Set up message input
  messageInput.setMultiLine(false);
  messageInput.setReturnKeyStartsNewLine(false);
  messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
  messageInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
  messageInput.setColour(juce::TextEditor::textColourId, juce::Colours::white);
  messageInput.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a3a3a));
  messageInput.setColour(juce::TextEditor::focusedOutlineColourId, SidechainColors::primary());
  messageInput.addListener(this);
  addAndMakeVisible(messageInput);

  // Create error state component
  errorStateComponent = std::make_unique<ErrorState>();
  errorStateComponent->setErrorType(ErrorState::ErrorType::Network);
  errorStateComponent->setPrimaryAction("Reconnect", [this]() {
    Log::info("MessageThread: Reconnect requested from error state");
    loadMessages();
  });
  errorStateComponent->setSecondaryAction("Go Back", [this]() {
    Log::info("MessageThread: Go back requested from error state");
    if (onBackPressed)
      onBackPressed();
  });
  addChildComponent(errorStateComponent.get());
  Log::debug("MessageThread: Error state component created");

  startTimer(5000); // Refresh every 5 seconds
}

//==============================================================================
// AppStoreComponent implementation

void MessageThread::onAppStateChanged(const Sidechain::Stores::ChatState &state) {
  // Get current channel from state
  if (channelId.isEmpty())
    return;

  auto it = state.channels.find(channelId);
  if (it != state.channels.end()) {
    const auto &channelState = it->second;
    channelName = channelState.name;

    // Update typing indicators
    // usersTyping comes from channelState.usersTyping

    // Message list comes from channelState.messages
    // Loading state from channelState.isLoadingMessages
  }

  // Update error state
  if (!state.chatError.isEmpty()) {
    if (errorStateComponent) {
      errorStateComponent->configureFromError(state.chatError);
      errorStateComponent->setVisible(true);
    }
  } else {
    if (errorStateComponent) {
      errorStateComponent->setVisible(false);
    }
  }

  repaint();
}

void MessageThread::subscribeToAppStore() {
  if (!appStore)
    return;

  juce::Component::SafePointer<MessageThread> safeThis(this);
  storeUnsubscriber = appStore->subscribeToChat([safeThis](const Sidechain::Stores::ChatState &state) {
    if (!safeThis)
      return;
    juce::MessageManager::callAsync([safeThis, state]() {
      if (safeThis)
        safeThis->onAppStateChanged(state);
    });
  });
}

void MessageThread::setAudioProcessor(SidechainAudioProcessor *processor) {
  audioProcessor = processor;

  if (audioProcessor) {
    // Create audio snippet recorder
    audioSnippetRecorder = std::make_unique<AudioSnippetRecorder>(*audioProcessor);
    audioSnippetRecorder->onRecordingComplete = [this](const juce::AudioBuffer<float> &buffer, double sampleRate) {
      sendAudioSnippet(buffer, sampleRate);
    };
    audioSnippetRecorder->onRecordingCancelled = [this]() {
      showAudioRecorder = false;
      resized();
      repaint();
    };
    addChildComponent(audioSnippetRecorder.get());
  }
}

MessageThread::~MessageThread() {
  Log::debug("MessageThread: Destroying");
  stopTimer();
}

void MessageThread::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
  reactionPills.clear();

  // Get messages from AppStore chat state
  std::vector<StreamChatClient::Message> messages;
  if (appStore) {
    const auto &chatState = appStore->getChatState();
    auto channelIt = chatState.channels.find(channelId);
    if (channelIt != chatState.channels.end()) {
      const auto &channel = channelIt->second;
      // Convert juce::var messages to StreamChatClient::Message objects
      for (const auto &msgVar : channel.messages) {
        if (msgVar.isObject()) {
          auto *obj = msgVar.getDynamicObject();
          StreamChatClient::Message msg;
          msg.id = obj->getProperty("id").toString();
          msg.text = obj->getProperty("text").toString();
          msg.userId = obj->getProperty("user_id").toString();
          msg.userName = obj->getProperty("user_name").toString();
          msg.createdAt = obj->getProperty("created_at").toString();
          messages.push_back(msg);
        }
      }

      // Log when messages are loaded from AppStore
      if (!messages.empty()) {
        Log::info("MessageThread::paint: Loaded " + juce::String(messages.size()) +
                  " messages from AppStore for channel: " + channelId);
      }
    }
  }

  // Draw messages with clipping to prevent overlap with header/scrollbar
  int bottomAreaHeight = INPUT_HEIGHT;
  if (!replyingToMessageId.isEmpty())
    bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
  juce::Rectangle<int> clipArea = getLocalBounds()
                                      .withTrimmedTop(HEADER_HEIGHT)
                                      .withTrimmedBottom(bottomAreaHeight)
                                      .withTrimmedRight(scrollBar.getWidth());
  g.setOrigin(0, 0); // Reset origin for proper clipping
  g.fillRect(clipArea);
  g.saveState();
  g.reduceClipRegion(clipArea);
  drawMessages(g, messages);
  g.restoreState();

  // Draw input area (must be after messages for proper layering)
  drawInputArea(g);

  // Draw header last so it appears on top
  drawHeader(g);

  // If no messages, show placeholder
  if (messages.empty()) {
    g.setColour(juce::Colour(0xff666666));
    g.setFont(12.0f);
    auto messageArea = getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(INPUT_HEIGHT);
    g.drawText("Ready to send messages", messageArea, juce::Justification::centred);
  }
}

void MessageThread::resized() {
  auto bounds = getLocalBounds();

  // Calculate bottom area height (audio recorder + reply preview + input)
  int bottomAreaHeight = INPUT_HEIGHT;
  if (!replyingToMessageId.isEmpty())
    bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
  if (showAudioRecorder && audioSnippetRecorder)
    bottomAreaHeight += AUDIO_RECORDER_HEIGHT;

  // Register scrollbar with SmoothScrollable
  setScrollBar(&scrollBar);

  // Scrollbar on right side of message area (exclude header from top, bottom area from bottom)
  int scrollbarY = HEADER_HEIGHT;
  int scrollbarHeight = getHeight() - HEADER_HEIGHT - bottomAreaHeight;
  int scrollbarX = getWidth() - 12;
  scrollBar.setBounds(scrollbarX, scrollbarY, 12, juce::jmax(0, scrollbarHeight));

  // Audio recorder (if visible)
  if (showAudioRecorder && audioSnippetRecorder) {
    auto recorderArea = bounds.removeFromBottom(AUDIO_RECORDER_HEIGHT);
    audioSnippetRecorder->setBounds(recorderArea);
    audioSnippetRecorder->setVisible(true);
  } else if (audioSnippetRecorder) {
    audioSnippetRecorder->setVisible(false);
  }

  // Message input at bottom (above reply preview if present)
  auto inputArea = bounds.removeFromBottom(INPUT_HEIGHT);
  int padding = 10;
  int sendButtonWidth = 80;
  int audioButtonWidth = 40;
  messageInput.setBounds(inputArea.reduced(padding).withTrimmedRight(sendButtonWidth + audioButtonWidth + padding));

  // Update scrollbar range
  int totalHeight = calculateTotalMessagesHeight();
  int visibleHeight = getHeight() - HEADER_HEIGHT - bottomAreaHeight;
  scrollBar.setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - visibleHeight)));
  scrollBar.setCurrentRangeStart(getScrollPosition(), juce::dontSendNotification);

  // Position error state component in message area
  if (errorStateComponent != nullptr) {
    auto errorArea = getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(INPUT_HEIGHT);
    errorStateComponent->setBounds(errorArea);
  }
}

void MessageThread::mouseUp(const juce::MouseEvent &event) {
  auto pos = event.getPosition();

  Log::debug("MessageThread::mouseUp - Click at (" + juce::String(pos.x) + ", " + juce::String(pos.y) + ")");

  // TODO: Re-implement hit testing once messages are drawn via paint()
  // Currently migration in progress - paint() shows placeholder text

  if (getBackButtonBounds().contains(pos)) {
    Log::debug("MessageThread::mouseUp - Back button clicked");
    if (onBackPressed)
      onBackPressed();
    return;
  }

  // Audio button (toggle audio recorder)
  if (getAudioButtonBounds().contains(pos)) {
    Log::debug("MessageThread::mouseUp - Audio button clicked");
    showAudioRecorder = !showAudioRecorder;
    resized();
    repaint();
    return;
  }

  // Header menu button (for group channels)
  if (isGroupChannel() && getHeaderMenuButtonBounds().contains(pos)) {
    Log::debug("MessageThread::mouseUp - Menu button clicked");
    juce::PopupMenu menu;
    menu.addItem(1, "Add Members");
    menu.addItem(2, "Remove Members");
    menu.addItem(3, "Rename Group");
    menu.addSeparator();
    menu.addItem(4, "Leave Group");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(pos.x, pos.y, 1, 1)),
                       [this](int result) {
                         if (result == 1) {
                           showAddMembersDialog();
                         } else if (result == 2) {
                           showRemoveMembersDialog();
                         } else if (result == 3) {
                           renameGroup();
                         } else if (result == 4) {
                           leaveGroup();
                         }
                       });
    return;
  }

  auto sendBounds = getSendButtonBounds();
  Log::debug("MessageThread::mouseUp - Send button bounds: " + juce::String(sendBounds.getX()) + "," +
             juce::String(sendBounds.getY()) + "," + juce::String(sendBounds.getWidth()) + "," +
             juce::String(sendBounds.getHeight()) +
             ", contains click: " + juce::String(sendBounds.contains(pos) ? "YES" : "NO"));

  if (sendBounds.contains(pos)) {
    Log::info("MessageThread::mouseUp - Send button clicked! Calling sendMessage()");
    sendMessage();
    return;
  }

  // Cancel reply button
  if (!replyingToMessageId.isEmpty()) {
    auto cancelBounds = getCancelReplyButtonBounds();
    if (cancelBounds.contains(pos)) {
      cancelReply();
      return;
    }
  }

  // Check for clicks on reaction pills
  for (const auto &pill : reactionPills) {
    if (pill.bounds.contains(pos)) {
      toggleReaction(pill.messageId, pill.reactionType);
      return;
    }
  }

  // TODO: Re-enable shared post/story/parent message hit testing
  // These require accessing messages from AppStore ChatState
}

void MessageThread::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {
  // Only scroll if wheel is within message area (not input box)
  if (event.y < getHeight() - MESSAGE_INPUT_HEIGHT - 10) {
    handleMouseWheelMove(event, wheel, getHeight() - MESSAGE_INPUT_HEIGHT, scrollBar.getWidth());
  }
}

void MessageThread::textEditorReturnKeyPressed(juce::TextEditor &editor) {
  if (&editor == &messageInput) {
    sendMessage();
  }
}

void MessageThread::textEditorTextChanged(juce::TextEditor &editor) {
  // Use text changes to update typing indicator state
  if (&editor == &messageInput) {
    bool hasText = !messageInput.getText().isEmpty();
    // Could broadcast typing status to other users here
    Log::debug("MessageThread: Input text changed, hasText=" + juce::String(hasText ? "true" : "false"));
  }
}

//==============================================================================
void MessageThread::setStreamChatClient(StreamChatClient *client) {
  // streamChatClient only kept for backward compatibility, not used
  // for message updates
  streamChatClient = client;
}

void MessageThread::setNetworkClient(NetworkClient *client) {
  networkClient = client;
}

void MessageThread::loadChannel(const juce::String &type, const juce::String &id) {
  channelType = type;
  channelId = id;
  Log::info("MessageThread: Loading channel " + type + "/" + id);

  // Load messages from AppStore (messages already loaded or will be loaded by AppStore)
  if (appStore) {
    appStore->selectChannel(id);
    appStore->loadMessages(id);
    Log::debug("MessageThread: Requested AppStore to load messages for channel " + id);
  } else {
    Log::warn("MessageThread: AppStore not available");
  }

  repaint();
}

void MessageThread::loadMessages() {
  if (channelId.isEmpty()) {
    Log::warn("MessageThread: loadMessages called but no channel selected");
    return;
  }

  Log::debug("MessageThread: loadMessages for channel " + channelId);

  if (!appStore) {
    Log::error("MessageThread: AppStore not available");
    return;
  }

  // Request AppStore to load messages (AppStore manages state and persistence)
  Log::info("MessageThread: Requesting AppStore to load messages from " + channelId);
  appStore->loadMessages(channelId, 100);
}

void MessageThread::sendMessage() {
  Log::info("MessageThread::sendMessage - 🚀 CALLED!");

  juce::String text = messageInput.getText().trim();
  Log::info("MessageThread::sendMessage - Message text length: " + juce::String(text.length()));

  if (text.isEmpty()) {
    Log::debug("MessageThread::sendMessage - Message text is empty, returning");
    return;
  }

  if (channelId.isEmpty()) {
    Log::error("MessageThread::sendMessage - SEGFAULT RISK: no channel selected!");
    return;
  }

  if (!appStore) {
    Log::error("MessageThread::sendMessage - SEGFAULT RISK: AppStore not available!");
    return;
  }

  Log::info("MessageThread::sendMessage - ✓ AppStore is valid");
  Log::info("MessageThread::sendMessage - ✓ ChannelId is valid: " + channelId);
  Log::info("MessageThread::sendMessage - ✓ All checks passed, sending message");

  // Check if we're editing or sending new message
  bool isEditing = !editingMessageId.isEmpty();
  juce::String messageIdToEdit = editingMessageId;

  // Clear reply/edit state
  replyingToMessageId = "";
  replyingToMessage = StreamChatClient::Message();
  editingMessageId = "";
  editingMessageText = "";

  // Clear input field immediately
  messageInput.setText("");
  messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
  resized();

  Log::debug("MessageThread::sendMessage - Text to send: " + text.substring(0, 50));

  try {
    if (isEditing) {
      Log::info("MessageThread::sendMessage - Editing message " + messageIdToEdit);
      appStore->editMessage(channelId, messageIdToEdit, text);
      Log::info("MessageThread::sendMessage - ✓ Message edited successfully via AppStore for channel " + channelId);
    } else {
      Log::info("MessageThread::sendMessage - About to call appStore->sendMessage()");
      appStore->sendMessage(channelId, text);
      Log::info("MessageThread::sendMessage - ✓ Message sent successfully via AppStore for channel " + channelId);
    }
  } catch (const std::exception &e) {
    Log::error("MessageThread::sendMessage - EXCEPTION: " + juce::String(e.what()));
  } catch (...) {
    Log::error("MessageThread::sendMessage - UNKNOWN EXCEPTION!");
  }
}

void MessageThread::timerCallback() {}

//==============================================================================
void MessageThread::drawHeader(juce::Graphics &g) {
  auto headerBounds = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);

  // Background
  g.setColour(juce::Colour(0xff2a2a2a));
  g.fillRect(headerBounds);

  // Back button
  auto backBounds = getBackButtonBounds();
  g.setColour(SidechainColors::primary());
  g.setFont(20.0f);
  g.drawText("<", backBounds, juce::Justification::centred);

  // Channel name
  g.setColour(juce::Colours::white);
  g.setFont(18.0f);
  g.drawText(channelName, headerBounds.withTrimmedLeft(60).withTrimmedRight(50), juce::Justification::centredLeft);

  // More menu button (for group channels)
  if (isGroupChannel()) {
    auto menuBounds = getHeaderMenuButtonBounds();
    g.setColour(juce::Colour(0xff888888));
    g.setFont(20.0f);
    g.drawText(juce::String::fromUTF8("⋯"), menuBounds,
               juce::Justification::centred); // Three dots
  }

  // Bottom border
  g.setColour(juce::Colour(0xff3a3a3a));
  g.drawHorizontalLine(HEADER_HEIGHT - 1, 0.0f, static_cast<float>(getWidth()));
}

void MessageThread::drawInputArea(juce::Graphics &g) {
  auto bounds = getLocalBounds();
  auto inputBounds = bounds.removeFromBottom(INPUT_HEIGHT);
  int padding = 10;
  int sendButtonWidth = 80;
  int audioButtonWidth = 40;

  // Background
  g.setColour(juce::Colour(0xff252525));
  g.fillRect(inputBounds);

  // Top border
  g.setColour(juce::Colour(0xff3a3a3a));
  g.drawHorizontalLine(inputBounds.getY(), 0.0f, static_cast<float>(getWidth()));

  // Send button bounds
  auto sendButtonBounds = getSendButtonBounds();
  g.setColour(SidechainColors::primary());
  g.fillRect(sendButtonBounds);
  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText("Send", sendButtonBounds, juce::Justification::centred);

  Log::debug("MessageThread::drawInputArea - Send button bounds: " + juce::String(sendButtonBounds.getX()) + "," +
             juce::String(sendButtonBounds.getY()) + "," + juce::String(sendButtonBounds.getWidth()) + "," +
             juce::String(sendButtonBounds.getHeight()));
}

void MessageThread::drawMessages(juce::Graphics &g, const std::vector<StreamChatClient::Message> &messages) {
  // Note: messages is passed in from paint() to avoid state changes between
  // paint() and drawMessages() This ensures we're always working with the same
  // snapshot of the message list

  int y = HEADER_HEIGHT + MESSAGE_TOP_PADDING - static_cast<int>(getScrollPosition());
  int width = getWidth() - scrollBar.getWidth();

  // Calculate bottom area height (reply preview + input)
  int bottomAreaHeight = INPUT_HEIGHT;
  if (!replyingToMessageId.isEmpty())
    bottomAreaHeight += REPLY_PREVIEW_HEIGHT;

  // Log at INFO level so it's always visible
  if (!messages.empty()) {
    Log::info("MessageThread: Drawing " + juce::String(messages.size()) + " messages to UI for channel: " + channelId);
  }

  Log::debug("MessageThread::drawMessages - height: " + juce::String(getHeight()) + ", bottomArea: " +
             juce::String(bottomAreaHeight) + ", scrollPosition: " + juce::String(getScrollPosition()) +
             ", y start: " + juce::String(y) + ", messages.size(): " + juce::String(messages.size()));

  for (size_t i = 0; i < messages.size(); ++i) {
    const auto &message = messages[i];
    int messageHeight = calculateMessageHeight(message, MESSAGE_MAX_WIDTH);

    Log::debug("MessageThread::drawMessages - message " + juce::String(i) + ": text='" + message.text.substring(0, 20) +
               "', height=" + juce::String(messageHeight) + ", y=" + juce::String(y) + ", visible=" +
               juce::String((y + messageHeight > HEADER_HEIGHT && y < getHeight() - bottomAreaHeight) ? "YES" : "NO"));

    // Only draw if visible
    if (y + messageHeight > HEADER_HEIGHT && y < getHeight() - bottomAreaHeight) {
      drawMessageBubble(g, message, y, width); // This increments y
    } else {
      // Skip to next message position if not visible
      y += messageHeight + MESSAGE_BUBBLE_PADDING;
    }
  }
}

void MessageThread::drawMessageBubble(juce::Graphics &g, const StreamChatClient::Message &message, int &y, int width) {
  Log::debug("MessageThread::drawMessageBubble - Starting to draw message: " + message.text.substring(0, 20) +
             ", y=" + juce::String(y) + ", width=" + juce::String(width) +
             ", ownMessage=" + juce::String(isOwnMessage(message) ? "YES" : "NO"));

  bool ownMessage = isOwnMessage(message);
  int bubbleMaxWidth = MESSAGE_MAX_WIDTH;
  int bubblePadding = 10;

  // Check if this is a reply
  juce::String replyToId = getReplyToMessageId(message);
  auto parentMessageOpt = findParentMessage(replyToId);
  bool isReply = parentMessageOpt.has_value();
  const StreamChatClient::Message parentMessage = parentMessageOpt.value_or(StreamChatClient::Message{});
  int threadIndent = isReply ? 20 : 0; // Indent replies

  // Check for shared content
  int sharedContentHeight = getSharedContentHeight(message);
  bool hasSharedContent = sharedContentHeight > 0;

  // Calculate text bounds
  juce::Font font(juce::FontOptions().withHeight(14.0f));
  g.setFont(font);

  // Calculate width using AttributedString
  juce::AttributedString widthAttrStr;
  widthAttrStr.setText(message.text);
  widthAttrStr.setFont(font);
  juce::TextLayout widthLayout;
  widthLayout.createLayout(widthAttrStr,
                           10000.0f); // Large width for width calculation
  int textWidth = juce::jmin(bubbleMaxWidth - 2 * bubblePadding - threadIndent,
                             static_cast<int>(widthLayout.getWidth()) + 2 * bubblePadding);

  // Ensure minimum width for shared content
  if (hasSharedContent)
    textWidth = juce::jmax(textWidth, 200); // Wider for shared content cards
  else
    textWidth = juce::jmax(textWidth, 100);

  // Calculate height based on wrapped text
  juce::AttributedString attrStr;
  attrStr.setText(message.text);
  attrStr.setFont(font);
  attrStr.setColour(juce::Colours::white);

  juce::TextLayout layout;
  layout.createLayout(attrStr, static_cast<float>(textWidth));
  int textHeight = static_cast<int>(layout.getHeight());

  // For messages with only shared content (no text), reduce text height
  if (message.text.isEmpty() && hasSharedContent)
    textHeight = 0;

  // Account for parent message preview and shared content
  int parentPreviewHeight = isReply ? 40 : 0;
  int bubbleHeight =
      textHeight + 2 * bubblePadding + 20 + parentPreviewHeight + sharedContentHeight; // Extra for timestamp + parent
                                                                                       // preview + shared content
  int bubbleWidth = textWidth + 2 * bubblePadding;

  // Position bubble (indent replies)
  int bubbleX;
  if (ownMessage) {
    bubbleX = width - bubbleWidth - 15 - threadIndent; // Right aligned, indented if reply
  } else {
    bubbleX = 15 + threadIndent; // Left aligned, indented if reply
  }

  auto bubbleBounds = juce::Rectangle<int>(bubbleX, y, bubbleWidth, bubbleHeight);

  Log::debug("MessageThread::drawMessageBubble - bubbleBounds: x=" + juce::String(bubbleX) + ", y=" + juce::String(y) +
             ", width=" + juce::String(bubbleWidth) + ", height=" + juce::String(bubbleHeight));

  // Draw bubble background
  juce::Colour bubbleColor = ownMessage ? SidechainColors::primary() : juce::Colour(0xff3a3a3a);
  g.setColour(bubbleColor);
  g.fillRoundedRectangle(bubbleBounds.toFloat(), 12.0f);

  Log::debug("MessageThread::drawMessageBubble - Drew bubble background with "
             "color: 0x" +
             juce::String::toHexString(bubbleColor.getARGB()));

  // Draw parent message preview for replies
  if (isReply) {

    // Parent preview area (above message text)
    auto parentPreviewBounds = bubbleBounds.withHeight(parentPreviewHeight - 5).reduced(bubblePadding, 5);

    // Left border (accent color)
    g.setColour(SidechainColors::primary());
    g.fillRect(parentPreviewBounds.withWidth(3));

    // Parent message sender name
    g.setColour(juce::Colour(0xff888888));
    g.setFont(10.0f);
    juce::String parentSender = parentMessage.userName.isEmpty() ? "User" : parentMessage.userName;
    g.drawText(parentSender, parentPreviewBounds.withTrimmedLeft(8).withHeight(12), juce::Justification::centredLeft);

    // Parent message text (truncated)
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(11.0f);
    juce::String parentText = parentMessage.text;
    if (parentText.length() > 50)
      parentText = parentText.substring(0, 50) + "...";
    g.drawText(parentText, parentPreviewBounds.withTrimmedLeft(8).withTrimmedTop(12), juce::Justification::centredLeft);

    // Divider line
    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawHorizontalLine(parentPreviewBounds.getBottom() - 1, static_cast<float>(parentPreviewBounds.getX()),
                         static_cast<float>(parentPreviewBounds.getRight()));
  }

  // Draw message text (if any)
  if (!message.text.isEmpty()) {
    g.setColour(juce::Colours::white);
    auto textBounds = bubbleBounds.reduced(bubblePadding)
                          .withTrimmedTop(parentPreviewHeight)
                          .withTrimmedBottom(16 + sharedContentHeight);
    // Clip text to bubble bounds to prevent overflow
    g.saveState();
    g.reduceClipRegion(textBounds);
    layout.draw(g, textBounds.toFloat());
    g.restoreState();
  }

  // Draw shared content preview (post or story)
  if (hasSharedContent) {
    int sharedContentY = bubbleBounds.getY() + bubblePadding + parentPreviewHeight + textHeight;
    auto sharedContentBounds = juce::Rectangle<int>(bubbleBounds.getX() + bubblePadding, sharedContentY,
                                                    bubbleBounds.getWidth() - 2 * bubblePadding,
                                                    sharedContentHeight - 5 // Slight padding
    );

    if (hasSharedPost(message))
      drawSharedPostPreview(g, message, sharedContentBounds);
    else if (hasSharedStory(message))
      drawSharedStoryPreview(g, message, sharedContentBounds);
  }

  // Draw timestamp
  g.setColour(juce::Colour(0xffcccccc));
  g.setFont(10.0f);
  juce::String timestamp = formatTimestamp(message.createdAt);
  g.drawText(timestamp, bubbleBounds.withTrimmedTop(bubbleHeight - 18).reduced(bubblePadding, 0),
             ownMessage ? juce::Justification::centredRight : juce::Justification::centredLeft);

  // Draw sender name for received messages
  if (!ownMessage && !message.userName.isEmpty()) {
    g.setColour(juce::Colour(0xff888888));
    g.setFont(11.0f);
    g.drawText(message.userName, bubbleX, y - 16, bubbleWidth, 14, juce::Justification::bottomLeft);
  }

  y += bubbleHeight + MESSAGE_BUBBLE_PADDING;

  // Draw reactions below the bubble
  drawMessageReactions(g, message, y, bubbleX, bubbleWidth);
}

void MessageThread::drawEmptyState(juce::Graphics &g) {
  int bottomAreaHeight = INPUT_HEIGHT;
  if (!replyingToMessageId.isEmpty())
    bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
  auto bounds = getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(bottomAreaHeight);

  g.setColour(juce::Colours::white);
  g.setFont(18.0f);
  g.drawText("No messages yet", bounds, juce::Justification::centred);

  g.setColour(juce::Colour(0xffaaaaaa));
  g.setFont(14.0f);
  g.drawText("Send a message to start the conversation", bounds.withTrimmedTop(30), juce::Justification::centred);
}

void MessageThread::drawErrorState(juce::Graphics &g) {
  int bottomAreaHeight = INPUT_HEIGHT;
  if (!replyingToMessageId.isEmpty())
    bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
  auto bounds = getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(bottomAreaHeight);

  g.setColour(juce::Colour(0xffcccccc));
  g.setFont(14.0f);
  g.drawText("No messages", bounds, juce::Justification::centred);
}

//==============================================================================
juce::String MessageThread::formatTimestamp(const juce::String &timestamp) {
  if (timestamp.isEmpty())
    return "";

  return StringFormatter::formatTimeAgo(timestamp);
}

int MessageThread::calculateMessageHeight(const StreamChatClient::Message &message, int maxWidth) const {
  juce::Font font(juce::FontOptions().withHeight(14.0f));
  int bubblePadding = 10;

  // Check if this is a reply (add parent preview height)
  juce::String replyToId = getReplyToMessageId(message);
  bool isReply = !replyToId.isEmpty() && findParentMessage(replyToId).has_value();
  int parentPreviewHeight = isReply ? 40 : 0;
  int threadIndent = isReply ? 20 : 0;

  // Check for shared content (posts/stories)
  int sharedContentHeight = getSharedContentHeight(message);

  juce::AttributedString attrStr;
  attrStr.setText(message.text);
  attrStr.setFont(font);

  juce::TextLayout layout;
  layout.createLayout(attrStr, static_cast<float>(maxWidth - 2 * bubblePadding - threadIndent));

  int textHeight = static_cast<int>(layout.getHeight());

  // For messages with only shared content (no text), ensure minimum height
  if (message.text.isEmpty() && sharedContentHeight > 0)
    textHeight = 0;

  return textHeight + 2 * bubblePadding + 20 + parentPreviewHeight + sharedContentHeight + MESSAGE_BUBBLE_PADDING;
}

int MessageThread::calculateTotalMessagesHeight() {
  int totalHeight = MESSAGE_TOP_PADDING;

  if (!appStore) {
    return totalHeight;
  }

  // Get messages from AppStore ChatState
  const auto &chatState = appStore->getChatState();
  if (chatState.channels.empty()) {
    return totalHeight;
  }

  // Find current channel
  auto channelIt = chatState.channels.find(channelId);
  if (channelIt == chatState.channels.end()) {
    return totalHeight;
  }

  const auto &messages = channelIt->second.messages;
  int messageAreaWidth = getWidth() - scrollBar.getWidth();

  for (const auto &message : messages) {
    if (message.isObject()) {
      // Create StreamChatClient::Message from juce::var
      StreamChatClient::Message msg;
      auto *obj = message.getDynamicObject();
      if (obj) {
        msg.id = obj->getProperty("id").toString();
        msg.text = obj->getProperty("text").toString();
        msg.userId = obj->getProperty("user_id").toString();
        msg.userName = obj->getProperty("user_name").toString();
        msg.createdAt = obj->getProperty("created_at").toString();

        int messageHeight = calculateMessageHeight(msg, messageAreaWidth);
        totalHeight += messageHeight + MESSAGE_BUBBLE_PADDING;
      }
    }
  }

  return totalHeight;
}

bool MessageThread::isOwnMessage(const StreamChatClient::Message &message) const {
  return message.userId == currentUserId;
}

juce::Rectangle<int> MessageThread::getBackButtonBounds() const {
  return juce::Rectangle<int>(10, 10, 40, 40);
}

juce::Rectangle<int> MessageThread::getHeaderMenuButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 45, 10, 40, 40);
}

juce::Rectangle<int> MessageThread::getAudioButtonBounds() const {
  int bottomAreaHeight = INPUT_HEIGHT;
  if (!replyingToMessageId.isEmpty())
    bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
  if (showAudioRecorder && audioSnippetRecorder)
    bottomAreaHeight += AUDIO_RECORDER_HEIGHT;

  int padding = 10;
  int audioButtonWidth = 40;
  int sendButtonWidth = 80;
  int audioButtonX = getWidth() - padding - audioButtonWidth;

  return juce::Rectangle<int>(audioButtonX, getHeight() - bottomAreaHeight + padding, audioButtonWidth,
                              INPUT_HEIGHT - 2 * padding);
}

juce::Rectangle<int> MessageThread::getSendButtonBounds() const {
  int bottomAreaHeight = INPUT_HEIGHT;
  if (!replyingToMessageId.isEmpty())
    bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
  if (showAudioRecorder && audioSnippetRecorder)
    bottomAreaHeight += AUDIO_RECORDER_HEIGHT;

  int padding = 10;
  int audioButtonWidth = 40;
  int sendButtonWidth = 80;
  int sendButtonX = getWidth() - padding - audioButtonWidth - padding - sendButtonWidth;

  return juce::Rectangle<int>(sendButtonX, getHeight() - bottomAreaHeight + padding, sendButtonWidth,
                              INPUT_HEIGHT - 2 * padding);
}

juce::Rectangle<int> MessageThread::getMessageBounds(const StreamChatClient::Message &message) const {
  // Calculate bounds for the given message based on its properties
  // Use message ID to estimate position (in practice would look up actual position)
  uint32 hashValue = static_cast<uint32>(std::hash<juce::String>{}(message.id));
  int estimatedY = HEADER_HEIGHT + static_cast<int>((hashValue % 500) * 0.1f);

  // Account for current scroll position
  int y = estimatedY - static_cast<int>(getScrollPosition());

  // Estimate message height from text length and bubble properties
  int messageHeight = 60; // Minimum height
  if (message.text.length() > 0) {
    // Simple estimation: ~40 pixels per line of text
    int estimatedLines = (message.text.length() / 50) + 1;
    messageHeight = 30 + (estimatedLines * 20);
  }

  // Return bounds for this message
  int msgPadding = 12;
  auto messageBounds = juce::Rectangle<int>(msgPadding, y, getWidth() - 2 * msgPadding - 12, messageHeight);
  return messageBounds;
}

juce::Rectangle<int> MessageThread::getSharedPostBounds(const StreamChatClient::Message &message) const {
  if (!hasSharedPost(message))
    return juce::Rectangle<int>();

  auto messageBounds = getMessageBounds(message);
  if (messageBounds.isEmpty())
    return juce::Rectangle<int>();

  // Shared post preview appears below the message text
  // Use the same logic as in drawMessageBubble where we draw the shared content
  int sharedContentHeight = getSharedContentHeight(message);
  if (sharedContentHeight == 0)
    return juce::Rectangle<int>();

  // Position shared content at bottom of message bubble
  return juce::Rectangle<int>(messageBounds.getX() + MESSAGE_BUBBLE_PADDING,
                              messageBounds.getBottom() - sharedContentHeight - MESSAGE_BUBBLE_PADDING,
                              messageBounds.getWidth() - 2 * MESSAGE_BUBBLE_PADDING, sharedContentHeight);
}

juce::Rectangle<int> MessageThread::getSharedStoryBounds(const StreamChatClient::Message &message) const {
  if (!hasSharedStory(message))
    return juce::Rectangle<int>();

  auto messageBounds = getMessageBounds(message);
  if (messageBounds.isEmpty())
    return juce::Rectangle<int>();

  // Shared story preview appears below the message text
  // Use the same logic as in drawMessageBubble where we draw the shared content
  int sharedContentHeight = getSharedContentHeight(message);
  if (sharedContentHeight == 0)
    return juce::Rectangle<int>();

  // Position shared content at bottom of message bubble
  return juce::Rectangle<int>(messageBounds.getX() + MESSAGE_BUBBLE_PADDING,
                              messageBounds.getBottom() - sharedContentHeight - MESSAGE_BUBBLE_PADDING,
                              messageBounds.getWidth() - 2 * MESSAGE_BUBBLE_PADDING, sharedContentHeight);
}

void MessageThread::showMessageActionsMenu(const StreamChatClient::Message &message,
                                           const juce::Point<int> &screenPos) {
  juce::PopupMenu menu;
  bool ownMessage = isOwnMessage(message);

  // React is always available
  menu.addItem(1, "React...");
  menu.addSeparator();

  // Copy is always available
  menu.addItem(2, "Copy");

  if (ownMessage) {
    // Only allow editing/deleting own messages
    // Edit only if message is less than 5 minutes old (getstream.io limit)
    // For now, we'll allow edit for all own messages - getstream.io will
    // enforce the limit
    menu.addItem(3, "Edit");
    menu.addItem(4, "Delete");
  } else {
    // Reply to others' messages
    menu.addItem(5, "Reply");
    menu.addSeparator();
    menu.addItem(6, "Report");
    menu.addItem(7, "Block User");
  }

  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos, juce::Point<int>(1, 1))),
      [this, message, ownMessage, screenPos](int result) {
        if (result == 1) {
          showQuickReactionPicker(message, screenPos);
        } else if (result == 2) {
          copyMessageText(message.text);
        } else if (result == 3 && ownMessage) {
          editMessage(message);
        } else if (result == 4 && ownMessage) {
          deleteMessage(message);
        } else if (result == 5 && !ownMessage) {
          replyToMessage(message);
        } else if (result == 6 && !ownMessage) {
          reportMessage(message);
        } else if (result == 7 && !ownMessage) {
          blockUser(message);
        }
      });
}

void MessageThread::copyMessageText(const juce::String &text) {
  juce::SystemClipboard::copyTextToClipboard(text);
  Log::info("MessageThread: Copied message text to clipboard");
}

void MessageThread::editMessage(const StreamChatClient::Message &message) {
  editingMessageId = message.id;
  editingMessageText = message.text;
  replyingToMessageId = ""; // Clear reply state when editing
  replyingToMessage = StreamChatClient::Message();
  messageInput.setText(message.text);
  messageInput.setHighlightedRegion({0, message.text.length()});
  messageInput.grabKeyboardFocus();
  resized(); // Update layout
  repaint();
  Log::info("MessageThread: Editing message " + message.id);
}

void MessageThread::deleteMessage(const StreamChatClient::Message &message) {
  if (channelId.isEmpty()) {
    Log::warn("Cannot delete message: ChatStore not set");
    return;
  }

  if (!appStore) {
    Log::error("MessageThread::deleteMessage - AppStore not available");
    return;
  }

  Log::info("MessageThread::deleteMessage - Deleting message " + message.id);
  appStore->deleteMessage(channelId, message.id);
}

void MessageThread::replyToMessage(const StreamChatClient::Message &message) {
  replyingToMessageId = message.id;
  replyingToMessage = message; // Store full message for preview
  messageInput.setText("");
  messageInput.setTextToShowWhenEmpty("Type a reply...", juce::Colour(0xff888888));
  messageInput.grabKeyboardFocus();
  repaint(); // Redraw to show reply preview
  Log::info("MessageThread: Replying to message " + message.id);
}

void MessageThread::cancelReply() {
  replyingToMessageId = "";
  replyingToMessage = StreamChatClient::Message(); // Clear message
  messageInput.setText("");
  messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
  repaint();
}

juce::Rectangle<int> MessageThread::getReplyPreviewBounds() const {
  if (replyingToMessageId.isEmpty())
    return juce::Rectangle<int>();

  return juce::Rectangle<int>(0, getHeight() - INPUT_HEIGHT - REPLY_PREVIEW_HEIGHT, getWidth(), REPLY_PREVIEW_HEIGHT);
}

juce::Rectangle<int> MessageThread::getCancelReplyButtonBounds() const {
  auto previewBounds = getReplyPreviewBounds();
  if (previewBounds.isEmpty())
    return juce::Rectangle<int>();

  return previewBounds.removeFromRight(40).reduced(5);
}

juce::String MessageThread::getReplyToMessageId(const StreamChatClient::Message &message) const {
  if (message.extraData.isObject()) {
    auto *obj = message.extraData.getDynamicObject();
    if (obj != nullptr) {
      return obj->getProperty("reply_to").toString();
    }
  }
  return "";
}

std::optional<StreamChatClient::Message> MessageThread::findParentMessage(const juce::String &messageId) const {
  if (messageId.isEmpty() || !appStore)
    return std::nullopt;

  const auto &chatState = appStore->getChatState();
  auto channelIt = chatState.channels.find(channelId);
  if (channelIt == chatState.channels.end()) {
    return std::nullopt;
  }

  const auto &messages = channelIt->second.messages;
  for (const auto &msg : messages) {
    if (msg.isObject()) {
      auto *obj = msg.getDynamicObject();
      if (obj && obj->getProperty("id").toString() == messageId) {
        Log::debug("MessageThread::findParentMessage - Found parent message " + messageId);
        // Construct and return parent message by value via optional
        StreamChatClient::Message parent;
        parent.id = obj->getProperty("id").toString();
        parent.text = obj->getProperty("text").toString();
        parent.userId = obj->getProperty("user_id").toString();
        parent.userName = obj->getProperty("user_name").toString();
        parent.createdAt = obj->getProperty("created_at").toString();
        return parent;
      }
    }
  }

  Log::debug("MessageThread::findParentMessage - Parent message not found: " + messageId);
  return std::nullopt;
}

void MessageThread::scrollToMessage(const juce::String &messageId) {
  if (messageId.isEmpty())
    return;

  // Get messages from ChatStore instead of local array
  if (false) // TODO: refactor to use AppStore
    return;

  return;
}

void MessageThread::reportMessage(const StreamChatClient::Message &message) {
  if (message.id.isEmpty() || message.userId.isEmpty()) {
    Log::warn("MessageThread: Cannot report message - empty messageId or userId");
    return;
  }

  // Create a popup menu with report reasons
  juce::PopupMenu menu;
  menu.addItem(1, "Inappropriate Content");
  menu.addItem(2, "Harassment");
  menu.addItem(3, "Spam");
  menu.addItem(4, "Offensive Language");
  menu.addItem(5, "Misinformation");
  menu.addItem(6, "Other");

  menu.showMenuAsync(juce::PopupMenu::Options(), [message](int result) {
    if (result == 0)
      return; // Cancelled

    static const std::array<juce::String, 6> reasons = {"Inappropriate Content", "Harassment",     "Spam",
                                                        "Offensive Language",    "Misinformation", "Other"};

    juce::String reason = reasons[static_cast<size_t>(result - 1)];

    Log::info("MessageThread: Reporting message " + message.id + " for: " + reason);

    // Show confirmation
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Report Submitted",
                                           "Thank you for your report. Our moderation team will review this content.");

    // TODO: Send report to backend when report API endpoint is available
    // networkClient->reportMessage(message.id, reason, [this](bool success) { ... });
  });
}

void MessageThread::blockUser(const StreamChatClient::Message &message) {
  if (message.userId.isEmpty()) {
    Log::warn("MessageThread: Cannot block user - empty userId");
    return;
  }

  if (!networkClient) {
    Log::warn("MessageThread: Cannot block user - NetworkClient not set");
    return;
  }

  // Show confirmation dialog
  juce::NativeMessageBox::showOkCancelBox(
      juce::MessageBoxIconType::QuestionIcon, "Block User",
      "Are you sure you want to block this user? You won't see their messages or content anymore.", nullptr,
      juce::ModalCallbackFunction::create([this, userId = message.userId](int result) {
        if (result == 1) { // "Block" button clicked
          Log::info("MessageThread: Blocking user " + userId);
          networkClient->blockUser(userId, [userId](const Outcome<juce::var> &outcome) {
            if (outcome.isOk()) {
              Log::info("MessageThread: Successfully blocked user " + userId);
              juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "User Blocked",
                                                     "This user has been blocked.");
            } else {
              Log::warn("MessageThread: Failed to block user " + userId);
              juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Error",
                                                     "Failed to block user. Please try again.");
            }
          });
        }
      }));
}

bool MessageThread::isGroupChannel() const {
  return channelType == "team" || (!channelName.isEmpty() && channelName != "Direct Message");
}

void MessageThread::leaveGroup() {
  if (channelId.isEmpty() || !isGroupChannel()) {
    Log::warn("Cannot leave group: ChatStore not set or not a group");
    return;
  }

  // TODO: appStore->leaveChannel(channelId);
  Log::debug("MessageThread: Leave group requested for " + channelId);

  // Navigate back to messages list
  juce::MessageManager::callAsync([this]() {
    if (onChannelClosed)
      onChannelClosed(channelType, channelId);
    if (onBackPressed)
      onBackPressed();
  });
}

void MessageThread::renameGroup() {
  // TODO: ChatStore doesn't support renaming groups yet
  Log::warn("MessageThread: Group rename not yet implemented via ChatStore");
}

void MessageThread::showAddMembersDialog() {
  // TODO: ChatStore doesn't support member management yet
  Log::warn("MessageThread: Add members not yet implemented via ChatStore");
}

void MessageThread::showRemoveMembersDialog() {
  // TODO: Remove members not yet implemented via ChatStore
  Log::warn("MessageThread: Remove members not yet implemented via ChatStore");
}

void MessageThread::sendAudioSnippet(const juce::AudioBuffer<float> &audioBuffer, double sampleRate) {
  if (audioBuffer.getNumSamples() == 0) {
    Log::warn("MessageThread: Cannot send audio snippet - empty audio buffer");
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Empty Audio",
                                           "Please record some audio before sending.");
    return;
  }

  if (!streamChatClient) {
    Log::warn("MessageThread: Cannot send audio snippet - StreamChatClient not set");
    return;
  }

  if (channelId.isEmpty() || channelType.isEmpty()) {
    Log::warn("MessageThread: Cannot send audio snippet - channel not loaded");
    return;
  }

  Log::info("MessageThread: Uploading audio snippet to channel " + channelId);

  // Upload audio snippet and send as message
  streamChatClient->sendMessageWithAudio(
      channelType, channelId, "", // Empty text, audio is the main content
      audioBuffer, sampleRate, [this](Outcome<StreamChatClient::Message> result) {
        if (result.isOk()) {
          Log::info("MessageThread: Audio snippet sent successfully");
          messageInput.clear(); // Clear text input if there was any
          showAudioRecorder = false;
          repaint();
        } else {
          Log::warn("MessageThread: Failed to send audio snippet: " + result.getError());
          juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error Sending Audio",
                                                 "Failed to send audio snippet. Please try again.");
        }
      });
}

//==============================================================================
// Shared content detection
bool MessageThread::hasSharedPost(const StreamChatClient::Message &message) const {
  if (!message.extraData.isObject())
    return false;

  auto *obj = message.extraData.getDynamicObject();
  if (obj == nullptr)
    return false;

  auto sharedPost = obj->getProperty("shared_post");
  return sharedPost.isObject();
}

bool MessageThread::hasSharedStory(const StreamChatClient::Message &message) const {
  if (!message.extraData.isObject())
    return false;

  auto *obj = message.extraData.getDynamicObject();
  if (obj == nullptr)
    return false;

  auto sharedStory = obj->getProperty("shared_story");
  return sharedStory.isObject();
}

int MessageThread::getSharedContentHeight(const StreamChatClient::Message &message) const {
  if (hasSharedPost(message) || hasSharedStory(message))
    return 80; // Height for shared content card
  return 0;
}

void MessageThread::drawSharedPostPreview(juce::Graphics &g, const StreamChatClient::Message &message,
                                          juce::Rectangle<int> bounds) {
  if (!message.extraData.isObject())
    return;

  auto *obj = message.extraData.getDynamicObject();
  if (obj == nullptr)
    return;

  auto sharedPost = obj->getProperty("shared_post");
  if (!sharedPost.isObject())
    return;

  // Extract post data
  juce::String authorUsername = sharedPost.getProperty("author_username", "").toString();
  juce::String audioUrl = sharedPost.getProperty("audio_url", "").toString();
  int bpm = static_cast<int>(sharedPost.getProperty("bpm", 0));
  juce::String key = sharedPost.getProperty("key", "").toString();
  float duration = static_cast<float>(sharedPost.getProperty("duration_seconds", 0.0));
  juce::String genres = sharedPost.getProperty("genres", "").toString();

  // Card background
  g.setColour(SidechainColors::surface().darker(0.1f));
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Left accent border
  g.setColour(SidechainColors::primary());
  g.fillRoundedRectangle(bounds.removeFromLeft(4).toFloat(), 8.0f);

  auto contentBounds = bounds.reduced(10, 8);

  // Music icon
  g.setColour(SidechainColors::primary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
  g.drawText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8E\xB5")), // Music note emoji
             contentBounds.removeFromLeft(24), juce::Justification::centred);

  contentBounds.removeFromLeft(8);

  // Post author
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
  g.drawText("Post by @" + authorUsername, contentBounds.removeFromTop(16), juce::Justification::centredLeft);

  // Audio info (BPM, key, duration)
  juce::String info;
  if (bpm > 0)
    info += juce::String(bpm) + " BPM";
  if (key.isNotEmpty()) {
    if (info.isNotEmpty())
      info += " • ";
    info += key;
  }
  if (duration > 0) {
    if (info.isNotEmpty())
      info += " • ";
    int secs = static_cast<int>(duration);
    info += juce::String(secs / 60) + ":" + juce::String(secs % 60).paddedLeft('0', 2);
  }

  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
  g.drawText(info, contentBounds.removeFromTop(18), juce::Justification::centredLeft);

  // Genres
  if (genres.isNotEmpty()) {
    g.setColour(SidechainColors::textMuted());
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
    g.drawText(genres, contentBounds, juce::Justification::centredLeft);
  }
}

void MessageThread::drawSharedStoryPreview(juce::Graphics &g, const StreamChatClient::Message &message,
                                           juce::Rectangle<int> bounds) {
  if (!message.extraData.isObject())
    return;

  auto *obj = message.extraData.getDynamicObject();
  if (obj == nullptr)
    return;

  auto sharedStory = obj->getProperty("shared_story");
  if (!sharedStory.isObject())
    return;

  // Extract story data
  juce::String username = sharedStory.getProperty("username", "").toString();
  juce::String audioUrl = sharedStory.getProperty("audio_url", "").toString();
  float duration = static_cast<float>(sharedStory.getProperty("duration", 0.0));

  // Card background
  g.setColour(SidechainColors::surface().darker(0.1f));
  g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

  // Left accent border (gradient for stories)
  auto gradientBounds = bounds.removeFromLeft(4);
  juce::ColourGradient gradient(juce::Colour(0xFFFF6B6B), static_cast<float>(gradientBounds.getX()),
                                static_cast<float>(gradientBounds.getY()), juce::Colour(0xFF9B59B6),
                                static_cast<float>(gradientBounds.getX()),
                                static_cast<float>(gradientBounds.getBottom()), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(gradientBounds.toFloat(), 8.0f);

  auto contentBounds = bounds.reduced(10, 8);

  // Story icon (camera/story emoji)
  g.setColour(SidechainColors::primary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)));
  g.drawText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8E\xA4")), // Microphone emoji
             contentBounds.removeFromLeft(24), juce::Justification::centred);

  contentBounds.removeFromLeft(8);

  // Story author
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
  g.drawText("Story by @" + username, contentBounds.removeFromTop(16), juce::Justification::centredLeft);

  // Audio info
  juce::String info = "Audio story";
  if (duration > 0) {
    int secs = static_cast<int>(duration);
    info += " • " + juce::String(secs / 60) + ":" + juce::String(secs % 60).paddedLeft('0', 2);
  }

  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
  g.drawText(info, contentBounds.removeFromTop(18), juce::Justification::centredLeft);

  // Tap to view hint
  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
  g.drawText("Tap to view", contentBounds, juce::Justification::centredLeft);
}

//==============================================================================
// Reaction Methods
//==============================================================================

std::vector<juce::String> MessageThread::getReactionTypes(const StreamChatClient::Message &message) const {
  std::vector<juce::String> types;

  if (!message.reactions.isObject())
    return types;

  // getstream.io stores reactions in reaction_groups: { "like": { count: 5,
  // sum_scores: 5 }, ... }
  auto reactionGroups = message.reactions.getProperty("reaction_groups", juce::var());
  if (!reactionGroups.isObject())
    return types;

  auto *obj = reactionGroups.getDynamicObject();
  if (obj == nullptr)
    return types;

  for (auto &prop : obj->getProperties())
    types.push_back(prop.name.toString());

  return types;
}

int MessageThread::getReactionCount(const StreamChatClient::Message &message, const juce::String &reactionType) const {
  if (!message.reactions.isObject())
    return 0;

  auto reactionGroups = message.reactions.getProperty("reaction_groups", juce::var());
  if (!reactionGroups.isObject())
    return 0;

  auto group = reactionGroups.getProperty(reactionType, juce::var());
  if (!group.isObject())
    return 0;

  return static_cast<int>(group.getProperty("count", 0));
}

bool MessageThread::hasUserReacted(const StreamChatClient::Message &message, const juce::String &reactionType) const {
  if (!message.reactions.isObject())
    return false;

  // Check own_reactions array
  auto ownReactions = message.reactions.getProperty("own_reactions", juce::var());
  if (!ownReactions.isArray())
    return false;

  auto *arr = ownReactions.getArray();
  if (arr == nullptr)
    return false;

  for (int i = 0; i < arr->size(); ++i) {
    auto reaction = (*arr)[i];
    if (reaction.isObject()) {
      auto type = reaction.getProperty("type", "").toString();
      if (type == reactionType)
        return true;
    }
  }

  return false;
}

void MessageThread::addReaction(const juce::String &messageId, const juce::String &reactionType) {
  if (channelId.isEmpty())
    return;

  // TODO: appStore->addReaction(channelId, messageId, reactionType);
  Log::debug("MessageThread: Reaction '" + reactionType + "' added to message " + messageId);
}

void MessageThread::removeReaction(const juce::String &messageId, const juce::String &reactionType) {
  if (channelId.isEmpty())
    return;

  // TODO: ChatStore doesn't have removeReaction yet
  Log::debug("MessageThread: Reaction '" + reactionType + "' removed from message " + messageId);
}

void MessageThread::toggleReaction(const juce::String &messageId, const juce::String &reactionType) {
  if (messageId.isEmpty() || reactionType.isEmpty()) {
    Log::warn("MessageThread: Cannot toggle reaction - empty messageId or reactionType");
    return;
  }

  if (!streamChatClient) {
    Log::warn("MessageThread: Cannot toggle reaction - StreamChatClient not set");
    return;
  }

  if (channelId.isEmpty() || channelType.isEmpty()) {
    Log::warn("MessageThread: Cannot toggle reaction - channel not loaded");
    return;
  }

  Log::info("MessageThread: Toggling reaction " + reactionType + " on message " + messageId);

  // Check if user has already reacted with this type
  // We need to find the message first
  bool userHasReacted = false;

  // TODO: Get messages from ChatStore to check current reactions
  // For now, we'll attempt to add the reaction - if it fails, we can try removing

  // Try adding the reaction
  streamChatClient->addReaction(
      channelType, channelId, messageId, reactionType, [this, messageId, reactionType](Outcome<void> result) {
        if (!result.isOk()) {
          // If adding failed, try removing (user might have already reacted)
          if (streamChatClient) {
            streamChatClient->removeReaction(
                channelType, channelId, messageId, reactionType, [](Outcome<void> removeResult) {
                  if (!removeResult.isOk()) {
                    Log::warn("MessageThread: Failed to remove reaction: " + removeResult.getError());
                  }
                });
          }
        } else {
          Log::info("MessageThread: Successfully added reaction " + reactionType);
        }
      });
}

void MessageThread::drawMessageReactions(juce::Graphics &g, const StreamChatClient::Message &message, int &y, int x,
                                         int maxWidth) {
  auto reactionTypes = getReactionTypes(message);
  if (reactionTypes.empty())
    return;

  // Map reaction types to emojis
  static const std::map<juce::String, juce::String> emojiMap = {
      {"like", "❤️"}, {"love", "❤️"},  {"fire", "🔥"},     {"laugh", "😂"},      {"wow", "😮"},
      {"sad", "😢"}, {"pray", "🙏"}, {"thumbsup", "👍"}, {"thumbsdown", "👎"}, {"clap", "👏"}};

  int pillHeight = 28;
  int pillPadding = 6;
  int pillSpacing = 6;
  int currentX = x;
  int currentY = y + 4; // Small gap below message bubble

  for (const auto &type : reactionTypes) {
    int count = getReactionCount(message, type);
    if (count == 0)
      continue;

    bool userReacted = hasUserReacted(message, type);

    // Get emoji for this reaction type
    juce::String emoji = type; // Default to type name
    auto it = emojiMap.find(type);
    if (it != emojiMap.end())
      emoji = it->second;

    // Calculate pill text and width
    juce::String pillText = emoji + " " + juce::String(count);
    juce::Font font(juce::FontOptions().withHeight(13.0f));

    // Use TextLayout instead of deprecated getStringWidth
    juce::AttributedString attrStr;
    attrStr.setText(pillText);
    attrStr.setFont(font);
    juce::TextLayout layout;
    layout.createLayout(attrStr, 1000.0f);
    int textWidth = static_cast<int>(layout.getWidth());

    int pillWidth = textWidth + 2 * pillPadding;

    // Wrap to next line if needed
    if (currentX + pillWidth > x + maxWidth) {
      currentX = x;
      currentY += pillHeight + pillSpacing;
    }

    // Draw pill background
    juce::Rectangle<int> pillBounds(currentX, currentY, pillWidth, pillHeight);

    if (userReacted) {
      // Highlighted state - filled with accent color
      g.setColour(SidechainColors::coralPink());
      g.fillRoundedRectangle(pillBounds.toFloat(), 14.0f);
    } else {
      // Normal state - border only
      g.setColour(SidechainColors::borderActive());
      g.drawRoundedRectangle(pillBounds.toFloat(), 14.0f, 1.5f);
    }

    // Draw pill text
    g.setColour(userReacted ? juce::Colours::white : SidechainColors::textPrimary());
    g.setFont(font);
    g.drawText(pillText, pillBounds, juce::Justification::centred);

    // Cache this pill for hit testing
    ReactionPill pill;
    pill.messageId = message.id;
    pill.reactionType = type;
    pill.bounds = pillBounds;
    pill.count = count;
    pill.userReacted = userReacted;
    reactionPills.push_back(pill);

    currentX += pillWidth + pillSpacing;
  }

  // Update y to below the reactions
  int reactionsHeight = (currentY - y) + pillHeight + 4;
  y += reactionsHeight;
}

void MessageThread::showQuickReactionPicker(const StreamChatClient::Message &message,
                                            const juce::Point<int> &screenPos) {
  // Quick reaction picker with common emojis
  juce::PopupMenu menu;

  // Map menu item IDs to reaction types
  struct ReactionOption {
    juce::String display;
    juce::String type;
  };

  std::vector<ReactionOption> reactions = {{"❤️ Love", "like"},      {"🔥 Fire", "fire"}, {"😂 Laugh", "laugh"},
                                           {"😮 Wow", "wow"},       {"😢 Sad", "sad"},   {"🙏 Pray", "pray"},
                                           {"👍 Like", "thumbsup"}, {"👏 Clap", "clap"}};

  int itemId = 1;
  for (const auto &reaction : reactions) {
    juce::String displayText = reaction.display;
    if (hasUserReacted(message, reaction.type))
      displayText += " ✓"; // Checkmark for already reacted

    menu.addItem(itemId++, displayText);
  }

  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
      [this, message, reactions](int result) {
        if (result > 0 && result <= static_cast<int>(reactions.size())) {
          const auto &reactionType = reactions[static_cast<size_t>(result - 1)].type;
          toggleReaction(message.id, reactionType);
        }
      });
}

//==============================================================================
// Scroll Bar Listener

void MessageThread::scrollBarMoved(juce::ScrollBar *scrollBarPtr, double newScrollPosition) {
  SmoothScrollable::scrollBarMoved(scrollBarPtr, newScrollPosition);
  onScrollUpdate(newScrollPosition);
}

//==============================================================================
// Message persistence (save/load from disk)

// Message persistence is now managed by AppStore - no local storage needed
