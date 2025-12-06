#include "StreamChatClient.h"
#include "../util/Log.h"
#include "../util/Async.h"
#include "../util/Result.h"
#include <JuceHeader.h>

//==============================================================================
StreamChatClient::StreamChatClient(NetworkClient* client, const Config& cfg)
    : networkClient(client), config(cfg)
{
    Log::info("StreamChatClient initialized");
}

StreamChatClient::~StreamChatClient()
{
    disconnectWebSocket();
}

//==============================================================================
void StreamChatClient::fetchToken(const juce::String& backendAuthToken, TokenCallback callback)
{
    this->backendAuthToken = backendAuthToken;

    if (networkClient == nullptr)
    {
        Log::warn("StreamChatClient: NetworkClient not set");
        if (callback) callback(Outcome<TokenResult>::error("NetworkClient not set"));
        return;
    }

    juce::StringPairArray headers;
    headers.set("Authorization", "Bearer " + backendAuthToken);

    networkClient->getAbsolute(
        config.backendBaseUrl + "/api/v1/auth/stream-token",
        [this, callback](Outcome<juce::var> responseOutcome) {
            if (responseOutcome.isOk())
            {
                auto response = responseOutcome.getValue();
                if (response.isObject())
                {
                    auto token = response.getProperty("token", "").toString();
                    auto apiKey = response.getProperty("api_key", "").toString();
                    auto userId = response.getProperty("user_id", "").toString();

                    if (!token.isEmpty() && !apiKey.isEmpty() && !userId.isEmpty())
                    {
                        setToken(token, apiKey, userId);
                        TokenResult result;
                        result.token = token;
                        result.apiKey = apiKey;
                        result.userId = userId;
                        if (callback) callback(Outcome<TokenResult>::ok(result));
                        return;
                    }
                }
            }

            Log::error("Failed to parse stream token response");
            if (callback) callback(Outcome<TokenResult>::error(responseOutcome.isError() ? responseOutcome.getError() : "Invalid token response"));
        },
        headers
    );
}

void StreamChatClient::setToken(const juce::String& token, const juce::String& key, const juce::String& userId)
{
    chatToken = token;
    apiKey = key;
    currentUserId = userId;

    // Build WebSocket URL: wss://chat.stream-io-api.com/?api_key={key}&authorization={token}&user_id={userId}
    wsUrl = "wss://chat.stream-io-api.com/?api_key=" + apiKey + "&authorization=" + token + "&user_id=" + userId;

    Log::info("StreamChatClient token set for user: " + userId + ", API key configured");
}

//==============================================================================
juce::String StreamChatClient::buildAuthHeaders() const
{
    juce::String headers = "Stream-Auth-Type: jwt\r\n";
    headers += "Authorization: " + chatToken + "\r\n";
    headers += "Content-Type: application/json\r\n";
    return headers;
}

juce::var StreamChatClient::makeStreamRequest(const juce::String& endpoint, const juce::String& method,
                                              const juce::var& data)
{
    if (!isAuthenticated())
    {
        Log::warn("StreamChatClient: Not authenticated, cannot make request");
        return juce::var();
    }

    juce::URL url(getStreamBaseUrl() + endpoint + "?api_key=" + apiKey);

    juce::String headers = buildAuthHeaders();

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                      .withExtraHeaders(headers)
                      .withConnectionTimeoutMs(config.timeoutMs);

    if (method == "POST" || method == "PUT" || method == "DELETE")
    {
        if (!data.isVoid())
        {
            juce::String jsonData = juce::JSON::toString(data);
            url = url.withPOSTData(jsonData);
        }
        else if (method == "POST")
        {
            url = url.withPOSTData("{}");
        }
    }

    auto stream = url.createInputStream(options);

    if (stream == nullptr)
    {
        Log::error("StreamChatClient: Request failed - " + endpoint);
        return juce::var();
    }

    auto response = stream->readEntireStreamAsString();
    return juce::JSON::parse(response);
}

//==============================================================================
void StreamChatClient::createDirectChannel(const juce::String& targetUserId,
                                          std::function<void(Outcome<Channel>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<Channel>::error("Not authenticated"));
        return;
    }

    Async::run<Channel>(
        [this, targetUserId]() -> Channel {
            juce::String channelId = generateDirectChannelId(currentUserId, targetUserId);
            juce::String endpoint = "/channels/messaging/" + channelId;

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();

            juce::var members = juce::var(juce::Array<juce::var>());
            members.getArray()->add(juce::var(currentUserId));
            members.getArray()->add(juce::var(targetUserId));
            obj->setProperty("members", members);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            if (response.isObject())
            {
                auto channelData = response.getProperty("channel", juce::var());
                if (channelData.isObject())
                {
                    return parseChannel(channelData);
                }
            }

            return Channel{};
        },
        [callback](const Channel& channel) {
            if (callback)
            {
                if (!channel.id.isEmpty())
                    callback(Outcome<Channel>::ok(channel));
                else
                    callback(Outcome<Channel>::error("Failed to create channel"));
            }
        }
    );
}

