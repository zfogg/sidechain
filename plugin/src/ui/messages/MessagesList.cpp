#include "MessagesList.h"
#include "../../network/NetworkClient.h"
#include "../../util/Colors.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringFormatter.h"

// ==============================================================================
MessagesList::MessagesList(Sidechain::Stores::AppStore *store)
    : Sidechain::UI::AppStoreComponent<Sidechain::Stores::ChatState>(
          store, [store](auto cb) { return store ? store->subscribeToChat(cb) : std::function<void()>([]() {}); }),
      scrollBar(true) {
  Log::info("MessagesList: Initializing");
  addAndMakeVisible(scrollBar);
  scrollBar.setRangeLimits(0.0, 0.0);
  scrollBar.addListener(this);

  // Create error state component (initially hidden)
  errorStateComponent = std::make_unique<ErrorState>();
  errorStateComponent->setErrorType(ErrorState::ErrorType::Network);
  errorStateComponent->setPrimaryAction("Reconnect", [this]() {
    Log::info("MessagesList: Reconnect requested from error state");
    loadChannels();
  });
  addChildComponent(errorStateComponent.get());
  Log::debug("MessagesList: Error state component created");

  startTimer(10000); // Refresh every 10 seconds

  // TODO: - Show "typing" indicator (future) - Deferred to future
  // phase
  // TODO: .2 - Implement audio snippet playback in messages
  // TODO: .3 - Upload audio snippet when sending
}

MessagesList::~MessagesList() {
  Log::debug("MessagesList: Destroying");
  stopTimer();
}

// ==============================================================================
// AppStoreComponent implementation

void MessagesList::onAppStateChanged(const Sidechain::Stores::ChatState &state) {
  // Update loading state
  if (state.isLoadingChannels) {
    listState = ListState::Loading;
  } else if (!state.chatError.isEmpty()) {
    listState = ListState::Error;
    errorMessage = state.chatError;
    if (errorStateComponent) {
      errorStateComponent->configureFromError(state.chatError);
      errorStateComponent->setVisible(true);
    }
  } else {
    // Convert channelOrder and channels map to StreamChatClient::Channel vector
    channels.clear();
    for (const auto &channelId : state.channelOrder) {
      auto it = state.channels.find(channelId);
      if (it != state.channels.end()) {
        const auto &channelState = it->second;
        StreamChatClient::Channel channel;
        channel.id = channelState.id;
        channel.name = channelState.name;
        channel.type = "messaging"; // Always messaging type for now (from AppStore)
        channel.unreadCount = channelState.unreadCount;

        // Map additional fields from ChannelState to StreamChatClient::Channel
        if (!channelState.messages.empty()) {
          // Map the last message from the messages vector
          const auto &lastMsg = channelState.messages.back();
          if (lastMsg) {
            channel.lastMessage.clear();
            channel.lastMessage["id"] = lastMsg->id;
            channel.lastMessage["text"] = lastMsg->text;
            channel.lastMessage["user_id"] = lastMsg->userId;
            channel.lastMessageAt = lastMsg->createdAt;
          }
        }

        // Store typing indicators in extraData
        if (!channelState.usersTyping.empty()) {
          juce::var typingArray;
          for (size_t i = 0; i < channelState.usersTyping.size(); ++i) {
            typingArray[static_cast<int>(i)] = channelState.usersTyping[i];
          }
          channel.extraData["users_typing"] = typingArray;
        }

        // Store loading state in extraData
        channel.extraData["is_loading_messages"] = channelState.isLoadingMessages;

        channels.push_back(channel);
        Log::debug("MessagesList: Added channel to list - id: " + channel.id + ", name: " + channel.name +
                   ", unreadCount: " + juce::String(channel.unreadCount));
      }
    }

    listState = channels.empty() ? ListState::Empty : ListState::Loaded;
    if (errorStateComponent) {
      errorStateComponent->setVisible(false);
    }
  }

  repaint();
}

