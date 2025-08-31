#pragma once

#include <JuceHeader.h>
#include <functional>

//==============================================================================
/**
 * NetworkClient handles all HTTP communication with the Sidechain backend
 * 
 * Features:
 * - Device registration and authentication
 * - Audio upload with metadata
 * - Social feed data fetching
 * - Real-time updates via WebSocket (future)
 */
class NetworkClient
{
public:
    NetworkClient(const juce::String& baseUrl);
    ~NetworkClient();
    
    // Callback types
    using DeviceRegistrationCallback = std::function<void(const juce::String& deviceId)>;
    using AuthenticationCallback = std::function<void(const juce::String& token, const juce::String& userId)>;
    using UploadCallback = std::function<void(bool success, const juce::String& audioUrl)>;
    using FeedCallback = std::function<void(const juce::var& feedData)>;
    
    // Authentication
    void registerDevice(DeviceRegistrationCallback callback);
    void verifyDeviceToken(const juce::String& deviceId);
    void setAuthenticationCallback(AuthenticationCallback callback);
    
    // Audio operations
    void uploadAudio(const juce::String& recordingId, 
                    const juce::AudioBuffer<float>& audioBuffer,
                    double sampleRate,
                    UploadCallback callback = nullptr);
    
    // Social feed operations
    void getGlobalFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);
    void getTimelineFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);
    void likePost(const juce::String& activityId, const juce::String& emoji = "");
    void followUser(const juce::String& userId);
    
    // Utility
    void setAuthToken(const juce::String& token);
    bool isAuthenticated() const { return !authToken.isEmpty(); }
    const juce::String& getBaseUrl() const { return baseUrl; }

private:
    juce::String baseUrl;
    juce::String authToken;
    juce::String currentDeviceId;
    
    AuthenticationCallback authCallback;
    
    // HTTP helpers
    juce::var makeRequest(const juce::String& endpoint, 
                         const juce::String& method = "GET",
                         const juce::var& data = juce::var(),
                         bool requireAuth = false);
    
    juce::String getAuthHeader() const;
    void handleAuthResponse(const juce::var& response);
    
    // Audio encoding
    juce::MemoryBlock encodeAudioToMP3(const juce::AudioBuffer<float>& buffer, double sampleRate);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkClient)
};