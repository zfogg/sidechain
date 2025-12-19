#pragma once

#include "../../stores/AppStore.h"
#include "../../ui/common/AppStoreComponent.h"
#include "../../util/HoverState.h"
#include <JuceHeader.h>

// ==============================================================================
/**
 * NotificationBell displays a bell icon with an optional badge
 * showing the count of unseen notifications.
 *
 * Features:
 * - Bell icon with animated hover effect
 * - Red badge with unseen count (hides when 0)
 * - "99+" display for overflow counts
 * - Click callback to open notification panel
 * - Tooltip showing notification status
 * - Reactive updates from AppStore NotificationState
 */
class NotificationBell : public Sidechain::UI::AppStoreComponent<Sidechain::Stores::NotificationState>,
                         public juce::TooltipClient {
public:
  NotificationBell(Sidechain::Stores::AppStore *store = nullptr);
  ~NotificationBell() override;

  // ==============================================================================
  // Badge control (legacy - prefer using store binding)
  void setUnseenCount(int count);
  int getUnseenCount() const {
    return unseenCount;
  }

  void setUnreadCount(int count);
  int getUnreadCount() const {
    return unreadCount;
  }

  // Follow request count (shown in combined badge)
  void setFollowRequestCount(int count);
  int getFollowRequestCount() const {
    return followRequestCount;
  }

  // Total badge count (unseenCount + followRequestCount)
  int getTotalBadgeCount() const {
    return unseenCount + followRequestCount;
  }

  // Clear badge (mark as seen)
  void clearBadge();

  // ==============================================================================
  // Callbacks
  std::function<void()> onBellClicked;

  // ==============================================================================
  // Component overrides
  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;
  void mouseDown(const juce::MouseEvent &event) override;

  // TooltipClient override
  juce::String getTooltip() override;

  // ==============================================================================
  // Layout constants
  static constexpr int PREFERRED_SIZE = 32;
  static constexpr int BADGE_SIZE = 18;

protected:
  // ==============================================================================
  // AppStoreComponent overrides
  void onAppStateChanged(const Sidechain::Stores::NotificationState &state) override;
  void subscribeToAppStore() override;

private:
  // ==============================================================================
  int unseenCount = 0;
  int unreadCount = 0;
  int followRequestCount = 0;
  HoverState hoverState;

  // ==============================================================================
  // Drawing helpers
  void drawBell(juce::Graphics &g, juce::Rectangle<float> bounds);
  void drawBadge(juce::Graphics &g, juce::Rectangle<float> bounds);

  // Format badge text (e.g., "5" or "99+")
  juce::String getBadgeText() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationBell)
};
