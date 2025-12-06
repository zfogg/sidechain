#include "MessageThreadComponent.h"
#include "AudioSnippetRecorder.h"
#include "UserPickerDialog.h"
#include "../../network/NetworkClient.h"
#include "../../PluginProcessor.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringFormatter.h"
#include "../../util/Colors.h"
#include <algorithm>

//==============================================================================
MessageThreadComponent::MessageThreadComponent()
    : scrollBar(true)
{
    Log::info("MessageThreadComponent: Initializing");

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

    startTimer(5000); // Refresh every 5 seconds
}

void MessageThreadComponent::setAudioProcessor(SidechainAudioProcessor* processor)
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

MessageThreadComponent::~MessageThreadComponent()
{
    Log::debug("MessageThreadComponent: Destroying");
    stopTimer();

    // Stop watching channel for real-time updates
    if (streamChatClient)
    {
        streamChatClient->unwatchChannel();
        streamChatClient->setMessageReceivedCallback(nullptr);
    }
}

void MessageThreadComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

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
            drawErrorState(g);
            break;

        case ThreadState::Loaded:
            drawMessages(g);
            break;
    }

    drawInputArea(g);
}

void MessageThreadComponent::resized()
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
}

void MessageThreadComponent::mouseUp(const juce::MouseEvent& event)
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

void MessageThreadComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    scrollPosition -= wheel.deltaY * 30.0;
    scrollPosition = juce::jlimit(0.0, scrollBar.getMaximumRangeLimit(), scrollPosition);
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
    repaint();
}

void MessageThreadComponent::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &messageInput)
    {
        sendMessage();
    }
}

void MessageThreadComponent::textEditorTextChanged(juce::TextEditor& editor)
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
void MessageThreadComponent::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;

    // Set up callback for real-time message updates
    if (streamChatClient)
    {
        streamChatClient->setMessageReceivedCallback([this](const StreamChatClient::Message& message, const juce::String& msgChannelId) {
            // Only handle messages for our current channel
            if (msgChannelId == channelId)
            {
                Log::debug("MessageThreadComponent: Real-time message received");

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

void MessageThreadComponent::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
}

void MessageThreadComponent::loadChannel(const juce::String& type, const juce::String& id)
{
    channelType = type;
    channelId = id;

    Log::info("MessageThreadComponent: Loading channel " + type + "/" + id);

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
                Log::error("MessageThreadComponent: Failed to get channel - " + channelResult.getError());
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
        repaint();
    }
}

void MessageThreadComponent::loadMessages()
{
    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        threadState = ThreadState::Error;
        errorMessage = "Not authenticated";
        repaint();
        return;
    }

    threadState = ThreadState::Loading;
    repaint();

    streamChatClient->queryMessages(channelType, channelId, 50, 0, [this](Outcome<std::vector<StreamChatClient::Message>> result) {
        if (result.isOk())
        {
            messages = result.getValue();
            Log::info("MessageThreadComponent: Loaded " + juce::String(messages.size()) + " messages");
            threadState = messages.empty() ? ThreadState::Empty : ThreadState::Loaded;

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
            Log::error("MessageThreadComponent: Failed to load messages - " + result.getError());
            threadState = ThreadState::Error;
            errorMessage = "Failed to load messages";
        }
        repaint();
    });
}

void MessageThreadComponent::sendMessage()
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
                Log::info("MessageThreadComponent: Message updated successfully");
                editingMessageId = "";
                editingMessageText = "";
                messageInput.setText("");
                messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
                resized();  // Update layout
                loadMessages();
            }
            else
            {
                Log::error("MessageThreadComponent: Failed to update message - " + result.getError());
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
            Log::info("MessageThreadComponent: Message sent successfully");
            // Reload messages to include the new one
            loadMessages();
        }
        else
        {
            Log::error("MessageThreadComponent: Failed to send message - " + result.getError());
            juce::MessageManager::callAsync([result]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Error",
                    "Failed to send message: " + result.getError());
            });
        }
    });
}

void MessageThreadComponent::timerCallback()
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
void MessageThreadComponent::drawHeader(juce::Graphics& g)
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

