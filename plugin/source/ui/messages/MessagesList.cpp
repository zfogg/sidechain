#include "MessagesList.h"
#include "../../network/NetworkClient.h"
#include "../../util/Log.h"
#include "../../util/Result.h"
#include "../../util/StringFormatter.h"

//==============================================================================
MessagesList::MessagesList()
    : scrollBar(true)
{
    Log::info("MessagesList: Initializing");
    addAndMakeVisible(scrollBar);
    scrollBar.setRangeLimits(0.0, 0.0);
    scrollBar.addListener(this);

    startTimer(10000); // Refresh every 10 seconds
}

MessagesList::~MessagesList()
{
    Log::debug("MessagesList: Destroying");
    stopTimer();
}

void MessagesList::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

    switch (listState)
    {
        case ListState::Loading:
            g.setColour(juce::Colours::white);
            g.setFont(16.0f);
            g.drawText("Loading conversations...", getLocalBounds(), juce::Justification::centred);
            break;

        case ListState::Empty:
            drawEmptyState(g);
            break;

        case ListState::Error:
            drawErrorState(g);
            break;

        case ListState::Loaded:
            drawHeader(g);

            int y = HEADER_HEIGHT;
            for (size_t i = 0; i < channels.size(); i++)
            {
                if (y - scrollPosition + ITEM_HEIGHT < 0)
                {
                    y += ITEM_HEIGHT;
                    continue; // Item is above visible area
                }

                if (y - scrollPosition > getHeight())
                    break; // Past visible area

                drawChannelItem(g, channels[i], static_cast<int>(y - scrollPosition), getWidth() - scrollBar.getWidth());
                y += ITEM_HEIGHT;
            }
            break;
    }
}

void MessagesList::resized()
{
    scrollBar.setBounds(getWidth() - 12, 0, 12, getHeight());

    // Update scrollbar range
    int totalHeight = HEADER_HEIGHT + static_cast<int>(channels.size() * ITEM_HEIGHT);
    scrollBar.setRangeLimits(0.0, static_cast<double>(juce::jmax(0, totalHeight - getHeight())));
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
}

void MessagesList::mouseUp(const juce::MouseEvent& event)
{
    auto newMessageBounds = getNewMessageButtonBounds();
    if (newMessageBounds.contains(event.getPosition()))
    {
        if (onNewMessage)
            onNewMessage();
        return;
    }

    auto createGroupBounds = getCreateGroupButtonBounds();
    if (createGroupBounds.getWidth() > 0 && createGroupBounds.contains(event.getPosition()))
    {
        if (onCreateGroup)
            onCreateGroup();
        return;
    }

    int index = getChannelIndexAtY(static_cast<int>(event.getPosition().y + scrollPosition));
    if (index >= 0 && index < static_cast<int>(channels.size()))
    {
        const auto& channel = channels[index];
        if (onChannelSelected)
            onChannelSelected(channel.type, channel.id);
    }
}

void MessagesList::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    scrollPosition -= wheel.deltaY * 30.0;
    scrollPosition = juce::jlimit(0.0, scrollBar.getMaximumRangeLimit(), scrollPosition);
    scrollBar.setCurrentRangeStart(scrollPosition, juce::dontSendNotification);
    repaint();
}

//==============================================================================
void MessagesList::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;
    loadChannels();
}

void MessagesList::setNetworkClient(NetworkClient* client)
{
    networkClient = client;
    Log::debug("MessagesList: NetworkClient set " + juce::String(client != nullptr ? "(valid)" : "(null)"));
}

void MessagesList::loadChannels()
{
    if (streamChatClient == nullptr || !streamChatClient->isAuthenticated())
    {
        Log::warn("MessagesList: Cannot load channels - not authenticated");
        listState = ListState::Error;
        errorMessage = "Not authenticated";
        repaint();
        return;
    }

    Log::info("MessagesList: Loading conversations");
    listState = ListState::Loading;
    repaint();

    streamChatClient->queryChannels([this](Outcome<std::vector<StreamChatClient::Channel>> channelsResult) {
        if (channelsResult.isOk())
        {
            channels = channelsResult.getValue();
            Log::info("MessagesList: Loaded " + juce::String(channels.size()) + " conversations");
            listState = channels.empty() ? ListState::Empty : ListState::Loaded;
        }
        else
        {
            Log::error("MessagesList: Failed to load channels - " + channelsResult.getError());
            listState = ListState::Error;
            errorMessage = "Failed to load channels: " + channelsResult.getError();
        }
        repaint();
    });
}