void MessagesList::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));

  // Always draw header with "New Message" button
  drawHeader(g);

  // Draw content below header based on state
  auto contentArea = getLocalBounds().withTrimmedTop(HEADER_HEIGHT);

  switch (listState) {
  case ListState::Loading:
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Loading conversations...", contentArea, juce::Justification::centred);
    break;

  case ListState::Empty:
    drawEmptyState(g);
    break;

  case ListState::Error:
    // ErrorState component handles the error UI as a child component
    break;

  case ListState::Loaded: {
    Log::debug("MessagesList::paint - Drawing " + juce::String(channels.size()) + " conversations");
    int y = HEADER_HEIGHT;
    double scrollPos = getScrollPosition();
    for (size_t i = 0; i < channels.size(); i++) {
      if (y - scrollPos + ITEM_HEIGHT < 0) {
        y += ITEM_HEIGHT;
        continue; // Item is above visible area
      }

      if (y - scrollPos > getHeight())
        break; // Past visible area

      Log::debug("MessagesList::paint - Drawing conversation at index " + juce::String(i) +
                 " at y=" + juce::String(y - scrollPos));
      drawChannelItem(g, channels[i], static_cast<int>(y - scrollPos), getWidth() - scrollBar.getWidth());
      y += ITEM_HEIGHT;
    }
    break;
  }
  }
}

void MessagesList::resized() {
  // Register scrollbar with SmoothScrollable
  setScrollBar(&scrollBar);

  scrollBar.setBounds(getWidth() - 12, 0, 12, getHeight());

  // Update scrollbar range
  int totalHeight = HEADER_HEIGHT + static_cast<int>(channels.size() * ITEM_HEIGHT);
  scrollBar.setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - getHeight())));
  scrollBar.setCurrentRangeStart(getScrollPosition(), juce::dontSendNotification);

  // Position error state component in content area below header
  if (errorStateComponent != nullptr) {
    auto contentArea = getLocalBounds().withTrimmedTop(HEADER_HEIGHT);
    errorStateComponent->setBounds(contentArea);
  }
}

void MessagesList::mouseUp(const juce::MouseEvent &event) {
  auto newMessageBounds = getNewMessageButtonBounds();
  if (newMessageBounds.contains(event.getPosition())) {
    if (onNewMessage)
      onNewMessage();
    return;
  }

  auto createGroupBounds = getCreateGroupButtonBounds();
  if (createGroupBounds.getWidth() > 0 && createGroupBounds.contains(event.getPosition())) {
    if (onCreateGroup)
      onCreateGroup();
    return;
  }

  // Empty state CTA button
  if (listState == ListState::Empty && emptyStateButtonBounds.contains(event.getPosition())) {
    Log::info("MessagesList: Empty state CTA clicked");
    if (onNewMessage)
      onNewMessage();
    return;
  }

  double scrollPos = getScrollPosition();
  int index = getChannelIndexAtY(static_cast<int>(event.getPosition().y + scrollPos));
  Log::debug("MessagesList::mouseUp - Click at y=" + juce::String(static_cast<int>(event.getPosition().y)) +
             ", scrollPosition=" + juce::String(scrollPos) + ", index=" + juce::String(index) +
             ", total channels=" + juce::String(channels.size()));

  if (index >= 0 && index < static_cast<int>(channels.size())) {
    const auto &channel = channels[static_cast<size_t>(index)];
    Log::info("MessagesList::mouseUp - Channel clicked: type=" + channel.type + ", id=" + channel.id +
              ", name=" + channel.name);
    if (onChannelSelected) {
      Log::info("MessagesList::mouseUp - Calling onChannelSelected callback");
      onChannelSelected(channel.type, channel.id);
    } else {
      Log::error("MessagesList::mouseUp - ERROR: onChannelSelected callback is null!");
    }
  }
}

void MessagesList::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) {
  // Only scroll if wheel is within message list area (not over scroll bar)
  if (event.x < getWidth() - scrollBar.getWidth()) {
    handleMouseWheelMove(event, wheel, getHeight() - HEADER_HEIGHT, scrollBar.getWidth());
  }
}

// ==============================================================================
void MessagesList::setStreamChatClient(StreamChatClient *client) {
  streamChatClient = client;
  loadChannels();
}

