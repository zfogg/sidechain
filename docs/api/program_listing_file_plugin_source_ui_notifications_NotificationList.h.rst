
.. _program_listing_file_plugin_source_ui_notifications_NotificationList.h:

Program Listing for File NotificationList.h
===========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_ui_notifications_NotificationList.h>` (``plugin/source/ui/notifications/NotificationList.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "../../util/HoverState.h"
   
   //==============================================================================
   struct NotificationItem
   {
       juce::String id;
       juce::String groupKey;          // The aggregation group key
       juce::String verb;              // "like", "follow", "comment"
       int activityCount = 1;          // Total activities in group
       int actorCount = 1;             // Unique actors
       bool isRead = false;
       bool isSeen = false;
       juce::String createdAt;
       juce::String updatedAt;
   
       // First actor info (for display)
       juce::String actorId;
       juce::String actorName;
       juce::String actorAvatarUrl;
   
       // Target info (the object that was acted upon)
       juce::String targetId;          // e.g., "loop:123" or "user:456"
       juce::String targetType;        // "loop", "user", "comment"
       juce::String targetPreview;     // Preview text or title
   
       // Parse from JSON response
       static NotificationItem fromJson(const juce::var& json);
   
       // Generate display text like "Alice and 3 others liked your loop"
       juce::String getDisplayText() const;
   
       // Get relative time like "2h ago"
       juce::String getRelativeTime() const;
   
       // Get icon path/type for the verb
       juce::String getVerbIcon() const;
   };
   
   //==============================================================================
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
