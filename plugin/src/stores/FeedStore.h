#pragma once

#include <JuceHeader.h>
#include "Store.h"
#include "CacheWarmer.h"
#include "../models/FeedPost.h"
#include "../models/FeedResponse.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "../util/cache/CacheLayer.h"
#include <map>

namespace Sidechain {
namespace Stores {

/**
 * FeedType - Types of feeds available in the application
 */
enum class FeedType
{
    Timeline,   // User's following feed (posts from people they follow)
    Global,     // Global discover feed (all public posts)
    Trending,   // Trending feed (posts sorted by engagement score)
    ForYou      // Personalized recommendations based on listening history
};

/**
 * Convert FeedType to string for logging/display
 */
inline juce::String feedTypeToString(FeedType type)
{
    switch (type)
    {
        case FeedType::Timeline: return "Timeline";
        case FeedType::Global:   return "Global";
        case FeedType::Trending: return "Trending";
        case FeedType::ForYou:   return "ForYou";
        default:                 return "Unknown";
    }
}

/**
 * FeedState - Immutable state for a single feed type
 */
struct SingleFeedState
{
    juce::Array<FeedPost> posts;
    bool isLoading = false;
    bool isRefreshing = false;
    bool hasMore = true;
    int offset = 0;
    int limit = 20;
    int total = 0;
    juce::String error;
    int64_t lastUpdated = 0;

    bool operator==(const SingleFeedState& other) const
    {
        return posts.size() == other.posts.size() &&
               isLoading == other.isLoading &&
               isRefreshing == other.isRefreshing &&
               hasMore == other.hasMore &&
               offset == other.offset &&
               error == other.error &&
               lastUpdated == other.lastUpdated;
    }
};

/**
 * FeedStoreState - Combined state for all feed types
 */
struct FeedStoreState
{
    std::map<FeedType, SingleFeedState> feeds;
    FeedType currentFeedType = FeedType::Timeline;

    // Convenience accessors
    const SingleFeedState& getCurrentFeed() const
    {
        static SingleFeedState empty;
        auto it = feeds.find(currentFeedType);
        return it != feeds.end() ? it->second : empty;
    }

    SingleFeedState& getCurrentFeedMutable()
    {
        return feeds[currentFeedType];
    }

    bool operator==(const FeedStoreState& other) const
    {
        return feeds == other.feeds && currentFeedType == other.currentFeedType;
    }
};

/**
 * FeedStore - Reactive store for managing feed data
 *
 * Replaces callback-based FeedDataManager with reactive subscriptions.
 *
 * Features:
 * - Reactive state management: subscribe to feed updates
 * - Optimistic updates: like/save operations update UI immediately
 * - Error recovery: rollback on network failure
 * - Caching: disk and memory cache with TTL
 * - Pagination: automatic load-more support
 *
 * Usage:
 *   // Get singleton instance
 *   auto& feedStore = FeedStore::getInstance();
 *   feedStore.setNetworkClient(networkClient);
 *
 *   // Subscribe to state changes
 *   auto unsubscribe = feedStore.subscribe([](const FeedStoreState& state) {
 *       const auto& feed = state.getCurrentFeed();
 *       if (feed.isLoading) {
 *           showLoadingSpinner();
 *       } else if (!feed.error.isEmpty()) {
 *           showError(feed.error);
 *       } else {
 *           displayPosts(feed.posts);
 *       }
 *   });
 *
 *   // Load feed
 *   feedStore.loadFeed(FeedType::Timeline);
 *
 *   // Optimistic like
 *   feedStore.toggleLike(postId);  // Updates UI immediately, syncs in background
 */
class FeedStore : public Store<FeedStoreState>, private juce::Timer
{
public:
    /**
     * Get singleton instance
     */
    static FeedStore& getInstance()
    {
        static FeedStore instance;
        return instance;
    }

    /**
     * Set the network client for API requests
     */
    void setNetworkClient(NetworkClient* client)
    {
        networkClient = client;
    }

    /**
     * Get the network client
     */
    NetworkClient* getNetworkClient() const { return networkClient; }

    //==========================================================================
    // Feed Loading

    /**
     * Load feed (first page or refresh)
     * @param feedType Which feed to load
     * @param forceRefresh If true, bypass cache
     */
    void loadFeed(FeedType feedType, bool forceRefresh = false);

    /**
     * Refresh current feed (clear cache and reload)
     */
    void refreshCurrentFeed();

    /**
     * Load more posts for current feed (pagination)
     */
    void loadMore();

    /**
     * Switch to a different feed type
     * @param feedType The feed type to switch to
     */
    void switchFeedType(FeedType feedType);

    /**
     * Get current feed type
     */
    FeedType getCurrentFeedType() const
    {
        return getState().currentFeedType;
    }

    //==========================================================================
    // Post Interactions (Optimistic Updates)

    /**
     * Toggle like on a post (optimistic update)
     * @param postId The post ID to toggle like on
     */
    void toggleLike(const juce::String& postId);

    /**
     * Toggle save/bookmark on a post (optimistic update)
     * @param postId The post ID to toggle save on
     */
    void toggleSave(const juce::String& postId);

