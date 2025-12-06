#include "NetworkClient.h"
#include "../util/HttpErrorHandler.h"
#include "../util/Log.h"
#include "../util/Async.h"
#include "../util/Result.h"

//==============================================================================
// Helper to convert RequestResult to Outcome<juce::var>
/** Convert a RequestResult to an Outcome<juce::var> for type-safe error handling
 * @param result The request result to convert
 * @return Outcome with data if successful, or error message if failed
 */
static Outcome<juce::var> requestResultToOutcome(const NetworkClient::RequestResult& result)
{
    if (result.success && result.isSuccess())
    {
        return Outcome<juce::var>::ok(result.data);
    }
    else
    {
        juce::String errorMsg = result.getUserFriendlyError();
        if (errorMsg.isEmpty())
            errorMsg = "Request failed (HTTP " + juce::String(result.httpStatus) + ")";
        return Outcome<juce::var>::error(errorMsg);
    }
}

//==============================================================================
// RequestResult helper methods
/** Get a user-friendly error message from the request result
 * Attempts to extract error message from JSON response, falls back to HTTP status messages
 * @return Human-readable error message
 */
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

/** Parse HTTP status code from response headers
 * @param headers Response headers from HTTP request
 * @return HTTP status code, or 0 if not found
 */
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
/** Construct NetworkClient with configuration
 * @param cfg NetworkClient configuration (defaults to development config)
 */
NetworkClient::NetworkClient(const Config& cfg)
    : config(cfg)
{
    Log::info("NetworkClient initialized with base URL: " + config.baseUrl);
    Log::debug("  Timeout: " + juce::String(config.timeoutMs) + "ms, Max retries: " + juce::String(config.maxRetries));
}

/** Destructor - cancels all pending requests
 */
NetworkClient::~NetworkClient()
{
    cancelAllRequests();
}

//==============================================================================
/** Set callback for connection status changes
 * @param callback Function called when connection status changes
 */
void NetworkClient::setConnectionStatusCallback(ConnectionStatusCallback callback)
{
    connectionStatusCallback = callback;
}

/** Update connection status and notify listeners
 * @param status New connection status
 */
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

/** Check connection to backend by pinging health endpoint
 * Updates connection status based on health check result
 */
void NetworkClient::checkConnection()
{
    updateConnectionStatus(ConnectionStatus::Connecting);

    Async::runVoid(
        [this]() {
            if (shuttingDown.load())
                return;

            auto result = makeRequestWithRetry("/health", "GET", juce::var(), false);

            juce::MessageManager::callAsync([this, result]() {
                if (result.success)
                {
                    updateConnectionStatus(ConnectionStatus::Connected);
                    Log::debug("Connection check: Connected to backend");
                }
                else
                {
                    updateConnectionStatus(ConnectionStatus::Disconnected);
                    Log::warn("Connection check: Failed - " + result.errorMessage);
                }
            });
        }
    );
}

/** Cancel all pending requests and wait for completion
 * Used during shutdown to ensure clean teardown
 */
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

/** Update NetworkClient configuration
 * @param newConfig New configuration to use
 */
void NetworkClient::setConfig(const Config& newConfig)
{
    config = newConfig;
    Log::info("NetworkClient config updated - base URL: " + config.baseUrl);
}

//==============================================================================
/** Register a new user account
 * @param email User's email address
 * @param username Desired username
 * @param password User's password
 * @param displayName User's display name
 * @param callback Called with authentication result (token, userId) or error
 */
void NetworkClient::registerAccount(const juce::String& email, const juce::String& username,
                                   const juce::String& password, const juce::String& displayName,
                                   AuthenticationCallback callback)
{
    Async::runVoid(
        [this, email, username, password, displayName, callback]() {
            juce::var registerData = juce::var(new juce::DynamicObject());
            registerData.getDynamicObject()->setProperty("email", email);
            registerData.getDynamicObject()->setProperty("username", username);
            registerData.getDynamicObject()->setProperty("password", password);
            registerData.getDynamicObject()->setProperty("display_name", displayName);

            auto response = makeRequest(buildApiPath("/auth/register"), "POST", registerData, false);

            juce::String token, userId, responseUsername;
            bool success = false;

            if (response.isObject())
            {
                auto authData = response.getProperty("auth", juce::var());
                if (authData.isObject())
                {
                    token = authData.getProperty("token", "").toString();
                    auto user = authData.getProperty("user", juce::var());

                    if (!token.isEmpty() && user.isObject())
                    {
                        userId = user.getProperty("id", "").toString();
                        responseUsername = user.getProperty("username", "").toString();
                        success = true;
                    }
                }
            }

            juce::MessageManager::callAsync([this, callback, token, userId, responseUsername, success]() {
                if (success)
                {
                    // Store authentication info
                    authToken = token;
                    currentUserId = userId;
                    currentUsername = responseUsername;

                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
                    callback(authResult);
                    Log::info("Account registered successfully: " + responseUsername);
                }
                else
                {
                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::error("Registration failed - invalid input or username already taken");
                    callback(authResult);
                    Log::error("Account registration failed");
                }
            });
        }
    );
}