void StreamChatClient::createGroupChannel(const juce::String& channelId, const juce::String& name,
                                        const std::vector<juce::String>& memberIds,
                                        std::function<void(Outcome<Channel>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<Channel>::error("Not authenticated"));
        return;
    }

    Async::run<Channel>(
        [this, channelId, name, memberIds]() -> Channel {
            juce::String endpoint = "/channels/team/" + channelId;

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();

            juce::var members = juce::var(juce::Array<juce::var>());
            for (const auto& memberId : memberIds)
            {
                members.getArray()->add(juce::var(memberId));
            }
            obj->setProperty("members", members);

            juce::var data = juce::var(new juce::DynamicObject());
            data.getDynamicObject()->setProperty("name", name);
            obj->setProperty("data", data);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            if (response.isObject())
            {
                auto channelData = response.getProperty("channel", juce::var());
                if (channelData.isObject())
                {
                    return parseChannel(channelData);
                }
            }

            return Channel{};
        },
        [callback](const Channel& channel) {
            if (callback)
            {
                if (!channel.id.isEmpty())
                    callback(Outcome<Channel>::ok(channel));
                else
                    callback(Outcome<Channel>::error("Failed to create channel"));
            }
        }
    );
}

void StreamChatClient::queryChannels(ChannelsCallback callback, int limit, int offset)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<std::vector<Channel>>::error("Not authenticated"));
        return;
    }

    Async::run<std::vector<Channel>>(
        [this, limit, offset]() -> std::vector<Channel> {
            juce::String endpoint = "/channels";

            // Build filter: {"members": {"$in": [userId]}}
            juce::var filter = juce::var(new juce::DynamicObject());
            auto* filterObj = filter.getDynamicObject();
            juce::var inArray = juce::var(juce::Array<juce::var>());
            inArray.getArray()->add(juce::var(currentUserId));
            juce::var inObj = juce::var(new juce::DynamicObject());
            inObj.getDynamicObject()->setProperty("$in", inArray);
            filterObj->setProperty("members", inObj);

            endpoint += "?filter=" + juce::URL::addEscapeChars(juce::JSON::toString(filter), true);
            endpoint += "&sort=" + juce::URL::addEscapeChars("[{\"field\":\"last_message_at\",\"direction\":-1}]", true);
            endpoint += "&limit=" + juce::String(limit);
            endpoint += "&offset=" + juce::String(offset);

            auto response = makeStreamRequest(endpoint, "GET", juce::var());

            std::vector<Channel> channels;

            if (response.isObject())
            {
                auto channelsArray = response.getProperty("channels", juce::var());
                if (channelsArray.isArray())
                {
                    for (int i = 0; i < channelsArray.getArray()->size(); i++)
                    {
                        auto channelData = channelsArray.getArray()->getReference(i);
                        channels.push_back(parseChannel(channelData));
                    }
                }
            }

            return channels;
        },
        [callback](const std::vector<Channel>& channels) {
            if (callback)
            {
                callback(Outcome<std::vector<Channel>>::ok(channels));
            }
        }
    );
}

//==============================================================================
void StreamChatClient::sendMessage(const juce::String& channelType, const juce::String& channelId,
                                  const juce::String& text, const juce::var& extraData,
                                  MessageCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<Message>::error("Not authenticated"));
        return;
    }

    Async::run<Message>(
        [this, channelType, channelId, text, extraData]() -> Message {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message";

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();

            juce::var message = juce::var(new juce::DynamicObject());
            auto* msgObj = message.getDynamicObject();
            msgObj->setProperty("text", text);

            if (!extraData.isVoid())
            {
                msgObj->setProperty("extra_data", extraData);
            }

            obj->setProperty("message", message);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            if (response.isObject())
            {
                auto messageData = response.getProperty("message", juce::var());
                if (messageData.isObject())
                {
                    return parseMessage(messageData);
                }
            }

            return Message{};
        },
        [callback](const Message& msg) {
            if (callback)
            {
                if (!msg.id.isEmpty())
                    callback(Outcome<Message>::ok(msg));
                else
                    callback(Outcome<Message>::error("Failed to send message"));
            }
        }
    );
}

