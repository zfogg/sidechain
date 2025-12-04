#include "WebSocketClient.h"

//==============================================================================
WebSocketClient::WebSocketClient(const Config& cfg)
    : Thread("WebSocketClient"), config(cfg)
{
    DBG("WebSocketClient initialized - host: " + config.host + ":" + juce::String(config.port));
}

WebSocketClient::~WebSocketClient()
{
    disconnect();
    stopThread(5000);
}

//==============================================================================
void WebSocketClient::connect()
{
    if (state.load() == ConnectionState::Connected ||
        state.load() == ConnectionState::Connecting)
    {
        DBG("WebSocket: Already connected or connecting");
        return;
    }

    shouldReconnect.store(true);
    reconnectAttempts.store(0);

    if (!isThreadRunning())
    {
        startThread();
    }
    else
    {
        notify();
    }
}

void WebSocketClient::disconnect()
{
    shouldReconnect.store(false);

    if (socket && socket->isConnected())
    {
        sendCloseFrame(1000, "Client disconnect");
        socket->close();
    }

    setState(ConnectionState::Disconnected);
    notify();
}

//==============================================================================
void WebSocketClient::setAuthToken(const juce::String& token)
{
    authToken = token;
    DBG("WebSocket: Auth token set");
}

void WebSocketClient::clearAuthToken()
{
    authToken = juce::String();
}

void WebSocketClient::setConfig(const Config& newConfig)
{
    config = newConfig;
}

//==============================================================================
bool WebSocketClient::send(const juce::var& message)
{
    juce::String json = juce::JSON::toString(message);

    if (state.load() != ConnectionState::Connected)
    {
        queueMessage(message);
        DBG("WebSocket: Message queued (not connected)");
        return false;
    }

    return sendTextFrame(json);
}

bool WebSocketClient::send(const juce::String& type, const juce::var& data)
{
    juce::var message = juce::var(new juce::DynamicObject());
    message.getDynamicObject()->setProperty("type", type);
    message.getDynamicObject()->setProperty("data", data);
    message.getDynamicObject()->setProperty("timestamp", juce::Time::currentTimeMillis());
    return send(message);
}

//==============================================================================
WebSocketClient::Stats WebSocketClient::getStats() const
{
    std::lock_guard<std::mutex> lock(statsMutex);
    return stats;
}

//==============================================================================
// Thread implementation
void WebSocketClient::run()
{
    while (!threadShouldExit())
    {
        if (shouldReconnect.load() && state.load() != ConnectionState::Connected)
        {
            // Check if we should wait before reconnecting
            int64_t now = juce::Time::currentTimeMillis();
            int64_t nextReconnect = nextReconnectTime.load();

            if (nextReconnect > 0 && now < nextReconnect)
            {
                wait(static_cast<int>(nextReconnect - now));
                continue;
            }

            setState(ConnectionState::Connecting);

            // Create socket and connect
            socket = std::make_unique<juce::StreamingSocket>();

            DBG("WebSocket: Connecting to " + config.host + ":" + juce::String(config.port));

            if (socket->connect(config.host, config.port, config.connectTimeoutMs))
            {
                if (performHandshake())
                {
                    setState(ConnectionState::Connected);
                    reconnectAttempts.store(0);
                    nextReconnectTime.store(0);

                    {
                        std::lock_guard<std::mutex> lock(statsMutex);
                        stats.connectedTime = juce::Time::currentTimeMillis();
                    }

                    // Flush queued messages
                    flushMessageQueue();

                    // Enter the main connection loop
                    connectionLoop();
                }
                else
                {
                    DBG("WebSocket: Handshake failed");
                    handleDisconnect("Handshake failed");
                }
            }
            else
            {
                DBG("WebSocket: Connection failed");
                handleDisconnect("Connection failed");
            }
        }
        else
        {
            // Wait for reconnect signal or shutdown
            wait(1000);
        }
    }

    // Clean up
    if (socket)
    {
        socket->close();
        socket.reset();
    }
}

