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

// ==============================================================================
// Reactive Notification Observables (Phase 6)
// ==============================================================================
//
// These methods return rxcpp::observable with proper model types (Notification values,
// not shared_ptr). They use the same network calls as the Redux actions above
// but wrap them in reactive streams for compose-able operations.

rxcpp::observable<std::vector<Notification>> AppStore::loadNotificationsObservable(int limit, int offset) {
  using ResultType = std::vector<Notification>;
  return rxcpp::sources::create<ResultType>([this, limit, offset](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Loading notifications via observable");

           networkClient->getNotifications(
               limit, offset, [observer](Outcome<NetworkClient::NotificationResult> result) {
                 if (result.isOk()) {
                   const auto notifResult = result.getValue();
                   ResultType notifications;

                   if (notifResult.notifications.isArray()) {
                     for (int i = 0; i < notifResult.notifications.size(); ++i) {
                       try {
                         auto jsonStr = juce::JSON::toString(notifResult.notifications[i]);
                         auto jsonObj = nlohmann::json::parse(jsonStr.toStdString());
                         Notification notif;
                         from_json(jsonObj, notif);
                         if (notif.isValid()) {
                           notifications.push_back(std::move(notif));
                         }
                       } catch (const std::exception &e) {
                         Util::logWarning("AppStore", "Failed to parse notification: " + juce::String(e.what()));
                       }
                     }
                   }

                   Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(notifications.size())) +
                                                 " notifications (unread: " + juce::String(notifResult.unread) + ")");
                   observer.on_next(std::move(notifications));
                   observer.on_completed();
                 } else {
                   Util::logError("AppStore", "Failed to load notifications: " + result.getError());
                   observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                 }
               });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::markNotificationsAsReadObservable() {
  return rxcpp::sources::create<int>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Marking notifications as read via observable");

           networkClient->markNotificationsRead([this, observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               // Update state
               auto notificationSlice = sliceManager.notifications;
               if (notificationSlice) {
                 NotificationState readState = notificationSlice->getState();
                 for (auto &notification : readState.notifications) {
                   if (notification) {
                     notification->isRead = true;
                   }
                 }
                 readState.unreadCount = 0;
                 notificationSlice->setState(readState);
               }

               Util::logInfo("AppStore", "All notifications marked as read");
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to mark notifications as read: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::markNotificationsAsSeenObservable() {
  return rxcpp::sources::create<int>([this](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Marking notifications as seen via observable");

           networkClient->markNotificationsSeen([this, observer](Outcome<juce::var> result) {
             if (result.isOk()) {
               // Update state
               auto notificationSlice = sliceManager.notifications;
               if (notificationSlice) {
                 NotificationState seenState = notificationSlice->getState();
                 seenState.unseenCount = 0;
                 notificationSlice->setState(seenState);
               }

               Util::logInfo("AppStore", "All notifications marked as seen");
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to mark notifications as seen: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
