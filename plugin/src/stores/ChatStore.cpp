#include "ChatStore.h"

namespace Sidechain {
namespace Stores {

ChatStore::ChatStore()
{
    Util::logInfo("ChatStore", "Initialized reactive chat store");
}

//==============================================================================
// Client Setup

void ChatStore::setStreamChatClient(StreamChatClient* client)
{
    streamChatClient = client;

    if (streamChatClient)
    {
        setupEventHandlers();
        Util::logInfo("ChatStore", "Stream Chat client configured");
    }
}

void ChatStore::setupEventHandlers()
{
    if (!streamChatClient)
        return;

    // Set up real-time event callbacks
    streamChatClient->setMessageReceivedCallback([this](const StreamChatClient::Message& message,
                                                         const juce::String& channelId)
    {
        handleMessageReceived(message, channelId);
    });

    streamChatClient->setTypingCallback([this](const juce::String& userId,
                                                 bool isTyping)
    {
        // Note: StreamChatClient typing events don't include channelId
        // Use currently selected channel or watched channel
        auto currentChannelId = getState().currentChannelId;
        if (!currentChannelId.isEmpty())
            handleTypingEvent(currentChannelId, userId, isTyping);
    });

    streamChatClient->setConnectionStatusCallback([this](StreamChatClient::ConnectionStatus status)
    {
        handleConnectionStatusChanged(status);
    });

    streamChatClient->setPresenceChangedCallback([this](const StreamChatClient::UserPresence& presence)
    {
        updateUserPresence(presence.userId, presence);
    });
}

//==============================================================================
// Authentication

void ChatStore::setAuthentication(const juce::String& token, const juce::String& apiKey,
                                   const juce::String& userId)
{
    Util::logInfo("ChatStore", "Setting authentication", "userId=" + userId);

    updateState([token, apiKey, userId](ChatStoreState& state)
    {
        state.chatToken = token;
        state.apiKey = apiKey;
        state.userId = userId;
        state.isAuthenticated = !token.isEmpty() && !apiKey.isEmpty();
        state.isConnecting = true;
    });

    if (streamChatClient)
    {
        streamChatClient->setToken(token, apiKey, userId);
        streamChatClient->connectWebSocket();
    }

    // Load channels after authentication
    loadChannels();
}

void ChatStore::clearAuthentication()
{
    Util::logInfo("ChatStore", "Clearing authentication");

    if (streamChatClient)
    {
        streamChatClient->disconnectWebSocket();
    }

    updateState([](ChatStoreState& state)
    {
        state = ChatStoreState{}; // Reset to default
        state.isAuthenticated = false;
    });
}

//==============================================================================
// Channel Management

void ChatStore::loadChannels(bool forceRefresh)
{
    if (!streamChatClient)
    {
        Util::logError("ChatStore", "Cannot load channels - client not configured");
        return;
    }

    if (!isAuthenticated())
    {
        Util::logWarning("ChatStore", "Cannot load channels - not authenticated");
        return;
    }

    Util::logInfo("ChatStore", "Loading channels", "forceRefresh=" + juce::String(forceRefresh ? "true" : "false"));

    updateState([](ChatStoreState& state)
    {
        state.isLoadingChannels = true;
        state.error = "";
    });

    streamChatClient->queryChannels([this](Outcome<std::vector<StreamChatClient::Channel>> result)
    {
        if (result.isOk())
        {
            handleChannelsLoaded(result.getValue());
        }
        else
        {
            handleChannelsError(result.getError());
        }
    });
}

void ChatStore::createDirectChannel(const juce::String& targetUserId)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot create channel - not authenticated");
        return;
    }

    Util::logInfo("ChatStore", "Creating direct channel", "targetUserId=" + targetUserId);

    streamChatClient->createDirectChannel(targetUserId,
        [this, targetUserId](Outcome<StreamChatClient::Channel> result)
        {
            if (result.isOk())
            {
                auto channel = result.getValue();

                updateState([&channel](ChatStoreState& state)
                {
                    ChannelState channelState;
                    channelState.id = channel.id;
                    channelState.type = channel.type;
                    channelState.name = channel.name;
                    channelState.lastMessageAt = channel.lastMessageAt;

                    state.channels[channel.id] = channelState;
                    state.channelOrder.insert(state.channelOrder.begin(), channel.id);
                    state.currentChannelId = channel.id;
                });

                Util::logInfo("ChatStore", "Direct channel created", "channelId=" + channel.id);

                // Load messages for the new channel
                loadMessages(channel.id);
            }
            else
            {
                Util::logError("ChatStore", "Failed to create direct channel: " + result.getError(),
                               "targetUserId=" + targetUserId);
            }
        }
    );
}