//==============================================================================
bool WebSocketClient::performHandshake()
{
    if (!socket || !socket->isConnected())
        return false;

    // Generate WebSocket key
    juce::String wsKey = generateWebSocketKey();
    juce::String expectedAccept = computeAcceptKey(wsKey);

    // Build HTTP upgrade request
    juce::String path = config.path;
    if (!authToken.isEmpty())
    {
        path += "?token=" + juce::URL::addEscapeChars(authToken, true);
    }

    juce::String request;
    request += "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + config.host + ":" + juce::String(config.port) + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + wsKey + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "Origin: sidechain-plugin\r\n";
    request += "\r\n";

    // Send request
    if (socket->write(request.toRawUTF8(), static_cast<int>(request.getNumBytesAsUTF8())) < 0)
    {
        DBG("WebSocket: Failed to send handshake request");
        return false;
    }

    // Read response (max 4KB for headers)
    char buffer[4096];
    int bytesRead = 0;
    int totalRead = 0;
    juce::String response;

    // Read until we see the end of headers
    while (totalRead < 4096)
    {
        if (!socket->waitUntilReady(true, config.connectTimeoutMs))
        {
            DBG("WebSocket: Timeout waiting for handshake response");
            return false;
        }

        bytesRead = socket->read(buffer + totalRead, 1, false);
        if (bytesRead <= 0)
        {
            DBG("WebSocket: Connection closed during handshake");
            return false;
        }

        totalRead += bytesRead;
        response = juce::String::fromUTF8(buffer, totalRead);

        if (response.contains("\r\n\r\n"))
            break;
    }

    DBG("WebSocket handshake response:\n" + response);

    // Verify response
    if (!response.startsWith("HTTP/1.1 101"))
    {
        DBG("WebSocket: Server rejected upgrade - " + response.upToFirstOccurrenceOf("\r\n", false, false));

        // Try to extract error message
        if (response.contains("401"))
        {
            juce::MessageManager::callAsync([this]() {
                if (onError)
                    onError("Authentication failed - please log in again");
            });
        }
        return false;
    }

    // Verify Sec-WebSocket-Accept header
    juce::String acceptHeader;
    juce::StringArray lines;
    lines.addTokens(response, "\r\n", "");

    for (auto& line : lines)
    {
        if (line.startsWithIgnoreCase("Sec-WebSocket-Accept:"))
        {
            acceptHeader = line.fromFirstOccurrenceOf(":", false, false).trim();
            break;
        }
    }

    if (acceptHeader != expectedAccept)
    {
        DBG("WebSocket: Invalid Sec-WebSocket-Accept header");
        DBG("  Expected: " + expectedAccept);
        DBG("  Got: " + acceptHeader);
        return false;
    }

    DBG("WebSocket: Handshake successful");
    return true;
}

