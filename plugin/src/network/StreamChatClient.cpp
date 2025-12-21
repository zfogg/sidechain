#include "StreamChatClient.h"
#include "../util/Async.h"
#include "../util/Log.h"
#include "../util/OSNotification.h"
#include "../util/Result.h"
#include <JuceHeader.h>
#include <chrono>
#include <thread>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>

// ==============================================================================
StreamChatClient::StreamChatClient(NetworkClient *client, const Config &cfg) : networkClient(client), config(cfg) {
  Log::info("StreamChatClient initialized");
}

StreamChatClient::~StreamChatClient() {
  disconnectWebSocket();
}

// ==============================================================================
void StreamChatClient::fetchToken(const juce::String &backendAuthTokenParam, TokenCallback callback) {
  this->backendAuthToken = backendAuthTokenParam;

  if (networkClient == nullptr) {
    Log::warn("StreamChatClient: NetworkClient not set");
    if (callback)
      callback(Outcome<TokenResult>::error("NetworkClient not set"));
    return;
  }

  juce::StringPairArray headers;
  headers.set("Authorization", "Bearer " + backendAuthToken);

  networkClient->getAbsolute(
      config.backendBaseUrl + "/api/v1/auth/stream-token",
      [this, callback](Outcome<juce::var> responseOutcome) {
        if (responseOutcome.isOk()) {
          auto response = responseOutcome.getValue();
          if (response.isObject()) {
            auto authToken = response.getProperty("token", "").toString();
            auto streamApiKey = response.getProperty("api_key", "").toString();
            auto streamUserId = response.getProperty("user_id", "").toString();

            if (!authToken.isEmpty() && !streamApiKey.isEmpty() && !streamUserId.isEmpty()) {
              setToken(authToken, streamApiKey, streamUserId);
              TokenResult result;
              result.token = authToken;
              result.apiKey = streamApiKey;
              result.userId = streamUserId;
              if (callback)
                callback(Outcome<TokenResult>::ok(result));
              return;
            }
          }
        }

        Log::error("Failed to parse stream token response");
        if (callback)
          callback(Outcome<TokenResult>::error(responseOutcome.isError() ? responseOutcome.getError()
                                                                         : "Invalid token response"));
      },
      headers);
}

void StreamChatClient::setToken(const juce::String &token, const juce::String &key, const juce::String &userId) {
  chatToken = token;
  apiKey = key;
  currentUserId = userId;

  // Build WebSocket URL:
  // wss://chat.stream-io-api.com/?api_key={key}&authorization={token}&user_id={userId}
  wsUrl = "wss:// chat.stream-io-api.com/?api_key=" + apiKey + "&authorization=" + token + "&user_id=" + userId;

  Log::info("StreamChatClient token set for user: " + userId + ", API key configured");
}

// ==============================================================================
juce::String StreamChatClient::buildAuthHeaders() const {
  juce::String headers = "Stream-Auth-Type: jwt\r\n";
  headers += "Authorization: " + chatToken + "\r\n";
  headers += "Content-Type: application/json\r\n";
  return headers;
}

juce::var StreamChatClient::makeStreamRequest(const juce::String &endpoint, const juce::String &method,
                                              const juce::var &data) {
  if (!isAuthenticated()) {
    Log::warn("StreamChatClient: Not authenticated, cannot make request");
    return juce::var();
  }

  // Debug authentication
  Log::debug("StreamChatClient: Auth check - apiKey: " + juce::String(apiKey.isEmpty() ? "EMPTY" : "SET") +
             ", chatToken: " + juce::String(chatToken.isEmpty() ? "EMPTY" : "SET") +
             ", currentUserId: " + currentUserId);

  // Build URL with API key - handle if endpoint already has query parameters
  juce::String baseUrl = getStreamBaseUrl() + endpoint;
  juce::String fullUrl = baseUrl.contains("?") ? baseUrl + "&api_key=" + apiKey : baseUrl + "?api_key=" + apiKey;
  juce::URL url(fullUrl);

  juce::String headers = buildAuthHeaders();

  juce::StringPairArray responseHeaders;
  auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                     .withExtraHeaders(headers)
                     .withConnectionTimeoutMs(config.timeoutMs)
                     .withResponseHeaders(&responseHeaders);

  // For POST/PUT/DELETE, include request body
  if (method == "POST" || method == "PUT" || method == "DELETE") {
    if (!data.isVoid()) {
      juce::String jsonString = juce::JSON::toString(data, true);
      Log::debug("StreamChatClient: " + method + " data: " + jsonString);

      // Use MemoryBlock like NetworkClient does
      juce::MemoryBlock jsonBody(jsonString.toRawUTF8(), jsonString.getNumBytesAsUTF8());
      url = url.withPOSTData(jsonBody);
    } else if (method == "POST") {
      url = url.withPOSTData(juce::MemoryBlock());
    }
  }

  Log::debug("StreamChatClient: Full URL: " + fullUrl);
  Log::debug("StreamChatClient: Making " + method + " request");
  Log::debug("StreamChatClient: Headers: " + headers);

  auto stream = url.createInputStream(options);

  if (stream == nullptr) {
    Log::error("StreamChatClient: Request failed - " + endpoint);
    return juce::var();
  }

  auto response = stream->readEntireStreamAsString();
  Log::debug("StreamChatClient: Response: " + response);
  return juce::JSON::parse(response);
}

