#include "WebSocketClient.h"
#include "../util/Log.h"
#include <chrono>
#include <thread>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/thread.hpp>

//==============================================================================
WebSocketClient::WebSocketClient(const Config &cfg) : Thread("WebSocketClient"), config(cfg) {
  Log::info("WebSocketClient initialized - host: " + config.host + ":" + juce::String(config.port));
}

WebSocketClient::~WebSocketClient() {
  disconnect();
  stopThread(5000);
}

//==============================================================================
void WebSocketClient::connect() {
  if (state.load() == ConnectionState::Connected || state.load() == ConnectionState::Connecting) {
    Log::debug("WebSocket: Already connected or connecting");
    return;
  }

  shouldReconnect.store(true);
  reconnectAttempts.store(0);

  if (!isThreadRunning()) {
    startThread();
  } else {
    notify();
  }
}

void WebSocketClient::disconnect() {
  shouldReconnect.store(false);

  // Close websocket connection if active
  if (connectionActive.load() && client) {
    try {
      connection_ptr con = client->get_con_from_hdl(currentConnection);
      if (con && con->get_state() == websocketpp::session::state::open) {
        client->close(currentConnection, websocketpp::close::status::going_away, "Client disconnect");
      }
    } catch (const std::exception &e) {
      Log::debug("WebSocket: Error during disconnect - " + juce::String(e.what()));
    }

    // Stop ASIO event loop
    try {
      client->stop_perpetual();
      client->stop();
    } catch (const std::exception &e) {
      Log::debug("WebSocket: Error stopping ASIO - " + juce::String(e.what()));
    }
  }

  setState(ConnectionState::Disconnected);
  notify();
}

//==============================================================================
void WebSocketClient::setAuthToken(const juce::String &token) {
  authToken = token;
  Log::debug("WebSocket: Auth token set");
}

void WebSocketClient::clearAuthToken() {
  authToken = juce::String();
}

void WebSocketClient::setConfig(const Config &newConfig) {
  config = newConfig;
}

//==============================================================================
bool WebSocketClient::send(const juce::var &message) {
  juce::String json = juce::JSON::toString(message);

  if (state.load() != ConnectionState::Connected || !connectionActive.load()) {
    queueMessage(message);
    Log::debug("WebSocket: Message queued (not connected)");
    return false;
  }

  try {
    connection_ptr con = client->get_con_from_hdl(currentConnection);
    if (con && con->get_state() == websocketpp::session::state::open) {
      websocketpp::lib::error_code ec;
      // Convert to std::string to ensure proper length handling
      std::string payload = json.toStdString();
      client->send(currentConnection, payload, websocketpp::frame::opcode::text, ec);

      if (ec) {
        Log::error("WebSocket: Failed to send message - " + juce::String(ec.message().c_str()));
        return false;
      }

      {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.messagesSent++;
      }
      return true;
    }
  } catch (const std::exception &e) {
    Log::error("WebSocket: Exception sending message - " + juce::String(e.what()));
  }

  return false;
}

bool WebSocketClient::send(const juce::String &type, const juce::var &data) {
  juce::var message = juce::var(new juce::DynamicObject());
  message.getDynamicObject()->setProperty("type", type);
  message.getDynamicObject()->setProperty("payload", data); // Server expects "payload", not "data"
  message.getDynamicObject()->setProperty("timestamp", juce::Time::currentTimeMillis());
  return send(message);
}

//==============================================================================
WebSocketClient::Stats WebSocketClient::getStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  return stats;
}

//==============================================================================
// Thread implementation
void WebSocketClient::run() {
  while (!threadShouldExit()) {
    if (shouldReconnect.load() && state.load() != ConnectionState::Connected) {
      // Check if we should wait before reconnecting
      int64_t now = juce::Time::currentTimeMillis();
      int64_t nextReconnect = nextReconnectTime.load();

      if (nextReconnect > 0 && now < nextReconnect) {
        wait(static_cast<int>(nextReconnect - now));
        continue;
      }

      attemptConnection();
    } else {
      // Wait for reconnect signal or shutdown
      wait(1000);
    }
  }

  // Clean up - stop any running connections
  cleanupClient();
}

