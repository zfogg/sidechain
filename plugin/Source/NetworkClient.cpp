#include "NetworkClient.h"

//==============================================================================
NetworkClient::NetworkClient(const juce::String& baseUrl)
    : baseUrl(baseUrl)
{
    DBG("NetworkClient initialized with base URL: " + baseUrl);
}

NetworkClient::~NetworkClient()
{
}

//==============================================================================
void NetworkClient::registerDevice(DeviceRegistrationCallback callback)
{
    // Make async request to register device
    juce::Thread::launch([this, callback]() {
        auto response = makeRequest("/api/v1/auth/device", "POST");
        
        if (response.isObject())
        {
            auto deviceId = response.getProperty("device_id", "").toString();
            if (!deviceId.isEmpty())
            {
                currentDeviceId = deviceId;
                
                // Call callback on message thread
                juce::MessageManager::callAsync([callback, deviceId]() {
                    callback(deviceId);
                });
            }
        }
    });
}

void NetworkClient::verifyDeviceToken(const juce::String& deviceId)
{
    juce::Thread::launch([this, deviceId]() {
        auto response = makeRequest("/api/v1/auth/verify", "POST");
        
        if (response.isObject())
        {
            handleAuthResponse(response);
        }
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
        juce::var uploadData;
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
        
        DBG(success ? "Audio uploaded successfully: " + audioUrl : "Audio upload failed");
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
        juce::var data;
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
        juce::var data;
        data.getDynamicObject()->setProperty("target_user_id", userId);
        
        auto response = makeRequest("/api/v1/social/follow", "POST", data, true);
        DBG("Follow response: " + juce::JSON::toString(response));
    });
}

//==============================================================================
void NetworkClient::setAuthToken(const juce::String& token)
{
    authToken = token;
}

juce::var NetworkClient::makeRequest(const juce::String& endpoint, 
                                    const juce::String& method,
                                    const juce::var& data,
                                    bool requireAuth)
{
    juce::URL url(baseUrl + endpoint);
    
    // Create request headers
    juce::StringPairArray headers;
    headers.set("Content-Type", "application/json");
    
    if (requireAuth && !authToken.isEmpty())
    {
        headers.set("Authorization", authToken);
    }
    
    // Create request options
    juce::URL::InputStreamOptions options(juce::URL::ParameterHandling::inAddress);
    options.withExtraHeaders(headers);
    
    if (method == "POST" || method == "PUT")
    {
        options.withConnectionTimeoutMs(10000);
        
        if (!data.isVoid())
        {
            juce::String jsonData = juce::JSON::toString(data);
            options.withDataToSend(jsonData.toUTF8(), jsonData.length());
        }
    }
    
    // Make request
    auto stream = url.createInputStream(options);
    
    if (stream == nullptr)
    {
        DBG("Failed to create stream for: " + url.toString(false));
        return juce::var();
    }
    
    auto response = stream->readEntireStreamAsString();
    
    // Parse JSON response
    auto result = juce::JSON::parse(response);
    DBG("API Response from " + endpoint + ": " + response);
    
    return result;
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