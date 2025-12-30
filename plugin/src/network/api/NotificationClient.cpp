// ==============================================================================
// NotificationClient.cpp - Notification operations
// Part of NetworkClient implementation split
// ==============================================================================

#include "../../util/Async.h"
#include "../../util/Json.h"
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
    auto result = makeRequestWithRetry(endpoint, "GET", juce::var(), true);

    int unseen = 0;
    int unread = 0;
    std::vector<Sidechain::Notification> notifications;

    if (result.success && result.data.isObject()) {
      unseen = Json::getInt(result.data, "unseen");
      unread = Json::getInt(result.data, "unread");

      auto groups = Json::getArray(result.data, "groups");
      if (groups.isArray()) {
        for (int i = 0; i < groups.size(); ++i) {
          try {
            auto jsonStr = juce::JSON::toString(groups[i]);
            auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
            Sidechain::Notification notif;
            from_json(jsonObj, notif);
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
    auto result = makeRequestWithRetry(buildApiPath("/notifications/counts"), "GET", juce::var(), true);

    int unseen = 0;
    int unread = 0;

    if (result.success && result.data.isObject()) {
      unseen = Json::getInt(result.data, "unseen");
      unread = Json::getInt(result.data, "unread");
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
      count = Json::getInt(result.data, "count");
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
      auto result = makeRequestWithRetry(buildApiPath("/notifications/counts"), "GET", juce::var(), true);

      int unseen = 0;
      int unread = 0;

      if (result.success && result.data.isObject()) {
        unseen = Json::getInt(result.data, "unseen");
        unread = Json::getInt(result.data, "unread");
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
      auto result = makeRequestWithRetry(buildApiPath("/users/me/follow-requests/count"), "GET", juce::var(), true);

      int count = 0;
      if (result.success && result.data.isObject()) {
        count = Json::getInt(result.data, "count");
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
    markNotificationsRead([observer](Outcome<juce::var> result) {
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
    markNotificationsSeen([observer](Outcome<juce::var> result) {
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
