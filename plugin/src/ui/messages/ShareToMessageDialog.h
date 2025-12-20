#pragma once

#include "../../models/FeedPost.h"
#include "../../models/Story.h"
#include "../../network/StreamChatClient.h"
#include <JuceHeader.h>

class NetworkClient;

// ==============================================================================
/**
 * ShareToMessageDialog - Modal dialog for sharing posts to conversations
 *
 * Features:
 * - Preview of post being shared
 * - Recent conversations list
 * - User search for new conversations
 * - Optional message text field
 * - Multi-select: share to multiple conversations
 * - Send progress indicators
 * - Success confirmation
 */
class ShareToMessageDialog : public juce::Component,
                             public juce::TextEditor::Listener,
                             public juce::Timer,
                             public juce::ScrollBar::Listener {
public:
  ShareToMessageDialog();
  ~ShareToMessageDialog() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // TextEditor::Listener
  void textEditorTextChanged(juce::TextEditor &editor) override;
  void textEditorReturnKeyPressed(juce::TextEditor &editor) override;

  // Timer for debounced search
  void timerCallback() override;

  // Callbacks
  std::function<void()> onClosed;                      // Dialog closed/cancelled
  std::function<void()> onCancelled;                   // Alias for onClosed (for compatibility)
  std::function<void(int conversationCount)> onShared; // Successfully shared to N conversations
  std::function<void()> onShareComplete;               // Alias for onShared (for compatibility)

  // Set clients
  void setStreamChatClient(StreamChatClient *client);
  void setNetworkClient(NetworkClient *client);
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Set the post to share
  void setPost(const Sidechain::FeedPost &postToShare);
  void setStoryToShare(const Sidechain::Story &story);

  // Show modal dialog
  void showModal(juce::Component *parent);

  // Load data
  void loadRecentConversations();

private:
  // ==============================================================================
  enum class DialogState { Loading, Ready, Sending, Success, Error };

  enum class ShareType { None, Post, Story };

  DialogState dialogState = DialogState::Loading;
  juce::String errorMessage;
  ShareType shareType = ShareType::None;
  FeedPost post; // The post being shared (when shareType == Post)
  Story story;   // The story being shared (when shareType == Story)

  StreamChatClient *streamChatClient = nullptr;
  NetworkClient *networkClient = nullptr;
  juce::String currentUserId;

  // UI elements
  juce::TextEditor messageInput; // Optional message to send with post
  juce::TextEditor searchInput;  // Search for conversations/users
  juce::ScrollBar scrollBar;
  double scrollPosition = 0.0;

  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int POST_PREVIEW_HEIGHT = 120;
  static constexpr int MESSAGE_INPUT_HEIGHT = 80;
  static constexpr int SEARCH_INPUT_HEIGHT = 50;
  static constexpr int CONVERSATION_ITEM_HEIGHT = 70;
  static constexpr int BUTTON_HEIGHT = 50;
  static constexpr int BOTTOM_PADDING = 80;

  // Data
  struct ConversationItem {
    juce::String channelType;
    juce::String channelId;
    juce::String channelName;
    juce::String avatarUrl;
    bool isGroup = false;
    int memberCount = 0;
  };

  std::vector<ConversationItem> recentConversations;
  std::vector<ConversationItem> searchResults;
  std::set<juce::String> selectedChannelIds; // Multi-select support

  // Search state
  juce::String currentSearchQuery;
  bool isSearching = false;
  static constexpr int SEARCH_DEBOUNCE_MS = 300;

  // Send progress tracking
  struct SendProgress {
    juce::String channelId;
    bool sent = false;
    bool failed = false;
    juce::String error;
  };
  std::vector<SendProgress> sendProgressList;
  int successfulSends = 0;

  // Drawing helpers
  void drawHeader(juce::Graphics &g);
  void drawPostPreview(juce::Graphics &g);
  void drawMessageInput(juce::Graphics &g);
  void drawSearchInput(juce::Graphics &g);
  void drawSectionHeader(juce::Graphics &g, const juce::String &title, int y);
  void drawConversationItem(juce::Graphics &g, const ConversationItem &conversation, int y, bool isSelected);
  void drawActionButtons(juce::Graphics &g);
  void drawLoadingState(juce::Graphics &g);
  void drawSendingProgress(juce::Graphics &g);
  void drawSuccessState(juce::Graphics &g);
  void drawErrorState(juce::Graphics &g);

  // ScrollBar::Listener
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  // Helper methods
  int calculateContentHeight();
  void performSearch(const juce::String &query);
  void toggleConversationSelection(const juce::String &channelId);
  bool isConversationSelected(const juce::String &channelId) const;
  void sendToSelectedConversations();
  void close();
  void reset();

  // Hit test bounds
  juce::Rectangle<int> getConversationItemBounds(int index, int yOffset) const;
  juce::Rectangle<int> getSendButtonBounds() const;
  juce::Rectangle<int> getCancelButtonBounds() const;
  juce::Rectangle<int> getCloseButtonBounds() const;
  juce::Rectangle<int> getDoneButtonBounds() const;

  // API helpers
  void sendPostToChannel(const ConversationItem &conversation);
  void handleSendSuccess(const juce::String &channelId);
  void handleSendError(const juce::String &channelId, const juce::String &error);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShareToMessageDialog)
};