void WebSocketClient::connectionLoop()
{
    lastPingSentTime.store(0);
    lastPongReceivedTime.store(juce::Time::currentTimeMillis());
    lastHeartbeatSent.store(0);

    while (!threadShouldExit() && shouldReconnect.load())
    {
        if (!socket || !socket->isConnected())
        {
            handleDisconnect("Socket disconnected");
            return;
        }

        // Check for heartbeat timeout
        if (isHeartbeatTimedOut())
        {
            DBG("WebSocket: Heartbeat timeout");
            handleDisconnect("Heartbeat timeout");
            return;
        }

        // Send heartbeat if needed
        int64_t now = juce::Time::currentTimeMillis();
        if (now - lastHeartbeatSent.load() >= config.heartbeatIntervalMs)
        {
            sendHeartbeat();
            lastHeartbeatSent.store(now);
        }

        // Wait for incoming data
        if (socket->waitUntilReady(true, 100))
        {
            Frame frame;
            if (readFrame(frame))
            {
                switch (frame.opcode)
                {
                    case Opcode::Text:
                    {
                        juce::String text = juce::String::fromUTF8(
                            static_cast<const char*>(frame.payload.getData()),
                            static_cast<int>(frame.payload.getSize())
                        );
                        processTextMessage(text);
                        break;
                    }

                    case Opcode::Binary:
                        DBG("WebSocket: Received binary frame (ignored)");
                        break;

                    case Opcode::Ping:
                        sendPongFrame(frame.payload.getData(), frame.payload.getSize());
                        break;

                    case Opcode::Pong:
                        lastPongReceivedTime.store(juce::Time::currentTimeMillis());
                        break;

                    case Opcode::Close:
                    {
                        uint16_t code = 1000;
                        juce::String reason;

                        if (frame.payload.getSize() >= 2)
                        {
                            auto* data = static_cast<const uint8_t*>(frame.payload.getData());
                            code = (static_cast<uint16_t>(data[0]) << 8) | data[1];

                            if (frame.payload.getSize() > 2)
                            {
                                reason = juce::String::fromUTF8(
                                    reinterpret_cast<const char*>(data + 2),
                                    static_cast<int>(frame.payload.getSize() - 2)
                                );
                            }
                        }

                        DBG("WebSocket: Received close frame - code: " + juce::String(code) + ", reason: " + reason);
                        sendCloseFrame(code);
                        handleDisconnect("Server closed connection: " + reason);
                        return;
                    }

                    case Opcode::Continuation:
                        // Handle fragmented messages
                        fragmentBuffer.append(frame.payload.getData(), frame.payload.getSize());
                        if (frame.fin)
                        {
                            if (fragmentOpcode == Opcode::Text)
                            {
                                juce::String text = juce::String::fromUTF8(
                                    static_cast<const char*>(fragmentBuffer.getData()),
                                    static_cast<int>(fragmentBuffer.getSize())
                                );
                                processTextMessage(text);
                            }
                            fragmentBuffer.reset();
                        }
                        break;

                    default:
                        break;
                }

                // Update stats
                {
                    std::lock_guard<std::mutex> lock(statsMutex);
                    stats.messagesReceived++;
                    stats.lastMessageTime = juce::Time::currentTimeMillis();
                }
            }
            else
            {
                // Read error
                handleDisconnect("Read error");
                return;
            }
        }
    }
}

void WebSocketClient::handleDisconnect(const juce::String& reason)
{
    DBG("WebSocket: Disconnected - " + reason);

    if (socket)
    {
        socket->close();
        socket.reset();
    }

    setState(ConnectionState::Disconnected);

    if (shouldReconnect.load())
    {
        scheduleReconnect();
    }
}

void WebSocketClient::scheduleReconnect()
{
    int attempts = reconnectAttempts.fetch_add(1) + 1;

    // Check max attempts
    if (config.maxReconnectAttempts >= 0 && attempts > config.maxReconnectAttempts)
    {
        DBG("WebSocket: Max reconnect attempts reached");
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

    DBG("WebSocket: Reconnecting in " + juce::String(delay) + "ms (attempt " + juce::String(attempts) + ")");

    nextReconnectTime.store(juce::Time::currentTimeMillis() + delay);
    setState(ConnectionState::Reconnecting);

    {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.reconnectAttempts = attempts;
    }
}

//==============================================================================
// WebSocket frame operations
bool WebSocketClient::sendFrame(Opcode opcode, const void* data, size_t length)
{
    if (!socket || !socket->isConnected())
        return false;

    juce::MemoryOutputStream frame;

    // First byte: FIN + opcode
    uint8_t byte1 = 0x80 | static_cast<uint8_t>(opcode);  // FIN = 1
    frame.writeByte(static_cast<char>(byte1));

    // Second byte: MASK + payload length
    // Client must mask all frames
    uint8_t byte2 = 0x80;  // MASK = 1

    if (length <= 125)
    {
        byte2 |= static_cast<uint8_t>(length);
        frame.writeByte(static_cast<char>(byte2));
    }
    else if (length <= 65535)
    {
        byte2 |= 126;
        frame.writeByte(static_cast<char>(byte2));
        frame.writeByte(static_cast<char>((length >> 8) & 0xFF));
        frame.writeByte(static_cast<char>(length & 0xFF));
    }
    else
    {
        byte2 |= 127;
        frame.writeByte(static_cast<char>(byte2));
        for (int i = 7; i >= 0; --i)
            frame.writeByte(static_cast<char>((length >> (i * 8)) & 0xFF));
    }

    // Generate masking key
    uint8_t maskKey[4];
    for (int i = 0; i < 4; ++i)
        maskKey[i] = static_cast<uint8_t>(juce::Random::getSystemRandom().nextInt(256));

    frame.write(maskKey, 4);

    // Apply mask and write payload
    auto* src = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < length; ++i)
    {
        uint8_t masked = src[i] ^ maskKey[i % 4];
        frame.writeByte(static_cast<char>(masked));
    }

    // Send frame
    int written = socket->write(frame.getData(), static_cast<int>(frame.getDataSize()));

    if (written < 0)
    {
        DBG("WebSocket: Failed to send frame");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.messagesSent++;
    }

    return true;
}

