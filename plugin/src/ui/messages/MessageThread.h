#pragma once

#include "../../network/StreamChatClient.h"
#include "../../stores/AppStore.h"
#include "../../ui/common/AppStoreComponent.h"
#include "../../ui/common/SmoothScrollable.h"
#include "../../util/reactive/ReactiveBoundComponent.h"
#include "../common/ErrorState.h"
#include <JuceHeader.h>

class NetworkClient;
class SidechainAudioProcessor;

// ==============================================================================
/**
 * MessageThread displays a single conversation/chat thread
 *
 * Features:
 * - Header with channel name and back button
 * - Scrollable list of messages (newest at bottom)
 * - Message input field with send button
 * - Message bubbles with different styling for sent/received
 * - Typing indicators (reactive via ChatState)
 * - Read receipts (future)
 *
 * Thread Safety:
 * - Uses AppStoreComponent for automatic reactive updates
 * - All UI operations must be on the message thread
 * - ChatState subscription automatically triggers repaint()
 */
class MessageThread : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChatState>,
                      public Sidechain::UI::SmoothScrollable,
                      public juce::Timer,
                      public juce::TextEditor::Listener {
public:
  MessageThread(Sidechain::Stores::AppStore *store = nullptr);
  ~MessageThread() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // TextEditor::Listener
  void textEditorReturnKeyPressed(juce::TextEditor &editor) override;
  void textEditorTextChanged(juce::TextEditor &editor) override;

  // Callbacks
  std::function<void()> onBackPressed;
  std::function<void(const juce::String &channelType, const juce::String &channelId)> onChannelClosed;
  std::function<void(const juce::String &postId)> onSharedPostClicked;   // Navigate to full post view
  std::function<void(const juce::String &storyId)> onSharedStoryClicked; // Navigate to story viewer

  // Set clients
  void setStreamChatClient(StreamChatClient *client);
  void setNetworkClient(NetworkClient *client);
  void setAudioProcessor(SidechainAudioProcessor *processor);

  // Load a specific channel
  void loadChannel(const juce::String &channelType, const juce::String &channelId);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Public method to send a message (for testing)
  void sendTestMessage(const juce::String &text) {
    messageInput.setText(text);
    sendMessage();
  }

  // Timer for auto-refresh
  void timerCallback() override;

protected:
  // ==============================================================================
  // SmoothScrollable implementation
  void onScrollUpdate(double newScrollPosition) override {
    scrollBar.setCurrentRangeStart(newScrollPosition, juce::dontSendNotification);
    repaint();
  }

  // ==============================================================================
  // AppStoreComponent methods
  void subscribeToAppStore() {}
  void unsubscribeFromAppStore() {}

  juce::String getComponentName() const override {
    return "MessageThread";
  }

  int getScrollableWidth(int scrollBarWidth) const override {
    return getWidth() - scrollBarWidth;
  }

  // ==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const Sidechain::Stores::ChatState &state) override;

private:
  // ==============================================================================
  // All state now comes from ChatStore - no duplicate state!
  // - Messages: chatStore->getState.getCurrentChannel->messages
  // - Loading: chatStore->getState.getCurrentChannel->isLoadingMessages
  // - Error: chatStore->getState.error
  // - Typing: chatStore->getState.getCurrentChannel->usersTyping

  StreamChatClient *streamChatClient = nullptr;
  NetworkClient *networkClient = nullptr;

  juce::String channelType;
  juce::String channelId;
  juce::String channelName;
  juce::String currentUserId;
  StreamChatClient::Channel currentChannel; // Store full channel data for group management

  // Typing indicator state (local state for current user's typing)
  bool isTyping = false;      // Is current user typing?
  int64_t lastTypingTime = 0; // When current user last typed
  // Note: Typing indicators from other users come from ChatStore.usersTyping

  // UI elements
  juce::ScrollBar scrollBar;
  juce::TextEditor messageInput;
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int INPUT_HEIGHT = 60;
  static constexpr int MESSAGE_INPUT_HEIGHT = 60;  // Alias for INPUT_HEIGHT
  static constexpr int REPLY_PREVIEW_HEIGHT = 60;  // Height for reply preview above input
  static constexpr int AUDIO_RECORDER_HEIGHT = 80; // Height for audio snippet recorder
  static constexpr int MESSAGE_BUBBLE_MIN_HEIGHT = 50;
  static constexpr int MESSAGE_BUBBLE_PADDING = 12;
  static constexpr int MESSAGE_TOP_PADDING = 16; // Top padding for messages container
  static constexpr int MESSAGE_MAX_WIDTH = 400;

  // Audio snippet recorder
  std::unique_ptr<class AudioSnippetRecorder> audioSnippetRecorder;
  bool showAudioRecorder = false;
  SidechainAudioProcessor *audioProcessor = nullptr;

  // Error state component
  std::unique_ptr<ErrorState> errorStateComponent;

  // Drawing helpers
  void drawHeader(juce::Graphics &g);
  void drawMessages(juce::Graphics &g);
  void drawMessageBubble(juce::Graphics &g, const StreamChatClient::Message &message, int &y, int width);
  void drawMessageReactions(juce::Graphics &g, const StreamChatClient::Message &message, int &y, int x, int maxWidth);
  void drawEmptyState(juce::Graphics &g);
  void drawErrorState(juce::Graphics &g);
  void drawInputArea(juce::Graphics &g);

  // ScrollBar::Listener
  // Helper methods
  juce::String formatTimestamp(const juce::String &timestamp);
  int calculateMessageHeight(const StreamChatClient::Message &message, int maxWidth) const;
  int calculateTotalMessagesHeight();
  bool isOwnMessage(const StreamChatClient::Message &message) const;

  // Threading helpers
  juce::String getReplyToMessageId(const StreamChatClient::Message &message) const;
  std::optional<StreamChatClient::Message> findParentMessage(const juce::String &messageId) const;
  void scrollToMessage(const juce::String &messageId);

  // Message sending (state managed by AppStore)
  void sendMessage();
  void loadMessages();

  // Hit test bounds
  juce::Rectangle<int> getBackButtonBounds() const;
  juce::Rectangle<int> getSendButtonBounds() const;
  juce::Rectangle<int> getAudioButtonBounds() const;
  juce::Rectangle<int> getMessageBounds(const StreamChatClient::Message &message) const;
  juce::Rectangle<int> getHeaderMenuButtonBounds() const;
  juce::Rectangle<int> getSharedPostBounds(const StreamChatClient::Message &message) const;
  juce::Rectangle<int> getSharedStoryBounds(const StreamChatClient::Message &message) const;

  // Group management
  void leaveGroup();
  void renameGroup();
  void showAddMembersDialog();
  void showRemoveMembersDialog();
  bool isGroupChannel() const;

  // Message actions
  void showMessageActionsMenu(const StreamChatClient::Message &message, const juce::Point<int> &screenPos);
  void copyMessageText(const juce::String &text);
  void editMessage(const StreamChatClient::Message &message);
  void deleteMessage(const StreamChatClient::Message &message);
  void replyToMessage(const StreamChatClient::Message &message);
  void cancelReply();

private:
  void reportMessage(const StreamChatClient::Message &message);
  void blockUser(const StreamChatClient::Message &message);

  // Reaction actions
  void showQuickReactionPicker(const StreamChatClient::Message &message, const juce::Point<int> &screenPos);
  void addReaction(const juce::String &messageId, const juce::String &reactionType);
  void removeReaction(const juce::String &messageId, const juce::String &reactionType);
  void toggleReaction(const juce::String &messageId, const juce::String &reactionType);
  bool hasUserReacted(const StreamChatClient::Message &message, const juce::String &reactionType) const;
  int getReactionCount(const StreamChatClient::Message &message, const juce::String &reactionType) const;
  std::vector<juce::String> getReactionTypes(const StreamChatClient::Message &message) const;
  juce::Rectangle<int> getReactionBounds(const StreamChatClient::Message &message,
                                         const juce::String &reactionType) const;

  // Helper to get reply preview bounds
  juce::Rectangle<int> getReplyPreviewBounds() const;
  juce::Rectangle<int> getCancelReplyButtonBounds() const;

  // Shared content detection and rendering
  bool hasSharedPost(const StreamChatClient::Message &message) const;
  bool hasSharedStory(const StreamChatClient::Message &message) const;
  void drawSharedPostPreview(juce::Graphics &g, const StreamChatClient::Message &message, juce::Rectangle<int> bounds);
  void drawSharedStoryPreview(juce::Graphics &g, const StreamChatClient::Message &message, juce::Rectangle<int> bounds);
  int getSharedContentHeight(const StreamChatClient::Message &message) const;

  // Reply/edit state
  juce::String replyingToMessageId;
  StreamChatClient::Message replyingToMessage; // Full message being replied to
  juce::String editingMessageId;
  juce::String editingMessageText;

  // Reaction state
  struct ReactionPill {
    juce::String messageId;
    juce::String reactionType;
    juce::Rectangle<int> bounds;
    int count;
    bool userReacted;
  };
  std::vector<ReactionPill> reactionPills; // Cached reaction pill bounds for hit testing

  // Audio snippet sending
  void sendAudioSnippet(const juce::AudioBuffer<float> &audioBuffer, double sampleRate);

  // User picker dialog for adding members
  std::unique_ptr<class UserPickerDialog> userPickerDialog;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessageThread)
};
