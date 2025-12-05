#include "NotificationBellComponent.h"
#include "../../util/Log.h"

//==============================================================================
NotificationBellComponent::NotificationBellComponent()
{
    setSize(PREFERRED_SIZE, PREFERRED_SIZE);
}

//==============================================================================
void NotificationBellComponent::setUnseenCount(int count)
{
    if (unseenCount != count)
    {
        unseenCount = juce::jmax(0, count);
        Log::debug("NotificationBellComponent: Unseen count updated - " + juce::String(unseenCount));
        repaint();
    }
}

void NotificationBellComponent::setUnreadCount(int count)
{
    if (unreadCount != count)
    {
        unreadCount = juce::jmax(0, count);
        Log::debug("NotificationBellComponent: Unread count updated - " + juce::String(unreadCount));
        repaint();
    }
}

void NotificationBellComponent::clearBadge()
{
    if (unseenCount != 0)
    {
        unseenCount = 0;
        repaint();
    }
}

//==============================================================================
void NotificationBellComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Draw background on hover
    if (isHovered)
    {
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.fillRoundedRectangle(bounds, 6.0f);
    }

    // Draw the bell icon
    auto bellBounds = bounds.reduced(4.0f);
    drawBell(g, bellBounds);

    // Draw badge if there are unseen notifications
    if (unseenCount > 0)
    {
        drawBadge(g, bounds);
    }
}

void NotificationBellComponent::drawBell(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Bell color - slightly dimmed when no notifications, brighter on hover
    juce::Colour bellColor;
    if (unseenCount > 0)
        bellColor = juce::Colours::white;
    else if (isHovered)
        bellColor = juce::Colours::white.withAlpha(0.9f);
    else
        bellColor = juce::Colours::white.withAlpha(0.7f);

    g.setColour(bellColor);

    // Calculate bell dimensions
    float bellWidth = bounds.getWidth() * 0.7f;
    float bellHeight = bounds.getHeight() * 0.65f;
    float bellX = bounds.getCentreX() - bellWidth / 2.0f;
    float bellY = bounds.getY() + bounds.getHeight() * 0.1f;

    // Draw bell body (rounded trapezoid shape using path)
    juce::Path bellPath;

    // Top of bell (narrow)
    float topWidth = bellWidth * 0.3f;
    float topX = bellX + (bellWidth - topWidth) / 2.0f;

    // Bell body path
    bellPath.startNewSubPath(topX, bellY + bellHeight * 0.15f);
    bellPath.lineTo(topX + topWidth, bellY + bellHeight * 0.15f);

    // Right curve down
    bellPath.quadraticTo(
        bellX + bellWidth + bellWidth * 0.1f, bellY + bellHeight * 0.6f,
        bellX + bellWidth, bellY + bellHeight
    );

    // Bottom
    bellPath.lineTo(bellX, bellY + bellHeight);

    // Left curve up
    bellPath.quadraticTo(
        bellX - bellWidth * 0.1f, bellY + bellHeight * 0.6f,
        topX, bellY + bellHeight * 0.15f
    );

    bellPath.closeSubPath();
    g.fillPath(bellPath);

    // Draw bell top (handle/hook)
    float handleWidth = bellWidth * 0.15f;
    float handleHeight = bellHeight * 0.2f;
    float handleX = bounds.getCentreX() - handleWidth / 2.0f;
    float handleY = bellY;

    g.fillRoundedRectangle(handleX, handleY, handleWidth, handleHeight, handleWidth / 2.0f);

    // Draw clapper (small circle at bottom)
    float clapperSize = bellWidth * 0.2f;
    float clapperX = bounds.getCentreX() - clapperSize / 2.0f;
    float clapperY = bellY + bellHeight + clapperSize * 0.3f;

    g.fillEllipse(clapperX, clapperY, clapperSize, clapperSize);
}

void NotificationBellComponent::drawBadge(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Badge dimensions - positioned at top right
    float badgeSize = static_cast<float>(BADGE_SIZE);
    float badgeX = bounds.getRight() - badgeSize - 1.0f;
    float badgeY = bounds.getY() - 2.0f;

    // Draw badge background (red circle/pill)
    juce::Colour badgeColor(0xffff1744); // Material red A400
    g.setColour(badgeColor);

    juce::String badgeText = getBadgeText();
    float textWidth = g.getCurrentFont().getStringWidthFloat(badgeText);

    // Use pill shape if text is wider than circle
    float minBadgeWidth = juce::jmax(badgeSize, textWidth + 8.0f);
    juce::Rectangle<float> badgeBounds(
        bounds.getRight() - minBadgeWidth - 1.0f,
        badgeY,
        minBadgeWidth,
        badgeSize
    );

    g.fillRoundedRectangle(badgeBounds, badgeSize / 2.0f);

    // Draw badge text
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(badgeText, badgeBounds, juce::Justification::centred, false);
}

juce::String NotificationBellComponent::getBadgeText() const
{
    if (unseenCount >= 100)
        return "99+";
    return juce::String(unseenCount);
}

//==============================================================================
void NotificationBellComponent::resized()
{
    // No child components to layout
}

void NotificationBellComponent::mouseEnter(const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void NotificationBellComponent::mouseExit(const juce::MouseEvent&)
{
    isHovered = false;
    repaint();
}

void NotificationBellComponent::mouseDown(const juce::MouseEvent&)
{
    if (onBellClicked)
        onBellClicked();
}

juce::String NotificationBellComponent::getTooltip()
{
    if (unseenCount == 0)
        return "No new notifications";
    else if (unseenCount == 1)
        return "1 new notification";
    else
        return juce::String(unseenCount) + " new notifications";
}
