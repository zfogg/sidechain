
.. _program_listing_file_plugin_source_ui_notifications_NotificationBell.h:

Program Listing for File NotificationBell.h
===========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_ui_notifications_NotificationBell.h>` (``plugin/source/ui/notifications/NotificationBell.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "../../util/HoverState.h"
   
   //==============================================================================
   class NotificationBell : public juce::Component,
                                      public juce::TooltipClient
   {
   public:
       NotificationBell();
       ~NotificationBell() override = default;
   
       //==============================================================================
       // Badge control
       void setUnseenCount(int count);
       int getUnseenCount() const { return unseenCount; }
   
       void setUnreadCount(int count);
       int getUnreadCount() const { return unreadCount; }
   
       // Clear badge (mark as seen)
       void clearBadge();
   
       //==============================================================================
       // Callbacks
       std::function<void()> onBellClicked;
   
       //==============================================================================
       // Component overrides
       void paint(juce::Graphics& g) override;
       void resized() override;
       void mouseEnter(const juce::MouseEvent& event) override;
       void mouseExit(const juce::MouseEvent& event) override;
       void mouseDown(const juce::MouseEvent& event) override;
   
       // TooltipClient override
       juce::String getTooltip() override;
   
       //==============================================================================
       // Layout constants
       static constexpr int PREFERRED_SIZE = 32;
       static constexpr int BADGE_SIZE = 18;
   
   private:
       //==============================================================================
       int unseenCount = 0;
       int unreadCount = 0;
       HoverState hoverState;
   
       //==============================================================================
       // Drawing helpers
       void drawBell(juce::Graphics& g, juce::Rectangle<float> bounds);
       void drawBadge(juce::Graphics& g, juce::Rectangle<float> bounds);
   
       // Format badge text (e.g., "5" or "99+")
       juce::String getBadgeText() const;
   
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationBell)
   };
