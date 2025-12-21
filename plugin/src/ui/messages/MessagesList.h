#pragma once

#include "../../network/StreamChatClient.h"
#include "../../stores/AppStore.h"
#include "../../ui/common/AppStoreComponent.h"
#include "../../ui/common/SmoothScrollable.h"
#include "../../util/reactive/ReactiveBoundComponent.h"
#include "../common/ErrorState.h"
#include <JuceHeader.h>
#include <map>

class NetworkClient;

// ==============================================================================
/**
 * MessagesList displays a list of chat conversations/channels
 *
 * Features:
 * - Shows all user's channels sorted by last message time
 * - Displays avatar, name, last message preview, timestamp, unread badge
 * - Click to open conversation
 * - "New Message" button to start new conversation
 * - Auto-refreshes channel list
 * - Reactive updates from AppStore ChatState
 * - Smooth scroll animations
 */
class MessagesList : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChatState>,
                     public Sidechain::UI::SmoothScrollable,
                     public juce::Timer {
public:
  MessagesList(Sidechain::Stores::AppStore *store = nullptr);
  ~MessagesList() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

  // Callbacks
  std::function<void(const juce::String &channelType, const juce::String &channelId)> onChannelSelected;
  std::function<void()> onNewMessage;
  std::function<void()> onGoToDiscovery;
  std::function<void()> onCreateGroup;

  // Set StreamChatClient
  void setStreamChatClient(StreamChatClient *client);
  void setNetworkClient(NetworkClient *client); // For user search
  void setCurrentUserId(const juce::String &userId) {
    currentUserId = userId;
  }

  // Load channels
  void loadChannels();
  void refreshChannels();

  // Timer for auto-refresh
  void timerCallback() override;

protected:
  // ==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const Sidechain::Stores::ChatState &state) override;

private:
  // ==============================================================================
  enum class ListState { Loading, Loaded, Empty, Error };

  ListState listState = ListState::Loading;
  juce::String errorMessage;
  std::vector<StreamChatClient::Channel> channels;

  StreamChatClient *streamChatClient = nullptr;
  NetworkClient *networkClient = nullptr;
  juce::String currentUserId; // Current user ID for filtering members

  // Presence tracking for online indicators
  std::map<juce::String, StreamChatClient::UserPresence> userPresence;

  // UI elements
  juce::ScrollBar scrollBar;
  static constexpr int ITEM_HEIGHT = 80;
  static constexpr int HEADER_HEIGHT = 60;
  static constexpr int BUTTON_HEIGHT = 40;

  // Error state component
  std::unique_ptr<ErrorState> errorStateComponent;

  // Drawing helpers
  void drawHeader(juce::Graphics &g);
  void drawChannelItem(juce::Graphics &g, const StreamChatClient::Channel &channel, int y, int width);
  void drawEmptyState(juce::Graphics &g);
  void drawErrorState(juce::Graphics &g);

protected:
  // SmoothScrollable implementation
  void onScrollUpdate(double newScrollPosition [[maybe_unused]]) override {
    repaint();
  }

  juce::String getComponentName() const override {
    return "MessagesList";
  }

  // Helper methods
  juce::String formatTimestamp(const juce::String &timestamp);
  juce::String getChannelName(const StreamChatClient::Channel &channel);
  juce::String getLastMessagePreview(const StreamChatClient::Channel &channel);
  int getUnreadCount(const StreamChatClient::Channel &channel);
  int getMemberCount(const StreamChatClient::Channel &channel) const;
  bool isGroupChannel(const StreamChatClient::Channel &channel) const;
  juce::String getOtherUserId(const StreamChatClient::Channel &channel) const;
  void loadPresenceForChannels();
  juce::String formatLastActive(const juce::String &lastActiveTimestamp) const;

  // Click handling
  int getChannelIndexAtY(int y);
  juce::Rectangle<int> getNewMessageButtonBounds() const;
  juce::Rectangle<int> getCreateGroupButtonBounds() const;
  juce::Rectangle<int> getChannelItemBounds(int index) const;

  // Empty state button bounds (set during paint)
  mutable juce::Rectangle<int> emptyStateButtonBounds;
};
