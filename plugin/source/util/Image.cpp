#include "Image.h"

namespace ImageUtils
{

void drawCircular(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::Image& image)
{
    if (!image.isValid())
        return;

    // Make bounds square (use smaller dimension)
    int size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto squareBounds = bounds.withSizeKeepingCentre(size, size);

    // Create circular clip path
    juce::Path clipPath;
    clipPath.addEllipse(squareBounds.toFloat());

    g.saveState();
    g.reduceClipRegion(clipPath);

    // Scale image to fit
    auto scaledImage = image.rescaled(size, size, juce::Graphics::highResamplingQuality);
    g.drawImageAt(scaledImage, squareBounds.getX(), squareBounds.getY());

    g.restoreState();
}

void drawCircularAvatar(juce::Graphics& g,
                        juce::Rectangle<int> bounds,
                        const juce::Image& image,
                        const juce::String& initials,
                        juce::Colour backgroundColor,
                        juce::Colour textColor,
                        float fontSize)
{
    // Make bounds square
    int size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto squareBounds = bounds.withSizeKeepingCentre(size, size);

    if (image.isValid())
    {
        drawCircular(g, squareBounds, image);
    }
    else
    {
        // Draw placeholder circle with initials
        g.setColour(backgroundColor);
        g.fillEllipse(squareBounds.toFloat());

        // Draw initials
        g.setColour(textColor);
        float actualFontSize = (fontSize > 0.0f) ? fontSize : (size * 0.4f);
        g.setFont(juce::Font(actualFontSize).boldened());

        juce::String displayInitials = initials.isEmpty() ? "?" : initials.toUpperCase();
        g.drawText(displayInitials, squareBounds, juce::Justification::centred);
    }
}

juce::String getInitials(const juce::String& displayName, int maxChars)
{
    if (displayName.isEmpty())
        return "?";

    juce::String initials;
    auto words = juce::StringArray::fromTokens(displayName.trim(), " ", "");

    for (const auto& word : words)
    {
        if (word.isNotEmpty() && initials.length() < maxChars)
        {
            initials += word.substring(0, 1).toUpperCase();
        }
    }

    return initials.isEmpty() ? displayName.substring(0, 1).toUpperCase() : initials;
}

void drawRounded(juce::Graphics& g,
                 juce::Rectangle<int> bounds,
                 const juce::Image& image,
                 float cornerRadius)
{
    if (!image.isValid())
        return;

    juce::Path clipPath;
    clipPath.addRoundedRectangle(bounds.toFloat(), cornerRadius);

    g.saveState();
    g.reduceClipRegion(clipPath);

    auto scaledImage = image.rescaled(bounds.getWidth(), bounds.getHeight(),
                                       juce::Graphics::highResamplingQuality);
    g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());

    g.restoreState();
}

}  // namespace ImageUtils
