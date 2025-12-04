#include "NetworkClient.h"

//==============================================================================
// RequestResult helper methods
juce::String NetworkClient::RequestResult::getUserFriendlyError() const
{
    // Try to extract error message from JSON response
    if (data.isObject())
    {
        // Check common error field names
        auto error = data.getProperty("error", juce::var());
        if (error.isString())
            return error.toString();

        auto message = data.getProperty("message", juce::var());
        if (message.isString())
            return message.toString();

        // Nested error object
        if (error.isObject())
        {
            auto errorMsg = error.getProperty("message", juce::var());
            if (errorMsg.isString())
                return errorMsg.toString();
        }
    }

    // Fall back to HTTP status-based messages
    switch (httpStatus)
    {
        case 400: return "Invalid request - please check your input";
        case 401: return "Authentication required - please log in";
        case 403: return "Access denied - you don't have permission";
        case 404: return "Not found - the requested resource doesn't exist";
        case 409: return "Conflict - this action conflicts with existing data";
        case 422: return "Validation failed - please check your input";
        case 429: return "Too many requests - please try again later";
        case 500: return "Server error - please try again later";
        case 502: return "Server unavailable - please try again later";
        case 503: return "Service temporarily unavailable";
        default:
            if (!errorMessage.isEmpty())
                return errorMessage;
            if (httpStatus >= 400)
                return "Request failed (HTTP " + juce::String(httpStatus) + ")";
            return "Unknown error occurred";
    }
}

int NetworkClient::parseStatusCode(const juce::StringPairArray& headers)
{
    // JUCE stores the status line in a special key
    for (auto& key : headers.getAllKeys())
    {
        if (key.startsWithIgnoreCase("HTTP/"))
        {
            // Parse "HTTP/1.1 200 OK" format
            auto statusLine = headers[key];
            auto parts = juce::StringArray::fromTokens(statusLine, " ", "");
            if (parts.size() >= 2)
                return parts[1].getIntValue();
        }
    }
    return 0;
}

//==============================================================================
NetworkClient::NetworkClient(const Config& cfg)
    : config(cfg)
{
    DBG("NetworkClient initialized with base URL: " + config.baseUrl);
    DBG("  Timeout: " + juce::String(config.timeoutMs) + "ms, Max retries: " + juce::String(config.maxRetries));
}

NetworkClient::~NetworkClient()
{
    cancelAllRequests();
}

//==============================================================================
void NetworkClient::setConnectionStatusCallback(ConnectionStatusCallback callback)
{
    connectionStatusCallback = callback;
}

void NetworkClient::updateConnectionStatus(ConnectionStatus status)
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

void NetworkClient::checkConnection()
{
    updateConnectionStatus(ConnectionStatus::Connecting);

    juce::Thread::launch([this]() {
        if (shuttingDown.load())
            return;

        auto result = makeRequestWithRetry("/health", "GET", juce::var(), false);

        if (result.success)
        {
            updateConnectionStatus(ConnectionStatus::Connected);
            DBG("Connection check: Connected to backend");
        }
        else
        {
            updateConnectionStatus(ConnectionStatus::Disconnected);
            DBG("Connection check: Failed - " + result.errorMessage);
        }
    });
}

void NetworkClient::cancelAllRequests()
{
    shuttingDown.store(true);
    // Wait for active requests to complete (with timeout)
    int waitCount = 0;
    while (activeRequestCount.load() > 0 && waitCount < 50)
    {
        juce::Thread::sleep(100);
        waitCount++;
    }
    shuttingDown.store(false);
}

void NetworkClient::setConfig(const Config& newConfig)
{
    config = newConfig;
    DBG("NetworkClient config updated - base URL: " + config.baseUrl);
}

