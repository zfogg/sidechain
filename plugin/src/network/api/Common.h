#pragma once

#include "../../util/Result.h"
#include "../NetworkClient.h"
#include <nlohmann/json.hpp>

namespace Sidechain::Network::Api {

/**
 * Build a versioned API path from a relative path
 * @param path Relative path (e.g., "/posts" or "/users/123")
 * @return Full API path (e.g., "/api/v1/posts")
 */
inline juce::String buildApiPath(const char *path) {
  return juce::String("/api/v1") + path;
}

/**
 * Convert a NetworkClient::RequestResult to an Outcome<nlohmann::json>
 * @param result The request result from NetworkClient
 * @return Outcome with the response data or error message
 */
inline Outcome<nlohmann::json> requestResultToOutcome(const NetworkClient::RequestResult &result) {
  // Check for error in response body
  bool hasErrorInBody = false;
  if (result.data.is_object() && result.data.contains("error")) {
    auto errorField = result.data["error"];
    if (errorField.is_string()) {
      hasErrorInBody = !errorField.get<std::string>().empty();
    }
  }

  if (result.success && result.isSuccess() && !hasErrorInBody) {
    return Outcome<nlohmann::json>::ok(result.data);
  } else {
    juce::String errorMsg = result.getUserFriendlyError();
    if (errorMsg.isEmpty())
      errorMsg = "Request failed (HTTP " + juce::String(result.httpStatus) + ")";
    return Outcome<nlohmann::json>::error(errorMsg);
  }
}

/**
 * Parse a JSON response and check for error field.
 * This handles the common pattern of checking for "error" and "message" fields
 * in API responses.
 *
 * @param response The JSON response from the API
 * @param invalidResponseMsg Message to use if response is not valid JSON object/array
 * @return Outcome with the response data or error message
 */
inline Outcome<nlohmann::json> parseJsonResponse(const nlohmann::json &response,
                                                 const juce::String &invalidResponseMsg = "Invalid response") {
  if (response.is_object() && response.contains("error")) {
    std::string errorMsg = response.value("error", "");
    std::string fullMsg = errorMsg;
    if (response.contains("message"))
      fullMsg = response.value("message", errorMsg);
    return Outcome<nlohmann::json>::error(juce::String(fullMsg));
  } else if (response.is_object() || response.is_array()) {
    return Outcome<nlohmann::json>::ok(response);
  } else {
    return Outcome<nlohmann::json>::error(invalidResponseMsg);
  }
}

/**
 * Extract a named property from a successful Outcome.
 * If the outcome contains an object with the specified property,
 * returns a new Outcome with just that property value.
 *
 * @param outcome The outcome to extract from
 * @param propertyName The property name to extract
 * @return Modified outcome with the extracted property, or original if not found
 */
inline Outcome<nlohmann::json> extractProperty(const Outcome<nlohmann::json> &outcome,
                                               const juce::String &propertyName) {
  if (outcome.isOk()) {
    const auto &data = outcome.getValue();
    std::string key = propertyName.toStdString();
    if (data.is_object() && data.contains(key)) {
      return Outcome<nlohmann::json>::ok(data[key]);
    }
  }
  return outcome;
}

} // namespace Sidechain::Network::Api
