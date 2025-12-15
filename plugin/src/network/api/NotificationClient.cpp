//==============================================================================
// NotificationClient.cpp - Notification operations
// Part of NetworkClient implementation split
//==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"

//==============================================================================
// Helper to build API endpoint paths consistently
static juce::String buildApiPath(const char *path) {
  return juce::String("/api/v1") + path;
}

// Helper to convert RequestResult to Outcome<juce::var>
static Outcome<juce::var> requestResultToOutcome(const NetworkClient::RequestResult &result) {
  if (result.success && result.isSuccess()) {
    return Outcome<juce::var>::ok(result.data);
  } else {
    juce::String errorMsg = result.getUserFriendlyError();
    if (errorMsg.isEmpty())
      errorMsg = "Request failed (HTTP " + juce::String(result.httpStatus) + ")";
    return Outcome<juce::var>::error(errorMsg);
  }
}

//==============================================================================
void NetworkClient::getNotifications(int limit, int offset, NotificationCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint =
      buildApiPath("/notifications") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    int unseen = 0;
    int unread = 0;
    juce::var groups;

    if (result.success && result.data.isObject()) {
      unseen = static_cast<int>(result.data.getProperty("unseen", 0));
      unread = static_cast<int>(result.data.getProperty("unread", 0));
      groups = result.data.getProperty("groups", juce::var());
    }

    juce::MessageManager::callAsync([callback, result, groups, unseen, unread]() {
      if (result.isSuccess()) {
        NotificationResult notifResult;
        notifResult.notifications = groups;
        notifResult.unseen = unseen;
        notifResult.unread = unread;
        callback(Outcome<NotificationResult>::ok(notifResult));
      } else {
        callback(Outcome<NotificationResult>::error(result.getUserFriendlyError()));
      }
    });
  });
}

void NetworkClient::getNotificationCounts(std::function<void(int unseen, int unread)> callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/notifications/counts"), "GET", juce::var(), true);

    int unseen = 0;
    int unread = 0;

    if (result.success && result.data.isObject()) {
      unseen = static_cast<int>(result.data.getProperty("unseen", 0));
      unread = static_cast<int>(result.data.getProperty("unread", 0));
    }

    juce::MessageManager::callAsync([callback, unseen, unread]() { callback(unseen, unread); });
  });
}

void NetworkClient::markNotificationsRead(ResponseCallback callback) {
  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/notifications/read"), "POST", juce::var(), true);

    if (callback != nullptr) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::markNotificationsSeen(ResponseCallback callback) {
  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/notifications/seen"), "POST", juce::var(), true);

    if (callback != nullptr) {
      juce::MessageManager::callAsync([callback, result]() {
        auto outcome = requestResultToOutcome(result);
        callback(outcome);
      });
    }
  });
}

void NetworkClient::getFollowRequestCount(std::function<void(int count)> callback) {
  if (callback == nullptr)
    return;

  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/users/me/follow-requests/count"), "GET", juce::var(), true);

    int count = 0;
    if (result.success && result.data.isObject()) {
      count = static_cast<int>(result.data.getProperty("count", 0));
    }

    juce::MessageManager::callAsync([callback, count]() { callback(count); });
  });
}
