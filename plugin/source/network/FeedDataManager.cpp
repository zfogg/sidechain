#include "FeedDataManager.h"
#include "../util/Log.h"
#include "../util/Result.h"

//==============================================================================
FeedDataManager::FeedDataManager()
{
    // Start timer to periodically check cache validity (every 60 seconds)
    startTimer(60000);
    Log::info("FeedDataManager: Initialized");
}

FeedDataManager::~FeedDataManager()
{
    Log::debug("FeedDataManager: Destroying");
    stopTimer();
}

//==============================================================================
void FeedDataManager::fetchFeed(FeedType feedType, int limit, int offset, FeedCallback callback)
{
    juce::String feedTypeStr = feedType == FeedType::Timeline ? "Timeline" : (feedType == FeedType::Global ? "Global" : "Trending");
    Log::debug("FeedDataManager: Fetching feed - type: " + feedTypeStr + ", limit: " + juce::String(limit) + ", offset: " + juce::String(offset));

    // Check cache first (only for offset 0, i.e., first page)
    if (offset == 0 && isCacheValid(feedType))
    {
        auto cached = getCachedFeed(feedType);
        if (!cached.posts.isEmpty())
        {
            Log::info("FeedDataManager: Using cached feed - type: " + feedTypeStr + ", posts: " + juce::String(cached.posts.size()));
            // Use cached data
            juce::MessageManager::callAsync([callback, cached]()
            {
                callback(cached);
            });
            return;
        }
    }

    performFetch(feedType, limit, offset, callback);
}

void FeedDataManager::fetchFeed(FeedType feedType, FeedCallback callback)
{
    fetchFeed(feedType, currentLimit, 0, callback);
}

//==============================================================================
void FeedDataManager::refreshFeed(RefreshCallback callback)
{
    pendingRefreshCallback = callback;

    // Clear cache for current feed type to force fresh fetch
    clearCache(currentFeedType);

    // Reset pagination
    currentOffset = 0;
    hasMore = true;
    loadedPosts[currentFeedType].clear();

    fetchFeed(currentFeedType, currentLimit, 0, [this, callback](const FeedResponse& response)
    {
        if (response.error.isNotEmpty())
        {
            if (callback)
                callback(false, response.error);
        }
        else
        {
            if (callback)
                callback(true, {});
        }
    });
}

//==============================================================================
void FeedDataManager::loadMorePosts(FeedCallback callback)
{
    if (!hasMore || fetchingInProgress)
    {
        // Nothing more to load or already loading
        FeedResponse emptyResponse;
        emptyResponse.hasMore = hasMore;
        callback(emptyResponse);
        return;
    }

    int nextOffset = currentOffset + currentLimit;
    fetchFeed(currentFeedType, currentLimit, nextOffset, callback);
}

//==============================================================================
void FeedDataManager::performFetch(FeedType feedType, int limit, int offset, FeedCallback callback)
{
    if (networkClient == nullptr)
    {
        Log::error("FeedDataManager: Cannot fetch - network client not configured");
        FeedResponse errorResponse;
        errorResponse.error = "Network client not configured";
        callback(errorResponse);
        return;
    }

    juce::String feedTypeStr = feedType == FeedType::Timeline ? "Timeline" : (feedType == FeedType::Global ? "Global" : "Trending");
    Log::info("FeedDataManager: Performing network fetch - type: " + feedTypeStr + ", limit: " + juce::String(limit) + ", offset: " + juce::String(offset));

    fetchingInProgress = true;
    pendingCallback = callback;

    // Use existing NetworkClient methods
    auto networkCallback = [this, feedType, limit, offset, callback](Outcome<juce::var> feedResult)
    {
        fetchingInProgress = false;

        if (feedResult.isOk())
        {
            handleFetchResponse(feedResult.getValue(), feedType, limit, offset, callback);
        }
        else
        {
            handleFetchError("Failed to fetch feed data: " + feedResult.getError(), callback);
        }
    };

    switch (feedType)
    {
        case FeedType::Timeline:
            networkClient->getTimelineFeed(limit, offset, networkCallback);
            break;
        case FeedType::Global:
            networkClient->getGlobalFeed(limit, offset, networkCallback);
            break;
        case FeedType::Trending:
            networkClient->getTrendingFeed(limit, offset, networkCallback);
            break;
    }
}