void MessagesList::refreshChannels()
{
    loadChannels();
}

void MessagesList::timerCallback()
{
    if (listState == ListState::Loaded)
    {
        refreshChannels();
    }
}

//==============================================================================
void MessagesList::drawHeader(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, 0, getWidth(), HEADER_HEIGHT);

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("Messages", 10, 0, 200, HEADER_HEIGHT, juce::Justification::centredLeft);

    // Draw "New Message" button
    auto newMessageBounds = getNewMessageButtonBounds();
    g.setColour(juce::Colour(0xff4a9eff));
    g.fillRoundedRectangle(newMessageBounds.toFloat(), 6.0f);
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("New Message", newMessageBounds, juce::Justification::centred);

    // Draw "Create Group" button (if space allows)
    auto createGroupBounds = getCreateGroupButtonBounds();
    if (createGroupBounds.getWidth() > 0)
    {
        g.setColour(juce::Colour(0xff3a3a3a));
        g.fillRoundedRectangle(createGroupBounds.toFloat(), 6.0f);
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText("Create Group", createGroupBounds, juce::Justification::centred);
    }
}

void MessagesList::drawChannelItem(juce::Graphics& g, const StreamChatClient::Channel& channel, int y, int width)
{
    // Background
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(0, y, width, ITEM_HEIGHT);

    // Avatar (circular)
    int avatarSize = 50;
    int avatarX = 10;
    int avatarY = y + (ITEM_HEIGHT - avatarSize) / 2;

    bool isGroup = isGroupChannel(channel);
    if (isGroup)
    {
        // Group avatar - show first letter of name or "G" for group
        juce::String channelName = getChannelName(channel);
        juce::String initial = channelName.isNotEmpty() ? channelName.substring(0, 1).toUpperCase() : "G";
        g.setColour(juce::Colour(0xff4a9eff));  // Group color
        g.fillEllipse(avatarX, avatarY, avatarSize, avatarSize);
        g.setColour(juce::Colours::white);
        g.setFont(20.0f);
        g.drawText(initial, avatarX, avatarY, avatarSize, avatarSize, juce::Justification::centred);
    }
    else
    {
        // Direct message avatar placeholder
        g.setColour(juce::Colour(0xff4a4a4a));
        g.fillEllipse(avatarX, avatarY, avatarSize, avatarSize);
    }

    // Unread badge
    int unread = getUnreadCount(channel);
    if (unread > 0)
    {
        g.setColour(juce::Colour(0xffff4444));
        int badgeSize = 20;
        g.fillEllipse(avatarX + avatarSize - badgeSize, avatarY, badgeSize, badgeSize);
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        juce::String badgeText = unread > 99 ? "99+" : juce::String(unread);
        g.drawText(badgeText, avatarX + avatarSize - badgeSize, avatarY, badgeSize, badgeSize, juce::Justification::centred);
    }

    // Channel name
    int textX = avatarX + avatarSize + 10;
    int textWidth = width - textX - 100;
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    juce::String channelName = getChannelName(channel);
    g.drawText(channelName, textX, y + 10, textWidth, 20, juce::Justification::topLeft);

    // Member count for groups
    if (isGroup)
    {
        int memberCount = getMemberCount(channel);
        juce::String memberText = juce::String(memberCount) + (memberCount == 1 ? " member" : " members");
        g.setColour(juce::Colour(0xff888888));
        g.setFont(12.0f);
        g.drawText(memberText, textX, y + 32, textWidth, 16, juce::Justification::topLeft);
    }

    // Last message preview (below member count for groups, or at y+30 for DMs)
    int previewY = isGroup ? y + 48 : y + 30;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(14.0f);
    juce::String preview = getLastMessagePreview(channel);
    g.drawText(preview, textX, previewY, textWidth, 20, juce::Justification::topLeft, true);

    // Timestamp
    juce::String timestamp = formatTimestamp(channel.lastMessageAt);
    g.setColour(juce::Colour(0xff888888));
    g.setFont(12.0f);
    g.drawText(timestamp, width - 100, y + 10, 90, 20, juce::Justification::topRight);
}