// ==============================================================================
void StreamChatClient::createDirectChannel(const juce::String &targetUserId,
                                           std::function<void(Outcome<Channel>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<Channel>::error("Not authenticated"));
    return;
  }

  Async::run<Channel>(
      [this, targetUserId]() -> Channel {
        juce::String channelId = generateDirectChannelId(currentUserId, targetUserId);

        // Use POST /channels/messaging/{id}/query endpoint to create/get
        // channel
        juce::String endpoint = "/channels/messaging/" + channelId + "/query";

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        // Build members as an OBJECT (not array): {"user_id": {}, "user_id":
        // {}} Stream.io API expects members field to be an object
        juce::var members = juce::var(new juce::DynamicObject());
        members.getDynamicObject()->setProperty(currentUserId, juce::var(new juce::DynamicObject()));
        members.getDynamicObject()->setProperty(targetUserId, juce::var(new juce::DynamicObject()));

        obj->setProperty("members", members);
        obj->setProperty("created_by_id", currentUserId);

        Log::debug("StreamChatClient: Creating direct channel with " + targetUserId + ", channel ID: " + channelId);
        Log::debug("StreamChatClient: Request data - " + juce::JSON::toString(requestData));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient: Create channel response - " + juce::JSON::toString(response));

        // Response from stream.io has channel wrapped in "channel" property
        juce::var channelData = response;
        if (response.isObject() && response.hasProperty("channel")) {
          channelData = response.getProperty("channel", juce::var());
        }

        if (channelData.isObject() && channelData.hasProperty("id")) {
          Log::debug("StreamChatClient: Direct channel created: " + channelData.getProperty("id", "").toString());
          return parseChannel(channelData);
        }

        Log::error("StreamChatClient: Failed to create direct channel. Response: " + juce::JSON::toString(response));
        return Channel{};
      },
      [callback](const Channel &channel) {
        if (callback) {
          if (!channel.id.isEmpty())
            callback(Outcome<Channel>::ok(channel));
          else
            callback(Outcome<Channel>::error("Failed to create channel"));
        }
      });
}

void StreamChatClient::createGroupChannel(const juce::String &channelId, const juce::String &name,
                                          const std::vector<juce::String> &memberIds,
                                          std::function<void(Outcome<Channel>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<Channel>::error("Not authenticated"));
    return;
  }

  Async::run<Channel>(
      [this, channelId, name, memberIds]() -> Channel {
        // Use POST /channels/team/{id}/query endpoint to create/get team
        // channel
        juce::String endpoint = "/channels/team/" + channelId + "/query";

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        // Build members as an OBJECT (not array): {"user_id": {}, "user_id":
        // {}} Stream.io API expects members field to be an object
        juce::var members = juce::var(new juce::DynamicObject());
        for (const auto &memberId : memberIds) {
          members.getDynamicObject()->setProperty(memberId, juce::var(new juce::DynamicObject()));
        }
        obj->setProperty("members", members);

        // Add channel data with name
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("name", name);
        obj->setProperty("data", data);

        obj->setProperty("created_by_id", currentUserId);

        Log::debug("StreamChatClient: Creating group channel " + channelId + " with " + juce::String(memberIds.size()) +
                   " members");
        Log::debug("StreamChatClient: Request data - " + juce::JSON::toString(requestData));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient: Create group channel response - " + juce::JSON::toString(response));

        // Response from stream.io has channel wrapped in "channel" property
        juce::var channelData = response;
        if (response.isObject() && response.hasProperty("channel")) {
          channelData = response.getProperty("channel", juce::var());
        }

        if (channelData.isObject() && channelData.hasProperty("id")) {
          Log::debug("StreamChatClient: Group channel created: " + channelData.getProperty("id", "").toString());
          return parseChannel(channelData);
        }

        Log::error("StreamChatClient: Failed to create group channel. Response: " + juce::JSON::toString(response));
        return Channel{};
      },
      [callback](const Channel &channel) {
        if (callback) {
          if (!channel.id.isEmpty())
            callback(Outcome<Channel>::ok(channel));
          else
            callback(Outcome<Channel>::error("Failed to create channel"));
        }
      });
}

void StreamChatClient::queryChannels(ChannelsCallback callback, int limit, int offset) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<std::vector<Channel>>::error("Not authenticated"));
    return;
  }

  Async::run<std::vector<Channel>>(
      [this, limit, offset]() -> std::vector<Channel> {
        juce::String endpoint = "/channels";

        // Build request body with filter, sort, limit, offset
        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        // Filter channels - query for channels where current user is a member
        // Format per getstream.io docs: {members: {$in: [userID]}}
        juce::var filter = juce::var(new juce::DynamicObject());
        auto *filterObj = filter.getDynamicObject();

        // Create the members.$in array
        juce::var membersFilter = juce::var(new juce::DynamicObject());
        juce::var userIds = juce::var(juce::Array<juce::var>());
        userIds.getArray()->add(currentUserId);
        membersFilter.getDynamicObject()->setProperty("$in", userIds);

        // Set members filter
        filterObj->setProperty("members", membersFilter);
        obj->setProperty("filter_conditions", filter);

        // Set state to true to get full channel data (required for queryChannels to work)
        obj->setProperty("state", true);

        // Build sort
        juce::var sort = juce::var(juce::Array<juce::var>());
        juce::var sortItem = juce::var(new juce::DynamicObject());
        sortItem.getDynamicObject()->setProperty("field", "last_message_at");
        sortItem.getDynamicObject()->setProperty("direction", -1);
        sort.getArray()->add(sortItem);
        obj->setProperty("sort", sort);

        obj->setProperty("limit", limit);
        obj->setProperty("offset", offset);

        Log::debug("StreamChatClient: Querying channels with filter - " + juce::JSON::toString(requestData));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient: queryChannels response - " + juce::JSON::toString(response));

        std::vector<Channel> channels;

        if (response.isObject()) {
          auto channelsArray = response.getProperty("channels", juce::var());
          if (channelsArray.isArray()) {
            Log::debug("StreamChatClient: Found channels array with " + juce::String(channelsArray.getArray()->size()) +
                       " channels");
            // Note: Stream.io queryChannels doesn't return member data in the
            // list response, so we can't filter by members. Just return all
            // channels - the API already restricts access to channels the user
            // has permission to view.
            for (int i = 0; i < channelsArray.getArray()->size(); i++) {
              auto channelData = channelsArray.getArray()->getReference(i);
              channels.push_back(parseChannel(channelData));
            }
          } else {
            Log::debug("StreamChatClient: No 'channels' property in response "
                       "or not an array");
          }
        } else {
          Log::debug("StreamChatClient: Response is not an object");
        }

        Log::debug("StreamChatClient: queryChannels returning " + juce::String(channels.size()) + " channels");
        return channels;
      },
      [callback](const std::vector<Channel> &channels) {
        if (callback) {
          callback(Outcome<std::vector<Channel>>::ok(channels));
        }
      });
}

