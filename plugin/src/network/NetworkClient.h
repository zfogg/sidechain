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
        juce::String daw;             ///< DAW name (e.g., "Ableton Live", "Logic Pro", "FL Studio") - auto-detected if empty

        // Auto-populated fields
        double durationSeconds = 0.0; ///< Audio duration in seconds
        int sampleRate = 44100;       ///< Audio sample rate in Hz
        int numChannels = 2;          ///< Number of audio channels

        // MIDI data (R.3.3 Cross-DAW MIDI Collaboration)
        juce::var midiData;           ///< MIDI events captured during recording (optional)
        bool includeMidi = true;      ///< Whether to upload MIDI with the post

        // Project file data (R.3.4 Project File Exchange)
        juce::File projectFile;       ///< DAW project file to upload (optional)
        bool includeProjectFile = false; ///< Whether to upload project file with the post
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

    /** Request password reset
     * @param email User's email address
     * @param callback Called with result (contains token in dev mode) or error
     */
    void requestPasswordReset(const juce::String& email, ResponseCallback callback = nullptr);

    /** Confirm password reset with token
     * @param token Password reset token
     * @param newPassword New password to set
     * @param callback Called with result or error
     */
    void resetPassword(const juce::String& token, const juce::String& newPassword, ResponseCallback callback = nullptr);

    /** Set the authentication callback for login/register operations
     * @param callback Called when authentication completes
     */
    void setAuthenticationCallback(AuthenticationCallback callback);

    //==========================================================================
    // Audio operations

    /**
     * Upload audio recording to the server
     *
     * @param recordingId Unique identifier for this recording
     * @param audioBuffer The audio data to upload (will be copied for thread safety)
     * @param sampleRate Sample rate of the audio
     * @param callback Called with the CDN URL or error
     *
     * @note IMPORTANT GOTCHAS:
     *       - BPM, key, and DAW are currently hardcoded to defaults (120 BPM, "C major", "Unknown")
     *       - See @ref notes/PLAN.md Phase 2 for planned DAW integration and automatic detection
     *       - Audio is encoded to WAV format (MP3 encoding not yet implemented - see encodeAudioToMP3)
     *       - The buffer is copied internally for thread safety
     *       - Encoding and upload happen on background threads to avoid blocking UI
     */
    void uploadAudio(const juce::String& recordingId,
                    const juce::AudioBuffer<float>& audioBuffer,
                    double sampleRate,
                    UploadCallback callback = nullptr);

    /**
     * Upload audio with full metadata (title, BPM, key, genre)
     *
     * @param audioBuffer The audio data to upload (will be copied for thread safety)
     * @param sampleRate Sample rate of the audio
     * @param metadata Audio metadata (title, BPM, key, genre, etc.)
     * @param callback Called with the CDN URL or error
     *
     * @note The audio buffer is copied internally for thread safety.
     *       Encoding happens on a background thread to avoid blocking the UI.
     *       Currently encodes to WAV format (MP3 encoding not yet implemented).
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

    /** Get "For You" personalized recommendations feed
     * @param limit Maximum number of posts to return
     * @param offset Pagination offset
     * @param callback Called with feed data or error
     */
    void getForYouFeed(int limit = 20, int offset = 0, FeedCallback callback = nullptr);

    /** Get similar posts to a given post
     * @param postId The post ID to find similar posts for
     * @param limit Maximum number of posts to return
     * @param callback Called with feed data or error
     */
    void getSimilarPosts(const juce::String& postId, int limit = 10, FeedCallback callback = nullptr);

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

    /** Delete a post
     * @param postId The post ID
     * @param callback Called with result or error
     */
    void deletePost(const juce::String& postId, ResponseCallback callback = nullptr);

    /** Report a post
     * @param postId The post ID
     * @param reason The reason for reporting (spam, harassment, inappropriate, copyright, violence, other)
     * @param description Optional additional context
     * @param callback Called with result or error
     */
    void reportPost(const juce::String& postId, const juce::String& reason, const juce::String& description = "", ResponseCallback callback = nullptr);

    //==========================================================================
    // R.3.2 Remix Chains operations

    /** Get the remix chain for a post (shows original -> remix -> remix lineage)
     * @param postId The post ID to get the chain for
     * @param callback Called with chain data or error
     *                 Response format: { chain: [{id, type, username, depth, is_current}, ...], total_depth: N }
     */
    void getRemixChain(const juce::String& postId, ResponseCallback callback = nullptr);

    /** Get all remixes of a post
     * @param postId The post ID
     * @param callback Called with array of remix posts or error
     */
    void getPostRemixes(const juce::String& postId, ResponseCallback callback = nullptr);

    /** Get remix source content (audio/MIDI URLs for remixing)
     * @param postId The post ID to get remix source from
     * @param callback Called with source URLs or error
     *                 Response format: { audio_url, midi_url, bpm, key, remix_type }
     */
    void getRemixSource(const juce::String& postId, ResponseCallback callback = nullptr);

    /** Create a remix post of another post
     * @param sourcePostId The post being remixed
     * @param remixType What was remixed: "audio", "midi", or "both"
     * @param callback Called with new post data or error
     */
    void createRemixPost(const juce::String& sourcePostId, const juce::String& remixType, ResponseCallback callback = nullptr);

    //==========================================================================
    // Download operations

    /** Download info structure for post downloads */
    struct DownloadInfo
    {
        juce::String downloadUrl;
        juce::String filename;
        juce::var metadata;  // Contains BPM, key, duration, genre, daw
        int downloadCount = 0;
    };

    using DownloadInfoCallback = std::function<void(Outcome<DownloadInfo>)>;
    using DownloadProgressCallback = std::function<void(float progress)>;  // 0.0 to 1.0

    /** Get download info for a post (generates download URL and metadata)
     * @param postId The post ID
     * @param callback Called with download info or error
     */
    void getPostDownloadInfo(const juce::String& postId, DownloadInfoCallback callback = nullptr);

    /** Download a file from a URL and save it to a location
     * @param url The URL to download from
     * @param targetFile The file to save to
     * @param progressCallback Optional callback for download progress (0.0 to 1.0)
     * @param callback Called with success/error
     */
    void downloadFile(const juce::String& url, const juce::File& targetFile,
                      DownloadProgressCallback progressCallback = nullptr,
                      ResponseCallback callback = nullptr);

    //==========================================================================
    // MIDI operations (R.3.3 Cross-DAW MIDI Collaboration)

    /** Download MIDI file for a pattern
     * @param midiId The MIDI pattern ID
     * @param targetFile The file to save the .mid file to
     * @param callback Called with success/error
     */
    void downloadMIDI(const juce::String& midiId, const juce::File& targetFile,
                      ResponseCallback callback = nullptr);

    /** Upload MIDI pattern to create a standalone MIDI resource
     * @param midiData MIDI data as JSON (events, tempo, time_signature)
     * @param name Optional name for the pattern
     * @param description Optional description
     * @param isPublic Whether the pattern is publicly visible (default true)
     * @param callback Called with created pattern ID or error
     */
    void uploadMIDI(const juce::var& midiData,
                    const juce::String& name = "",
                    const juce::String& description = "",
                    bool isPublic = true,
                    ResponseCallback callback = nullptr);

    //==========================================================================
    // Playlist operations (R.3.1 Collaborative Playlists)

    /** Create a new playlist
     * @param name Playlist name
     * @param description Optional description
     * @param isCollaborative Whether others can add tracks
     * @param isPublic Whether playlist is publicly visible
     * @param callback Called with created playlist or error
     */
    void createPlaylist(const juce::String& name,
                       const juce::String& description = "",
                       bool isCollaborative = false,
                       bool isPublic = true,
                       ResponseCallback callback = nullptr);

    /** Get user's playlists
     * @param filter Filter type: "all", "owned", "collaborated", "public"
     * @param callback Called with playlists array or error
     */
    void getPlaylists(const juce::String& filter = "all",
                     ResponseCallback callback = nullptr);

    /** Get a single playlist with entries
     * @param playlistId The playlist ID
     * @param callback Called with playlist data or error
     */
    void getPlaylist(const juce::String& playlistId,
                    ResponseCallback callback = nullptr);

    /** Add a post to a playlist
     * @param playlistId The playlist ID
     * @param postId The post ID to add
     * @param position Optional position to insert at (default: append to end)
     * @param callback Called with entry data or error
     */
    void addPlaylistEntry(const juce::String& playlistId,
                         const juce::String& postId,
                         int position = -1,
                         ResponseCallback callback = nullptr);

    /** Remove a post from a playlist
     * @param playlistId The playlist ID
     * @param entryId The entry ID to remove
     * @param callback Called with success/error
     */
    void removePlaylistEntry(const juce::String& playlistId,
                            const juce::String& entryId,
                            ResponseCallback callback = nullptr);

    /** Add a collaborator to a playlist
     * @param playlistId The playlist ID
     * @param userId The user ID to add as collaborator
     * @param role Role: "editor" or "viewer"
     * @param callback Called with collaborator data or error
     */
    void addPlaylistCollaborator(const juce::String& playlistId,
                                const juce::String& userId,
                                const juce::String& role,
                                ResponseCallback callback = nullptr);

    /** Remove a collaborator from a playlist
     * @param playlistId The playlist ID
     * @param userId The user ID to remove
     * @param callback Called with success/error
     */
    void removePlaylistCollaborator(const juce::String& playlistId,
                                   const juce::String& userId,
                                   ResponseCallback callback = nullptr);

    //==========================================================================
    // MIDI Challenge operations (R.2.2 MIDI Battle Royale)

    /** Get list of MIDI challenges
     * @param status Filter by status: "active", "voting", "past", "upcoming", or empty for all
     * @param callback Called with challenges array or error
     */
    void getMIDIChallenges(const juce::String& status = "",
                          ResponseCallback callback = nullptr);

    /** Get a single MIDI challenge with entries
     * @param challengeId The challenge ID
     * @param callback Called with challenge data or error
     */
    void getMIDIChallenge(const juce::String& challengeId,
                        ResponseCallback callback = nullptr);

    /** Submit an entry to a MIDI challenge
     * @param challengeId The challenge ID
     * @param audioUrl The audio file URL (from post upload)
     * @param postId Optional post ID if entry is linked to a post
     * @param midiData Optional MIDI data JSON
     * @param midiPatternId Optional MIDI pattern ID
     * @param callback Called with entry data or error
     */
    void submitMIDIChallengeEntry(const juce::String& challengeId,
                                 const juce::String& audioUrl,
                                 const juce::String& postId = "",
                                 const juce::var& midiData = juce::var(),
                                 const juce::String& midiPatternId = "",
                                 ResponseCallback callback = nullptr);

    /** Get entries for a MIDI challenge
     * @param challengeId The challenge ID
     * @param callback Called with entries array or error
     */
    void getMIDIChallengeEntries(const juce::String& challengeId,
                               ResponseCallback callback = nullptr);

    /** Vote for a MIDI challenge entry
     * @param challengeId The challenge ID
     * @param entryId The entry ID to vote for
     * @param callback Called with result or error
     */
    void voteMIDIChallengeEntry(const juce::String& challengeId,
                              const juce::String& entryId,
                              ResponseCallback callback = nullptr);

    //==========================================================================
    // Project file operations (R.3.4 Project File Exchange)

    /** Download a project file (DAW project: .als, .flp, .logic, etc.)
     * @param projectFileId The project file ID
     * @param targetFile The file to save the project file to
     * @param progressCallback Optional callback for download progress (0.0 to 1.0)
     * @param callback Called with success/error
     */
    void downloadProjectFile(const juce::String& projectFileId, const juce::File& targetFile,
                             DownloadProgressCallback progressCallback = nullptr,
                             ResponseCallback callback = nullptr);

    /** Upload a project file (DAW project: .als, .flp, .logic, etc.)
     * @param projectFile The project file to upload
     * @param audioPostId Optional audio post ID to link the project file to
     * @param description Optional description
     * @param isPublic Whether the project file is public (default: true)
     * @param progressCallback Optional callback for upload progress (0.0 to 1.0)
     * @param callback Called with project file ID or error
     */
    void uploadProjectFile(const juce::File& projectFile,
                          const juce::String& audioPostId = "",
                          const juce::String& description = "",
                          bool isPublic = true,
                          DownloadProgressCallback progressCallback = nullptr,
                          UploadCallback callback = nullptr);

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
    // Save/Bookmark operations (P0 Social Feature)

    /** Save (bookmark) a post for later
     * @param postId The post ID to save
     * @param callback Called with result or error
     */
    void savePost(const juce::String& postId, ResponseCallback callback = nullptr);

    /** Remove a saved post (unbookmark)
     * @param postId The post ID to unsave
     * @param callback Called with result or error
     */
    void unsavePost(const juce::String& postId, ResponseCallback callback = nullptr);

    /** Get saved posts for the current user
     * @param limit Maximum number of posts to return
     * @param offset Pagination offset
     * @param callback Called with saved posts or error
     */
    void getSavedPosts(int limit = 20, int offset = 0, FeedCallback callback = nullptr);

    //==========================================================================
    // Repost operations (P0 Social Feature)

    /** Repost a post to the current user's feed (like a retweet)
     * @param postId The post ID to repost
     * @param quote Optional quote text to add to the repost
     * @param callback Called with result or error
     */
    void repostPost(const juce::String& postId, const juce::String& quote = "", ResponseCallback callback = nullptr);

    /** Undo/remove a repost
     * @param postId The original post ID that was reposted
     * @param callback Called with result or error
     */
    void undoRepost(const juce::String& postId, ResponseCallback callback = nullptr);

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

    /** Report a comment
     * @param commentId The comment ID
     * @param reason The reason for reporting (spam, harassment, inappropriate, copyright, violence, other)
     * @param description Optional additional context
     * @param callback Called with result or error
     */
    void reportComment(const juce::String& commentId, const juce::String& reason, const juce::String& description = "", ResponseCallback callback = nullptr);

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

    /** Get personalized user recommendations to follow (via Gorse collaborative filtering)
     *  @param limit Maximum number of results
     *  @param offset Pagination offset
     *  @param callback Called with recommended users or error
     */
    void getRecommendedUsersToFollow(int limit = 20, int offset = 0, ResponseCallback callback = nullptr);

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
    void deleteStory(const juce::String& storyId, ResponseCallback callback = nullptr);

    // Mark a story as viewed
    void viewStory(const juce::String& storyId, ResponseCallback callback = nullptr);

    /** Get download info for a story (generates download URL and metadata) - 19.1
     * @param storyId The story ID
     * @param callback Called with download info or error
     */
    void getStoryDownloadInfo(const juce::String& storyId, DownloadInfoCallback callback = nullptr);

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
    // Story Highlights operations (permanent story collections)

    /** Get all highlights for a user
     * @param userId The user ID to get highlights for
     * @param callback Called with highlights array or error
     */
    void getHighlights(const juce::String& userId, ResponseCallback callback = nullptr);

    /** Get a single highlight with its stories
     * @param highlightId The highlight ID
     * @param callback Called with highlight data including stories or error
     */
    void getHighlight(const juce::String& highlightId, ResponseCallback callback = nullptr);

    /** Create a new highlight collection
     * @param name Display name for the highlight
     * @param description Optional description
     * @param callback Called with created highlight or error
     */
    void createHighlight(const juce::String& name,
                         const juce::String& description = "",
                         ResponseCallback callback = nullptr);

    /** Update an existing highlight
     * @param highlightId The highlight ID to update
     * @param name New name (empty to keep current)
     * @param description New description (empty to keep current)
     * @param callback Called with updated highlight or error
     */
    void updateHighlight(const juce::String& highlightId,
                         const juce::String& name,
                         const juce::String& description = "",
                         ResponseCallback callback = nullptr);

    /** Delete a highlight collection
     * @param highlightId The highlight ID to delete
     * @param callback Called with success/error
     */
    void deleteHighlight(const juce::String& highlightId, ResponseCallback callback = nullptr);

    /** Add a story to a highlight
     * @param highlightId The highlight to add to
     * @param storyId The story to add
     * @param callback Called with success/error
     */
    void addStoryToHighlight(const juce::String& highlightId,
                             const juce::String& storyId,
                             ResponseCallback callback = nullptr);

    /** Remove a story from a highlight
     * @param highlightId The highlight to remove from
     * @param storyId The story to remove
     * @param callback Called with success/error
     */
    void removeStoryFromHighlight(const juce::String& highlightId,
                                  const juce::String& storyId,
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

    /** Get the current user's email verification status
     * @return true if email is verified, false otherwise
     */
    bool isCurrentUserEmailVerified() const { return currentUserEmailVerified; }

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

    //==========================================================================
    // DAW Detection

    /**
     * Detect DAW name from host application.
     * Attempts to identify the DAW hosting the plugin.
     *
     * @return DAW name (e.g., "Ableton Live", "Logic Pro", "FL Studio") or "Unknown"
     */
    static juce::String detectDAWName();

private:
    Config config;
    juce::String authToken;
    juce::String currentUsername;
    juce::String currentUserId;
    bool currentUserEmailVerified = true;  // Default to verified

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

    //==========================================================================
    // Audio encoding

    /**
     * Encode audio buffer to MP3 format.
     *
     * @note NOT YET IMPLEMENTED - Currently falls back to WAV encoding.
     *       The server will transcode WAV to MP3, but this is less efficient.
     *       See @ref notes/PLAN.md for implementation plan.
     *
     * @param buffer Audio buffer to encode
     * @param sampleRate Sample rate of the audio
     * @return Encoded MP3 data (currently returns WAV data)
     */
    juce::MemoryBlock encodeAudioToMP3(const juce::AudioBuffer<float>& buffer, double sampleRate);

    /**
     * Encode audio buffer to WAV format.
     *
     * @param buffer Audio buffer to encode (will be copied internally)
     * @param sampleRate Sample rate of the audio
     * @return Encoded WAV data, or empty MemoryBlock on failure
     * @note Uses 16-bit PCM encoding. The buffer is copied before encoding
     *       to ensure thread safety when called from background threads.
     */
    juce::MemoryBlock encodeAudioToWAV(const juce::AudioBuffer<float>& buffer, double sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetworkClient)
};