void MessageThreadComponent::drawMessages(juce::Graphics& g)
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

void MessageThreadComponent::drawMessageBubble(juce::Graphics& g, const StreamChatClient::Message& message, int& y, int width)
{
    bool ownMessage = isOwnMessage(message);
    int bubbleMaxWidth = MESSAGE_MAX_WIDTH;
    int bubblePadding = 10;

    // Check if this is a reply
    juce::String replyToId = getReplyToMessageId(message);
    const StreamChatClient::Message* parentMessage = findParentMessage(replyToId);
    bool isReply = parentMessage != nullptr;
    int threadIndent = isReply ? 20 : 0;  // Indent replies

    // Calculate text bounds
    juce::Font font(14.0f);
    g.setFont(font);

    int textWidth = juce::jmin(bubbleMaxWidth - 2 * bubblePadding - threadIndent,
                               static_cast<int>(font.getStringWidthFloat(message.text)) + 2 * bubblePadding);
    textWidth = juce::jmax(textWidth, 100);

    // Calculate height based on wrapped text
    juce::AttributedString attrStr;
    attrStr.setText(message.text);
    attrStr.setFont(font);
    attrStr.setColour(juce::Colours::white);

    juce::TextLayout layout;
    layout.createLayout(attrStr, static_cast<float>(textWidth));
    int textHeight = static_cast<int>(layout.getHeight());

    // Account for parent message preview
    int parentPreviewHeight = isReply ? 40 : 0;
    int bubbleHeight = textHeight + 2 * bubblePadding + 20 + parentPreviewHeight; // Extra for timestamp + parent preview
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

    // Draw message text
    g.setColour(juce::Colours::white);
    auto textBounds = bubbleBounds.reduced(bubblePadding).withTrimmedTop(parentPreviewHeight).withTrimmedBottom(16);
    layout.draw(g, textBounds.toFloat());

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
}

void MessageThreadComponent::drawEmptyState(juce::Graphics& g)
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

void MessageThreadComponent::drawErrorState(juce::Graphics& g)
{
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
    auto bounds = getLocalBounds().withTrimmedTop(HEADER_HEIGHT).withTrimmedBottom(bottomAreaHeight);

    g.setColour(juce::Colour(0xffff4444));
    g.setFont(16.0f);
    g.drawText("Error: " + errorMessage, bounds, juce::Justification::centred);
}

void MessageThreadComponent::drawInputArea(juce::Graphics& g)
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
    g.drawText("🎤", audioBounds, juce::Justification::centred);

    // Send button
    auto sendBounds = getSendButtonBounds();
    g.setColour(SidechainColors::primary());
    g.fillRoundedRectangle(sendBounds.toFloat(), 6.0f);
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("Send", sendBounds, juce::Justification::centred);
}

//==============================================================================
void MessageThreadComponent::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    scrollPosition = newRangeStart;
    repaint();
}

juce::String MessageThreadComponent::formatTimestamp(const juce::String& timestamp)
{
    if (timestamp.isEmpty())
        return "";

    return StringFormatter::formatTimeAgo(timestamp);
}

int MessageThreadComponent::calculateMessageHeight(const StreamChatClient::Message& message, int maxWidth) const
{
    juce::Font font(14.0f);
    int bubblePadding = 10;

    // Check if this is a reply (add parent preview height)
    juce::String replyToId = getReplyToMessageId(message);
    bool isReply = !replyToId.isEmpty() && findParentMessage(replyToId) != nullptr;
    int parentPreviewHeight = isReply ? 40 : 0;
    int threadIndent = isReply ? 20 : 0;

    juce::AttributedString attrStr;
    attrStr.setText(message.text);
    attrStr.setFont(font);

    juce::TextLayout layout;
    layout.createLayout(attrStr, static_cast<float>(maxWidth - 2 * bubblePadding - threadIndent));

    int textHeight = static_cast<int>(layout.getHeight());
    return textHeight + 2 * bubblePadding + 20 + parentPreviewHeight + MESSAGE_BUBBLE_PADDING; // Text + padding + timestamp + parent preview + gap
}

