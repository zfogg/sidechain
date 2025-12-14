#include "FeedStore.h"
#include "../util/Time.h"

namespace Sidechain {
namespace Stores {

FeedStore::FeedStore()
{
    // Start timer for periodic cache cleanup (every 60 seconds)
    startTimer(60000);
    Util::logInfo("FeedStore", "Initialized reactive feed store");
}

FeedStore::~FeedStore()
{
    stopTimer();
}

//==============================================================================
// Feed Loading

void FeedStore::loadFeed(FeedType feedType, bool forceRefresh)
{
    Util::logInfo("FeedStore", "Loading feed: " + feedTypeToString(feedType),
                  "forceRefresh=" + juce::String(forceRefresh ? "true" : "false"));

    // Update state to loading
    updateState([feedType](FeedStoreState& state)
    {
        state.currentFeedType = feedType;
        auto& feed = state.feeds[feedType];
        feed.isLoading = true;
        feed.error = "";
    });

    // Check cache if not forcing refresh
    if (!forceRefresh)
    {
        auto it = diskCache.find(feedType);
        if (it != diskCache.end() && it->second.isValid(cacheTTLSeconds))
        {
            Util::logInfo("FeedStore", "Using cached feed: " + feedTypeToString(feedType),
                          "posts=" + juce::String(it->second.response.posts.size()));

            // Use cached data
            updateState([feedType, cached = it->second.response](FeedStoreState& state)
            {
                auto& feed = state.feeds[feedType];
                feed.posts = cached.posts;
                feed.isLoading = false;
                feed.hasMore = cached.hasMore;
                feed.offset = 0;
                feed.total = cached.total;
                feed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
            });
            return;
        }

        // Try loading from disk
        loadCacheFromDisk(feedType);
        auto diskIt = diskCache.find(feedType);
        if (diskIt != diskCache.end() && diskIt->second.isValid(cacheTTLSeconds))
        {
            Util::logInfo("FeedStore", "Using disk cached feed: " + feedTypeToString(feedType),
                          "posts=" + juce::String(diskIt->second.response.posts.size()));

            updateState([feedType, cached = diskIt->second.response](FeedStoreState& state)
            {
                auto& feed = state.feeds[feedType];
                feed.posts = cached.posts;
                feed.isLoading = false;
                feed.hasMore = cached.hasMore;
                feed.offset = 0;
                feed.total = cached.total;
                feed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
            });
            return;
        }
    }

    // Perform network fetch
    performFetch(feedType, 20, 0);
}

void FeedStore::refreshCurrentFeed()
{
    auto currentType = getCurrentFeedType();

    Util::logInfo("FeedStore", "Refreshing current feed: " + feedTypeToString(currentType));

    // Clear cache
    clearCache(currentType);

    // Update state to refreshing
    updateState([currentType](FeedStoreState& state)
    {
        auto& feed = state.feeds[currentType];
        feed.isRefreshing = true;
        feed.error = "";
    });

    // Fetch from network
    performFetch(currentType, 20, 0);
}

void FeedStore::loadMore()
{
    auto currentType = getCurrentFeedType();
    auto currentState = getState();
    const auto& currentFeed = currentState.getCurrentFeed();

    if (!currentFeed.hasMore || currentFeed.isLoading)
    {
        Util::logDebug("FeedStore", "Load more skipped",
                       "hasMore=" + juce::String(currentFeed.hasMore ? "true" : "false") +
                       " isLoading=" + juce::String(currentFeed.isLoading ? "true" : "false"));
        return;
    }

    Util::logInfo("FeedStore", "Loading more posts: " + feedTypeToString(currentType),
                  "offset=" + juce::String(currentFeed.offset + currentFeed.limit));

    // Update loading state
    updateState([currentType](FeedStoreState& state)
    {
        state.feeds[currentType].isLoading = true;
    });

    // Fetch next page
    performFetch(currentType, currentFeed.limit, currentFeed.offset + currentFeed.limit);
}

void FeedStore::switchFeedType(FeedType feedType)
{
    if (getCurrentFeedType() == feedType)
        return;

    Util::logInfo("FeedStore", "Switching feed type to: " + feedTypeToString(feedType));

    updateState([feedType](FeedStoreState& state)
    {
        state.currentFeedType = feedType;
    });

    // Load feed if not already loaded
    auto currentState = getState();
    const auto& feed = currentState.feeds.find(feedType);
    if (feed == currentState.feeds.end() || feed->second.posts.isEmpty())
    {
        loadFeed(feedType);
    }
}

//==============================================================================
// Post Interactions

void FeedStore::toggleLike(const juce::String& postId)
{
    Util::logDebug("FeedStore", "Toggle like", "postId=" + postId);

    // Optimistic update
    optimisticUpdate(
        [postId](FeedStoreState& state)
        {
            // Find and update post in current feed
            auto& currentFeed = state.getCurrentFeedMutable();
            for (auto& post : currentFeed.posts)
            {
                if (post.id == postId)
                {
                    post.isLiked = !post.isLiked;
                    post.likeCount += post.isLiked ? 1 : -1;
                    break;
                }
            }
        },
        [this, postId](auto callback)
        {
            if (!networkClient)
            {
                callback(false, "Network client not configured");
                return;
            }

            // Find the post to get current state
            const auto* post = findPost(postId);
            if (!post)
            {
                callback(false, "Post not found");
                return;
            }

            bool shouldLike = post->isLiked;

            networkClient->toggleLike(postId, shouldLike, [callback](Outcome<juce::var> result)
            {
                if (result.isOk())
                {
                    callback(true, "");
                }
                else
                {
                    callback(false, result.getError());
                }
            });
        },
        [postId](const juce::String& error)
        {
            Util::logError("FeedStore", "Failed to toggle like: " + error, "postId=" + postId);
        }
    );
}

void FeedStore::toggleSave(const juce::String& postId)
{
    Util::logDebug("FeedStore", "Toggle save", "postId=" + postId);

    optimisticUpdate(
        [postId](FeedStoreState& state)
        {
            auto& currentFeed = state.getCurrentFeedMutable();
            for (auto& post : currentFeed.posts)
            {
                if (post.id == postId)
                {
                    post.isSaved = !post.isSaved;
                    post.saveCount += post.isSaved ? 1 : -1;
                    break;
                }
            }
        },
        [this, postId](auto callback)
        {
            if (!networkClient)
            {
                callback(false, "Network client not configured");
                return;
            }

            const auto* post = findPost(postId);
            if (!post)
            {
                callback(false, "Post not found");
                return;
            }

            bool shouldSave = post->isSaved;

            networkClient->toggleSave(postId, shouldSave, [callback](Outcome<juce::var> result)
            {
                callback(result.isOk(), result.isOk() ? "" : result.getError());
            });
        },
        [postId](const juce::String& error)
        {
            Util::logError("FeedStore", "Failed to toggle save: " + error, "postId=" + postId);
        }
    );
}

void FeedStore::toggleRepost(const juce::String& postId)
{
    Util::logDebug("FeedStore", "Toggle repost", "postId=" + postId);

    optimisticUpdate(
        [postId](FeedStoreState& state)
        {
            auto& currentFeed = state.getCurrentFeedMutable();
            for (auto& post : currentFeed.posts)
            {
                if (post.id == postId)
                {
                    post.isReposted = !post.isReposted;
                    post.repostCount += post.isReposted ? 1 : -1;
                    break;
                }
            }
        },
        [this, postId](auto callback)
        {
            if (!networkClient)
            {
                callback(false, "Network client not configured");
                return;
            }

            const auto* post = findPost(postId);
            if (!post)
            {
                callback(false, "Post not found");
                return;
            }

            bool shouldRepost = post->isReposted;

            networkClient->toggleRepost(postId, shouldRepost, [callback](Outcome<juce::var> result)
            {
                callback(result.isOk(), result.isOk() ? "" : result.getError());
            });
        },
        [postId](const juce::String& error)
        {
            Util::logError("FeedStore", "Failed to toggle repost: " + error, "postId=" + postId);
        }
    );
}

void FeedStore::addReaction(const juce::String& postId, const juce::String& emoji)
{
    Util::logDebug("FeedStore", "Add reaction", "postId=" + postId + " emoji=" + emoji);

    optimisticUpdate(
        [postId, emoji](FeedStoreState& state)
        {
            auto& currentFeed = state.getCurrentFeedMutable();
            for (auto& post : currentFeed.posts)
            {
                if (post.id == postId)
                {
                    // Remove previous reaction if exists
                    if (!post.userReaction.isEmpty())
                    {
                        auto it = post.reactionCounts.find(post.userReaction.toStdString());
                        if (it != post.reactionCounts.end() && it->second > 0)
                            it->second--;
                    }

                    // Add new reaction
                    if (!emoji.isEmpty())
                    {
                        post.userReaction = emoji;
                        post.reactionCounts[emoji.toStdString()]++;
                    }
                    else
                    {
                        post.userReaction = "";
                    }
                    break;
                }
            }
        },
        [this, postId, emoji](auto callback)
        {
            if (!networkClient)
            {
                callback(false, "Network client not configured");
                return;
            }

            networkClient->addEmojiReaction(postId, emoji, [callback](Outcome<juce::var> result)
            {
                callback(result.isOk(), result.isOk() ? "" : result.getError());
            });
        },
        [postId, emoji](const juce::String& error)
        {
            Util::logError("FeedStore", "Failed to add reaction: " + error,
                           "postId=" + postId + " emoji=" + emoji);
        }
    );
}

void FeedStore::updatePlayCount(const juce::String& postId, int newCount)
{
    updatePostInAllFeeds(postId, [newCount](FeedPost& post)
    {
        post.playCount = newCount;
    });
}

//==============================================================================
// Real-time updates

void FeedStore::handleNewPostNotification(const juce::var& postData)
{
    auto newPost = FeedPost::fromJson(postData);
    if (!newPost.isValid())
    {
        Util::logWarning("FeedStore", "Received invalid post notification");
        return;
    }

    Util::logInfo("FeedStore", "New post notification", "postId=" + newPost.id);

    // Add to timeline feed (prepend)
    updateState([newPost](FeedStoreState& state)
    {
        auto& timelineFeed = state.feeds[FeedType::Timeline];
        timelineFeed.posts.insert(0, newPost);
        timelineFeed.total++;
    });
}

void FeedStore::handleLikeCountUpdate(const juce::String& postId, int likeCount)
{
    Util::logDebug("FeedStore", "Like count update", "postId=" + postId + " count=" + juce::String(likeCount));

    updatePostInAllFeeds(postId, [likeCount](FeedPost& post)
    {
        post.likeCount = likeCount;
    });
}

void FeedStore::updateUserPresence(const juce::String& userId, bool isOnline, const juce::String& status)
{
    Util::logDebug("FeedStore", "User presence update",
                   "userId=" + userId + " online=" + juce::String(isOnline ? "true" : "false"));

    updateState([userId, isOnline, status](FeedStoreState& state)
    {
        for (auto& [feedType, feed] : state.feeds)
        {
            for (auto& post : feed.posts)
            {
                if (post.userId == userId)
                {
                    post.isOnline = isOnline;
                    if (status == "in_studio")
                        post.isInStudio = true;
                    else
                        post.isInStudio = false;
                }
            }
        }
    });
}

//==============================================================================
// Cache Management

void FeedStore::clearCache()
{
    Util::logInfo("FeedStore", "Clearing all cache");

    diskCache.clear();

    // Delete cache files
    for (auto feedType : { FeedType::Timeline, FeedType::Global, FeedType::Trending, FeedType::ForYou })
    {
        auto cacheFile = getCacheFile(feedType);
        if (cacheFile.exists())
            cacheFile.deleteFile();
    }
}

void FeedStore::clearCache(FeedType feedType)
{
    Util::logInfo("FeedStore", "Clearing cache for: " + feedTypeToString(feedType));

    diskCache.erase(feedType);

    auto cacheFile = getCacheFile(feedType);
    if (cacheFile.exists())
        cacheFile.deleteFile();
}

//==============================================================================
// Internal Implementation

void FeedStore::performFetch(FeedType feedType, int limit, int offset)
{
    if (!networkClient)
    {
        handleFetchError(feedType, "Network client not configured");
        return;
    }

    Util::logInfo("FeedStore", "Performing network fetch",
                  "feedType=" + feedTypeToString(feedType) +
                  " limit=" + juce::String(limit) +
                  " offset=" + juce::String(offset));

    auto callback = [this, feedType, limit, offset](Outcome<juce::var> result)
    {
        if (result.isOk())
        {
            handleFetchSuccess(feedType, result.getValue(), limit, offset);
        }
        else
        {
            handleFetchError(feedType, result.getError());
        }
    };

    // Call appropriate network method
    switch (feedType)
    {
        case FeedType::Timeline:
            networkClient->getTimelineFeed(limit, offset, callback);
            break;
        case FeedType::Global:
            networkClient->getGlobalFeed(limit, offset, callback);
            break;
        case FeedType::Trending:
            networkClient->getTrendingFeed(limit, offset, callback);
            break;
        case FeedType::ForYou:
            networkClient->getForYouFeed(limit, offset, callback);
            break;
    }
}

void FeedStore::handleFetchSuccess(FeedType feedType, const juce::var& data, int limit, int offset)
{
    auto response = parseJsonResponse(data);
    response.limit = limit;
    response.offset = offset;

    Util::logInfo("FeedStore", "Fetch success",
                  "feedType=" + feedTypeToString(feedType) +
                  " posts=" + juce::String(response.posts.size()) +
                  " hasMore=" + juce::String(response.hasMore ? "true" : "false"));

    // Update state
    updateState([feedType, response, offset](FeedStoreState& state)
    {
        auto& feed = state.feeds[feedType];

        if (offset == 0)
        {
            // First page - replace posts
            feed.posts = response.posts;
        }
        else
        {
            // Subsequent page - append posts
            for (const auto& post : response.posts)
                feed.posts.add(post);
        }

        feed.isLoading = false;
        feed.isRefreshing = false;
        feed.hasMore = response.hasMore;
        feed.offset = offset;
        feed.limit = response.limit;
        feed.total = response.total;
        feed.error = "";
        feed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
    });

    // Save to cache (only first page)
    if (offset == 0)
    {
        CacheEntry entry;
        entry.response = response;
        entry.timestamp = juce::Time::getCurrentTime();
        diskCache[feedType] = entry;
        saveCacheToDisk(feedType, entry);
    }
}

void FeedStore::handleFetchError(FeedType feedType, const juce::String& error)
{
    Util::logError("FeedStore", "Fetch error: " + error, "feedType=" + feedTypeToString(feedType));

    updateState([feedType, error](FeedStoreState& state)
    {
        auto& feed = state.feeds[feedType];
        feed.isLoading = false;
        feed.isRefreshing = false;
        feed.error = error;
    });
}

FeedResponse FeedStore::parseJsonResponse(const juce::var& json)
{
    FeedResponse response;

    if (json.isVoid())
    {
        response.error = "Invalid JSON response";
        return response;
    }

    // Parse activities array
    auto activities = json.getProperty("activities", juce::var());
    if (!activities.isArray())
    {
        if (json.isArray())
            activities = json;
        else
            return response; // Empty response
    }

    // Parse each activity into a FeedPost
    for (int i = 0; i < activities.size(); ++i)
    {
        auto post = FeedPost::fromJson(activities[i]);
        if (post.isValid())
            response.posts.add(post);
    }

    // Parse pagination info
    auto meta = json.getProperty("meta", juce::var());
    if (meta.isObject())
    {
        response.total = static_cast<int>(meta.getProperty("count", 0));
        response.limit = static_cast<int>(meta.getProperty("limit", 20));
        response.offset = static_cast<int>(meta.getProperty("offset", 0));
        response.hasMore = static_cast<bool>(meta.getProperty("has_more", false));
    }
    else
    {
        // Fallback to legacy format
        response.total = static_cast<int>(json.getProperty("total", 0));
        response.limit = static_cast<int>(json.getProperty("limit", 20));
        response.offset = static_cast<int>(json.getProperty("offset", 0));

        if (json.hasProperty("has_more"))
            response.hasMore = static_cast<bool>(json.getProperty("has_more", false));
        else
            response.hasMore = (response.offset + response.posts.size()) < response.total;
    }

    return response;
}

//==============================================================================
// Disk Cache

juce::File FeedStore::getCacheFile(FeedType feedType) const
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
        case FeedType::ForYou:   filename = "feed_foryou.json"; break;
    }

