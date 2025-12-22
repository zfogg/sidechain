#include "UIHelpers.h"

namespace UIHelpers {

// ==============================================================================
// Card/Panel Drawing

void drawCard(juce::Graphics &g, juce::Rectangle<float> bounds, juce::Colour fillColor, juce::Colour borderColor,
              float cornerRadius, float borderWidth) {
  // Fill
  g.setColour(fillColor);
  g.fillRoundedRectangle(bounds, cornerRadius);

  // Border (only if not transparent)
  if (borderColor.getAlpha() > 0) {
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds, cornerRadius, borderWidth);
  }
}

void drawCard(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour fillColor, juce::Colour borderColor,
              float cornerRadius, float borderWidth) {
  drawCard(g, bounds.toFloat(), fillColor, borderColor, cornerRadius, borderWidth);
}

void drawCardWithHover(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour normalColor,
                       juce::Colour hoverColor, juce::Colour borderColor, bool isHovered, float cornerRadius) {
  drawCard(g, bounds, isHovered ? hoverColor : normalColor, borderColor, cornerRadius);
}

// ==============================================================================
// Badge/Tag Drawing

void drawBadge(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
               juce::Colour textColor, float fontSize, float cornerRadius) {
  // Background
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.toFloat(), cornerRadius);

  // Text
  g.setColour(textColor);
  g.setFont(fontSize);
  g.drawText(text, bounds, juce::Justification::centred);
}

juce::Rectangle<int> drawPillBadge(juce::Graphics &g, int x, int y, const juce::String &text, juce::Colour bgColor,
                                   juce::Colour textColor, float fontSize, int hPadding, int vPadding) {
  juce::Font font(juce::FontOptions().withHeight(fontSize));
  juce::AttributedString attrStr;
  attrStr.setText(text);
  attrStr.setFont(font);
  juce::TextLayout layout;
  layout.createLayout(attrStr, 10000.0f); // Large width for single-line text
  int textWidth = static_cast<int>(layout.getWidth());
  int height = static_cast<int>(fontSize) + vPadding * 2;
  int width = textWidth + hPadding * 2;

  g.setFont(font);

  auto bounds = juce::Rectangle<int>(x, y, width, height);

  // Background pill
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.toFloat(), static_cast<float>(height) / 2.0f);

  // Text
  g.setColour(textColor);
  g.drawText(text, bounds, juce::Justification::centred);

  return bounds;
}

// ==============================================================================
// Button Drawing

void drawButton(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
                juce::Colour textColor, bool isHovered, float cornerRadius) {
  // Adjust color for hover
  auto adjustedBg = isHovered ? bgColor.brighter(0.1f) : bgColor;

  // Background
  g.setColour(adjustedBg);
  g.fillRoundedRectangle(bounds.toFloat(), cornerRadius);

  // Text
  g.setColour(textColor);
  g.drawText(text, bounds, juce::Justification::centred);
}

void drawOutlineButton(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text,
                       juce::Colour borderColor, juce::Colour textColor, bool isHovered, float cornerRadius) {
  // Adjust for hover - subtle fill
  if (isHovered) {
    g.setColour(borderColor.withAlpha(0.1f));
    g.fillRoundedRectangle(bounds.toFloat(), cornerRadius);
  }

  // Border
  g.setColour(borderColor);
  g.drawRoundedRectangle(bounds.toFloat(), cornerRadius, 1.0f);

  // Text
  g.setColour(textColor);
  g.drawText(text, bounds, juce::Justification::centred);
}

// ==============================================================================
// Avatar Drawing

void drawCircularAvatar(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::Image &image,
                        juce::Colour placeholderColor, juce::Colour borderColor, float borderWidth) {
  if (image.isValid()) {
    // Draw the image clipped to a circle
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());

    g.saveState();
    g.reduceClipRegion(circlePath);

    // Scale and draw the image to fit the bounds
    auto scaledImage = image.rescaled(bounds.getWidth(), bounds.getHeight(), juce::Graphics::highResamplingQuality);
    g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());

    g.restoreState();
  } else {
    // Draw placeholder circle
    g.setColour(placeholderColor);
    g.fillEllipse(bounds.toFloat());
  }

  // Draw border (only if not transparent)
  if (borderColor.getAlpha() > 0) {
    g.setColour(borderColor);
    g.drawEllipse(bounds.toFloat(), borderWidth);
  }
}