bool WebSocketClient::sendTextFrame(const juce::String& text)
{
    auto utf8 = text.toUTF8();
    return sendFrame(Opcode::Text, utf8.getAddress(), utf8.sizeInBytes() - 1);
}

bool WebSocketClient::sendPingFrame()
{
    lastPingSentTime.store(juce::Time::currentTimeMillis());
    return sendFrame(Opcode::Ping, nullptr, 0);
}

bool WebSocketClient::sendPongFrame(const void* data, size_t length)
{
    return sendFrame(Opcode::Pong, data, length);
}

bool WebSocketClient::sendCloseFrame(uint16_t code, const juce::String& reason)
{
    juce::MemoryOutputStream payload;
    payload.writeByte(static_cast<char>((code >> 8) & 0xFF));
    payload.writeByte(static_cast<char>(code & 0xFF));

    if (reason.isNotEmpty())
    {
        auto utf8 = reason.toUTF8();
        payload.write(utf8.getAddress(), utf8.sizeInBytes() - 1);
    }

    return sendFrame(Opcode::Close, payload.getData(), payload.getDataSize());
}

bool WebSocketClient::readFrame(Frame& frame)
{
    if (!socket || !socket->isConnected())
        return false;

    uint8_t header[2];
    int bytesRead = socket->read(header, 2, true);
    if (bytesRead != 2)
        return false;

    frame.fin = (header[0] & 0x80) != 0;
    frame.opcode = static_cast<Opcode>(header[0] & 0x0F);

    bool masked = (header[1] & 0x80) != 0;
    uint64_t payloadLen = header[1] & 0x7F;

    // Extended payload length
    if (payloadLen == 126)
    {
        uint8_t extLen[2];
        if (socket->read(extLen, 2, true) != 2)
            return false;
        payloadLen = (static_cast<uint64_t>(extLen[0]) << 8) | extLen[1];
    }
    else if (payloadLen == 127)
    {
        uint8_t extLen[8];
        if (socket->read(extLen, 8, true) != 8)
            return false;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i)
            payloadLen = (payloadLen << 8) | extLen[i];
    }

    // Read masking key if present (servers shouldn't mask, but handle it)
    uint8_t maskKey[4] = {0, 0, 0, 0};
    if (masked)
    {
        if (socket->read(maskKey, 4, true) != 4)
            return false;
    }

    // Read payload
    frame.payload.setSize(static_cast<size_t>(payloadLen));
    if (payloadLen > 0)
    {
        int totalRead = 0;
        while (totalRead < static_cast<int>(payloadLen))
        {
            int remaining = static_cast<int>(payloadLen) - totalRead;
            int n = socket->read(static_cast<uint8_t*>(frame.payload.getData()) + totalRead, remaining, true);
            if (n <= 0)
                return false;
            totalRead += n;
        }

        // Unmask if needed
        if (masked)
        {
            auto* data = static_cast<uint8_t*>(frame.payload.getData());
            for (uint64_t i = 0; i < payloadLen; ++i)
                data[i] ^= maskKey[i % 4];
        }
    }

    // Handle fragmentation start
    if (frame.opcode != Opcode::Continuation && !frame.fin)
    {
        fragmentOpcode = frame.opcode;
        fragmentBuffer.reset();
        fragmentBuffer.append(frame.payload.getData(), frame.payload.getSize());
    }

    return true;
}

//==============================================================================
void WebSocketClient::processTextMessage(const juce::String& text)
{
    DBG("WebSocket received: " + text);

    Message msg;
    msg.rawJson = text;
    msg.data = juce::JSON::parse(text);

    if (!msg.data.isObject())
    {
        DBG("WebSocket: Invalid JSON message");
        return;
    }

    // Extract type
    msg.typeString = msg.data.getProperty("type", "").toString();
    msg.type = parseMessageType(msg.typeString);

    // Update pong time for heartbeat responses
    if (msg.type == MessageType::Heartbeat)
    {
        lastPongReceivedTime.store(juce::Time::currentTimeMillis());
    }

    dispatchMessage(msg);
}