void StreamChatClient::queryMessages(const juce::String& channelType, const juce::String& channelId,
                                   int limit, int offset, MessagesCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<std::vector<Message>>::error("Not authenticated"));
        return;
    }

    Async::run<std::vector<Message>>(
        [this, channelType, channelId, limit, offset]() -> std::vector<Message> {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/query";
            endpoint += "?messages.limit=" + juce::String(limit);
            endpoint += "&messages.offset=" + juce::String(offset);

            auto response = makeStreamRequest(endpoint, "GET", juce::var());

            std::vector<Message> messages;

            if (response.isObject())
            {
                auto channelData = response.getProperty("channel", juce::var());
                if (channelData.isObject())
                {
                    auto messagesArray = channelData.getProperty("messages", juce::var());
                    if (messagesArray.isArray())
                    {
                        for (int i = 0; i < messagesArray.getArray()->size(); i++)
                        {
                            auto messageData = messagesArray.getArray()->getReference(i);
                            messages.push_back(parseMessage(messageData));
                        }
                    }
                }
            }

            return messages;
        },
        [callback](const std::vector<Message>& messages) {
            if (callback)
            {
                callback(Outcome<std::vector<Message>>::ok(messages));
            }
        }
    );
}

//==============================================================================
void StreamChatClient::searchMessages(const juce::String& query, const juce::var& channelFilters,
                                    int limit, int offset, MessagesCallback callback)
{
    if (!isAuthenticated() || query.isEmpty())
    {
        if (callback) callback(Outcome<std::vector<Message>>::error("Not authenticated or query is empty"));
        return;
    }

    Async::run<std::vector<Message>>(
        [this, query, channelFilters, limit, offset]() -> std::vector<Message> {
            juce::String endpoint = "/search";

            // Build query parameters
            endpoint += "?query=" + juce::URL::addEscapeChars(query, true);
            endpoint += "&limit=" + juce::String(limit);
            endpoint += "&offset=" + juce::String(offset);

            if (!channelFilters.isVoid())
            {
                endpoint += "&filter_conditions=" + juce::URL::addEscapeChars(juce::JSON::toString(channelFilters), true);
            }

            auto response = makeStreamRequest(endpoint, "GET", juce::var());

            std::vector<Message> messages;

            if (response.isObject())
            {
                auto results = response.getProperty("results", juce::var());
                if (results.isArray())
                {
                    for (int i = 0; i < results.getArray()->size(); i++)
                    {
                        auto messageData = results.getArray()->getReference(i);
                        messages.push_back(parseMessage(messageData));
                    }
                }
            }

            return messages;
        },
        [callback](const std::vector<Message>& messages) {
            if (callback)
            {
                callback(Outcome<std::vector<Message>>::ok(messages));
            }
        }
    );
}

//==============================================================================
void StreamChatClient::queryPresence(const std::vector<juce::String>& userIds, PresenceCallback callback)
{
    if (!isAuthenticated() || userIds.empty())
    {
        if (callback) callback(Outcome<std::vector<UserPresence>>::error("Not authenticated or empty user list"));
        return;
    }

    Async::run<std::vector<UserPresence>>(
        [this, userIds]() -> std::vector<UserPresence> {
            juce::String endpoint = "/users";

            // Build filter: {"id": {"$in": [userIds]}}
            juce::var filter = juce::var(new juce::DynamicObject());
            auto* filterObj = filter.getDynamicObject();
            juce::var inArray = juce::var(juce::Array<juce::var>());
            for (const auto& userId : userIds)
            {
                inArray.getArray()->add(juce::var(userId));
            }
            juce::var inObj = juce::var(new juce::DynamicObject());
            inObj.getDynamicObject()->setProperty("$in", inArray);
            filterObj->setProperty("id", inObj);

            endpoint += "?filter=" + juce::URL::addEscapeChars(juce::JSON::toString(filter), true);
            endpoint += "&presence=true";

            auto response = makeStreamRequest(endpoint, "GET", juce::var());

            std::vector<UserPresence> presenceList;

            if (response.isObject())
            {
                auto usersArray = response.getProperty("users", juce::var());
                if (usersArray.isArray())
                {
                    for (int i = 0; i < usersArray.getArray()->size(); i++)
                    {
                        auto userData = usersArray.getArray()->getReference(i);
                        presenceList.push_back(parsePresence(userData));
                    }
                }
            }

            return presenceList;
        },
        [callback](const std::vector<UserPresence>& presenceList) {
            if (callback)
            {
                callback(Outcome<std::vector<UserPresence>>::ok(presenceList));
            }
        }
    );
}