void drawOnlineIndicator(juce::Graphics &g, juce::Rectangle<int> avatarBounds, bool isOnline, bool isInStudio,
                         juce::Colour backgroundColor, int indicatorSize) {
  if (!isOnline && !isInStudio) {
    return;
  }

  const int borderWidth = 2;

  // Position at bottom-right of avatar
  auto indicatorBounds =
      juce::Rectangle<int>(avatarBounds.getRight() - indicatorSize + 2, avatarBounds.getBottom() - indicatorSize + 2,
                           indicatorSize, indicatorSize)
          .toFloat();

  // Draw dark border (matches card background)
  g.setColour(backgroundColor);
  g.fillEllipse(indicatorBounds);

  // Draw indicator (cyan for in_studio, green for just online)
  auto innerBounds = indicatorBounds.reduced(static_cast<float>(borderWidth));
  // In-studio: cyan (#00D4FF), Online: green (#00D464)
  g.setColour(isInStudio ? juce::Colour(0xFF00D4FF) : juce::Colour(0xFF00D464));
  g.fillEllipse(innerBounds);
}

// ==============================================================================
// Follow Button Drawing

void drawFollowButton(juce::Graphics &g, juce::Rectangle<int> bounds, bool isFollowing, juce::Colour followColor,
                      juce::Colour followTextColor, juce::Colour followingTextColor, juce::Colour borderColor,
                      float cornerRadius) {
  juce::String buttonText = isFollowing ? "Following" : "Follow";

  if (isFollowing) {
    // Following state: transparent background with border
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), cornerRadius, 1.0f);

    g.setColour(followingTextColor);
    g.setFont(11.0f);
    g.drawText(buttonText, bounds, juce::Justification::centred);
  } else {
    // Not following: filled button
    g.setColour(followColor);
    g.fillRoundedRectangle(bounds.toFloat(), cornerRadius);

    g.setColour(followTextColor);
    g.setFont(11.0f);
    g.drawText(buttonText, bounds, juce::Justification::centred);
  }
}

// ==============================================================================
// Icon Drawing

void drawIconButton(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour bgColor, bool isHovered) {
  auto adjustedBg = isHovered ? bgColor.brighter(0.15f) : bgColor;
  g.setColour(adjustedBg);
  g.fillEllipse(bounds.toFloat());
}

void drawIcon(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &icon, juce::Colour color,
              float fontSize) {
  g.setColour(color);
  g.setFont(fontSize);
  g.drawText(icon, bounds, juce::Justification::centred);
}

// ==============================================================================
// Progress/Status Drawing

void drawProgressBar(juce::Graphics &g, juce::Rectangle<int> bounds, float progress, juce::Colour bgColor,
                     juce::Colour fillColor, float cornerRadius) {
  progress = juce::jlimit(0.0f, 1.0f, progress);

  // Background track
  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.toFloat(), cornerRadius);

  // Progress fill
  if (progress > 0.0f) {
    auto fillWidth = static_cast<int>(static_cast<float>(bounds.getWidth()) * progress);
    auto fillBounds = bounds.withWidth(juce::jmax(1, fillWidth));

    g.setColour(fillColor);
    g.fillRoundedRectangle(fillBounds.toFloat(), cornerRadius);
  }
}

void drawLoadingSpinner(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour color, float rotation) {
  auto center = bounds.getCentre().toFloat();
  float radius = static_cast<float>(juce::jmin(bounds.getWidth(), bounds.getHeight())) / 2.0f - 2.0f;

  // Draw spinning arc
  juce::Path arc;
  arc.addCentredArc(center.x, center.y, radius, radius, rotation, 0.0f, juce::MathConstants<float>::pi * 1.5f, true);

  g.setColour(color);
  g.strokePath(arc, juce::PathStrokeType(2.5f));
}

// ==============================================================================
// Separator Drawing

void drawDivider(juce::Graphics &g, int x, int y, int width, juce::Colour color, float thickness) {
  g.setColour(color);
  g.fillRect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), thickness);
}

void drawVerticalDivider(juce::Graphics &g, int x, int y, int height, juce::Colour color, float thickness) {
  g.setColour(color);
  g.fillRect(static_cast<float>(x), static_cast<float>(y), thickness, static_cast<float>(height));
}

