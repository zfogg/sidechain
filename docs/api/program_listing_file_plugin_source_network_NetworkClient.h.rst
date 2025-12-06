
.. _program_listing_file_plugin_source_network_NetworkClient.h:

Program Listing for File NetworkClient.h
========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_network_NetworkClient.h>` (``plugin/source/network/NetworkClient.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "../util/Constants.h"
   #include "../util/Result.h"
   #include <functional>
   #include <atomic>
   #include <memory>
   #include <map>
   
   //==============================================================================
   class NetworkClient
   {
   public:
       //==========================================================================
       // Connection status enum for UI indicator
   
       enum class ConnectionStatus
       {
           Disconnected,   
           Connecting,     
           Connected       
       };
   
       //==========================================================================
       // Configuration for different environments
   
       struct Config
       {
           juce::String baseUrl;                    
           int timeoutMs = Constants::Api::DEFAULT_TIMEOUT_MS;  
           int maxRetries = Constants::Api::MAX_RETRIES;         
           int retryDelayMs = Constants::Api::RETRY_DELAY_BASE_MS;  
   
           static Config development()
           {
               return { Constants::Endpoints::DEV_BASE_URL, Constants::Api::DEFAULT_TIMEOUT_MS, Constants::Api::MAX_RETRIES, Constants::Api::RETRY_DELAY_BASE_MS };
           }
   
           static Config production()
           {
               return { Constants::Endpoints::PROD_BASE_URL, Constants::Api::DEFAULT_TIMEOUT_MS, Constants::Api::MAX_RETRIES, Constants::Api::RETRY_DELAY_BASE_MS * 2 };
           }
       };
   
       NetworkClient(const Config& config = Config::development());
       ~NetworkClient();
   
       // Callback types - using Outcome<T> for type-safe error handling
       using DeviceRegistrationCallback = std::function<void(Outcome<juce::String>)>;
       // Authentication callback: returns Outcome with pair<token, userId> or error
       using AuthenticationCallback = std::function<void(Outcome<std::pair<juce::String, juce::String>>)>;
       using UploadCallback = std::function<void(Outcome<juce::String> audioUrl)>;
       using FeedCallback = std::function<void(Outcome<juce::var> feedData)>;
       using ProfilePictureCallback = std::function<void(Outcome<juce::String> pictureUrl)>;
       using ConnectionStatusCallback = std::function<void(ConnectionStatus status)>;
       using ResponseCallback = std::function<void(Outcome<juce::var> response)>;
   
       //==========================================================================
       // Metadata for audio uploads
   
       struct AudioUploadMetadata
       {
           juce::String title;           
           double bpm = 0.0;             
           juce::String key;             
           juce::String genre;           
   
           // Auto-populated fields
           double durationSeconds = 0.0; 
           int sampleRate = 44100;       
           int numChannels = 2;          
       };
   
       //==========================================================================
       // Authentication (simplified - no device claiming)
   
       void registerAccount(const juce::String& email, const juce::String& username,
                           const juce::String& password, const juce::String& displayName,
                           AuthenticationCallback callback);
   
       void loginAccount(const juce::String& email, const juce::String& password,
                        AuthenticationCallback callback);
   
       void setAuthenticationCallback(AuthenticationCallback callback);
   
       //==========================================================================
       // Audio operations
   
       void uploadAudio(const juce::String& recordingId,
                       const juce::AudioBuffer<float>& audioBuffer,
                       double sampleRate,
                       UploadCallback callback = nullptr);
   
       void uploadAudioWithMetadata(const juce::AudioBuffer<float>& audioBuffer,
                                    double sampleRate,
                                    const AudioUploadMetadata& metadata,
                                    UploadCallback callback = nullptr);
   
       //==========================================================================
       // Social feed operations (all use enriched endpoints with reaction data from getstream.io)
   
       void getGlobalFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);
   
       void getTimelineFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);
   
       void getTrendingFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);
   
       void likePost(const juce::String& activityId, const juce::String& emoji = "", ResponseCallback callback = nullptr);
   
       void unlikePost(const juce::String& activityId, ResponseCallback callback = nullptr);
   
       void followUser(const juce::String& userId, ResponseCallback callback = nullptr);
   
       void unfollowUser(const juce::String& userId, ResponseCallback callback = nullptr);
   
       //==========================================================================
       // Profile operations
   
       void uploadProfilePicture(const juce::File& imageFile, ProfilePictureCallback callback = nullptr);
   
       void changeUsername(const juce::String& newUsername, ResponseCallback callback = nullptr);
   
       void getFollowers(const juce::String& userId, int limit = 20, int offset = 0, ResponseCallback callback = nullptr);
   
       void getFollowing(const juce::String& userId, int limit = 20, int offset = 0, ResponseCallback callback = nullptr);
   
       //==========================================================================
       // Comment operations
       using CommentCallback = std::function<void(Outcome<juce::var> comment)>;
       // CommentsListCallback: returns Outcome with pair<comments, totalCount> or error
       using CommentsListCallback = std::function<void(Outcome<std::pair<juce::var, int>>)>;
   
       void getComments(const juce::String& postId, int limit = 20, int offset = 0, CommentsListCallback callback = nullptr);
   
       void createComment(const juce::String& postId, const juce::String& content, const juce::String& parentId = "", CommentCallback callback = nullptr);
   
       void getCommentReplies(const juce::String& commentId, int limit = 20, int offset = 0, CommentsListCallback callback = nullptr);
   
       void updateComment(const juce::String& commentId, const juce::String& content, CommentCallback callback = nullptr);
   
       void deleteComment(const juce::String& commentId, ResponseCallback callback = nullptr);
   
       void likeComment(const juce::String& commentId, ResponseCallback callback = nullptr);
   
       void unlikeComment(const juce::String& commentId, ResponseCallback callback = nullptr);
   
       //==========================================================================
       // Generic HTTP methods for custom API calls
       void get(const juce::String& endpoint, ResponseCallback callback);
       void post(const juce::String& endpoint, const juce::var& data, ResponseCallback callback);
       void put(const juce::String& endpoint, const juce::var& data, ResponseCallback callback);
       void del(const juce::String& endpoint, ResponseCallback callback);
   
       //==========================================================================
       // Absolute URL methods (for CDN, external APIs, etc.)
       // These methods accept full URLs instead of relative endpoints
       using BinaryDataCallback = std::function<void(Outcome<juce::MemoryBlock> data)>;
   
       void getAbsolute(const juce::String& absoluteUrl, ResponseCallback callback,
                        const juce::StringPairArray& customHeaders = juce::StringPairArray());
       void postAbsolute(const juce::String& absoluteUrl, const juce::var& data, ResponseCallback callback,
                         const juce::StringPairArray& customHeaders = juce::StringPairArray());
       void getBinaryAbsolute(const juce::String& absoluteUrl, BinaryDataCallback callback,
                              const juce::StringPairArray& customHeaders = juce::StringPairArray());
   
       // Multipart form upload to absolute URL (for external APIs like getstream.io, CDN uploads, etc.)
       using MultipartUploadCallback = std::function<void(Outcome<juce::var> response)>;
       void uploadMultipartAbsolute(const juce::String& absoluteUrl,
                                   const juce::String& fieldName,
                                   const juce::MemoryBlock& fileData,
                                   const juce::String& fileName,
                                   const juce::String& mimeType,
                                   const std::map<juce::String, juce::String>& extraFields,
                                   MultipartUploadCallback callback,
                                   const juce::StringPairArray& customHeaders = juce::StringPairArray());
   
       // Play tracking
       void trackPlay(const juce::String& activityId, ResponseCallback callback = nullptr);
       void trackListenDuration(const juce::String& activityId, double durationSeconds, ResponseCallback callback = nullptr);
   
       //==========================================================================
       // Notification operations
   
       struct NotificationResult
       {
           juce::var notifications;  
           int unseen = 0;           
           int unread = 0;          
       };
       using NotificationCallback = std::function<void(Outcome<NotificationResult>)>;
   
       void getNotifications(int limit = 20, int offset = 0, NotificationCallback callback = nullptr);
       void getNotificationCounts(std::function<void(int unseen, int unread)> callback);
       void markNotificationsRead(ResponseCallback callback = nullptr);
       void markNotificationsSeen(ResponseCallback callback = nullptr);
   
       //==========================================================================
       // User Discovery operations
       // Search users by username or display name
       void searchUsers(const juce::String& query, int limit = 20, int offset = 0, ResponseCallback callback = nullptr);
   
       // Get trending users (most active/followed recently)
       void getTrendingUsers(int limit = 20, ResponseCallback callback = nullptr);
   
       // Get featured producers (high engagement + recent activity)
       void getFeaturedProducers(int limit = 20, ResponseCallback callback = nullptr);
   
       // Get suggested users based on shared interests
       void getSuggestedUsers(int limit = 20, ResponseCallback callback = nullptr);
   
       // Get users by genre
       void getUsersByGenre(const juce::String& genre, int limit = 20, int offset = 0, ResponseCallback callback = nullptr);
   
       // Get available genres for filtering
       void getAvailableGenres(ResponseCallback callback = nullptr);
   
       // Get users similar to a specific user (by BPM/key preferences)
       void getSimilarUsers(const juce::String& userId, int limit = 10, ResponseCallback callback = nullptr);
   
       //==========================================================================
       // Search operations
       // Search posts with optional filters (genre, BPM range, key)
       void searchPosts(const juce::String& query,
                        const juce::String& genre = "",
                        int bpmMin = 0,
                        int bpmMax = 200,
                        const juce::String& key = "",
                        int limit = 20,
                        int offset = 0,
                        ResponseCallback callback = nullptr);
   
       // Get search suggestions/autocomplete
       void getSearchSuggestions(const juce::String& query, int limit = 5, ResponseCallback callback = nullptr);
   
       //==========================================================================
       // Stories operations
       // Get stories feed (ephemeral music clips from followed users)
       void getStoriesFeed(ResponseCallback callback = nullptr);
   
       // Mark a story as viewed
       void viewStory(const juce::String& storyId, ResponseCallback callback = nullptr);
   
       // Get list of users who viewed a story (story owner only)
       void getStoryViews(const juce::String& storyId, ResponseCallback callback = nullptr);
   
       // Upload a new story
       void uploadStory(const juce::AudioBuffer<float>& audioBuffer,
                        double sampleRate,
                        const juce::var& midiData,
                        int bpm = 0,
                        const juce::String& key = "",
                        const juce::StringArray& genres = juce::StringArray(),
                        ResponseCallback callback = nullptr);
   
       //==========================================================================
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
   
       //==========================================================================
       // Synchronous request method for use from background threads
       // (Use sparingly - prefer async methods for UI code)
       RequestResult makeAbsoluteRequestSync(const juce::String& absoluteUrl,
                                             const juce::String& method = "GET",
                                             const juce::var& data = juce::var(),
                                             bool requireAuth = false,
                                             const juce::StringPairArray& customHeaders = juce::StringPairArray(),
                                             juce::MemoryBlock* binaryData = nullptr);
   
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
       RequestResult makeRequestWithRetry(const juce::String& endpoint,
                                          const juce::String& method = "GET",
                                          const juce::var& data = juce::var(),
                                          bool requireAuth = false);
   
       RequestResult makeAbsoluteRequestWithRetry(const juce::String& absoluteUrl,
                                                   const juce::String& method = "GET",
                                                   const juce::var& data = juce::var(),
                                                   bool requireAuth = false,
                                                   const juce::StringPairArray& customHeaders = juce::StringPairArray(),
                                                   juce::MemoryBlock* binaryData = nullptr);
   
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
   
       // Absolute URL version for external APIs
       RequestResult uploadMultipartDataAbsolute(const juce::String& absoluteUrl,
                                                  const juce::String& fieldName,
                                                  const juce::MemoryBlock& fileData,
                                                  const juce::String& fileName,
                                                  const juce::String& mimeType,
                                                  const std::map<juce::String, juce::String>& extraFields = {},
                                                  const juce::StringPairArray& customHeaders = juce::StringPairArray());
   
       juce::String getAuthHeader() const;
       void handleAuthResponse(const juce::var& response);
       void updateConnectionStatus(ConnectionStatus status);
   
       // Helper to check authentication and return error if not authenticated
       template<typename Callback>
       bool requireAuth(Callback&& callback) const;
   
       // Helper to build API endpoint paths consistently
       static juce::String buildApiPath(const char* path);
   
       // Parse HTTP status code from response headers
       static int parseStatusCode(const juce::StringPairArray& headers);
   
       // Audio encoding
       juce::MemoryBlock encodeAudioToMP3(const juce::AudioBuffer<float>& buffer, double sampleRate);
       juce::MemoryBlock encodeAudioToWAV(const juce::AudioBuffer<float>& buffer, double sampleRate);
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkClient)
   };