//==============================================================================
void StreamChatClient::connectWebSocket()
{
    if (!isAuthenticated() || wsUrl.isEmpty())
    {
        Log::warn("StreamChatClient: Cannot connect WebSocket - not authenticated");
        return;
    }

    if (wsConnected.load())
    {
        Log::debug("StreamChatClient: WebSocket already connected");
        return;
    }

    updateConnectionStatus(ConnectionStatus::Connecting);

    // Note: JUCE's WebSocket support may vary by platform
    // For now, we'll use a placeholder - full WebSocket implementation
    // would require platform-specific code or a third-party library
    Log::warn("StreamChatClient: WebSocket connection not yet fully implemented");
    Log::debug("  URL: " + wsUrl);

    // TODO: Implement full WebSocket connection using JUCE's WebSocket class
    // or a platform-specific implementation
    updateConnectionStatus(ConnectionStatus::Disconnected);
}

void StreamChatClient::disconnectWebSocket()
{
    if (webSocket != nullptr)
    {
        webSocket = nullptr;
    }
    wsConnected.store(false);
    updateConnectionStatus(ConnectionStatus::Disconnected);
}

//==============================================================================
juce::String StreamChatClient::generateDirectChannelId(const juce::String& userId1, const juce::String& userId2)
{
    // Sort user IDs to ensure consistent channel ID
    juce::StringArray ids;
    ids.add(userId1);
    ids.add(userId2);
    ids.sort(true);
    return ids[0] + "_" + ids[1];
}

//==============================================================================
StreamChatClient::Channel StreamChatClient::parseChannel(const juce::var& channelData)
{
    Channel channel;

    if (channelData.isObject())
    {
        channel.id = channelData.getProperty("id", "").toString();
        channel.type = channelData.getProperty("type", "").toString();
        channel.members = channelData.getProperty("members", juce::var());

        auto data = channelData.getProperty("data", juce::var());
        if (data.isObject())
        {
            channel.name = data.getProperty("name", "").toString();
            channel.extraData = data;
        }

        channel.lastMessage = channelData.getProperty("last_message", juce::var());
        channel.unreadCount = channelData.getProperty("unread_count", 0).operator int();
        channel.lastMessageAt = channelData.getProperty("last_message_at", "").toString();
    }

    return channel;
}

StreamChatClient::Message StreamChatClient::parseMessage(const juce::var& messageData)
{
    Message message;

    if (messageData.isObject())
    {
        message.id = messageData.getProperty("id", "").toString();
        message.text = messageData.getProperty("text", "").toString();
        message.userId = messageData.getProperty("user_id", "").toString();
        message.createdAt = messageData.getProperty("created_at", "").toString();
        message.reactions = messageData.getProperty("reactions", juce::var());
        message.extraData = messageData.getProperty("extra_data", juce::var());
        message.isDeleted = messageData.getProperty("deleted_at", juce::var()).isString();

        auto user = messageData.getProperty("user", juce::var());
        if (user.isObject())
        {
            message.userName = user.getProperty("name", "").toString();
        }
    }

    return message;
}

StreamChatClient::UserPresence StreamChatClient::parsePresence(const juce::var& userData)
{
    UserPresence presence;

    if (userData.isObject())
    {
        presence.userId = userData.getProperty("id", "").toString();
        presence.online = userData.getProperty("online", false).operator bool();
        presence.lastActive = userData.getProperty("last_active", "").toString();
        presence.status = userData.getProperty("status", "").toString();
    }

    return presence;
}

//==============================================================================
void StreamChatClient::updateConnectionStatus(ConnectionStatus status)
{
    auto previousStatus = connectionStatus.exchange(status);
    if (previousStatus != status && connectionStatusCallback)
    {
        juce::MessageManager::callAsync([this, status]() {
            if (connectionStatusCallback)
                connectionStatusCallback(status);
        });
    }
}

