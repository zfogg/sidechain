#include "ImageCache.h"
#include "Async.h"
#include "Constants.h"
#include "Log.h"

namespace ImageLoader
{

//==============================================================================
// Internal implementation
namespace
{
    // LRU Cache entry
    struct CacheEntry
    {
        juce::Image image;
        juce::String url;
    };

    // Cache state
    std::mutex cacheMutex;
    std::map<juce::String, std::list<CacheEntry>::iterator> cacheMap;
    std::list<CacheEntry> cacheList;  // Front = most recently used
    size_t maxCacheSize = Constants::Cache::IMAGE_CACHE_MAX_ITEMS;

    // Pending downloads (URL -> list of callbacks waiting)
    std::mutex pendingMutex;
    std::map<juce::String, std::vector<ImageCallback>> pendingDownloads;

    // Statistics
    Stats stats;

    //==========================================================================
    // Move entry to front of LRU list (must hold cacheMutex)
    void touchEntry(const juce::String& url)
    {
        auto it = cacheMap.find(url);
        if (it != cacheMap.end())
        {
            // Move to front
            cacheList.splice(cacheList.begin(), cacheList, it->second);
        }
    }

    //==========================================================================
    // Evict least recently used entries until under max size (must hold cacheMutex)
    void evictIfNeeded()
    {
        while (cacheList.size() > maxCacheSize && !cacheList.empty())
        {
            auto& oldest = cacheList.back();
            cacheMap.erase(oldest.url);
            cacheList.pop_back();
            stats.evictions++;
        }
    }

    //==========================================================================
    // Add image to cache (must hold cacheMutex)
    void addToCache(const juce::String& url, const juce::Image& image)
    {
        // Check if already cached
        auto existing = cacheMap.find(url);
        if (existing != cacheMap.end())
        {
            // Update existing entry
            existing->second->image = image;
            touchEntry(url);
            return;
        }

        // Add new entry at front
        cacheList.push_front({image, url});
        cacheMap[url] = cacheList.begin();

        // Evict if over capacity
        evictIfNeeded();
    }

    //==========================================================================
    // Download image in background using Async utility
    void downloadImage(const juce::String& url, int targetWidth, int targetHeight)
    {
        // Use Async::run to download image on background thread and deliver to message thread
        Async::run<juce::Image>(
            // Background work: download and decode image
            [url, targetWidth, targetHeight]() -> juce::Image {
                juce::Image loadedImage;

                try
                {
                    juce::URL imageUrl(url);

                    // Create input stream with timeout
                    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs(Constants::Api::IMAGE_TIMEOUT_MS)
                        .withResponseHeaders(nullptr);

                    auto stream = imageUrl.createInputStream(options);

                    if (stream != nullptr)
                    {
                        // Read image data
                        juce::MemoryBlock data;
                        stream->readIntoMemoryBlock(data);

                        if (data.getSize() > 0)
                        {
                            // Decode image
                            loadedImage = juce::ImageFileFormat::loadFrom(data.getData(), data.getSize());

                            // Resize if requested
                            if (loadedImage.isValid() && (targetWidth > 0 || targetHeight > 0))
                            {
                                int newWidth = targetWidth > 0 ? targetWidth : loadedImage.getWidth();
                                int newHeight = targetHeight > 0 ? targetHeight : loadedImage.getHeight();

                                if (newWidth != loadedImage.getWidth() || newHeight != loadedImage.getHeight())
                                {
                                    loadedImage = loadedImage.rescaled(newWidth, newHeight,
                                        juce::Graphics::highResamplingQuality);
                                }
                            }
                        }
                    }
                }
                catch (...)
                {
                    Log::warn("ImageCache: Exception loading image from " + url);
                }

                // Update stats (mutex protected)
                {
                    std::lock_guard<std::mutex> lock(cacheMutex);
                    if (loadedImage.isValid())
                    {
                        stats.downloadSuccesses++;
                        addToCache(url, loadedImage);
                    }
                    else
                    {
                        stats.downloadFailures++;
                        Log::warn("ImageCache: Failed to load image from " + url);
                    }
                }

                return loadedImage;
            },
            // Callback on message thread: notify all waiting callbacks
            [url](const juce::Image& loadedImage) {
                std::vector<ImageCallback> callbacks;

                {
                    std::lock_guard<std::mutex> lock(pendingMutex);
                    auto it = pendingDownloads.find(url);
                    if (it != pendingDownloads.end())
                    {
                        callbacks = std::move(it->second);
                        pendingDownloads.erase(it);
                    }
                }

                for (auto& callback : callbacks)
                {
                    if (callback)
                        callback(loadedImage);
                }
            }
        );
    }

}  // anonymous namespace

//==============================================================================
// Public API

void load(const juce::String& url, ImageCallback callback, int width, int height)
{
    if (url.isEmpty())
    {
        if (callback)
            callback(juce::Image());
        return;
    }

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        auto it = cacheMap.find(url);
        if (it != cacheMap.end())
        {
            stats.cacheHits++;
            touchEntry(url);
            juce::Image cachedImage = it->second->image;

            // Call callback async to maintain consistent behavior
            juce::MessageManager::callAsync([callback, cachedImage]()
            {
                if (callback)
                    callback(cachedImage);
            });
            return;
        }

        stats.cacheMisses++;
    }

