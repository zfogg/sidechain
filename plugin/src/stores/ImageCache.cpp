#include "ImageCache.h"
#include "../util/Async.h"
#include "../util/Constants.h"
#include "../util/Log.h"
#include "../network/NetworkClient.h"
#include "../util/profiling/PerformanceMonitor.h"
#include <set>

namespace ImageLoader
{

//==============================================================================
// Internal implementation
namespace
{
    /**
     * CacheEntry - LRU cache entry for image storage
     *
     * Stores a cached image along with its source URL for identification.
     * Used in the LRU (Least Recently Used) cache implementation.
     */
    struct CacheEntry
    {
        juce::Image image;  ///< The cached image
        juce::String url;   ///< Source URL for this image
    };

    // Cache state
    std::mutex cacheMutex;
    std::map<juce::String, std::list<CacheEntry>::iterator> cacheMap;
    std::list<CacheEntry> cacheList;  // Front = most recently used
    size_t maxCacheSize = Constants::Cache::IMAGE_CACHE_MAX_ITEMS;

    // Pending downloads (URL -> list of callbacks waiting)
    std::mutex pendingMutex;
    std::map<juce::String, std::vector<ImageCallback>> pendingDownloads;

    // Failed URLs cache (to avoid spamming logs for the same URL)
    std::set<juce::String> failedUrls;

    // NetworkClient for HTTP requests (optional)
    NetworkClient* networkClient = nullptr;

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
                SCOPED_TIMER_THRESHOLD("cache::image_download", 3000.0);
                juce::Image loadedImage;

