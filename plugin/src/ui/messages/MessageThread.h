#pragma once

#include <JuceHeader.h>
#include "../../network/StreamChatClient.h"
#include "../common/ErrorState.h"

class NetworkClient;
class SidechainAudioProcessor;

//==============================================================================
/**
 * MessageThread displays a single conversation/chat thread
 *
 * Features:
 * - Header with channel name and back button
 * - Scrollable list of messages (newest at bottom)
 * - Message input field with send button
 * - Message bubbles with different styling for sent/received
 * - Typing indicators (future)
 * - Read receipts (future)
 */
class MessageThread : public juce::Component,
                                public juce::Timer,
                                public juce::ScrollBar::Listener,
                                public juce::TextEditor::Listener
{
public:
    MessageThread();
    ~MessageThread() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // TextEditor::Listener
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;

    // Callbacks
    std::function<void()> onBackPressed;
    std::function<void(const juce::String& channelType, const juce::String& channelId)> onChannelClosed;

    // Set clients
    void setStreamChatClient(StreamChatClient* client);
    void setNetworkClient(NetworkClient* client);
    void setAudioProcessor(SidechainAudioProcessor* processor);

    // Load a specific channel
    void loadChannel(const juce::String& channelType, const juce::String& channelId);
    void setCurrentUserId(const juce::String& userId) { currentUserId = userId; }

    // Timer for auto-refresh
    void timerCallback() override;

private:
    //==============================================================================
    enum class ThreadState
    {
        Loading,
        Loaded,
        Empty,
        Error
    };

    ThreadState threadState = ThreadState::Loading;
    juce::String errorMessage;
    std::vector<StreamChatClient::Message> messages;

    StreamChatClient* streamChatClient = nullptr;
    NetworkClient* networkClient = nullptr;

    juce::String channelType;
    juce::String channelId;
    juce::String channelName;
    juce::String currentUserId;
    StreamChatClient::Channel currentChannel;  // Store full channel data for group management

    // Typing indicator state
    bool isTyping = false;
    int64_t lastTypingTime = 0;
    juce::String typingUserName;  // Name of user who is typing

    // UI elements
    juce::ScrollBar scrollBar;
    juce::TextEditor messageInput;
    double scrollPosition = 0.0;
    static constexpr int HEADER_HEIGHT = 60;
    static constexpr int INPUT_HEIGHT = 60;
    static constexpr int REPLY_PREVIEW_HEIGHT = 60;  // Height for reply preview above input
    static constexpr int AUDIO_RECORDER_HEIGHT = 80;  // Height for audio snippet recorder
    static constexpr int MESSAGE_BUBBLE_MIN_HEIGHT = 50;
    static constexpr int MESSAGE_BUBBLE_PADDING = 12;
    static constexpr int MESSAGE_MAX_WIDTH = 400;

    // Audio snippet recorder
    std::unique_ptr<class AudioSnippetRecorder> audioSnippetRecorder;
    bool showAudioRecorder = false;
    SidechainAudioProcessor* audioProcessor = nullptr;

    // Error state component
    std::unique_ptr<ErrorState> errorStateComponent;

    // Drawing helpers
    void drawHeader(juce::Graphics& g);
    void drawMessages(juce::Graphics& g);
    void drawMessageBubble(juce::Graphics& g, const StreamChatClient::Message& message, int& y, int width);
    void drawEmptyState(juce::Graphics& g);
    void drawErrorState(juce::Graphics& g);
    void drawInputArea(juce::Graphics& g);

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    // Helper methods
    juce::String formatTimestamp(const juce::String& timestamp);
    int calculateMessageHeight(const StreamChatClient::Message& message, int maxWidth) const;
    int calculateTotalMessagesHeight();
    bool isOwnMessage(const StreamChatClient::Message& message) const;

    // Threading helpers
    juce::String getReplyToMessageId(const StreamChatClient::Message& message) const;
    const StreamChatClient::Message* findParentMessage(const juce::String& messageId) const;
    void scrollToMessage(const juce::String& messageId);

    // Message sending
    void sendMessage();
    void loadMessages();

    // Hit test bounds
    juce::Rectangle<int> getBackButtonBounds() const;
    juce::Rectangle<int> getSendButtonBounds() const;
    juce::Rectangle<int> getAudioButtonBounds() const;
    juce::Rectangle<int> getMessageBounds(const StreamChatClient::Message& message) const;
    juce::Rectangle<int> getHeaderMenuButtonBounds() const;

    // Group management
    void leaveGroup();
    void renameGroup();
    void showAddMembersDialog();
    void showRemoveMembersDialog();
    bool isGroupChannel() const;

    // Message actions
    void showMessageActionsMenu(const StreamChatClient::Message& message, const juce::Point<int>& screenPos);
    void copyMessageText(const juce::String& text);
    void editMessage(const StreamChatClient::Message& message);
    void deleteMessage(const StreamChatClient::Message& message);
    void replyToMessage(const StreamChatClient::Message& message);
    void cancelReply();
    void reportMessage(const StreamChatClient::Message& message);
    void blockUser(const StreamChatClient::Message& message);

    // Helper to get reply preview bounds
    juce::Rectangle<int> getReplyPreviewBounds() const;
    juce::Rectangle<int> getCancelReplyButtonBounds() const;

    // Shared content detection and rendering
    bool hasSharedPost(const StreamChatClient::Message& message) const;
    bool hasSharedStory(const StreamChatClient::Message& message) const;
    void drawSharedPostPreview(juce::Graphics& g, const StreamChatClient::Message& message,
                                juce::Rectangle<int> bounds);
    void drawSharedStoryPreview(juce::Graphics& g, const StreamChatClient::Message& message,
                                 juce::Rectangle<int> bounds);
    int getSharedContentHeight(const StreamChatClient::Message& message) const;

    // Reply/edit state
    juce::String replyingToMessageId;
    StreamChatClient::Message replyingToMessage;  // Full message being replied to
    juce::String editingMessageId;
    juce::String editingMessageText;

    // Audio snippet sending
    void sendAudioSnippet(const juce::AudioBuffer<float>& audioBuffer, double sampleRate);

    // User picker dialog for adding members
    std::unique_ptr<class UserPickerDialog> userPickerDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessageThread)
};