    // Add callback to pending list
    bool shouldStartDownload = false;
    {
        std::lock_guard<std::mutex> lock(pendingMutex);

        auto& callbacks = pendingDownloads[url];
        shouldStartDownload = callbacks.empty();  // First request for this URL
        callbacks.push_back(callback);
    }

    // Start download if this is the first request for this URL
    if (shouldStartDownload)
    {
        downloadImage(url, width, height);
    }
}

juce::Image loadSync(const juce::String& url)
{
    if (url.isEmpty())
        return {};

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        auto it = cacheMap.find(url);
        if (it != cacheMap.end())
        {
            stats.cacheHits++;
            touchEntry(url);
            return it->second->image;
        }

        stats.cacheMisses++;
    }

    // Download synchronously
    juce::Image loadedImage;

    try
    {
        juce::URL imageUrl(url);
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(Constants::Api::IMAGE_TIMEOUT_MS);

        auto stream = imageUrl.createInputStream(options);

        if (stream != nullptr)
        {
            juce::MemoryBlock data;
            stream->readIntoMemoryBlock(data);

            if (data.getSize() > 0)
            {
                loadedImage = juce::ImageFileFormat::loadFrom(data.getData(), data.getSize());
            }
        }
    }
    catch (...)
    {
        Log::warn("ImageCache: Exception in sync load from " + url);
    }

    // Add to cache
    {
        std::lock_guard<std::mutex> lock(cacheMutex);

        if (loadedImage.isValid())
        {
            stats.downloadSuccesses++;
            addToCache(url, loadedImage);
        }
        else
        {
            stats.downloadFailures++;
        }
    }

    return loadedImage;
}

bool isCached(const juce::String& url)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return cacheMap.find(url) != cacheMap.end();
}

juce::Image getCached(const juce::String& url)
{
    std::lock_guard<std::mutex> lock(cacheMutex);

    auto it = cacheMap.find(url);
    if (it != cacheMap.end())
    {
        touchEntry(url);
        return it->second->image;
    }

    return {};
}

void preload(const juce::StringArray& urls)
{
    for (const auto& url : urls)
    {
        if (!isCached(url))
        {
            load(url, nullptr);  // Fire and forget
        }
    }
}

void setMaxSize(size_t maxImages)
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    maxCacheSize = maxImages;
    evictIfNeeded();
}

size_t getSize()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return cacheList.size();
}

void clear()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    cacheMap.clear();
    cacheList.clear();
}

void evict(const juce::String& url)
{
    std::lock_guard<std::mutex> lock(cacheMutex);

    auto it = cacheMap.find(url);
    if (it != cacheMap.end())
    {
        cacheList.erase(it->second);
        cacheMap.erase(it);
    }
}

//==============================================================================
// Statistics

Stats getStats()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return stats;
}

void resetStats()
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    stats = Stats();
}

//==============================================================================
// Drawing Helpers

juce::String getInitials(const juce::String& name)
{
    if (name.isEmpty())
        return "?";

    juce::String initials;
    auto words = juce::StringArray::fromTokens(name, " ", "");

    for (const auto& word : words)
    {
        if (word.isNotEmpty() && initials.length() < 2)
            initials += word.substring(0, 1).toUpperCase();
    }

    if (initials.isEmpty())
        initials = name.substring(0, 1).toUpperCase();

    return initials;
}

void drawCircularAvatar(juce::Graphics& g,
                        juce::Rectangle<int> bounds,
                        const juce::Image& image,
                        const juce::String& initials,
                        juce::Colour bgColor,
                        juce::Colour textColor,
                        float fontSize)
{
    // Create circular clipping path
    juce::Path circlePath;
    circlePath.addEllipse(bounds.toFloat());

    g.saveState();
    g.reduceClipRegion(circlePath);

    if (image.isValid())
    {
        // Draw the image scaled to fit
        auto scaledImage = image.rescaled(bounds.getWidth(), bounds.getHeight(),
                                          juce::Graphics::highResamplingQuality);
        g.drawImageAt(scaledImage, bounds.getX(), bounds.getY());
    }
    else
    {
        // Draw placeholder with initials
        g.setColour(bgColor);
        g.fillEllipse(bounds.toFloat());

        g.setColour(textColor);
        g.setFont(fontSize);
        g.drawText(initials, bounds, juce::Justification::centred);
    }

    g.restoreState();
}

}  // namespace ImageLoader