// ==============================================================================
// Text Utilities

juce::String truncateWithEllipsis(const juce::String &text, const juce::Font &font, int maxWidth) {
  if (text.isEmpty() || maxWidth <= 0)
    return text;

  juce::AttributedString attrStr;
  attrStr.setText(text);
  attrStr.setFont(font);
  juce::TextLayout layout;
  layout.createLayout(attrStr, 10000.0f); // Large width for single-line text
  float textWidth = layout.getWidth();
  if (textWidth <= static_cast<float>(maxWidth))
    return text;

  // Binary search for the right length
  juce::String ellipsis = "...";
  juce::AttributedString ellipsisAttrStr;
  ellipsisAttrStr.setText(ellipsis);
  ellipsisAttrStr.setFont(font);
  juce::TextLayout ellipsisLayout;
  ellipsisLayout.createLayout(ellipsisAttrStr, 10000.0f);
  float ellipsisWidth = ellipsisLayout.getWidth();

  if (ellipsisWidth >= static_cast<float>(maxWidth))
    return ellipsis;

  int availableWidth = maxWidth - static_cast<int>(ellipsisWidth);

  // Start from the end and work backwards
  for (int len = text.length() - 1; len > 0; --len) {
    auto truncated = text.substring(0, len);
    juce::AttributedString truncatedAttrStr;
    truncatedAttrStr.setText(truncated);
    truncatedAttrStr.setFont(font);
    juce::TextLayout truncatedLayout;
    truncatedLayout.createLayout(truncatedAttrStr, 10000.0f);
    if (truncatedLayout.getWidth() <= static_cast<float>(availableWidth))
      return truncated.trimEnd() + ellipsis;
  }

  return ellipsis;
}

void drawTruncatedText(juce::Graphics &g, const juce::String &text, juce::Rectangle<int> bounds, juce::Colour color,
                       juce::Justification justification) {
  g.setColour(color);
  auto truncated = truncateWithEllipsis(text, g.getCurrentFont(), bounds.getWidth());
  g.drawText(truncated, bounds, justification);
}

int getTextWidth(const juce::Graphics &g, const juce::String &text) {
  juce::AttributedString attrStr;
  attrStr.setText(text);
  attrStr.setFont(g.getCurrentFont());
  juce::TextLayout layout;
  layout.createLayout(attrStr, 10000.0f); // Large width for single-line text
  return static_cast<int>(layout.getWidth());
}

int getTextWidth(const juce::Font &font, const juce::String &text) {
  juce::AttributedString attrStr;
  attrStr.setText(text);
  attrStr.setFont(font);
  juce::TextLayout layout;
  layout.createLayout(attrStr, 10000.0f); // Large width for single-line text
  return static_cast<int>(layout.getWidth());
}

// ==============================================================================
// Shadow/Effects

void drawDropShadow(juce::Graphics &g, juce::Rectangle<int> bounds, juce::Colour shadowColor, int radius,
                    juce::Point<int> offset) {
  auto shadowBounds = bounds.translated(offset.x, offset.y);

  // Draw multiple layers with decreasing alpha for soft shadow
  for (int i = radius; i > 0; --i) {
    float alpha = shadowColor.getFloatAlpha() * (1.0f - (static_cast<float>(i) / static_cast<float>(radius)));
    g.setColour(shadowColor.withAlpha(alpha * 0.3f));
    g.fillRoundedRectangle(shadowBounds.expanded(i).toFloat(), 8.0f + static_cast<float>(i));
  }
}

// ==============================================================================
// Tooltip

void drawTooltip(juce::Graphics &g, juce::Rectangle<int> bounds, const juce::String &text, juce::Colour bgColor,
                 juce::Colour textColor) {
  // Background with slight shadow effect
  g.setColour(bgColor.darker(0.1f));
  g.fillRoundedRectangle(bounds.translated(1, 1).toFloat(), 4.0f);

  g.setColour(bgColor);
  g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

  // Border
  g.setColour(bgColor.darker(0.2f));
  g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);

  // Text
  g.setColour(textColor);
  g.setFont(12.0f);
  g.drawText(text, bounds.reduced(6, 2), juce::Justification::centred);
}

} // namespace UIHelpers
