// ==============================================================================
// NetworkClient.cpp - Core HTTP functionality
// Domain-specific API methods are split into separate files in api/ folder
// ==============================================================================

#include "NetworkClient.h"
#include "../models/User.h"
#include "../util/Async.h"
#include "../util/HttpErrorHandler.h"
#include "../util/Json.h"
#include "../util/Log.h"
#include "../util/Result.h"
#include "../util/error/ErrorTracking.h"
#include "../util/profiling/PerformanceMonitor.h"

// ==============================================================================
// Helper to create JSON POST body from nlohmann::json
// This function ensures clean JSON string for POST bodies.
static juce::MemoryBlock createJsonPostBody(const nlohmann::json &data) {
  if (data.is_null() || data.empty()) {
    return juce::MemoryBlock();
  }
  std::string jsonString = data.dump();
  return juce::MemoryBlock(jsonString.data(), jsonString.size());
}

// ==============================================================================
// Helper to convert RequestResult to Outcome<nlohmann::json> for type-safe error handling
static Outcome<nlohmann::json> requestResultToOutcome(const NetworkClient::RequestResult &result) {
  // Check if response JSON contains an error field (even if HTTP status looks successful)
  bool hasErrorInBody = false;
  if (result.data.is_object() && result.data.contains("error")) {
    auto errorField = result.data["error"];
    if (errorField.is_string()) {
      hasErrorInBody = !errorField.get<std::string>().empty();
      if (hasErrorInBody) {
        Log::debug("requestResultToOutcome: Detected error in response body: " +
                   juce::String(errorField.get<std::string>()));
      }
    }
  }

  Log::debug("requestResultToOutcome: success=" + juce::String(result.success ? "true" : "false") +
             ", isSuccess=" + juce::String(result.isSuccess() ? "true" : "false") +
             ", hasErrorInBody=" + juce::String(hasErrorInBody ? "true" : "false"));

  if (result.success && result.isSuccess() && !hasErrorInBody) {
    return Outcome<nlohmann::json>::ok(result.data);
  } else {
    juce::String errorMsg = result.getUserFriendlyError();
    if (errorMsg.isEmpty())
      errorMsg = "Request failed (HTTP " + juce::String(result.httpStatus) + ")";
    Log::debug("requestResultToOutcome: Returning error: " + errorMsg);
    return Outcome<nlohmann::json>::error(errorMsg);
  }
}

// ==============================================================================
// RequestResult helper methods
juce::String NetworkClient::RequestResult::getUserFriendlyError() const {
  // Try to extract error message from JSON response
  if (data.is_object()) {
    // Check common error field names - first try string error
    if (data.contains("error")) {
      auto errorField = data["error"];
      if (errorField.is_string()) {
        juce::String errorStr(errorField.get<std::string>());
        if (!errorStr.isEmpty())
          return errorStr;
      }
      // Nested error object
      if (errorField.is_object() && errorField.contains("message")) {
        auto msgField = errorField["message"];
        if (msgField.is_string()) {
          juce::String errorMsg(msgField.get<std::string>());
          if (!errorMsg.isEmpty())
            return errorMsg;
        }
      }
    }

    if (data.contains("message")) {
      auto msgField = data["message"];
      if (msgField.is_string()) {
        juce::String message(msgField.get<std::string>());
        if (!message.isEmpty())
          return message;
      }
    }
  }

  // Fall back to HTTP status-based messages
  switch (httpStatus) {
  case 400:
    return "Invalid request - please check your input";
  case 401:
    return "Authentication required - please log in";
  case 403:
    return "Access denied - you don't have permission";
  case 404:
    return "Not found - the requested resource doesn't exist";
  case 409:
    return "Conflict - this action conflicts with existing data";
  case 422:
    return "Validation failed - please check your input";
  case 429:
    return "Too many requests - please try again later";
  case 500:
    return "Server error - please try again later";
  case 502:
    return "Server unavailable - please try again later";
  case 503:
    return "Service temporarily unavailable";
  default:
    if (!errorMessage.isEmpty())
      return errorMessage;
    if (httpStatus >= 400)
      return "Request failed (HTTP " + juce::String(httpStatus) + ")";
    return "Unknown error occurred";
  }
}

int NetworkClient::parseStatusCode(const juce::StringPairArray &headers) {
  // JUCE stores the status line in a special key
  for (auto &key : headers.getAllKeys()) {
    if (key.startsWithIgnoreCase("HTTP/")) {
      // Parse "HTTP/1.1 200 OK" format
      auto statusLine = headers[key];
      auto parts = juce::StringArray::fromTokens(statusLine, " ", "");
      if (parts.size() >= 2) {
        int statusCode = parts[1].getIntValue();
        Log::debug("parseStatusCode: Found HTTP status '" + statusLine + "' -> " + juce::String(statusCode));
        return statusCode;
      }
    }
  }

  // If we still can't find it, log all headers for debugging
  Log::warn("parseStatusCode: Failed to parse status code. Headers present:");
  for (auto &key : headers.getAllKeys()) {
    Log::warn("  Header[" + key + "] = " + headers[key]);
  }
  return 0;
}

// ==============================================================================
NetworkClient::NetworkClient(const Config &cfg) : config(cfg) {
  Log::info("NetworkClient initialized with base URL: " + config.baseUrl);
  Log::debug("  Timeout: " + juce::String(config.timeoutMs) + "ms, Max retries: " + juce::String(config.maxRetries));

  // Initialize rate limiters
  using namespace Sidechain::Security;

  // API rate limiter: 100 requests per 60 seconds with 20 burst allowance
  apiRateLimiter = RateLimiter::create()->setRate(100)->setWindow(60)->setBurstSize(20)->setAlgorithm(
      RateLimiter::Algorithm::TokenBucket);

  // Upload rate limiter: 10 uploads per hour (3600 seconds) with 3 burst
  // allowance
  uploadRateLimiter = RateLimiter::create()->setRate(10)->setWindow(3600)->setBurstSize(3)->setAlgorithm(
      RateLimiter::Algorithm::TokenBucket);

  Log::info("Rate limiters initialized: API (100/60s), Uploads (10/hour)");
}

