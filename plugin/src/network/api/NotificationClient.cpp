// ==============================================================================
// NotificationClient.cpp - Notification operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../NetworkClient.h"
#include "Common.h"

using namespace Sidechain::Network::Api;

// ==============================================================================
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
