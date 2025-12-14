#include "MessageThread.h"
#include "AudioSnippetRecorder.h"
#include "UserPickerDialog.h"
#include "../../network/NetworkClient.h"
#include "core/PluginProcessor.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringFormatter.h"
#include "../../util/Colors.h"
#include <algorithm>
#include <map>

//==============================================================================
MessageThread::MessageThread()
    : scrollBar(true)
{
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

void MessageThread::setAudioProcessor(SidechainAudioProcessor* processor)
{
    audioProcessor = processor;

    if (audioProcessor)
    {
        // Create audio snippet recorder
        audioSnippetRecorder = std::make_unique<AudioSnippetRecorder>(*audioProcessor);
        audioSnippetRecorder->onRecordingComplete = [this](const juce::AudioBuffer<float>& buffer, double sampleRate) {
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

MessageThread::~MessageThread()
{
    Log::debug("MessageThread: Destroying");
    stopTimer();

    // Stop watching channel for real-time updates
    if (streamChatClient)
    {
        streamChatClient->unwatchChannel();
        streamChatClient->setMessageReceivedCallback(nullptr);
    }
}

void MessageThread::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

    // Clear reaction pills cache for this paint cycle
    reactionPills.clear();

    drawHeader(g);

    switch (threadState)
    {
        case ThreadState::Loading:
        {
            int bottomAreaHeight = INPUT_HEIGHT;
            if (!replyingToMessageId.isEmpty())
                bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
            g.setColour(juce::Colours::white);
            g.setFont(16.0f);
            g.drawText("Loading messages...", getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(bottomAreaHeight),
                       juce::Justification::centred);
            break;
        }

        case ThreadState::Empty:
            drawEmptyState(g);
            break;

        case ThreadState::Error:
            // ErrorState component handles the error UI as a child component
            break;

        case ThreadState::Loaded:
            drawMessages(g);
            break;
    }

    drawInputArea(g);
}

void MessageThread::resized()
{
    auto bounds = getLocalBounds();

    // Calculate bottom area height (audio recorder + reply preview + input)
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
    if (showAudioRecorder && audioSnippetRecorder)
        bottomAreaHeight += AUDIO_RECORDER_HEIGHT;

    // Scrollbar on right side of message area
    auto messageArea = bounds.withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(bottomAreaHeight);
    scrollBar.setBounds(messageArea.removeFromRight(12));

    // Audio recorder (if visible)
    if (showAudioRecorder && audioSnippetRecorder)
    {
        auto recorderArea = bounds.removeFromBottom(AUDIO_RECORDER_HEIGHT);
        audioSnippetRecorder->setBounds(recorderArea);
        audioSnippetRecorder->setVisible(true);
    }
    else if (audioSnippetRecorder)
    {
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
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);

    // Position error state component in message area
    if (errorStateComponent != nullptr)
    {
        auto errorArea = getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(INPUT_HEIGHT);
        errorStateComponent->setBounds(errorArea);
    }
}

void MessageThread::mouseUp(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    if (getBackButtonBounds().contains(pos))
    {
        if (onBackPressed)
            onBackPressed();
        return;
    }

    // Audio button (toggle audio recorder)
    if (getAudioButtonBounds().contains(pos))
    {
        showAudioRecorder = !showAudioRecorder;
        resized();
        repaint();
        return;
    }

    // Header menu button (for group channels)
    if (isGroupChannel() && getHeaderMenuButtonBounds().contains(pos))
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Add Members");
        menu.addItem(2, "Remove Members");
        menu.addItem(3, "Rename Group");
        menu.addSeparator();
        menu.addItem(4, "Leave Group");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(pos.x, pos.y, 1, 1)),
            [this](int result) {
                if (result == 1)
                {
                    showAddMembersDialog();
                }
                else if (result == 2)
                {
                    showRemoveMembersDialog();
                }
                else if (result == 3)
                {
                    renameGroup();
                }
                else if (result == 4)
                {
                    leaveGroup();
                }
            });
        return;
    }

    if (getSendButtonBounds().contains(pos))
    {
        sendMessage();
        return;
    }

    // Cancel reply button
    if (!replyingToMessageId.isEmpty())
    {
        auto cancelBounds = getCancelReplyButtonBounds();
        if (cancelBounds.contains(pos))
        {
            cancelReply();
            return;
        }
    }

    // Check for clicks on reaction pills
    for (const auto& pill : reactionPills)
    {
        if (pill.bounds.contains(pos))
        {
            toggleReaction(pill.messageId, pill.reactionType);
            return;
        }
    }

    // Check for clicks on parent message preview (to scroll to parent)
    for (const auto& message : messages)
    {
        juce::String replyToId = getReplyToMessageId(message);
        if (!replyToId.isEmpty())
        {
            auto messageBounds = getMessageBounds(message);
            if (!messageBounds.isEmpty() && messageBounds.contains(pos))
            {
                // Check if click is in parent preview area (top 40px of message bubble)
                auto parentPreviewArea = messageBounds.withHeight(40);
                if (parentPreviewArea.contains(pos) && !event.mods.isRightButtonDown())
                {
                    scrollToMessage(replyToId);
                    return;
                }
            }
        }
    }

    // Right-click on message for actions menu
    if (event.mods.isRightButtonDown())
    {
        // Find which message was clicked
        for (const auto& message : messages)
        {
            auto messageBounds = getMessageBounds(message);
            if (messageBounds.contains(pos))
            {
                showMessageActionsMenu(message, event.getScreenPosition());
                return;
            }
        }
    }
}

void MessageThread::mouseWheelMove([[maybe_unused]] const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    scrollPosition -= wheel.deltaY * 30.0;
    scrollPosition = juce::jlimit(0.0, scrollBar.getMaximumRangeLimit(), scrollPosition);
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
    repaint();
}

void MessageThread::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &messageInput)
    {
        sendMessage();
    }
}

void MessageThread::textEditorTextChanged(juce::TextEditor& editor)
{
    // Send typing indicator when user is typing
    if (&editor == &messageInput && streamChatClient && !channelType.isEmpty() && !channelId.isEmpty())
    {
        bool hasText = !editor.getText().trim().isEmpty();

        // Send typing start when user has typed something
        if (hasText && !isTyping)
        {
            isTyping = true;
            streamChatClient->sendTypingIndicator(channelType, channelId, true);
            lastTypingTime = juce::Time::currentTimeMillis();
        }

        // Reset typing time if user is still typing
        if (hasText)
        {
            lastTypingTime = juce::Time::currentTimeMillis();
        }
    }
}

