//==============================================================================
// NetworkClient.cpp - Core HTTP functionality
// Domain-specific API methods are split into separate files in api/ folder
//==============================================================================

#include "NetworkClient.h"
#include "../util/HttpErrorHandler.h"
#include "../util/Log.h"
#include "../util/Async.h"
#include "../util/Result.h"

//==============================================================================
// Helper to convert RequestResult to Outcome<juce::var> for type-safe error handling
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
    Log::info("NetworkClient initialized with base URL: " + config.baseUrl);
    Log::debug("  Timeout: " + juce::String(config.timeoutMs) + "ms, Max retries: " + juce::String(config.maxRetries));
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
    Log::info("NetworkClient config updated - base URL: " + config.baseUrl);
}

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

//==============================================================================
// Audio encoding
juce::MemoryBlock NetworkClient::encodeAudioToMP3(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // NOT YET IMPLEMENTED - This function currently falls back to WAV encoding.
    // The server will transcode WAV to MP3, but this is less efficient than encoding client-side.
    Log::warn("MP3 encoding not yet implemented, using WAV format");
    return encodeAudioToWAV(buffer, sampleRate);
}

juce::MemoryBlock NetworkClient::encodeAudioToWAV(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // Encode audio buffer to WAV format using 16-bit PCM.
    Log::debug("encodeAudioToWAV: Starting - samples: " + juce::String(buffer.getNumSamples()) +
               ", channels: " + juce::String(buffer.getNumChannels()) +
               ", sampleRate: " + juce::String(sampleRate));

    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        Log::error("encodeAudioToWAV: Invalid buffer - empty or no channels");
        return juce::MemoryBlock();
    }

    if (sampleRate <= 0)
    {
        Log::error("encodeAudioToWAV: Invalid sample rate: " + juce::String(sampleRate));
        return juce::MemoryBlock();
    }

    // IMPORTANT: AudioFormatWriter takes ownership of the output stream and deletes it!
    // We must allocate the stream on the heap and let the writer own it.
    juce::MemoryBlock resultBlock;
    auto* outputStream = new juce::MemoryOutputStream(resultBlock, false);

    juce::WavAudioFormat wavFormat;
    juce::StringPairArray metadata; // Explicit empty metadata

    // Writer takes ownership of outputStream and will delete it
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream, sampleRate,
                                  static_cast<unsigned int>(buffer.getNumChannels()),
                                  16, // bits per sample
                                  metadata, 0));

    if (writer == nullptr)
    {
        // If writer creation failed, we still own the stream
        delete outputStream;
        Log::error("encodeAudioToWAV: Failed to create WAV writer");
        return juce::MemoryBlock();
    }

    Log::debug("encodeAudioToWAV: WAV writer created, writing samples...");

    // Write audio data
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        Log::error("encodeAudioToWAV: Failed to write audio data to WAV");
        return juce::MemoryBlock();
    }

    Log::debug("encodeAudioToWAV: Audio data written, flushing...");

    // Explicitly flush before writer destructor
    writer->flush();

    // Writer destructor will delete the outputStream, but resultBlock still has the data
    writer.reset();

    Log::debug("encodeAudioToWAV: Encoded " + juce::String(buffer.getNumSamples()) + " samples to WAV ("
        + juce::String(resultBlock.getSize()) + " bytes)");

    if (resultBlock.getSize() == 0)
    {
        Log::error("encodeAudioToWAV: Result block is empty after encoding");
        return juce::MemoryBlock();
    }

    return resultBlock;
}