// ==============================================================================
void StreamChatClient::sendMessage(const juce::String &channelType, const juce::String &channelId,
                                   const juce::String &text, const juce::var &extraData, MessageCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<Message>::error("Not authenticated"));
    return;
  }

  Async::run<Message>(
      [this, channelType, channelId, text, extraData]() -> Message {
        // Use "messaging" as default channel type if empty
        juce::String actualChannelType = channelType.isEmpty() ? "messaging" : channelType;
        juce::String endpoint = "/channels/" + actualChannelType + "/" + channelId + "/message";

        Log::debug("StreamChatClient::sendMessage - endpoint: " + endpoint);
        Log::debug("StreamChatClient::sendMessage - text: " + text);
        Log::debug("StreamChatClient::sendMessage - channelType: " + actualChannelType);

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        juce::var message = juce::var(new juce::DynamicObject());
        auto *msgObj = message.getDynamicObject();

        // Message type is required by stream.io - use "regular" for user
        // messages
        msgObj->setProperty("type", "regular");
        msgObj->setProperty("text", text);

        if (!extraData.isVoid()) {
          msgObj->setProperty("extra_data", extraData);
        }

        obj->setProperty("message", message);

        Log::debug("StreamChatClient::sendMessage - request data: " + juce::JSON::toString(requestData));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient::sendMessage - response: " + juce::JSON::toString(response));

        if (response.isObject()) {
          auto messageData = response.getProperty("message", juce::var());
          if (messageData.isObject()) {
            Log::debug("StreamChatClient::sendMessage - message sent "
                       "successfully with ID: " +
                       messageData.getProperty("id", "").toString());
            return parseMessage(messageData);
          }
        }

        Log::error("StreamChatClient::sendMessage - failed to parse response");
        return Message{};
      },
      [callback](const Message &msg) {
        if (callback) {
          if (!msg.id.isEmpty()) {
            Log::info("StreamChatClient::sendMessage - callback with success");
            callback(Outcome<Message>::ok(msg));
          } else {
            Log::error("StreamChatClient::sendMessage - callback with error");
            callback(Outcome<Message>::error("Failed to send message"));
          }
        }
      });
}

void StreamChatClient::queryMessages(const juce::String &channelType, const juce::String &channelId, int limit,
                                     int offset, MessagesCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<std::vector<Message>>::error("Not authenticated"));
    return;
  }

  Async::run<std::vector<Message>>(
      [this, channelType, channelId, limit, offset]() -> std::vector<Message> {
        // Use "messaging" as default channel type if empty
        juce::String actualChannelType = channelType.isEmpty() ? "messaging" : channelType;
        juce::String endpoint = "/channels/" + actualChannelType + "/" + channelId + "/query";

        // Build request body with message query parameters
        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        // Build the messages pagination parameters
        // According to getstream.io API, messages should be a nested object with limit and offset
        juce::var messagesObj = juce::var(new juce::DynamicObject());
        messagesObj.getDynamicObject()->setProperty("limit", limit);
        messagesObj.getDynamicObject()->setProperty("offset", offset);
        obj->setProperty("messages", messagesObj);

        // Set state to true to get full response with messages
        // Note: watch requires active WebSocket connection, so we don't set it here
        obj->setProperty("state", true);

        Log::debug("StreamChatClient::queryMessages - endpoint: " + endpoint);
        Log::debug("StreamChatClient::queryMessages - request: " + juce::JSON::toString(requestData));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient::queryMessages - response: " + juce::JSON::toString(response));

        std::vector<Message> messages;

        // Parse response using proper JSON object access instead of string manipulation
        if (response.isObject()) {
          auto messagesVar = response["messages"];

          if (!messagesVar.isVoid() && messagesVar.isArray()) {
            auto messagesArray = messagesVar.getArray();
            Log::debug("StreamChatClient::queryMessages - found " + juce::String(messagesArray->size()) + " messages");

            for (int i = 0; i < messagesArray->size(); i++) {
              auto messageData = messagesArray->getReference(i);
              if (messageData.isObject()) {
                messages.push_back(parseMessage(messageData));
              } else {
                Log::warn("StreamChatClient::queryMessages - message at index " + juce::String(i) +
                          " is not an object");
              }
            }
          } else if (messagesVar.isVoid()) {
            Log::debug("StreamChatClient::queryMessages - 'messages' field not found in response");
          } else {
            Log::warn("StreamChatClient::queryMessages - 'messages' field exists but is not an array");
          }
        } else {
          Log::error("StreamChatClient::queryMessages - response is not a JSON object");
        }

        return messages;
      },
      [callback](const std::vector<Message> &messages) {
        if (callback) {
          callback(Outcome<std::vector<Message>>::ok(messages));
        }
      });
}

// ==============================================================================
void StreamChatClient::searchMessages(const juce::String &query, const juce::var &channelFilters, int limit, int offset,
                                      MessagesCallback callback) {
  if (!isAuthenticated() || query.isEmpty()) {
    if (callback)
      callback(Outcome<std::vector<Message>>::error("Not authenticated or query is empty"));
    return;
  }

  Async::run<std::vector<Message>>(
      [this, query, channelFilters, limit, offset]() -> std::vector<Message> {
        juce::String endpoint = "/search";

        // Build query parameters
        endpoint += "?query=" + juce::URL::addEscapeChars(query, true);
        endpoint += "&limit=" + juce::String(limit);
        endpoint += "&offset=" + juce::String(offset);

        if (!channelFilters.isVoid()) {
          endpoint += "&filter_conditions=" + juce::URL::addEscapeChars(juce::JSON::toString(channelFilters), true);
        }

        auto response = makeStreamRequest(endpoint, "GET", juce::var());

        std::vector<Message> messages;

        if (response.isObject()) {
          auto results = response.getProperty("results", juce::var());
          if (results.isArray()) {
            for (int i = 0; i < results.getArray()->size(); i++) {
              auto messageData = results.getArray()->getReference(i);
              messages.push_back(parseMessage(messageData));
            }
          }
        }

        return messages;
      },
      [callback](const std::vector<Message> &messages) {
        if (callback) {
          callback(Outcome<std::vector<Message>>::ok(messages));
        }
      });
}