//==============================================================================
void MessageThread::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;

    // Set up callback for real-time message updates
    if (streamChatClient)
    {
        streamChatClient->setMessageReceivedCallback([this](const StreamChatClient::Message& message, const juce::String& msgChannelId) {
            // Only handle messages for our current channel
            if (msgChannelId == channelId)
            {
                Log::debug("MessageThread: Real-time message received");

                // Add message to our list
                messages.push_back(message);

                // Update UI on message thread
                juce::MessageManager::callAsync([this]() {
                    // Scroll to bottom to show new message
                    int totalHeight = calculateTotalMessagesHeight();
                    int visibleHeight = getHeight() - HEADER_HEIGHT - INPUT_HEIGHT;
                    scrollPosition = juce::jmax(0.0, static_cast<double>(totalHeight - visibleHeight));
                    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
                    resized();
                    repaint();
                });
            }
        });
    }
}

void MessageThread::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
}

void MessageThread::loadChannel(const juce::String& type, const juce::String& id)
{
    channelType = type;
    channelId = id;

    Log::info("MessageThread: Loading channel " + type + "/" + id);

    // First get channel details for the name
    if (streamChatClient && streamChatClient->isAuthenticated())
    {
        // Start watching this channel for real-time updates
        streamChatClient->watchChannel(type, id);

        streamChatClient->getChannel(type, id, [this](Outcome<StreamChatClient::Channel> channelResult) {
            if (channelResult.isOk())
            {
                auto channel = channelResult.getValue();
                currentChannel = channel;  // Store full channel data
                channelName = channel.name.isNotEmpty() ? channel.name : "Direct Message";

                // Now load messages
                loadMessages();
            }
            else
            {
                Log::error("MessageThread: Failed to get channel - " + channelResult.getError());
                channelName = "Conversation";
                loadMessages(); // Still try to load messages
            }
            repaint();
        });
    }
    else
    {
        threadState = ThreadState::Error;
        errorMessage = "Not authenticated";

        // Configure and show error state component
        if (errorStateComponent != nullptr)
        {
            errorStateComponent->setErrorType(ErrorState::ErrorType::Auth);
            errorStateComponent->setMessage("Please sign in to view your messages.");
            errorStateComponent->setVisible(true);
        }
        repaint();
    }
}

void MessageThread::loadMessages()
{
    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        threadState = ThreadState::Error;
        errorMessage = "Not authenticated";

        // Configure and show error state component
        if (errorStateComponent != nullptr)
        {
            errorStateComponent->setErrorType(ErrorState::ErrorType::Auth);
            errorStateComponent->setMessage("Please sign in to view your messages.");
            errorStateComponent->setVisible(true);
        }
        repaint();
        return;
    }

    threadState = ThreadState::Loading;

    // Hide error state while loading
    if (errorStateComponent != nullptr)
        errorStateComponent->setVisible(false);

    repaint();

    streamChatClient->queryMessages(channelType, channelId, 50, 0, [this](Outcome<std::vector<StreamChatClient::Message>> result) {
        if (result.isOk())
        {
            messages = result.getValue();
            Log::info("MessageThread: Loaded " + juce::String(messages.size()) + " messages");
            threadState = messages.empty() ? ThreadState::Empty : ThreadState::Loaded;

            // Hide error state on success
            if (errorStateComponent != nullptr)
                errorStateComponent->setVisible(false);

            // Mark channel as read
            streamChatClient->markChannelRead(channelType, channelId, [](Outcome<void> readResult) {
                if (readResult.isError())
                    Log::warn("Failed to mark channel as read");
            });

            // Scroll to bottom to show newest messages
            juce::MessageManager::callAsync([this]() {
                int totalHeight = calculateTotalMessagesHeight();
                int visibleHeight = getHeight() - HEADER_HEIGHT - INPUT_HEIGHT;
                scrollPosition = juce::jmax(0.0, static_cast<double>(totalHeight - visibleHeight));
                scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
                resized();
                repaint();
            });
        }
        else
        {
            Log::error("MessageThread: Failed to load messages - " + result.getError());
            threadState = ThreadState::Error;
            errorMessage = "Failed to load messages";

            // Configure and show error state component
            if (errorStateComponent != nullptr)
            {
                errorStateComponent->configureFromError(result.getError());
                errorStateComponent->setVisible(true);
            }
        }
        repaint();
    });
}

void MessageThread::sendMessage()
{
    juce::String text = messageInput.getText().trim();
    if (text.isEmpty())
        return;

    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        Log::warn("Cannot send message: not authenticated");
        return;
    }

    // Check if we're editing a message
    if (!editingMessageId.isEmpty())
    {
        // Update existing message
        streamChatClient->updateMessage(channelType, channelId, editingMessageId, text, [this](Outcome<StreamChatClient::Message> result) {
            if (result.isOk())
            {
                Log::info("MessageThread: Message updated successfully");
                editingMessageId = "";
                editingMessageText = "";
                messageInput.setText("");
                messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
                resized();  // Update layout
                loadMessages();
            }
            else
            {
                Log::error("MessageThread: Failed to update message - " + result.getError());
            }
        });
        return;
    }

    // Prepare extra data for reply
    juce::var extraData;
    if (!replyingToMessageId.isEmpty())
    {
        extraData = juce::var(new juce::DynamicObject());
        auto* obj = extraData.getDynamicObject();
        obj->setProperty("reply_to", replyingToMessageId);
    }

    messageInput.setText("");
    replyingToMessageId = "";
    replyingToMessage = StreamChatClient::Message();  // Clear reply message
    messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
    resized();  // Update layout to remove reply preview

    streamChatClient->sendMessage(channelType, channelId, text, extraData, [this](Outcome<StreamChatClient::Message> result) {
        if (result.isOk())
        {
            Log::info("MessageThread: Message sent successfully");
            // Reload messages to include the new one
            loadMessages();
        }
        else
        {
            Log::error("MessageThread: Failed to send message - " + result.getError());
            juce::MessageManager::callAsync([result]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Error",
                    "Failed to send message: " + result.getError());
            });
        }
    });
}

void MessageThread::timerCallback()
{
    // Check if we need to stop typing indicator (3 seconds of inactivity)
    if (isTyping && streamChatClient)
    {
        int64_t now = juce::Time::currentTimeMillis();
        if (now - lastTypingTime > 3000)
        {
            isTyping = false;
            streamChatClient->sendTypingIndicator(channelType, channelId, false);
        }
    }

    // Clear typing indicator from other user after 4 seconds
    if (!typingUserName.isEmpty())
    {
        // This will be cleared by the timer - typing indicator auto-expires
        // The polling will pick up new typing events
    }

    // Reload messages periodically (less frequently since we have polling)
    if (threadState == ThreadState::Loaded || threadState == ThreadState::Empty)
    {
        // Only reload via timer if not using real-time watching
        // The watchChannel already polls every 2 seconds
    }
}