NetworkClient::~NetworkClient() {
  cancelAllRequests();
}

// ==============================================================================
void NetworkClient::setConnectionStatusCallback(ConnectionStatusCallback callback) {
  connectionStatusCallback = callback;
}

void NetworkClient::updateConnectionStatus(ConnectionStatus status) {
  auto previousStatus = connectionStatus.exchange(status);
  if (previousStatus != status && connectionStatusCallback) {
    juce::MessageManager::callAsync([this, status]() {
      if (connectionStatusCallback)
        connectionStatusCallback(status);
    });
  }
}

void NetworkClient::checkConnection() {
  updateConnectionStatus(ConnectionStatus::Connecting);

  Async::runVoid([this]() {
    if (shuttingDown.load())
      return;

    auto result = makeRequestWithRetry("/health", "GET", nlohmann::json(), false);

    juce::MessageManager::callAsync([this, result]() {
      if (result.success) {
        updateConnectionStatus(ConnectionStatus::Connected);
        Log::debug("Connection check: Connected to backend");
      } else {
        updateConnectionStatus(ConnectionStatus::Disconnected);
        Log::warn("Connection check: Failed - " + result.errorMessage);
      }
    });
  });
}

void NetworkClient::cancelAllRequests() {
  shuttingDown.store(true);
  // Wait for active requests to complete (with timeout)
  int waitCount = 0;
  while (activeRequestCount.load() > 0 && waitCount < 50) {
    juce::Thread::sleep(100);
    waitCount++;
  }
  shuttingDown.store(false);
}

void NetworkClient::setConfig(const Config &newConfig) {
  config = newConfig;
  Log::info("NetworkClient config updated - base URL: " + config.baseUrl);
}

void NetworkClient::setAuthToken(const juce::String &token) {
  authToken = token;
}