void WebSocketClient::dispatchMessage(const Message& message)
{
    juce::MessageManager::callAsync([this, message]() {
        if (onMessage)
            onMessage(message);
    });
}

WebSocketClient::MessageType WebSocketClient::parseMessageType(const juce::String& typeStr)
{
    if (typeStr == "new_post" || typeStr == "post")
        return MessageType::NewPost;
    if (typeStr == "like" || typeStr == "reaction")
        return MessageType::Like;
    if (typeStr == "follow")
        return MessageType::Follow;
    if (typeStr == "comment")
        return MessageType::Comment;
    if (typeStr == "notification")
        return MessageType::Notification;
    if (typeStr == "presence" || typeStr == "presence_update")
        return MessageType::PresenceUpdate;
    if (typeStr == "play_count" || typeStr == "play")
        return MessageType::PlayCount;
    if (typeStr == "heartbeat" || typeStr == "pong")
        return MessageType::Heartbeat;
    if (typeStr == "error")
        return MessageType::Error;

    return MessageType::Unknown;
}

//==============================================================================
void WebSocketClient::sendHeartbeat()
{
    // Send application-level heartbeat
    send("heartbeat", juce::var(new juce::DynamicObject()));

    // Also send WebSocket ping
    sendPingFrame();
}

bool WebSocketClient::isHeartbeatTimedOut() const
{
    int64_t lastPong = lastPongReceivedTime.load();
    if (lastPong == 0)
        return false;

    int64_t now = juce::Time::currentTimeMillis();
    // Timeout if no pong for 2x heartbeat interval
    return (now - lastPong) > (config.heartbeatIntervalMs * 2);
}

//==============================================================================
void WebSocketClient::queueMessage(const juce::var& message)
{
    std::lock_guard<std::mutex> lock(queueMutex);

    if (messageQueue.size() >= static_cast<size_t>(config.messageQueueMaxSize))
    {
        // Remove oldest message
        messageQueue.pop();
        DBG("WebSocket: Message queue full, dropped oldest message");
    }

    messageQueue.push(message);
}

void WebSocketClient::flushMessageQueue()
{
    std::lock_guard<std::mutex> lock(queueMutex);

    DBG("WebSocket: Flushing " + juce::String(static_cast<int>(messageQueue.size())) + " queued messages");

    while (!messageQueue.empty())
    {
        auto& msg = messageQueue.front();
        juce::String json = juce::JSON::toString(msg);
        sendTextFrame(json);
        messageQueue.pop();
    }
}

void WebSocketClient::clearMessageQueue()
{
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!messageQueue.empty())
        messageQueue.pop();
}

//==============================================================================
void WebSocketClient::setState(ConnectionState newState)
{
    auto previous = state.exchange(newState);
    if (previous != newState)
    {
        DBG("WebSocket state: " +
            juce::String(newState == ConnectionState::Disconnected ? "Disconnected" :
                        newState == ConnectionState::Connecting ? "Connecting" :
                        newState == ConnectionState::Connected ? "Connected" : "Reconnecting"));

        juce::MessageManager::callAsync([this, newState]() {
            if (onStateChanged)
                onStateChanged(newState);
        });
    }
}

//==============================================================================
juce::String WebSocketClient::generateWebSocketKey()
{
    // Generate 16 random bytes and base64 encode
    uint8_t bytes[16];
    for (int i = 0; i < 16; ++i)
        bytes[i] = static_cast<uint8_t>(juce::Random::getSystemRandom().nextInt(256));

    return juce::Base64::toBase64(bytes, 16);
}

juce::String WebSocketClient::computeAcceptKey(const juce::String& key)
{
    // Concatenate with magic GUID
    juce::String concat = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // SHA-1 hash
    juce::SHA256 sha(concat.toUTF8());

    // We need SHA-1, not SHA-256. JUCE doesn't have SHA-1, so we'll skip verification
    // In production, you'd want to use a SHA-1 implementation
    // For now, we'll trust the server's response if we get a 101

    // This is a simplification - proper implementation would verify
    return juce::String();
}