//==============================================================================
void MessageThread::drawHeader(juce::Graphics& g)
{
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
    if (isGroupChannel())
    {
        auto menuBounds = getHeaderMenuButtonBounds();
        g.setColour(juce::Colour(0xff888888));
        g.setFont(20.0f);
        g.drawText("⋯", menuBounds, juce::Justification::centred);  // Three dots
    }

    // Bottom border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawHorizontalLine(HEADER_HEIGHT - 1, 0.0f, static_cast<float>(getWidth()));
}

void MessageThread::drawMessages(juce::Graphics& g)
{
    int y = HEADER_HEIGHT - static_cast<int>(scrollPosition);
    int width = getWidth() - scrollBar.getWidth();

    // Calculate bottom area height (reply preview + input)
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;

    for (const auto& message : messages)
    {
        int messageHeight = calculateMessageHeight(message, MESSAGE_MAX_WIDTH);

        // Only draw if visible
        if (y + messageHeight > HEADER_HEIGHT && y < getHeight() - bottomAreaHeight)
        {
            drawMessageBubble(g, message, y, width);
        }
        else
        {
            y += messageHeight + MESSAGE_BUBBLE_PADDING;
        }
    }
}

void MessageThread::drawMessageBubble(juce::Graphics& g, const StreamChatClient::Message& message, int& y, int width)
{
    bool ownMessage = isOwnMessage(message);
    int bubbleMaxWidth = MESSAGE_MAX_WIDTH;
    int bubblePadding = 10;

    // Check if this is a reply
    juce::String replyToId = getReplyToMessageId(message);
    const StreamChatClient::Message* parentMessage = findParentMessage(replyToId);
    bool isReply = parentMessage != nullptr;
    int threadIndent = isReply ? 20 : 0;  // Indent replies

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
    widthLayout.createLayout(widthAttrStr, 10000.0f);  // Large width for width calculation
    int textWidth = juce::jmin(bubbleMaxWidth - 2 * bubblePadding - threadIndent,
                               static_cast<int>(widthLayout.getWidth()) + 2 * bubblePadding);

    // Ensure minimum width for shared content
    if (hasSharedContent)
        textWidth = juce::jmax(textWidth, 200);  // Wider for shared content cards
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
    int bubbleHeight = textHeight + 2 * bubblePadding + 20 + parentPreviewHeight + sharedContentHeight; // Extra for timestamp + parent preview + shared content
    int bubbleWidth = textWidth + 2 * bubblePadding;

    // Position bubble (indent replies)
    int bubbleX;
    if (ownMessage)
    {
        bubbleX = width - bubbleWidth - 15 - threadIndent; // Right aligned, indented if reply
    }
    else
    {
        bubbleX = 15 + threadIndent; // Left aligned, indented if reply
    }

    auto bubbleBounds = juce::Rectangle<int>(bubbleX, y, bubbleWidth, bubbleHeight);

    // Draw bubble background
    juce::Colour bubbleColor = ownMessage ? SidechainColors::primary() : juce::Colour(0xff3a3a3a);
    g.setColour(bubbleColor);
    g.fillRoundedRectangle(bubbleBounds.toFloat(), 12.0f);

    // Draw parent message preview for replies
    if (isReply && parentMessage)
    {

        // Parent preview area (above message text)
        auto parentPreviewBounds = bubbleBounds.withHeight(parentPreviewHeight - 5).reduced(bubblePadding, 5);

        // Left border (accent color)
        g.setColour(SidechainColors::primary());
        g.fillRect(parentPreviewBounds.withWidth(3));

        // Parent message sender name
        g.setColour(juce::Colour(0xff888888));
        g.setFont(10.0f);
        juce::String parentSender = parentMessage->userName.isEmpty() ? "User" : parentMessage->userName;
        g.drawText(parentSender, parentPreviewBounds.withTrimmedLeft(8).withHeight(12),
                   juce::Justification::centredLeft);

        // Parent message text (truncated)
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(11.0f);
        juce::String parentText = parentMessage->text;
        if (parentText.length() > 50)
            parentText = parentText.substring(0, 50) + "...";
        g.drawText(parentText, parentPreviewBounds.withTrimmedLeft(8).withTrimmedTop(12),
                   juce::Justification::centredLeft);

        // Divider line
        g.setColour(juce::Colour(0xff4a4a4a));
        g.drawHorizontalLine(parentPreviewBounds.getBottom() - 1,
                            static_cast<float>(parentPreviewBounds.getX()),
                            static_cast<float>(parentPreviewBounds.getRight()));
    }

    // Draw message text (if any)
    if (!message.text.isEmpty())
    {
        g.setColour(juce::Colours::white);
        auto textBounds = bubbleBounds.reduced(bubblePadding).withTrimmedTop(parentPreviewHeight).withTrimmedBottom(16 + sharedContentHeight);
        layout.draw(g, textBounds.toFloat());
    }

    // Draw shared content preview (post or story)
    if (hasSharedContent)
    {
        int sharedContentY = bubbleBounds.getY() + bubblePadding + parentPreviewHeight + textHeight;
        auto sharedContentBounds = juce::Rectangle<int>(
            bubbleBounds.getX() + bubblePadding,
            sharedContentY,
            bubbleBounds.getWidth() - 2 * bubblePadding,
            sharedContentHeight - 5  // Slight padding
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
    if (!ownMessage && !message.userName.isEmpty())
    {
        g.setColour(juce::Colour(0xff888888));
        g.setFont(11.0f);
        g.drawText(message.userName, bubbleX, y - 16, bubbleWidth, 14, juce::Justification::bottomLeft);
    }

    y += bubbleHeight + MESSAGE_BUBBLE_PADDING;

    // Draw reactions below the bubble
    drawMessageReactions(g, message, y, bubbleX, bubbleWidth);
}

void MessageThread::drawEmptyState(juce::Graphics& g)
{
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

void MessageThread::drawErrorState(juce::Graphics& g)
{
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
    auto bounds = getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(bottomAreaHeight);

    g.setColour(juce::Colour(0xffff4444));
    g.setFont(16.0f);
    g.drawText("Error: " + errorMessage, bounds, juce::Justification::centred);
}

void MessageThread::drawInputArea(juce::Graphics& g)
{
    auto inputAreaBounds = juce::Rectangle<int>(0, getHeight() - INPUT_HEIGHT, getWidth(), INPUT_HEIGHT);

    // Background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(inputAreaBounds);

    // Top border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawHorizontalLine(getHeight() - INPUT_HEIGHT, 0.0f, static_cast<float>(getWidth()));

    // Typing indicator (above input area)
    if (!typingUserName.isEmpty())
    {
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(12.0f);
        juce::String typingText = typingUserName + " is typing...";
        g.drawText(typingText, inputAreaBounds.withTrimmedBottom(INPUT_HEIGHT - 15).reduced(15, 0),
                   juce::Justification::centredLeft);
    }

    // Reply preview (above input area)
    if (!replyingToMessageId.isEmpty() && !replyingToMessage.id.isEmpty())
    {
        auto replyBounds = getReplyPreviewBounds();
        if (!replyBounds.isEmpty())
        {
            // Background
            g.setColour(juce::Colour(0xff252525));
            g.fillRect(replyBounds);

            // Border on top
            g.setColour(juce::Colour(0xff3a3a3a));
            g.drawHorizontalLine(replyBounds.getY(), 0.0f, static_cast<float>(getWidth()));

            // Left border (accent color)
            g.setColour(SidechainColors::primary());
            g.fillRect(replyBounds.withWidth(4));

            // Reply header
            g.setColour(SidechainColors::primary());
            g.setFont(11.0f);
            juce::String replyHeader = "Replying to " + replyingToMessage.userName;
            g.drawText(replyHeader, replyBounds.withTrimmedLeft(15).withTrimmedBottom(REPLY_PREVIEW_HEIGHT - 20).withHeight(16),
                       juce::Justification::centredLeft);

            // Quoted message text (truncated if too long)
            g.setColour(juce::Colour(0xffaaaaaa));
            g.setFont(11.0f);
            juce::String quotedText = replyingToMessage.text;
            if (quotedText.length() > 60)
                quotedText = quotedText.substring(0, 60) + "...";
            g.drawText(quotedText, replyBounds.withTrimmedLeft(15).withTrimmedTop(18).reduced(0, 2),
                       juce::Justification::centredLeft);

            // Cancel button (X)
            auto cancelBounds = getCancelReplyButtonBounds();
            g.setColour(juce::Colour(0xff888888));
            g.setFont(16.0f);
            g.drawText("×", cancelBounds, juce::Justification::centred);
        }
    }
    else if (!editingMessageId.isEmpty())
    {
        g.setColour(SidechainColors::primary());
        g.setFont(11.0f);
        g.drawText("Editing message...", inputAreaBounds.withTrimmedBottom(INPUT_HEIGHT - 15).reduced(15, 0),
                   juce::Justification::centredLeft);
    }

    // Audio button
    auto audioBounds = getAudioButtonBounds();
    g.setColour(showAudioRecorder ? SidechainColors::primary() : juce::Colour(0xff3a3a3a));
    g.fillRoundedRectangle(audioBounds.toFloat(), 6.0f);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("MIC", audioBounds, juce::Justification::centred);

    // Send button
    auto sendBounds = getSendButtonBounds();
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(sendBounds.toFloat(), 6.0f);
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("Send", sendBounds, juce::Justification::centred);
}

//==============================================================================
void MessageThread::scrollBarMoved([[maybe_unused]] juce::ScrollBar* bar, double newRangeStart)
{
    scrollPosition = newRangeStart;
    repaint();
}

juce::String MessageThread::formatTimestamp(const juce::String& timestamp)
{
    if (timestamp.isEmpty())
        return "";

    return StringFormatter::formatTimeAgo(timestamp);
}

int MessageThread::calculateMessageHeight(const StreamChatClient::Message& message, int maxWidth) const
{
    juce::Font font(juce::FontOptions().withHeight(14.0f));
    int bubblePadding = 10;

    // Check if this is a reply (add parent preview height)
    juce::String replyToId = getReplyToMessageId(message);
    bool isReply = !replyToId.isEmpty() && findParentMessage(replyToId) != nullptr;
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

int MessageThread::calculateTotalMessagesHeight()
{
    int totalHeight = 0;
    for (const auto& message : messages)
    {
        totalHeight += calculateMessageHeight(message, MESSAGE_MAX_WIDTH);
    }
    return totalHeight;
}

bool MessageThread::isOwnMessage(const StreamChatClient::Message& message) const
{
    return message.userId == currentUserId;
}

juce::Rectangle<int> MessageThread::getBackButtonBounds() const
{
    return juce::Rectangle<int>(10, 10, 40, 40);
}

juce::Rectangle<int> MessageThread::getHeaderMenuButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 45, 10, 40, 40);
}

juce::Rectangle<int> MessageThread::getAudioButtonBounds() const
{
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
    if (showAudioRecorder && audioSnippetRecorder)
        bottomAreaHeight += AUDIO_RECORDER_HEIGHT;
    return juce::Rectangle<int>(getWidth() - 140, getHeight() - bottomAreaHeight + 10, 40, INPUT_HEIGHT - 20);
}

juce::Rectangle<int> MessageThread::getSendButtonBounds() const
{
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
    if (showAudioRecorder && audioSnippetRecorder)
        bottomAreaHeight += AUDIO_RECORDER_HEIGHT;
    return juce::Rectangle<int>(getWidth() - 90, getHeight() - bottomAreaHeight + 10, 80, INPUT_HEIGHT - 20);
}

juce::Rectangle<int> MessageThread::getMessageBounds(const StreamChatClient::Message& message) const
{
    int y = HEADER_HEIGHT - static_cast<int>(scrollPosition);
    int width = getWidth() - scrollBar.getWidth();
    int bubbleMaxWidth = MESSAGE_MAX_WIDTH;
    int bubblePadding = 10;

    for (const auto& msg : messages)
    {
        if (msg.id == message.id)
        {
            // Calculate the same way as drawMessageBubble
            juce::Font font(juce::FontOptions().withHeight(14.0f));
            juce::AttributedString widthAttrStr;
            widthAttrStr.setText(msg.text);
            widthAttrStr.setFont(font);
            juce::TextLayout widthLayout;
            widthLayout.createLayout(widthAttrStr, 10000.0f);  // Large width for width calculation
            int textWidth = juce::jmin(bubbleMaxWidth - 2 * bubblePadding,
                                       static_cast<int>(widthLayout.getWidth()) + 2 * bubblePadding);
            textWidth = juce::jmax(textWidth, 100);

            juce::AttributedString attrStr;
            attrStr.setText(msg.text);
            attrStr.setFont(font);

            juce::TextLayout layout;
            layout.createLayout(attrStr, static_cast<float>(textWidth));
            int textHeight = static_cast<int>(layout.getHeight());

            int bubbleHeight = textHeight + 2 * bubblePadding + 20;
            int bubbleWidth = textWidth + 2 * bubblePadding;

            int bubbleX = isOwnMessage(msg) ? (width - bubbleWidth - 15) : 15;
            return juce::Rectangle<int>(bubbleX, y, bubbleWidth, bubbleHeight);
        }
        y += calculateMessageHeight(msg, MESSAGE_MAX_WIDTH) + MESSAGE_BUBBLE_PADDING;
    }
    return juce::Rectangle<int>();
}

void MessageThread::showMessageActionsMenu(const StreamChatClient::Message& message, const juce::Point<int>& screenPos)
{
    juce::PopupMenu menu;
    bool ownMessage = isOwnMessage(message);

    // React is always available
    menu.addItem(1, "React...");
    menu.addSeparator();

    // Copy is always available
    menu.addItem(2, "Copy");

    if (ownMessage)
    {
        // Only allow editing/deleting own messages
        // Edit only if message is less than 5 minutes old (getstream.io limit)
        // For now, we'll allow edit for all own messages - getstream.io will enforce the limit
        menu.addItem(3, "Edit");
        menu.addItem(4, "Delete");
    }
    else
    {
        // Reply to others' messages
        menu.addItem(5, "Reply");
        menu.addSeparator();
        menu.addItem(6, "Report");
        menu.addItem(7, "Block User");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos, juce::Point<int>(1, 1))),
        [this, message, ownMessage, screenPos](int result) {
            if (result == 1)
            {
                showQuickReactionPicker(message, screenPos);
            }
            else if (result == 2)
            {
                copyMessageText(message.text);
            }
            else if (result == 3 && ownMessage)
            {
                editMessage(message);
            }
            else if (result == 4 && ownMessage)
            {
                deleteMessage(message);
            }
            else if (result == 5 && !ownMessage)
            {
                replyToMessage(message);
            }
            else if (result == 6 && !ownMessage)
            {
                reportMessage(message);
            }
            else if (result == 7 && !ownMessage)
            {
                blockUser(message);
            }
        });
}

