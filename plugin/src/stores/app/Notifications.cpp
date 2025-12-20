#include "../AppStore.h"
#include "../../util/logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

void AppStore::loadNotifications() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load notifications - network client not set");
    return;
  }

  auto notificationSlice = sliceManager.getNotificationSlice();
  notificationSlice->dispatch([](NotificationState &state) { state.isLoading = true; });

  networkClient->getNotifications(20, 0, [notificationSlice](Outcome<NetworkClient::NotificationResult> result) {
    if (result.isOk()) {
      const auto notifResult = result.getValue();
      std::vector<std::shared_ptr<Sidechain::Notification>> notificationsList;

      if (notifResult.notifications.isArray()) {
        for (int i = 0; i < notifResult.notifications.size(); ++i) {
          try {
            auto jsonStr = juce::JSON::toString(notifResult.notifications[i]);
            auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
            auto parseResult = Sidechain::Notification::createFromJson(jsonObj);
            if (parseResult.isOk()) {
              notificationsList.push_back(parseResult.getValue());
            }
          } catch (...) {
            // Skip invalid items
          }
        }
      }

      notificationSlice->dispatch([notificationsList, notifResult](NotificationState &state) {
        state.notifications = notificationsList;
        state.isLoading = false;
        state.unreadCount = notifResult.unread;
        state.unseenCount = notifResult.unseen;
        state.notificationError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(notificationsList.size()) +
                                      " notifications (unread: " + juce::String(notifResult.unread) + ")");
      });
    } else {
      notificationSlice->dispatch([result](NotificationState &state) {
        state.isLoading = false;
        state.notificationError = result.getError();
        Util::logError("AppStore", "Failed to load notifications: " + result.getError());
      });
    }
  });
}

void AppStore::loadMoreNotifications() {
  if (!networkClient) {
    return;
  }

  auto notificationSlice = sliceManager.getNotificationSlice();
  const auto &currentState = notificationSlice->getState();
  if (currentState.notifications.empty()) {
    return;
  }

  networkClient->getNotifications(
      20, currentState.notifications.size(), [notificationSlice](Outcome<NetworkClient::NotificationResult> result) {
        if (result.isOk()) {
          const auto notifResult = result.getValue();
          std::vector<std::shared_ptr<Sidechain::Notification>> newNotifications;

          if (notifResult.notifications.isArray()) {
            for (int i = 0; i < notifResult.notifications.size(); ++i) {
              try {
                auto jsonStr = juce::JSON::toString(notifResult.notifications[i]);
                auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                auto parseResult = Sidechain::Notification::createFromJson(jsonObj);
                if (parseResult.isOk()) {
                  newNotifications.push_back(parseResult.getValue());
                }
              } catch (...) {
                // Skip invalid items
              }
            }
          }

          notificationSlice->dispatch([newNotifications, notifResult](NotificationState &state) {
            for (const auto &notification : newNotifications) {
              state.notifications.push_back(notification);
            }
            state.unreadCount = notifResult.unread;
            state.unseenCount = notifResult.unseen;
          });
        }
      });
}

void AppStore::markNotificationsAsRead() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as read - network client not set");
    return;
  }

  auto notificationSlice = sliceManager.getNotificationSlice();

  networkClient->markNotificationsRead([notificationSlice](Outcome<juce::var> result) {
    if (result.isOk()) {
      notificationSlice->dispatch([](NotificationState &state) {
        // Mark all notifications as read
        for (auto &notification : state.notifications) {
          if (notification) {
            notification->isRead = true;
          }
        }
        state.unreadCount = 0;
        Util::logInfo("AppStore", "All notifications marked as read");
      });
    } else {
      notificationSlice->dispatch([result](NotificationState &state) {
        state.notificationError = result.getError();
        Util::logError("AppStore", "Failed to mark notifications as read: " + result.getError());
      });
    }
  });
}

void AppStore::markNotificationsAsSeen() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as seen - network client not set");
    return;
  }

  auto notificationSlice = sliceManager.getNotificationSlice();

  networkClient->markNotificationsSeen([notificationSlice](Outcome<juce::var> result) {
    if (result.isOk()) {
      notificationSlice->dispatch([](NotificationState &state) {
        state.unseenCount = 0;
        Util::logInfo("AppStore", "All notifications marked as seen");
      });
    } else {
      notificationSlice->dispatch([result](NotificationState &state) {
        state.notificationError = result.getError();
        Util::logError("AppStore", "Failed to mark notifications as seen: " + result.getError());
      });
    }
  });
}

} // namespace Stores
} // namespace Sidechain