//==============================================================================
void NetworkClient::registerAccount(const juce::String& email, const juce::String& username, 
                                   const juce::String& password, const juce::String& displayName,
                                   AuthenticationCallback callback)
{
    juce::Thread::launch([this, email, username, password, displayName, callback]() {
        juce::var registerData = juce::var(new juce::DynamicObject());
        registerData.getDynamicObject()->setProperty("email", email);
        registerData.getDynamicObject()->setProperty("username", username);
        registerData.getDynamicObject()->setProperty("password", password);
        registerData.getDynamicObject()->setProperty("display_name", displayName);
        
        auto response = makeRequest("/api/v1/auth/register", "POST", registerData, false);
        
        if (response.isObject())
        {
            auto authData = response.getProperty("auth", juce::var());
            if (authData.isObject())
            {
                auto token = authData.getProperty("token", "").toString();
                auto user = authData.getProperty("user", juce::var());
                
                if (!token.isEmpty() && user.isObject())
                {
                    auto userId = user.getProperty("id", "").toString();
                    auto responseUsername = user.getProperty("username", "").toString();
                    
                    // Store authentication info
                    authToken = token;
                    currentUserId = userId;
                    currentUsername = responseUsername;
                    
                    // Call callback on message thread
                    juce::MessageManager::callAsync([callback, token, userId]() {
                        callback(token, userId);
                    });
                    
                    DBG("Account registered successfully: " + responseUsername);
                    return;
                }
            }
        }
        
        // Registration failed
        juce::MessageManager::callAsync([callback]() {
            callback("", "");
        });
        DBG("Account registration failed");
    });
}

void NetworkClient::loginAccount(const juce::String& email, const juce::String& password, 
                                AuthenticationCallback callback)
{
    juce::Thread::launch([this, email, password, callback]() {
        juce::var loginData = juce::var(new juce::DynamicObject());
        loginData.getDynamicObject()->setProperty("email", email);
        loginData.getDynamicObject()->setProperty("password", password);
        
        auto response = makeRequest("/api/v1/auth/login", "POST", loginData, false);
        
        if (response.isObject())
        {
            auto authData = response.getProperty("auth", juce::var());
            if (authData.isObject())
            {
                auto token = authData.getProperty("token", "").toString();
                auto user = authData.getProperty("user", juce::var());
                
                if (!token.isEmpty() && user.isObject())
                {
                    auto userId = user.getProperty("id", "").toString();
                    auto username = user.getProperty("username", "").toString();
                    
                    // Store authentication info
                    authToken = token;
                    currentUserId = userId;
                    currentUsername = username;
                    
                    // Call callback on message thread
                    juce::MessageManager::callAsync([callback, token, userId]() {
                        callback(token, userId);
                    });
                    
                    DBG("Login successful: " + username);
                    return;
                }
            }
        }
        
        // Login failed
        juce::MessageManager::callAsync([callback]() {
            callback("", "");
        });
        DBG("Login failed");
    });
}

void NetworkClient::setAuthenticationCallback(AuthenticationCallback callback)
{
    authCallback = callback;
}

//==============================================================================
void NetworkClient::uploadAudio(const juce::String& recordingId,
                                const juce::AudioBuffer<float>& audioBuffer,
                                double sampleRate,
                                UploadCallback callback)
{
    if (!isAuthenticated())
    {
        DBG("Cannot upload audio: not authenticated");
        if (callback)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(false, "");
            });
        }
        return;
    }

    // Copy the buffer for the background thread
    juce::AudioBuffer<float> bufferCopy(audioBuffer);

    juce::Thread::launch([this, recordingId, bufferCopy, sampleRate, callback]() {
        // Encode audio to WAV (server will transcode to MP3)
        auto audioData = encodeAudioToWAV(bufferCopy, sampleRate);

        if (audioData.getSize() == 0)
        {
            DBG("Failed to encode audio");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(false, "");
                });
            }
            return;
        }

        // Calculate duration in seconds
        double durationSecs = static_cast<double>(bufferCopy.getNumSamples()) / sampleRate;

        // Build metadata fields for multipart upload
        std::map<juce::String, juce::String> metadata;
        metadata["recording_id"] = recordingId;
        metadata["bpm"] = "120";  // TODO: detect from DAW or user input
        metadata["key"] = "C major";  // TODO: detect or user input
        metadata["daw"] = "Unknown";  // TODO: detect from host
        metadata["duration_bars"] = "8";  // TODO: calculate from BPM and duration
        metadata["duration_seconds"] = juce::String(durationSecs, 2);
        metadata["sample_rate"] = juce::String(static_cast<int>(sampleRate));
        metadata["channels"] = juce::String(bufferCopy.getNumChannels());

        // Generate filename
        juce::String fileName = recordingId + ".wav";

        // Upload using multipart form data
        auto result = uploadMultipartData(
            "/api/v1/audio/upload",
            "audio_file",
            audioData,
            fileName,
            "audio/wav",
            metadata
        );

        bool success = result.success;
        juce::String audioUrl;

        if (result.data.isObject())
        {
            audioUrl = result.data.getProperty("audio_url", "").toString();
            if (audioUrl.isEmpty())
                audioUrl = result.data.getProperty("url", "").toString();
        }

        if (callback)
        {
            juce::MessageManager::callAsync([callback, success, audioUrl]() {
                callback(success, audioUrl);
            });
        }

        if (success)
            DBG("Audio uploaded successfully: " + audioUrl);
        else
            DBG("Audio upload failed: " + result.getUserFriendlyError());
    });
}