    /**
     * Toggle repost on a post (optimistic update)
     * @param postId The post ID to toggle repost on
     */
    void toggleRepost(const juce::String& postId);

    /**
     * Add emoji reaction to a post (optimistic update)
     * @param postId The post ID to react to
     * @param emoji The emoji to add (empty string to remove reaction)
     */
    void addReaction(const juce::String& postId, const juce::String& emoji);

    /**
     * Update play count for a post
     * @param postId The post ID
     * @param newCount The new play count
     */
    void updatePlayCount(const juce::String& postId, int newCount);

    //==========================================================================
    // Post Updates (from real-time events)

    /**
     * Handle new post notification (from WebSocket)
     * @param postData JSON data for new post
     */
    void handleNewPostNotification(const juce::var& postData);

    /**
     * Update like count from server event
     * @param postId The post ID
     * @param likeCount New like count
     */
    void handleLikeCountUpdate(const juce::String& postId, int likeCount);

    /**
     * Update user presence in feed posts
     * @param userId User ID
     * @param isOnline Online status
     * @param status Custom status string
     */
    void updateUserPresence(const juce::String& userId, bool isOnline, const juce::String& status);

    //==========================================================================
    // Cache Management

    /**
     * Set cache TTL in seconds (default 3600 = 1 hour)
     */
    void setCacheTTL(int seconds) { cacheTTLSeconds = seconds; }

    /**
     * Get cache TTL
     */
    int getCacheTTL() const { return cacheTTLSeconds; }

    /**
     * Clear all cached data
     */
    void clearCache();

    /**
     * Clear cache for a specific feed type
     */
    void clearCache(FeedType feedType);

    //==========================================================================
    // Cache Warming & Offline Support (Task 4.14)

    /**
     * Start cache warming for popular feeds
     * Pre-fetches Timeline, Trending, and user's own posts in background
     */
    void startCacheWarming();

    /**
     * Stop cache warming
     */
    void stopCacheWarming();

    /**
     * Set online/offline status
     * When offline, shows cached data. When online, auto-syncs.
     *
     * @param isOnline True if online, false if offline
     */
    void setOnlineStatus(bool isOnline);

    /**
     * Get current online status
     */
    bool isOnline() const { return isOnlineStatus_; }

    /**
     * Check if current feed data is from cache (for "cached" badge)
     */
    bool isCurrentFeedCached() const;

    //==========================================================================
    // Helpers

    /**
     * Find a post by ID in current feed
     * @param postId The post ID to find
     * @return Pointer to post or nullptr if not found
     */
    const FeedPost* findPost(const juce::String& postId) const;

    /**
     * Find a post by ID in any feed
     * @param postId The post ID to find
     * @return Pair of (FeedType, index) or nullopt if not found
     */
    std::optional<std::pair<FeedType, int>> findPostLocation(const juce::String& postId) const;

private:
    FeedStore();
    ~FeedStore() override;

    // Timer callback for cache expiration
    void timerCallback() override;

    // Network client (not owned)
    NetworkClient* networkClient = nullptr;

    // Cache settings
    int cacheTTLSeconds = 3600; // 1 hour (Task 4.13 requirement)

    // Multi-tier cache for feed data (Task 4.13)
    // Key: FeedType encoded as string, Value: Array of FeedPosts
    std::unique_ptr<Util::Cache::MultiTierCache<juce::String, juce::Array<FeedPost>>> feedCache;

    // Cache warmer for offline support (Task 4.14)
    std::shared_ptr<CacheWarmer> cacheWarmer;
    bool isOnlineStatus_ = true;
    bool currentFeedIsFromCache_ = false;

    // Legacy cache storage for backward compatibility during migration
    struct CacheEntry
    {
        FeedResponse response;
        juce::Time timestamp;

        bool isValid(int ttlSeconds) const
        {
            auto age = juce::Time::getCurrentTime() - timestamp;
            return age.inSeconds() < ttlSeconds;
        }
    };
    std::map<FeedType, CacheEntry> diskCache;

    // Internal helpers
    void performFetch(FeedType feedType, int limit, int offset);
    void handleFetchSuccess(FeedType feedType, const juce::var& data, int limit, int offset);
    void handleFetchError(FeedType feedType, const juce::String& error);
    FeedResponse parseJsonResponse(const juce::var& json);

    // Cache helpers (Task 4.13)
    juce::String feedTypeToCacheKey(FeedType feedType) const;

    // Cache warming helpers (Task 4.14)
    void schedulePopularFeedWarmup();
    void warmTimeline();
    void warmTrending();
    void warmUserPosts();

    // Disk cache helpers (legacy)
    juce::File getCacheFile(FeedType feedType) const;
    void loadCacheFromDisk(FeedType feedType);
    void saveCacheToDisk(FeedType feedType, const CacheEntry& entry);

    // Update a post across all feeds
    void updatePostInAllFeeds(const juce::String& postId,
                               std::function<void(FeedPost&)> updater);

    // Singleton enforcement
    FeedStore(const FeedStore&) = delete;
    FeedStore& operator=(const FeedStore&) = delete;
};

}  // namespace Stores
}  // namespace Sidechain