int MessageThreadComponent::calculateTotalMessagesHeight()
{
    int totalHeight = 0;
    for (const auto& message : messages)
    {
        totalHeight += calculateMessageHeight(message, MESSAGE_MAX_WIDTH);
    }
    return totalHeight;
}

bool MessageThreadComponent::isOwnMessage(const StreamChatClient::Message& message) const
{
    return message.userId == currentUserId;
}

juce::Rectangle<int> MessageThreadComponent::getBackButtonBounds() const
{
    return juce::Rectangle<int>(10, 10, 40, 40);
}

juce::Rectangle<int> MessageThreadComponent::getHeaderMenuButtonBounds() const
{
    return juce::Rectangle<int>(getWidth() - 45, 10, 40, 40);
}

juce::Rectangle<int> MessageThreadComponent::getAudioButtonBounds() const
{
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
    if (showAudioRecorder && audioSnippetRecorder)
        bottomAreaHeight += AUDIO_RECORDER_HEIGHT;
    return juce::Rectangle<int>(getWidth() - 140, getHeight() - bottomAreaHeight + 10, 40, INPUT_HEIGHT - 20);
}

juce::Rectangle<int> MessageThreadComponent::getSendButtonBounds() const
{
    int bottomAreaHeight = INPUT_HEIGHT;
    if (!replyingToMessageId.isEmpty())
        bottomAreaHeight += REPLY_PREVIEW_HEIGHT;
    if (showAudioRecorder && audioSnippetRecorder)
        bottomAreaHeight += AUDIO_RECORDER_HEIGHT;
    return juce::Rectangle<int>(getWidth() - 90, getHeight() - bottomAreaHeight + 10, 80, INPUT_HEIGHT - 20);
}

juce::Rectangle<int> MessageThreadComponent::getMessageBounds(const StreamChatClient::Message& message) const
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
            juce::Font font(14.0f);
            int textWidth = juce::jmin(bubbleMaxWidth - 2 * bubblePadding,
                                       static_cast<int>(font.getStringWidthFloat(msg.text)) + 2 * bubblePadding);
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

void MessageThreadComponent::showMessageActionsMenu(const StreamChatClient::Message& message, const juce::Point<int>& screenPos)
{
    juce::PopupMenu menu;
    bool ownMessage = isOwnMessage(message);

    // Copy is always available
    menu.addItem(1, "Copy");

    if (ownMessage)
    {
        // Only allow editing/deleting own messages
        // Edit only if message is less than 5 minutes old (getstream.io limit)
        // For now, we'll allow edit for all own messages - getstream.io will enforce the limit
        menu.addItem(2, "Edit");
        menu.addItem(3, "Delete");
    }
    else
    {
        // Reply to others' messages
        menu.addItem(4, "Reply");
        menu.addSeparator();
        menu.addItem(5, "Report");
        menu.addItem(6, "Block User");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos, juce::Point<int>(1, 1))),
        [this, message, ownMessage](int result) {
            if (result == 1)
            {
                copyMessageText(message.text);
            }
            else if (result == 2 && ownMessage)
            {
                editMessage(message);
            }
            else if (result == 3 && ownMessage)
            {
                deleteMessage(message);
            }
            else if (result == 4 && !ownMessage)
            {
                replyToMessage(message);
            }
            else if (result == 5 && !ownMessage)
            {
                reportMessage(message);
            }
            else if (result == 6 && !ownMessage)
            {
                blockUser(message);
            }
        });
}

void MessageThreadComponent::copyMessageText(const juce::String& text)
{
    juce::SystemClipboard::copyTextToClipboard(text);
    Log::info("MessageThreadComponent: Copied message text to clipboard");
}

void MessageThreadComponent::editMessage(const StreamChatClient::Message& message)
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
    Log::info("MessageThreadComponent: Editing message " + message.id);
}

