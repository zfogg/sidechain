#pragma once

#include <JuceHeader.h>
#include <map>
#include <list>
#include <mutex>
#include <functional>

class NetworkClient;

//==============================================================================
/**
 * ImageLoader - Async image loading with LRU caching
 *
 * Named ImageLoader instead of ImageCache to avoid conflict with juce::ImageCache.
 *
 * Features:
 * - Async loading from URLs (doesn't block UI thread)
 * - LRU eviction when cache is full
 * - Placeholder support while loading
 * - Automatic retry on failure
 * - Thread-safe
 * - Drawing helpers for avatars
 *
 * Usage:
 *   // Load image asynchronously
 *   ImageLoader::load(url, [this](const juce::Image& img) {
 *       avatarImage = img;
 *       repaint();
 *   });
 */
namespace ImageLoader
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

    /**
     * Set NetworkClient for HTTP requests (optional - falls back to JUCE URL if not set).
     */
    void setNetworkClient(NetworkClient* client);

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

    //==========================================================================
    // Drawing Helpers

    /**
     * Get initials from a name (e.g., "John Doe" -> "JD", "alice" -> "A")
     */
    juce::String getInitials(const juce::String& name);

    /**
     * Draw a circular avatar with image or initials fallback.
     *
     * @param g             Graphics context
     * @param bounds        Rectangle to draw in
     * @param image         The avatar image (can be null)
     * @param initials      Initials to show if image is null
     * @param bgColor       Background color for initials placeholder
     * @param textColor     Text color for initials
     * @param fontSize      Font size for initials (default 14.0f)
     */
    void drawCircularAvatar(juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            const juce::Image& image,
                            const juce::String& initials,
                            juce::Colour bgColor,
                            juce::Colour textColor,
                            float fontSize = 14.0f);

}  // namespace ImageLoader
