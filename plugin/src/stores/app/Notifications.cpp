#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadNotifications() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load notifications - network client not set");
    return;
  }

  updateState([](AppState &state) { state.notifications.isLoading = true; });

  networkClient->getNotifications(20, 0, [this](Outcome<NetworkClient::NotificationResult> result) {
    if (result.isOk()) {
      const auto notifResult = result.getValue();
      juce::Array<juce::var> notificationsList;

      if (notifResult.notifications.isArray()) {
        for (int i = 0; i < notifResult.notifications.size(); ++i) {
          notificationsList.add(notifResult.notifications[i]);
        }
      }

      updateState([notificationsList, notifResult](AppState &state) {
        state.notifications.notifications = notificationsList;
        state.notifications.isLoading = false;
        state.notifications.unreadCount = notifResult.unread;
        state.notifications.unseenCount = notifResult.unseen;
        state.notifications.notificationError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(notificationsList.size()) +
                                      " notifications (unread: " + juce::String(notifResult.unread) + ")");
      });

      // Notify notification subscribers
      auto currentState = getState();
      for (const auto &[id, callback] : notificationSubscribers) {
        try {
          callback(currentState.notifications);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Notification subscriber threw exception", e.what());
        }
      }
    } else {
      updateState([result](AppState &state) {
        state.notifications.isLoading = false;
        state.notifications.notificationError = result.getError();
        Util::logError("AppStore", "Failed to load notifications: " + result.getError());
      });

      // Notify notification subscribers
      auto currentState = getState();
      for (const auto &[id, callback] : notificationSubscribers) {
        try {
          callback(currentState.notifications);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Notification subscriber threw exception", e.what());
        }
      }
    }

    notifyObservers();
  });
}

void AppStore::loadMoreNotifications() {
  if (!networkClient) {
    return;
  }

  const auto &currentState = getState();
  if (currentState.notifications.notifications.isEmpty()) {
    return;
  }

  networkClient->getNotifications(
      20, currentState.notifications.notifications.size(), [this](Outcome<NetworkClient::NotificationResult> result) {
        if (result.isOk()) {
          const auto notifResult = result.getValue();
          juce::Array<juce::var> newNotifications;

          if (notifResult.notifications.isArray()) {
            for (int i = 0; i < notifResult.notifications.size(); ++i) {
              newNotifications.add(notifResult.notifications[i]);
            }
          }

          updateState([newNotifications, notifResult](AppState &state) {
            for (const auto &notification : newNotifications) {
              state.notifications.notifications.add(notification);
            }
            state.notifications.unreadCount = notifResult.unread;
            state.notifications.unseenCount = notifResult.unseen;
          });

          // Notify notification subscribers
          auto currentState = getState();
          for (const auto &[id, callback] : notificationSubscribers) {
            try {
              callback(currentState.notifications);
            } catch (const std::exception &e) {
              Util::logError("AppStore", "Notification subscriber threw exception", e.what());
            }
          }
        }

        notifyObservers();
      });
}

void AppStore::markNotificationsAsRead() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as read - network client not set");
    return;
  }

  networkClient->markNotificationsRead([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      updateState([](AppState &state) {
        // Mark all notifications as read
        for (auto &notification : state.notifications.notifications) {
          notification.getDynamicObject()->setProperty("is_read", true);
        }
        state.notifications.unreadCount = 0;
        Util::logInfo("AppStore", "All notifications marked as read");
      });

      // Notify notification subscribers
      auto currentState = getState();
      for (const auto &[id, callback] : notificationSubscribers) {
        try {
          callback(currentState.notifications);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Notification subscriber threw exception", e.what());
        }
      }

      notifyObservers();
    } else {
      updateState([result](AppState &state) {
        state.notifications.notificationError = result.getError();
        Util::logError("AppStore", "Failed to mark notifications as read: " + result.getError());
      });

      // Notify notification subscribers
      auto currentState = getState();
      for (const auto &[id, callback] : notificationSubscribers) {
        try {
          callback(currentState.notifications);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Notification subscriber threw exception", e.what());
        }
      }

      notifyObservers();
    }
  });
}

void AppStore::markNotificationsAsSeen() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as seen - network client not set");
    return;
  }

  networkClient->markNotificationsSeen([this](Outcome<juce::var> result) {
    if (result.isOk()) {
      updateState([](AppState &state) {
        state.notifications.unseenCount = 0;
        Util::logInfo("AppStore", "All notifications marked as seen");
      });

      // Notify notification subscribers
      auto currentState = getState();
      for (const auto &[id, callback] : notificationSubscribers) {
        try {
          callback(currentState.notifications);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Notification subscriber threw exception", e.what());
        }
      }

      notifyObservers();
    } else {
      updateState([result](AppState &state) {
        state.notifications.notificationError = result.getError();
        Util::logError("AppStore", "Failed to mark notifications as seen: " + result.getError());
      });

      // Notify notification subscribers
      auto currentState = getState();
      for (const auto &[id, callback] : notificationSubscribers) {
        try {
          callback(currentState.notifications);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Notification subscriber threw exception", e.what());
        }
      }

      notifyObservers();
    }
  });
}

} // namespace Stores
} // namespace Sidechain
