#pragma once

#include "../../stores/AppStore.h"
#include "../../ui/common/AppStoreComponent.h"
#include "../../util/HoverState.h"
#include "NotificationItem.h"
#include <JuceHeader.h>

//==============================================================================
/**
 * NotificationRow displays a single notification item
 */
class NotificationRow : public juce::Component {
public:
  NotificationRow();
  ~NotificationRow() override = default;

  void setNotification(const NotificationItem &notification);
  const NotificationItem &getNotification() const {
    return notification;
  }

  // Callbacks
  std::function<void(const NotificationItem &)> onClicked;

  // Component overrides
  void paint(juce::Graphics &g) override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;
  void mouseDown(const juce::MouseEvent &event) override;

  static constexpr int ROW_HEIGHT = 72;

private:
  NotificationItem notification;
  HoverState hoverState;

  void drawAvatar(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawVerbIcon(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawText(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawUnreadIndicator(juce::Graphics &g, juce::Rectangle<int> bounds);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationRow)
};

//==============================================================================
/**
 * NotificationList displays a scrollable list of notification groups
 *
 * Features:
 * - Scrollable list of grouped notifications
 * - Read/unread visual distinction (bold text, blue dot)
 * - Click to navigate to the notification target
 * - "Mark all as read" button
 * - Empty state when no notifications
 * - Loading state while fetching
 * - Reactive updates from AppStore NotificationState
 */
class NotificationList : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::NotificationState>,
                         public juce::ScrollBar::Listener {
public:
  NotificationList(Sidechain::Stores::AppStore *store = nullptr);
  ~NotificationList() override;

  //==============================================================================
  // Data binding (legacy - prefer using store binding)
  void setNotifications(const juce::Array<NotificationItem> &notifications);
  void clearNotifications();
  void setLoading(bool loading);
  void setError(const juce::String &errorMessage);

  // Update counts (displayed in header)
  void setUnseenCount(int count);
  void setUnreadCount(int count);

  //==============================================================================
  // Callbacks
  std::function<void(const NotificationItem &)> onNotificationClicked;
  std::function<void()> onMarkAllReadClicked; // Legacy - store binding calls markAllRead directly
  std::function<void()> onCloseClicked;
  std::function<void()> onRefreshRequested;

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void scrollBarMoved(juce::ScrollBar *scrollBar, double newRangeStart) override;

  //==============================================================================
  // Layout constants
  static constexpr int HEADER_HEIGHT = 50;
  static constexpr int PREFERRED_WIDTH = 360;
  static constexpr int MAX_HEIGHT = 500;

protected:
  //==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const Sidechain::Stores::NotificationState &state) override;
  void subscribeToAppStore() override;

private:
  //==============================================================================
  juce::Array<NotificationItem> notifications;
  juce::OwnedArray<NotificationRow> rowComponents;

  int unseenCount = 0;
  int unreadCount = 0;
  bool isLoading = false;
  juce::String errorMessage;

  // Scrolling
  juce::Viewport viewport;
  juce::Component contentComponent;
  int scrollOffset = 0;

  //==============================================================================
  // Drawing helpers
  void drawHeader(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawEmptyState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawLoadingState(juce::Graphics &g, juce::Rectangle<int> bounds);
  void drawErrorState(juce::Graphics &g, juce::Rectangle<int> bounds);

  // Layout helpers
  void rebuildRowComponents();
  void layoutRows();

  // Hit testing
  juce::Rectangle<int> getMarkAllReadButtonBounds() const;
  juce::Rectangle<int> getCloseButtonBounds() const;

  // Mouse handling
  void mouseDown(const juce::MouseEvent &event) override;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationList)
};