void ChatStore::createGroupChannel(const juce::String& channelId, const juce::String& name,
                                    const std::vector<juce::String>& memberIds)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot create group channel - not authenticated");
        return;
    }

    Util::logInfo("ChatStore", "Creating group channel", "channelId=" + channelId + " name=" + name);

    streamChatClient->createGroupChannel(channelId, name, memberIds,
        [this](Outcome<StreamChatClient::Channel> result)
        {
            if (result.isOk())
            {
                auto channel = result.getValue();

                updateState([&channel](ChatStoreState& state)
                {
                    ChannelState channelState;
                    channelState.id = channel.id;
                    channelState.type = channel.type;
                    channelState.name = channel.name;
                    channelState.lastMessageAt = channel.lastMessageAt;

                    state.channels[channel.id] = channelState;
                    state.channelOrder.insert(state.channelOrder.begin(), channel.id);
                    state.currentChannelId = channel.id;
                });

                Util::logInfo("ChatStore", "Group channel created", "channelId=" + channel.id);

                loadMessages(channel.id);
            }
            else
            {
                Util::logError("ChatStore", "Failed to create group channel: " + result.getError());
            }
        }
    );
}

void ChatStore::deleteChannel(const juce::String& channelId)
{
    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
    {
        Util::logWarning("ChatStore", "Cannot delete channel - not found", "channelId=" + channelId);
        return;
    }

    auto channelType = it->second.type;

    Util::logInfo("ChatStore", "Deleting channel", "channelId=" + channelId);

    if (streamChatClient)
    {
        streamChatClient->deleteChannel(channelType, channelId,
            [this, channelId](Outcome<void> result)
            {
                if (result.isOk())
                {
                    updateState([channelId](ChatStoreState& state)
                    {
                        state.channels.erase(channelId);
                        auto orderIt = std::find(state.channelOrder.begin(), state.channelOrder.end(), channelId);
                        if (orderIt != state.channelOrder.end())
                            state.channelOrder.erase(orderIt);

                        // If this was the current channel, clear selection
                        if (state.currentChannelId == channelId)
                            state.currentChannelId = "";
                    });

                    Util::logInfo("ChatStore", "Channel deleted", "channelId=" + channelId);
                }
                else
                {
                    Util::logError("ChatStore", "Failed to delete channel: " + result.getError(),
                                   "channelId=" + channelId);
                }
            }
        );
    }
}

void ChatStore::leaveChannel(const juce::String& channelId)
{
    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
    {
        Util::logWarning("ChatStore", "Cannot leave channel - not found", "channelId=" + channelId);
        return;
    }

    auto channelType = it->second.type;

    Util::logInfo("ChatStore", "Leaving channel", "channelId=" + channelId);

    if (streamChatClient)
    {
        streamChatClient->leaveChannel(channelType, channelId,
            [this, channelId](Outcome<void> result)
            {
                if (result.isOk())
                {
                    // Same as delete for local state
                    updateState([channelId](ChatStoreState& state)
                    {
                        state.channels.erase(channelId);
                        auto orderIt = std::find(state.channelOrder.begin(), state.channelOrder.end(), channelId);
                        if (orderIt != state.channelOrder.end())
                            state.channelOrder.erase(orderIt);

                        if (state.currentChannelId == channelId)
                            state.currentChannelId = "";
                    });

                    Util::logInfo("ChatStore", "Left channel", "channelId=" + channelId);
                }
                else
                {
                    Util::logError("ChatStore", "Failed to leave channel: " + result.getError(),
                                   "channelId=" + channelId);
                }
            }
        );
    }
}

void ChatStore::selectChannel(const juce::String& channelId)
{
    Util::logDebug("ChatStore", "Selecting channel", "channelId=" + channelId);

    updateState([channelId](ChatStoreState& state)
    {
        state.currentChannelId = channelId;
    });

    // Load messages if not already loaded
    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it != currentState.channels.end() && it->second.messages.empty())
    {
        loadMessages(channelId);
    }

    // Mark as read
    markAsRead(channelId);
}

//==============================================================================
// Message Management