//==============================================================================
void WebSocketClient::cleanupClient() {
  if (client) {
    try {
      // Stop ASIO event loop
      client->stop_perpetual();

      // Close connection if open
      if (connectionActive.load()) {
        try {
          connection_ptr con = client->get_con_from_hdl(currentConnection);
          if (con && con->get_state() == websocketpp::session::state::open) {
            client->close(currentConnection, websocketpp::close::status::going_away, "Client shutdown");
          }
        } catch (...) {
        }
      }

      client->stop();
    } catch (const std::exception &e) {
      Log::debug("WebSocket: Error stopping client - " + juce::String(e.what()));
    }

    // Wait for ASIO thread to finish (with timeout)
    if (asioThread && asioThread->joinable()) {
      // Give it a moment to finish
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (asioThread->joinable()) {
        // Thread is still running - attempt to join with a timeout
        // Note: std::thread doesn't support timed join, so we accept some resource usage
        // The thread will eventually finish when the client is destroyed
        Log::warn("WebSocket: ASIO thread still running during cleanup");
      }
      asioThread.reset();
    }

    client.reset();
    connectionActive.store(false);
  }
}

//==============================================================================
void WebSocketClient::attemptConnection() {
  setState(ConnectionState::Connecting);

  // Clean up any existing client before creating a new one
  cleanupClient();

  try {
    // Create and initialize client
    client = std::make_unique<wspp_client>();

    // Disable verbose logging from websocketpp (only log errors)
    client->clear_access_channels(websocketpp::log::alevel::all);
    client->set_access_channels(websocketpp::log::alevel::fail);
    client->clear_error_channels(websocketpp::log::elevel::all);
    client->set_error_channels(websocketpp::log::elevel::info | websocketpp::log::elevel::warn |
                               websocketpp::log::elevel::rerror | websocketpp::log::elevel::fatal);

    // Initialize ASIO
    client->init_asio();

    // Set up event handlers
    client->set_open_handler([this](connection_hdl hdl) { onWsOpen(hdl); });

    client->set_close_handler([this](connection_hdl hdl) { onWsClose(hdl); });

    client->set_message_handler([this](connection_hdl hdl, message_ptr msg) { onWsMessage(hdl, msg); });

    client->set_fail_handler([this](connection_hdl hdl) { onWsFail(hdl); });

    client->set_pong_handler([this](connection_hdl hdl, std::string appData) { onWsPong(hdl, appData); });

    // Build URI
    juce::String uri = buildUri();
    Log::info("WebSocket: Connecting to " + uri);

    // Create connection
    websocketpp::lib::error_code ec;
    connection_ptr con = client->get_connection(uri.toStdString(), ec);

    if (ec) {
      Log::error("WebSocket: Connection error - " + juce::String(ec.message().c_str()));
      handleDisconnect("Connection error: " + juce::String(ec.message().c_str()));
      return;
    }

    // Note: Compression (permessage-deflate) is automatically negotiated by
    // websocketpp if both client and server support it. The backend supports
    // compression, so it will be enabled automatically during the WebSocket
    // handshake.

    currentConnection = con->get_handle();

    // Start ASIO event loop in background (runs until stopped)
    client->start_perpetual();

    // Connect (non-blocking)
    client->connect(con);

    // Run ASIO event loop in a separate thread context (blocks until stop() is
    // called)
    asioThread = std::make_unique<std::thread>([this]() {
      try {
        client->run();
      } catch (const std::exception &e) {
        Log::error("WebSocket: ASIO thread exception - " + juce::String(e.what()));
      }
    });

    // Wait for connection to complete or fail
    int timeout = config.connectTimeoutMs;
    int waited = 0;
    while (waited < timeout && !threadShouldExit()) {
      wait(100);
      waited += 100;

      if (state.load() == ConnectionState::Connected) {
        // Connection successful, enter heartbeat/maintenance loop
        connectionLoop();
        return;
      } else if (state.load() == ConnectionState::Disconnected && !connectionActive.load()) {
        // Connection failed, will reconnect
        return;
      }
    }

    if (waited >= timeout) {
      Log::error("WebSocket: Connection timeout");
      handleDisconnect("Connection timeout");
    }
  } catch (const std::exception &e) {
    Log::error("WebSocket: Exception during connection - " + juce::String(e.what()));
    handleDisconnect("Exception: " + juce::String(e.what()));
  }
}