void MessageThread::copyMessageText(const juce::String& text)
{
    juce::SystemClipboard::copyTextToClipboard(text);
    Log::info("MessageThread: Copied message text to clipboard");
}

void MessageThread::editMessage(const StreamChatClient::Message& message)
{
    editingMessageId = message.id;
    editingMessageText = message.text;
    replyingToMessageId = "";  // Clear reply state when editing
    replyingToMessage = StreamChatClient::Message();
    messageInput.setText(message.text);
    messageInput.setHighlightedRegion({ 0, message.text.length() });
    messageInput.grabKeyboardFocus();
    resized();  // Update layout
    repaint();
    Log::info("MessageThread: Editing message " + message.id);
}

void MessageThread::deleteMessage(const StreamChatClient::Message& message)
{
    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        Log::warn("Cannot delete message: not authenticated");
        return;
    }

    streamChatClient->deleteMessage(channelType, channelId, message.id, [this](Outcome<void> result) {
        if (result.isOk())
        {
            Log::info("MessageThread: Message deleted successfully");
            // Reload messages to update UI
            loadMessages();
        }
        else
        {
            Log::error("MessageThread: Failed to delete message - " + result.getError());
        }
    });
}

void MessageThread::replyToMessage(const StreamChatClient::Message& message)
{
    replyingToMessageId = message.id;
    replyingToMessage = message;  // Store full message for preview
    messageInput.setText("");
    messageInput.setTextToShowWhenEmpty("Type a reply...", juce::Colour(0xff888888));
    messageInput.grabKeyboardFocus();
    repaint();  // Redraw to show reply preview
    Log::info("MessageThread: Replying to message " + message.id);
}