/** Login with existing account credentials
 * @param email User's email address
 * @param password User's password
 * @param callback Called with authentication result (token, userId) or error
 */
void NetworkClient::loginAccount(const juce::String& email, const juce::String& password,
                                AuthenticationCallback callback)
{
    Async::runVoid(
        [this, email, password, callback]() {
            juce::var loginData = juce::var(new juce::DynamicObject());
            loginData.getDynamicObject()->setProperty("email", email);
            loginData.getDynamicObject()->setProperty("password", password);

            auto response = makeRequest(buildApiPath("/auth/login"), "POST", loginData, false);

            juce::String token, userId, username;
            bool success = false;

            if (response.isObject())
            {
                auto authData = response.getProperty("auth", juce::var());
                if (authData.isObject())
                {
                    token = authData.getProperty("token", "").toString();
                    auto user = authData.getProperty("user", juce::var());

                    if (!token.isEmpty() && user.isObject())
                    {
                        userId = user.getProperty("id", "").toString();
                        username = user.getProperty("username", "").toString();
                        success = true;
                    }
                }
            }

            juce::MessageManager::callAsync([this, callback, token, userId, username, success]() {
                if (success)
                {
                    // Store authentication info
                    authToken = token;
                    currentUserId = userId;
                    currentUsername = username;

                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
                    callback(authResult);
                    Log::info("Login successful: " + username);
                }
                else
                {
                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::error("Login failed - invalid credentials");
                    callback(authResult);
                    Log::warn("Login failed");
                }
            });
        }
    );
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
        Log::warn("Cannot upload audio: " + juce::String(Constants::Errors::NOT_AUTHENTICATED));
        if (callback)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(Outcome<juce::String>::error(Constants::Errors::NOT_AUTHENTICATED));
            });
        }
        return;
    }

    // Copy the buffer for the background thread
    juce::AudioBuffer<float> bufferCopy(audioBuffer);

    Async::runVoid([this, recordingId, bufferCopy, sampleRate, callback]() {
        // Encode audio to WAV (server will transcode to MP3)
        auto audioData = encodeAudioToWAV(bufferCopy, sampleRate);

        if (audioData.getSize() == 0)
        {
            Log::error("Failed to encode audio");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(Outcome<juce::String>::error("Failed to encode audio"));
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
            juce::MessageManager::callAsync([callback, success, audioUrl, result]() {
                if (success)
                    callback(Outcome<juce::String>::ok(audioUrl));
                else
                    callback(Outcome<juce::String>::error(result.getUserFriendlyError()));
            });
        }

        if (success)
            Log::info("Audio uploaded successfully: " + audioUrl);
        else
            Log::error("Audio upload failed: " + result.getUserFriendlyError());
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
        Log::warn("Cannot upload audio: " + juce::String(Constants::Errors::NOT_AUTHENTICATED));
        if (callback)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(Outcome<juce::String>::error(Constants::Errors::NOT_AUTHENTICATED));
            });
        }
        return;
    }

        // Copy the buffer and metadata for the background thread
    juce::AudioBuffer<float> bufferCopy(audioBuffer);
    AudioUploadMetadata metadataCopy = metadata;

    Async::runVoid([this, bufferCopy, sampleRate, metadataCopy, callback]() {
        // Encode audio to WAV (server will transcode to MP3)
        auto audioData = encodeAudioToWAV(bufferCopy, sampleRate);

        if (audioData.getSize() == 0)
        {
            Log::error("Failed to encode audio");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(Outcome<juce::String>::error("Failed to encode audio"));
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
            juce::MessageManager::callAsync([callback, success, audioUrl, result]() {
                if (success)
                    callback(Outcome<juce::String>::ok(audioUrl));
                else
                    callback(Outcome<juce::String>::error(result.getUserFriendlyError()));
            });
        }

        if (success)
            Log::info("Audio with metadata uploaded successfully: " + audioUrl);
        else
            Log::error("Audio upload failed: " + result.getUserFriendlyError());
    });
}