void WebSocketClient::connectionLoop() {
  lastPingSentTime.store(0);
  lastPongReceivedTime.store(juce::Time::currentTimeMillis());
  lastHeartbeatSent.store(0);

  while (!threadShouldExit() && shouldReconnect.load() && connectionActive.load()) {
    if (!connectionActive.load() || state.load() != ConnectionState::Connected) {
      return;
    }

    // Check for heartbeat timeout
    if (isHeartbeatTimedOut()) {
      Log::warn("WebSocket: Heartbeat timeout");
      handleDisconnect("Heartbeat timeout");
      return;
    }

    // Send heartbeat if needed
    int64_t now = juce::Time::currentTimeMillis();
    if (now - lastHeartbeatSent.load() >= config.heartbeatIntervalMs) {
      sendHeartbeat();
      lastHeartbeatSent.store(now);
    }

    // ASIO event loop runs in separate thread, just check state and send
    // heartbeats
    wait(100); // Check heartbeat interval periodically
  }
}

//==============================================================================
// websocketpp event handlers
void WebSocketClient::onWsOpen(connection_hdl /* hdl */) {
  Log::info("WebSocket: Connected");

  connectionActive.store(true);
  setState(ConnectionState::Connected);
  reconnectAttempts.store(0);
  nextReconnectTime.store(0);

  {
    std::lock_guard<std::mutex> lock(statsMutex);
    stats.connectedTime = juce::Time::currentTimeMillis();
  }

  // Flush queued messages
  flushMessageQueue();
}

void WebSocketClient::onWsClose(connection_hdl hdl) {
  Log::info("WebSocket: Connection closed");

  connectionActive.store(false);
  currentConnection = connection_hdl();

  connection_ptr con = client->get_con_from_hdl(hdl);
  if (con) {
    juce::String reason = juce::String(con->get_remote_close_reason().c_str());
    juce::String code = juce::String(con->get_remote_close_code());
    Log::debug("WebSocket: Close code: " + code + ", reason: " + reason);
  }

  handleDisconnect("Server closed connection");
}

void WebSocketClient::onWsMessage(connection_hdl /* hdl */, message_ptr msg) {
  try {
    // Get the opcode to determine message type
    auto opcode = msg->get_opcode();
    std::string payload = msg->get_payload();

    // Log raw payload info for debugging (truncate for readability)
    if (payload.length() > 0) {
      juce::String payloadInfo = "WebSocket frame received - opcode: ";
      payloadInfo += juce::String(static_cast<int>(opcode));
      payloadInfo += ", size: " + juce::String(static_cast<size_t>(payload.length())) + " bytes";

      // If it's a text message, decode and show preview
      if (opcode == websocketpp::frame::opcode::text) {
        juce::String text = juce::String::fromUTF8(payload.c_str(), static_cast<int>(payload.length()));

        // Log decoded text (truncate long messages)
        if (text.length() > 200) {
          payloadInfo += ", decoded (truncated): " + text.substring(0, 200) + "...";
        } else {
          payloadInfo += ", decoded: " + text;
        }

        Log::debug(payloadInfo);
        processTextMessage(text);
      } else if (opcode == websocketpp::frame::opcode::binary) {
        // Binary message - show hex representation of first bytes
        payloadInfo += ", binary data (hex): ";
        size_t bytesToShow = juce::jmin(payload.length(), static_cast<size_t>(50));
        for (size_t i = 0; i < bytesToShow; ++i) {
          payloadInfo += juce::String::formatted("%02X ", static_cast<unsigned char>(payload[i]));
        }
        if (payload.length() > 50) {
          payloadInfo += "...";
        }
        Log::debug(payloadInfo);

        // Try to decode as UTF-8 text anyway (some servers send text as binary)
        juce::String text = juce::String::fromUTF8(payload.c_str(), static_cast<int>(payload.length()));
        if (text.isNotEmpty()) {
          processTextMessage(text);
        } else {
          Log::warn("WebSocket: Received binary message that couldn't be "
                    "decoded as text");
        }
      } else {
        // Other opcodes (ping, pong, close, etc.)
        Log::debug(payloadInfo);
      }
    } else {
      Log::debug("WebSocket: Received empty frame - opcode: " + juce::String(static_cast<int>(opcode)));
    }

    {
      std::lock_guard<std::mutex> lock(statsMutex);
      stats.messagesReceived++;
      stats.lastMessageTime = juce::Time::currentTimeMillis();
    }
  } catch (const std::exception &e) {
    Log::error("WebSocket: Error processing message - " + juce::String(e.what()));
  }
}