void MessagesList::setNetworkClient(NetworkClient *client) {
  networkClient = client;
  Log::debug("MessagesList: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void MessagesList::loadChannels() {
  if (streamChatClient == nullptr || !streamChatClient->isAuthenticated()) {
    Log::warn("MessagesList: Cannot load channels - not authenticated");
    listState = ListState::Error;
    errorMessage = "Not authenticated";

    // Show auth error state
    if (errorStateComponent != nullptr) {
      errorStateComponent->setErrorType(ErrorState::ErrorType::Auth);
      errorStateComponent->setMessage("Please sign in to view your messages.");
      errorStateComponent->setVisible(true);
    }
    repaint();
    return;
  }

  Log::info("MessagesList: Loading conversations");
  listState = ListState::Loading;
  repaint();

  streamChatClient->queryChannels([this](Outcome<std::vector<StreamChatClient::Channel>> channelsResult) {
    if (channelsResult.isOk()) {
      channels = channelsResult.getValue();
      Log::info("MessagesList: Loaded " + juce::String(channels.size()) + " conversations");
      listState = channels.empty() ? ListState::Empty : ListState::Loaded;

      // Hide error state on success
      if (errorStateComponent != nullptr)
        errorStateComponent->setVisible(false);

      // Load presence info for channel members
      if (!channels.empty())
        loadPresenceForChannels();
    } else {
      Log::error("MessagesList: Failed to load channels - " + channelsResult.getError());
      listState = ListState::Error;
      errorMessage = "Failed to load channels: " + channelsResult.getError();

      // Configure and show error state component
      if (errorStateComponent != nullptr) {
        errorStateComponent->configureFromError(channelsResult.getError());
        errorStateComponent->setVisible(true);
      }
    }
    repaint();
  });
}

void MessagesList::loadPresenceForChannels() {
  if (streamChatClient == nullptr)
    return;

  // Collect unique user IDs from all channels (excluding current user)
  std::vector<juce::String> userIds;
  for (const auto &channel : channels) {
    if (!isGroupChannel(channel)) {
      auto otherId = getOtherUserId(channel);
      if (otherId.isNotEmpty()) {
        // Avoid duplicates
        if (std::find(userIds.begin(), userIds.end(), otherId) == userIds.end())
          userIds.push_back(otherId);
      }
    }
  }

  if (userIds.empty())
    return;

  Log::debug("MessagesList: Querying presence for " + juce::String(userIds.size()) + " users");

  streamChatClient->queryPresence(userIds, [this](Outcome<std::vector<StreamChatClient::UserPresence>> result) {
    if (result.isOk()) {
      const auto &presences = result.getValue();
      for (const auto &presence : presences) {
        userPresence[presence.userId] = presence;
      }
      Log::debug("MessagesList: Updated presence for " + juce::String(presences.size()) + " users");
      repaint();
    } else {
      Log::warn("MessagesList: Failed to load presence - " + result.getError());
    }
  });
}

juce::String MessagesList::getOtherUserId(const StreamChatClient::Channel &channel) const {
  if (!channel.members.isArray())
    return {};

  auto *membersArray = channel.members.getArray();
  if (membersArray == nullptr)
    return {};

  for (int i = 0; i < membersArray->size(); ++i) {
    auto member = (*membersArray)[i];
    juce::String memberId;

    if (member.isObject()) {
      memberId = member.getProperty("user_id", "").toString();
      if (memberId.isEmpty())
        memberId = member.getProperty("id", "").toString();

      // Also try nested user object
      auto user = member.getProperty("user", juce::var());
      if (user.isObject() && memberId.isEmpty())
        memberId = user.getProperty("id", "").toString();
    } else if (member.isString()) {
      memberId = member.toString();
    }

    if (memberId.isNotEmpty() && memberId != currentUserId)
      return memberId;
  }

  return {};
}

juce::String MessagesList::formatLastActive(const juce::String &lastActiveTimestamp) const {
  if (lastActiveTimestamp.isEmpty())
    return "";

  // Parse ISO 8601 timestamp and compute time ago
  auto now = juce::Time::getCurrentTime();

  // Try to parse the timestamp
  juce::Time lastActive;
  if (!juce::Time::fromISO8601(lastActiveTimestamp).toMilliseconds()) {
    // Fallback: try parsing without timezone
    lastActive = juce::Time::fromISO8601(lastActiveTimestamp);
  } else {
    lastActive = juce::Time::fromISO8601(lastActiveTimestamp);
  }

  auto diffSeconds = (now - lastActive).inSeconds();

  if (diffSeconds < 60)
    return "Active now";
  else if (diffSeconds < 3600)
    return "Active " + juce::String(diffSeconds / 60) + "m ago";
  else if (diffSeconds < 86400)
    return "Active " + juce::String(diffSeconds / 3600) + "h ago";
  else
    return "Active " + juce::String(diffSeconds / 86400) + "d ago";
}

void MessagesList::refreshChannels() {
  loadChannels();
}

void MessagesList::timerCallback() {
  if (listState == ListState::Loaded) {
    refreshChannels();
  }
}

// ==============================================================================
void MessagesList::drawHeader(juce::Graphics &g) {
  // Header background - slightly lighter than content area
  g.setColour(juce::Colour(0xff2a2a2a));
  g.fillRect(0, 0, getWidth(), HEADER_HEIGHT);

  // Draw "Messages" title on the left
  g.setColour(juce::Colours::white);
  g.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
  g.drawText("Messages", 15, 0, 200, HEADER_HEIGHT, juce::Justification::centredLeft);

  // Draw circular "New Message" button (plus icon in circle) on the right
  auto newMessageBounds = getNewMessageButtonBounds();

  // Blue filled circle
  g.setColour(juce::Colour(0xff4a9eff));
  g.fillEllipse(newMessageBounds.toFloat());

  // White plus sign
  g.setColour(juce::Colours::white);
  auto center = newMessageBounds.getCentre();
  int plusSize = 14;
  g.drawLine(static_cast<float>(center.x - plusSize / 2), static_cast<float>(center.y),
             static_cast<float>(center.x + plusSize / 2), static_cast<float>(center.y), 2.5f);
  g.drawLine(static_cast<float>(center.x), static_cast<float>(center.y - plusSize / 2), static_cast<float>(center.x),
             static_cast<float>(center.y + plusSize / 2), 2.5f);

  // Draw bottom border
  g.setColour(juce::Colour(0xff3a3a3a));
  g.drawHorizontalLine(HEADER_HEIGHT - 1, 0.0f, static_cast<float>(getWidth()));
}

void MessagesList::drawChannelItem(juce::Graphics &g, const StreamChatClient::Channel &channel, int y, int width) {
  // Background
  g.setColour(SidechainColors::backgroundLight());
  g.fillRect(0, y, width, ITEM_HEIGHT);

  // Avatar (circular)
  int avatarSize = 50;
  int avatarX = 10;
  int avatarY = y + (ITEM_HEIGHT - avatarSize) / 2;

  bool isGroup = isGroupChannel(channel);

  // Get presence info for DM channels
  bool isOnline = false;
  juce::String lastActiveText;
  if (!isGroup) {
    auto otherId = getOtherUserId(channel);
    auto it = userPresence.find(otherId);
    if (it != userPresence.end()) {
      isOnline = it->second.online;
      if (!isOnline)
        lastActiveText = formatLastActive(it->second.lastActive);
    }
  }

  if (isGroup) {
    // Group avatar - show first letter of name or "G" for group
    juce::String channelName = getChannelName(channel);
    juce::String initial = channelName.isNotEmpty() ? channelName.substring(0, 1).toUpperCase() : "G";
    g.setColour(SidechainColors::softBlue());
    g.fillEllipse(static_cast<float>(avatarX), static_cast<float>(avatarY), static_cast<float>(avatarSize),
                  static_cast<float>(avatarSize));
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText(initial, avatarX, avatarY, avatarSize, avatarSize, juce::Justification::centred);
  } else {
    // Direct message avatar placeholder
    g.setColour(juce::Colour(0xff4a4a4a));
    g.fillEllipse(static_cast<float>(avatarX), static_cast<float>(avatarY), static_cast<float>(avatarSize),
                  static_cast<float>(avatarSize));

    // Online indicator (green dot) - bottom right of avatar
    if (isOnline) {
      int dotSize = 14;
      int dotX = avatarX + avatarSize - dotSize + 2;
      int dotY = avatarY + avatarSize - dotSize + 2;

      // White border
      g.setColour(SidechainColors::backgroundLight());
      g.fillEllipse(static_cast<float>(dotX - 2), static_cast<float>(dotY - 2), static_cast<float>(dotSize + 4),
                    static_cast<float>(dotSize + 4));

      // Green dot
      g.setColour(SidechainColors::onlineIndicator());
      g.fillEllipse(static_cast<float>(dotX), static_cast<float>(dotY), static_cast<float>(dotSize),
                    static_cast<float>(dotSize));
    }
  }

  // Unread badge (top right of avatar, overlapping online indicator)
  int unread = getUnreadCount(channel);
  if (unread > 0) {
    g.setColour(SidechainColors::coralPink());
    int badgeSize = 20;
    int badgeX = avatarX + avatarSize - badgeSize;
    int badgeY = avatarY;
    g.fillEllipse(static_cast<float>(badgeX), static_cast<float>(badgeY), static_cast<float>(badgeSize),
                  static_cast<float>(badgeSize));
    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    juce::String badgeText = unread > 99 ? "99+" : juce::String(unread);
    g.drawText(badgeText, badgeX, badgeY, badgeSize, badgeSize, juce::Justification::centred);
  }

  // Channel name
  int textX = avatarX + avatarSize + 12;
  int textWidth = width - textX - 90;
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
  juce::String channelName = getChannelName(channel);
  g.drawText(channelName, textX, y + 12, textWidth, 20, juce::Justification::topLeft);

  // Online status or member count (second line)
  g.setFont(juce::FontOptions(12.0f));
  if (isGroup) {
    int memberCount = getMemberCount(channel);
    juce::String memberText = juce::String(memberCount) + (memberCount == 1 ? " member" : " members");
    g.setColour(SidechainColors::textMuted());
    g.drawText(memberText, textX, y + 32, textWidth, 16, juce::Justification::topLeft);
  } else if (isOnline) {
    g.setColour(SidechainColors::onlineIndicator());
    g.drawText("Active now", textX, y + 32, textWidth, 16, juce::Justification::topLeft);
  } else if (lastActiveText.isNotEmpty()) {
    g.setColour(SidechainColors::textMuted());
    g.drawText(lastActiveText, textX, y + 32, textWidth, 16, juce::Justification::topLeft);
  }

  // Last message preview
  int previewY = y + 50;
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::FontOptions(13.0f));
  juce::String preview = getLastMessagePreview(channel);
  g.drawText(preview, textX, previewY, textWidth, 20, juce::Justification::topLeft, true);

  // Timestamp (top right)
  juce::String timestamp = formatTimestamp(channel.lastMessageAt);
  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::FontOptions(11.0f));
  g.drawText(timestamp, width - 85, y + 14, 75, 16, juce::Justification::topRight);

  // Bottom border
  g.setColour(SidechainColors::borderSubtle());
  g.drawHorizontalLine(y + ITEM_HEIGHT - 1, static_cast<float>(textX), static_cast<float>(width));
}

