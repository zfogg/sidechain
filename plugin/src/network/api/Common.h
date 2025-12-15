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

} // namespace Sidechain::Network::Api