//==============================================================================
void NetworkClient::getGlobalFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;

    Async::runVoid([this, limit, offset, callback]() {
        // Use enriched endpoint to get reaction counts and own reactions from getstream.io
        juce::String endpoint = buildApiPath("/feed/global/enriched") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::getTimelineFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;

    Async::runVoid([this, limit, offset, callback]() {
        // Use enriched endpoint to get reaction counts and own reactions from getstream.io
        juce::String endpoint = "/api/feed/timeline/enriched?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::getTrendingFeed(int limit, int offset, FeedCallback callback)
{
    if (!isAuthenticated())
        return;

    Async::runVoid([this, limit, offset, callback]() {
        // Trending feed uses engagement scoring (likes, plays, comments weighted by recency)
        juce::String endpoint = buildApiPath("/feed/trending") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
        auto response = makeRequest(endpoint, "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, response]() {
                if (response.isObject() || response.isArray())
                    callback(Outcome<juce::var>::ok(response));
                else
                    callback(Outcome<juce::var>::error("Invalid feed response"));
            });
        }
    });
}

void NetworkClient::likePost(const juce::String& activityId, const juce::String& emoji, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, activityId, emoji, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("activity_id", activityId);

        juce::String endpoint;
        if (!emoji.isEmpty())
        {
            // Use emoji reaction endpoint
            data.getDynamicObject()->setProperty("emoji", emoji);
            endpoint = buildApiPath("/social/react");
        }
        else
        {
            // Use standard like endpoint
            endpoint = buildApiPath("/social/like");
        }

        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        Log::debug("Like/reaction response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::unlikePost(const juce::String& activityId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, activityId, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("activity_id", activityId);

        auto result = makeRequestWithRetry(buildApiPath("/social/like"), "DELETE", data, true);
        Log::debug("Unlike response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::followUser(const juce::String& userId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, userId, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("target_user_id", userId);

        auto result = makeRequestWithRetry(buildApiPath("/social/follow"), "POST", data, true);
        Log::debug("Follow response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::unfollowUser(const juce::String& userId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, userId, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("target_user_id", userId);

        auto result = makeRequestWithRetry(buildApiPath("/social/unfollow"), "POST", data, true);
        Log::debug("Unfollow response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::trackPlay(const juce::String& activityId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, activityId, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("activity_id", activityId);

        auto result = makeRequestWithRetry(buildApiPath("/social/play"), "POST", data, true);
        Log::debug("Track play response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::trackListenDuration(const juce::String& activityId, double durationSeconds, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    // Only track if duration is meaningful (at least 1 second)
    if (durationSeconds < 1.0)
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, activityId, durationSeconds, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("activity_id", activityId);
        data.getDynamicObject()->setProperty("duration", durationSeconds);

        auto result = makeRequestWithRetry(buildApiPath("/social/listen-duration"), "POST", data, true);
        Log::debug("Track listen duration response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

//==============================================================================
void NetworkClient::uploadProfilePicture(const juce::File& imageFile, ProfilePictureCallback callback)
{
    if (!isAuthenticated())
    {
        Log::warn("Cannot upload profile picture: " + juce::String(Constants::Errors::NOT_AUTHENTICATED));
        if (callback)
            callback(Outcome<juce::String>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    if (!imageFile.existsAsFile())
    {
        Log::error("Profile picture file does not exist: " + imageFile.getFullPathName());
        if (callback)
        {
            juce::MessageManager::callAsync([callback]() {
                callback(Outcome<juce::String>::error("File does not exist"));
            });
        }
        return;
    }

    Async::runVoid([this, imageFile, callback]() {
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

        // Create URL with file upload using JUCE's built-in multipart form handling
        juce::URL url(config.baseUrl + buildApiPath("/users/upload-profile-picture"));

        // Use withFileToUpload - JUCE will automatically create proper multipart/form-data
        url = url.withFileToUpload("profile_picture", imageFile, getMimeType(imageFile.getFileExtension()));

        // Build headers (auth only - Content-Type will be set automatically by JUCE)
        juce::String headers = "Authorization: Bearer " + authToken + "\r\n";

        // Create request options
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withExtraHeaders(headers)
                           .withConnectionTimeoutMs(config.timeoutMs);

        // Make request
        auto stream = url.createInputStream(options);

        if (stream == nullptr)
        {
            Log::error("Failed to create stream for profile picture upload");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(Outcome<juce::String>::error("Failed to encode audio"));
                });
            }
            return;
        }

        auto response = stream->readEntireStreamAsString();
        Log::debug("Profile picture upload response: " + response);

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
                if (success)
                    callback(Outcome<juce::String>::ok(pictureUrl));
                else
                    callback(Outcome<juce::String>::error("Failed to upload profile picture"));
            });
        }

        if (success)
            Log::info("Profile picture uploaded successfully: " + pictureUrl);
        else
            Log::error("Profile picture upload failed");
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
            Log::debug("Request attempt " + juce::String(attempts) + "/" + juce::String(config.maxRetries)
                + " failed for: " + endpoint);

            if (attempts < config.maxRetries)
            {
                // Wait before retry with exponential backoff
                int delay = config.retryDelayMs * attempts;
                juce::Thread::sleep(delay);
                continue;
            }

            // Report connection error after all retries exhausted
            HttpErrorHandler::getInstance().reportError(
                endpoint, method, 0, result.errorMessage, "");

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

        Log::debug("API Response from " + endpoint + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

        // Check for server errors that should trigger retry
        if (result.httpStatus >= 500 && attempts < config.maxRetries)
        {
            Log::warn("Server error, retrying...");
            int delay = config.retryDelayMs * attempts;
            juce::Thread::sleep(delay);
            continue;
        }

        // Report HTTP errors (4xx and 5xx status codes)
        if (result.httpStatus >= 400)
        {
            HttpErrorHandler::getInstance().reportError(
                endpoint, method, result.httpStatus,
                result.getUserFriendlyError(),
                juce::JSON::toString(result.data));
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

NetworkClient::RequestResult NetworkClient::makeAbsoluteRequestWithRetry(const juce::String& absoluteUrl,
                                                                           const juce::String& method,
                                                                           const juce::var& data,
                                                                           bool requireAuth,
                                                                           const juce::StringPairArray& customHeaders,
                                                                           juce::MemoryBlock* binaryData)
{
    RequestResult result;
    int attempts = 0;

    while (attempts < config.maxRetries && !shuttingDown.load())
    {
        attempts++;
        activeRequestCount++;

        juce::URL url(absoluteUrl);

        // Build headers string
        juce::String headers = "Content-Type: application/json\r\n";

        if (requireAuth && !authToken.isEmpty())
        {
            headers += "Authorization: Bearer " + authToken + "\r\n";
        }

        // Add custom headers
        for (int i = 0; i < customHeaders.size(); i++)
        {
            auto key = customHeaders.getAllKeys()[i];
            auto value = customHeaders[key];
            headers += key + ": " + value + "\r\n";
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
            Log::debug("Absolute request attempt " + juce::String(attempts) + "/" + juce::String(config.maxRetries)
                + " failed for: " + absoluteUrl);

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

        // Store response headers and extract status code
        result.responseHeaders = responseHeaders;
        result.httpStatus = parseStatusCode(responseHeaders);

        // If we couldn't parse status code, assume 200 for successful stream
        if (result.httpStatus == 0)
            result.httpStatus = 200;

        // Read response - either as binary or as string
        if (binaryData != nullptr)
        {
            stream->readIntoMemoryBlock(*binaryData);
            result.success = result.isSuccess() && binaryData->getSize() > 0;
        }
        else
        {
            auto response = stream->readEntireStreamAsString();
            result.data = juce::JSON::parse(response);
            result.success = result.isSuccess();
            Log::debug("Absolute URL Response from " + absoluteUrl + " (HTTP " + juce::String(result.httpStatus) + ")");
        }

        // Check for server errors that should trigger retry
        if (result.httpStatus >= 500 && attempts < config.maxRetries)
        {
            Log::warn("Server error, retrying...");
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

NetworkClient::RequestResult NetworkClient::makeAbsoluteRequestSync(const juce::String& absoluteUrl,
                                                                      const juce::String& method,
                                                                      const juce::var& data,
                                                                      bool requireAuth,
                                                                      const juce::StringPairArray& customHeaders,
                                                                      juce::MemoryBlock* binaryData)
{
    RequestResult result;

    if (shuttingDown.load())
    {
        result.errorMessage = "Request cancelled";
        return result;
    }

    activeRequestCount++;

    juce::URL url(absoluteUrl);

    // Build headers string
    juce::String headers = "Content-Type: application/json\r\n";

    if (requireAuth && !authToken.isEmpty())
    {
        headers += "Authorization: Bearer " + authToken + "\r\n";
    }

    // Add custom headers
    for (int i = 0; i < customHeaders.size(); i++)
    {
        auto key = customHeaders.getAllKeys()[i];
        auto value = customHeaders[key];
        headers += key + ": " + value + "\r\n";
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
        return result;
    }

    // Store response headers and extract status code
    result.responseHeaders = responseHeaders;
    result.httpStatus = parseStatusCode(responseHeaders);

    // If we couldn't parse status code, assume 200 for successful stream
    if (result.httpStatus == 0)
        result.httpStatus = 200;

    // Read response - either as binary or as string
    if (binaryData != nullptr)
    {
        stream->readIntoMemoryBlock(*binaryData);
        result.success = result.isSuccess() && binaryData->getSize() > 0;
    }
    else
    {
        auto response = stream->readEntireStreamAsString();
        result.data = juce::JSON::parse(response);
        result.success = result.isSuccess();
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

//==============================================================================
// Helper to build API endpoint paths consistently
juce::String NetworkClient::buildApiPath(const char* path)
{
    juce::String pathStr(path);

    // If path already starts with /api/v1, return as-is
    if (pathStr.startsWith("/api/v1"))
        return pathStr;

    // If path starts with /api/, replace with /api/v1/
    if (pathStr.startsWith("/api/"))
        return pathStr.replace("/api/", "/api/v1/");

    // Otherwise, prepend /api/v1
    if (pathStr.startsWith("/"))
        return juce::String(Constants::Endpoints::API_VERSION) + pathStr;
    return juce::String(Constants::Endpoints::API_VERSION) + "/" + pathStr;
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
                    auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
                    authCallback(authResult);
                });
            }
        }
    }
}

juce::MemoryBlock NetworkClient::encodeAudioToMP3(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // TODO: Implement MP3 encoding with LAME or similar library
    // For now, fall back to WAV which the server can transcode
    Log::warn("MP3 encoding not yet implemented, using WAV format");
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
        Log::error("Failed to create WAV writer");
        return juce::MemoryBlock();
    }

    // Write audio data
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        Log::error("Failed to write audio data to WAV");
        return juce::MemoryBlock();
    }

    writer.reset(); // Flush and close

    Log::debug("Encoded " + juce::String(buffer.getNumSamples()) + " samples at "
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
        result.errorMessage = Constants::Errors::NOT_AUTHENTICATED;
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

        // Report connection error
        HttpErrorHandler::getInstance().reportError(
            endpoint, "POST (multipart)", 0, result.errorMessage, "");

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

    Log::debug("Multipart upload to " + endpoint + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

    // Report HTTP errors
    if (result.httpStatus >= 400)
    {
        HttpErrorHandler::getInstance().reportError(
            endpoint, "POST (multipart)", result.httpStatus,
            result.getUserFriendlyError(),
            juce::JSON::toString(result.data));
    }

    updateConnectionStatus(result.success ? ConnectionStatus::Connected : ConnectionStatus::Disconnected);

    return result;
}

NetworkClient::RequestResult NetworkClient::uploadMultipartDataAbsolute(
    const juce::String& absoluteUrl,
    const juce::String& fieldName,
    const juce::MemoryBlock& fileData,
    const juce::String& fileName,
    const juce::String& mimeType,
    const std::map<juce::String, juce::String>& extraFields,
    const juce::StringPairArray& customHeaders)
{
    RequestResult result;

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
    juce::URL url(absoluteUrl);

    // Build headers
    juce::String headers = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";

    // Add custom headers
    for (int i = 0; i < customHeaders.size(); i++)
    {
        auto key = customHeaders.getAllKeys()[i];
        auto value = customHeaders[key];
        headers += key + ": " + value + "\r\n";
    }

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

    Log::debug("Multipart upload to " + absoluteUrl + " (HTTP " + juce::String(result.httpStatus) + ")");

    updateConnectionStatus(result.success ? ConnectionStatus::Connected : ConnectionStatus::Disconnected);

    return result;
}

//==============================================================================
// Generic HTTP methods
//==============================================================================
void NetworkClient::get(const juce::String& endpoint, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);
        auto outcome = requestResultToOutcome(result);

        juce::MessageManager::callAsync([callback, outcome]() {
            callback(outcome);
        });
    });
}

void NetworkClient::post(const juce::String& endpoint, const juce::var& data, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, endpoint, data, callback]() {
        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        auto outcome = requestResultToOutcome(result);

        juce::MessageManager::callAsync([callback, outcome]() {
            callback(outcome);
        });
    });
}

void NetworkClient::put(const juce::String& endpoint, const juce::var& data, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, endpoint, data, callback]() {
        auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
        auto outcome = requestResultToOutcome(result);

        juce::MessageManager::callAsync([callback, outcome]() {
            callback(outcome);
        });
    });
}

void NetworkClient::del(const juce::String& endpoint, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
        auto outcome = requestResultToOutcome(result);

        juce::MessageManager::callAsync([callback, outcome]() {
            callback(outcome);
        });
    });
}

//==============================================================================
// Absolute URL methods (for CDN, external APIs, etc.)
//==============================================================================
void NetworkClient::getAbsolute(const juce::String& absoluteUrl, ResponseCallback callback,
                                 const juce::StringPairArray& customHeaders)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, absoluteUrl, callback, customHeaders]() {
        RequestResult result = makeAbsoluteRequestWithRetry(absoluteUrl, "GET", juce::var(), false, customHeaders);
        auto outcome = requestResultToOutcome(result);

        juce::MessageManager::callAsync([callback, outcome]() {
            callback(outcome);
        });
    });
}