void MessageThreadComponent::deleteMessage(const StreamChatClient::Message& message)
{
    if (!streamChatClient || !streamChatClient->isAuthenticated())
    {
        Log::warn("Cannot delete message: not authenticated");
        return;
    }

    streamChatClient->deleteMessage(channelType, channelId, message.id, [this](Outcome<void> result) {
        if (result.isOk())
        {
            Log::info("MessageThreadComponent: Message deleted successfully");
            // Reload messages to update UI
            loadMessages();
        }
        else
        {
            Log::error("MessageThreadComponent: Failed to delete message - " + result.getError());
        }
    });
}

void MessageThreadComponent::replyToMessage(const StreamChatClient::Message& message)
{
    replyingToMessageId = message.id;
    replyingToMessage = message;  // Store full message for preview
    messageInput.setText("");
    messageInput.setTextToShowWhenEmpty("Type a reply...", juce::Colour(0xff888888));
    messageInput.grabKeyboardFocus();
    repaint();  // Redraw to show reply preview
    Log::info("MessageThreadComponent: Replying to message " + message.id);
}

void MessageThreadComponent::cancelReply()
{
    replyingToMessageId = "";
    replyingToMessage = StreamChatClient::Message();  // Clear message
    messageInput.setText("");
    messageInput.setTextToShowWhenEmpty("Type a message...", juce::Colour(0xff888888));
    repaint();
}

juce::Rectangle<int> MessageThreadComponent::getReplyPreviewBounds() const
{
    if (replyingToMessageId.isEmpty())
        return juce::Rectangle<int>();

    return juce::Rectangle<int>(0, getHeight() - INPUT_HEIGHT - REPLY_PREVIEW_HEIGHT, getWidth(), REPLY_PREVIEW_HEIGHT);
}

juce::Rectangle<int> MessageThreadComponent::getCancelReplyButtonBounds() const
{
    auto previewBounds = getReplyPreviewBounds();
    if (previewBounds.isEmpty())
        return juce::Rectangle<int>();

    return previewBounds.removeFromRight(40).reduced(5);
}

juce::String MessageThreadComponent::getReplyToMessageId(const StreamChatClient::Message& message) const
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

const StreamChatClient::Message* MessageThreadComponent::findParentMessage(const juce::String& messageId) const
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

void MessageThreadComponent::scrollToMessage(const juce::String& messageId)
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

void MessageThreadComponent::reportMessage(const StreamChatClient::Message& message)
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
                    Log::info("MessageThreadComponent: Message reported successfully");
                    juce::MessageManager::callAsync([]() {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::InfoIcon,
                            "Report Submitted",
                            "Thank you for reporting this message. We will review it shortly.");
                    });
                }
                else
                {
                    Log::error("MessageThreadComponent: Failed to report message - " + result.getError());
                    juce::MessageManager::callAsync([result]() {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon,
                            "Error",
                            "Failed to report message: " + result.getError());
                    });
                }
            });
        });
}

void MessageThreadComponent::blockUser(const StreamChatClient::Message& message)
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
            Log::info("MessageThreadComponent: User blocked successfully");

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
            Log::error("MessageThreadComponent: Failed to block user - " + result.getError());
            juce::MessageManager::callAsync([result]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Error",
                    "Failed to block user: " + result.getError());
            });
        }
    });
}

bool MessageThreadComponent::isGroupChannel() const
{
    return channelType == "team" || (!channelName.isEmpty() && channelName != "Direct Message");
}

void MessageThreadComponent::leaveGroup()
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
            Log::info("MessageThreadComponent: Left group successfully");

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
            Log::error("MessageThreadComponent: Failed to leave group - " + result.getError());
            juce::MessageManager::callAsync([result]() {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Error",
                    "Failed to leave group: " + result.getError());
            });
        }
    });
}