//==============================================================================
void StreamChatClient::getChannel(const juce::String& channelType, const juce::String& channelId,
                                  std::function<void(Outcome<Channel>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<Channel>::error("Not authenticated"));
        return;
    }

    Async::run<Channel>(
        [this, channelType, channelId]() -> Channel {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId;

            auto response = makeStreamRequest(endpoint, "GET", juce::var());

            if (response.isObject())
            {
                auto channelData = response.getProperty("channel", juce::var());
                if (channelData.isObject())
                {
                    return parseChannel(channelData);
                }
            }

            return Channel{};
        },
        [callback](const Channel& channel) {
            if (callback)
            {
                if (!channel.id.isEmpty())
                    callback(Outcome<Channel>::ok(channel));
                else
                    callback(Outcome<Channel>::error("Failed to create channel"));
            }
        }
    );
}

void StreamChatClient::deleteChannel(const juce::String& channelType, const juce::String& channelId,
                                    std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, channelType, channelId]() -> bool {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId;

            auto response = makeStreamRequest(endpoint, "DELETE", juce::var());

            return response.isObject() && !response.getProperty("channel", juce::var()).isVoid();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

void StreamChatClient::leaveChannel(const juce::String& channelType, const juce::String& channelId,
                                   std::function<void(Outcome<void>)> callback)
{
    // Leave channel is just removing the current user
    removeMembers(channelType, channelId, {currentUserId}, callback);
}

void StreamChatClient::addMembers(const juce::String& channelType, const juce::String& channelId,
                                 const std::vector<juce::String>& memberIds,
                                 std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, channelType, channelId, memberIds]() -> bool {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/add_members";

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();
            juce::var members = juce::var(juce::Array<juce::var>());
            for (const auto& memberId : memberIds)
            {
                members.getArray()->add(juce::var(memberId));
            }
            obj->setProperty("user_ids", members);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            return response.isObject() && !response.getProperty("channel", juce::var()).isVoid();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

void StreamChatClient::removeMembers(const juce::String& channelType, const juce::String& channelId,
                                    const std::vector<juce::String>& memberIds,
                                    std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, channelType, channelId, memberIds]() -> bool {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/remove_members";

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();
            juce::var members = juce::var(juce::Array<juce::var>());
            for (const auto& memberId : memberIds)
            {
                members.getArray()->add(juce::var(memberId));
            }
            obj->setProperty("user_ids", members);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            return response.isObject() && !response.getProperty("channel", juce::var()).isVoid();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

void StreamChatClient::updateChannel(const juce::String& channelType, const juce::String& channelId,
                                    const juce::String& name, const juce::var& extraData,
                                    std::function<void(Outcome<Channel>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<Channel>::error("Not authenticated"));
        return;
    }

    Async::run<Channel>(
        [this, channelType, channelId, name, extraData]() -> Channel {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId;

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();

            juce::var data = juce::var(new juce::DynamicObject());
            auto* dataObj = data.getDynamicObject();

            if (!name.isEmpty())
            {
                dataObj->setProperty("name", name);
            }

            // Merge extra data if provided
            if (!extraData.isVoid() && extraData.isObject())
            {
                auto* extraObj = extraData.getDynamicObject();
                auto props = extraObj->getProperties();
                for (int i = 0; i < props.size(); ++i)
                {
                    auto name = props.getName(i);
                    if (name != "name")  // Don't override name if already set
                    {
                        dataObj->setProperty(name, props.getValueAt(i));
                    }
                }
            }

            obj->setProperty("data", data);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            if (response.isObject())
            {
                auto channelData = response.getProperty("channel", juce::var());
                if (channelData.isObject())
                {
                    return parseChannel(channelData);
                }
            }

            return Channel{};
        },
        [callback](const Channel& channel) {
            if (callback)
            {
                if (!channel.id.isEmpty())
                    callback(Outcome<Channel>::ok(channel));
                else
                    callback(Outcome<Channel>::error("Failed to update channel"));
            }
        }
    );
}

void StreamChatClient::updateMessage(const juce::String& channelType, const juce::String& channelId,
                                    const juce::String& messageId, const juce::String& newText,
                                    MessageCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<Message>::error("Not authenticated"));
        return;
    }

    Async::run<Message>(
        [this, channelType, channelId, messageId, newText]() -> Message {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message";

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();

            juce::var message = juce::var(new juce::DynamicObject());
            auto* msgObj = message.getDynamicObject();
            msgObj->setProperty("id", messageId);
            msgObj->setProperty("text", newText);

            obj->setProperty("message", message);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            if (response.isObject())
            {
                auto messageData = response.getProperty("message", juce::var());
                if (messageData.isObject())
                {
                    return parseMessage(messageData);
                }
            }

            return Message{};
        },
        [callback](const Message& msg) {
            if (callback)
            {
                if (!msg.id.isEmpty())
                    callback(Outcome<Message>::ok(msg));
                else
                    callback(Outcome<Message>::error("Failed to update message"));
            }
        }
    );
}