// ==============================================================================
void StreamChatClient::queryPresence(const std::vector<juce::String> &userIds, PresenceCallback callback) {
  if (!isAuthenticated() || userIds.empty()) {
    if (callback)
      callback(Outcome<std::vector<UserPresence>>::error("Not authenticated or empty user list"));
    return;
  }

  Async::run<std::vector<UserPresence>>(
      [this, userIds]() -> std::vector<UserPresence> {
        juce::String endpoint = "/users";

        // Build filter: {"id": {"$in": [userIds]}}
        juce::var filter = juce::var(new juce::DynamicObject());
        auto *filterObj = filter.getDynamicObject();
        juce::var inArray = juce::var(juce::Array<juce::var>());
        for (const auto &userId : userIds) {
          inArray.getArray()->add(juce::var(userId));
        }
        juce::var inObj = juce::var(new juce::DynamicObject());
        inObj.getDynamicObject()->setProperty("$in", inArray);
        filterObj->setProperty("id", inObj);

        endpoint += "?filter=" + juce::URL::addEscapeChars(juce::JSON::toString(filter, true), true);
        endpoint += "&presence=true";

        auto response = makeStreamRequest(endpoint, "GET", juce::var());

        std::vector<UserPresence> presenceList;

        if (response.isObject()) {
          auto usersArray = response.getProperty("users", juce::var());
          if (usersArray.isArray()) {
            for (int i = 0; i < usersArray.getArray()->size(); i++) {
              auto userData = usersArray.getArray()->getReference(i);
              presenceList.push_back(parsePresence(userData));
            }
          }
        }

        return presenceList;
      },
      [callback](const std::vector<UserPresence> &presenceList) {
        if (callback) {
          callback(Outcome<std::vector<UserPresence>>::ok(presenceList));
        }
      });
}

// ==============================================================================
void StreamChatClient::connectWebSocket() {
  if (!isAuthenticated() || wsUrl.isEmpty()) {
    Log::warn("StreamChatClient: Cannot connect WebSocket - not authenticated");
    return;
  }

  // Atomically set connection active to prevent race condition where multiple threads
  // could pass the check below and both attempt to connect
  bool alreadyConnecting = wsConnectionActive.exchange(true);
  if (wsConnected.load() || alreadyConnecting) {
    Log::debug("StreamChatClient: WebSocket already connected or connecting");
    return;
  }

  updateConnectionStatus(ConnectionStatus::Connecting);

  // Clean up any existing connection
  cleanupWebSocket();

  try {
    // Create websocketpp client
    wsClient = std::make_unique<wspp_client>();

    // Initialize ASIO
    wsClient->init_asio();

    // Set up event handlers
    wsClient->set_open_handler([this](connection_hdl hdl) { onWsOpen(hdl); });

    wsClient->set_close_handler([this](connection_hdl hdl) { onWsClose(hdl); });

    wsClient->set_message_handler([this](connection_hdl hdl, message_ptr msg) { onWsMessage(hdl, msg); });

    wsClient->set_fail_handler([this](connection_hdl hdl) { onWsFail(hdl); });

    // Build WebSocket URI (wsUrl already contains full URL with query params)
    juce::String uri = wsUrl;
    Log::info("StreamChatClient: Connecting to getstream.io WebSocket: " + uri);

    // Create connection
    websocketpp::lib::error_code ec;
    connection_ptr con = wsClient->get_connection(uri.toStdString(), ec);

    if (ec) {
      Log::error("StreamChatClient: WebSocket connection error - " + juce::String(ec.message().c_str()));
      updateConnectionStatus(ConnectionStatus::Disconnected);
      cleanupWebSocket();
      return;
    }

    wsConnection = con->get_handle();

    // Start ASIO event loop in background
    wsClient->start_perpetual();

    // Connect (non-blocking)
    wsClient->connect(con);

    // Run ASIO event loop in a separate thread
    wsAsioThread = std::make_unique<std::thread>([this]() {
      try {
        wsClient->run();
      } catch (const std::exception &e) {
        Log::error("StreamChatClient: ASIO thread exception - " + juce::String(e.what()));
      }
    });

    Log::info("StreamChatClient: WebSocket connection initiated");
  } catch (const std::exception &e) {
    Log::error("StreamChatClient: Exception connecting WebSocket - " + juce::String(e.what()));
    updateConnectionStatus(ConnectionStatus::Disconnected);
    wsConnectionActive.store(false); // Reset flag since connection failed
    cleanupWebSocket();
  }
}

void StreamChatClient::disconnectWebSocket() {
  wsConnected.store(false);
  cleanupWebSocket();
  updateConnectionStatus(ConnectionStatus::Disconnected);
}

void StreamChatClient::cleanupWebSocket() {
  wsConnectionActive.store(false);

  // Stop channel polling timer
  if (channelPollTimer) {
    channelPollTimer->stopTimer();
    channelPollTimer.reset();
  }

  if (wsClient) {
    try {
      // Stop ASIO event loop
      wsClient->stop_perpetual();

      // Close connection if open
      if (wsConnected.load()) {
        try {
          connection_ptr con = wsClient->get_con_from_hdl(wsConnection);
          if (con && con->get_state() == websocketpp::session::state::open) {
            wsClient->close(wsConnection, websocketpp::close::status::going_away, "Client disconnect");
          }
        } catch (const std::exception &e) {
          Log::debug("StreamChatClient: Error closing WebSocket - " + juce::String(e.what()));
        } catch (...) {
          Log::debug("StreamChatClient: Unknown error closing WebSocket");
        }
      }

      wsClient->stop();
    } catch (const std::exception &e) {
      Log::debug("StreamChatClient: Error stopping WebSocket client - " + juce::String(e.what()));
    }

    // Wait for ASIO thread to finish (with timeout, then detach)
    if (wsAsioThread && wsAsioThread->joinable()) {
      // Give the thread time to respond to stop
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      if (wsAsioThread->joinable()) {
        // Detach instead of blocking forever - the thread will exit
        // on its own once the ASIO io_context is stopped
        wsAsioThread->detach();
      }
      wsAsioThread.reset();
    }

    wsClient.reset();
  }
}