void MessageThread::cancelReply()
{
    replyingToMessageId = "";
    replyingToMessage = StreamChatClient::Message();  // Clear message
    messageInput.setText("");
    messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
    repaint();
}

juce::Rectangle<int> MessageThread::getReplyPreviewBounds() const
{
    if (replyingToMessageId.isEmpty())
        return juce::Rectangle<int>();

    return juce::Rectangle<int>(0, getHeight() - INPUT_HEIGHT - REPLY_PREVIEW_HEIGHT, getWidth(), REPLY_PREVIEW_HEIGHT);
}

juce::Rectangle<int> MessageThread::getCancelReplyButtonBounds() const
{
    auto previewBounds = getReplyPreviewBounds();
    if (previewBounds.isEmpty())
        return juce::Rectangle<int>();

    return previewBounds.removeFromRight(40).reduced(5);
}

juce::String MessageThread::getReplyToMessageId(const StreamChatClient::Message& message) const
{
    if (message.extraData.isObject())
    {
        auto* obj = message.extraData.getDynamicObject();
        if (obj != nullptr)
        {
            return obj->getProperty("reply_to").toString();
        }
    }
    return "";
}

const StreamChatClient::Message* MessageThread::findParentMessage(const juce::String& messageId) const
{
    if (messageId.isEmpty())
        return nullptr;

    for (const auto& msg : messages)
    {
        if (msg.id == messageId)
            return &msg;
    }
    return nullptr;
}

void MessageThread::scrollToMessage(const juce::String& messageId)
{
    if (messageId.isEmpty())
        return;

    // Find message position
    int y = HEADER_HEIGHT;
    for (const auto& msg : messages)
    {
        if (msg.id == messageId)
        {
            // Scroll to show this message
            int totalHeight = calculateTotalMessagesHeight();
            int visibleHeight = getHeight() - HEADER_HEIGHT - INPUT_HEIGHT;
            if (!replyingToMessageId.isEmpty())
                visibleHeight -= REPLY_PREVIEW_HEIGHT;

            // Center the message in view
            double centerOffset = static_cast<double>(visibleHeight) / 2.0;
            scrollPosition = juce::jlimit(0.0, static_cast<double>(totalHeight - visibleHeight),
                                         static_cast<double>(y) - centerOffset);
            scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
            resized();
            repaint();
            return;
        }
        y += calculateMessageHeight(msg, MESSAGE_MAX_WIDTH) + MESSAGE_BUBBLE_PADDING;
    }
}

void MessageThread::reportMessage(const StreamChatClient::Message& message)
{
    if (!networkClient || !networkClient->isAuthenticated())
    {
        Log::warn("Cannot report message: not authenticated");
        return;
    }

    // Show a simple popup menu with report reasons
    juce::PopupMenu reasonMenu;
    reasonMenu.addItem(1, "Spam");
    reasonMenu.addItem(2, "Harassment");
    reasonMenu.addItem(3, "Inappropriate Content");
    reasonMenu.addItem(4, "Other");

    reasonMenu.showMenuAsync(juce::PopupMenu::Options(),
        [this, message](int reasonCode) {
            if (reasonCode == 0) return; // User cancelled

            juce::String reason;
            switch (reasonCode)
            {
                case 1: reason = "spam"; break;
                case 2: reason = "harassment"; break;
                case 3: reason = "inappropriate"; break;
                case 4: reason = "other"; break;
                default: return;
            }

            // Report the user who sent the message via backend
            // Since we're reporting a message, we report the user who sent it
            juce::String userId = message.userId;
            if (userId.isEmpty())
                return;

            juce::String url = networkClient->getBaseUrl() + "/api/v1/users/" + userId + "/report";
            juce::var data = juce::var(new juce::DynamicObject());
            auto* obj = data.getDynamicObject();
            obj->setProperty("reason", reason);
            obj->setProperty("description", "Reported from message: " + message.text.substring(0, 100));

            networkClient->postAbsolute(url, data, [](Outcome<juce::var> result) {
                if (result.isOk())
                {
                    Log::info("MessageThread: Message reported successfully");
                    juce::MessageManager::callAsync([]() {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::InfoIcon,
                            "Report Submitted",
                            "Thank you for reporting this message. We will review it shortly.");
                    });
                }
                else
                {
                    Log::error("MessageThread: Failed to report message - " + result.getError());
                    juce::MessageManager::callAsync([result]() {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Error",
                            "Failed to report message: " + result.getError());
                    });
                }
            });
        });
}