//==============================================================================
void FeedDataManager::handleFetchResponse(const juce::var& feedData, FeedType feedType,
                                          int limit, int offset, FeedCallback callback)
{
    FeedResponse feedResponse = parseJsonResponse(feedData);
    feedResponse.limit = limit;
    feedResponse.offset = offset;

    juce::String feedTypeStr = feedType == FeedType::Timeline ? "Timeline" : (feedType == FeedType::Global ? "Global" : "Trending");
    Log::info("FeedDataManager: Fetch response received - type: " + feedTypeStr + ", posts: " + juce::String(feedResponse.posts.size()) + ", hasMore: " + juce::String(feedResponse.hasMore ? "true" : "false"));

    // Update pagination state
    currentFeedType = feedType;
    currentOffset = offset;
    hasMore = feedResponse.hasMore;

    // Accumulate posts for infinite scroll
    if (offset == 0)
    {
        // First page - replace existing posts
        loadedPosts[feedType] = feedResponse.posts;
    }
    else
    {
        // Subsequent page - append to existing
        for (const auto& post : feedResponse.posts)
            loadedPosts[feedType].add(post);
    }

    // Update cache (only for first page)
    if (offset == 0)
    {
        updateCache(feedType, feedResponse, offset);
    }

    // Dispatch callback on message thread
    juce::MessageManager::callAsync([callback, feedResponse]()
    {
        callback(feedResponse);
    });
}

//==============================================================================
void FeedDataManager::handleFetchError(const juce::String& error, FeedCallback callback)
{
    Log::error("FeedDataManager: Fetch error - " + error);
    FeedResponse errorResponse;
    errorResponse.error = error;

    juce::MessageManager::callAsync([callback, errorResponse]()
    {
        callback(errorResponse);
    });
}

//==============================================================================
FeedResponse FeedDataManager::parseJsonResponse(const juce::var& json)
{
    FeedResponse response;

    if (json.isVoid())
    {
        response.error = "Invalid JSON response";
        return response;
    }

    // Parse activities array (from backend /api/feed/timeline or /api/feed/global)
    auto activities = json.getProperty("activities", juce::var());
    if (!activities.isArray())
    {
        // Try alternate format (direct array)
        if (json.isArray())
            activities = json;
        else
        {
            // If activities is null/missing, treat as empty feed (not an error)
            // This happens when the user has no posts or follows no one
            return response;  // Empty response with no error
        }
    }

    // Parse each activity into a FeedPost
    for (int i = 0; i < activities.size(); ++i)
    {
        auto post = FeedPost::fromJson(activities[i]);
        if (post.isValid())
            response.posts.add(post);
    }

    // Parse pagination info
    response.total = static_cast<int>(json.getProperty("total", 0));
    response.limit = static_cast<int>(json.getProperty("limit", 20));
    response.offset = static_cast<int>(json.getProperty("offset", 0));

    // Determine if there are more posts
    if (json.hasProperty("has_more"))
    {
        response.hasMore = static_cast<bool>(json.getProperty("has_more", false));
    }
    else
    {
        // Infer from total and current position
        response.hasMore = (response.offset + response.posts.size()) < response.total;
    }

    return response;
}

//==============================================================================
juce::String FeedDataManager::getEndpointForFeedType(FeedType feedType) const
{
    switch (feedType)
    {
        case FeedType::Timeline:
            return "/api/feed/timeline";
        case FeedType::Global:
            return "/api/feed/global";
        default:
            return "/api/feed/global";
    }
}

//==============================================================================
// Cache Management
//==============================================================================

void FeedDataManager::clearCache()
{
    cache.clear();
    loadedPosts.clear();

    // Delete cache files
    for (auto feedType : { FeedType::Timeline, FeedType::Global, FeedType::Trending })
    {
        auto cacheFile = getCacheFile(feedType);
        if (cacheFile.exists())
            cacheFile.deleteFile();
    }
}

void FeedDataManager::clearCache(FeedType feedType)
{
    cache.erase(feedType);
    loadedPosts.erase(feedType);

    auto cacheFile = getCacheFile(feedType);
    if (cacheFile.exists())
        cacheFile.deleteFile();
}

//==============================================================================
bool FeedDataManager::isCacheValid(FeedType feedType) const
{
    auto it = cache.find(feedType);
    if (it != cache.end())
        return it->second.isValid(cacheTTLSeconds);

    // Check disk cache
    auto cacheFile = getCacheFile(feedType);
    if (cacheFile.exists())
    {
        // Check file modification time as proxy for cache age
        auto fileTime = cacheFile.getLastModificationTime();
        auto age = juce::Time::getCurrentTime() - fileTime;
        return age.inSeconds() < cacheTTLSeconds;
    }

    return false;
}