void ChatStore::loadMessages(const juce::String& channelId, int limit)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot load messages - not authenticated");
        return;
    }

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
    {
        Util::logWarning("ChatStore", "Cannot load messages - channel not found", "channelId=" + channelId);
        return;
    }

    Util::logInfo("ChatStore", "Loading messages", "channelId=" + channelId + " limit=" + juce::String(limit));

    updateState([channelId](ChatStoreState& state)
    {
        auto& channel = state.channels[channelId];
        channel.isLoadingMessages = true;
    });

    auto channelType = it->second.type;

    streamChatClient->queryMessages(channelType, channelId, limit, 0,
        [this, channelId](Outcome<std::vector<StreamChatClient::Message>> result)
        {
            if (result.isOk())
            {
                handleMessagesLoaded(channelId, result.getValue());
            }
            else
            {
                handleMessagesError(channelId, result.getError());
            }
        }
    );
}

void ChatStore::loadMoreMessages(const juce::String& channelId)
{
    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end() || !it->second.hasMoreMessages)
    {
        return;
    }

    // Load messages before the oldest message
    loadMessages(channelId, 50);
}

void ChatStore::sendMessage(const juce::String& channelId, const juce::String& text)
{
    if (text.isEmpty())
        return;

    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot send message - not authenticated");
        return;
    }

    Util::logInfo("ChatStore", "Sending message", "channelId=" + channelId);

    // Generate temporary ID for optimistic update
    auto tempId = generateTempMessageId();
    auto userId = getState().userId;

    // Optimistic update - add message immediately
    optimisticUpdate(
        [channelId, text, tempId, userId](ChatStoreState& state)
        {
            auto& channel = state.channels[channelId];

            StreamChatClient::Message tempMessage;
            tempMessage.id = tempId;
            tempMessage.text = text;
            tempMessage.userId = userId;
            tempMessage.createdAt = juce::Time::getCurrentTime().toISO8601(true);

            channel.messages.push_back(tempMessage);
            channel.draftText = ""; // Clear draft
        },
        [this, channelId, text, tempId](auto callback)
        {
            auto currentState = getState();
            auto it = currentState.channels.find(channelId);
            if (it == currentState.channels.end())
            {
                callback(false, "Channel not found");
                return;
            }

            auto channelType = it->second.type;

            streamChatClient->sendMessage(channelType, channelId, text, juce::var(),
                [this, channelId, tempId, callback](Outcome<StreamChatClient::Message> result)
                {
                    if (result.isOk())
                    {
                        handleMessageSent(channelId, result.getValue());
                        callback(true, "");
                    }
                    else
                    {
                        handleMessageSendError(channelId, tempId, result.getError());
                        callback(false, result.getError());
                    }
                }
            );
        },
        [channelId, tempId](const juce::String& error)
        {
            Util::logError("ChatStore", "Failed to send message: " + error,
                           "channelId=" + channelId + " tempId=" + tempId);
        }
    );
}

void ChatStore::sendMessageWithAudio(const juce::String& channelId, const juce::String& text,
                                      const juce::AudioBuffer<float>& audioBuffer, double sampleRate)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot send audio message - not authenticated");
        return;
    }

    Util::logInfo("ChatStore", "Sending audio message", "channelId=" + channelId);

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
        return;

    auto channelType = it->second.type;

    streamChatClient->sendMessageWithAudio(channelType, channelId, text, audioBuffer, sampleRate,
        [this, channelId](Outcome<StreamChatClient::Message> result)
        {
            if (result.isOk())
            {
                handleMessageSent(channelId, result.getValue());
            }
            else
            {
                Util::logError("ChatStore", "Failed to send audio message: " + result.getError(),
                               "channelId=" + channelId);
            }
        }
    );
}

void ChatStore::deleteMessage(const juce::String& channelId, const juce::String& messageId)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot delete message - not authenticated");
        return;
    }

    Util::logInfo("ChatStore", "Deleting message", "channelId=" + channelId + " messageId=" + messageId);

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
        return;

    auto channelType = it->second.type;

    streamChatClient->deleteMessage(channelType, channelId, messageId,
        [this, channelId, messageId](Outcome<void> result)
        {
            if (result.isOk())
            {
                updateState([channelId, messageId](ChatStoreState& state)
                {
                    auto& channel = state.channels[channelId];
                    auto msgIt = std::find_if(channel.messages.begin(), channel.messages.end(),
                        [&messageId](const StreamChatClient::Message& msg) { return msg.id == messageId; });

                    if (msgIt != channel.messages.end())
                    {
                        msgIt->isDeleted = true;
                        msgIt->text = "[deleted]";
                    }
                });

                Util::logInfo("ChatStore", "Message deleted", "messageId=" + messageId);
            }
            else
            {
                Util::logError("ChatStore", "Failed to delete message: " + result.getError(),
                               "messageId=" + messageId);
            }
        }
    );
}