void MessageThread::blockUser(const StreamChatClient::Message& message)
{
    if (!networkClient || !networkClient->isAuthenticated())
    {
        Log::warn("Cannot block user: not authenticated");
        return;
    }

    juce::String userId = message.userId;
    if (userId.isEmpty())
        return;

    // Block user via backend
    juce::String url = networkClient->getBaseUrl() + "/api/v1/users/" + userId + "/block";

    networkClient->postAbsolute(url, juce::var(), [this, userId](Outcome<juce::var> result) {
        if (result.isOk())
        {
            Log::info("MessageThread: User blocked successfully");

            // Remove blocked user's messages from view
            juce::MessageManager::callAsync([this, userId]() {
                messages.erase(
                    std::remove_if(messages.begin(), messages.end(),
                        [userId](const StreamChatClient::Message& msg) {
                            return msg.userId == userId;
                        }),
                    messages.end());

                // Reload messages to get updated list
                loadMessages();
            });
        }
        else
        {
            Log::error("MessageThread: Failed to block user - " + result.getError());
            juce::MessageManager::callAsync([result]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Error",
                    "Failed to block user: " + result.getError());
            });
        }
    });
}

bool MessageThread::isGroupChannel() const
{
    return channelType == "team" || (!channelName.isEmpty() && channelName != "Direct Message");
}

void MessageThread::leaveGroup()
{
    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        Log::warn("Cannot leave group: not authenticated");
        return;
    }

    if (!isGroupChannel())
    {
        Log::warn("Cannot leave: not a group channel");
        return;
    }

    streamChatClient->leaveChannel(channelType, channelId, [this](Outcome<void> result) {
        if (result.isOk())
        {
            Log::info("MessageThread: Left group successfully");

            // Navigate back to messages list
            juce::MessageManager::callAsync([this]() {
                if (onChannelClosed)
                    onChannelClosed(channelType, channelId);
                if (onBackPressed)
                    onBackPressed();
            });
        }
        else
        {
            Log::error("MessageThread: Failed to leave group - " + result.getError());
            juce::MessageManager::callAsync([result]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Error",
                    "Failed to leave group: " + result.getError());
            });
        }
    });
}

void MessageThread::renameGroup()
{
    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        Log::warn("Cannot rename group: not authenticated");
        return;
    }

    if (!isGroupChannel())
    {
        Log::warn("Cannot rename: not a group channel");
        return;
    }

    // Show input dialog using JUCE AlertWindow
    juce::AlertWindow alert("Rename Group", "Enter a new name for this group:", juce::MessageBoxIconType::QuestionIcon);
    alert.addTextEditor("name", channelName, "Group name:");
    alert.addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert.enterModalState(true, juce::ModalCallbackFunction::create([this, &alert](int result) {
        if (result == 1)
        {
            juce::String newName = alert.getTextEditorContents("name").trim();
            if (newName.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Invalid Name", "Group name cannot be empty.");
                return;
            }

            if (newName == channelName)
            {
                return;  // No change
            }

            streamChatClient->updateChannel(channelType, channelId, newName, juce::var(),
                [this, channelNameNew = newName](Outcome<StreamChatClient::Channel> result) {
                    if (result.isOk())
                    {
                        auto updatedChannel = result.getValue();
                        channelName = updatedChannel.name;
                        currentChannel = updatedChannel;

                        juce::MessageManager::callAsync([this]() {
                            repaint();
                        });

                        Log::info("MessageThread: Group renamed successfully");
                    }
                    else
                    {
                        Log::error("MessageThread: Failed to rename group - " + result.getError());
                        juce::MessageManager::callAsync([result]() {
                            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                "Error", "Failed to rename group: " + result.getError());
                        });
                    }
                });
        }
    }));
}

void MessageThread::showAddMembersDialog()
{
    if (!isGroupChannel())
    {
        return;
    }

    if (!streamChatClient || !networkClient)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Error",
            "Cannot add members: chat client not initialized.");
        return;
    }

    // Create user picker dialog if needed
    if (!userPickerDialog)
    {
        userPickerDialog = std::make_unique<UserPickerDialog>();
        userPickerDialog->setNetworkClient(networkClient);
        userPickerDialog->setCurrentUserId(currentUserId);
    }

    // Exclude current members from search results
    juce::Array<juce::String> excludedIds;
    excludedIds.add(currentUserId);  // Exclude self
    if (currentChannel.members.isArray())
    {
        auto* membersArray = currentChannel.members.getArray();
        for (int i = 0; i < membersArray->size(); ++i)
        {
            auto member = (*membersArray)[i];
            if (member.isObject())
            {
                auto* memberObj = member.getDynamicObject();
                if (memberObj != nullptr && memberObj->hasProperty("user_id"))
                {
                    juce::String memberId = memberObj->getProperty("user_id").toString();
                    if (memberId.isNotEmpty() && !excludedIds.contains(memberId))
                    {
                        excludedIds.add(memberId);
                    }
                }
            }
        }
    }
    userPickerDialog->setExcludedUserIds(excludedIds);

    // Set callback for when users are selected
    userPickerDialog->onUsersSelected = [this](const juce::Array<juce::String>& selectedUserIds) {
        if (selectedUserIds.isEmpty())
        {
            return;
        }

        // Convert to vector for StreamChatClient
        std::vector<juce::String> memberIds;
        for (const auto& id : selectedUserIds)
        {
            memberIds.push_back(id);
        }

        // Add members to channel
        streamChatClient->addMembers(channelType, channelId, memberIds,
            [this](Outcome<void> result) {
                if (result.isOk())
                {
                    Log::info("MessageThread: Members added successfully");
                    juce::MessageManager::callAsync([this]() {
                        // Reload channel to get updated member list
                        loadChannel(channelType, channelId);
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::InfoIcon,
                            "Success",
                            "Members added successfully.");
                    });
                }
                else
                {
                    Log::error("MessageThread: Failed to add members - " + result.getError());
                    juce::MessageManager::callAsync([result]() {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Error",
                            "Failed to add members: " + result.getError());
                    });
                }
            });
    };

    // Show dialog
    userPickerDialog->showModal(this);
}

