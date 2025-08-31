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
    
    // Authentication (simplified - no device claiming)
    void registerAccount(const juce::String& email, const juce::String& username, 
                        const juce::String& password, const juce::String& displayName,
                        AuthenticationCallback callback);
    void loginAccount(const juce::String& email, const juce::String& password, 
                     AuthenticationCallback callback);
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
    
    // Authentication state
    void setAuthToken(const juce::String& token);
    bool isAuthenticated() const { return !authToken.isEmpty(); }
    const juce::String& getBaseUrl() const { return baseUrl; }
    const juce::String& getCurrentUsername() const { return currentUsername; }
    const juce::String& getCurrentUserId() const { return currentUserId; }

private:
    juce::String baseUrl;
    juce::String authToken;
    juce::String currentUsername;
    juce::String currentUserId;
    
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