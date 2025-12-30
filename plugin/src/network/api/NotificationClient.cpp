// ==============================================================================
// NotificationClient.cpp - Notification operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Log.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/Notification.h"
#include "../NetworkClient.h"
#include "Common.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

using namespace Sidechain::Network::Api;

// ==============================================================================
void NetworkClient::getNotifications(int limit, int offset, NotificationCallback callback) {
  if (callback == nullptr)
    return;

  juce::String endpoint =
      buildApiPath("/notifications") + "?limit=" + juce::String(limit) + "&offset=" + juce::String(offset);

  Async::runVoid([this, endpoint, callback]() {
    auto result = makeRequestWithRetry(endpoint, "GET", nlohmann::json(), true);

    int unseen = 0;
    int unread = 0;
    std::vector<Sidechain::Notification> notifications;

    if (result.success && result.data.is_object()) {
      unseen = result.data.value("unseen", 0);
      unread = result.data.value("unread", 0);

      if (result.data.contains("groups") && result.data["groups"].is_array()) {
        const auto &groups = result.data["groups"];
        for (const auto &group : groups) {
          try {
            Sidechain::Notification notif;
            from_json(group, notif);
            if (notif.isValid()) {
              notifications.push_back(std::move(notif));
            }
          } catch (const std::exception &e) {
            Log::warn("NotificationClient: Failed to parse notification: " + juce::String(e.what()));
          }
        }
      }
    }

    juce::MessageManager::callAsync(
        [callback, result, parsedNotifications = std::move(notifications), unseen, unread]() mutable {
          if (result.isSuccess()) {
            NotificationResult notifResult;
            notifResult.notifications = std::move(parsedNotifications);
            notifResult.unseen = unseen;
            notifResult.unread = unread;
            callback(Outcome<NotificationResult>::ok(std::move(notifResult)));
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
    auto result = makeRequestWithRetry(buildApiPath("/notifications/counts"), "GET", nlohmann::json(), true);

    int unseen = 0;
    int unread = 0;

    if (result.success && result.data.is_object()) {
      unseen = result.data.value("unseen", 0);
      unread = result.data.value("unread", 0);
    }

    juce::MessageManager::callAsync([callback, unseen, unread]() { callback(unseen, unread); });
  });
}

void NetworkClient::markNotificationsRead(ResponseCallback callback) {
  Async::runVoid([this, callback]() {
    auto result = makeRequestWithRetry(buildApiPath("/notifications/read"), "POST", nlohmann::json(), true);

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
    auto result = makeRequestWithRetry(buildApiPath("/notifications/seen"), "POST", nlohmann::json(), true);

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
    auto result = makeRequestWithRetry(buildApiPath("/users/me/follow-requests/count"), "GET", nlohmann::json(), true);

    int count = 0;
    if (result.success && result.data.is_object()) {
      count = result.data.value("count", 0);
    }

    juce::MessageManager::callAsync([callback, count]() { callback(count); });
  });
}

// ==============================================================================
// Reactive Observable Methods (Phase 5)
// ==============================================================================

rxcpp::observable<std::pair<int, int>> NetworkClient::getNotificationCountsObservable() {
  auto source = rxcpp::sources::create<std::pair<int, int>>([this](auto observer) {
    Async::runVoid([this, observer]() {
      auto result = makeRequestWithRetry(buildApiPath("/notifications/counts"), "GET", nlohmann::json(), true);

      int unseen = 0;
      int unread = 0;

      if (result.success && result.data.is_object()) {
        unseen = result.data.value("unseen", 0);
        unread = result.data.value("unread", 0);
      }

      juce::MessageManager::callAsync([observer, result, unseen, unread]() {
        if (result.isSuccess()) {
          observer.on_next(std::make_pair(unseen, unread));
          observer.on_completed();
        } else {
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getUserFriendlyError().toStdString())));
        }
      });
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::getFollowRequestCountObservable() {
  auto source = rxcpp::sources::create<int>([this](auto observer) {
    Async::runVoid([this, observer]() {
      auto result =
          makeRequestWithRetry(buildApiPath("/users/me/follow-requests/count"), "GET", nlohmann::json(), true);

      int count = 0;
      if (result.success && result.data.is_object()) {
        count = result.data.value("count", 0);
      }

      juce::MessageManager::callAsync([observer, result, count]() {
        if (result.isSuccess()) {
          observer.on_next(count);
          observer.on_completed();
        } else {
          observer.on_error(std::make_exception_ptr(std::runtime_error(result.getUserFriendlyError().toStdString())));
        }
      });
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<NetworkClient::NotificationResult> NetworkClient::getNotificationsObservable(int limit, int offset) {
  auto source = rxcpp::sources::create<NotificationResult>([this, limit, offset](auto observer) {
    getNotifications(limit, offset, [observer](Outcome<NotificationResult> result) {
      if (result.isOk()) {
        observer.on_next(result.getValue());
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::markNotificationsReadObservable() {
  auto source = rxcpp::sources::create<int>([this](auto observer) {
    markNotificationsRead([observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        observer.on_next(0);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}

rxcpp::observable<int> NetworkClient::markNotificationsSeenObservable() {
  auto source = rxcpp::sources::create<int>([this](auto observer) {
    markNotificationsSeen([observer](Outcome<nlohmann::json> result) {
      if (result.isOk()) {
        observer.on_next(0);
        observer.on_completed();
      } else {
        observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
      }
    });
  });

  return Sidechain::Rx::retryWithBackoff(source.as_dynamic()).observe_on(Sidechain::Rx::observe_on_juce_thread());
}