//==============================================================================
void NetworkClient::uploadAudioWithMetadata(const juce::AudioBuffer<float>& audioBuffer,
                                             double sampleRate,
                                             const AudioUploadMetadata& metadata,
                                             UploadCallback callback)
{
    if (!isAuthenticated())
    {
        DBG("Cannot upload audio: not authenticated");
        if (callback)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(false, "");
            });
        }
        return;
    }

    // Copy the buffer and metadata for the background thread
    juce::AudioBuffer<float> bufferCopy(audioBuffer);
    AudioUploadMetadata metadataCopy = metadata;

    juce::Thread::launch([this, bufferCopy, sampleRate, metadataCopy, callback]() {
        // Encode audio to WAV (server will transcode to MP3)
        auto audioData = encodeAudioToWAV(bufferCopy, sampleRate);

        if (audioData.getSize() == 0)
        {
            DBG("Failed to encode audio");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(false, "");
                });
            }
            return;
        }

        // Generate unique recording ID
        juce::String recordingId = juce::Uuid().toString();

        // Calculate duration
        double durationSecs = static_cast<double>(bufferCopy.getNumSamples()) / sampleRate;

        // Build metadata fields for multipart upload
        std::map<juce::String, juce::String> fields;
        fields["recording_id"] = recordingId;
        fields["title"] = metadataCopy.title;

        if (metadataCopy.bpm > 0)
            fields["bpm"] = juce::String(metadataCopy.bpm, 1);

        if (metadataCopy.key.isNotEmpty())
            fields["key"] = metadataCopy.key;

        if (metadataCopy.genre.isNotEmpty())
            fields["genre"] = metadataCopy.genre;

        fields["duration_seconds"] = juce::String(durationSecs, 2);
        fields["sample_rate"] = juce::String(static_cast<int>(sampleRate));
        fields["channels"] = juce::String(bufferCopy.getNumChannels());

        // Calculate approximate bar count if BPM is known
        if (metadataCopy.bpm > 0)
        {
            double beatsPerSecond = metadataCopy.bpm / 60.0;
            double totalBeats = durationSecs * beatsPerSecond;
            int bars = static_cast<int>(totalBeats / 4.0 + 0.5); // Assuming 4/4 time
            fields["duration_bars"] = juce::String(juce::jmax(1, bars));
        }

        // Generate filename
        juce::String safeTitle = metadataCopy.title.replaceCharacters(" /\\:*?\"<>|", "-----------");
        juce::String fileName = safeTitle + "-" + recordingId.substring(0, 8) + ".wav";

        // Upload using multipart form data
        auto result = uploadMultipartData(
            "/api/v1/audio/upload",
            "audio_file",
            audioData,
            fileName,
            "audio/wav",
            fields
        );

        bool success = result.success;
        juce::String audioUrl;

        if (result.data.isObject())
        {
            audioUrl = result.data.getProperty("audio_url", "").toString();
            if (audioUrl.isEmpty())
                audioUrl = result.data.getProperty("url", "").toString();
        }

        if (callback)
        {
            juce::MessageManager::callAsync([callback, success, audioUrl]() {
                callback(success, audioUrl);
            });
        }

        if (success)
            DBG("Audio with metadata uploaded successfully: " + audioUrl);
        else
            DBG("Audio upload failed: " + result.getUserFriendlyError());
    });
}

