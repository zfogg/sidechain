#pragma once

#include <JuceHeader.h>
#include "../../network/StreamChatClient.h"

class NetworkClient;

//==============================================================================
/**
 * MessagesListComponent displays a list of chat conversations/channels
 *
 * Features:
 * - Shows all user's channels sorted by last message time
 * - Displays avatar, name, last message preview, timestamp, unread badge
 * - Click to open conversation
 * - "New Message" button to start new conversation
 * - Auto-refreshes channel list
 */
class MessagesListComponent : public juce::Component,
                              public juce::Timer,
                              public juce::ScrollBar::Listener
{
public:
    MessagesListComponent();
    ~MessagesListComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Callbacks
    std::function<void(const juce::String& channelType, const juce::String& channelId)> onChannelSelected;
    std::function<void()> onNewMessage;
    std::function<void()> onGoToDiscovery;
    std::function<void()> onCreateGroup;

    // Set StreamChatClient
    void setStreamChatClient(StreamChatClient* client);
    void setNetworkClient(NetworkClient* client);  // For user search

    // Load channels
    void loadChannels();
    void refreshChannels();

    // Timer for auto-refresh
    void timerCallback() override;

private:
    //==============================================================================
    enum class ListState
    {
        Loading,
        Loaded,
        Empty,
        Error
    };

    ListState listState = ListState::Loading;
    juce::String errorMessage;
    std::vector<StreamChatClient::Channel> channels;

    StreamChatClient* streamChatClient = nullptr;
    NetworkClient* networkClient = nullptr;

    // UI elements
    juce::ScrollBar scrollBar;
    double scrollPosition = 0.0;
    static constexpr int ITEM_HEIGHT = 80;
    static constexpr int HEADER_HEIGHT = 60;
    static constexpr int BUTTON_HEIGHT = 40;

    // Drawing helpers
    void drawHeader(juce::Graphics& g);
    void drawChannelItem(juce::Graphics& g, const StreamChatClient::Channel& channel, int y, int width);
    void drawEmptyState(juce::Graphics& g);
    void drawErrorState(juce::Graphics& g);

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    // Helper methods
    juce::String formatTimestamp(const juce::String& timestamp);
    juce::String getChannelName(const StreamChatClient::Channel& channel);
    juce::String getLastMessagePreview(const StreamChatClient::Channel& channel);
    int getUnreadCount(const StreamChatClient::Channel& channel);
    int getMemberCount(const StreamChatClient::Channel& channel) const;
    bool isGroupChannel(const StreamChatClient::Channel& channel) const;

    // Click handling
    int getChannelIndexAtY(int y);
    juce::Rectangle<int> getNewMessageButtonBounds() const;
    juce::Rectangle<int> getCreateGroupButtonBounds() const;
    juce::Rectangle<int> getChannelItemBounds(int index) const;
};
