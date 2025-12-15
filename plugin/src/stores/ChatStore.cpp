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

    // If channel doesn't exist in our map, create a placeholder entry
    // This handles the case where MessagesList loads channels independently
    // and the user opens a channel before ChatStore has loaded its own channel list
    juce::String channelType = "messaging";  // Default to messaging type
    if (it != currentState.channels.end())
    {
        channelType = it->second.type;
    }
    else
    {
        Log::debug("ChatStore::loadMessages - Channel not in map, creating placeholder. channelId=" + channelId);
        // Create placeholder channel entry so messages can be loaded
        updateState([channelId](ChatStoreState& state)
        {
            if (state.channels.find(channelId) == state.channels.end())
            {
                ChannelState newChannel;
                newChannel.id = channelId;
                newChannel.type = "messaging";
                state.channels[channelId] = newChannel;
            }
        });
    }

    Util::logInfo("ChatStore", "Loading messages", "channelId=" + channelId + " limit=" + juce::String(limit));

    updateState([channelId](ChatStoreState& state)
    {
        auto& channel = state.channels[channelId];
        channel.isLoadingMessages = true;
    });

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
    Log::debug("ChatStore::sendMessage - text: " + text + ", channelId: " + channelId);

    if (text.isEmpty())
    {
        Log::debug("ChatStore::sendMessage - text is empty, returning");
        return;
    }

    Log::debug("ChatStore::sendMessage - streamChatClient: " + juce::String(streamChatClient != nullptr ? "SET" : "NULL") +
              ", isAuthenticated: " + juce::String(isAuthenticated() ? "YES" : "NO"));

    if (!streamChatClient || !isAuthenticated())
    {
        Log::error("ChatStore::sendMessage - Cannot send message - streamChatClient=" + juce::String(streamChatClient != nullptr ? "SET" : "NULL") +
                   ", authenticated=" + juce::String(isAuthenticated() ? "YES" : "NO"));
        Util::logError("ChatStore", "Cannot send message - not authenticated");
        return;
    }

    Log::debug("ChatStore::sendMessage - Proceeding with message send");
    Util::logInfo("ChatStore", "Sending message", "channelId=" + channelId);

    // Generate temporary ID for optimistic update
    auto tempId = generateTempMessageId();
    auto userId = getState().userId;

    // Optimistic update - add message immediately
    Log::debug("ChatStore::sendMessage - Starting optimistic update with tempId: " + tempId);
    Log::debug("ChatStore::sendMessage - State BEFORE optimisticUpdate: channel has " +
               juce::String(getState().channels.count(channelId) > 0 ?
                           juce::String(getState().channels.at(channelId).messages.size()) :
                           juce::String(-1)) + " messages");

    optimisticUpdate(
        [channelId, text, tempId, userId](ChatStoreState& state)
        {
            Log::debug("ChatStore::sendMessage - Optimistic callback firing for channelId: " + channelId);
            auto& channel = state.channels[channelId];

            StreamChatClient::Message tempMessage;
            tempMessage.id = tempId;
            tempMessage.text = text;
            tempMessage.userId = userId;
            tempMessage.createdAt = juce::Time::getCurrentTime().toISO8601(true);

            channel.messages.push_back(tempMessage);
            channel.draftText = ""; // Clear draft

            Log::debug("ChatStore::sendMessage - Added temp message. Channel now has " + juce::String(channel.messages.size()) + " messages");
        },
        [this, channelId, text, tempId](auto callback)
        {
            Log::debug("ChatStore::sendMessage - asyncOperation callback: State NOW has " +
                       juce::String(getState().channels.count(channelId) > 0 ?
                                   juce::String(getState().channels.at(channelId).messages.size()) :
                                   juce::String(-1)) + " messages");

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
// Sharing Content to Channels (Task 2.5)

void ChatStore::sharePostToChannels(const juce::String& postId, const std::vector<juce::String>& channelIds,
                                   const juce::String& optionalMessage)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot share post - not authenticated");
        return;
    }

    if (postId.isEmpty() || channelIds.empty())
    {
        Util::logError("ChatStore", "Cannot share post - invalid postId or channelIds");
        return;
    }

    Util::logDebug("ChatStore", "Sharing post to channels", "postId=" + postId + " channelCount=" + juce::String((int)channelIds.size()));

    // Build shared content object with post metadata
    auto* sharedObj = new juce::DynamicObject();
    sharedObj->setProperty("type", "post");
    sharedObj->setProperty("id", postId);
    // Additional post data would be fetched separately if needed
    juce::var sharedContent(sharedObj);

    // Send to each channel
    for (const auto& channelId : channelIds)
    {
        sendMessageWithSharedContent(channelId, optionalMessage, sharedContent);
    }
}