void MessageThreadComponent::renameGroup()
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
    juce::AlertWindow alert("Rename Group", "Enter a new name for this group:", juce::AlertWindow::QuestionIcon);
    alert.addTextEditor("name", channelName, "Group name:");
    alert.addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert.enterModalState(true, juce::ModalCallbackFunction::create([this, &alert](int result) {
        if (result == 1)
        {
            juce::String newName = alert.getTextEditorContents("name").trim();
            if (newName.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Invalid Name", "Group name cannot be empty.");
                return;
            }

            if (newName == channelName)
            {
                return;  // No change
            }

            streamChatClient->updateChannel(channelType, channelId, newName, juce::var(),
                [this, newName](Outcome<StreamChatClient::Channel> result) {
                    if (result.isOk())
                    {
                        auto updatedChannel = result.getValue();
                        channelName = updatedChannel.name;
                        currentChannel = updatedChannel;

                        juce::MessageManager::callAsync([this]() {
                            repaint();
                        });

                        Log::info("MessageThreadComponent: Group renamed successfully");
                    }
                    else
                    {
                        Log::error("MessageThreadComponent: Failed to rename group - " + result.getError());
                        juce::MessageManager::callAsync([result]() {
                            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                "Error", "Failed to rename group: " + result.getError());
                        });
                    }
                });
        }
    }));
}

void MessageThreadComponent::showAddMembersDialog()
{
    if (!isGroupChannel() || !streamChatClient || !networkClient)
    {
        Log::warn("Cannot add members: not a group channel or clients not set");
        return;
    }

    // Create user picker dialog if needed
    if (!userPickerDialog)
    {
        userPickerDialog = std::make_unique<UserPickerDialog>();
        userPickerDialog->setNetworkClient(networkClient);
        userPickerDialog->setCurrentUserId(currentUserId);

        // Exclude current members from results
        juce::Array<juce::String> excludedIds;
        if (currentChannel.members.isArray())
        {
            auto* membersArray = currentChannel.members.getArray();
            if (membersArray != nullptr)
            {
                for (int i = 0; i < membersArray->size(); ++i)
                {
                    auto member = (*membersArray)[i];
                    juce::String memberId;
                    if (member.isObject())
                    {
                        memberId = member.getProperty("user_id", "").toString();
                        if (memberId.isEmpty())
                            memberId = member.getProperty("id", "").toString();
                    }
                    else if (member.isString())
                    {
                        memberId = member.toString();
                    }
                    if (memberId.isNotEmpty())
                        excludedIds.add(memberId);
                }
            }
        }
        userPickerDialog->setExcludedUserIds(excludedIds);

        // Set callback for when users are selected
        userPickerDialog->onUsersSelected = [this](const juce::Array<juce::String>& selectedUserIds) {
            if (selectedUserIds.size() == 0 || !streamChatClient)
                return;

            // Convert to vector for API call
            std::vector<juce::String> memberIds;
            for (const auto& id : selectedUserIds)
                memberIds.push_back(id);

            streamChatClient->addMembers(channelType, channelId, memberIds,
                [this](Outcome<void> result) {
                    if (result.isOk())
                    {
                        Log::info("MessageThreadComponent: Members added successfully");
                        // Reload channel to get updated member list
                        loadChannel(channelType, channelId);
                        juce::MessageManager::callAsync([this]() {
                            repaint();
                        });
                    }
                    else
                    {
                        Log::error("MessageThreadComponent: Failed to add members - " + result.getError());
                        juce::MessageManager::callAsync([result]() {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::AlertWindow::WarningIcon,
                                "Error",
                                "Failed to add members: " + result.getError());
                        });
                    }
                });
        };
    }

    userPickerDialog->showModal(this);
}