// ==============================================================================
// WebSocket event handlers
void StreamChatClient::onWsOpen(connection_hdl /* hdl */) {
  wsConnected.store(true);
  wsConnectionActive.store(false); // Connection attempt complete
  updateConnectionStatus(ConnectionStatus::Connected);
  Log::info("StreamChatClient: WebSocket connected to getstream.io");
}

void StreamChatClient::onWsClose(connection_hdl /* hdl */) {
  wsConnected.store(false);
  wsConnectionActive.store(false);
  updateConnectionStatus(ConnectionStatus::Disconnected);
  Log::info("StreamChatClient: WebSocket disconnected from getstream.io");
}

void StreamChatClient::onWsMessage(connection_hdl /* hdl */, message_ptr msg) {
  try {
    std::string payload = msg->get_payload();
    juce::String messageText(payload.c_str());

    Log::debug("StreamChatClient: WebSocket message received: " + messageText.substring(0, 100));

    // Parse and handle the message
    handleWebSocketMessage(messageText);
  } catch (const std::exception &e) {
    Log::error("StreamChatClient: Error processing WebSocket message - " + juce::String(e.what()));
  }
}

void StreamChatClient::onWsFail(connection_hdl hdl) {
  wsConnected.store(false);
  wsConnectionActive.store(false);
  updateConnectionStatus(ConnectionStatus::Disconnected);

  try {
    connection_ptr con = wsClient->get_con_from_hdl(hdl);
    if (con) {
      auto ec = con->get_ec();
      juce::String errorMsg = ec ? juce::String(ec.message().c_str()) : "Unknown error";
      Log::error("StreamChatClient: WebSocket connection failed - " + errorMsg);
    }
  } catch (...) {
    Log::error("StreamChatClient: WebSocket connection failed");
  }
}

// ==============================================================================
juce::String StreamChatClient::generateDirectChannelId(const juce::String &userId1, const juce::String &userId2) {
  // Sort user IDs to ensure consistent channel ID
  juce::StringArray ids;
  ids.add(userId1);
  ids.add(userId2);
  ids.sort(true);

  // Shorten the combined ID to stay under 64 character limit for stream.io
  // Take first 30 chars from each user ID, separated by underscore
  juce::String id1 = ids[0].substring(0, 30);
  juce::String id2 = ids[1].substring(0, 30);
  juce::String channelId = id1 + "_" + id2;

  Log::debug("StreamChatClient: Generated channel ID '" + channelId + "' (length: " + juce::String(channelId.length()) +
             ") for users " + userId1 + " and " + userId2);

  return channelId;
}

// ==============================================================================
StreamChatClient::Channel StreamChatClient::parseChannel(const juce::var &channelData) {
  Channel channel;

  if (channelData.isObject()) {
    channel.id = channelData.getProperty("id", "").toString();
    channel.type = channelData.getProperty("type", "").toString();
    channel.members = channelData.getProperty("members", juce::var());

    auto data = channelData.getProperty("data", juce::var());
    if (data.isObject()) {
      channel.name = data.getProperty("name", "").toString();
      channel.extraData = data;
    }

    channel.lastMessage = channelData.getProperty("last_message", juce::var());
    channel.unreadCount = channelData.getProperty("unread_count", 0).operator int();
    channel.lastMessageAt = channelData.getProperty("last_message_at", "").toString();
  }

  return channel;
}

StreamChatClient::Message StreamChatClient::parseMessage(const juce::var &messageData) {
  Message message;

  if (messageData.isObject()) {
    message.id = messageData.getProperty("id", "").toString();
    message.text = messageData.getProperty("text", "").toString();
    message.createdAt = messageData.getProperty("created_at", "").toString();
    message.reactions = messageData.getProperty("reactions", juce::var());
    message.extraData = messageData.getProperty("extra_data", juce::var());
    message.isDeleted = messageData.getProperty("deleted_at", juce::var()).isString();

    auto user = messageData.getProperty("user", juce::var());
    if (user.isObject()) {
      message.userId = user.getProperty("id", "").toString();
      message.userName = user.getProperty("name", "").toString();
    }
  }

  return message;
}

StreamChatClient::UserPresence StreamChatClient::parsePresence(const juce::var &userData) {
  UserPresence presence;

  if (userData.isObject()) {
    presence.userId = userData.getProperty("id", "").toString();
    presence.online = userData.getProperty("online", false).operator bool();
    presence.lastActive = userData.getProperty("last_active", "").toString();
    presence.status = userData.getProperty("status", "").toString();
  }

  return presence;
}

// ==============================================================================
void StreamChatClient::updateConnectionStatus(ConnectionStatus status) {
  auto previousStatus = connectionStatus.exchange(status);
  if (previousStatus != status && connectionStatusCallback) {
    juce::MessageManager::callAsync([this, status]() {
      if (connectionStatusCallback)
        connectionStatusCallback(status);
    });
  }
}

// ==============================================================================
void StreamChatClient::getChannel(const juce::String &channelType, const juce::String &channelId,
                                  std::function<void(Outcome<Channel>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<Channel>::error("Not authenticated"));
    return;
  }

  Async::run<Channel>(
      [this, channelType, channelId]() -> Channel {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId;

        auto response = makeStreamRequest(endpoint, "GET", juce::var());

        if (response.isObject()) {
          auto channelData = response.getProperty("channel", juce::var());
          if (channelData.isObject()) {
            return parseChannel(channelData);
          }
        }

        return Channel{};
      },
      [callback](const Channel &channel) {
        if (callback) {
          if (!channel.id.isEmpty())
            callback(Outcome<Channel>::ok(channel));
          else
            callback(Outcome<Channel>::error("Failed to create channel"));
        }
      });
}

