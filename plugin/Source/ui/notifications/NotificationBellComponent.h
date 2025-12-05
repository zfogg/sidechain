#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * NotificationBellComponent displays a bell icon with an optional badge
 * showing the count of unseen notifications.
 *
 * Features:
 * - Bell icon with animated hover effect
 * - Red badge with unseen count (hides when 0)
 * - "99+" display for overflow counts
 * - Click callback to open notification panel
 * - Tooltip showing notification status
 */
class NotificationBellComponent : public juce::Component,
                                   public juce::TooltipClient
{
public:
    NotificationBellComponent();
    ~NotificationBellComponent() override = default;

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
    bool isHovered = false;

    //==============================================================================
    // Drawing helpers
    void drawBell(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawBadge(juce::Graphics& g, juce::Rectangle<float> bounds);

    // Format badge text (e.g., "5" or "99+")
    juce::String getBadgeText() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotificationBellComponent)
};