void NetworkClient::postAbsolute(const juce::String& absoluteUrl, const juce::var& data, ResponseCallback callback,
                                  const juce::StringPairArray& customHeaders)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, absoluteUrl, data, callback, customHeaders]() {
        RequestResult result = makeAbsoluteRequestWithRetry(absoluteUrl, "POST", data, false, customHeaders);
        auto outcome = requestResultToOutcome(result);

        juce::MessageManager::callAsync([callback, outcome]() {
            callback(outcome);
        });
    });
}

void NetworkClient::uploadMultipartAbsolute(const juce::String& absoluteUrl,
                                            const juce::String& fieldName,
                                            const juce::MemoryBlock& fileData,
                                            const juce::String& fileName,
                                            const juce::String& mimeType,
                                            const std::map<juce::String, juce::String>& extraFields,
                                            MultipartUploadCallback callback,
                                            const juce::StringPairArray& customHeaders)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, absoluteUrl, fieldName, fileData, fileName, mimeType, extraFields, callback, customHeaders]() {
        RequestResult result = uploadMultipartDataAbsolute(absoluteUrl, fieldName, fileData, fileName, mimeType, extraFields, customHeaders);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getBinaryAbsolute(const juce::String& absoluteUrl, BinaryDataCallback callback,
                                      const juce::StringPairArray& customHeaders)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, absoluteUrl, callback, customHeaders]() {
        juce::MemoryBlock data;
        bool success = false;

        RequestResult result = makeAbsoluteRequestWithRetry(absoluteUrl, "GET", juce::var(), false, customHeaders, &data);

        if (result.success && data.getSize() > 0)
        {
            success = true;
        }

        juce::MessageManager::callAsync([callback, success, data, result]() {
            if (success)
                callback(Outcome<juce::MemoryBlock>::ok(data));
            else
                callback(Outcome<juce::MemoryBlock>::error(result.getUserFriendlyError()));
        });
    });
}

