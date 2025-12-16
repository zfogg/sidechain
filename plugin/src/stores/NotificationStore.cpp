#include "NotificationStore.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

NotificationStore::NotificationStore(NetworkClient *client) : Store(NotificationState()), networkClient(client) {
  Util::logInfo("NotificationStore", "Initialized");
}

//==============================================================================
void NotificationStore::loadNotifications() {
  if (networkClient == nullptr) {
    Util::logWarning("NotificationStore", "Cannot load notifications - networkClient null");
    return;
  }

  Util::logInfo("NotificationStore", "Loading notifications");

  // Update state: set loading flag
  updateState([](NotificationState &state) {
    state.isLoading = true;
    state.offset = 0;
    state.notifications.clear();
    state.error = "";
  });

  auto state = getState();
  // Load from network
  networkClient->getNotifications(state.limit, 0, [this](Outcome<NetworkClient::NotificationResult> result) {
    handleNotificationsLoaded(result, false);
  });
}

void NotificationStore::loadMoreNotifications() {
  auto state = getState();
  if (!state.hasMore || state.isLoading || networkClient == nullptr) {
    return;
  }

  Util::logDebug("NotificationStore", "Loading more notifications, offset: " + juce::String(state.offset));

  updateState([](NotificationState &s) { s.isLoading = true; });

  networkClient->getNotifications(state.limit, state.offset, [this](Outcome<NetworkClient::NotificationResult> result) {
    handleNotificationsLoaded(result, true);
  });
}

void NotificationStore::refreshNotifications() {
  loadNotifications();
}

void NotificationStore::handleNotificationsLoaded(Outcome<NetworkClient::NotificationResult> result, bool append) {
  if (!result.isOk()) {
    updateState([error = result.getError()](NotificationState &s) {
      s.isLoading = false;
      s.error = error;
    });
    Util::logError("NotificationStore", "Failed to load notifications: " + result.getError());
    return;
  }

  auto notifResult = result.getValue();
  auto &notificationsData = notifResult.notifications;

  if (!notificationsData.isArray()) {
    updateState([](NotificationState &s) {
      s.isLoading = false;
      s.error = "Invalid notifications response";
    });
    return;
  }

  juce::Array<NotificationItem> loadedNotifications;
  for (int i = 0; i < notificationsData.size(); ++i) {
    auto notification = NotificationItem::fromJson(notificationsData[i]);
    loadedNotifications.add(notification);
  }

  updateState([loadedNotifications, &notifResult, append](NotificationState &s) {
    if (append) {
      s.notifications.addArray(loadedNotifications);
    } else {
      s.notifications = loadedNotifications;
    }
    s.isLoading = false;
    s.unseenCount = notifResult.unseen;
    s.unreadCount = notifResult.unread;
    s.offset += loadedNotifications.size();
    s.hasMore = loadedNotifications.size() >= s.limit;
    s.error = "";
    s.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  Util::logDebug("NotificationStore", "Loaded " + juce::String(loadedNotifications.size()) +
                                          " notifications, unseen: " + juce::String(notifResult.unseen) +
                                          ", unread: " + juce::String(notifResult.unread));
}

//==============================================================================
void NotificationStore::refreshCounts() {
  if (networkClient == nullptr) {
    return;
  }

  networkClient->getNotificationCounts([this](int unseen, int unread) {
    updateState([unseen, unread](NotificationState &s) {
      s.unseenCount = unseen;
      s.unreadCount = unread;
    });
  });
}

void NotificationStore::markAllSeen() {
  if (networkClient == nullptr) {
    Util::logWarning("NotificationStore", "Cannot mark seen - networkClient null");
    return;
  }

  // Optimistic update
  updateState([](NotificationState &s) {
    s.unseenCount = 0;
    for (auto &notification : s.notifications) {
      notification.isSeen = true;
    }
  });

  networkClient->markNotificationsSeen([this](Outcome<juce::var> result) { handleMarkSeenComplete(result); });
}

void NotificationStore::handleMarkSeenComplete(Outcome<juce::var> result) {
  if (!result.isOk()) {
    Util::logError("NotificationStore", "Failed to mark notifications seen: " + result.getError());
    // Refresh to get actual state
    refreshCounts();
  } else {
    Util::logDebug("NotificationStore", "Notifications marked as seen");
  }
}

void NotificationStore::markAllRead() {
  if (networkClient == nullptr) {
    Util::logWarning("NotificationStore", "Cannot mark read - networkClient null");
    return;
  }

  // Optimistic update
  updateState([](NotificationState &s) {
    s.unreadCount = 0;
    for (auto &notification : s.notifications) {
      notification.isRead = true;
    }
  });

  networkClient->markNotificationsRead([this](Outcome<juce::var> result) { handleMarkReadComplete(result); });
}

void NotificationStore::handleMarkReadComplete(Outcome<juce::var> result) {
  if (!result.isOk()) {
    Util::logError("NotificationStore", "Failed to mark notifications read: " + result.getError());
    // Refresh to get actual state
    refreshCounts();
  } else {
    Util::logDebug("NotificationStore", "Notifications marked as read");
  }
}

//==============================================================================
void NotificationStore::setFollowRequestCount(int count) {
  updateState([count](NotificationState &s) { s.followRequestCount = count; });
}

} // namespace Stores
} // namespace Sidechain
