#pragma once

#include <JuceHeader.h>
#include "../../network/NetworkClient.h"
#include "../../network/StreamChatClient.h"
#include "../../models/FeedPost.h"
#include "../stories/StoriesFeed.h"

//==============================================================================
/**
 * ShareToMessageDialog - Modal for sharing posts/stories to DM conversations
 *
 * Features:
 * - Shows recent conversations at top
 * - Search for users to start new conversation
 * - Multi-select support for sending to multiple recipients
 * - Preview of content being shared
 * - Send button to share to selected conversations
 */
class ShareToMessageDialog : public juce::Component,
                              public juce::TextEditor::Listener,
                              public juce::Button::Listener,
                              public juce::ScrollBar::Listener,
                              public juce::Timer
{
public:
    ShareToMessageDialog();
    ~ShareToMessageDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void buttonClicked(juce::Button* button) override;
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
    void timerCallback() override;

    // Setup
    void setNetworkClient(NetworkClient* client) { networkClient = client; }
    void setStreamChatClient(StreamChatClient* client) { streamChatClient = client; }
    void setCurrentUserId(const juce::String& userId) { currentUserId = userId; }

    // Set content to share
    void setPostToShare(const FeedPost& post);
    void setStoryToShare(const StoryData& story);

    // Callbacks
    std::function<void()> onShareComplete;
    std::function<void()> onCancelled;

    // Show as modal dialog
    void showModal(juce::Component* parentComponent);
    void closeDialog();

private:
    NetworkClient* networkClient = nullptr;
    StreamChatClient* streamChatClient = nullptr;
    juce::String currentUserId;

    // Content to share
    enum class ShareType { None, Post, Story };
    ShareType shareType = ShareType::None;
    FeedPost postToShare;
    StoryData storyToShare;

    // UI Components
    std::unique_ptr<juce::TextEditor> searchInput;
    std::unique_ptr<juce::TextButton> sendButton;
    std::unique_ptr<juce::TextButton> cancelButton;
    std::unique_ptr<juce::ScrollBar> scrollBar;

    // Data - recent conversations
    std::vector<StreamChatClient::Channel> recentChannels;
    bool isLoadingChannels = false;

    // Data - search results (users)
    struct SearchUser
    {
        juce::String id;
        juce::String username;
        juce::String displayName;
        juce::String avatarUrl;
    };
    std::vector<SearchUser> searchResults;
    bool isSearching = false;
    juce::String currentQuery;

    // Selection state
    // Stores channel IDs for existing channels, or "user:userId" for new DMs
    juce::StringArray selectedRecipients;

    // Sending state
    bool isSending = false;
    int sendProgress = 0;
    int sendTotal = 0;

    // Layout constants
    static constexpr int DIALOG_WIDTH = 450;
    static constexpr int DIALOG_HEIGHT = 550;
    static constexpr int HEADER_HEIGHT = 70;
    static constexpr int PREVIEW_HEIGHT = 80;
    static constexpr int INPUT_HEIGHT = 50;
    static constexpr int BUTTON_HEIGHT = 44;
    static constexpr int ITEM_HEIGHT = 60;
    static constexpr int PADDING = 16;

    double scrollOffset = 0.0;

    // Drawing
    void drawHeader(juce::Graphics& g);
    void drawContentPreview(juce::Graphics& g);
    void drawSearchInput(juce::Graphics& g);
    void drawRecipientsList(juce::Graphics& g);
    void drawChannelItem(juce::Graphics& g, const StreamChatClient::Channel& channel,
                         juce::Rectangle<int> bounds, bool isSelected);
    void drawUserItem(juce::Graphics& g, const SearchUser& user,
                      juce::Rectangle<int> bounds, bool isSelected);
    void drawLoadingState(juce::Graphics& g);
    void drawSendingState(juce::Graphics& g);

    // Helpers
    void loadRecentChannels();
    void performSearch();
    void toggleSelection(const juce::String& recipientId);
    bool isSelected(const juce::String& recipientId) const;
    void sendToSelectedRecipients();
    void sendToChannel(const juce::String& channelType, const juce::String& channelId);
    void createAndSendToUser(const juce::String& userId);

    // Hit testing
    juce::Rectangle<int> getContentBounds() const;
    juce::Rectangle<int> getItemBounds(int index) const;
    int getItemIndexAt(juce::Point<int> pos) const;

    // Get channel display name
    juce::String getChannelDisplayName(const StreamChatClient::Channel& channel) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShareToMessageDialog)
};