//==============================================================================
// Notification operations
//==============================================================================

void NetworkClient::getNotifications(int limit, int offset, NotificationCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/notifications") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        int unseen = 0;
        int unread = 0;
        juce::var groups;

        if (result.success && result.data.isObject())
        {
            unseen = static_cast<int>(result.data.getProperty("unseen", 0));
            unread = static_cast<int>(result.data.getProperty("unread", 0));
            groups = result.data.getProperty("groups", juce::var());
        }

        juce::MessageManager::callAsync([callback, result, groups, unseen, unread]() {
            if (result.isSuccess())
            {
                NotificationResult notifResult;
                notifResult.notifications = groups;
                notifResult.unseen = unseen;
                notifResult.unread = unread;
                callback(Outcome<NotificationResult>::ok(notifResult));
            }
            else
            {
                callback(Outcome<NotificationResult>::error(result.getUserFriendlyError()));
            }
        });
    });
}

void NetworkClient::getNotificationCounts(std::function<void(int unseen, int unread)> callback)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, callback]() {
        auto result = makeRequestWithRetry(buildApiPath("/notifications/counts"), "GET", juce::var(), true);

        int unseen = 0;
        int unread = 0;

        if (result.success && result.data.isObject())
        {
            unseen = static_cast<int>(result.data.getProperty("unseen", 0));
            unread = static_cast<int>(result.data.getProperty("unread", 0));
        }

        juce::MessageManager::callAsync([callback, unseen, unread]() {
            callback(unseen, unread);
        });
    });
}