//==============================================================================
void NetworkClient::getGlobalFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;
        
    juce::Thread::launch([this, limit, offset, callback]() {
        juce::String endpoint = "/api/v1/feed/global?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);
        
        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                callback(response);
            });
        }
    });
}

void NetworkClient::getTimelineFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;
        
    juce::Thread::launch([this, limit, offset, callback]() {
        juce::String endpoint = "/api/v1/feed/timeline?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);
        
        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                callback(response);
            });
        }
    });
}

void NetworkClient::likePost(const juce::String& activityId, const juce::String& emoji)
{
    if (!isAuthenticated())
        return;
        
    juce::Thread::launch([this, activityId, emoji]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("activity_id", activityId);
        
        juce::String endpoint;
        if (!emoji.isEmpty())
        {
            // Use emoji reaction endpoint
            data.getDynamicObject()->setProperty("emoji", emoji);
            endpoint = "/api/v1/social/react";
        }
        else
        {
            // Use standard like endpoint
            endpoint = "/api/v1/social/like";
        }
        
        auto response = makeRequest(endpoint, "POST", data, true);
        DBG("Like/reaction response: " + juce::JSON::toString(response));
    });
}

void NetworkClient::followUser(const juce::String& userId)
{
    if (!isAuthenticated())
        return;

    juce::Thread::launch([this, userId]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("target_user_id", userId);

        auto response = makeRequest("/api/v1/social/follow", "POST", data, true);
        DBG("Follow response: " + juce::JSON::toString(response));
    });
}

//==============================================================================
void NetworkClient::uploadProfilePicture(const juce::File& imageFile, ProfilePictureCallback callback)
{
    if (!isAuthenticated())
    {
        DBG("Cannot upload profile picture: not authenticated");
        if (callback)
            callback(false, "");
        return;
    }

    if (!imageFile.existsAsFile())
    {
        DBG("Profile picture file does not exist: " + imageFile.getFullPathName());
        if (callback)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(false, "");
            });
        }
        return;
    }

    juce::Thread::launch([this, imageFile, callback]() {
        // Helper function for MIME types
        auto getMimeType = [](const juce::String& extension) -> juce::String {
            juce::String ext = extension.toLowerCase();
            if (ext == ".jpg" || ext == ".jpeg")
                return "image/jpeg";
            else if (ext == ".png")
                return "image/png";
            else if (ext == ".gif")
                return "image/gif";
            else if (ext == ".webp")
                return "image/webp";
            else
                return "application/octet-stream";
        };

        // Read the image file
        juce::MemoryBlock imageData;
        if (!imageFile.loadFileAsData(imageData) || imageData.getSize() == 0)
        {
            DBG("Failed to read image file");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(false, "");
                });
            }
            return;
        }

        // Create multipart form data manually
        juce::String boundary = "----SidechainBoundary" + juce::String(juce::Random::getSystemRandom().nextInt64());
        juce::MemoryOutputStream formData;

        // Add file field
        formData.writeString("--" + boundary + "\r\n");
        formData.writeString("Content-Disposition: form-data; name=\"profile_picture\"; filename=\"" + imageFile.getFileName() + "\"\r\n");
        formData.writeString("Content-Type: " + getMimeType(imageFile.getFileExtension()) + "\r\n\r\n");
        formData.write(imageData.getData(), imageData.getSize());
        formData.writeString("\r\n");
        formData.writeString("--" + boundary + "--\r\n");

        // Create URL
        juce::URL url(config.baseUrl + "/api/v1/users/upload-profile-picture");

        // Build headers
        juce::String headers = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
        headers += "Authorization: Bearer " + authToken + "\r\n";

        // Create request options
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders(headers)
                           .withConnectionTimeoutMs(config.timeoutMs);

        // Add form data to URL
        url = url.withPOSTData(formData.getMemoryBlock());

        // Make request
        auto stream = url.createInputStream(options);

        if (stream == nullptr)
        {
            DBG("Failed to create stream for profile picture upload");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(false, "");
                });
            }
            return;
        }

        auto response = stream->readEntireStreamAsString();
        DBG("Profile picture upload response: " + response);

        // Parse response
        auto result = juce::JSON::parse(response);
        bool success = false;
        juce::String pictureUrl;

        if (result.isObject())
        {
            pictureUrl = result.getProperty("url", "").toString();
            success = !pictureUrl.isEmpty();
        }

        if (callback)
        {
            juce::MessageManager::callAsync([callback, success, pictureUrl]() {
                callback(success, pictureUrl);
            });
        }

        if (success)
            DBG("Profile picture uploaded successfully: " + pictureUrl);
        else
            DBG("Profile picture upload failed");
    });
}

