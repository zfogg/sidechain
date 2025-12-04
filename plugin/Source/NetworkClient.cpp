#include "NetworkClient.h"

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
            callback(false, "");
        return;
    }
    
    juce::Thread::launch([this, recordingId, audioBuffer, sampleRate, callback]() {
        // Encode audio to MP3
        auto mp3Data = encodeAudioToMP3(audioBuffer, sampleRate);
        
        if (mp3Data.getSize() == 0)
        {
            DBG("Failed to encode audio to MP3");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(false, "");
                });
            }
            return;
        }
        
        // Create multipart form data (simplified for now)
        juce::var uploadData = juce::var(new juce::DynamicObject());
        uploadData.getDynamicObject()->setProperty("recording_id", recordingId);
        uploadData.getDynamicObject()->setProperty("bpm", 120); // TODO: detect from DAW
        uploadData.getDynamicObject()->setProperty("key", "C major"); // TODO: detect
        uploadData.getDynamicObject()->setProperty("daw", "Ableton Live"); // TODO: detect
        uploadData.getDynamicObject()->setProperty("duration_bars", 8);
        
        auto response = makeRequest("/api/v1/audio/upload", "POST", uploadData, true);
        
        bool success = false;
        juce::String audioUrl;
        
        if (response.isObject())
        {
            success = response.getProperty("status", "") == "uploaded";
            audioUrl = response.getProperty("audio_url", "").toString();
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
            DBG("Audio upload failed");
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

        // Create request options
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders(headers)
                           .withConnectionTimeoutMs(config.timeoutMs);

        // For POST requests, add data to URL
        if (method == "POST" || method == "PUT")
        {
            if (!data.isVoid())
            {
                juce::String jsonData = juce::JSON::toString(data);
                url = url.withPOSTData(jsonData);
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

        // Parse JSON response
        result.data = juce::JSON::parse(response);
        result.success = true;
        result.httpStatus = 200; // JUCE URL doesn't expose status codes easily

        DBG("API Response from " + endpoint + ": " + response);

        // Update connection status on success
        updateConnectionStatus(ConnectionStatus::Connected);

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
    // TODO: Implement MP3 encoding
    // For now, return empty block - we'll implement this with a proper encoder later
    // This would typically use LAME encoder or similar

    DBG("Audio encoding requested: " + juce::String(buffer.getNumSamples()) + " samples at " + juce::String(sampleRate) + "Hz");

    return juce::MemoryBlock();
}