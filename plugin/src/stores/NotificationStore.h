#pragma once

#include "../network/NetworkClient.h"
#include "../ui/notifications/NotificationItem.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * NotificationState - Immutable state for notifications
 */
struct NotificationState {
  juce::Array<NotificationItem> notifications;
  int unseenCount = 0;
  int unreadCount = 0;
  int followRequestCount = 0;
  bool isLoading = false;
  juce::String error;
  int offset = 0;
  int limit = 20;
  bool hasMore = true;
  int64_t lastUpdated = 0;
};

/**
 * NotificationStore - Reactive store for managing notifications
 *
 * Features:
 * - Load notifications for current user
 * - Track unseen/unread counts for badge display
 * - Mark notifications as read/seen
 * - Real-time count updates
 * - Pagination support
 *
 * Usage:
 * ```cpp
 * auto notificationStore = std::make_shared<NotificationStore>(networkClient);
 * notificationStore->subscribe([this](const NotificationState& state) {
 *   bellComponent.setUnseenCount(state.unseenCount);
 *   bellComponent.setUnreadCount(state.unreadCount);
 * });
 * notificationStore->loadNotifications();
 * ```
 */
class NotificationStore : public Store<NotificationState> {
public:
  explicit NotificationStore(NetworkClient *client);
  ~NotificationStore() override = default;

  //==============================================================================
  // Data Loading
  void loadNotifications();
  void loadMoreNotifications();
  void refreshNotifications();

  //==============================================================================
  // Count Operations
  /**
   * Refresh just the notification counts (unseen/unread)
   * Lightweight operation for periodic polling
   */
  void refreshCounts();

  /**
   * Mark all notifications as seen (clears badge)
   */
  void markAllSeen();

  /**
   * Mark all notifications as read
   */
  void markAllRead();

  //==============================================================================
  // Follow Request Count
  /**
   * Set the follow request count (usually managed separately)
   */
  void setFollowRequestCount(int count);

  //==============================================================================
  // Current State Access
  int getUnseenCount() const {
    return getState().unseenCount;
  }
  int getUnreadCount() const {
    return getState().unreadCount;
  }
  int getFollowRequestCount() const {
    return getState().followRequestCount;
  }
  int getTotalBadgeCount() const {
    auto state = getState();
    return state.unseenCount + state.followRequestCount;
  }
  bool isLoading() const {
    return getState().isLoading;
  }

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handleNotificationsLoaded(Outcome<NetworkClient::NotificationResult> result, bool append);
  void handleCountsLoaded(Outcome<std::pair<int, int>> result);
  void handleMarkSeenComplete(Outcome<juce::var> result);
  void handleMarkReadComplete(Outcome<juce::var> result);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationStore)
};

} // namespace Stores
} // namespace Sidechain
