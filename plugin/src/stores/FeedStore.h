#pragma once

#include <JuceHeader.h>
#include "Store.h"
#include "CacheWarmer.h"
#include "../models/FeedPost.h"
#include "../models/FeedResponse.h"
#include "../models/AggregatedFeedGroup.h"
#include "../models/AggregatedFeedResponse.h"
#include "../network/NetworkClient.h"
#include "../network/RealtimeSync.h"
#include "../util/logging/Logger.h"
#include "../util/cache/CacheLayer.h"
#include <map>

namespace Sidechain {
namespace Stores {

/**
 * FeedType - Types of feeds available in the application
 *
 * Aggregation formats from getstream.io:
 * - TrendingAggregated: {{ genre }}_{{ time.strftime('%Y-%m-%d') }}
 * - TimelineAggregated: {{ actor }}_{{ verb }}_{{ time.strftime('%Y-%m-%d') }}
 * - NotificationAggregated: {{ verb }}_{{ time.strftime('%Y-%m-%d') }}
 * - UserActivityAggregated: {{ verb }}_{{ time.strftime('%Y-%m-%d') }}
 */
enum class FeedType
{
    // Flat feeds (individual activities)
    Timeline,                  // User's following feed (posts from people they follow)
    Global,                    // Global discover feed (all public posts)
    Trending,                  // Trending feed (posts sorted by engagement score)
    ForYou,                    // Personalized recommendations based on listening history

    // Aggregated feeds (grouped activities)
    TimelineAggregated,        // Timeline grouped by actor+verb+day
    TrendingAggregated,        // Trending grouped by genre+day
    NotificationAggregated,    // Notifications grouped by verb+day
    UserActivityAggregated     // User activity grouped by verb+day
};

/**
 * Convert FeedType to string for logging/display
 */
inline juce::String feedTypeToString(FeedType type)
{
    switch (type)
    {
        case FeedType::Timeline:               return "Timeline";
        case FeedType::Global:                 return "Global";
        case FeedType::Trending:               return "Trending";
        case FeedType::ForYou:                 return "ForYou";
        case FeedType::TimelineAggregated:     return "TimelineAggregated";
        case FeedType::TrendingAggregated:     return "TrendingAggregated";
        case FeedType::NotificationAggregated: return "NotificationAggregated";
        case FeedType::UserActivityAggregated: return "UserActivityAggregated";
        default:                               return "Unknown";
    }
}

/**
 * Check if a FeedType is aggregated
 */
inline bool isAggregatedFeedType(FeedType type)
{
    return type == FeedType::TimelineAggregated ||
           type == FeedType::TrendingAggregated ||
           type == FeedType::NotificationAggregated ||
           type == FeedType::UserActivityAggregated;
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
    bool isSynced = true;  // Real-time sync status (Task 4.21)

    bool operator==(const SingleFeedState& other) const
    {
        return posts.size() == other.posts.size() &&
               isLoading == other.isLoading &&
               isRefreshing == other.isRefreshing &&
               hasMore == other.hasMore &&
               offset == other.offset &&
               error == other.error &&
               lastUpdated == other.lastUpdated &&
               isSynced == other.isSynced;
    }
};

/**
 * AggregatedFeedState - State for aggregated feeds (groups instead of flat posts)
 */
struct AggregatedFeedState
{
    juce::Array<AggregatedFeedGroup> groups;
    bool isLoading = false;
    bool isRefreshing = false;
    bool hasMore = true;
    int offset = 0;
    int limit = 20;
    int total = 0;
    juce::String error;
    int64_t lastUpdated = 0;
    bool isSynced = true;

    bool operator==(const AggregatedFeedState& other) const
    {
        return groups.size() == other.groups.size() &&
               isLoading == other.isLoading &&
               isRefreshing == other.isRefreshing &&
               hasMore == other.hasMore &&
               offset == other.offset &&
               error == other.error &&
               lastUpdated == other.lastUpdated &&
               isSynced == other.isSynced;
    }
};

/**
 * FeedStoreState - Combined state for all feed types
 */
struct FeedStoreState
{
    std::map<FeedType, SingleFeedState> feeds;
    std::map<FeedType, AggregatedFeedState> aggregatedFeeds;
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

    const AggregatedFeedState& getCurrentAggregatedFeed() const
    {
        static AggregatedFeedState empty;
        auto it = aggregatedFeeds.find(currentFeedType);
        return it != aggregatedFeeds.end() ? it->second : empty;
    }

    AggregatedFeedState& getCurrentAggregatedFeedMutable()
    {
        return aggregatedFeeds[currentFeedType];
    }

    bool operator==(const FeedStoreState& other) const
    {
        return feeds == other.feeds &&
               aggregatedFeeds == other.aggregatedFeeds &&
               currentFeedType == other.currentFeedType;
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
     * Toggle follow/unfollow on a post author (optimistic update)
     * @param postId The post ID (to find the author)
     * @param willFollow true to follow, false to unfollow
     */
    void toggleFollow(const juce::String& postId, bool willFollow);

    /**
     * Toggle mute/unmute on a user by ID (optimistic update, Task 2.4)
     * @param userId The user ID to mute/unmute
     * @param willMute true to mute, false to unmute
     */
    void toggleMute(const juce::String& userId, bool willMute);

    /**
     * Toggle block/unblock on a user by ID (optimistic update)
     * @param userId The user ID to block/unblock
     * @param willBlock true to block, false to unblock
     */
    void toggleBlock(const juce::String& userId, bool willBlock);

    /**
     * Toggle archive state on a post (optimistic update)
     * @param postId The post ID to archive/unarchive
     * @param archived true to archive, false to unarchive
     */
    void toggleArchive(const juce::String& postId, bool archived);

    /**
     * Toggle pin state on a post (optimistic update, own posts only)
     * @param postId The post ID to pin/unpin
     * @param pinned true to pin, false to unpin
     */
    void togglePin(const juce::String& postId, bool pinned);

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
    // Real-Time Synchronization (Task 4.21)

    /**
     * Enable real-time synchronization for feed updates
     * Uses WebSocket + Operational Transform for < 500ms latency
     */
    void enableRealtimeSync();

    /**
     * Disable real-time synchronization
     */
    void disableRealtimeSync();

    /**
     * Check if real-time sync is enabled
     */
    bool isRealtimeSyncEnabled() const { return realtimeSync != nullptr; }

    /**
     * Get real-time sync status for current feed
     */
    bool isCurrentFeedSynced() const
    {
        return getState().getCurrentFeed().isSynced;
    }

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

    // Real-time synchronization (Task 4.21)
    std::shared_ptr<Network::RealtimeSync> realtimeSync;

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
    AggregatedFeedResponse parseAggregatedJsonResponse(const juce::var& json);

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