//==============================================================================
void NetworkClient::setAuthToken(const juce::String& token)
{
    authToken = token;
}

//==============================================================================
// Core request method with retry logic
NetworkClient::RequestResult NetworkClient::makeRequestWithRetry(const juce::String& endpoint,
                                                                  const juce::String& method,
                                                                  const juce::var& data,
                                                                  bool requireAuth)
{
    RequestResult result;
    int attempts = 0;

    while (attempts < config.maxRetries && !shuttingDown.load())
    {
        attempts++;
        activeRequestCount++;

        juce::URL url(config.baseUrl + endpoint);

        // Build headers string
        juce::String headers = "Content-Type: application/json\r\n";

        if (requireAuth && !authToken.isEmpty())
        {
            headers += "Authorization: Bearer " + authToken + "\r\n";
        }

        // Create request options with response headers capture
        juce::StringPairArray responseHeaders;
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders(headers)
                           .withConnectionTimeoutMs(config.timeoutMs)
                           .withResponseHeaders(&responseHeaders);

        // For POST/PUT/DELETE requests, add data to URL
        if (method == "POST" || method == "PUT" || method == "DELETE")
        {
            if (!data.isVoid())
            {
                juce::String jsonData = juce::JSON::toString(data);
                url = url.withPOSTData(jsonData);
            }
            else if (method == "POST")
            {
                // Empty POST body
                url = url.withPOSTData("");
            }
        }

        // Make request
        auto stream = url.createInputStream(options);

        activeRequestCount--;

        if (shuttingDown.load())
        {
            result.errorMessage = "Request cancelled";
            return result;
        }

        if (stream == nullptr)
        {
            result.errorMessage = "Failed to connect to server";
            DBG("Request attempt " + juce::String(attempts) + "/" + juce::String(config.maxRetries)
                + " failed for: " + endpoint);

            if (attempts < config.maxRetries)
            {
                // Wait before retry with exponential backoff
                int delay = config.retryDelayMs * attempts;
                juce::Thread::sleep(delay);
                continue;
            }

            updateConnectionStatus(ConnectionStatus::Disconnected);
            return result;
        }

        auto response = stream->readEntireStreamAsString();

        // Store response headers and extract status code
        result.responseHeaders = responseHeaders;
        result.httpStatus = parseStatusCode(responseHeaders);

        // If we couldn't parse status code, assume 200 for successful stream
        if (result.httpStatus == 0)
            result.httpStatus = 200;

        // Parse JSON response
        result.data = juce::JSON::parse(response);
        result.success = result.isSuccess();

        DBG("API Response from " + endpoint + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

        // Check for server errors that should trigger retry
        if (result.httpStatus >= 500 && attempts < config.maxRetries)
        {
            DBG("Server error, retrying...");
            int delay = config.retryDelayMs * attempts;
            juce::Thread::sleep(delay);
            continue;
        }

        // Update connection status based on result
        if (result.httpStatus >= 200 && result.httpStatus < 500)
        {
            updateConnectionStatus(ConnectionStatus::Connected);
        }
        else
        {
            updateConnectionStatus(ConnectionStatus::Disconnected);
        }

        return result;
    }

    return result;
}

juce::var NetworkClient::makeRequest(const juce::String& endpoint,
                                    const juce::String& method,
                                    const juce::var& data,
                                    bool requireAuth)
{
    auto result = makeRequestWithRetry(endpoint, method, data, requireAuth);
    return result.data;
}

juce::String NetworkClient::getAuthHeader() const
{
    return "Bearer " + authToken;
}

void NetworkClient::handleAuthResponse(const juce::var& response)
{
    if (response.isObject())
    {
        auto token = response.getProperty("token", "").toString();
        auto userId = response.getProperty("user_id", "").toString();
        
        if (!token.isEmpty() && !userId.isEmpty())
        {
            setAuthToken(token);
            
            if (authCallback)
            {
                juce::MessageManager::callAsync([this, token, userId]() {
                    authCallback(token, userId);
                });
            }
        }
    }
}

juce::MemoryBlock NetworkClient::encodeAudioToMP3(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // TODO: Implement MP3 encoding with LAME or similar library
    // For now, fall back to WAV which the server can transcode
    DBG("MP3 encoding not yet implemented, using WAV format");
    return encodeAudioToWAV(buffer, sampleRate);
}

juce::MemoryBlock NetworkClient::encodeAudioToWAV(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::MemoryOutputStream outputStream;

    // Create WAV format writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(&outputStream, sampleRate,
                                  static_cast<unsigned int>(buffer.getNumChannels()),
                                  16, // bits per sample
                                  {}, 0));

    if (writer == nullptr)
    {
        DBG("Failed to create WAV writer");
        return juce::MemoryBlock();
    }

    // Write audio data
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        DBG("Failed to write audio data to WAV");
        return juce::MemoryBlock();
    }

    writer.reset(); // Flush and close

    DBG("Encoded " + juce::String(buffer.getNumSamples()) + " samples at "
        + juce::String(sampleRate) + "Hz to WAV ("
        + juce::String(outputStream.getDataSize()) + " bytes)");

    return outputStream.getMemoryBlock();
}

