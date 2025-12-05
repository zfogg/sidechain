#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Image drawing utilities for common UI patterns.
 */
namespace ImageUtils
{
    /**
     * Draw an image clipped to a circle.
     *
     * @param g       Graphics context
     * @param bounds  Rectangle to draw in (will be made square)
     * @param image   The image to draw
     */
    void drawCircular(juce::Graphics& g,
                      juce::Rectangle<int> bounds,
                      const juce::Image& image);

    /**
     * Draw a circular avatar with fallback to initials.
     * If image is invalid, draws a colored circle with initials.
     *
     * @param g              Graphics context
     * @param bounds         Rectangle to draw in
     * @param image          The avatar image (can be null/invalid)
     * @param initials       Fallback initials (e.g., "JD" for John Doe)
     * @param backgroundColor Background color for initials fallback
     * @param textColor      Text color for initials
     * @param fontSize       Font size for initials (0 = auto-calculate from bounds)
     */
    void drawCircularAvatar(juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            const juce::Image& image,
                            const juce::String& initials,
                            juce::Colour backgroundColor = juce::Colour(0xff3a3a3e),
                            juce::Colour textColor = juce::Colours::white,
                            float fontSize = 0.0f);

    /**
     * Generate initials from a display name.
     * "John Doe" -> "JD", "alice" -> "A", "" -> "?"
     */
    juce::String getInitials(const juce::String& displayName, int maxChars = 2);

    /**
     * Draw a rounded rectangle image.
     *
     * @param g            Graphics context
     * @param bounds       Rectangle to draw in
     * @param image        The image to draw
     * @param cornerRadius Corner radius in pixels
     */
    void drawRounded(juce::Graphics& g,
                     juce::Rectangle<int> bounds,
                     const juce::Image& image,
                     float cornerRadius);

}  // namespace ImageUtils