void NetworkClient::markNotificationsRead(ResponseCallback callback)
{
    Async::runVoid([this, callback]() {
        auto result = makeRequestWithRetry(buildApiPath("/notifications/read"), "POST", juce::var(), true);

        if (callback != nullptr)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::markNotificationsSeen(ResponseCallback callback)
{
    Async::runVoid([this, callback]() {
        auto result = makeRequestWithRetry(buildApiPath("/notifications/seen"), "POST", juce::var(), true);

        if (callback != nullptr)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

//==============================================================================
// User Discovery operations
//==============================================================================

void NetworkClient::searchUsers(const juce::String& query, int limit, int offset, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    // URL-encode the query string
    juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
    juce::String endpoint = buildApiPath("/search/users") + "?q=" + encodedQuery
                          + "&limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getTrendingUsers(int limit, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/discover/trending") + "?limit=" + juce::String(limit);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getFeaturedProducers(int limit, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/discover/featured") + "?limit=" + juce::String(limit);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getSuggestedUsers(int limit, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/discover/suggested") + "?limit=" + juce::String(limit);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getUsersByGenre(const juce::String& genre, int limit, int offset, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    // URL-encode the genre
    juce::String encodedGenre = juce::URL::addEscapeChars(genre, true);
    juce::String endpoint = buildApiPath("/discover/genre") + "/" + encodedGenre
                          + "?limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getAvailableGenres(ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    Async::runVoid([this, callback]() {
        auto result = makeRequestWithRetry(buildApiPath("/discover/genres"), "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getSimilarUsers(const juce::String& userId, int limit, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/users") + "/" + userId + "/similar?limit=" + juce::String(limit);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

//==============================================================================
// Search operations
//==============================================================================

void NetworkClient::searchPosts(const juce::String& query,
                                const juce::String& genre,
                                int bpmMin,
                                int bpmMax,
                                const juce::String& key,
                                int limit,
                                int offset,
                                ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    // Build query string with filters
    juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
    juce::String endpoint = buildApiPath("/search/posts") + "?q=" + encodedQuery
                          + "&limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    if (!genre.isEmpty())
    {
        juce::String encodedGenre = juce::URL::addEscapeChars(genre, true);
        endpoint += "&genre=" + encodedGenre;
    }

    if (bpmMin > 0)
        endpoint += "&bpm_min=" + juce::String(bpmMin);

    if (bpmMax < 200)
        endpoint += "&bpm_max=" + juce::String(bpmMax);

    if (!key.isEmpty())
    {
        juce::String encodedKey = juce::URL::addEscapeChars(key, true);
        endpoint += "&key=" + encodedKey;
    }

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getSearchSuggestions(const juce::String& query, int limit, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String encodedQuery = juce::URL::addEscapeChars(query, true);
    juce::String endpoint = buildApiPath("/search/suggestions") + "?q=" + encodedQuery
                          + "&limit=" + juce::String(limit);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

//==============================================================================
// Stories operations

void NetworkClient::getStoriesFeed(ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, callback]() {
        auto result = makeRequestWithRetry(buildApiPath("/stories/feed"), "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::viewStory(const juce::String& storyId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, storyId, callback]() {
        juce::String endpoint = buildApiPath("/stories/") + storyId + "/view";
        auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::uploadStory(const juce::AudioBuffer<float>& audioBuffer,
                                 double sampleRate,
                                 const juce::var& midiData,
                                 int bpm,
                                 const juce::String& key,
                                 const juce::StringArray& genres,
                                 ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, audioBuffer, sampleRate, midiData, bpm, key, genres, callback]() {
        // Encode audio to MP3
        juce::MemoryBlock mp3Data = encodeAudioToMP3(audioBuffer, sampleRate);

        if (mp3Data.isEmpty())
        {
            Log::error("NetworkClient::uploadStory: Failed to encode audio");
            if (callback)
            {
                juce::MessageManager::callAsync([callback]() {
                    callback(Outcome<juce::var>::error("Failed to encode audio"));
                });
            }
            return;
        }

        // Build request with audio and MIDI data
        std::map<juce::String, juce::String> extraFields;
        if (midiData.isObject())
        {
            extraFields["midi_data"] = juce::JSON::toString(midiData);
        }

        // Calculate duration
        double durationSeconds = audioBuffer.getNumSamples() / sampleRate;
        extraFields["duration"] = juce::String(durationSeconds);

        // Add metadata if provided
        if (bpm > 0)
            extraFields["bpm"] = juce::String(bpm);
        if (key.isNotEmpty())
            extraFields["key"] = key;
        if (genres.size() > 0)
            extraFields["genre"] = genres.joinIntoString(",");

        auto result = uploadMultipartData(buildApiPath("/stories"),
                                          "audio",
                                          mp3Data,
                                          "story.mp3",
                                          "audio/mpeg",
                                          extraFields);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::getStoryViews(const juce::String& storyId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, storyId, callback]() {
        juce::String path = "/stories/" + storyId + "/views";
        auto result = makeRequestWithRetry(buildApiPath(path.toRawUTF8()), "GET", juce::var(), true);

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

//==============================================================================
// Profile operations

void NetworkClient::changeUsername(const juce::String& newUsername, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, newUsername, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("username", newUsername);

        auto result = makeRequestWithRetry(buildApiPath("/users/username"), "PUT", data, true);
        Log::debug("Change username response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::getFollowers(const juce::String& userId, int limit, int offset, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/users") + "/" + userId + "/followers?limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

void NetworkClient::getFollowing(const juce::String& userId, int limit, int offset, ResponseCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/users") + "/" + userId + "/following?limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        juce::MessageManager::callAsync([callback, result]() {
            auto outcome = requestResultToOutcome(result);
            callback(outcome);
        });
    });
}

//==============================================================================
// Comment operations

void NetworkClient::getComments(const juce::String& postId, int limit, int offset, CommentsListCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments?limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        int totalCount = 0;
        juce::var comments;

        if (result.isSuccess() && result.data.isObject())
        {
            comments = result.data.getProperty("comments", juce::var());
            totalCount = static_cast<int>(result.data.getProperty("total_count", 0));
        }

        juce::MessageManager::callAsync([callback, result, comments, totalCount]() {
            if (result.isSuccess())
                callback(Outcome<std::pair<juce::var, int>>::ok({comments, totalCount}));
            else
                callback(Outcome<std::pair<juce::var, int>>::error(result.getUserFriendlyError()));
        });
    });
}

void NetworkClient::createComment(const juce::String& postId, const juce::String& content,
                                  const juce::String& parentId, CommentCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, postId, content, parentId, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("content", content);

        if (parentId.isNotEmpty())
            data.getDynamicObject()->setProperty("parent_id", parentId);

        juce::String endpoint = buildApiPath("/posts") + "/" + postId + "/comments";
        auto result = makeRequestWithRetry(endpoint, "POST", data, true);
        Log::debug("Create comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::getCommentReplies(const juce::String& commentId, int limit, int offset, CommentsListCallback callback)
{
    if (callback == nullptr)
        return;

    juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/replies?limit=" + juce::String(limit)
                          + "&offset=" + juce::String(offset);

    Async::runVoid([this, endpoint, callback]() {
        auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

        int totalCount = 0;
        juce::var replies;

        if (result.isSuccess() && result.data.isObject())
        {
            replies = result.data.getProperty("replies", juce::var());
            totalCount = static_cast<int>(result.data.getProperty("total_count", 0));
        }

        juce::MessageManager::callAsync([callback, result, replies, totalCount]() {
            if (result.isSuccess())
                callback(Outcome<std::pair<juce::var, int>>::ok({replies, totalCount}));
            else
                callback(Outcome<std::pair<juce::var, int>>::error(result.getUserFriendlyError()));
        });
    });
}

void NetworkClient::updateComment(const juce::String& commentId, const juce::String& content, CommentCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, content, callback]() {
        juce::var data = juce::var(new juce::DynamicObject());
        data.getDynamicObject()->setProperty("content", content);

        juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
        auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
        Log::debug("Update comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::deleteComment(const juce::String& commentId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, callback]() {
        juce::String endpoint = buildApiPath("/comments") + "/" + commentId;
        auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
        Log::debug("Delete comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::likeComment(const juce::String& commentId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, callback]() {
        juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
        auto result = makeRequestWithRetry(endpoint, "POST", juce::var(), true);
        Log::debug("Like comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}

void NetworkClient::unlikeComment(const juce::String& commentId, ResponseCallback callback)
{
    if (!isAuthenticated())
    {
        if (callback)
            callback(Outcome<juce::var>::error(Constants::Errors::NOT_AUTHENTICATED));
        return;
    }

    Async::runVoid([this, commentId, callback]() {
        juce::String endpoint = buildApiPath("/comments") + "/" + commentId + "/like";
        auto result = makeRequestWithRetry(endpoint, "DELETE", juce::var(), true);
        Log::debug("Unlike comment response: " + juce::JSON::toString(result.data));

        if (callback)
        {
            juce::MessageManager::callAsync([callback, result]() {
                auto outcome = requestResultToOutcome(result);
                callback(outcome);
            });
        }
    });
}