void MessageThreadComponent::showRemoveMembersDialog()
{
    if (!isGroupChannel() || !streamChatClient)
    {
        return;
    }

    // Parse members from channel data
    auto members = currentChannel.members;
    if (!members.isArray())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Remove Members",
            "Unable to load member list.");
        return;
    }

    auto* membersArray = members.getArray();
    if (membersArray == nullptr || membersArray->size() <= 2)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Remove Members",
            "This group doesn't have enough members to remove.");
        return;
    }

    // Build list of current members (excluding current user)
    juce::Array<juce::String> memberIds;
    juce::Array<juce::String> memberNames;
    for (int i = 0; i < membersArray->size(); ++i)
    {
        auto member = (*membersArray)[i];
        juce::String memberId;
        juce::String memberName;

        if (member.isObject())
        {
            memberId = member.getProperty("user_id", "").toString();
            if (memberId.isEmpty())
                memberId = member.getProperty("id", "").toString();

            auto user = member.getProperty("user", juce::var());
            if (user.isObject())
            {
                memberName = user.getProperty("name", "").toString();
                if (memberName.isEmpty())
                    memberName = user.getProperty("username", "").toString();
            }
        }
        else if (member.isString())
        {
            memberId = member.toString();
        }

        // Exclude current user
        if (memberId.isNotEmpty() && memberId != currentUserId)
        {
            memberIds.add(memberId);
            memberNames.add(memberName.isNotEmpty() ? memberName : memberId);
        }
    }

    if (memberIds.size() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Remove Members",
            "No other members to remove.");
        return;
    }

    // Show selection dialog using AlertWindow with checkboxes
    juce::AlertWindow alert("Remove Members", "Select members to remove:", juce::AlertWindow::QuestionIcon);

    // Add checkboxes for each member
    for (int i = 0; i < memberIds.size(); ++i)
    {
        alert.addToggleButton(memberNames[i], false, "member_" + juce::String(i));
    }

    alert.addButton("Remove", 1);
    alert.addButton("Cancel", 0);

    alert.enterModalState(true, juce::ModalCallbackFunction::create([this, memberIds](int result) {
        if (result == 1)
        {
            // Collect selected member IDs
            std::vector<juce::String> idsToRemove;
            for (int i = 0; i < memberIds.size(); ++i)
            {
                // Check if this member was selected (we'd need to track the alert window state)
                // For now, use a simpler approach: show individual confirmation dialogs
            }

            // For simplicity, show a menu to select one member at a time
            juce::PopupMenu menu;
            for (int i = 0; i < memberIds.size(); ++i)
            {
                menu.addItem(i + 1, "Remove " + memberIds[i]);
            }

            menu.showMenuAsync(juce::PopupMenu::Options(), [this, memberIds](int selectedIndex) {
                if (selectedIndex > 0 && selectedIndex <= memberIds.size())
                {
                    int index = selectedIndex - 1;
                    juce::String memberId = memberIds[index];

                    // Confirm removal
                    auto options = juce::MessageBoxOptions()
                        .withTitle("Remove Member")
                        .withMessage("Are you sure you want to remove this member from the group?")
                        .withButton("Remove")
                        .withButton("Cancel");

                    juce::AlertWindow::showAsync(options, [this, memberId](int confirmResult) {
                        if (confirmResult == 1 && streamChatClient)
                        {
                            std::vector<juce::String> idsToRemove = {memberId};
                            streamChatClient->removeMembers(channelType, channelId, idsToRemove,
                                [this](Outcome<void> result) {
                                    if (result.isOk())
                                    {
                                        Log::info("MessageThreadComponent: Member removed successfully");
                                        // Reload channel
                                        loadChannel(channelType, channelId);
                                        juce::MessageManager::callAsync([this]() {
                                            repaint();
                                        });
                                    }
                                    else
                                    {
                                        Log::error("MessageThreadComponent: Failed to remove member - " + result.getError());
                                        juce::MessageManager::callAsync([result]() {
                                            juce::AlertWindow::showMessageBoxAsync(
                                                juce::AlertWindow::WarningIcon,
                                                "Error",
                                                "Failed to remove member: " + result.getError());
                                        });
                                    }
                                });
                        }
                    });
                }
            });
        }
    }));
}

void MessageThreadComponent::sendAudioSnippet(const juce::AudioBuffer<float>& audioBuffer, double sampleRate)
{
    if (!streamChatClient || channelType.isEmpty() || channelId.isEmpty())
    {
        Log::error("MessageThreadComponent::sendAudioSnippet: Cannot send - missing client or channel");
        return;
    }

    Log::info("MessageThreadComponent::sendAudioSnippet: Sending audio snippet - " +
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
                Log::info("MessageThreadComponent::sendAudioSnippet: Audio snippet sent successfully");
                // Reload messages to include the new one
                loadMessages();
            }
            else
            {
                Log::error("MessageThreadComponent::sendAudioSnippet: Failed to send audio snippet - " + result.getError());
                juce::MessageManager::callAsync([result]() {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Error",
                        "Failed to send audio snippet: " + result.getError());
                });
            }
        });
}