void MessagesList::drawEmptyState(juce::Graphics& g)
{
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("No messages yet", getLocalBounds().withTrimmedTop(100), juce::Justification::centred);

    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(14.0f);
    g.drawText("Start a conversation to get started", getLocalBounds().withTrimmedTop(130), juce::Justification::centred);
}

void MessagesList::drawErrorState(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xffff4444));
    g.setFont(16.0f);
    g.drawText("Error: " + errorMessage, getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
juce::String MessagesList::formatTimestamp(const juce::String& timestamp)
{
    if (timestamp.isEmpty())
        return "";

    return StringFormatter::formatTimeAgo(timestamp);
}

juce::String MessagesList::getChannelName(const StreamChatClient::Channel& channel)
{
    if (!channel.name.isEmpty())
        return channel.name;

    // For direct messages, extract other user's name from members
    if (channel.type == "messaging" && channel.members.isArray())
    {
        // TODO: Extract other member's name from members array
        return "Direct Message";
    }

    return "Channel " + channel.id;
}

juce::String MessagesList::getLastMessagePreview(const StreamChatClient::Channel& channel)
{
    if (channel.lastMessage.isObject())
    {
        auto text = channel.lastMessage.getProperty("text", "").toString();
        if (text.length() > 50)
            return text.substring(0, 47) + "...";
        return text;
    }
    return "No messages";
}

int MessagesList::getUnreadCount(const StreamChatClient::Channel& channel)
{
    return channel.unreadCount;
}

int MessagesList::getChannelIndexAtY(int y)
{
    if (y < HEADER_HEIGHT)
        return -1;

    int itemY = y - HEADER_HEIGHT;
    int index = itemY / ITEM_HEIGHT;

    if (index >= 0 && index < static_cast<int>(channels.size()))
        return index;

    return -1;
}

juce::Rectangle<int> MessagesList::getNewMessageButtonBounds() const
{
    // Position to the right, leaving space for Create Group button if there's room
    int buttonWidth = 120;
    int rightMargin = 10;
    return juce::Rectangle<int>(getWidth() - buttonWidth - rightMargin, 10, buttonWidth, BUTTON_HEIGHT);
}

juce::Rectangle<int> MessagesList::getCreateGroupButtonBounds() const
{
    // Show Create Group button if there's enough space (next to New Message)
    int newMessageWidth = 120;
    int buttonWidth = 120;
    int spacing = 10;
    int rightMargin = 10;
    int totalWidth = newMessageWidth + spacing + buttonWidth + rightMargin;

    if (getWidth() < totalWidth)
        return juce::Rectangle<int>();  // Not enough space, hide button

    return juce::Rectangle<int>(getWidth() - newMessageWidth - spacing - buttonWidth - rightMargin,
                               10, buttonWidth, BUTTON_HEIGHT);
}

juce::Rectangle<int> MessagesList::getChannelItemBounds(int index) const
{
    return juce::Rectangle<int>(0, HEADER_HEIGHT + index * ITEM_HEIGHT, getWidth() - scrollBar.getWidth(), ITEM_HEIGHT);
}

void MessagesList::scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart)
{
    scrollPosition = newRangeStart;
    repaint();
}

bool MessagesList::isGroupChannel(const StreamChatClient::Channel& channel) const
{
    return channel.type == "team" || (!channel.name.isEmpty() && channel.members.isArray());
}

int MessagesList::getMemberCount(const StreamChatClient::Channel& channel) const
{
    if (channel.members.isArray())
    {
        return static_cast<int>(channel.members.size());
    }
    return 0;
}
