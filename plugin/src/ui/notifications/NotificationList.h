#pragma once

#include <JuceHeader.h>
#include "../../util/HoverState.h"
#include "../../models/AggregatedFeedGroup.h"

//==============================================================================
/**
 * NotificationItem represents a single notification group from getstream.io
 *
 * Now uses AggregatedFeedGroup as the underlying model for consistency.
 * getstream.io groups notifications by aggregation format: {{ verb }}_{{ time.strftime("%Y-%m-%d") }}
 *
 * Examples:
 * - "Alice and 3 others liked your loop" (grouped by verb+target+day)
 * - "Bob started following you" (single follow notification)
 */
struct NotificationItem
{
    AggregatedFeedGroup group;      // The underlying aggregated group
    bool isRead = false;
    bool isSeen = false;

    // Derived from first activity in group
    juce::String actorId;
    juce::String actorName;
    juce::String actorAvatarUrl;
    juce::String targetId;          // e.g., "loop:123" or "user:456"
    juce::String targetType;        // "loop", "user", "comment"
    juce::String targetPreview;     // Preview text or title

    // Parse from JSON response (old format for backward compatibility)
    static NotificationItem fromJson(const juce::var& json);

    // Create from AggregatedFeedGroup
    static NotificationItem fromAggregatedGroup(const AggregatedFeedGroup& group, bool read = false, bool seen = false);

    // Generate display text like "Alice and 3 others liked your loop"
    juce::String getDisplayText() const;

    // Get relative time like "2h ago"
    juce::String getRelativeTime() const;

    // Get icon path/type for the verb
    juce::String getVerbIcon() const;
};

//==============================================================================
/**
 * NotificationRow displays a single notification item
 */
class NotificationRow : public juce::Component
{
public:
    NotificationRow();
    ~NotificationRow() override = default;

    void setNotification(const NotificationItem& notification);
    const NotificationItem& getNotification() const { return notification; }

    // Callbacks
    std::function<void(const NotificationItem&)> onClicked;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;

    static constexpr int ROW_HEIGHT = 72;

private:
    NotificationItem notification;
    HoverState hoverState;

    void drawAvatar(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawVerbIcon(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawText(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawUnreadIndicator(juce::Graphics& g, juce::Rectangle<int> bounds);

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
 */
class NotificationList : public juce::Component,
                                   public juce::ScrollBar::Listener
{
public:
    NotificationList();
    ~NotificationList() override;

    //==============================================================================
    // Data binding
    void setNotifications(const juce::Array<NotificationItem>& notifications);
    void clearNotifications();
    void setLoading(bool loading);
    void setError(const juce::String& errorMessage);

    // Update counts (displayed in header)
    void setUnseenCount(int count);
    void setUnreadCount(int count);

    //==============================================================================
    // Callbacks
    std::function<void(const NotificationItem&)> onNotificationClicked;
    std::function<void()> onMarkAllReadClicked;
    std::function<void()> onCloseClicked;
    std::function<void()> onRefreshRequested;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

    //==============================================================================
    // Layout constants
    static constexpr int HEADER_HEIGHT = 50;
    static constexpr int PREFERRED_WIDTH = 360;
    static constexpr int MAX_HEIGHT = 500;

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
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLoadingState(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawErrorState(juce::Graphics& g, juce::Rectangle<int> bounds);

    // Layout helpers
    void rebuildRowComponents();
    void layoutRows();

    // Hit testing
    juce::Rectangle<int> getMarkAllReadButtonBounds() const;
    juce::Rectangle<int> getCloseButtonBounds() const;

    // Mouse handling
    void mouseDown(const juce::MouseEvent& event) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationList)
};