void WebSocketClient::onWsFail(connection_hdl hdl) {
  Log::error("WebSocket: Connection failed");

  connectionActive.store(false);
  currentConnection = connection_hdl();

  connection_ptr con = client->get_con_from_hdl(hdl);
  if (con) {
    juce::String errorMsg = juce::String(con->get_ec().message().c_str());
    Log::error("WebSocket: Error - " + errorMsg);

    // Check for authentication error
    if (errorMsg.contains("401") || errorMsg.contains("Unauthorized")) {
      juce::MessageManager::callAsync([this]() {
        if (onError)
          onError("Authentication failed - please log in again");
      });
    }
  }

  handleDisconnect("Connection failed");
}

void WebSocketClient::onWsPong(connection_hdl /* hdl */, std::string /* appData */) {
  lastPongReceivedTime.store(juce::Time::currentTimeMillis());
}

//==============================================================================
void WebSocketClient::handleDisconnect(const juce::String &reason) {
  Log::info("WebSocket: Disconnected - " + reason);

  connectionActive.store(false);
  setState(ConnectionState::Disconnected);

  // Note: We don't cleanupClient here because we want to preserve the client
  // for potential immediate reconnection. cleanupClient will be called in
  // attemptConnection() before creating a new client, or in destructor.

  if (shouldReconnect.load()) {
    scheduleReconnect();
  }
}

void WebSocketClient::scheduleReconnect() {
  int attempts = reconnectAttempts.fetch_add(1) + 1;

  // Check max attempts
  if (config.maxReconnectAttempts >= 0 && attempts > config.maxReconnectAttempts) {
    Log::warn("WebSocket: Max reconnect attempts reached");
    shouldReconnect.store(false);

    juce::MessageManager::callAsync([this]() {
      if (onError)
        onError("Connection lost - max reconnect attempts reached");
    });
    return;
  }

  // Calculate backoff delay (exponential with jitter)
  int baseDelay = config.reconnectBaseDelayMs * (1 << juce::jmin(attempts - 1, 10));
  int maxDelay = juce::jmin(baseDelay, config.reconnectMaxDelayMs);
  int jitter = juce::Random::getSystemRandom().nextInt(maxDelay / 4);
  int delay = maxDelay + jitter;

  Log::debug("WebSocket: Reconnecting in " + juce::String(delay) + "ms (attempt " + juce::String(attempts) + ")");

  nextReconnectTime.store(juce::Time::currentTimeMillis() + delay);
  setState(ConnectionState::Reconnecting);

  {
    std::lock_guard<std::mutex> lock(statsMutex);
    stats.reconnectAttempts = attempts;
  }
}

//==============================================================================
void WebSocketClient::processTextMessage(const juce::String &text) {
  Log::debug("WebSocket received: " + text);

  Message msg;
  msg.rawJson = text;
  msg.data = juce::JSON::parse(text);

  if (!msg.data.isObject()) {
    Log::warn("WebSocket: Invalid JSON message");
    return;
  }

  // Extract type
  msg.typeString = msg.data.getProperty("type", "").toString();
  msg.type = parseMessageType(msg.typeString);

  // Update pong time for heartbeat responses
  if (msg.type == MessageType::Heartbeat) {
    lastPongReceivedTime.store(juce::Time::currentTimeMillis());
  }

  dispatchMessage(msg);
}

void WebSocketClient::dispatchMessage(const Message &message) {
  juce::MessageManager::callAsync([this, message]() {
    if (onMessage)
      onMessage(message);
  });
}

