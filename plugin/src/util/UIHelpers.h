#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * UIHelpers - Common UI drawing utilities
 *
 * Provides consistent drawing patterns for cards, badges, buttons, and text
 * to reduce duplication across components.
 *
 * Usage:
 *   // Draw a card with background and border
 *   UIHelpers::drawCard(g, bounds, Colors::backgroundLight, Colors::border);
 *
 *   // Draw a badge with text
 *   UIHelpers::drawBadge(g, bounds, "120 BPM", Colors::badge, Colors::textPrimary);
 *
 *   // Truncate text with ellipsis
 *   auto truncated = UIHelpers::truncateWithEllipsis("Very long text...", font, 100);
 */
namespace UIHelpers
{
    //==========================================================================
    // Card/Panel Drawing

    /**
     * Draw a rounded rectangle card with fill and optional border.
     *
     * @param g            Graphics context
     * @param bounds       Rectangle to draw
     * @param fillColor    Background fill color
     * @param borderColor  Border color (use transparent for no border)
     * @param cornerRadius Corner radius (default 8.0f)
     * @param borderWidth  Border thickness (default 1.0f)
     */
    void drawCard(juce::Graphics& g,
                  juce::Rectangle<float> bounds,
                  juce::Colour fillColor,
                  juce::Colour borderColor = juce::Colours::transparentBlack,
                  float cornerRadius = 8.0f,
                  float borderWidth = 1.0f);

    /** Overload for int bounds */
    void drawCard(juce::Graphics& g,
                  juce::Rectangle<int> bounds,
                  juce::Colour fillColor,
                  juce::Colour borderColor = juce::Colours::transparentBlack,
                  float cornerRadius = 8.0f,
                  float borderWidth = 1.0f);

    /**
     * Draw a card with hover state support.
     */
    void drawCardWithHover(juce::Graphics& g,
                           juce::Rectangle<int> bounds,
                           juce::Colour normalColor,
                           juce::Colour hoverColor,
                           juce::Colour borderColor,
                           bool isHovered,
                           float cornerRadius = 8.0f);

    //==========================================================================
    // Badge/Tag Drawing

    /**
     * Draw a small badge/tag with text (e.g., "120 BPM", "NEW").
     *
     * @param g         Graphics context
     * @param bounds    Rectangle to draw
     * @param text      Badge text
     * @param bgColor   Background color
     * @param textColor Text color
     * @param fontSize  Font size (default 11.0f)
     * @param cornerRadius Corner radius (default 4.0f)
     */
    void drawBadge(juce::Graphics& g,
                   juce::Rectangle<int> bounds,
                   const juce::String& text,
                   juce::Colour bgColor,
                   juce::Colour textColor,
                   float fontSize = 11.0f,
                   float cornerRadius = 4.0f);

    /**
     * Draw a pill-shaped badge that auto-sizes to text.
     * Returns the actual bounds used.
     */
    juce::Rectangle<int> drawPillBadge(juce::Graphics& g,
                                        int x, int y,
                                        const juce::String& text,
                                        juce::Colour bgColor,
                                        juce::Colour textColor,
                                        float fontSize = 11.0f,
                                        int hPadding = 8,
                                        int vPadding = 4);

    //==========================================================================
    // Button Drawing

    /**
     * Draw a rounded button with text.
     *
     * @param g           Graphics context
     * @param bounds      Button bounds
     * @param text        Button text
     * @param bgColor     Background color
     * @param textColor   Text color
     * @param isHovered   Whether button is being hovered
     * @param cornerRadius Corner radius (default 6.0f)
     */
    void drawButton(juce::Graphics& g,
                    juce::Rectangle<int> bounds,
                    const juce::String& text,
                    juce::Colour bgColor,
                    juce::Colour textColor,
                    bool isHovered = false,
                    float cornerRadius = 6.0f);

    /**
     * Draw an outline-style button.
     */
    void drawOutlineButton(juce::Graphics& g,
                           juce::Rectangle<int> bounds,
                           const juce::String& text,
                           juce::Colour borderColor,
                           juce::Colour textColor,
                           bool isHovered = false,
                           float cornerRadius = 6.0f);

    //==========================================================================
    // Icon Drawing

    /**
     * Draw a circular icon button background.
     */
    void drawIconButton(juce::Graphics& g,
                        juce::Rectangle<int> bounds,
                        juce::Colour bgColor,
                        bool isHovered = false);

    /**
     * Draw a simple icon character (emoji or symbol).
     */
    void drawIcon(juce::Graphics& g,
                  juce::Rectangle<int> bounds,
                  const juce::String& icon,
                  juce::Colour color,
                  float fontSize = 16.0f);

    //==========================================================================
    // Progress/Status Drawing

    /**
     * Draw a horizontal progress bar.
     */
    void drawProgressBar(juce::Graphics& g,
                         juce::Rectangle<int> bounds,
                         float progress,  // 0.0 to 1.0
                         juce::Colour bgColor,
                         juce::Colour fillColor,
                         float cornerRadius = 4.0f);

    /**
     * Draw a loading spinner indicator.
     */
    void drawLoadingSpinner(juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            juce::Colour color,
                            float rotation);  // In radians

    //==========================================================================
    // Separator Drawing

    /**
     * Draw a horizontal divider line.
     */
    void drawDivider(juce::Graphics& g,
                     int x, int y, int width,
                     juce::Colour color,
                     float thickness = 1.0f);

    /**
     * Draw a vertical divider line.
     */
    void drawVerticalDivider(juce::Graphics& g,
                             int x, int y, int height,
                             juce::Colour color,
                             float thickness = 1.0f);

    //==========================================================================
    // Text Utilities

    /**
     * Truncate text to fit within maxWidth, adding ellipsis if needed.
     *
     * @param text     The text to truncate
     * @param font     Font to measure with
     * @param maxWidth Maximum width in pixels
     * @return         Truncated text with "..." if needed
     */
    juce::String truncateWithEllipsis(const juce::String& text,
                                       const juce::Font& font,
                                       int maxWidth);

    /**
     * Draw text with automatic ellipsis truncation.
     */
    void drawTruncatedText(juce::Graphics& g,
                           const juce::String& text,
                           juce::Rectangle<int> bounds,
                           juce::Colour color,
                           juce::Justification justification = juce::Justification::centredLeft);

    /**
     * Calculate text width for the current font.
     */
    int getTextWidth(const juce::Graphics& g, const juce::String& text);
    int getTextWidth(const juce::Font& font, const juce::String& text);

    //==========================================================================
    // Shadow/Effects

    /**
     * Draw a drop shadow beneath a rectangle.
     * Call before drawing the actual element.
     */
    void drawDropShadow(juce::Graphics& g,
                        juce::Rectangle<int> bounds,
                        juce::Colour shadowColor,
                        int radius = 4,
                        juce::Point<int> offset = {0, 2});

    //==========================================================================
    // Tooltip

    /**
     * Draw a tooltip box with text.
     */
    void drawTooltip(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     const juce::String& text,
                     juce::Colour bgColor,
                     juce::Colour textColor);

}  // namespace UIHelpers
