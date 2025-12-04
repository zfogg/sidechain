#pragma once

#include <JuceHeader.h>
#include <functional>
#include <atomic>
#include <memory>
#include <map>

//==============================================================================
/**
 * NetworkClient handles all HTTP communication with the Sidechain backend
 *
 * Features:
 * - Device registration and authentication
 * - Audio upload with metadata
 * - Social feed data fetching
 * - Retry logic for network failures
 * - Connection status monitoring
 * - Request cancellation support
 * - Real-time updates via WebSocket (future)
 */
class NetworkClient
{
public:
    //==========================================================================
    // Connection status enum for UI indicator
    enum class ConnectionStatus
    {
        Disconnected,   // Red - no connection
        Connecting,     // Yellow - attempting connection
        Connected       // Green - successfully connected
    };

    //==========================================================================
    // Configuration for different environments
    struct Config
    {
        juce::String baseUrl;
        int timeoutMs = 30000;          // Default 30 second timeout
        int maxRetries = 3;             // Retry failed requests up to 3 times
        int retryDelayMs = 1000;        // Wait 1 second between retries

        static Config development()
        {
            return { "http://localhost:8787", 30000, 3, 1000 };
        }

        static Config production()
        {
            return { "https://api.sidechain.app", 30000, 3, 2000 };
        }
    };

    NetworkClient(const Config& config = Config::development());
    ~NetworkClient();

    // Callback types
    using DeviceRegistrationCallback = std::function<void(const juce::String& deviceId)>;
    using AuthenticationCallback = std::function<void(const juce::String& token, const juce::String& userId)>;
    using UploadCallback = std::function<void(bool success, const juce::String& audioUrl)>;
    using FeedCallback = std::function<void(const juce::var& feedData)>;
    using ProfilePictureCallback = std::function<void(bool success, const juce::String& pictureUrl)>;
    using ConnectionStatusCallback = std::function<void(ConnectionStatus status)>;

    //==========================================================================
    // Metadata for audio uploads
    struct AudioUploadMetadata
    {
        juce::String title;
        double bpm = 0.0;
        juce::String key;        // e.g., "C", "Am", "F#m" or empty
        juce::String genre;      // e.g., "Electronic", "Hip-Hop"

        // Auto-populated fields
        double durationSeconds = 0.0;
        int sampleRate = 44100;
        int numChannels = 2;
    };

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

    // Upload audio with full metadata (title, BPM, key, genre)
    void uploadAudioWithMetadata(const juce::AudioBuffer<float>& audioBuffer,
                                 double sampleRate,
                                 const AudioUploadMetadata& metadata,
                                 UploadCallback callback = nullptr);
    
    // Social feed operations
    void getGlobalFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);
    void getTimelineFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);
    void likePost(const juce::String& activityId, const juce::String& emoji = "");
    void followUser(const juce::String& userId);

    // Profile operations
    void uploadProfilePicture(const juce::File& imageFile, ProfilePictureCallback callback = nullptr);

    //==========================================================================
    // Generic HTTP methods for custom API calls
    using ResponseCallback = std::function<void(bool success, const juce::var& response)>;

    void get(const juce::String& endpoint, ResponseCallback callback);
    void post(const juce::String& endpoint, const juce::var& data, ResponseCallback callback);
    void put(const juce::String& endpoint, const juce::var& data, ResponseCallback callback);
    void del(const juce::String& endpoint, ResponseCallback callback);

    // Play tracking (must be after ResponseCallback typedef)
    void trackPlay(const juce::String& activityId, ResponseCallback callback = nullptr);

    // Authentication state
    void setAuthToken(const juce::String& token);
    bool isAuthenticated() const { return !authToken.isEmpty(); }
    const juce::String& getBaseUrl() const { return config.baseUrl; }
    const juce::String& getCurrentUsername() const { return currentUsername; }
    const juce::String& getCurrentUserId() const { return currentUserId; }

    //==========================================================================
    // Connection status and management
    ConnectionStatus getConnectionStatus() const { return connectionStatus.load(); }
    void setConnectionStatusCallback(ConnectionStatusCallback callback);
    void checkConnection();  // Performs a health check request

    //==========================================================================
    // Request cancellation
    void cancelAllRequests();
    bool isShuttingDown() const { return shuttingDown.load(); }

    //==========================================================================
    // Configuration
    void setConfig(const Config& newConfig);
    const Config& getConfig() const { return config; }

private:
    Config config;
    juce::String authToken;
    juce::String currentUsername;
    juce::String currentUserId;

    AuthenticationCallback authCallback;
    ConnectionStatusCallback connectionStatusCallback;

    std::atomic<ConnectionStatus> connectionStatus { ConnectionStatus::Disconnected };
    std::atomic<bool> shuttingDown { false };
    std::atomic<int> activeRequestCount { 0 };

    //==========================================================================
    // HTTP helpers with retry logic
    struct RequestResult
    {
        juce::var data;
        int httpStatus = 0;
        bool success = false;
        juce::String errorMessage;
        juce::StringPairArray responseHeaders;

        // Helper to check if request succeeded (2xx status)
        bool isSuccess() const { return httpStatus >= 200 && httpStatus < 300; }

        // Extract user-friendly error message from response
        juce::String getUserFriendlyError() const;
    };

    RequestResult makeRequestWithRetry(const juce::String& endpoint,
                                       const juce::String& method = "GET",
                                       const juce::var& data = juce::var(),
                                       bool requireAuth = false);

    juce::var makeRequest(const juce::String& endpoint,
                         const juce::String& method = "GET",
                         const juce::var& data = juce::var(),
                         bool requireAuth = false);

    // Multipart form data upload helper
    RequestResult uploadMultipartData(const juce::String& endpoint,
                                      const juce::String& fieldName,
                                      const juce::MemoryBlock& fileData,
                                      const juce::String& fileName,
                                      const juce::String& mimeType,
                                      const std::map<juce::String, juce::String>& extraFields = {});

    juce::String getAuthHeader() const;
    void handleAuthResponse(const juce::var& response);
    void updateConnectionStatus(ConnectionStatus status);

    // Parse HTTP status code from response headers
    static int parseStatusCode(const juce::StringPairArray& headers);

    // Audio encoding
    juce::MemoryBlock encodeAudioToMP3(const juce::AudioBuffer<float>& buffer, double sampleRate);
    juce::MemoryBlock encodeAudioToWAV(const juce::AudioBuffer<float>& buffer, double sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkClient)
};