void StreamChatClient::deleteMessage(const juce::String& channelType, const juce::String& channelId,
                                    const juce::String& messageId, std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, channelType, channelId, messageId]() -> bool {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message/" + messageId;

            auto response = makeStreamRequest(endpoint, "DELETE", juce::var());

            return response.isObject() && !response.getProperty("message", juce::var()).isVoid();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

void StreamChatClient::addReaction(const juce::String& channelType, const juce::String& channelId,
                                  const juce::String& messageId, const juce::String& reactionType,
                                  std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, channelType, channelId, messageId, reactionType]() -> bool {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message/" + messageId + "/reaction";

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();

            juce::var reaction = juce::var(new juce::DynamicObject());
            reaction.getDynamicObject()->setProperty("type", reactionType);
            obj->setProperty("reaction", reaction);

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            return response.isObject() && !response.getProperty("message", juce::var()).isVoid();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

void StreamChatClient::removeReaction(const juce::String& channelType, const juce::String& channelId,
                                     const juce::String& messageId, const juce::String& reactionType,
                                     std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, channelType, channelId, messageId, reactionType]() -> bool {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message/" + messageId + "/reaction/" + reactionType;

            auto response = makeStreamRequest(endpoint, "DELETE", juce::var());

            return response.isObject() && !response.getProperty("message", juce::var()).isVoid();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

void StreamChatClient::markChannelRead(const juce::String& channelType, const juce::String& channelId,
                                      std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, channelType, channelId]() -> bool {
            juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/read";

            auto response = makeStreamRequest(endpoint, "POST", juce::var());

            return response.isObject();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

void StreamChatClient::updateStatus(const juce::String& status, const juce::var& extraData,
                                   std::function<void(Outcome<void>)> callback)
{
    if (!isAuthenticated())
    {
        if (callback) callback(Outcome<void>::error("Not authenticated"));
        return;
    }

    Async::run<bool>(
        [this, status, extraData]() -> bool {
            juce::String endpoint = "/users/" + currentUserId;

            juce::var requestData = juce::var(new juce::DynamicObject());
            auto* obj = requestData.getDynamicObject();
            obj->setProperty("status", status);

            if (!extraData.isVoid())
            {
                obj->setProperty("extra_data", extraData);
            }

            auto response = makeStreamRequest(endpoint, "POST", requestData);

            return response.isObject() && !response.getProperty("user", juce::var()).isVoid();
        },
        [callback](bool success) {
            if (callback)
            {
                if (success)
                    callback(Outcome<void>::ok());
                else
                    callback(Outcome<void>::error("Operation failed"));
            }
        }
    );
}

//==============================================================================
// Channel Watching (Polling-based Real-time)

// Helper class for channel polling timer
class ChannelPollTimer : public juce::Timer
{
public:
    ChannelPollTimer(std::function<void()> callback) : pollCallback(callback) {}
    void timerCallback() override { if (pollCallback) pollCallback(); }
private:
    std::function<void()> pollCallback;
};

// Helper class for unread count polling timer
class UnreadPollTimer : public juce::Timer
{
public:
    UnreadPollTimer(std::function<void()> callback) : pollCallback(callback) {}
    void timerCallback() override { if (pollCallback) pollCallback(); }
private:
    std::function<void()> pollCallback;
};

void StreamChatClient::watchChannel(const juce::String& channelType, const juce::String& channelId)
{
    Log::info("StreamChatClient: Watching channel " + channelType + "/" + channelId);

    watchedChannelType = channelType;
    watchedChannelId = channelId;
    lastSeenMessageId = "";

    // Stop existing timer if any
    if (channelPollTimer)
    {
        channelPollTimer->stopTimer();
    }

    // Create new polling timer - poll every 2 seconds for responsive messaging
    channelPollTimer = std::make_unique<ChannelPollTimer>([this]() {
        pollWatchedChannel();
    });
    static_cast<ChannelPollTimer*>(channelPollTimer.get())->startTimer(2000);

    // Initial poll
    pollWatchedChannel();
}

void StreamChatClient::unwatchChannel()
{
    Log::info("StreamChatClient: Unwatching channel");

    watchedChannelType = "";
    watchedChannelId = "";
    lastSeenMessageId = "";

    if (channelPollTimer)
    {
        channelPollTimer->stopTimer();
        channelPollTimer.reset();
    }
}

void StreamChatClient::pollWatchedChannel()
{
    if (watchedChannelId.isEmpty() || !isAuthenticated())
        return;

    // Query messages to check for new ones
    queryMessages(watchedChannelType, watchedChannelId, 20, 0,
        [this](Outcome<std::vector<Message>> result) {
            if (result.isError() || result.getValue().empty())
                return;

            auto messages = result.getValue();

            // Sort by created_at to find the newest
            if (messages.empty())
                return;

            // The newest message should be at the end (messages are usually returned oldest first)
            const auto& newestMessage = messages.back();

            // If we have a new message that we haven't seen
            if (!newestMessage.id.isEmpty() && newestMessage.id != lastSeenMessageId)
            {
                // If this is not our first poll (lastSeenMessageId was set)
                if (!lastSeenMessageId.isEmpty())
                {
                    // Find all new messages
                    bool foundLastSeen = false;
                    for (const auto& msg : messages)
                    {
                        if (msg.id == lastSeenMessageId)
                        {
                            foundLastSeen = true;
                            continue;
                        }

                        if (foundLastSeen && messageReceivedCallback)
                        {
                            // Only notify for messages from other users
                            if (msg.userId != currentUserId)
                            {
                                Log::debug("StreamChatClient: New message received from " + msg.userName);
                                juce::MessageManager::callAsync([this, msg]() {
                                    if (messageReceivedCallback)
                                        messageReceivedCallback(msg, watchedChannelId);
                                });
                            }
                        }
                    }
                }

                lastSeenMessageId = newestMessage.id;
            }
        });
}

void StreamChatClient::pollUnreadCount()
{
    if (!isAuthenticated())
        return;

    // Query all channels to get total unread count
    queryChannels([this](Outcome<std::vector<Channel>> result) {
        if (result.isError())
            return;

        int totalUnread = 0;
        for (const auto& channel : result.getValue())
        {
            totalUnread += channel.unreadCount;
        }

        if (unreadCountCallback)
        {
            juce::MessageManager::callAsync([this, totalUnread]() {
                if (unreadCountCallback)
                    unreadCountCallback(totalUnread);
            });
        }
    });
}

void StreamChatClient::sendTypingIndicator(const juce::String& channelType, const juce::String& channelId, bool isTyping)
{
    if (!isAuthenticated())
        return;

    // Send typing event via REST API
    Async::runVoid([this, channelType, channelId, isTyping]() {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/event";

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto* obj = requestData.getDynamicObject();

        juce::var eventData = juce::var(new juce::DynamicObject());
        auto* eventObj = eventData.getDynamicObject();
        eventObj->setProperty("type", isTyping ? "typing.start" : "typing.stop");

        obj->setProperty("event", eventData);

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        if (response.isVoid())
        {
            Log::warn("StreamChatClient: Failed to send typing indicator");
        }
    });
}

void StreamChatClient::handleWebSocketMessage(const juce::String& message)
{
    // Parse JSON message
    auto event = juce::JSON::parse(message);
    if (event.isObject())
    {
        parseWebSocketEvent(event);
    }
}

void StreamChatClient::parseWebSocketEvent(const juce::var& event)
{
    auto eventType = event.getProperty("type", "").toString();

    if (eventType == "message.new")
    {
        auto messageData = event.getProperty("message", juce::var());
        if (messageData.isObject() && messageReceivedCallback)
        {
            auto message = parseMessage(messageData);
            auto channelId = event.getProperty("channel_id", "").toString();
            juce::MessageManager::callAsync([this, message, channelId]() {
                if (messageReceivedCallback)
                    messageReceivedCallback(message, channelId);
            });
        }
    }
    else if (eventType == "typing.start" || eventType == "typing.stop")
    {
        auto userData = event.getProperty("user", juce::var());
        if (userData.isObject() && typingCallback)
        {
            auto userId = userData.getProperty("id", "").toString();
            bool isTyping = eventType == "typing.start";
            juce::MessageManager::callAsync([this, userId, isTyping]() {
                if (typingCallback)
                    typingCallback(userId, isTyping);
            });
        }
    }
    else if (eventType == "user.presence.changed")
    {
        auto userData = event.getProperty("user", juce::var());
        if (userData.isObject() && presenceChangedCallback)
        {
            auto presence = parsePresence(userData);
            juce::MessageManager::callAsync([this, presence]() {
                if (presenceChangedCallback)
                    presenceChangedCallback(presence);
            });
        }
    }
}

//==============================================================================
void StreamChatClient::uploadAudioSnippet(const juce::AudioBuffer<float>& audioBuffer, double sampleRate,
                                         AudioSnippetCallback callback)
{
    if (!isAuthenticated() || backendAuthToken.isEmpty())
    {
        if (callback) callback(Outcome<AudioSnippetResult>::error("Not authenticated"));
        return;
    }

    // Validate duration (max 30 seconds for snippets)
    double durationSecs = static_cast<double>(audioBuffer.getNumSamples()) / sampleRate;
    if (durationSecs > 30.0)
    {
        Log::warn("Audio snippet too long: " + juce::String(durationSecs) + "s (max 30s)");
        if (callback) callback(Outcome<AudioSnippetResult>::error("Audio snippet too long (max 30s)"));
        return;
    }

    // Note: This function does network I/O inside the work function, which is fine
    // The callback from uploadMultipartAbsolute is already on the message thread
    Async::runVoid(
        [this, audioBuffer, sampleRate, durationSecs, callback]() {
            // Encode audio to WAV
            juce::MemoryOutputStream outputStream;
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                &outputStream, sampleRate, audioBuffer.getNumChannels(), 16, {}, 0));

            if (writer == nullptr)
            {
                Log::error("Failed to create WAV writer");
                juce::MessageManager::callAsync([callback]() {
                    if (callback) callback(Outcome<AudioSnippetResult>::error("Failed to create WAV writer"));
                });
                return;
            }

            writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples());
            writer.reset(); // Flush

            juce::MemoryBlock audioData = outputStream.getMemoryBlock();

            if (networkClient == nullptr)
            {
                Log::warn("StreamChatClient: NetworkClient not set");
                juce::MessageManager::callAsync([callback]() {
                    if (callback) callback(Outcome<AudioSnippetResult>::error("Failed to create WAV writer"));
                });
                return;
            }

            // Build metadata fields
            std::map<juce::String, juce::String> metadata;
            metadata["bpm"] = "120";
            metadata["duration_seconds"] = juce::String(durationSecs, 2);
            metadata["sample_rate"] = juce::String(static_cast<int>(sampleRate));

            // Add auth header
            juce::StringPairArray headers;
            headers.set("Authorization", "Bearer " + backendAuthToken);

            // Use NetworkClient's public multipart upload method
            // Note: uploadMultipartAbsolute's callback is already on message thread
            networkClient->uploadMultipartAbsolute(
                config.backendBaseUrl + "/api/v1/audio/upload",
                "audio",
                audioData,
                "snippet.wav",
                "audio/wav",
                metadata,
                [callback, durationSecs](Outcome<juce::var> responseOutcome) {
                    if (responseOutcome.isOk())
                    {
                        auto response = responseOutcome.getValue();
                        if (response.isObject())
                        {
                            auto audioUrl = response.getProperty("audio_url", "").toString();
                            if (audioUrl.isEmpty())
                                audioUrl = response.getProperty("url", "").toString();

                            if (!audioUrl.isEmpty())
                            {
                                AudioSnippetResult result;
                                result.audioUrl = audioUrl;
                                result.duration = durationSecs;
                                if (callback) callback(Outcome<AudioSnippetResult>::ok(result));
                                return;
                            }
                        }
                    }

                    if (callback) callback(Outcome<AudioSnippetResult>::error(responseOutcome.isError() ? responseOutcome.getError() : "Failed to upload audio snippet"));
                },
                headers
            );
        }
    );
}

void StreamChatClient::sendMessageWithAudio(const juce::String& channelType, const juce::String& channelId,
                                           const juce::String& text, const juce::AudioBuffer<float>& audioBuffer,
                                           double sampleRate, MessageCallback callback)
{
    // First upload the audio snippet
    uploadAudioSnippet(audioBuffer, sampleRate, [this, channelType, channelId, text, callback]
                      (Outcome<AudioSnippetResult> uploadResult) {
        if (uploadResult.isError() || uploadResult.getValue().audioUrl.isEmpty())
        {
            if (callback) callback(Outcome<Message>::error(uploadResult.isError() ? uploadResult.getError() : "Audio upload failed"));
            return;
        }

        auto audioResult = uploadResult.getValue();
        // Then send message with audio URL in extra_data
        juce::var extraData = juce::var(new juce::DynamicObject());
        auto* obj = extraData.getDynamicObject();
        obj->setProperty("audio_url", audioResult.audioUrl);
        obj->setProperty("audio_duration", audioResult.duration);

        sendMessage(channelType, channelId, text, extraData, callback);
    });
}