void MessagesList::drawEmptyState(juce::Graphics &g) {
  auto contentArea = getLocalBounds().withTrimmedTop(HEADER_HEIGHT);
  auto centerX = contentArea.getCentreX();
  auto startY = contentArea.getY() + 40;

  // ============================================================
  // Music collaboration illustration - two headphones with a connection
  // ============================================================
  auto illustrationSize = 120;
  auto illustrationCenterY = startY + illustrationSize / 2;

  // Draw a subtle background circle
  g.setColour(SidechainColors::backgroundLight());
  g.fillEllipse(static_cast<float>(centerX - illustrationSize / 2 - 10),
                static_cast<float>(illustrationCenterY - illustrationSize / 2 - 10),
                static_cast<float>(illustrationSize + 20), static_cast<float>(illustrationSize + 20));

  // Left headphone
  auto leftHeadphoneX = centerX - 45;
  auto headphoneY = illustrationCenterY - 15;

  // Headphone arc (headband)
  juce::Path leftArc;
  leftArc.addCentredArc(static_cast<float>(leftHeadphoneX), static_cast<float>(headphoneY), 25.0f, 25.0f, 0.0f,
                        juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 1.8f, true);
  g.setColour(SidechainColors::softBlue());
  g.strokePath(leftArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

  // Left ear cup
  g.setColour(SidechainColors::softBlue());
  g.fillRoundedRectangle(static_cast<float>(leftHeadphoneX - 35), static_cast<float>(headphoneY - 5), 16.0f, 30.0f,
                         5.0f);
  g.fillRoundedRectangle(static_cast<float>(leftHeadphoneX + 19), static_cast<float>(headphoneY - 5), 16.0f, 30.0f,
                         5.0f);

  // Right headphone
  auto rightHeadphoneX = centerX + 45;

  // Headphone arc (headband)
  juce::Path rightArc;
  rightArc.addCentredArc(static_cast<float>(rightHeadphoneX), static_cast<float>(headphoneY), 25.0f, 25.0f, 0.0f,
                         juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 1.8f, true);
  g.setColour(SidechainColors::coralPink());
  g.strokePath(rightArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

  // Right ear cups
  g.setColour(SidechainColors::coralPink());
  g.fillRoundedRectangle(static_cast<float>(rightHeadphoneX - 35), static_cast<float>(headphoneY - 5), 16.0f, 30.0f,
                         5.0f);
  g.fillRoundedRectangle(static_cast<float>(rightHeadphoneX + 19), static_cast<float>(headphoneY - 5), 16.0f, 30.0f,
                         5.0f);

  // Connection line between headphones (music waves)
  g.setColour(SidechainColors::lavender().withAlpha(0.7f));
  auto waveY = static_cast<float>(headphoneY + 10);
  for (int i = 0; i < 3; i++) {
    float offsetY = static_cast<float>(i - 1) * 8.0f;
    juce::Path wave;
    wave.startNewSubPath(static_cast<float>(leftHeadphoneX + 25), waveY + offsetY);
    wave.cubicTo(static_cast<float>(centerX - 15), waveY + offsetY - 8.0f, static_cast<float>(centerX + 15),
                 waveY + offsetY + 8.0f, static_cast<float>(rightHeadphoneX - 25), waveY + offsetY);
    g.strokePath(wave, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
  }

  // Small music notes floating around
  g.setColour(SidechainColors::creamYellow().withAlpha(0.6f));
  g.setFont(juce::FontOptions(16.0f));
  g.drawText(juce::String::charToString(0x266B), centerX - 60, illustrationCenterY - 40, 20, 20,
             juce::Justification::centred); // ♫
  g.drawText(juce::String::charToString(0x266A), centerX + 50, illustrationCenterY - 30, 20, 20,
             juce::Justification::centred); // ♪

  // ============================================================
  // Text content
  // ============================================================
  auto textStartY = startY + illustrationSize + 30;

  // Main heading
  g.setColour(SidechainColors::textPrimary());
  g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
  g.drawText("Connect with Producers", contentArea.withY(textStartY).withHeight(30), juce::Justification::centred);

  // Subheading
  g.setColour(SidechainColors::textSecondary());
  g.setFont(juce::FontOptions(15.0f));
  g.drawText("Share loops, give feedback, and collaborate", contentArea.withY(textStartY + 35).withHeight(24),
             juce::Justification::centred);
  g.drawText("with other music makers.", contentArea.withY(textStartY + 55).withHeight(24),
             juce::Justification::centred);

  // ============================================================
  // CTA Button - "Start a Conversation"
  // ============================================================
  auto buttonWidth = 200;
  auto buttonHeight = 44;
  auto buttonX = centerX - buttonWidth / 2;
  auto buttonY = textStartY + 100;
  emptyStateButtonBounds = juce::Rectangle<int>(buttonX, buttonY, buttonWidth, buttonHeight);

  // Button background with gradient effect
  g.setColour(SidechainColors::coralPink());
  g.fillRoundedRectangle(emptyStateButtonBounds.toFloat(), 22.0f);

  // Button text
  g.setColour(juce::Colours::white);
  g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
  g.drawText("Start a Conversation", emptyStateButtonBounds, juce::Justification::centred);

  // ============================================================
  // Tip at the bottom
  // ============================================================
  auto tipY = buttonY + buttonHeight + 30;
  g.setColour(SidechainColors::textMuted());
  g.setFont(juce::FontOptions(12.0f));
  g.drawText(juce::String::charToString(0x1F4A1) + " Tip: You can also message people from their profile",
             contentArea.withY(tipY).withHeight(20), juce::Justification::centred);
}

void MessagesList::drawErrorState(juce::Graphics &g) {
  g.setColour(juce::Colour(0xffff4444));
  g.setFont(16.0f);
  g.drawText("Error: " + errorMessage, getLocalBounds(), juce::Justification::centred);
}

// ==============================================================================
juce::String MessagesList::formatTimestamp(const juce::String &timestamp) {
  if (timestamp.isEmpty())
    return "";

  return StringFormatter::formatTimeAgo(timestamp);
}

juce::String MessagesList::getChannelName(const StreamChatClient::Channel &channel) {
  if (!channel.name.isEmpty())
    return channel.name;

  // For direct messages, extract other user's name from members
  if (channel.type == "messaging" && channel.members.isArray()) {
    auto *membersArray = channel.members.getArray();
    if (membersArray != nullptr && membersArray->size() >= 2) {
      // Find the other member (not the current user)
      // Members array contains objects with user_id or just user IDs
      for (int i = 0; i < membersArray->size(); ++i) {
        auto member = (*membersArray)[i];
        juce::String memberId;

        // Check if member is an object with user_id property
        if (member.isObject()) {
          memberId = member.getProperty("user_id", "").toString();
          if (memberId.isEmpty())
            memberId = member.getProperty("id", "").toString();

          // If we have a user object with name, use it
          auto user = member.getProperty("user", juce::var());
          if (user.isObject()) {
            juce::String userName = user.getProperty("name", "").toString();
            if (userName.isEmpty())
              userName = user.getProperty("username", "").toString();
            if (userName.isNotEmpty())
              return userName;
          }
        } else if (member.isString()) {
          memberId = member.toString();
        }

        // If we found a member ID that's not empty, use it as fallback
        // (We'll enhance this later to fetch actual username)
        if (memberId.isNotEmpty() && currentUserId.isNotEmpty() && memberId != currentUserId) {
          // For now, return a formatted version - can be enhanced to fetch
          // username
          return "@" + memberId.substring(0, 8); // Show first 8 chars of ID
        }
      }
    }
    return "Direct Message";
  }

  return "Channel " + channel.id;
}

juce::String MessagesList::getLastMessagePreview(const StreamChatClient::Channel &channel) {
  if (channel.lastMessage.isObject()) {
    auto text = channel.lastMessage.getProperty("text", "").toString();
    if (text.length() > 50)
      return text.substring(0, 47) + "...";
    return text;
  }
  return "No messages";
}

int MessagesList::getUnreadCount(const StreamChatClient::Channel &channel) {
  return channel.unreadCount;
}

int MessagesList::getChannelIndexAtY(int y) {
  if (y < HEADER_HEIGHT)
    return -1;

  int itemY = y - HEADER_HEIGHT;
  int index = itemY / ITEM_HEIGHT;

  if (index >= 0 && index < static_cast<int>(channels.size()))
    return index;

  return -1;
}

juce::Rectangle<int> MessagesList::getNewMessageButtonBounds() const {
  // Circular button on the right side of the header
  int buttonSize = 40;
  int rightMargin = 15;
  int y = (HEADER_HEIGHT - buttonSize) / 2;
  return juce::Rectangle<int>(getWidth() - buttonSize - rightMargin - scrollBar.getWidth(), y, buttonSize, buttonSize);
}

juce::Rectangle<int> MessagesList::getCreateGroupButtonBounds() const {
  // Create Group button removed - now using circular plus button for new
  // message only
  return juce::Rectangle<int>();
}

juce::Rectangle<int> MessagesList::getChannelItemBounds(int index) const {
  return juce::Rectangle<int>(0, HEADER_HEIGHT + index * ITEM_HEIGHT, getWidth() - scrollBar.getWidth(), ITEM_HEIGHT);
}

bool MessagesList::isGroupChannel(const StreamChatClient::Channel &channel) const {
  return channel.type == "team" || (!channel.name.isEmpty() && channel.members.isArray());
}

int MessagesList::getMemberCount(const StreamChatClient::Channel &channel) const {
  if (channel.members.isArray()) {
    return static_cast<int>(channel.members.size());
  }
  return 0;
}