void StreamChatClient::deleteChannel(const juce::String &channelType, const juce::String &channelId,
                                     std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, channelType, channelId]() -> bool {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId;

        auto response = makeStreamRequest(endpoint, "DELETE", juce::var());

        return response.isObject() && !response.getProperty("channel", juce::var()).isVoid();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

void StreamChatClient::leaveChannel(const juce::String &channelType, const juce::String &channelId,
                                    std::function<void(Outcome<void>)> callback) {
  // Leave channel is just removing the current user
  removeMembers(channelType, channelId, {currentUserId}, callback);
}

void StreamChatClient::addMembers(const juce::String &channelType, const juce::String &channelId,
                                  const std::vector<juce::String> &memberIds,
                                  std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, channelType, channelId, memberIds]() -> bool {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/add_members";

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();
        juce::var members = juce::var(juce::Array<juce::var>());
        for (const auto &memberId : memberIds) {
          members.getArray()->add(juce::var(memberId));
        }
        obj->setProperty("user_ids", members);

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.isObject() && !response.getProperty("channel", juce::var()).isVoid();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

void StreamChatClient::removeMembers(const juce::String &channelType, const juce::String &channelId,
                                     const std::vector<juce::String> &memberIds,
                                     std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, channelType, channelId, memberIds]() -> bool {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/remove_members";

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();
        juce::var members = juce::var(juce::Array<juce::var>());
        for (const auto &memberId : memberIds) {
          members.getArray()->add(juce::var(memberId));
        }
        obj->setProperty("user_ids", members);

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.isObject() && !response.getProperty("channel", juce::var()).isVoid();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

void StreamChatClient::updateChannel(const juce::String &channelType, const juce::String &channelId,
                                     const juce::String &name, const juce::var &extraData,
                                     std::function<void(Outcome<Channel>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<Channel>::error("Not authenticated"));
    return;
  }

  Async::run<Channel>(
      [this, channelType, channelId, name, extraData]() -> Channel {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId;

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        juce::var data = juce::var(new juce::DynamicObject());
        auto *dataObj = data.getDynamicObject();

        if (!name.isEmpty()) {
          dataObj->setProperty("name", name);
        }

        // Merge extra data if provided
        if (!extraData.isVoid() && extraData.isObject()) {
          auto *extraObj = extraData.getDynamicObject();
          auto props = extraObj->getProperties();
          for (int i = 0; i < props.size(); ++i) {
            auto propName = props.getName(i);
            if (propName != juce::Identifier("name")) // Don't override name if already set
            {
              dataObj->setProperty(propName, props.getValueAt(i));
            }
          }
        }

        obj->setProperty("data", data);

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        if (response.isObject()) {
          auto channelData = response.getProperty("channel", juce::var());
          if (channelData.isObject()) {
            return parseChannel(channelData);
          }
        }

        return Channel{};
      },
      [callback](const Channel &channel) {
        if (callback) {
          if (!channel.id.isEmpty())
            callback(Outcome<Channel>::ok(channel));
          else
            callback(Outcome<Channel>::error("Failed to update channel"));
        }
      });
}

void StreamChatClient::updateMessage(const juce::String &channelType, const juce::String &channelId,
                                     const juce::String &messageId, const juce::String &newText,
                                     MessageCallback callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<Message>::error("Not authenticated"));
    return;
  }

  Async::run<Message>(
      [this, channelType, channelId, messageId, newText]() -> Message {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message";

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        juce::var message = juce::var(new juce::DynamicObject());
        auto *msgObj = message.getDynamicObject();
        msgObj->setProperty("id", messageId);
        msgObj->setProperty("text", newText);

        obj->setProperty("message", message);

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        if (response.isObject()) {
          auto messageData = response.getProperty("message", juce::var());
          if (messageData.isObject()) {
            return parseMessage(messageData);
          }
        }

        return Message{};
      },
      [callback](const Message &msg) {
        if (callback) {
          if (!msg.id.isEmpty())
            callback(Outcome<Message>::ok(msg));
          else
            callback(Outcome<Message>::error("Failed to update message"));
        }
      });
}