// ==============================================================================
// Core request method with retry logic
NetworkClient::RequestResult NetworkClient::makeRequestWithRetry(const juce::String &endpoint,
                                                                 const juce::String &method, const nlohmann::json &data,
                                                                 bool requireAuth) {
  SCOPED_TIMER_THRESHOLD("network::api_call", 2000.0);
  RequestResult result;

  // Rate limiting check
  if (apiRateLimiter) {
    // Use user ID as identifier, or "anonymous" if not authenticated
    juce::String identifier = currentUserId.isEmpty() ? "anonymous" : currentUserId;
    auto rateLimitStatus = apiRateLimiter->tryConsume(identifier, 1);

    if (!rateLimitStatus.allowed) {
      result.success = false;
      result.httpStatus = 429; // HTTP 429 Too Many Requests

      // Calculate retry time
      int retrySeconds =
          rateLimitStatus.retryAfterSeconds > 0 ? rateLimitStatus.retryAfterSeconds : rateLimitStatus.resetInSeconds;

      juce::String retryMsg = retrySeconds > 0 ? " Please try again in " + juce::String(retrySeconds) + " seconds."
                                               : " Please try again later.";

      result.errorMessage = "Too many requests" + retryMsg;

      // Create error response JSON
      result.data = {{"error", "rate_limit_exceeded"},
                     {"message", result.errorMessage.toStdString()},
                     {"retry_after", retrySeconds},
                     {"limit", rateLimitStatus.limit},
                     {"remaining", rateLimitStatus.remaining}};

      Log::warn("Rate limit exceeded for " + identifier + ": " + result.errorMessage);

      // Report rate limit error
      HttpErrorHandler::getInstance().reportError(endpoint, method, 429, result.errorMessage,
                                                  juce::String(result.data.dump()));

      // Track rate limit error
      using namespace Sidechain::Util::Error;
      auto errorTracker = ErrorTracker::getInstance();
      errorTracker->recordError(ErrorSource::Network, "Rate limit exceeded: " + result.errorMessage,
                                ErrorSeverity::Warning,
                                {{"endpoint", endpoint},
                                 {"method", method},
                                 {"user_id", identifier},
                                 {"limit", juce::String(rateLimitStatus.limit)},
                                 {"remaining", juce::String(rateLimitStatus.remaining)},
                                 {"retry_after", juce::String(retrySeconds)}});

      return result;
    }

    Log::debug("Rate limit OK for " + identifier + " - remaining: " + juce::String(rateLimitStatus.remaining) + "/" +
               juce::String(rateLimitStatus.limit));
  }

  int attempts = 0;

  while (attempts < config.maxRetries && !shuttingDown.load()) {
    attempts++;
    activeRequestCount++;

    juce::URL url(config.baseUrl + endpoint);

    // Build headers string
    juce::String headers = "Content-Type: application/json\r\n";

    if (requireAuth && !authToken.isEmpty()) {
      headers += "Authorization: Bearer " + authToken + "\r\n";
    }

    // Create request options with response headers capture
    juce::StringPairArray responseHeaders;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(config.timeoutMs)
                       .withResponseHeaders(&responseHeaders);

    // For POST/PUT/DELETE requests, add data to URL
    if (method == "POST" || method == "PUT" || method == "DELETE") {
      if (!data.is_null() && !data.empty()) {
        // Serialize nlohmann::json to string
        std::string jsonString = data.dump();
        Log::debug("POST data: " + juce::String(jsonString) +
                   " (size: " + juce::String(static_cast<int>(jsonString.size())) + " bytes)");

        // Create MemoryBlock from JSON string
        juce::MemoryBlock jsonBody(jsonString.data(), jsonString.size());
        url = url.withPOSTData(jsonBody);
      } else if (method == "POST") {
        // Empty POST body
        url = url.withPOSTData(juce::MemoryBlock());
      }
    }

    // Make request
    Log::debug("Making " + method + " request to: " + url.toString(true));
    auto stream = url.createInputStream(options);

    activeRequestCount--;

    if (shuttingDown.load()) {
      result.errorMessage = "Request cancelled";
      return result;
    }

    if (stream == nullptr) {
      result.errorMessage = "Failed to connect to server";
      Log::debug("Request attempt " + juce::String(attempts) + "/" + juce::String(config.maxRetries) +
                 " failed for: " + endpoint);

      if (attempts < config.maxRetries) {
        // Wait before retry with exponential backoff
        int delay = config.retryDelayMs * attempts;
        juce::Thread::sleep(delay);
        continue;
      }

      // Report connection error after all retries exhausted
      HttpErrorHandler::getInstance().reportError(endpoint, method, 0, result.errorMessage, "");

      // Track connection error
      using namespace Sidechain::Util::Error;
      auto errorTracker = ErrorTracker::getInstance();
      errorTracker->recordError(ErrorSource::Network, "Connection failed: " + result.errorMessage, ErrorSeverity::Error,
                                {{"endpoint", endpoint},
                                 {"method", method},
                                 {"attempts", juce::String(attempts)},
                                 {"max_retries", juce::String(config.maxRetries)},
                                 {"base_url", config.baseUrl}});

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

    // Parse JSON response using nlohmann::json
    try {
      result.data = nlohmann::json::parse(response.toStdString());
    } catch (const nlohmann::json::parse_error &e) {
      result.data = nlohmann::json();
      Log::warn("JSON parse error: " + juce::String(e.what()));
    }
    result.success = result.isSuccess();

    Log::debug("API Response from " + endpoint + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

    // Check for server rate limiting (429) and honor Retry-After header
    if (result.httpStatus == 429 && attempts < config.maxRetries) {
      auto retryAfterHeader = responseHeaders.getValue("Retry-After", "");
      int retryDelaySeconds = 60; // Default to 60 seconds

      if (retryAfterHeader.isNotEmpty()) {
        // Try to parse as integer (seconds)
        retryDelaySeconds = retryAfterHeader.getIntValue();

        // If 0, might be HTTP-date format - use default
        if (retryDelaySeconds == 0)
          retryDelaySeconds = 60;
      }

      Log::warn("Server rate limit (429) on " + endpoint + ", retrying after " + juce::String(retryDelaySeconds) + "s");

      // Respect server's retry-after time
      juce::Thread::sleep(retryDelaySeconds * 1000);
      continue; // Retry the request
    }

    // Check for server errors that should trigger retry
    if (result.httpStatus >= 500 && attempts < config.maxRetries) {
      Log::warn("Server error, retrying...");
      int delay = config.retryDelayMs * attempts;
      juce::Thread::sleep(delay);
      continue;
    }

    // Report HTTP errors (4xx and 5xx status codes)
    if (result.httpStatus >= 400) {
      std::string dataStr = result.data.dump();
      HttpErrorHandler::getInstance().reportError(endpoint, method, result.httpStatus, result.getUserFriendlyError(),
                                                  juce::String(dataStr));

      // Track HTTP errors
      using namespace Sidechain::Util::Error;
      auto errorTracker = ErrorTracker::getInstance();

      // Determine severity based on status code
      ErrorSeverity severity = ErrorSeverity::Error;
      if (result.httpStatus >= 500)
        severity = ErrorSeverity::Critical; // Server errors are critical
      else if (result.httpStatus == 401 || result.httpStatus == 403)
        severity = ErrorSeverity::Error; // Auth errors
      else if (result.httpStatus >= 400)
        severity = ErrorSeverity::Warning; // Client errors

      errorTracker->recordError(ErrorSource::Network,
                                "HTTP " + juce::String(result.httpStatus) + ": " + result.getUserFriendlyError(),
                                severity,
                                {
                                    {"endpoint", endpoint},
                                    {"method", method},
                                    {"status_code", juce::String(result.httpStatus)},
                                    {"response", juce::String(dataStr).substring(0, 200)} // First 200 chars
                                });
    }

    // Update connection status based on result
    if (result.httpStatus >= 200 && result.httpStatus < 500) {
      updateConnectionStatus(ConnectionStatus::Connected);
    } else {
      updateConnectionStatus(ConnectionStatus::Disconnected);
    }

    return result;
  }

  return result;
}

NetworkClient::RequestResult NetworkClient::makeAbsoluteRequestWithRetry(const juce::String &absoluteUrl,
                                                                         const juce::String &method,
                                                                         const nlohmann::json &data, bool requireAuth,
                                                                         const juce::StringPairArray &customHeaders,
                                                                         juce::MemoryBlock *binaryData) {
  RequestResult result;
  int attempts = 0;

  while (attempts < config.maxRetries && !shuttingDown.load()) {
    attempts++;
    activeRequestCount++;

    juce::URL url(absoluteUrl);

    // Build headers string
    juce::String headers = "Content-Type: application/json\r\n";

    if (requireAuth && !authToken.isEmpty()) {
      headers += "Authorization: Bearer " + authToken + "\r\n";
    }

    // Add custom headers
    for (int i = 0; i < customHeaders.size(); i++) {
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
    if (method == "POST" || method == "PUT" || method == "DELETE") {
      if (!data.is_null() && !data.empty()) {
        // Use helper function to serialize nlohmann::json
        url = url.withPOSTData(createJsonPostBody(data));
      } else if (method == "POST") {
        // Empty POST body
        url = url.withPOSTData(juce::MemoryBlock());
      }
    }

    // Make request
    auto stream = url.createInputStream(options);

    activeRequestCount--;

    if (shuttingDown.load()) {
      result.errorMessage = "Request cancelled";
      return result;
    }

    if (stream == nullptr) {
      result.errorMessage = "Failed to connect to server";
      Log::debug("Absolute request attempt " + juce::String(attempts) + "/" + juce::String(config.maxRetries) +
                 " failed for: " + absoluteUrl);

      if (attempts < config.maxRetries) {
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
    if (binaryData != nullptr) {
      stream->readIntoMemoryBlock(*binaryData);
      result.success = result.isSuccess() && binaryData->getSize() > 0;
    } else {
      auto response = stream->readEntireStreamAsString();
      try {
        result.data = nlohmann::json::parse(response.toStdString());
      } catch (const nlohmann::json::parse_error &e) {
        result.data = nlohmann::json();
        Log::warn("JSON parse error: " + juce::String(e.what()));
      }
      result.success = result.isSuccess();
      Log::debug("Absolute URL Response from " + absoluteUrl + " (HTTP " + juce::String(result.httpStatus) + ")");
    }

    // Check for server errors that should trigger retry
    if (result.httpStatus >= 500 && attempts < config.maxRetries) {
      Log::warn("Server error, retrying...");
      int delay = config.retryDelayMs * attempts;
      juce::Thread::sleep(delay);
      continue;
    }

    // Update connection status based on result
    if (result.httpStatus >= 200 && result.httpStatus < 500) {
      updateConnectionStatus(ConnectionStatus::Connected);
    } else {
      updateConnectionStatus(ConnectionStatus::Disconnected);
    }

    return result;
  }

  return result;
}

NetworkClient::RequestResult NetworkClient::makeAbsoluteRequestSync(const juce::String &absoluteUrl,
                                                                    const juce::String &method,
                                                                    const nlohmann::json &data, bool requireAuth,
                                                                    const juce::StringPairArray &customHeaders,
                                                                    juce::MemoryBlock *binaryData) {
  RequestResult result;

  if (shuttingDown.load()) {
    result.errorMessage = "Request cancelled";
    return result;
  }

  activeRequestCount++;

  juce::URL url(absoluteUrl);

  // Build headers string
  juce::String headers = "Content-Type: application/json\r\n";

  if (requireAuth && !authToken.isEmpty()) {
    headers += "Authorization: Bearer " + authToken + "\r\n";
  }

  // Add custom headers
  for (int i = 0; i < customHeaders.size(); i++) {
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
  if (method == "POST" || method == "PUT" || method == "DELETE") {
    if (!data.is_null() && !data.empty()) {
      // Use helper function to serialize nlohmann::json
      url = url.withPOSTData(createJsonPostBody(data));
    } else if (method == "POST") {
      // Empty POST body
      url = url.withPOSTData(juce::MemoryBlock());
    }
  }

  // Make request
  auto stream = url.createInputStream(options);

  activeRequestCount--;

  if (shuttingDown.load()) {
    result.errorMessage = "Request cancelled";
    return result;
  }

  if (stream == nullptr) {
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
  if (binaryData != nullptr) {
    stream->readIntoMemoryBlock(*binaryData);
    result.success = result.isSuccess() && binaryData->getSize() > 0;

    Log::debug("NetworkClient: Binary download - URL: " + absoluteUrl + ", status: " + juce::String(result.httpStatus) +
               ", size: " + juce::String((int)binaryData->getSize()) + " bytes" +
               ", success: " + (result.success ? "true" : "false"));

    if (!result.success) {
      Log::warn("NetworkClient: Binary download failed - URL: " + absoluteUrl + ", status: " +
                juce::String(result.httpStatus) + ", isSuccess: " + (result.isSuccess() ? "true" : "false") +
                ", size: " + juce::String((int)binaryData->getSize()));
    }
  } else {
    auto response = stream->readEntireStreamAsString();
    try {
      result.data = nlohmann::json::parse(response.toStdString());
    } catch (const nlohmann::json::parse_error &e) {
      result.data = nlohmann::json();
      Log::warn("JSON parse error: " + juce::String(e.what()));
    }
    result.success = result.isSuccess();
  }

  return result;
}

nlohmann::json NetworkClient::makeRequest(const juce::String &endpoint, const juce::String &method,
                                          const nlohmann::json &data, bool requireAuth) {
  auto result = makeRequestWithRetry(endpoint, method, data, requireAuth);
  return result.data;
}

juce::String NetworkClient::getAuthHeader() const {
  return "Bearer " + authToken;
}

// ==============================================================================
// Helper to build API endpoint paths consistently
juce::String NetworkClient::buildApiPath(const char *path) {
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

void NetworkClient::handleAuthResponse(const nlohmann::json &response) {
  if (response.is_object()) {
    juce::String token;
    juce::String userId;

    if (response.contains("token") && response["token"].is_string()) {
      token = juce::String(response["token"].get<std::string>());
    }
    if (response.contains("user_id") && response["user_id"].is_string()) {
      userId = juce::String(response["user_id"].get<std::string>());
    }

    if (!token.isEmpty() && !userId.isEmpty()) {
      setAuthToken(token);

      if (authCallback) {
        juce::MessageManager::callAsync([this, token, userId]() {
          auto authResult = Outcome<std::pair<juce::String, juce::String>>::ok({token, userId});
          authCallback(authResult);
        });
      }
    }
  }
}

// ==============================================================================
// Audio encoding
juce::MemoryBlock NetworkClient::encodeAudioToMP3(const juce::AudioBuffer<float> &buffer, double sampleRate) {
  // NOT YET IMPLEMENTED - This function currently falls back to WAV encoding.
  // The server will transcode WAV to MP3, but this is less efficient than
  // encoding client-side.
  Log::warn("MP3 encoding not yet implemented, using WAV format");
  return encodeAudioToWAV(buffer, sampleRate);
}

juce::MemoryBlock NetworkClient::encodeAudioToWAV(const juce::AudioBuffer<float> &buffer, double sampleRate) {
  // Encode audio buffer to WAV format using 16-bit PCM.
  Log::debug("encodeAudioToWAV: Starting - samples: " + juce::String(buffer.getNumSamples()) +
             ", channels: " + juce::String(buffer.getNumChannels()) + ", sampleRate: " + juce::String(sampleRate));

  if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0) {
    Log::error("encodeAudioToWAV: Invalid buffer - empty or no channels");
    return juce::MemoryBlock();
  }

  if (sampleRate <= 0) {
    Log::error("encodeAudioToWAV: Invalid sample rate: " + juce::String(sampleRate));
    return juce::MemoryBlock();
  }

  // Create memory output stream - ownership will be transferred to writer
  juce::MemoryBlock resultBlock;
  std::unique_ptr<juce::OutputStream> outputStream(new juce::MemoryOutputStream(resultBlock, false));

  juce::WavAudioFormat wavFormat;

  // Create writer using the JUCE AudioFormatWriter API
  // JUCE 8.0.8 API: createWriterFor(stream, sampleRate, numChannels, bitsPerSample, metadata, quality)
  std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
      outputStream.get(), sampleRate, static_cast<unsigned int>(buffer.getNumChannels()), 16, {}, 0));

  if (writer == nullptr) {
    Log::error("encodeAudioToWAV: Failed to create WAV writer");
    return juce::MemoryBlock();
  }

  // Ownership of outputStream is transferred to writer
  outputStream.release();

  Log::debug("encodeAudioToWAV: WAV writer created, writing samples...");

  // Write audio data
  if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples())) {
    Log::error("encodeAudioToWAV: Failed to write audio data to WAV");
    return juce::MemoryBlock();
  }

  Log::debug("encodeAudioToWAV: Audio data written, flushing...");

  // Explicitly flush before writer destructor
  writer->flush();

  // Writer destructor will delete the outputStream, but resultBlock still has
  // the data
  writer.reset();

  Log::debug("encodeAudioToWAV: Encoded " + juce::String(buffer.getNumSamples()) + " samples to WAV (" +
             juce::String(resultBlock.getSize()) + " bytes)");

  if (resultBlock.getSize() == 0) {
    Log::error("encodeAudioToWAV: Result block is empty after encoding");
    return juce::MemoryBlock();
  }

  return resultBlock;
}

// ==============================================================================
juce::String NetworkClient::detectDAWName() {
  // Try to detect DAW from process name or environment
  // This is platform-specific and may not always work

#if JUCE_MAC
  // On macOS, try to get the parent process name
  juce::String processName = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                 .getParentDirectory()
                                 .getParentDirectory()
                                 .getParentDirectory()
                                 .getFileName();

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
  if (auto *app = juce::JUCEApplication::getInstance()) {
    juce::String hostName = app->getApplicationName();

    if (hostName.isNotEmpty()) {
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

// ==============================================================================
// Multipart form data upload helper
NetworkClient::RequestResult
NetworkClient::uploadMultipartData(const juce::String &endpoint, const juce::String &fieldName,
                                   const juce::MemoryBlock &fileData, const juce::String &fileName,
                                   const juce::String &mimeType,
                                   const std::map<juce::String, juce::String> &extraFields) {
  SCOPED_TIMER_THRESHOLD("network::upload", 5000.0);
  RequestResult result;

  // Rate limiting check - BEFORE authentication check to prevent
  // wasted resources
  if (uploadRateLimiter) {
    juce::String identifier = currentUserId.isEmpty() ? "anonymous" : currentUserId;
    auto rateLimitStatus = uploadRateLimiter->tryConsume(identifier, 1);

    if (!rateLimitStatus.allowed) {
      result.success = false;
      result.httpStatus = 429;

      int retrySeconds =
          rateLimitStatus.retryAfterSeconds > 0 ? rateLimitStatus.retryAfterSeconds : rateLimitStatus.resetInSeconds;

      juce::String retryMsg = retrySeconds > 0 ? " Please try again in " + juce::String(retrySeconds / 60) + " minutes."
                                               : " Please try again later.";

      result.errorMessage = "Upload rate limit exceeded" + retryMsg;

      result.data = {{"error", "upload_rate_limit_exceeded"},
                     {"message", result.errorMessage.toStdString()},
                     {"retry_after", retrySeconds},
                     {"limit", rateLimitStatus.limit},
                     {"remaining", rateLimitStatus.remaining}};

      Log::warn("Upload rate limit exceeded for " + identifier + ": " + result.errorMessage);

      HttpErrorHandler::getInstance().reportError(endpoint, "POST", 429, result.errorMessage,
                                                  juce::String(result.data.dump()));

      // Track error
      using namespace Sidechain::Util::Error;
      auto errorTracker = ErrorTracker::getInstance();
      errorTracker->recordError(ErrorSource::Network, "Upload rate limit exceeded: " + result.errorMessage,
                                ErrorSeverity::Warning,
                                {{"endpoint", endpoint},
                                 {"method", "POST"},
                                 {"identifier", identifier},
                                 {"retry_after", juce::String(retrySeconds)}});

      return result;
    }

    Log::debug("Upload rate limit check passed for " + identifier + " (" + juce::String(rateLimitStatus.remaining) +
               " uploads remaining)");
  }

  if (!isAuthenticated()) {
    result.errorMessage = Constants::Errors::NOT_AUTHENTICATED;
    result.httpStatus = 401;
    return result;
  }

  juce::String fullUrl = config.baseUrl + endpoint;

  Log::debug("uploadMultipartData: URL = " + fullUrl);
  Log::debug("uploadMultipartData: File size = " + juce::String(fileData.getSize()) + " bytes");

  // Write file data to a temporary file (required for JUCE's withFileToUpload)
  // Use abs to avoid negative numbers which can cause JUCE String assertion
  // failures
  juce::File tempFile =
      juce::File::getSpecialLocation(juce::File::tempDirectory)
          .getChildFile("sidechain_upload_" + juce::String(std::abs(juce::Random::getSystemRandom().nextInt64())) +
                        "_" + fileName);

  if (!tempFile.replaceWithData(fileData.getData(), fileData.getSize())) {
    result.errorMessage = "Failed to create temporary file for upload";
    Log::error("uploadMultipartData: " + result.errorMessage);
    return result;
  }

  Log::debug("uploadMultipartData: Created temp file: " + tempFile.getFullPathName());

  // Build URL with file upload using JUCE's proper multipart encoding
  juce::URL url(fullUrl);

  // Add extra form fields as parameters
  for (const auto &field : extraFields) {
    url = url.withParameter(field.first, field.second);
  }

  // Add file upload - JUCE handles multipart encoding automatically
  url = url.withFileToUpload(fieldName, tempFile, mimeType);

  // Build headers
  juce::String headers = "Authorization: Bearer " + authToken + "\r\n";

  // Create request options - use inPostData so parameters go in the multipart
  // body
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

  if (stream == nullptr) {
    result.errorMessage = "Failed to connect to server";

    // Report connection error
    HttpErrorHandler::getInstance().reportError(endpoint, "POST (multipart)", 0, result.errorMessage, "");

    updateConnectionStatus(ConnectionStatus::Disconnected);
    return result;
  }

  auto response = stream->readEntireStreamAsString();

  // Extract status code and parse response
  result.responseHeaders = responseHeaders;
  result.httpStatus = parseStatusCode(responseHeaders);
  if (result.httpStatus == 0)
    result.httpStatus = 200;

  try {
    result.data = nlohmann::json::parse(response.toStdString());
  } catch (const nlohmann::json::parse_error &e) {
    result.data = nlohmann::json();
    Log::warn("JSON parse error in multipart response: " + juce::String(e.what()));
  }
  result.success = result.isSuccess();

  Log::debug("Multipart upload to " + endpoint + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

  // Report HTTP errors
  if (result.httpStatus >= 400) {
    HttpErrorHandler::getInstance().reportError(endpoint, "POST (multipart)", result.httpStatus,
                                                result.getUserFriendlyError(), juce::String(result.data.dump()));
  }

  updateConnectionStatus(result.success ? ConnectionStatus::Connected : ConnectionStatus::Disconnected);

  return result;
}

/**
 * Upload file data to an external endpoint using absolute URL
 *
 * IMPORTANT: This method bypasses rate limiting because it's designed for
 * external endpoints (CDN, S3, etc.) which have their own rate limiting
 * mechanisms. These uploads should not count against the API's rate limits.
 *
 * @param absoluteUrl Full URL to upload to (not relative to API host)
 * @param fieldName Form field name for the file
 * @param fileData Binary data to upload
 * @param fileName Original filename to include in multipart form
 * @param mimeType Content type for the file
 * @param extraFields Additional form fields to include
 * @param customHeaders Additional HTTP headers to include
 * @return RequestResult with status and error details
 */
// Note: uploadMultipartDataAbsolute is NOT rate limited
// External endpoints (CDN, S3, etc.) have their own rate limiting and should
// not count against our API rate limits. This is intentional for external
// uploads.
NetworkClient::RequestResult NetworkClient::uploadMultipartDataAbsolute(
    const juce::String &absoluteUrl, const juce::String &fieldName, const juce::MemoryBlock &fileData,
    const juce::String &fileName, const juce::String &mimeType, const std::map<juce::String, juce::String> &extraFields,
    const juce::StringPairArray &customHeaders) {
  RequestResult result;

  Log::debug("uploadMultipartDataAbsolute: URL = " + absoluteUrl);
  Log::debug("uploadMultipartDataAbsolute: File size = " + juce::String(fileData.getSize()) + " bytes");

  // Write file data to a temporary file (required for JUCE's withFileToUpload)
  // Use abs to avoid negative numbers which can cause JUCE String assertion
  // failures
  juce::File tempFile =
      juce::File::getSpecialLocation(juce::File::tempDirectory)
          .getChildFile("sidechain_upload_" + juce::String(std::abs(juce::Random::getSystemRandom().nextInt64())) +
                        "_" + fileName);

  if (!tempFile.replaceWithData(fileData.getData(), fileData.getSize())) {
    result.errorMessage = "Failed to create temporary file for upload";
    Log::error("uploadMultipartDataAbsolute: " + result.errorMessage);
    return result;
  }

  Log::debug("uploadMultipartDataAbsolute: Created temp file: " + tempFile.getFullPathName());

  // Build URL with file upload using JUCE's proper multipart encoding
  juce::URL url(absoluteUrl);

  // Add extra form fields as parameters
  for (const auto &field : extraFields) {
    url = url.withParameter(field.first, field.second);
  }

  // Add file upload - JUCE handles multipart encoding automatically
  url = url.withFileToUpload(fieldName, tempFile, mimeType);

  // Build headers from custom headers
  juce::String headers;
  for (int i = 0; i < customHeaders.size(); i++) {
    auto key = customHeaders.getAllKeys()[i];
    auto value = customHeaders[key];
    headers += key + ": " + value + "\r\n";
  }

  // Create request options - use inPostData so parameters go in the multipart
  // body
  juce::StringPairArray responseHeaders;
  auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                     .withExtraHeaders(headers)
                     .withConnectionTimeoutMs(config.timeoutMs)
                     .withResponseHeaders(&responseHeaders);

  Log::debug("uploadMultipartDataAbsolute: Making request with JUCE multipart "
             "encoding...");

  // Make request
  activeRequestCount++;
  auto stream = url.createInputStream(options);
  activeRequestCount--;

  // Clean up temp file
  tempFile.deleteFile();

  if (stream == nullptr) {
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

  try {
    result.data = nlohmann::json::parse(response.toStdString());
  } catch (const nlohmann::json::parse_error &e) {
    result.data = nlohmann::json();
    Log::warn("JSON parse error in multipart absolute response: " + juce::String(e.what()));
  }
  result.success = result.isSuccess();

  Log::debug("Multipart upload to " + absoluteUrl + " (HTTP " + juce::String(result.httpStatus) + "): " + response);

  updateConnectionStatus(result.success ? ConnectionStatus::Connected : ConnectionStatus::Disconnected);

  return result;
}

// ==============================================================================
// Generic HTTP methods
// ==============================================================================
void NetworkClient::get(const juce::String &endpoint, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::post(const juce::String &endpoint, const nlohmann::json &data, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, endpoint, data, callback]() {
    auto result = makeRequestWithRetry(endpoint, "POST", data, true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::put(const juce::String &endpoint, const nlohmann::json &data, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, endpoint, data, callback]() {
    auto result = makeRequestWithRetry(endpoint, "PUT", data, true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::del(const juce::String &endpoint, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "DELETE", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

// ==============================================================================
// Absolute URL methods (for CDN, external APIs, etc.)
// ==============================================================================
void NetworkClient::getAbsolute(const juce::String &absoluteUrl, ResponseCallback callback,
                                const juce::StringPairArray &customHeaders) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, absoluteUrl, callback, customHeaders]() {
    RequestResult result = makeAbsoluteRequestWithRetry(absoluteUrl, "GET", nlohmann::json(), false, customHeaders);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::postAbsolute(const juce::String &absoluteUrl, const nlohmann::json &data, ResponseCallback callback,
                                 const juce::StringPairArray &customHeaders) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, absoluteUrl, data, callback, customHeaders]() {
    RequestResult result = makeAbsoluteRequestWithRetry(absoluteUrl, "POST", data, false, customHeaders);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::uploadMultipartAbsolute(const juce::String &absoluteUrl, const juce::String &fieldName,
                                            const juce::MemoryBlock &fileData, const juce::String &fileName,
                                            const juce::String &mimeType,
                                            const std::map<juce::String, juce::String> &extraFields,
                                            MultipartUploadCallback callback,
                                            const juce::StringPairArray &customHeaders) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, absoluteUrl, fieldName, fileData, fileName, mimeType, extraFields, callback, customHeaders]() {
    RequestResult result =
        uploadMultipartDataAbsolute(absoluteUrl, fieldName, fileData, fileName, mimeType, extraFields, customHeaders);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::getBinaryAbsolute(const juce::String &absoluteUrl, BinaryDataCallback callback,
                                      const juce::StringPairArray &customHeaders) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, absoluteUrl, callback, customHeaders]() {
    juce::MemoryBlock data;
    bool success = false;

    RequestResult result =
        makeAbsoluteRequestWithRetry(absoluteUrl, "GET", nlohmann::json(), false, customHeaders, &data);

    if (result.success && data.getSize() > 0) {
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

// ==============================================================================
// User profile operations (for UserStore)

void NetworkClient::getCurrentUser(ResponseCallback callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry("/api/v1/users/me", "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::updateUserProfile(const juce::String &username, const juce::String &displayName,
                                      const juce::String &bio, ResponseCallback callback) {
  if (callback == nullptr)
    return;

  nlohmann::json data;
  if (!username.isEmpty())
    data["username"] = username.toStdString();
  if (!displayName.isEmpty())
    data["display_name"] = displayName.toStdString();
  if (!bio.isEmpty())
    data["bio"] = bio.toStdString();

  Async::runVoid([this, data, callback]() {
    auto result = makeRequestWithRetry("/api/v1/users/me", "PUT", data, true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

// ==============================================================================
// User Profile Observable Methods

// Helper function to parse a single user from JSON response
static Sidechain::User parseUserFromJson(const nlohmann::json &json) {
  Sidechain::User user;

  if (!json.is_object()) {
    return user;
  }

  try {
    from_json(json, user);
  } catch (const std::exception &e) {
    Log::warn("NetworkClient: Failed to parse user: " + juce::String(e.what()));
  }

  return user;
}

rxcpp::observable<Sidechain::User> NetworkClient::getCurrentUserObservable() {
  auto source = rxcpp::sources::create<Sidechain::User>([this](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    getCurrentUser([observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        auto user = parseUserFromJson(result.getValue());
        if (user.isValid()) {
          observer.on_next(std::move(user));
          observer.on_completed();
        } else {
          observer.on_error(std::make_exception_ptr(std::runtime_error("Failed to parse current user")));
        }
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<Sidechain::User> NetworkClient::updateUserProfileObservable(const juce::String &username,
                                                                              const juce::String &displayName,
                                                                              const juce::String &bio) {
  auto source = rxcpp::sources::create<Sidechain::User>([this, username, displayName, bio](auto observer) {
    if (!isAuthenticated()) {
      observer.on_error(std::make_exception_ptr(std::runtime_error(Constants::Errors::NOT_AUTHENTICATED)));
      return;
    }

    updateUserProfile(username, displayName, bio, [observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        auto user = parseUserFromJson(result.getValue());
        observer.on_next(std::move(user));
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

// ==============================================================================
// Model parsing helpers (Phase 3 refactoring)

Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>
NetworkClient::parseFeedPostsResponse(const nlohmann::json &response) {
  // Response should be an array of FeedPost objects
  if (!response.is_array()) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error("Expected array response for feed posts");
  }

  try {
    return Sidechain::FeedPost::createFromJsonArray(response);
  } catch (const std::exception &e) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::FeedPost>>>::error("Failed to parse feed posts: " +
                                                                             juce::String(e.what()));
  }
}

Outcome<std::shared_ptr<Sidechain::User>> NetworkClient::parseUserResponse(const nlohmann::json &response) {
  // Response should be a User object
  if (!response.is_object()) {
    return Outcome<std::shared_ptr<Sidechain::User>>::error("Expected object response for user");
  }

  try {
    return Sidechain::User::createFromJson(response);
  } catch (const std::exception &e) {
    return Outcome<std::shared_ptr<Sidechain::User>>::error("Failed to parse user: " + juce::String(e.what()));
  }
}

Outcome<std::vector<std::shared_ptr<Sidechain::User>>>
NetworkClient::parseUsersResponse(const nlohmann::json &response) {
  // Response should be an array of User objects
  if (!response.is_array()) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::User>>>::error("Expected array response for users");
  }

  try {
    return Sidechain::User::createFromJsonArray(response);
  } catch (const std::exception &e) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::User>>>::error("Failed to parse users: " +
                                                                         juce::String(e.what()));
  }
}

// ==============================================================================
// Toggle convenience methods (for FeedStore optimistic updates)

// ==============================================================================
// Additional model parsing helpers (Phase 3 refactoring)

Outcome<std::vector<std::shared_ptr<Sidechain::Playlist>>>
NetworkClient::parsePlaylistsResponse(const nlohmann::json &response) {
  if (!response.is_array()) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::Playlist>>>::error("Expected array response for playlists");
  }

  try {
    return Sidechain::Playlist::createFromJsonArray(response);
  } catch (const std::exception &e) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::Playlist>>>::error("Failed to parse playlists: " +
                                                                             juce::String(e.what()));
  }
}

Outcome<std::vector<std::shared_ptr<Sidechain::Story>>>
NetworkClient::parseStoriesResponse(const nlohmann::json &response) {
  if (!response.is_array()) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::Story>>>::error("Expected array response for stories");
  }

  try {
    return Sidechain::Story::createFromJsonArray(response);
  } catch (const std::exception &e) {
    return Outcome<std::vector<std::shared_ptr<Sidechain::Story>>>::error("Failed to parse stories: " +
                                                                          juce::String(e.what()));
  }
}

// ==============================================================================
// Feed API methods (Phase 3 refactoring)

void NetworkClient::getLatestFeed(int limit, int offset, FeedCallback callback) {
  if (!callback)
    return;

  Async::runVoid([this, limit, offset, callback]() {
    juce::String path = "/api/v1/feed/latest?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto result = makeRequestWithRetry(path, "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::getPopularFeed(int limit, int offset, FeedCallback callback) {
  if (!callback)
    return;

  Async::runVoid([this, limit, offset, callback]() {
    juce::String path = "/api/v1/feed/popular?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto result = makeRequestWithRetry(path, "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::getDiscoveryFeed(int limit, int offset, FeedCallback callback) {
  if (!callback)
    return;

  Async::runVoid([this, limit, offset, callback]() {
    juce::String path = "/api/v1/feed/discovery?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto result = makeRequestWithRetry(path, "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::getAggregatedTimeline(int limit, int offset, AggregatedFeedCallback callback) {
  if (!callback)
    return;

  Async::runVoid([this, limit, offset, callback]() {
    juce::String path =
        "/api/v1/feed/timeline/aggregated?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto result = makeRequestWithRetry(path, "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::getTrendingFeedGrouped(int limit, int offset, AggregatedFeedCallback callback) {
  if (!callback)
    return;

  Async::runVoid([this, limit, offset, callback]() {
    juce::String path =
        "/api/v1/feed/trending/grouped?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto result = makeRequestWithRetry(path, "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

void NetworkClient::getNotificationsAggregated(int limit, int offset, AggregatedFeedCallback callback) {
  if (!callback)
    return;

  Async::runVoid([this, limit, offset, callback]() {
    juce::String path =
        "/api/v1/notifications/aggregated?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);
    auto result = makeRequestWithRetry(path, "GET", nlohmann::json(), true);
    auto outcome = requestResultToOutcome(result);

    juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
  });
}

// ==============================================================================
// Telemetry / Distributed Tracing
// ==============================================================================

void NetworkClient::sendTelemetrySpans(const nlohmann::json &spans, ResponseCallback callback) {
  // Send spans asynchronously on background thread to avoid blocking UI
  Async::runVoid([this, spans, callback]() {
    // Build telemetry payload
    nlohmann::json payload = {{"spans", spans}, {"clientType", "plugin"}, {"clientVersion", "1.0.0"}};

    // Send to /api/v1/telemetry/spans endpoint
    auto result = makeRequestWithRetry("/api/v1/telemetry/spans", "POST", payload, true);
    auto outcome = requestResultToOutcome(result);

    // Call callback on message thread if provided
    if (callback) {
      juce::MessageManager::callAsync([callback, outcome]() { callback(outcome); });
    }
  });
}