void ChatStore::shareStoryToChannels(const juce::String& storyId, const std::vector<juce::String>& channelIds,
                                    const juce::String& optionalMessage)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot share story - not authenticated");
        return;
    }

    if (storyId.isEmpty() || channelIds.empty())
    {
        Util::logError("ChatStore", "Cannot share story - invalid storyId or channelIds");
        return;
    }

    Util::logDebug("ChatStore", "Sharing story to channels", "storyId=" + storyId + " channelCount=" + juce::String((int)channelIds.size()));

    // Build shared content object with story metadata
    auto* sharedObj = new juce::DynamicObject();
    sharedObj->setProperty("type", "story");
    sharedObj->setProperty("id", storyId);
    // Additional story data would be fetched separately if needed
    juce::var sharedContent(sharedObj);

    // Send to each channel
    for (const auto& channelId : channelIds)
    {
        sendMessageWithSharedContent(channelId, optionalMessage, sharedContent);
    }
}

void ChatStore::sendMessageWithSharedContent(const juce::String& channelId, const juce::String& text,
                                            const juce::var& sharedContent)
{
    if (!streamChatClient || !isAuthenticated())
    {
        Util::logError("ChatStore", "Cannot send message - not authenticated");
        return;
    }

    auto currentState = getState();
    auto it = currentState.channels.find(channelId);
    if (it == currentState.channels.end())
    {
        Util::logWarning("ChatStore", "Channel not found for sharing", "channelId=" + channelId);
        return;
    }

    Util::logDebug("ChatStore", "Sending message with shared content", "channelId=" + channelId);

    // Build message with shared content
    auto* messageObj = new juce::DynamicObject();
    if (text.isNotEmpty())
        messageObj->setProperty("text", text);
    messageObj->setProperty("shared_content", sharedContent);
    juce::var messageData(messageObj);

    auto channelType = it->second.type;

    // Send message via StreamChatClient
    streamChatClient->sendMessage(channelType, channelId, text, messageData,
        [channelId, text](Outcome<StreamChatClient::Message> result)
        {
            if (!result.isOk())
            {
                Util::logError("ChatStore", "Failed to send message with shared content: " + result.getError(),
                               "channelId=" + channelId);
            }
            else
            {
                Util::logDebug("ChatStore", "Message with shared content sent successfully", "channelId=" + channelId);
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
    Log::info("ChatStore::handleMessageSent - Received message for channelId: " + channelId + " messageId: " + message.id);
    Util::logInfo("ChatStore", "Message sent successfully", "channelId=" + channelId + " messageId=" + message.id);

    updateState([channelId, message](ChatStoreState& state)
    {
        Log::debug("ChatStore::handleMessageSent - updateState callback firing");
        auto it = state.channels.find(channelId);
        if (it != state.channels.end())
        {
            auto& channel = it->second;
            Log::debug("ChatStore::handleMessageSent - Found channel. Currently has " + juce::String(channel.messages.size()) + " messages");

            // Find and replace temporary message with real one
            auto msgIt = std::find_if(channel.messages.begin(), channel.messages.end(),
                [](const StreamChatClient::Message& msg) {
                    return msg.id.startsWith("temp_");
                });

            if (msgIt != channel.messages.end())
            {
                Log::debug("ChatStore::handleMessageSent - Replacing temp message with real one");
                *msgIt = message;
            }
            else
            {
                // Not found, just add it
                Log::debug("ChatStore::handleMessageSent - Temp message not found, adding new message");
                channel.messages.push_back(message);
            }
            Log::debug("ChatStore::handleMessageSent - Channel now has " + juce::String(channel.messages.size()) + " messages");
        }
        else
        {
            Log::error("ChatStore::handleMessageSent - Channel not found in state!");
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

//==============================================================================
// Collaborative Channel Description Editing (Task 4.20)

int ChatStore::getClientId()
{
    if (clientId == -1)
    {
        // Generate client ID from user ID hash
        auto userId = getState().userId;
        if (!userId.isEmpty())
        {
            // Use hash of user ID as stable client ID
            clientId = static_cast<int>(std::hash<std::string>()(userId.toStdString())) & 0x7FFFFFFF;
        }
        else
        {
            // Fallback: use random ID if not authenticated
            clientId = juce::Random::getSystemRandom().nextInt() & 0x7FFFFFFF;
        }

        Util::logInfo("ChatStore", "Generated client ID for OT: " + juce::String(clientId));
    }

    return clientId;
}

std::shared_ptr<Util::CRDT::OperationalTransform::Operation>
ChatStore::generateDescriptionOperation(const juce::String& oldDescription, const juce::String& newDescription)
{
    using namespace Util::CRDT;

    // Convert to std::string for easier manipulation
    auto oldStr = oldDescription.toStdString();
    auto newStr = newDescription.toStdString();

    // Simple algorithm: if content completely changed, use Delete + Insert
    // Otherwise, find the difference
    if (oldStr.empty() && !newStr.empty())
    {
        // Pure insertion at position 0
        auto insert = std::make_shared<OperationalTransform::Insert>();
        insert->position = 0;
        insert->content = newStr;
        insert->clientId = getClientId();
        return insert;
    }

    if (!oldStr.empty() && newStr.empty())
    {
        // Pure deletion of entire content
        auto del = std::make_shared<OperationalTransform::Delete>();
        del->position = 0;
        del->length = static_cast<int>(oldStr.length());
        del->content = oldStr;
        del->clientId = getClientId();
        return del;
    }

    // For simplicity, if strings differ, replace entire content
    // In a real application, you'd implement a diff algorithm
    if (oldStr != newStr)
    {
        // Delete old, insert new
        if (!oldStr.empty())
        {
            auto del = std::make_shared<OperationalTransform::Delete>();
            del->position = 0;
            del->length = static_cast<int>(oldStr.length());
            del->content = oldStr;
            del->clientId = getClientId();
            return del;
        }
        else
        {
            auto insert = std::make_shared<OperationalTransform::Insert>();
            insert->position = 0;
            insert->content = newStr;
            insert->clientId = getClientId();
            return insert;
        }
    }

    // No change needed
    auto noop = std::make_shared<OperationalTransform::Insert>();
    noop->position = 0;
    noop->content = "";
    noop->clientId = getClientId();
    return noop;
}

void ChatStore::editChannelDescription(const juce::String& channelId, const juce::String& newDescription)
{
    Util::logInfo("ChatStore", "Editing channel description",
                  "channelId=" + channelId + ", newLen=" + juce::String(newDescription.length()));

    // Update local state
    updateState([channelId, newDescription, this](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it == state.channels.end())
            return;

        auto& channel = it->second;
        auto oldDescription = channel.description;

        // Generate operation comparing old and new description
        auto operation = generateDescriptionOperation(oldDescription, newDescription);

        if (!operation || Util::CRDT::OperationalTransform::isNoOp(operation))
        {
            Util::logDebug("ChatStore", "No-op description edit, ignoring");
            return;
        }

        // Update local description immediately (optimistic)
        channel.description = newDescription;
        channel.isSyncingDescription = true;

        // Add to operation history
        operation->timestamp = channel.operationTimestamp++;
        channel.operationHistory.push_back(operation);

        // Queue for sending to server
        channel.pendingOperations.push(operation);

        Util::logDebug("ChatStore", "Queued description operation",
                       "timestamp=" + juce::String(operation->timestamp));
    });

    // Send to server after state update
    auto state = getState();
    auto it = state.channels.find(channelId);
    if (it != state.channels.end() && !it->second.pendingOperations.empty())
    {
        auto operation = it->second.pendingOperations.front();
        sendOperationToServer(channelId, operation);
    }
}

void ChatStore::sendOperationToServer(const juce::String& channelId,
                                     const std::shared_ptr<Util::CRDT::OperationalTransform::Operation>& operation)
{
    if (!streamChatClient || !operation)
        return;

    using namespace Util::CRDT;

    // Build JSON for the operation
    juce::DynamicObject obj;
    obj.setProperty("channel_id", channelId);
    obj.setProperty("client_id", getClientId());
    obj.setProperty("timestamp", operation->timestamp);

    // Serialize operation type and data
    if (auto ins = std::dynamic_pointer_cast<OperationalTransform::Insert>(operation))
    {
        obj.setProperty("type", "insert");
        obj.setProperty("position", ins->position);
        obj.setProperty("content", juce::String(ins->content));
    }
    else if (auto del = std::dynamic_pointer_cast<OperationalTransform::Delete>(operation))
    {
        obj.setProperty("type", "delete");
        obj.setProperty("position", del->position);
        obj.setProperty("length", del->length);
        obj.setProperty("content", juce::String(del->content));
    }
    else if (auto mod = std::dynamic_pointer_cast<OperationalTransform::Modify>(operation))
    {
        obj.setProperty("type", "modify");
        obj.setProperty("position", mod->position);
        obj.setProperty("old_content", juce::String(mod->oldContent));
        obj.setProperty("new_content", juce::String(mod->newContent));
    }

    // Send via NetworkClient
    // TODO: Implement endpoint in backend: POST /api/v1/channels/{channelId}/description-operation
    Util::logDebug("ChatStore", "Sending operation to server",
                   "channel=" + channelId + ", type=" + juce::String(static_cast<int>(operation->getType())));

    // After successful send, remove from pending queue
    updateState([channelId](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it != state.channels.end() && !it->second.pendingOperations.empty())
        {
            it->second.pendingOperations.pop();
        }
    });
}

void ChatStore::applyServerOperation(const juce::String& channelId,
                                    const std::shared_ptr<Util::CRDT::OperationalTransform::Operation>& operation)
{
    if (!operation)
        return;

    using namespace Util::CRDT;

    Util::logInfo("ChatStore", "Applying server-transformed operation",
                  "channelId=" + channelId + ", timestamp=" + juce::String(operation->timestamp));

    updateState([channelId, operation](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it == state.channels.end())
            return;

        auto& channel = it->second;

        // Apply operation to description
        auto newDescription = OperationalTransform::apply(channel.description.toStdString(), operation);
        channel.description = juce::String(newDescription);

        // Add to operation history
        channel.operationHistory.push_back(operation);

        // Mark sync complete
        channel.isSyncingDescription = false;

        Util::logDebug("ChatStore", "Applied operation, description updated",
                       "newLen=" + juce::String(channel.description.length()));
    });
}

void ChatStore::handleRemoteOperation(const juce::String& channelId,
                                     const std::shared_ptr<Util::CRDT::OperationalTransform::Operation>& remoteOperation,
                                     int remoteClientId)
{
    if (!remoteOperation)
        return;

    using namespace Util::CRDT;

    Util::logInfo("ChatStore", "Handling remote operation from another user",
                  "channelId=" + channelId + ", remoteClientId=" + juce::String(remoteClientId));

    updateState([channelId, remoteOperation, remoteClientId](ChatStoreState& state)
    {
        auto it = state.channels.find(channelId);
        if (it == state.channels.end())
            return;

        auto& channel = it->second;

        // Set client ID on remote operation
        remoteOperation->clientId = remoteClientId;

        // Transform remote operation against pending local operations
        auto currentOp = remoteOperation->clone();
        while (!channel.pendingOperations.empty())
        {
            auto localOp = channel.pendingOperations.front();
            auto [transformedRemote, transformedLocal] = OperationalTransform::transform(currentOp, localOp);
            currentOp = transformedRemote;
            // Local operation stays the same as it's already applied
        }

        // Apply transformed remote operation to description
        auto newDescription = OperationalTransform::apply(channel.description.toStdString(), currentOp);
        channel.description = juce::String(newDescription);

        // Add to operation history
        channel.operationHistory.push_back(currentOp);

        Util::logDebug("ChatStore", "Applied transformed remote operation",
                       "newLen=" + juce::String(channel.description.length()));
    });
}

}  // namespace Stores
}  // namespace Sidechain