WebSocketClient::MessageType WebSocketClient::parseMessageType(const juce::String &typeStr) {
  if (typeStr == "new_post" || typeStr == "post")
    return MessageType::NewPost;
  if (typeStr == "post_liked" || typeStr == "like" || typeStr == "reaction")
    return MessageType::Like;
  if (typeStr == "post_unliked" || typeStr == "unlike")
    return MessageType::Unlike;
  if (typeStr == "follow")
    return MessageType::Follow;
  if (typeStr == "new_comment")
    return MessageType::NewComment;
  if (typeStr == "comment")
    return MessageType::Comment;  // Legacy support
  if (typeStr == "comment_liked")
    return MessageType::CommentLiked;
  if (typeStr == "comment_unliked")
    return MessageType::CommentUnliked;
  if (typeStr == "notification")
    return MessageType::Notification;
  if (typeStr == "presence" || typeStr == "presence_update")
    return MessageType::PresenceUpdate;
  if (typeStr == "play_count" || typeStr == "play")
    return MessageType::PlayCount;
  if (typeStr == "like_count_update")
    return MessageType::LikeCountUpdate;
  if (typeStr == "comment_count_update")
    return MessageType::CommentCountUpdate;
  if (typeStr == "engagement_metrics")
    return MessageType::EngagementMetrics;
  if (typeStr == "follower_count_update")
    return MessageType::FollowerCountUpdate;
  if (typeStr == "feed_invalidate")
    return MessageType::FeedInvalidate;
  if (typeStr == "timeline_update")
    return MessageType::TimelineUpdate;
  if (typeStr == "notification_count_update")
    return MessageType::NotificationCountUpdate;
  if (typeStr == "user_typing")
    return MessageType::UserTyping;
  if (typeStr == "user_stop_typing")
    return MessageType::UserStopTyping;
  if (typeStr == "heartbeat" || typeStr == "pong")
    return MessageType::Heartbeat;
  if (typeStr == "error")
    return MessageType::Error;

  return MessageType::Unknown;
}

//==============================================================================
void WebSocketClient::sendHeartbeat() {
  // Send application-level heartbeat
  send("heartbeat", juce::var(new juce::DynamicObject()));

  // Send WebSocket ping
  if (connectionActive.load() && client) {
    try {
      websocketpp::lib::error_code ec;
      client->ping(currentConnection, "", ec);
      if (!ec) {
        lastPingSentTime.store(juce::Time::currentTimeMillis());
      }
    } catch (const std::exception &e) {
      Log::debug("WebSocket: Exception sending ping - " + juce::String(e.what()));
    }
  }
}

bool WebSocketClient::isHeartbeatTimedOut() const {
  int64_t lastPong = lastPongReceivedTime.load();
  if (lastPong == 0)
    return false;

  int64_t now = juce::Time::currentTimeMillis();
  // Timeout if no pong for 2x heartbeat interval
  return (now - lastPong) > (config.heartbeatIntervalMs * 2);
}

//==============================================================================
void WebSocketClient::queueMessage(const juce::var &message) {
  std::lock_guard<std::mutex> lock(queueMutex);

  if (messageQueue.size() >= static_cast<size_t>(config.messageQueueMaxSize)) {
    // Remove oldest message
    messageQueue.pop();
    Log::warn("WebSocket: Message queue full, dropped oldest message");
  }

  messageQueue.push(message);
}

void WebSocketClient::flushMessageQueue() {
  std::lock_guard<std::mutex> lock(queueMutex);

  Log::debug("WebSocket: Flushing " + juce::String(static_cast<int>(messageQueue.size())) + " queued messages");

  while (!messageQueue.empty()) {
    auto &msg = messageQueue.front();
    send(msg); // This will send via websocketpp
    messageQueue.pop();
  }
}

void WebSocketClient::clearMessageQueue() {
  std::lock_guard<std::mutex> lock(queueMutex);
  while (!messageQueue.empty())
    messageQueue.pop();
}

//==============================================================================
void WebSocketClient::setState(ConnectionState newState) {
  auto previous = state.exchange(newState);
  if (previous != newState) {
    Log::debug("WebSocket state: " + juce::String(newState == ConnectionState::Disconnected ? "Disconnected"
                                                  : newState == ConnectionState::Connecting ? "Connecting"
                                                  : newState == ConnectionState::Connected  ? "Connected"
                                                                                            : "Reconnecting"));

    juce::MessageManager::callAsync([this, newState]() {
      if (onStateChanged)
        onStateChanged(newState);
    });
  }
}

//==============================================================================
juce::String WebSocketClient::buildUri() const {
  juce::String scheme = config.useTLS ? "wss://" : "ws://";
  juce::String uri = scheme + config.host;

  if ((config.useTLS && config.port != 443) || (!config.useTLS && config.port != 80)) {
    uri += ":" + juce::String(config.port);
  }

  uri += config.path;

  // Add auth token as query parameter
  if (!authToken.isEmpty()) {
    uri +=
        (config.path.contains("?") ? "&" : "?") + juce::String("token=") + juce::URL::addEscapeChars(authToken, true);
  }

  return uri;
}
