#pragma once

#include <JuceHeader.h>
#include <map>
#include <list>
#include <mutex>
#include <functional>

//==============================================================================
/**
 * ImageCache - Async image loading with LRU caching
 *
 * Features:
 * - Async loading from URLs (doesn't block UI thread)
 * - LRU eviction when cache is full
 * - Placeholder support while loading
 * - Automatic retry on failure
 * - Circular avatar drawing helper
 * - Thread-safe
 *
 * Usage:
 *   // Load image asynchronously
 *   ImageCache::load(url, [this](const juce::Image& img) {
 *       avatarImage = img;
 *       repaint();
 *   });
 *
 *   // Draw circular avatar with placeholder
 *   ImageCache::drawCircularAvatar(g, bounds, avatarImage, "JD");
 */
namespace ImageCache
{
    //==========================================================================
    // Types

    using ImageCallback = std::function<void(const juce::Image&)>;

    //==========================================================================
    // Core API

    /**
     * Load an image from URL asynchronously.
     * If cached, callback fires immediately on the message thread.
     * If not cached, downloads in background and calls back on message thread.
     *
     * @param url       The image URL to load
     * @param callback  Called with the image (or null image on failure)
     * @param width     Optional: resize to this width (0 = original size)
     * @param height    Optional: resize to this height (0 = original size)
     */
    void load(const juce::String& url,
              ImageCallback callback,
              int width = 0,
              int height = 0);

    /**
     * Load an image synchronously (blocks until loaded or failed).
     * Use sparingly - prefer async load() for UI code.
     *
     * @param url  The image URL to load
     * @return     The loaded image, or null image on failure
     */
    juce::Image loadSync(const juce::String& url);

    /**
     * Check if an image is already cached.
     */
    bool isCached(const juce::String& url);

    /**
     * Get a cached image if available (doesn't trigger download).
     * Returns null image if not cached.
     */
    juce::Image getCached(const juce::String& url);

    /**
     * Pre-load images into cache (useful for known upcoming images).
     */
    void preload(const juce::StringArray& urls);

    //==========================================================================
    // Cache Management

    /**
     * Set maximum number of images to cache (default: 100).
     * When exceeded, least recently used images are evicted.
     */
    void setMaxSize(size_t maxImages);

    /**
     * Get current cache size.
     */
    size_t getSize();

    /**
     * Clear all cached images.
     */
    void clear();

    /**
     * Remove a specific URL from cache.
     */
    void evict(const juce::String& url);

    //==========================================================================
    // Drawing Helpers

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
     */
    void drawCircularAvatar(juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            const juce::Image& image,
                            const juce::String& initials,
                            juce::Colour backgroundColor = juce::Colour(0xff3a3a3e),
                            juce::Colour textColor = juce::Colours::white);

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

    //==========================================================================
    // Statistics (for debugging)

    struct Stats
    {
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t downloadSuccesses = 0;
        size_t downloadFailures = 0;
        size_t evictions = 0;
    };

    Stats getStats();
    void resetStats();

}  // namespace ImageCache