//==============================================================================
// Multipart form data upload helper
NetworkClient::RequestResult NetworkClient::uploadMultipartData(
    const juce::String& endpoint,
    const juce::String& fieldName,
    const juce::MemoryBlock& fileData,
    const juce::String& fileName,
    const juce::String& mimeType,
    const std::map<juce::String, juce::String>& extraFields)
{
    RequestResult result;

    if (!isAuthenticated())
    {
        result.errorMessage = "Not authenticated";
        result.httpStatus = 401;
        return result;
    }

    // Generate unique boundary
    juce::String boundary = "----SidechainBoundary" + juce::String(juce::Random::getSystemRandom().nextInt64());

    // Build multipart body
    juce::MemoryOutputStream formData;

    // Add extra text fields first
    for (const auto& field : extraFields)
    {
        formData.writeString("--" + boundary + "\r\n");
        formData.writeString("Content-Disposition: form-data; name=\"" + field.first + "\"\r\n\r\n");
        formData.writeString(field.second + "\r\n");
    }

    // Add file field
    formData.writeString("--" + boundary + "\r\n");
    formData.writeString("Content-Disposition: form-data; name=\"" + fieldName + "\"; filename=\"" + fileName + "\"\r\n");
    formData.writeString("Content-Type: " + mimeType + "\r\n\r\n");
    formData.write(fileData.getData(), fileData.getSize());
    formData.writeString("\r\n");
    formData.writeString("--" + boundary + "--\r\n");

    // Create URL
    juce::URL url(config.baseUrl + endpoint);

    // Build headers
    juce::String headers = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
    headers += "Authorization: Bearer " + authToken + "\r\n";

    // Create request options with response headers capture
    juce::StringPairArray responseHeaders;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(config.timeoutMs)
                       .withResponseHeaders(&responseHeaders);

    // Add form data to URL
    url = url.withPOSTData(formData.getMemoryBlock());

    // Make request
    activeRequestCount++;
    auto stream = url.createInputStream(options);
    activeRequestCount--;

    if (stream == nullptr)
    {
        result.errorMessage = "Failed to connect to server";
        updateConnectionStatus(ConnectionStatus::Disconnected);
        return result;
    }

    auto response = stream->readEntireStreamAsString();

    // Extract status code and parse response
    result.responseHeaders = responseHeaders;
    result.httpStatus = parseStatusCode(responseHeaders);
    if (result.httpStatus == 0)
        result.httpStatus = 200;

    result.data = juce::JSON::parse(response);
    result.success = result.isSuccess();

    DBG("Multipart upload to " + endpoint + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

    updateConnectionStatus(result.success ? ConnectionStatus::Connected : ConnectionStatus::Disconnected);

    return result;
}