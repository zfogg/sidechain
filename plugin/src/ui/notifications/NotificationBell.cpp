#include "NotificationBell.h"
#include "../../util/Log.h"

//==============================================================================
NotificationBell::NotificationBell() {
  setSize(PREFERRED_SIZE, PREFERRED_SIZE);

  // Set up hover state
  hoverState.onHoverChanged = [this]([[maybe_unused]] bool hovered) { repaint(); };
}

//==============================================================================
void NotificationBell::setUnseenCount(int count) {
  if (unseenCount != count) {
    unseenCount = juce::jmax(0, count);
    Log::debug("NotificationBell: Unseen count updated - " + juce::String(unseenCount));
    repaint();
  }
}

void NotificationBell::setUnreadCount(int count) {
  if (unreadCount != count) {
    unreadCount = juce::jmax(0, count);
    Log::debug("NotificationBell: Unread count updated - " + juce::String(unreadCount));
    repaint();
  }
}

void NotificationBell::setFollowRequestCount(int count) {
  if (followRequestCount != count) {
    followRequestCount = juce::jmax(0, count);
    Log::debug("NotificationBell: Follow request count updated - " + juce::String(followRequestCount));
    repaint();
  }
}

void NotificationBell::clearBadge() {
  if (unseenCount != 0) {
    unseenCount = 0;
    repaint();
  }
}

//==============================================================================
void NotificationBell::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();

  // Draw background on hover
  if (hoverState.isHovered()) {
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.fillRoundedRectangle(bounds, 6.0f);
  }

  // Draw the bell icon
  auto bellBounds = bounds.reduced(4.0f);
  drawBell(g, bellBounds);

  // Draw badge if there are unseen notifications or follow requests
  if (getTotalBadgeCount() > 0) {
    drawBadge(g, bounds);
  }
}

void NotificationBell::drawBell(juce::Graphics &g, juce::Rectangle<float> bounds) {
  // Bell color - slightly dimmed when no notifications, brighter on hover
  juce::Colour bellColor;
  if (getTotalBadgeCount() > 0)
    bellColor = juce::Colours::white;
  else if (hoverState.isHovered())
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
  bellPath.quadraticTo(bellX + bellWidth + bellWidth * 0.1f, bellY + bellHeight * 0.6f, bellX + bellWidth,
                       bellY + bellHeight);

  // Bottom
  bellPath.lineTo(bellX, bellY + bellHeight);

  // Left curve up
  bellPath.quadraticTo(bellX - bellWidth * 0.1f, bellY + bellHeight * 0.6f, topX, bellY + bellHeight * 0.15f);

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

void NotificationBell::drawBadge(juce::Graphics &g, juce::Rectangle<float> bounds) {
  // Badge dimensions - positioned at top right
  float badgeSize = static_cast<float>(BADGE_SIZE);
  float badgeY = bounds.getY() - 2.0f;

  // Draw badge background (red circle/pill)
  juce::Colour badgeColor(0xffff1744); // Material red A400
  g.setColour(badgeColor);

  juce::String badgeText = getBadgeText();
  juce::AttributedString attrStr;
  attrStr.setText(badgeText);
  attrStr.setFont(g.getCurrentFont());
  juce::TextLayout layout;
  layout.createLayout(attrStr, 10000.0f);
  float textWidth = layout.getWidth();

  // Use pill shape if text is wider than circle
  float minBadgeWidth = juce::jmax(badgeSize, textWidth + 8.0f);
  juce::Rectangle<float> badgeBounds(bounds.getRight() - minBadgeWidth - 1.0f, badgeY, minBadgeWidth, badgeSize);

  g.fillRoundedRectangle(badgeBounds, badgeSize / 2.0f);

  // Draw badge text
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
  g.drawText(badgeText, badgeBounds, juce::Justification::centred, false);
}

juce::String NotificationBell::getBadgeText() const {
  int total = getTotalBadgeCount();
  if (total >= 100)
    return "99+";
  return juce::String(total);
}

//==============================================================================
void NotificationBell::resized() {
  // No child components to layout
}

void NotificationBell::mouseEnter(const juce::MouseEvent &) {
  hoverState.setHovered(true);
}

void NotificationBell::mouseExit(const juce::MouseEvent &) {
  hoverState.setHovered(false);
}

void NotificationBell::mouseDown(const juce::MouseEvent &) {
  if (onBellClicked)
    onBellClicked();
}

juce::String NotificationBell::getTooltip() {
  int total = getTotalBadgeCount();

  if (total == 0)
    return "No new notifications";

  juce::String tooltip;
  if (unseenCount > 0) {
    tooltip = juce::String(unseenCount) + " new notification" + (unseenCount == 1 ? "" : "s");
  }
  if (followRequestCount > 0) {
    if (tooltip.isNotEmpty())
      tooltip += ", ";
    tooltip += juce::String(followRequestCount) + " follow request" + (followRequestCount == 1 ? "" : "s");
  }
  return tooltip;
}
