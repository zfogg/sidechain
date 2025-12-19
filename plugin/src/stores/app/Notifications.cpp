#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadNotifications() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load notifications - network client not set");
    return;
  }

  auto notificationSlice = sliceManager.getNotificationSlice();
  notificationSlice->dispatch([](NotificationState &state) { state.isLoading = true; });

  networkClient->getNotifications(20, 0, [this, notificationSlice](Outcome<NetworkClient::NotificationResult> result) {
    if (result.isOk()) {
      const auto notifResult = result.getValue();
      juce::Array<juce::var> notificationsList;

      if (notifResult.notifications.isArray()) {
        for (int i = 0; i < notifResult.notifications.size(); ++i) {
          notificationsList.add(notifResult.notifications[i]);
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
  if (currentState.notifications.isEmpty()) {
    return;
  }

  networkClient->getNotifications(20, currentState.notifications.size(),
                                  [this, notificationSlice](Outcome<NetworkClient::NotificationResult> result) {
                                    if (result.isOk()) {
                                      const auto notifResult = result.getValue();
                                      juce::Array<juce::var> newNotifications;

                                      if (notifResult.notifications.isArray()) {
                                        for (int i = 0; i < notifResult.notifications.size(); ++i) {
                                          newNotifications.add(notifResult.notifications[i]);
                                        }
                                      }

                                      notificationSlice->dispatch(
                                          [newNotifications, notifResult](NotificationState &state) {
                                            for (const auto &notification : newNotifications) {
                                              state.notifications.add(notification);
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

  networkClient->markNotificationsRead([this, notificationSlice](Outcome<juce::var> result) {
    if (result.isOk()) {
      notificationSlice->dispatch([](NotificationState &state) {
        // Mark all notifications as read
        for (auto &notification : state.notifications) {
          notification.getDynamicObject()->setProperty("is_read", true);
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

  networkClient->markNotificationsSeen([this, notificationSlice](Outcome<juce::var> result) {
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