void ChatStore::addReaction(const juce::String& channelId, const juce::String& messageId,
                            const juce::String& reaction)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot add reaction - not authenticated");
        return;
    }

    Util::logDebug("ChatStore", "Adding reaction", "messageId=" + messageId + " reaction=" + reaction);

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
        return;

    auto channelType = it->second.type;

    streamChatClient->addReaction(channelType, channelId, messageId, reaction,
        [messageId, reaction](Outcome<void> result)
        {
            if (!result.isOk())
            {
                Util::logError("ChatStore", "Failed to add reaction: " + result.getError(),
                               "messageId=" + messageId + " reaction=" + reaction);
            }
        }
    );
}

//==============================================================================
// Typing Indicators

void ChatStore::startTyping(const juce::String& channelId)
{
    if (!streamChatClient || !isAuthenticated())
        return;

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
        return;

    auto channelType = it->second.type;
    streamChatClient->sendTypingIndicator(channelType, channelId, true);
}

void ChatStore::stopTyping(const juce::String& channelId)
{
    if (!streamChatClient || !isAuthenticated())
        return;

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
        return;

    auto channelType = it->second.type;
    streamChatClient->sendTypingIndicator(channelType, channelId, false);
}

//==============================================================================
// Drafts

void ChatStore::updateDraft(const juce::String& channelId, const juce::String& text)
{
    updateState([channelId, text](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            it->second.draftText = text;
        }
    });
}

void ChatStore::clearDraft(const juce::String& channelId)
{
    updateDraft(channelId, "");
}

//==============================================================================
// Read Receipts

void ChatStore::markAsRead(const juce::String& channelId)
{
    if (!streamChatClient || !isAuthenticated())
        return;

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
        return;

    auto channelType = it->second.type;

    streamChatClient->markChannelRead(channelType, channelId,
        [this, channelId](Outcome<void> result)
        {
            if (result.isOk())
            {
                updateState([channelId](ChatStoreState& state)
                {
                    auto& channel = state.channels[channelId];
                    channel.unreadCount = 0;
                });
            }
        }
    );
}

//==============================================================================
// Presence

void ChatStore::updateUserPresence(const juce::String& userId, const StreamChatClient::UserPresence& presence)
{
    updateState([userId, presence](ChatStoreState& state)
    {
        state.userPresence[userId] = presence;
    });
}

void ChatStore::queryPresence(const std::vector<juce::String>& userIds)
{
    if (!streamChatClient || !isAuthenticated())
        return;

    streamChatClient->queryPresence(userIds,
        [this](Outcome<std::vector<StreamChatClient::UserPresence>> result)
        {
            if (result.isOk())
            {
                for (const auto& presence : result.getValue())
                {
                    updateUserPresence(presence.userId, presence);
                }
            }
        }
    );
}

//==============================================================================
// Real-time Events

void ChatStore::handleMessageReceived(const StreamChatClient::Message& message, const juce::String& channelId)
{
    Util::logDebug("ChatStore", "Message received", "channelId=" + channelId + " messageId=" + message.id);

    updateState([message, channelId](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            auto& channel = it->second;

            // Check if message already exists (avoid duplicates)
            auto msgIt = std::find_if(channel.messages.begin(), channel.messages.end(),
                [&message](const StreamChatClient::Message& msg) { return msg.id == message.id; });

            if (msgIt == channel.messages.end())
            {
                // New message - add to end
                channel.messages.push_back(message);

                // Increment unread if not current channel
                if (state.currentChannelId != channelId)
                {
                    channel.unreadCount++;
                }
            }
        }
    });
}

