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

  auto notificationSlice = sliceManager.notifications;
  NotificationState loadingState = notificationSlice->getState();
  loadingState.isLoading = true;
  notificationSlice->setState(loadingState);

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

      NotificationState successState = notificationSlice->getState();
      successState.notifications = notificationsList;
      successState.isLoading = false;
      successState.unreadCount = notifResult.unread;
      successState.unseenCount = notifResult.unseen;
      successState.notificationError = "";
      Util::logInfo("AppStore", "Loaded " + juce::String(notificationsList.size()) +
                                    " notifications (unread: " + juce::String(notifResult.unread) + ")");
      notificationSlice->setState(successState);
    } else {
      NotificationState errorState = notificationSlice->getState();
      errorState.isLoading = false;
      errorState.notificationError = result.getError();
      Util::logError("AppStore", "Failed to load notifications: " + result.getError());
      notificationSlice->setState(errorState);
    }
  });
}

void AppStore::loadMoreNotifications() {
  if (!networkClient) {
    return;
  }

  auto notificationSlice = sliceManager.notifications;
  const auto &currentState = notificationSlice->getState();
  if (currentState.notifications.empty()) {
    return;
  }

  networkClient->getNotifications(20, static_cast<int>(currentState.notifications.size()),
                                  [notificationSlice](Outcome<NetworkClient::NotificationResult> result) {
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

                                      NotificationState moreState = notificationSlice->getState();
                                      for (const auto &notification : newNotifications) {
                                        moreState.notifications.push_back(notification);
                                      }
                                      moreState.unreadCount = notifResult.unread;
                                      moreState.unseenCount = notifResult.unseen;
                                      notificationSlice->setState(moreState);
                                    }
                                  });
}

void AppStore::markNotificationsAsRead() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as read - network client not set");
    return;
  }

  auto notificationSlice = sliceManager.notifications;

  networkClient->markNotificationsRead([notificationSlice](Outcome<juce::var> result) {
    if (result.isOk()) {
      NotificationState readState = notificationSlice->getState();
      // Mark all notifications as read
      for (auto &notification : readState.notifications) {
        if (notification) {
          notification->isRead = true;
        }
      }
      readState.unreadCount = 0;
      Util::logInfo("AppStore", "All notifications marked as read");
      notificationSlice->setState(readState);
    } else {
      NotificationState errorState = notificationSlice->getState();
      errorState.notificationError = result.getError();
      Util::logError("AppStore", "Failed to mark notifications as read: " + result.getError());
      notificationSlice->setState(errorState);
    }
  });
}

void AppStore::markNotificationsAsSeen() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot mark notifications as seen - network client not set");
    return;
  }

  auto notificationSlice = sliceManager.notifications;

  networkClient->markNotificationsSeen([notificationSlice](Outcome<juce::var> result) {
    if (result.isOk()) {
      NotificationState seenState = notificationSlice->getState();
      seenState.unseenCount = 0;
      Util::logInfo("AppStore", "All notifications marked as seen");
      notificationSlice->setState(seenState);
    } else {
      NotificationState errorState = notificationSlice->getState();
      errorState.notificationError = result.getError();
      Util::logError("AppStore", "Failed to mark notifications as seen: " + result.getError());
      notificationSlice->setState(errorState);
    }
  });
}

} // namespace Stores
} // namespace Sidechain