void MessageThread::showRemoveMembersDialog()
{
    if (!isGroupChannel())
    {
        return;
    }

    if (!streamChatClient)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Error",
            "Cannot remove members: chat client not initialized.");
        return;
    }

    // Parse members from channel data
    auto members = currentChannel.members;
    if (!members.isArray())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Remove Members",
            "No members found in this group.");
        return;
    }

    auto* membersArray = members.getArray();
    if (membersArray->size() <= 1)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Remove Members",
            "This group doesn't have enough members to remove.");
        return;
    }

    // Build list of removable members (exclude self)
    juce::Array<juce::String> memberIds;
    juce::Array<juce::String> memberNames;
    for (int i = 0; i < membersArray->size(); ++i)
    {
        auto member = (*membersArray)[i];
        if (member.isObject())
        {
            auto* memberObj = member.getDynamicObject();
            if (memberObj != nullptr && memberObj->hasProperty("user_id"))
            {
                juce::String memberId = memberObj->getProperty("user_id").toString();
                // Don't allow removing self
                if (memberId != currentUserId && memberId.isNotEmpty())
                {
                    memberIds.add(memberId);
                    // Get member name if available
                    juce::String memberName = "User";
                    if (memberObj->hasProperty("name"))
                    {
                        memberName = memberObj->getProperty("name").toString();
                    }
                    else if (memberObj->hasProperty("username"))
                    {
                        memberName = memberObj->getProperty("username").toString();
                    }
                    memberNames.add(memberName);
                }
            }
        }
    }

    if (memberIds.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "Remove Members",
            "No removable members found.");
        return;
    }

    // For simplicity, just remove the first member in the list with confirmation
    // (JUCE 8 doesn't support showMessageBoxAsync with custom buttons and callbacks the same way)
    if (memberIds.isEmpty())
        return;

    juce::String memberIdToRemove = memberIds[0];
    juce::String memberName = memberNames[0];

    auto opts = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Confirm Removal")
        .withMessage("Are you sure you want to remove " + memberName + " from this group?")
        .withButton("Remove")
        .withButton("Cancel");

    juce::AlertWindow::showAsync(opts, [this, memberIdToRemove, memberName](int confirmResult) {
        if (confirmResult == 1)  // Remove button clicked
        {
            std::vector<juce::String> memberIdsToRemove;
            memberIdsToRemove.push_back(memberIdToRemove);

            streamChatClient->removeMembers(channelType, channelId, memberIdsToRemove,
                [this, memberName](Outcome<void> result) {
                    if (result.isOk())
                    {
                        Log::info("MessageThread: Member removed successfully");
                        juce::MessageManager::callAsync([this, memberName]() {
                            // Reload channel to get updated member list
                            loadChannel(channelType, channelId);
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::InfoIcon,
                                "Success",
                                memberName + " has been removed from the group.");
                        });
                    }
                    else
                    {
                        Log::error("MessageThread: Failed to remove member - " + result.getError());
                        juce::MessageManager::callAsync([result]() {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::WarningIcon,
                                "Error",
                                "Failed to remove member: " + result.getError());
                        });
                    }
                });
        }
    });
}

void MessageThread::sendAudioSnippet(const juce::AudioBuffer<float>& audioBuffer, double sampleRate)
{
    if (!streamChatClient || channelType.isEmpty() || channelId.isEmpty())
    {
        Log::error("MessageThread::sendAudioSnippet: Cannot send - missing client or channel");
        return;
    }

    Log::info("MessageThread::sendAudioSnippet: Sending audio snippet - " +
              juce::String(audioBuffer.getNumSamples()) + " samples, " +
              juce::String(sampleRate, 1) + "Hz");

    // Hide recorder after sending
    showAudioRecorder = false;
    resized();
    repaint();

    // Prepare extra data for reply (if replying)
    juce::var extraData;
    if (!replyingToMessageId.isEmpty())
    {
        extraData = juce::var(new juce::DynamicObject());
        auto* obj = extraData.getDynamicObject();
        obj->setProperty("reply_to", replyingToMessageId);
    }

    // Clear reply state
    replyingToMessageId = "";
    replyingToMessage = StreamChatClient::Message();
    resized();

    // Send audio snippet via StreamChatClient
    streamChatClient->sendMessageWithAudio(channelType, channelId, "", audioBuffer, sampleRate,
        [this](Outcome<StreamChatClient::Message> result) {
            if (result.isOk())
            {
                Log::info("MessageThread::sendAudioSnippet: Audio snippet sent successfully");
                // Reload messages to include the new one
                loadMessages();
            }
            else
            {
                Log::error("MessageThread::sendAudioSnippet: Failed to send audio snippet - " + result.getError());
                juce::MessageManager::callAsync([result]() {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Error",
                        "Failed to send audio snippet: " + result.getError());
                });
            }
        });
}

//==============================================================================
// Shared content detection
bool MessageThread::hasSharedPost(const StreamChatClient::Message& message) const
{
    if (!message.extraData.isObject())
        return false;

    auto* obj = message.extraData.getDynamicObject();
    if (obj == nullptr)
        return false;

    auto sharedPost = obj->getProperty("shared_post");
    return sharedPost.isObject();
}

bool MessageThread::hasSharedStory(const StreamChatClient::Message& message) const
{
    if (!message.extraData.isObject())
        return false;

    auto* obj = message.extraData.getDynamicObject();
    if (obj == nullptr)
        return false;

    auto sharedStory = obj->getProperty("shared_story");
    return sharedStory.isObject();
}

int MessageThread::getSharedContentHeight(const StreamChatClient::Message& message) const
{
    if (hasSharedPost(message) || hasSharedStory(message))
        return 80; // Height for shared content card
    return 0;
}

void MessageThread::drawSharedPostPreview(juce::Graphics& g, const StreamChatClient::Message& message,
                                          juce::Rectangle<int> bounds)
{
    if (!message.extraData.isObject())
        return;

    auto* obj = message.extraData.getDynamicObject();
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
    if (key.isNotEmpty())
    {
        if (info.isNotEmpty()) info += " • ";
        info += key;
    }
    if (duration > 0)
    {
        if (info.isNotEmpty()) info += " • ";
        int secs = static_cast<int>(duration);
        info += juce::String(secs / 60) + ":" + juce::String(secs % 60).paddedLeft('0', 2);
    }

    g.setColour(SidechainColors::textPrimary());
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    g.drawText(info, contentBounds.removeFromTop(18), juce::Justification::centredLeft);

    // Genres
    if (genres.isNotEmpty())
    {
        g.setColour(SidechainColors::textMuted());
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.drawText(genres, contentBounds, juce::Justification::centredLeft);
    }
}

