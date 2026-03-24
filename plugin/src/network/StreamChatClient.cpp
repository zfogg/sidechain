#include "StreamChatClient.h"
#include "../util/Async.h"
#include "../util/Json.h"
#include "../util/Log.h"
#include "../util/OSNotification.h"
#include "../util/Result.h"
#include <JuceHeader.h>
#include <nlohmann/json.hpp>
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
      [this, callback](Outcome<nlohmann::json> responseOutcome) {
        if (responseOutcome.isOk()) {
          const auto &response = responseOutcome.getValue();
          if (response.is_object()) {
            juce::String authToken = response.contains("token") && response["token"].is_string()
                                         ? juce::String(response["token"].get<std::string>())
                                         : juce::String();
            juce::String streamApiKey = response.contains("api_key") && response["api_key"].is_string()
                                            ? juce::String(response["api_key"].get<std::string>())
                                            : juce::String();
            juce::String streamUserId = response.contains("user_id") && response["user_id"].is_string()
                                            ? juce::String(response["user_id"].get<std::string>())
                                            : juce::String();

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
  wsUrl = "wss://chat.stream-io-api.com/?api_key=" + apiKey + "&authorization=" + token + "&user_id=" + userId;

  Log::info("StreamChatClient token set for user: " + userId + ", API key configured");
}

// ==============================================================================
juce::String StreamChatClient::buildAuthHeaders() const {
  juce::String headers = "Stream-Auth-Type: jwt\r\n";
  headers += "Authorization: " + chatToken + "\r\n";
  headers += "Content-Type: application/json\r\n";
  return headers;
}

nlohmann::json StreamChatClient::makeStreamRequest(const juce::String &endpoint, const juce::String &method,
                                                   const nlohmann::json &data) {
  if (!isAuthenticated()) {
    Log::warn("StreamChatClient: Not authenticated, cannot make request");
    return nlohmann::json();
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
    if (!data.is_null()) {
      juce::String jsonString = juce::String(data.dump());
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
    return nlohmann::json();
  }

  auto response = stream->readEntireStreamAsString();
  Log::debug("StreamChatClient: Response: " + response);

  try {
    return nlohmann::json::parse(response.toStdString());
  } catch (const nlohmann::json::parse_error &e) {
    Log::error("StreamChatClient: JSON parse error - " + juce::String(e.what()));
    return nlohmann::json();
  }
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

        nlohmann::json requestData = nlohmann::json::object();

        // Build members as an OBJECT (not array): {"user_id": {}, "user_id":
        // {}} Stream.io API expects members field to be an object
        nlohmann::json members = nlohmann::json::object();
        members[currentUserId.toStdString()] = nlohmann::json::object();
        members[targetUserId.toStdString()] = nlohmann::json::object();

        requestData["members"] = members;
        requestData["created_by_id"] = currentUserId.toStdString();

        Log::debug("StreamChatClient: Creating direct channel with " + targetUserId + ", channel ID: " + channelId);
        Log::debug("StreamChatClient: Request data - " + juce::String(requestData.dump()));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient: Create channel response - " + juce::String(response.dump()));

        // Response from stream.io has channel wrapped in "channel" property
        nlohmann::json channelData = response;
        if (response.is_object() && response.contains("channel")) {
          channelData = response["channel"];
        }

        if (channelData.is_object() && channelData.contains("id")) {
          Log::debug("StreamChatClient: Direct channel created: " + juce::String(channelData.value("id", "")));
          return parseChannel(channelData);
        }

        Log::error("StreamChatClient: Failed to create direct channel. Response: " + juce::String(response.dump()));
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

        nlohmann::json requestData = nlohmann::json::object();

        // Build members as an OBJECT (not array): {"user_id": {}, "user_id":
        // {}} Stream.io API expects members field to be an object
        nlohmann::json members = nlohmann::json::object();
        for (const auto &memberId : memberIds) {
          members[memberId.toStdString()] = nlohmann::json::object();
        }
        requestData["members"] = members;

        // Add channel data with name
        nlohmann::json data = nlohmann::json::object();
        data["name"] = name.toStdString();
        requestData["data"] = data;

        requestData["created_by_id"] = currentUserId.toStdString();

        Log::debug("StreamChatClient: Creating group channel " + channelId + " with " + juce::String(memberIds.size()) +
                   " members");
        Log::debug("StreamChatClient: Request data - " + juce::String(requestData.dump()));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient: Create group channel response - " + juce::String(response.dump()));

        // Response from stream.io has channel wrapped in "channel" property
        nlohmann::json channelData = response;
        if (response.is_object() && response.contains("channel")) {
          channelData = response["channel"];
        }

        if (channelData.is_object() && channelData.contains("id")) {
          Log::debug("StreamChatClient: Group channel created: " + juce::String(channelData.value("id", "")));
          return parseChannel(channelData);
        }

        Log::error("StreamChatClient: Failed to create group channel. Response: " + juce::String(response.dump()));
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
        nlohmann::json requestData = nlohmann::json::object();

        // Filter channels - query for channels where current user is a member
        // Format per getstream.io docs: {members: {$in: [userID]}}
        nlohmann::json filter = nlohmann::json::object();

        // Create the members.$in array
        nlohmann::json membersFilter = nlohmann::json::object();
        membersFilter["$in"] = nlohmann::json::array({currentUserId.toStdString()});

        // Set members filter
        filter["members"] = membersFilter;
        requestData["filter_conditions"] = filter;

        // Set state to true to get full channel data (required for queryChannels to work)
        requestData["state"] = true;

        // Build sort
        nlohmann::json sort = nlohmann::json::array();
        nlohmann::json sortItem = nlohmann::json::object();
        sortItem["field"] = "last_message_at";
        sortItem["direction"] = -1;
        sort.push_back(sortItem);
        requestData["sort"] = sort;

        requestData["limit"] = limit;
        requestData["offset"] = offset;

        Log::debug("StreamChatClient: Querying channels with filter - " + juce::String(requestData.dump()));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient: queryChannels response - " + juce::String(response.dump()));

        std::vector<Channel> channels;

        if (response.is_object()) {
          if (response.contains("channels") && response["channels"].is_array()) {
            const auto &channelsArray = response["channels"];
            Log::debug("StreamChatClient: Found channels array with " + juce::String(channelsArray.size()) +
                       " channels");
            // Note: Stream.io queryChannels doesn't return member data in the
            // list response, so we can't filter by members. Just return all
            // channels - the API already restricts access to channels the user
            // has permission to view.
            for (const auto &channelData : channelsArray) {
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
                                   const juce::String &text, const nlohmann::json &extraData,
                                   MessageCallback callback) {
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

        nlohmann::json requestData = nlohmann::json::object();
        nlohmann::json message = nlohmann::json::object();

        // Message type is required by stream.io - use "regular" for user
        // messages
        message["type"] = "regular";
        message["text"] = text.toStdString();

        if (!extraData.is_null()) {
          message["extra_data"] = extraData;
        }

        requestData["message"] = message;

        Log::debug("StreamChatClient::sendMessage - request data: " + juce::String(requestData.dump()));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient::sendMessage - response: " + juce::String(response.dump()));

        if (response.is_object() && response.contains("message") && response["message"].is_object()) {
          const auto &messageData = response["message"];
          Log::debug("StreamChatClient::sendMessage - message sent "
                     "successfully with ID: " +
                     juce::String(messageData.value("id", "")));
          return parseMessage(messageData);
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
        nlohmann::json requestData = nlohmann::json::object();

        // Build the messages pagination parameters
        // According to getstream.io API, messages should be a nested object with limit and offset
        nlohmann::json messagesObj = nlohmann::json::object();
        messagesObj["limit"] = limit;
        messagesObj["offset"] = offset;
        requestData["messages"] = messagesObj;

        // Set state to true to get full response with messages
        // Note: watch requires active WebSocket connection, so we don't set it here
        requestData["state"] = true;

        Log::debug("StreamChatClient::queryMessages - endpoint: " + endpoint);
        Log::debug("StreamChatClient::queryMessages - request: " + juce::String(requestData.dump()));

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        Log::debug("StreamChatClient::queryMessages - response: " + juce::String(response.dump()));

        std::vector<Message> messages;

        // Parse response using proper JSON object access
        if (response.is_object()) {
          if (response.contains("messages") && response["messages"].is_array()) {
            const auto &messagesArray = response["messages"];
            Log::debug("StreamChatClient::queryMessages - found " + juce::String(messagesArray.size()) + " messages");

            for (size_t i = 0; i < messagesArray.size(); i++) {
              const auto &messageData = messagesArray[i];
              if (messageData.is_object()) {
                messages.push_back(parseMessage(messageData));
              } else {
                Log::warn("StreamChatClient::queryMessages - message at index " + juce::String(i) +
                          " is not an object");
              }
            }
          } else if (!response.contains("messages")) {
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
void StreamChatClient::searchMessages(const juce::String &query, const nlohmann::json &channelFilters, int limit,
                                      int offset, MessagesCallback callback) {
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

        if (!channelFilters.is_null()) {
          endpoint += "&filter_conditions=" + juce::URL::addEscapeChars(juce::String(channelFilters.dump()), true);
        }

        auto response = makeStreamRequest(endpoint, "GET", nlohmann::json());

        std::vector<Message> messages;

        if (response.is_object() && response.contains("results") && response["results"].is_array()) {
          const auto &results = response["results"];
          for (const auto &messageData : results) {
            messages.push_back(parseMessage(messageData));
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
        nlohmann::json filter = nlohmann::json::object();
        nlohmann::json inArray = nlohmann::json::array();
        for (const auto &userId : userIds) {
          inArray.push_back(userId.toStdString());
        }
        nlohmann::json inObj = nlohmann::json::object();
        inObj["$in"] = inArray;
        filter["id"] = inObj;

        endpoint += "?filter=" + juce::URL::addEscapeChars(juce::String(filter.dump()), true);
        endpoint += "&presence=true";

        auto response = makeStreamRequest(endpoint, "GET", nlohmann::json());

        std::vector<UserPresence> presenceList;

        if (response.is_object() && response.contains("users") && response["users"].is_array()) {
          const auto &usersArray = response["users"];
          for (const auto &userData : usersArray) {
            presenceList.push_back(parsePresence(userData));
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
  if (ids.size() < 2) {
    Log::error("StreamChatClient: Not enough user IDs to create channel (got " + juce::String(ids.size()) + ")");
    return "";
  }

  juce::String id1 = ids[0].substring(0, 30);
  juce::String id2 = ids[1].substring(0, 30);
  juce::String channelId = id1 + "_" + id2;

  Log::debug("StreamChatClient: Generated channel ID '" + channelId + "' (length: " + juce::String(channelId.length()) +
             ") for users " + userId1 + " and " + userId2);

  return channelId;
}

// ==============================================================================
StreamChatClient::Channel StreamChatClient::parseChannel(const nlohmann::json &channelData) {
  Channel channel;

  if (channelData.is_object()) {
    channel.id = juce::String(channelData.value("id", ""));
    channel.type = juce::String(channelData.value("type", ""));

    // Store members array as nlohmann::json
    if (channelData.contains("members") && channelData["members"].is_array()) {
      channel.members = channelData["members"];
    }

    if (channelData.contains("data") && channelData["data"].is_object()) {
      const auto &data = channelData["data"];
      channel.name = juce::String(data.value("name", ""));
      channel.extraData = data;
    }

    if (channelData.contains("last_message") && channelData["last_message"].is_object()) {
      channel.lastMessage = channelData["last_message"];
    }
    channel.unreadCount = channelData.value("unread_count", 0);
    channel.lastMessageAt = juce::String(channelData.value("last_message_at", ""));
  }

  return channel;
}

StreamChatClient::Message StreamChatClient::parseMessage(const nlohmann::json &messageData) {
  Message message;

  if (messageData.is_object()) {
    message.id = juce::String(messageData.value("id", ""));
    message.text = juce::String(messageData.value("text", ""));
    message.createdAt = juce::String(messageData.value("created_at", ""));

    if (messageData.contains("reactions") && messageData["reactions"].is_object()) {
      message.reactions = messageData["reactions"];
    }
    if (messageData.contains("extra_data") && messageData["extra_data"].is_object()) {
      message.extraData = messageData["extra_data"];
    }
    message.isDeleted = messageData.contains("deleted_at") && messageData["deleted_at"].is_string();

    if (messageData.contains("user") && messageData["user"].is_object()) {
      const auto &user = messageData["user"];
      message.userId = juce::String(user.value("id", ""));
      message.userName = juce::String(user.value("name", ""));
    }
  }

  return message;
}

StreamChatClient::UserPresence StreamChatClient::parsePresence(const nlohmann::json &userData) {
  UserPresence presence;

  if (userData.is_object()) {
    presence.userId = juce::String(userData.value("id", ""));
    presence.online = userData.value("online", false);
    presence.lastActive = juce::String(userData.value("last_active", ""));
    presence.status = juce::String(userData.value("status", ""));
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

        auto response = makeStreamRequest(endpoint, "GET", nlohmann::json());

        if (response.is_object() && response.contains("channel") && response["channel"].is_object()) {
          return parseChannel(response["channel"]);
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

        auto response = makeStreamRequest(endpoint, "DELETE", nlohmann::json());

        return response.is_object() && response.contains("channel");
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

        nlohmann::json requestData = nlohmann::json::object();
        nlohmann::json members = nlohmann::json::array();
        for (const auto &memberId : memberIds) {
          members.push_back(memberId.toStdString());
        }
        requestData["user_ids"] = members;

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.is_object() && response.contains("channel");
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

        nlohmann::json requestData = nlohmann::json::object();
        nlohmann::json members = nlohmann::json::array();
        for (const auto &memberId : memberIds) {
          members.push_back(memberId.toStdString());
        }
        requestData["user_ids"] = members;

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.is_object() && response.contains("channel");
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
                                     const juce::String &name, const nlohmann::json &extraData,
                                     std::function<void(Outcome<Channel>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<Channel>::error("Not authenticated"));
    return;
  }

  Async::run<Channel>(
      [this, channelType, channelId, name, extraData]() -> Channel {
        juce::String endpoint = "/channels/" + channelType + "/" + channelId;

        nlohmann::json requestData = nlohmann::json::object();
        nlohmann::json data = nlohmann::json::object();

        if (!name.isEmpty()) {
          data["name"] = name.toStdString();
        }

        // Merge extra data if provided
        if (!extraData.is_null() && extraData.is_object()) {
          for (auto &[key, value] : extraData.items()) {
            if (key != "name") { // Don't override name if already set
              data[key] = value;
            }
          }
        }

        requestData["data"] = data;

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        if (response.is_object() && response.contains("channel") && response["channel"].is_object()) {
          return parseChannel(response["channel"]);
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

        nlohmann::json requestData = nlohmann::json::object();
        nlohmann::json message = nlohmann::json::object();
        message["id"] = messageId.toStdString();
        message["text"] = newText.toStdString();
        requestData["message"] = message;

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        if (response.is_object() && response.contains("message") && response["message"].is_object()) {
          return parseMessage(response["message"]);
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

        auto response = makeStreamRequest(endpoint, "DELETE", nlohmann::json());

        return response.is_object() && response.contains("message");
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

        nlohmann::json requestData = nlohmann::json::object();
        nlohmann::json reaction = nlohmann::json::object();
        reaction["type"] = reactionType.toStdString();
        requestData["reaction"] = reaction;

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.is_object() && response.contains("message");
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

        auto response = makeStreamRequest(endpoint, "DELETE", nlohmann::json());

        return response.is_object() && response.contains("message");
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

        auto response = makeStreamRequest(endpoint, "POST", nlohmann::json());

        return response.is_object();
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

void StreamChatClient::updateStatus(const juce::String &status, const nlohmann::json &extraData,
                                    std::function<void(Outcome<void>)> callback) {
  if (!isAuthenticated()) {
    if (callback)
      callback(Outcome<void>::error("Not authenticated"));
    return;
  }

  Async::run<bool>(
      [this, status, extraData]() -> bool {
        juce::String endpoint = "/users/" + currentUserId;

        nlohmann::json requestData = nlohmann::json::object();
        requestData["status"] = status.toStdString();

        if (!extraData.is_null()) {
          requestData["extra_data"] = extraData;
        }

        auto response = makeStreamRequest(endpoint, "POST", requestData);

        return response.is_object() && response.contains("user");
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

    nlohmann::json requestData = nlohmann::json::object();
    nlohmann::json eventData = nlohmann::json::object();
    eventData["type"] = isTyping ? "typing.start" : "typing.stop";
    requestData["event"] = eventData;

    auto response = makeStreamRequest(endpoint, "POST", requestData);

    if (response.is_null()) {
      Log::warn("StreamChatClient: Failed to send typing indicator");
    }
  });
}

void StreamChatClient::handleWebSocketMessage(const juce::String &message) {
  // Parse JSON message
  try {
    auto event = nlohmann::json::parse(message.toStdString());
    if (event.is_object()) {
      parseWebSocketEvent(event);
    }
  } catch (const nlohmann::json::parse_error &e) {
    Log::error("StreamChatClient: WebSocket message parse error - " + juce::String(e.what()));
  }
}

void StreamChatClient::parseWebSocketEvent(const nlohmann::json &event) {
  juce::String eventType = juce::String(event.value("type", ""));

  if (eventType == "message.new") {
    if (event.contains("message") && event["message"].is_object()) {
      auto message = parseMessage(event["message"]);
      juce::String channelId = juce::String(event.value("channel_id", ""));

      // Push to RxCpp subject for reactive subscribers
      MessageEvent msgEvent{message, channelId};
      messageSubject_.get_subscriber().on_next(msgEvent);

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

      // Call the existing callback for UI updates (legacy support)
      if (messageReceivedCallback) {
        juce::MessageManager::callAsync([this, message, channelId]() {
          if (messageReceivedCallback)
            messageReceivedCallback(message, channelId);
        });
      }
    }
  } else if (eventType == "typing.start" || eventType == "typing.stop") {
    if (event.contains("user") && event["user"].is_object()) {
      const auto &userData = event["user"];
      juce::String userId = juce::String(userData.value("id", ""));
      juce::String channelId = juce::String(event.value("channel_id", ""));
      bool isTyping = eventType == "typing.start";

      // Push to RxCpp subject for reactive subscribers
      TypingEvent typingEvent{userId, channelId, isTyping};
      typingSubject_.get_subscriber().on_next(typingEvent);

      // Call the existing callback (legacy support)
      if (typingCallback) {
        juce::MessageManager::callAsync([this, userId, isTyping]() {
          if (typingCallback)
            typingCallback(userId, isTyping);
        });
      }
    }
  } else if (eventType == "user.presence.changed") {
    if (event.contains("user") && event["user"].is_object()) {
      auto presence = parsePresence(event["user"]);

      // Push to RxCpp subject for reactive subscribers
      presenceSubject_.get_subscriber().on_next(presence);

      // Call the existing callback (legacy support)
      if (presenceChangedCallback) {
        juce::MessageManager::callAsync([this, presence]() {
          if (presenceChangedCallback)
            presenceChangedCallback(presence);
        });
      }
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

    // JUCE 8.0.8 API: createWriterFor(stream, sampleRate, numChannels, bitsPerSample, metadata, quality)
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
        outputStream.get(), sampleRate, static_cast<unsigned int>(audioBuffer.getNumChannels()), 16, {}, 0));

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
        [callback, durationSecs](Outcome<nlohmann::json> responseOutcome) {
          if (responseOutcome.isOk()) {
            const auto &response = responseOutcome.getValue();
            if (response.is_object()) {
              juce::String audioUrl;
              if (response.contains("audio_url") && response["audio_url"].is_string()) {
                audioUrl = juce::String(response["audio_url"].get<std::string>());
              }
              if (audioUrl.isEmpty() && response.contains("url") && response["url"].is_string()) {
                audioUrl = juce::String(response["url"].get<std::string>());
              }

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
        nlohmann::json extraData = nlohmann::json::object();
        extraData["audio_url"] = audioResult.audioUrl.toStdString();
        extraData["audio_duration"] = audioResult.duration;

        sendMessage(channelType, channelId, text, extraData, callback);
      });
}

// ==============================================================================
// Reactive Observable Methods (Phase 6)
// ==============================================================================

rxcpp::observable<std::vector<StreamChatClient::Channel>> StreamChatClient::queryChannelsObservable(int limit,
                                                                                                    int offset) {
  return rxcpp::sources::create<std::vector<Channel>>([this, limit, offset](auto observer) {
           queryChannels(
               [observer](Outcome<std::vector<Channel>> result) {
                 if (result.isOk()) {
                   observer.on_next(result.getValue());
                   observer.on_completed();
                 } else {
                   observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                 }
               },
               limit, offset);
         })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<StreamChatClient::Channel>
StreamChatClient::createDirectChannelObservable(const juce::String &targetUserId) {
  return rxcpp::sources::create<Channel>([this, targetUserId](auto observer) {
           createDirectChannel(targetUserId, [observer](Outcome<Channel> result) {
             if (result.isOk()) {
               observer.on_next(result.getValue());
               observer.on_completed();
             } else {
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<StreamChatClient::Channel> StreamChatClient::getChannelObservable(const juce::String &channelType,
                                                                                    const juce::String &channelId) {
  return rxcpp::sources::create<Channel>([this, channelType, channelId](auto observer) {
           getChannel(channelType, channelId, [observer](Outcome<Channel> result) {
             if (result.isOk()) {
               observer.on_next(result.getValue());
               observer.on_completed();
             } else {
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<StreamChatClient::Message>>
StreamChatClient::queryMessagesObservable(const juce::String &channelType, const juce::String &channelId, int limit,
                                          int offset) {
  return rxcpp::sources::create<std::vector<Message>>([this, channelType, channelId, limit, offset](auto observer) {
           queryMessages(channelType, channelId, limit, offset, [observer](Outcome<std::vector<Message>> result) {
             if (result.isOk()) {
               observer.on_next(result.getValue());
               observer.on_completed();
             } else {
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<StreamChatClient::Message> StreamChatClient::sendMessageObservable(const juce::String &channelType,
                                                                                     const juce::String &channelId,
                                                                                     const juce::String &text,
                                                                                     const nlohmann::json &extraData) {
  return rxcpp::sources::create<Message>([this, channelType, channelId, text, extraData](auto observer) {
           sendMessage(channelType, channelId, text, extraData, [observer](Outcome<Message> result) {
             if (result.isOk()) {
               observer.on_next(result.getValue());
               observer.on_completed();
             } else {
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<StreamChatClient::Message>>
StreamChatClient::searchMessagesObservable(const juce::String &query, const nlohmann::json &channelFilters, int limit,
                                           int offset) {
  return rxcpp::sources::create<std::vector<Message>>([this, query, channelFilters, limit, offset](auto observer) {
           searchMessages(query, channelFilters, limit, offset, [observer](Outcome<std::vector<Message>> result) {
             if (result.isOk()) {
               observer.on_next(result.getValue());
               observer.on_completed();
             } else {
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<StreamChatClient::UserPresence>>
StreamChatClient::queryPresenceObservable(const std::vector<juce::String> &userIds) {
  return rxcpp::sources::create<std::vector<UserPresence>>([this, userIds](auto observer) {
           queryPresence(userIds, [observer](Outcome<std::vector<UserPresence>> result) {
             if (result.isOk()) {
               observer.on_next(result.getValue());
               observer.on_completed();
             } else {
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

// ==============================================================================
// Interval-based Polling Observables
// ==============================================================================

rxcpp::observable<std::vector<StreamChatClient::Message>>
StreamChatClient::pollChannelMessagesObservable(const juce::String &channelType, const juce::String &channelId,
                                                int intervalMs) {
  // Use a shared state to track the last seen message ID
  auto lastSeenId = std::make_shared<juce::String>("");

  return rxcpp::sources::interval(std::chrono::milliseconds(intervalMs))
      .flat_map([this, channelType, channelId, lastSeenId](long /* tick */) {
        return queryMessagesObservable(channelType, channelId, 20, 0)
            .map([lastSeenId](std::vector<Message> messages) -> std::vector<Message> {
              if (messages.empty()) {
                return {};
              }

              // Find new messages since last seen
              std::vector<Message> newMessages;
              bool foundLastSeen = lastSeenId->isEmpty();

              for (const auto &msg : messages) {
                if (msg.id == *lastSeenId) {
                  foundLastSeen = true;
                  continue;
                }
                if (foundLastSeen) {
                  newMessages.push_back(msg);
                }
              }

              // Update last seen to newest message
              if (!messages.empty()) {
                *lastSeenId = messages.back().id;
              }

              return newMessages;
            });
      })
      .filter([](const std::vector<Message> &messages) { return !messages.empty(); })
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> StreamChatClient::pollUnreadCountObservable(int intervalMs) {
  return rxcpp::sources::interval(std::chrono::milliseconds(intervalMs))
      .flat_map([this](long /* tick */) {
        return queryChannelsObservable().map([](std::vector<Channel> channels) {
          int totalUnread = 0;
          for (const auto &channel : channels) {
            totalUnread += channel.unreadCount;
          }
          return totalUnread;
        });
      })
      .distinct_until_changed() // Only emit when count actually changes
      .observe_on(Sidechain::Rx::observe_on_juce_thread());
}