void StreamChatClient::deleteMessage(const juce::String &channelType, const juce::String &channelId,
                                     const juce::String &messageId, std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, channelType, channelId, messageId]() -> bool {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message/" + messageId;

        auto response = makeStreamRequest(endpoint, "DELETE", juce::var());

        return response.isObject() && !response.getProperty("message", juce::var()).isVoid();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

void StreamChatClient::addReaction(const juce::String &channelType, const juce::String &channelId,
                                   const juce::String &messageId, const juce::String &reactionType,
                                   std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, channelType, channelId, messageId, reactionType]() -> bool {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/message/" + messageId + "/reaction";

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();

        juce::var reaction = juce::var(new juce::DynamicObject());
        reaction.getDynamicObject()->setProperty("type", reactionType);
        obj->setProperty("reaction", reaction);

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.isObject() && !response.getProperty("message", juce::var()).isVoid();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

void StreamChatClient::removeReaction(const juce::String &channelType, const juce::String &channelId,
                                      const juce::String &messageId, const juce::String &reactionType,
                                      std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, channelType, channelId, messageId, reactionType]() -> bool {
        juce::String endpoint =
            "/channels/" + channelType + "/" + channelId + "/message/" + messageId + "/reaction/" + reactionType;

        auto response = makeStreamRequest(endpoint, "DELETE", juce::var());

        return response.isObject() && !response.getProperty("message", juce::var()).isVoid();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

void StreamChatClient::markChannelRead(const juce::String &channelType, const juce::String &channelId,
                                       std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, channelType, channelId]() -> bool {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId + "/read";

        auto response = makeStreamRequest(endpoint, "POST", juce::var());

        return response.isObject();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

void StreamChatClient::updateStatus(const juce::String &status, const juce::var &extraData,
                                    std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, status, extraData]() -> bool {
        juce::String endpoint = "/users/" + currentUserId;

        juce::var requestData = juce::var(new juce::DynamicObject());
        auto *obj = requestData.getDynamicObject();
        obj->setProperty("status", status);

        if (!extraData.isVoid()) {
          obj->setProperty("extra_data", extraData);
        }

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.isObject() && !response.getProperty("user", juce::var()).isVoid();
      },
      [callback](bool success) {
        if (callback) {
          if (success)
            callback(Outcome<void>::ok());
          else
            callback(Outcome<void>::error("Operation failed"));
        }
      });
}

// ==============================================================================
// Channel Watching (Polling-based Real-time)

// Helper class for channel polling timer
class ChannelPollTimer : public juce::Timer {
public:
  ChannelPollTimer(std::function<void()> callback) : pollCallback(callback) {}
  void timerCallback() override {
    if (pollCallback)
      pollCallback();
  }

private:
  std::function<void()> pollCallback;
};

// Helper class for unread count polling timer
class UnreadPollTimer : public juce::Timer {
public:
  UnreadPollTimer(std::function<void()> callback) : pollCallback(callback) {}
  void timerCallback() override {
    if (pollCallback)
      pollCallback();
  }

private:
  std::function<void()> pollCallback;
};

void StreamChatClient::watchChannel(const juce::String &channelType, const juce::String &channelId) {
  Log::info("StreamChatClient: Watching channel " + channelType + "/" + channelId);

  watchedChannelType = channelType;
  watchedChannelId = channelId;
  lastSeenMessageId = "";

  // Stop existing timer if any
  if (channelPollTimer) {
    channelPollTimer->stopTimer();
  }

  // Create new polling timer - poll every 2 seconds for responsive messaging
  channelPollTimer = std::make_unique<ChannelPollTimer>([this]() { pollWatchedChannel(); });
  static_cast<ChannelPollTimer *>(channelPollTimer.get())->startTimer(2000);

  // Initial poll
  pollWatchedChannel();
}

void StreamChatClient::unwatchChannel() {
  Log::info("StreamChatClient: Unwatching channel");

  watchedChannelType = "";
  watchedChannelId = "";
  lastSeenMessageId = "";

  if (channelPollTimer) {
    channelPollTimer->stopTimer();
    channelPollTimer.reset();
  }
}

void StreamChatClient::pollWatchedChannel() {
  if (watchedChannelId.isEmpty() || !isAuthenticated())
    return;

  // Query messages to check for new ones
  queryMessages(watchedChannelType, watchedChannelId, 20, 0, [this](Outcome<std::vector<Message>> result) {
    if (result.isError() || result.getValue().empty())
      return;

    auto messages = result.getValue();

    // Sort by created_at to find the newest
    if (messages.empty())
      return;

    // The newest message should be at the end (messages are
    // usually returned oldest first)
    const auto &newestMessage = messages.back();

    // If we have a new message that we haven't seen
    if (!newestMessage.id.isEmpty() && newestMessage.id != lastSeenMessageId) {
      // If this is not our first poll (lastSeenMessageId was set)
      if (!lastSeenMessageId.isEmpty()) {
        // Find all new messages
        bool foundLastSeen = false;
        for (const auto &msg : messages) {
          if (msg.id == lastSeenMessageId) {
            foundLastSeen = true;
            continue;
          }

          if (foundLastSeen && messageReceivedCallback) {
            // Only notify for messages from other users
            if (msg.userId != currentUserId) {
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

void StreamChatClient::pollUnreadCount() {
  if (!isAuthenticated())
    return;

  // Query all channels to get total unread count
  queryChannels([this](Outcome<std::vector<Channel>> result) {
    if (result.isError())
      return;

    int totalUnread = 0;
    for (const auto &channel : result.getValue()) {
      totalUnread += channel.unreadCount;
    }

    if (unreadCountCallback) {
      juce::MessageManager::callAsync([this, totalUnread]() {
        if (unreadCountCallback)
          unreadCountCallback(totalUnread);
      });
    }
  });
}

void StreamChatClient::sendTypingIndicator(const juce::String &channelType, const juce::String &channelId,
                                           bool isTyping) {
  if (!isAuthenticated())
    return;

  // Send typing event via REST API
  Async::runVoid([this, channelType, channelId, isTyping]() {
    // Use "messaging" as default channel type if empty
    juce::String actualChannelType = channelType.isEmpty() ? "messaging" : channelType;
    juce::String endpoint = "/channels/" + actualChannelType + "/" + channelId + "/event";

    juce::var requestData = juce::var(new juce::DynamicObject());
    auto *obj = requestData.getDynamicObject();

    juce::var eventData = juce::var(new juce::DynamicObject());
    auto *eventObj = eventData.getDynamicObject();
    eventObj->setProperty("type", isTyping ? "typing.start" : "typing.stop");

    obj->setProperty("event", eventData);

    auto response = makeStreamRequest(endpoint, "POST", requestData);

    if (response.isVoid()) {
      Log::warn("StreamChatClient: Failed to send typing indicator");
    }
  });
}

void StreamChatClient::handleWebSocketMessage(const juce::String &message) {
  // Parse JSON message
  auto event = juce::JSON::parse(message);
  if (event.isObject()) {
    parseWebSocketEvent(event);
  }
}

void StreamChatClient::parseWebSocketEvent(const juce::var &event) {
  auto eventType = event.getProperty("type", "").toString();

  if (eventType == "message.new") {
    auto messageData = event.getProperty("message", juce::var());
    if (messageData.isObject()) {
      auto message = parseMessage(messageData);
      auto channelId = event.getProperty("channel_id", "").toString();

      // Show OS notification for messages from other users (GetStream.io Chat
      // notifications)
      if (message.userId != currentUserId && !message.userId.isEmpty()) {
        // Truncate message text if too long
        juce::String displayText = message.text;
        if (displayText.length() > 100) {
          displayText = displayText.substring(0, 100) + "...";
        }

        // Show OS notification (setting check done via callback if provided)
        juce::String notificationTitle =
            message.userName.isNotEmpty() ? message.userName + " sent a message" : "New message";

        // Use callback if provided (allows PluginEditor to check settings)
        // Otherwise show directly (for backwards compatibility)
        if (onMessageNotificationRequested) {
          onMessageNotificationRequested(notificationTitle, displayText);
        } else {
          // Fallback: show notification directly
          OSNotification::show(notificationTitle, displayText, "",
                               true // sound enabled
          );
        }

        Log::debug("StreamChatClient: OS notification requested for message from " + message.userName);
      }

      // Call the existing callback for UI updates
      if (messageReceivedCallback) {
        juce::MessageManager::callAsync([this, message, channelId]() {
          if (messageReceivedCallback)
            messageReceivedCallback(message, channelId);
        });
      }
    }
  } else if (eventType == "typing.start" || eventType == "typing.stop") {
    auto userData = event.getProperty("user", juce::var());
    if (userData.isObject() && typingCallback) {
      auto userId = userData.getProperty("id", "").toString();
      bool isTyping = eventType == "typing.start";
      juce::MessageManager::callAsync([this, userId, isTyping]() {
        if (typingCallback)
          typingCallback(userId, isTyping);
      });
    }
  } else if (eventType == "user.presence.changed") {
    auto userData = event.getProperty("user", juce::var());
    if (userData.isObject() && presenceChangedCallback) {
      auto presence = parsePresence(userData);
      juce::MessageManager::callAsync([this, presence]() {
        if (presenceChangedCallback)
          presenceChangedCallback(presence);
      });
    }
  }
}

// ==============================================================================
void StreamChatClient::uploadAudioSnippet(const juce::AudioBuffer<float> &audioBuffer, double sampleRate,
                                          AudioSnippetCallback callback) {
  if (!isAuthenticated() || backendAuthToken.isEmpty()) {
    if (callback)
      callback(Outcome<AudioSnippetResult>::error("Not authenticated"));
    return;
  }

  // Validate duration (max 30 seconds for snippets)
  double durationSecs = static_cast<double>(audioBuffer.getNumSamples()) / sampleRate;
  if (durationSecs > 30.0) {
    Log::warn("Audio snippet too long: " + juce::String(durationSecs) + "s (max 30s)");
    if (callback)
      callback(Outcome<AudioSnippetResult>::error("Audio snippet too long (max 30s)"));
    return;
  }

  // Note: This function does network I/O inside the work function, which is
  // fine The callback from uploadMultipartAbsolute is already on the message
  // thread
  Async::runVoid([this, audioBuffer, sampleRate, durationSecs, callback]() {
    // Encode audio to WAV - use MemoryBlock that survives stream destruction
    juce::MemoryBlock audioDataBlock;
    std::unique_ptr<juce::OutputStream> outputStream =
        std::make_unique<juce::MemoryOutputStream>(audioDataBlock, false);
    juce::WavAudioFormat wavFormat;

    juce::AudioFormatWriter::AudioFormatWriterOptions options;
    options.sampleRate = sampleRate;
    options.numChannels = static_cast<unsigned int>(audioBuffer.getNumChannels());
    options.bitsPerSample = 16;

    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outputStream.get(), options));

    if (writer == nullptr) {
      Log::error("Failed to create WAV writer");
      juce::MessageManager::callAsync([callback]() {
        if (callback)
          callback(Outcome<AudioSnippetResult>::error("Failed to create WAV writer"));
      });
      return;
    }

    // Ownership of outputStream is transferred to writer
    outputStream.release();

    writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples());
    writer.reset(); // Flush and release stream

    // audioDataBlock still has the data (MemoryOutputStream wrote to it)

    if (networkClient == nullptr) {
      Log::warn("StreamChatClient: NetworkClient not set");
      juce::MessageManager::callAsync([callback]() {
        if (callback)
          callback(Outcome<AudioSnippetResult>::error("Failed to create WAV writer"));
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
        config.backendBaseUrl + "/api/v1/audio/upload", "audio", audioDataBlock, "snippet.wav", "audio/wav", metadata,
        [callback, durationSecs](Outcome<juce::var> responseOutcome) {
          if (responseOutcome.isOk()) {
            auto response = responseOutcome.getValue();
            if (response.isObject()) {
              auto audioUrl = response.getProperty("audio_url", "").toString();
              if (audioUrl.isEmpty())
                audioUrl = response.getProperty("url", "").toString();

              if (!audioUrl.isEmpty()) {
                AudioSnippetResult result;
                result.audioUrl = audioUrl;
                result.duration = durationSecs;
                if (callback)
                  callback(Outcome<AudioSnippetResult>::ok(result));
                return;
              }
            }
          }

          if (callback)
            callback(Outcome<AudioSnippetResult>::error(responseOutcome.isError() ? responseOutcome.getError()
                                                                                  : "Failed to upload audio snippet"));
        },
        headers);
  });
}

void StreamChatClient::sendMessageWithAudio(const juce::String &channelType, const juce::String &channelId,
                                            const juce::String &text, const juce::AudioBuffer<float> &audioBuffer,
                                            double sampleRate, MessageCallback callback) {
  // First upload the audio snippet
  uploadAudioSnippet(
      audioBuffer, sampleRate,
      [this, channelType, channelId, text, callback](Outcome<AudioSnippetResult> uploadResult) {
        if (uploadResult.isError() || uploadResult.getValue().audioUrl.isEmpty()) {
          if (callback)
            callback(Outcome<Message>::error(uploadResult.isError() ? uploadResult.getError() : "Audio upload failed"));
          return;
        }

        auto audioResult = uploadResult.getValue();
        // Then send message with audio URL in extra_data
        juce::var extraData = juce::var(new juce::DynamicObject());
        auto *obj = extraData.getDynamicObject();
        obj->setProperty("audio_url", audioResult.audioUrl);
        obj->setProperty("audio_duration", audioResult.duration);

        sendMessage(channelType, channelId, text, extraData, callback);
      });
}