void MessageThread::drawSharedStoryPreview(juce::Graphics& g, const StreamChatClient::Message& message,
                                           juce::Rectangle<int> bounds)
{
    if (!message.extraData.isObject())
        return;

    auto* obj = message.extraData.getDynamicObject();
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
    juce::ColourGradient gradient(
        juce::Colour(0xFFFF6B6B), static_cast<float>(gradientBounds.getX()), static_cast<float>(gradientBounds.getY()),
        juce::Colour(0xFF9B59B6), static_cast<float>(gradientBounds.getX()), static_cast<float>(gradientBounds.getBottom()),
        false);
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
    if (duration > 0)
    {
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

std::vector<juce::String> MessageThread::getReactionTypes(const StreamChatClient::Message& message) const
{
    std::vector<juce::String> types;

    if (!message.reactions.isObject())
        return types;

    // getstream.io stores reactions in reaction_groups: { "like": { count: 5, sum_scores: 5 }, ... }
    auto reactionGroups = message.reactions.getProperty("reaction_groups", juce::var());
    if (!reactionGroups.isObject())
        return types;

    auto* obj = reactionGroups.getDynamicObject();
    if (obj == nullptr)
        return types;

    for (auto& prop : obj->getProperties())
        types.push_back(prop.name.toString());

    return types;
}

int MessageThread::getReactionCount(const StreamChatClient::Message& message, const juce::String& reactionType) const
{
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

bool MessageThread::hasUserReacted(const StreamChatClient::Message& message, const juce::String& reactionType) const
{
    if (!message.reactions.isObject())
        return false;

    // Check own_reactions array
    auto ownReactions = message.reactions.getProperty("own_reactions", juce::var());
    if (!ownReactions.isArray())
        return false;

    auto* arr = ownReactions.getArray();
    if (arr == nullptr)
        return false;

    for (int i = 0; i < arr->size(); ++i)
    {
        auto reaction = (*arr)[i];
        if (reaction.isObject())
        {
            auto type = reaction.getProperty("type", "").toString();
            if (type == reactionType)
                return true;
        }
    }

    return false;
}

void MessageThread::addReaction(const juce::String& messageId, const juce::String& reactionType)
{
    if (streamChatClient == nullptr)
        return;

    Log::info("MessageThread: Adding reaction '" + reactionType + "' to message " + messageId);

    streamChatClient->addReaction(channelType, channelId, messageId, reactionType,
        [this, messageId, reactionType](Outcome<void> result) {
            if (result.isOk())
            {
                Log::info("MessageThread: Reaction added successfully");
                loadMessages();  // Reload to get updated reactions
            }
            else
            {
                Log::error("MessageThread: Failed to add reaction - " + result.getError());
            }
        });
}

void MessageThread::removeReaction(const juce::String& messageId, const juce::String& reactionType)
{
    if (streamChatClient == nullptr)
        return;

    Log::info("MessageThread: Removing reaction '" + reactionType + "' from message " + messageId);

    streamChatClient->removeReaction(channelType, channelId, messageId, reactionType,
        [this, messageId, reactionType](Outcome<void> result) {
            if (result.isOk())
            {
                Log::info("MessageThread: Reaction removed successfully");
                loadMessages();  // Reload to get updated reactions
            }
            else
            {
                Log::error("MessageThread: Failed to remove reaction - " + result.getError());
            }
        });
}

void MessageThread::toggleReaction(const juce::String& messageId, const juce::String& reactionType)
{
    // Find the message
    const StreamChatClient::Message* msg = nullptr;
    for (const auto& m : messages)
    {
        if (m.id == messageId)
        {
            msg = &m;
            break;
        }
    }

    if (msg == nullptr)
        return;

    if (hasUserReacted(*msg, reactionType))
        removeReaction(messageId, reactionType);
    else
        addReaction(messageId, reactionType);
}

void MessageThread::drawMessageReactions(juce::Graphics& g, const StreamChatClient::Message& message, int& y, int x, int maxWidth)
{
    auto reactionTypes = getReactionTypes(message);
    if (reactionTypes.empty())
        return;

    // Map reaction types to emojis
    static const std::map<juce::String, juce::String> emojiMap = {
        {"like", "❤️"},
        {"love", "❤️"},
        {"fire", "🔥"},
        {"laugh", "😂"},
        {"wow", "😮"},
        {"sad", "😢"},
        {"pray", "🙏"},
        {"thumbsup", "👍"},
        {"thumbsdown", "👎"},
        {"clap", "👏"}
    };

    int pillHeight = 28;
    int pillPadding = 6;
    int pillSpacing = 6;
    int currentX = x;
    int currentY = y + 4;  // Small gap below message bubble

    for (const auto& type : reactionTypes)
    {
        int count = getReactionCount(message, type);
        if (count == 0)
            continue;

        bool userReacted = hasUserReacted(message, type);

        // Get emoji for this reaction type
        juce::String emoji = type;  // Default to type name
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
        if (currentX + pillWidth > x + maxWidth)
        {
            currentX = x;
            currentY += pillHeight + pillSpacing;
        }

        // Draw pill background
        juce::Rectangle<int> pillBounds(currentX, currentY, pillWidth, pillHeight);

        if (userReacted)
        {
            // Highlighted state - filled with accent color
            g.setColour(SidechainColors::coralPink());
            g.fillRoundedRectangle(pillBounds.toFloat(), 14.0f);
        }
        else
        {
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

void MessageThread::showQuickReactionPicker(const StreamChatClient::Message& message, const juce::Point<int>& screenPos)
{
    // Quick reaction picker with common emojis
    juce::PopupMenu menu;

    // Map menu item IDs to reaction types
    struct ReactionOption
    {
        juce::String display;
        juce::String type;
    };

    std::vector<ReactionOption> reactions = {
        {"❤️ Love", "like"},
        {"🔥 Fire", "fire"},
        {"😂 Laugh", "laugh"},
        {"😮 Wow", "wow"},
        {"😢 Sad", "sad"},
        {"🙏 Pray", "pray"},
        {"👍 Like", "thumbsup"},
        {"👏 Clap", "clap"}
    };

    int itemId = 1;
    for (const auto& reaction : reactions)
    {
        juce::String displayText = reaction.display;
        if (hasUserReacted(message, reaction.type))
            displayText += " ✓";  // Checkmark for already reacted

        menu.addItem(itemId++, displayText);
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
        [this, message, reactions](int result) {
            if (result > 0 && result <= static_cast<int>(reactions.size()))
            {
                const auto& reactionType = reactions[static_cast<size_t>(result - 1)].type;
                toggleReaction(message.id, reactionType);
            }
        });
}