//==============================================================================
FeedResponse FeedDataManager::getCachedFeed(FeedType feedType) const
{
    // Check memory cache first
    auto it = cache.find(feedType);
    if (it != cache.end() && it->second.isValid(cacheTTLSeconds))
        return it->second.response;

    // Try loading from disk
    const_cast<FeedDataManager*>(this)->loadCacheFromDisk(feedType);

    // Check again after loading
    it = cache.find(feedType);
    if (it != cache.end() && it->second.isValid(cacheTTLSeconds))
        return it->second.response;

    return FeedResponse{};
}

//==============================================================================
void FeedDataManager::updateCache(FeedType feedType, const FeedResponse& response, int offset)
{
    CacheEntry entry;
    entry.response = response;
    entry.timestamp = juce::Time::getCurrentTime();
    entry.feedType = feedType;
    entry.offset = offset;

    cache[feedType] = entry;

    // Persist to disk
    saveCacheToDisk(feedType, entry);
}

//==============================================================================
juce::File FeedDataManager::getCacheFile(FeedType feedType) const
{
    auto cacheDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("Sidechain")
                        .getChildFile("cache");

    if (!cacheDir.exists())
        cacheDir.createDirectory();

    juce::String filename;
    switch (feedType)
    {
        case FeedType::Timeline: filename = "feed_timeline.json"; break;
        case FeedType::Global:   filename = "feed_global.json"; break;
        case FeedType::Trending: filename = "feed_trending.json"; break;
        default:                 filename = "feed_unknown.json"; break;
    }

    return cacheDir.getChildFile(filename);
}

//==============================================================================
void FeedDataManager::loadCacheFromDisk(FeedType feedType)
{
    auto cacheFile = getCacheFile(feedType);
    if (!cacheFile.exists())
        return;

    auto jsonStr = cacheFile.loadFileAsString();
    if (jsonStr.isEmpty())
        return;

    auto json = juce::JSON::parse(jsonStr);
    if (json.isVoid())
        return;

    CacheEntry entry;
    entry.feedType = feedType;
    entry.offset = 0;

    // Parse timestamp
    auto timestampStr = json.getProperty("cache_timestamp", "").toString();
    if (timestampStr.isNotEmpty())
        entry.timestamp = juce::Time::fromISO8601(timestampStr);
    else
        entry.timestamp = cacheFile.getLastModificationTime();

    // Parse posts
    auto postsVar = json.getProperty("posts", juce::var());
    if (postsVar.isArray())
    {
        for (int i = 0; i < postsVar.size(); ++i)
        {
            auto post = FeedPost::fromJson(postsVar[i]);
            if (post.isValid())
                entry.response.posts.add(post);
        }
    }

    entry.response.limit = static_cast<int>(json.getProperty("limit", 20));
    entry.response.offset = static_cast<int>(json.getProperty("offset", 0));
    entry.response.total = static_cast<int>(json.getProperty("total", 0));
    entry.response.hasMore = static_cast<bool>(json.getProperty("has_more", false));

    if (entry.isValid(cacheTTLSeconds))
        cache[feedType] = entry;
}

//==============================================================================
void FeedDataManager::saveCacheToDisk(FeedType feedType, const CacheEntry& entry)
{
    auto obj = new juce::DynamicObject();

    // Add cache metadata
    obj->setProperty("cache_timestamp", entry.timestamp.toISO8601(true));
    juce::String feedTypeStr = (feedType == FeedType::Timeline) ? "timeline" :
                               (feedType == FeedType::Global) ? "global" : "trending";
    obj->setProperty("feed_type", feedTypeStr);

    // Serialize posts
    juce::Array<juce::var> postsArray;
    for (const auto& post : entry.response.posts)
        postsArray.add(post.toJson());
    obj->setProperty("posts", postsArray);

    // Pagination info
    obj->setProperty("limit", entry.response.limit);
    obj->setProperty("offset", entry.response.offset);
    obj->setProperty("total", entry.response.total);
    obj->setProperty("has_more", entry.response.hasMore);

    // Write to file
    auto cacheFile = getCacheFile(feedType);
    auto jsonStr = juce::JSON::toString(juce::var(obj), true);
    cacheFile.replaceWithText(jsonStr);
}

//==============================================================================
void FeedDataManager::timerCallback()
{
    // Periodically clean up expired cache entries
    for (auto it = cache.begin(); it != cache.end();)
    {
        if (!it->second.isValid(cacheTTLSeconds))
            it = cache.erase(it);
        else
            ++it;
    }
}

//==============================================================================
int FeedDataManager::getLoadedPostsCount() const
{
    auto it = loadedPosts.find(currentFeedType);
    if (it != loadedPosts.end())
        return it->second.size();
    return 0;
}
