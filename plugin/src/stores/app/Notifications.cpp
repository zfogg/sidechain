#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include "../../models/Notification.h"
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::loadNotifications() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load notifications - network client not set");
    return;
  }

  auto notificationState = stateManager.notifications;
  NotificationState loadingState = notificationState->getState();
  loadingState.isLoading = true;
  notificationState->setState(loadingState);

  networkClient->getNotificationsObservable(20, 0).subscribe(
      [notificationState](const NetworkClient::NotificationResult &notifResult) {
        // Convert already-parsed Notification values to shared_ptr for state
        std::vector<std::shared_ptr<Sidechain::Notification>> notificationsList;
        notificationsList.reserve(notifResult.notifications.size());
        for (const auto &notif : notifResult.notifications) {
          notificationsList.push_back(std::make_shared<Sidechain::Notification>(notif));
        }

        NotificationState successState = notificationState->getState();
        successState.notifications = notificationsList;
        successState.isLoading = false;
        successState.unreadCount = notifResult.unread;
        successState.unseenCount = notifResult.unseen;
        successState.notificationError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(notificationsList.size()) +
                                      " notifications (unread: " + juce::String(notifResult.unread) + ")");
        notificationState->setState(successState);
      },
      [notificationState](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        NotificationState errorState = notificationState->getState();
        errorState.isLoading = false;
        errorState.notificationError = errorMsg;
        Util::logError("AppStore", "Failed to load notifications: " + errorMsg);
        notificationState->setState(errorState);
      });
}

void AppStore::loadMoreNotifications() {
  if (!networkClient) {
    return;
  }

  auto notificationState = stateManager.notifications;
  const auto &currentState = notificationState->getState();
  if (currentState.notifications.empty()) {
    return;
  }

  int offset = static_cast<int>(currentState.notifications.size());
  networkClient->getNotificationsObservable(20, offset)
      .subscribe(
          [notificationState](const NetworkClient::NotificationResult &notifResult) {
            // Convert already-parsed Notification values to shared_ptr for state
            std::vector<std::shared_ptr<Sidechain::Notification>> newNotifications;
            newNotifications.reserve(notifResult.notifications.size());
            for (const auto &notif : notifResult.notifications) {
              newNotifications.push_back(std::make_shared<Sidechain::Notification>(notif));
            }

            NotificationState moreState = notificationState->getState();
            for (const auto &notification : newNotifications) {
              moreState.notifications.push_back(notification);
            }
            moreState.unreadCount = notifResult.unread;
            moreState.unseenCount = notifResult.unseen;
            notificationState->setState(moreState);
          },
          [](std::exception_ptr) {
            // Silent failure for pagination - don't update error state
          });
}

void AppStore::markNotificationsAsRead() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as read - network client not set");
    return;
  }

  auto notificationState = stateManager.notifications;

  networkClient->markNotificationsReadObservable().subscribe(
      [notificationState](const juce::var &) {
        NotificationState readState = notificationState->getState();
        // Mark all notifications as read
        for (auto &notification : readState.notifications) {
          if (notification) {
            notification->isRead = true;
          }
        }
        readState.unreadCount = 0;
        Util::logInfo("AppStore", "All notifications marked as read");
        notificationState->setState(readState);
      },
      [notificationState](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        NotificationState errorState = notificationState->getState();
        errorState.notificationError = errorMsg;
        Util::logError("AppStore", "Failed to mark notifications as read: " + errorMsg);
        notificationState->setState(errorState);
      });
}

void AppStore::markNotificationsAsSeen() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as seen - network client not set");
    return;
  }

  auto notificationState = stateManager.notifications;

  networkClient->markNotificationsSeenObservable().subscribe(
      [notificationState](const juce::var &) {
        NotificationState seenState = notificationState->getState();
        seenState.unseenCount = 0;
        Util::logInfo("AppStore", "All notifications marked as seen");
        notificationState->setState(seenState);
      },
      [notificationState](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        NotificationState errorState = notificationState->getState();
        errorState.notificationError = errorMsg;
        Util::logError("AppStore", "Failed to mark notifications as seen: " + errorMsg);
        notificationState->setState(errorState);
      });
}

// ==============================================================================
// Reactive Notification Observables (Phase 6)
// ==============================================================================
//
// These methods return rxcpp::observable with proper model types (Notification values,
// not shared_ptr). They use the same network calls as the Redux actions above
// but wrap them in reactive streams for compose-able operations.

rxcpp::observable<std::vector<Notification>> AppStore::loadNotificationsObservable(int limit, int offset) {
  using ResultType = std::vector<Notification>;

  if (!networkClient) {
    return rxcpp::sources::error<ResultType>(
        std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  Util::logDebug("AppStore", "Loading notifications via observable");

  // The NetworkClient already parses notifications, so we just return them directly
  return networkClient->getNotificationsObservable(limit, offset)
      .map([](const NetworkClient::NotificationResult &notifResult) {
        Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(notifResult.notifications.size())) +
                                      " notifications (unread: " + juce::String(notifResult.unread) + ")");
        return notifResult.notifications;
      });
}

rxcpp::observable<int> AppStore::markNotificationsAsReadObservable() {
  if (!networkClient) {
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  Util::logDebug("AppStore", "Marking notifications as read via observable");

  return networkClient->markNotificationsReadObservable().map([this](const juce::var &) {
    // Update state
    auto notificationState = stateManager.notifications;
    if (notificationState) {
      NotificationState readState = notificationState->getState();
      for (auto &notification : readState.notifications) {
        if (notification) {
          notification->isRead = true;
        }
      }
      readState.unreadCount = 0;
      notificationState->setState(readState);
    }

    Util::logInfo("AppStore", "All notifications marked as read");
    return 0;
  });
}

rxcpp::observable<int> AppStore::markNotificationsAsSeenObservable() {
  if (!networkClient) {
    return rxcpp::sources::error<int>(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
  }

  Util::logDebug("AppStore", "Marking notifications as seen via observable");

  return networkClient->markNotificationsSeenObservable().map([this](const juce::var &) {
    // Update state
    auto notificationState = stateManager.notifications;
    if (notificationState) {
      NotificationState seenState = notificationState->getState();
      seenState.unseenCount = 0;
      notificationState->setState(seenState);
    }

    Util::logInfo("AppStore", "All notifications marked as seen");
    return 0;
  });
}

} // namespace Stores
} // namespace Sidechain
