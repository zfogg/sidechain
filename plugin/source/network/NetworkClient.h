#pragma once

#include <JuceHeader.h>
#include "../util/Constants.h"
#include "../util/Result.h"
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

    /** Connection status for network operations */
    enum class ConnectionStatus
    {
        Disconnected,   ///< No connection (red indicator)
        Connecting,     ///< Attempting connection (yellow indicator)
        Connected       ///< Successfully connected (green indicator)
    };

    //==========================================================================
    // Configuration for different environments

    /** Configuration structure for NetworkClient */
    struct Config
    {
        juce::String baseUrl;                    ///< Base URL for API requests
        int timeoutMs = Constants::Api::DEFAULT_TIMEOUT_MS;  ///< Request timeout in milliseconds
        int maxRetries = Constants::Api::MAX_RETRIES;         ///< Maximum retry attempts
        int retryDelayMs = Constants::Api::RETRY_DELAY_BASE_MS;  ///< Base delay between retries in milliseconds

        /** Create development configuration
         * @return Config with development backend URL
         */
        static Config development()
        {
            return { Constants::Endpoints::DEV_BASE_URL, Constants::Api::DEFAULT_TIMEOUT_MS, Constants::Api::MAX_RETRIES, Constants::Api::RETRY_DELAY_BASE_MS };
        }

        /** Create production configuration
         * @return Config with production backend URL and longer retry delays
         */
        static Config production()
        {
            return { Constants::Endpoints::PROD_BASE_URL, Constants::Api::DEFAULT_TIMEOUT_MS, Constants::Api::MAX_RETRIES, Constants::Api::RETRY_DELAY_BASE_MS * 2 };
        }
    };

    /** Constructor
     *  @param config NetworkClient configuration (defaults to development)
     */
    NetworkClient(const Config& config = Config::development());

    /** Destructor */
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

    /** Metadata structure for audio uploads */
    struct AudioUploadMetadata
    {
        juce::String title;           ///< Post title
        double bpm = 0.0;             ///< Beats per minute (0.0 if unknown)
        juce::String key;             ///< Musical key (e.g., "C", "Am", "F#m" or empty)
        juce::String genre;           ///< Genre (e.g., "Electronic", "Hip-Hop")

        // Auto-populated fields
        double durationSeconds = 0.0; ///< Audio duration in seconds
        int sampleRate = 44100;       ///< Audio sample rate in Hz
        int numChannels = 2;          ///< Number of audio channels
    };

    //==========================================================================
    // Authentication (simplified - no device claiming)

    /** Register a new user account
     * @param email User's email address
     * @param username Desired username
     * @param password User's password
     * @param displayName User's display name
     * @param callback Called with authentication result (token, userId) or error
     */
    void registerAccount(const juce::String& email, const juce::String& username,
                        const juce::String& password, const juce::String& displayName,
                        AuthenticationCallback callback);

    /** Login with existing account credentials
     * @param email User's email address
     * @param password User's password
     * @param callback Called with authentication result (token, userId) or error
     */
    void loginAccount(const juce::String& email, const juce::String& password,
                     AuthenticationCallback callback);

    /** Set the authentication callback for login/register operations
     * @param callback Called when authentication completes
     */
    void setAuthenticationCallback(AuthenticationCallback callback);

    //==========================================================================
    // Audio operations

    /** Upload audio to the server
     * @param recordingId Unique identifier for this recording
     * @param audioBuffer The audio data to upload
     * @param sampleRate Sample rate of the audio
     * @param callback Called with the CDN URL or error
     */
    void uploadAudio(const juce::String& recordingId,
                    const juce::AudioBuffer<float>& audioBuffer,
                    double sampleRate,
                    UploadCallback callback = nullptr);

    /** Upload audio with full metadata (title, BPM, key, genre)
     * @param audioBuffer The audio data to upload
     * @param sampleRate Sample rate of the audio
     * @param metadata Audio metadata (title, BPM, key, genre, etc.)
     * @param callback Called with the CDN URL or error
     */
    void uploadAudioWithMetadata(const juce::AudioBuffer<float>& audioBuffer,
                                 double sampleRate,
                                 const AudioUploadMetadata& metadata,
                                 UploadCallback callback = nullptr);

    //==========================================================================
    // Social feed operations (all use enriched endpoints with reaction data from getstream.io)

    /** Get the global feed (all posts)
     * @param limit Maximum number of posts to return
     * @param offset Pagination offset
     * @param callback Called with feed data or error
     */
    void getGlobalFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);

    /** Get the timeline feed (posts from followed users)
     * @param limit Maximum number of posts to return
     * @param offset Pagination offset
     * @param callback Called with feed data or error
     */
    void getTimelineFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);

    /** Get the trending feed (popular posts)
     * @param limit Maximum number of posts to return
     * @param offset Pagination offset
     * @param callback Called with feed data or error
     */
    void getTrendingFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);

    /** Like a post with optional emoji reaction
     * @param activityId The post activity ID
     * @param emoji Optional emoji reaction (empty for default like)
     * @param callback Called with result or error
     */
    void likePost(const juce::String& activityId, const juce::String& emoji = "", ResponseCallback callback = nullptr);

    /** Unlike a post
     * @param activityId The post activity ID
     * @param callback Called with result or error
     */
    void unlikePost(const juce::String& activityId, ResponseCallback callback = nullptr);

    /** Follow a user
     * @param userId The user ID to follow
     * @param callback Called with result or error
     */
    void followUser(const juce::String& userId, ResponseCallback callback = nullptr);

    /** Unfollow a user
     * @param userId The user ID to unfollow
     * @param callback Called with result or error
     */
    void unfollowUser(const juce::String& userId, ResponseCallback callback = nullptr);

    //==========================================================================
    // Profile operations

    /** Upload a profile picture
     * @param imageFile The image file to upload
     * @param callback Called with the CDN URL or error
     */
    void uploadProfilePicture(const juce::File& imageFile, ProfilePictureCallback callback = nullptr);

    /** Change the current user's username
     * @param newUsername The new username
     * @param callback Called with result or error
     */
    void changeUsername(const juce::String& newUsername, ResponseCallback callback = nullptr);

    /** Get a user's followers list
     * @param userId The user ID
     * @param limit Maximum number of results
     * @param offset Pagination offset
     * @param callback Called with followers data or error
     */
    void getFollowers(const juce::String& userId, int limit = 20, int offset = 0, ResponseCallback callback = nullptr);

    /** Get a user's following list
     * @param userId The user ID
     * @param limit Maximum number of results
     * @param offset Pagination offset
     * @param callback Called with following data or error
     */
    void getFollowing(const juce::String& userId, int limit = 20, int offset = 0, ResponseCallback callback = nullptr);

    //==========================================================================
    // Comment operations
    using CommentCallback = std::function<void(Outcome<juce::var> comment)>;
    // CommentsListCallback: returns Outcome with pair<comments, totalCount> or error
    using CommentsListCallback = std::function<void(Outcome<std::pair<juce::var, int>>)>;

    /** Get comments for a post
     * @param postId The post ID
     * @param limit Maximum number of comments to return
     * @param offset Pagination offset
     * @param callback Called with comments list and total count or error
     */
    void getComments(const juce::String& postId, int limit = 20, int offset = 0, CommentsListCallback callback = nullptr);

    /** Create a new comment on a post
     * @param postId The post ID
     * @param content The comment text
     * @param parentId Optional parent comment ID for replies
     * @param callback Called with the created comment or error
     */
    void createComment(const juce::String& postId, const juce::String& content, const juce::String& parentId = "", CommentCallback callback = nullptr);

    /** Get replies to a comment
     * @param commentId The parent comment ID
     * @param limit Maximum number of replies to return
     * @param offset Pagination offset
     * @param callback Called with replies list and total count or error
     */
    void getCommentReplies(const juce::String& commentId, int limit = 20, int offset = 0, CommentsListCallback callback = nullptr);

    /** Update an existing comment
     * @param commentId The comment ID to update
     * @param content The new comment text
     * @param callback Called with the updated comment or error
     */
    void updateComment(const juce::String& commentId, const juce::String& content, CommentCallback callback = nullptr);

    /** Delete a comment
     * @param commentId The comment ID to delete
     * @param callback Called with result or error
     */
    void deleteComment(const juce::String& commentId, ResponseCallback callback = nullptr);

    /** Like a comment
     * @param commentId The comment ID
     * @param callback Called with result or error
     */
    void likeComment(const juce::String& commentId, ResponseCallback callback = nullptr);

    /** Unlike a comment
     * @param commentId The comment ID
     * @param callback Called with result or error
     */
    void unlikeComment(const juce::String& commentId, ResponseCallback callback = nullptr);

    //==========================================================================
    // Generic HTTP methods for custom API calls

    /** Make a GET request to an endpoint
     *  @param endpoint Relative endpoint path (e.g., "/api/v1/users/me")
     *  @param callback Called with response data or error
     */
    void get(const juce::String& endpoint, ResponseCallback callback);

    /** Make a POST request to an endpoint
     *  @param endpoint Relative endpoint path
     *  @param data JSON data to send in request body
     *  @param callback Called with response data or error
     */
    void post(const juce::String& endpoint, const juce::var& data, ResponseCallback callback);

    /** Make a PUT request to an endpoint
     *  @param endpoint Relative endpoint path
     *  @param data JSON data to send in request body
     *  @param callback Called with response data or error
     */
    void put(const juce::String& endpoint, const juce::var& data, ResponseCallback callback);

    /** Make a DELETE request to an endpoint
     *  @param endpoint Relative endpoint path
     *  @param callback Called with response data or error
     */
    void del(const juce::String& endpoint, ResponseCallback callback);

    //==========================================================================
    // Absolute URL methods (for CDN, external APIs, etc.)
    // These methods accept full URLs instead of relative endpoints
    using BinaryDataCallback = std::function<void(Outcome<juce::MemoryBlock> data)>;

    /** Make a GET request to an absolute URL
     *  @param absoluteUrl Full URL (e.g., "https://cdn.example.com/file.mp3")
     *  @param callback Called with response data or error
     *  @param customHeaders Optional custom HTTP headers
     */
    void getAbsolute(const juce::String& absoluteUrl, ResponseCallback callback,
                     const juce::StringPairArray& customHeaders = juce::StringPairArray());

    /** Make a POST request to an absolute URL
     *  @param absoluteUrl Full URL
     *  @param data JSON data to send in request body
     *  @param callback Called with response data or error
     *  @param customHeaders Optional custom HTTP headers
     */
    void postAbsolute(const juce::String& absoluteUrl, const juce::var& data, ResponseCallback callback,
                      const juce::StringPairArray& customHeaders = juce::StringPairArray());

    /** Make a GET request to an absolute URL and receive binary data
     *  @param absoluteUrl Full URL
     *  @param callback Called with binary data or error
     *  @param customHeaders Optional custom HTTP headers
     */
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

    //==========================================================================
    // Play tracking

    /** Track that a post was played
     *  @param activityId The post activity ID
     *  @param callback Called with result or error
     */
    void trackPlay(const juce::String& activityId, ResponseCallback callback = nullptr);

    /** Track listen duration for analytics
     *  @param activityId The post activity ID
     *  @param durationSeconds How long the audio was played
     *  @param callback Called with result or error
     */
    void trackListenDuration(const juce::String& activityId, double durationSeconds, ResponseCallback callback = nullptr);

    //==========================================================================
    // Notification operations

    /** Result structure for notification queries */
    struct NotificationResult
    {
        juce::var notifications;  ///< Array of notification objects
        int unseen = 0;           ///< Count of unseen notifications
        int unread = 0;          ///< Count of unread notifications
    };
    using NotificationCallback = std::function<void(Outcome<NotificationResult>)>;

    /** Get user notifications
     *  @param limit Maximum number of notifications to return
     *  @param offset Pagination offset
     *  @param callback Called with notifications and counts or error
     */
    void getNotifications(int limit = 20, int offset = 0, NotificationCallback callback = nullptr);

    /** Get notification counts (unseen and unread)
     *  @param callback Called with unseen and unread counts
     */
    void getNotificationCounts(std::function<void(int unseen, int unread)> callback);

    /** Mark all notifications as read
     *  @param callback Called with result or error
     */
    void markNotificationsRead(ResponseCallback callback = nullptr);

    /** Mark all notifications as seen
     *  @param callback Called with result or error
     */
    void markNotificationsSeen(ResponseCallback callback = nullptr);

    //==========================================================================
    // User Discovery operations

    /** Search users by username or display name
     *  @param query Search query string
     *  @param limit Maximum number of results
     *  @param offset Pagination offset
     *  @param callback Called with search results or error
     */
    void searchUsers(const juce::String& query, int limit = 20, int offset = 0, ResponseCallback callback = nullptr);

    /** Get trending users (most active/followed recently)
     *  @param limit Maximum number of results
     *  @param callback Called with trending users or error
     */
    void getTrendingUsers(int limit = 20, ResponseCallback callback = nullptr);

    /** Get featured producers (high engagement + recent activity)
     *  @param limit Maximum number of results
     *  @param callback Called with featured producers or error
     */
    void getFeaturedProducers(int limit = 20, ResponseCallback callback = nullptr);

    /** Get suggested users based on shared interests
     *  @param limit Maximum number of results
     *  @param callback Called with suggested users or error
     */
    void getSuggestedUsers(int limit = 20, ResponseCallback callback = nullptr);

    /** Get users by genre
     *  @param genre Genre name (e.g., "Electronic", "Hip-Hop")
     *  @param limit Maximum number of results
     *  @param offset Pagination offset
     *  @param callback Called with users or error
     */
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

    /** Set the authentication token
     * @param token The JWT authentication token
     */
    void setAuthToken(const juce::String& token);

    /** Check if currently authenticated
     * @return true if authenticated, false otherwise
     */
    bool isAuthenticated() const { return !authToken.isEmpty(); }

    /** Get the base URL for API requests
     * @return The configured base URL
     */
    const juce::String& getBaseUrl() const { return config.baseUrl; }

    /** Get the current authenticated username
     * @return The username, or empty string if not authenticated
     */
    const juce::String& getCurrentUsername() const { return currentUsername; }

    /** Get the current authenticated user ID
     * @return The user ID, or empty string if not authenticated
     */
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