                try
                {
                    juce::MemoryBlock data;
                    bool success = false;

                    // Use NetworkClient if available, otherwise fall back to JUCE URL
                    if (networkClient != nullptr)
                    {
                        juce::MemoryBlock binaryData;
                        auto result = networkClient->makeAbsoluteRequestSync(url, "GET", juce::var(), false, juce::StringPairArray(), &binaryData);
                        if (result.success && binaryData.getSize() > 0)
                        {
                            data = binaryData;
                            success = true;
                        }
                    }
                    else
                    {
                        // Fallback to JUCE URL
                        juce::URL imageUrl(url);
                        int statusCode = 0;
                        juce::StringPairArray responseHeaders;
                        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs(Constants::Api::IMAGE_TIMEOUT_MS)
                            .withStatusCode(&statusCode)
                            .withResponseHeaders(&responseHeaders);

                        auto stream = imageUrl.createInputStream(options);
                        if (stream != nullptr)
                        {
                            stream->readIntoMemoryBlock(data);
                            success = data.getSize() > 0;
                        }
                    }

                    if (!success || data.getSize() == 0)
                    {
                        // Only log if we haven't already logged this URL
                        bool alreadyFailed = false;
                        {
                            std::lock_guard<std::mutex> lock(cacheMutex);
                            alreadyFailed = failedUrls.count(url) > 0;
                            if (!alreadyFailed)
                                failedUrls.insert(url);
                        }
                        if (!alreadyFailed)
                            Log::warn("ImageCache: Failed to load image: " + url);
                    }
                    else
                    {
                        // Decode image
                        loadedImage = juce::ImageFileFormat::loadFrom(data.getData(), data.getSize());

                        if (!loadedImage.isValid())
                        {
                            // Only log if we haven't already logged this URL
                            bool alreadyFailed = false;
                            {
                                std::lock_guard<std::mutex> lock(cacheMutex);
                                alreadyFailed = failedUrls.count(url) > 0;
                                if (!alreadyFailed)
                                    failedUrls.insert(url);
                            }
                            if (!alreadyFailed)
                                Log::warn("ImageCache: Failed to decode " + juce::String(data.getSize()) + " bytes: " + url);
                        }
                        else
                        {
                            // Resize if requested
                            if (targetWidth > 0 || targetHeight > 0)
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
                catch (const std::exception& e)
                {
                    Log::warn("ImageCache: Exception loading image from " + url + ": " + juce::String(e.what()));
                }
                catch (...)
                {
                    Log::warn("ImageCache: Unknown exception loading image from " + url);
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

void load(const juce::String& url, ImageLoader::ImageCallback callback, int width, int height)
{
    SCOPED_TIMER("cache::image_load");
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
    failedUrls.clear();  // Also clear failed URLs so they can be retried
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
    failedUrls.erase(url);  // Also allow retry
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

//==============================================================================
void setNetworkClient(NetworkClient* client)
{
    networkClient = client;
}

//==============================================================================
// Avatar loading - fetches URL from backend, then downloads image directly
// The backend returns JSON with the actual image URL, which we then download via HTTPS

void loadAvatarForUser(const juce::String& userId, ImageCallback callback, int width, int height)
{
    if (userId.isEmpty())
    {
        if (callback)
            callback(juce::Image());
        return;
    }

    // First, check if we have a cached image for this user (keyed by userId)
    juce::String cacheKey = "avatar:" + userId;
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cacheMap.find(cacheKey);
        if (it != cacheMap.end())
        {
            stats.cacheHits++;
            touchEntry(cacheKey);
            juce::Image cachedImage = it->second->image;
            juce::MessageManager::callAsync([callback, cachedImage]()
            {
                if (callback)
                    callback(cachedImage);
            });
            return;
        }
    }

    // Construct URL endpoint: /api/v1/users/{userId}/profile-picture
    // This returns JSON: {"url": "...", "user_id": "..."}
    juce::String apiUrl = juce::String(Constants::Endpoints::DEV_BASE_URL)
        + Constants::Endpoints::API_VERSION
        + "/users/" + userId + "/profile-picture";

    Log::debug("ImageLoader: Fetching avatar URL for user " + userId);

    // Fetch the JSON to get the actual image URL, then download the image
    Async::runVoid([apiUrl, userId, cacheKey, callback, width, height]()
    {
        juce::String imageUrl;

        // Step 1: Fetch JSON from backend to get the actual image URL
        if (networkClient != nullptr)
        {
            auto result = networkClient->makeAbsoluteRequestSync(apiUrl, "GET");
            if (result.success && result.data.isObject())
            {
                imageUrl = result.data.getProperty("url", "").toString();
            }
            else
            {
                Log::debug("ImageLoader: Failed to get avatar URL for user " + userId + ": " + result.errorMessage);
            }
        }
        else
        {
            // Fallback: use JUCE URL
            juce::URL url(apiUrl);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(Constants::Api::DEFAULT_TIMEOUT_MS);
            if (auto stream = url.createInputStream(options))
            {
                juce::String jsonStr = stream->readEntireStreamAsString();
                auto json = juce::JSON::parse(jsonStr);
                if (json.isObject())
                {
                    imageUrl = json.getProperty("url", "").toString();
                }
            }
        }

        if (imageUrl.isEmpty())
        {
            juce::MessageManager::callAsync([callback]()
            {
                if (callback)
                    callback(juce::Image());
            });
            return;
        }

        Log::debug("ImageLoader: Downloading avatar from " + imageUrl);

        // Step 2: Download the actual image from the URL
        juce::Image loadedImage;
        juce::MemoryBlock data;
        bool success = false;

        if (networkClient != nullptr)
        {
            auto result = networkClient->makeAbsoluteRequestSync(imageUrl, "GET", juce::var(), false, juce::StringPairArray(), &data);
            success = result.success && data.getSize() > 0;
        }
        else
        {
            juce::URL imgUrl(imageUrl);
            auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(Constants::Api::IMAGE_TIMEOUT_MS)
                .withNumRedirectsToFollow(Constants::Api::MAX_REDIRECTS);
            if (auto stream = imgUrl.createInputStream(options))
            {
                stream->readIntoMemoryBlock(data);
                success = data.getSize() > 0;
            }
        }

        if (success && data.getSize() > 0)
        {
            loadedImage = juce::ImageFileFormat::loadFrom(data.getData(), data.getSize());

            if (loadedImage.isValid())
            {
                // Resize if requested
                if (width > 0 || height > 0)
                {
                    int newWidth = width > 0 ? width : loadedImage.getWidth();
                    int newHeight = height > 0 ? height : loadedImage.getHeight();
                    if (newWidth != loadedImage.getWidth() || newHeight != loadedImage.getHeight())
                    {
                        loadedImage = loadedImage.rescaled(newWidth, newHeight,
                            juce::Graphics::highResamplingQuality);
                    }
                }

                // Add to cache with userId-based key
                {
                    std::lock_guard<std::mutex> lock(cacheMutex);
                    stats.downloadSuccesses++;
                    addToCache(cacheKey, loadedImage);
                }

                Log::debug("ImageLoader: Avatar loaded for user " + userId + " (" +
                          juce::String(loadedImage.getWidth()) + "x" +
                          juce::String(loadedImage.getHeight()) + ")");
            }
            else
            {
                Log::warn("ImageLoader: Failed to decode avatar for user " + userId);
                std::lock_guard<std::mutex> lock(cacheMutex);
                stats.downloadFailures++;
            }
        }
        else
        {
            Log::warn("ImageLoader: Failed to download avatar for user " + userId);
            std::lock_guard<std::mutex> lock(cacheMutex);
            stats.downloadFailures++;
        }

        // Deliver result on message thread
        juce::MessageManager::callAsync([callback, loadedImage]()
        {
            if (callback)
                callback(loadedImage);
        });
    });
}

}  // namespace ImageLoader
