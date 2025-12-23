#pragma once

#include "../../util/Result.h"
#include "../NetworkClient.h"

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
 * Convert a NetworkClient::RequestResult to an Outcome<juce::var>
 * @param result The request result from NetworkClient
 * @return Outcome with the response data or error message
 */
inline Outcome<juce::var> requestResultToOutcome(const NetworkClient::RequestResult &result) {
  if (result.success && result.isSuccess()) {
    return Outcome<juce::var>::ok(result.data);
  } else {
    juce::String errorMsg = result.getUserFriendlyError();
    if (errorMsg.isEmpty())
      errorMsg = "Request failed (HTTP " + juce::String(result.httpStatus) + ")";
    return Outcome<juce::var>::error(errorMsg);
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
inline Outcome<juce::var> parseJsonResponse(const juce::var &response,
                                            const juce::String &invalidResponseMsg = "Invalid response") {
  if (response.isObject() && response.hasProperty("error")) {
    juce::String errorMsg = response["error"].toString();
    juce::String fullMsg = errorMsg;
    if (response.hasProperty("message"))
      fullMsg = response["message"].toString();
    return Outcome<juce::var>::error(fullMsg);
  } else if (response.isObject() || response.isArray()) {
    return Outcome<juce::var>::ok(response);
  } else {
    return Outcome<juce::var>::error(invalidResponseMsg);
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
inline Outcome<juce::var> extractProperty(const Outcome<juce::var> &outcome, const juce::String &propertyName) {
  if (outcome.isOk()) {
    auto data = outcome.getValue();
    if (data.isObject() && data.hasProperty(propertyName)) {
      return Outcome<juce::var>::ok(data.getProperty(propertyName, juce::var()));
    }
  }
  return outcome;
}

/**
 * Create a juce::var object with the given properties.
 * Simplifies the common pattern of creating DynamicObject and setting properties.
 *
 * Usage:
 *   auto data = createJsonObject({
 *     {"activity_id", activityId},
 *     {"emoji", emoji}
 *   });
 *
 * @param properties Vector of key-value pairs to set
 * @return juce::var containing a DynamicObject with the properties
 */
inline juce::var createJsonObject(std::initializer_list<std::pair<juce::String, juce::var>> properties) {
  juce::var data(new juce::DynamicObject());
  auto *obj = data.getDynamicObject();
  if (obj != nullptr) {
    for (const auto &[key, value] : properties) {
      obj->setProperty(key, value);
    }
  }
  return data;
}

} // namespace Sidechain::Network::Api