//==============================================================================
juce::String NetworkClient::detectDAWName()
{
    // Try to detect DAW from process name or environment
    // This is platform-specific and may not always work

    #if JUCE_MAC
        // On macOS, try to get the parent process name
        juce::String processName = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getParentDirectory().getParentDirectory().getFileName();

        // Common DAW identifiers on macOS
        if (processName.containsIgnoreCase("Ableton"))
            return "Ableton Live";
        if (processName.containsIgnoreCase("Logic"))
            return "Logic Pro";
        if (processName.containsIgnoreCase("Pro Tools"))
            return "Pro Tools";
        if (processName.containsIgnoreCase("Cubase"))
            return "Cubase";
        if (processName.containsIgnoreCase("Studio One"))
            return "Studio One";
        if (processName.containsIgnoreCase("Reaper"))
            return "REAPER";
        if (processName.containsIgnoreCase("Bitwig"))
            return "Bitwig Studio";
        if (processName.containsIgnoreCase("FL Studio"))
            return "FL Studio";
    #elif JUCE_WINDOWS
        // On Windows, try to get parent process name from environment
        // Note: This is limited on Windows without process enumeration
        // In practice, the DAW name might need to be passed from the processor
    #elif JUCE_LINUX
        // On Linux, similar limitations apply
    #endif

    // Fallback: Try to detect from JUCE plugin wrapper info
    // Some hosts provide this information, but it's not always available
    if (auto* app = juce::JUCEApplication::getInstance())
    {
        juce::String hostName = app->getApplicationName();

        if (hostName.isNotEmpty())
        {
            if (hostName.containsIgnoreCase("Ableton"))
                return "Ableton Live";
            if (hostName.containsIgnoreCase("Logic"))
                return "Logic Pro";
            if (hostName.containsIgnoreCase("Pro Tools"))
                return "Pro Tools";
            if (hostName.containsIgnoreCase("Cubase"))
                return "Cubase";
            if (hostName.containsIgnoreCase("Studio One"))
                return "Studio One";
            if (hostName.containsIgnoreCase("Reaper"))
                return "REAPER";
            if (hostName.containsIgnoreCase("Bitwig"))
                return "Bitwig Studio";
            if (hostName.containsIgnoreCase("FL Studio"))
                return "FL Studio";
            if (hostName.containsIgnoreCase("Audacity"))
                return "Audacity";
        }
    }

    // Default fallback
    return "Unknown";
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

    juce::String fullUrl = config.baseUrl + endpoint;

    Log::debug("uploadMultipartData: URL = " + fullUrl);
    Log::debug("uploadMultipartData: File size = " + juce::String(fileData.getSize()) + " bytes");

    // Write file data to a temporary file (required for JUCE's withFileToUpload)
    // Use abs() to avoid negative numbers which can cause JUCE String assertion failures
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("sidechain_upload_" + juce::String(std::abs(juce::Random::getSystemRandom().nextInt64())) + "_" + fileName);

    if (!tempFile.replaceWithData(fileData.getData(), fileData.getSize()))
    {
        result.errorMessage = "Failed to create temporary file for upload";
        Log::error("uploadMultipartData: " + result.errorMessage);
        return result;
    }

    Log::debug("uploadMultipartData: Created temp file: " + tempFile.getFullPathName());

    // Build URL with file upload using JUCE's proper multipart encoding
    juce::URL url(fullUrl);

    // Add extra form fields as parameters
    for (const auto& field : extraFields)
    {
        url = url.withParameter(field.first, field.second);
    }

    // Add file upload - JUCE handles multipart encoding automatically
    url = url.withFileToUpload(fieldName, tempFile, mimeType);

    // Build headers
    juce::String headers = "Authorization: Bearer " + authToken + "\r\n";

    // Create request options - use inPostData so parameters go in the multipart body
    juce::StringPairArray responseHeaders;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(config.timeoutMs * 2)
                       .withResponseHeaders(&responseHeaders);

    Log::debug("uploadMultipartData: Making request with JUCE multipart encoding...");

    // Make request
    activeRequestCount++;
    auto stream = url.createInputStream(options);
    activeRequestCount--;

    // Clean up temp file
    tempFile.deleteFile();

    Log::debug("uploadMultipartData: Stream created = " + juce::String(stream != nullptr ? "yes" : "no"));

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

    Log::debug("uploadMultipartDataAbsolute: URL = " + absoluteUrl);
    Log::debug("uploadMultipartDataAbsolute: File size = " + juce::String(fileData.getSize()) + " bytes");

    // Write file data to a temporary file (required for JUCE's withFileToUpload)
    // Use abs() to avoid negative numbers which can cause JUCE String assertion failures
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("sidechain_upload_" + juce::String(std::abs(juce::Random::getSystemRandom().nextInt64())) + "_" + fileName);

    if (!tempFile.replaceWithData(fileData.getData(), fileData.getSize()))
    {
        result.errorMessage = "Failed to create temporary file for upload";
        Log::error("uploadMultipartDataAbsolute: " + result.errorMessage);
        return result;
    }

    Log::debug("uploadMultipartDataAbsolute: Created temp file: " + tempFile.getFullPathName());

    // Build URL with file upload using JUCE's proper multipart encoding
    juce::URL url(absoluteUrl);

    // Add extra form fields as parameters
    for (const auto& field : extraFields)
    {
        url = url.withParameter(field.first, field.second);
    }

    // Add file upload - JUCE handles multipart encoding automatically
    url = url.withFileToUpload(fieldName, tempFile, mimeType);

    // Build headers from custom headers
    juce::String headers;
    for (int i = 0; i < customHeaders.size(); i++)
    {
        auto key = customHeaders.getAllKeys()[i];
        auto value = customHeaders[key];
        headers += key + ": " + value + "\r\n";
    }

    // Create request options - use inPostData so parameters go in the multipart body
    juce::StringPairArray responseHeaders;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(config.timeoutMs)
                       .withResponseHeaders(&responseHeaders);

    Log::debug("uploadMultipartDataAbsolute: Making request with JUCE multipart encoding...");

    // Make request
    activeRequestCount++;
    auto stream = url.createInputStream(options);
    activeRequestCount--;

    // Clean up temp file
    tempFile.deleteFile();

    if (stream == nullptr)
    {
        result.errorMessage = "Failed to connect to server";
        Log::error("uploadMultipartDataAbsolute: " + result.errorMessage);
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

    Log::debug("Multipart upload to " + absoluteUrl + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

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
        auto outcome = requestResultToOutcome(result);

        juce::MessageManager::callAsync([callback, outcome]() {
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