    return cacheDir.getChildFile(filename);
}

void FeedStore::loadCacheFromDisk(FeedType feedType)
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
        diskCache[feedType] = entry;
}

void FeedStore::saveCacheToDisk(FeedType feedType, const CacheEntry& entry)
{
    auto obj = new juce::DynamicObject();

    obj->setProperty("cache_timestamp", entry.timestamp.toISO8601(true));
    obj->setProperty("feed_type", feedTypeToString(feedType));

    // Serialize posts
    juce::Array<juce::var> postsArray;
    for (const auto& post : entry.response.posts)
        postsArray.add(post.toJson());
    obj->setProperty("posts", postsArray);

    obj->setProperty("limit", entry.response.limit);
    obj->setProperty("offset", entry.response.offset);
    obj->setProperty("total", entry.response.total);
    obj->setProperty("has_more", entry.response.hasMore);

    auto cacheFile = getCacheFile(feedType);
    auto jsonStr = juce::JSON::toString(juce::var(obj), true);
    cacheFile.replaceWithText(jsonStr);
}

//==============================================================================
// Helpers

const FeedPost* FeedStore::findPost(const juce::String& postId) const
{
    auto state = getState();
    const auto& currentFeed = state.getCurrentFeed();

    for (const auto& post : currentFeed.posts)
    {
        if (post.id == postId)
            return &post;
    }

    return nullptr;
}

std::optional<std::pair<FeedType, int>> FeedStore::findPostLocation(const juce::String& postId) const
{
    auto state = getState();

    for (const auto& [feedType, feed] : state.feeds)
    {
        for (int i = 0; i < feed.posts.size(); ++i)
        {
            if (feed.posts[i].id == postId)
                return std::make_pair(feedType, i);
        }
    }

    return std::nullopt;
}

void FeedStore::updatePostInAllFeeds(const juce::String& postId,
                                      std::function<void(FeedPost&)> updater)
{
    updateState([postId, updater](FeedStoreState& state)
    {
        for (auto& [feedType, feed] : state.feeds)
        {
            for (auto& post : feed.posts)
            {
                if (post.id == postId)
                {
                    updater(post);
                }
            }
        }
    });
}

void FeedStore::timerCallback()
{
    // Periodically clean up expired cache entries
    for (auto it = diskCache.begin(); it != diskCache.end();)
    {
        if (!it->second.isValid(cacheTTLSeconds))
            it = diskCache.erase(it);
        else
            ++it;
    }
}

}  // namespace Stores
}  // namespace Sidechain