void ChatStore::handleTypingEvent(const juce::String& channelId, const juce::String& userId, bool isTyping)
{
    auto currentUserId = getState().userId;
    if (userId == currentUserId)
        return; // Ignore own typing events

    updateState([channelId, userId, isTyping](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            auto& typingUsers = it->second.usersTyping;

            auto userIt = std::find(typingUsers.begin(), typingUsers.end(), userId);

            if (isTyping && userIt == typingUsers.end())
            {
                typingUsers.push_back(userId);
            }
            else if (!isTyping && userIt != typingUsers.end())
            {
                typingUsers.erase(userIt);
            }
        }
    });
}

void ChatStore::handleConnectionStatusChanged(StreamChatClient::ConnectionStatus status)
{
    Util::logInfo("ChatStore", "Connection status changed", "status=" + juce::String((int)status));

    updateState([status](ChatStoreState& state)
    {
        state.connectionStatus = status;
        state.isConnecting = (status == StreamChatClient::ConnectionStatus::Connecting);
    });
}

//==============================================================================
// Internal Helpers

void ChatStore::handleChannelsLoaded(const std::vector<StreamChatClient::Channel>& channels)
{
    Util::logInfo("ChatStore", "Channels loaded", "count=" + juce::String(channels.size()));

    updateState([&channels](ChatStoreState& state)
    {
        state.isLoadingChannels = false;
        state.channels.clear();
        state.channelOrder.clear();

        for (const auto& channel : channels)
        {
            ChannelState channelState;
            channelState.id = channel.id;
            channelState.type = channel.type;
            channelState.name = channel.name;
            channelState.unreadCount = channel.unreadCount;
            channelState.lastMessageAt = channel.lastMessageAt;
            channelState.extraData = channel.extraData;

            state.channels[channel.id] = channelState;
            state.channelOrder.push_back(channel.id);
        }
    });
}

void ChatStore::handleChannelsError(const juce::String& error)
{
    Util::logError("ChatStore", "Failed to load channels: " + error);

    updateState([error](ChatStoreState& state)
    {
        state.isLoadingChannels = false;
        state.error = error;
    });
}

void ChatStore::handleMessagesLoaded(const juce::String& channelId,
                                      const std::vector<StreamChatClient::Message>& messages)
{
    Util::logInfo("ChatStore", "Messages loaded", "channelId=" + channelId + " count=" + juce::String(messages.size()));

    updateState([channelId, &messages](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            auto& channel = it->second;
            channel.isLoadingMessages = false;
            channel.messages.clear();

            for (const auto& msg : messages)
            {
                channel.messages.push_back(msg);
            }

            channel.hasMoreMessages = messages.size() >= 50; // Assume more if we got full page
        }
    });
}

void ChatStore::handleMessagesError(const juce::String& channelId, const juce::String& error)
{
    Util::logError("ChatStore", "Failed to load messages: " + error, "channelId=" + channelId);

    updateState([channelId](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            it->second.isLoadingMessages = false;
        }
    });
}

void ChatStore::handleMessageSent(const juce::String& channelId, const StreamChatClient::Message& message)
{
    Util::logInfo("ChatStore", "Message sent successfully", "channelId=" + channelId + " messageId=" + message.id);

    updateState([channelId, message](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            auto& channel = it->second;

            // Find and replace temporary message with real one
            auto msgIt = std::find_if(channel.messages.begin(), channel.messages.end(),
                [](const StreamChatClient::Message& msg) {
                    return msg.id.startsWith("temp_");
                });

            if (msgIt != channel.messages.end())
            {
                *msgIt = message;
            }
            else
            {
                // Not found, just add it
                channel.messages.push_back(message);
            }
        }
    });
}

void ChatStore::handleMessageSendError(const juce::String& channelId, const juce::String& tempId,
                                        const juce::String& error)
{
    Util::logError("ChatStore", "Failed to send message: " + error,
                   "channelId=" + channelId + " tempId=" + tempId);

    // Remove the temporary message
    updateState([channelId, tempId](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            auto& channel = it->second;
            auto msgIt = std::find_if(channel.messages.begin(), channel.messages.end(),
                [&tempId](const StreamChatClient::Message& msg) { return msg.id == tempId; });

            if (msgIt != channel.messages.end())
            {
                channel.messages.erase(msgIt);
            }
        }
    });
}

juce::String ChatStore::generateTempMessageId() const
{
    // Generate temporary ID using timestamp + random number
    auto timestamp = juce::Time::currentTimeMillis();
    auto random = juce::Random::getSystemRandom().nextInt();
    return "temp_" + juce::String(timestamp) + "_" + juce::String(random);
}

}  // namespace Stores
}  // namespace Sidechain
