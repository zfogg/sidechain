#pragma once

#include "../models/AggregatedFeedGroup.h"
#include "../models/AggregatedFeedResponse.h"
#include "../models/FeedPost.h"
#include "../models/FeedResponse.h"
#include "../network/NetworkClient.h"
#include "../network/RealtimeSync.h"
#include "../util/cache/CacheLayer.h"
#include "../util/logging/Logger.h"
#include "CacheWarmer.h"
#include "Store.h"
#include <JuceHeader.h>
#include <map>

namespace Sidechain {
namespace Stores {

//==============================================================================
// Feed Types & Helpers
//==============================================================================

/**
 * FeedType - Types of feeds available in the application
 */
enum class FeedType {
  // Flat feeds (individual activities)
  Timeline,  // User's following feed
  Global,    // Global discover feed
  Trending,  // Trending feed
  ForYou,    // Personalized recommendations
  Popular,   // Popular posts from Gorse
  Latest,    // Latest posts from Gorse
  Discovery, // Discovery feed

  // Aggregated feeds
  TimelineAggregated,     // Timeline grouped
  TrendingAggregated,     // Trending grouped
  NotificationAggregated, // Notifications grouped
  UserActivityAggregated  // User activity grouped
};

inline juce::String feedTypeToString(FeedType type) {
  switch (type) {
  case FeedType::Timeline:
    return "Timeline";
  case FeedType::Global:
    return "Global";
  case FeedType::Trending:
    return "Trending";
  case FeedType::ForYou:
    return "ForYou";
  case FeedType::Popular:
    return "Popular";
  case FeedType::Latest:
    return "Latest";
  case FeedType::Discovery:
    return "Discovery";
  case FeedType::TimelineAggregated:
    return "TimelineAggregated";
  case FeedType::TrendingAggregated:
    return "TrendingAggregated";
  case FeedType::NotificationAggregated:
    return "NotificationAggregated";
  case FeedType::UserActivityAggregated:
    return "UserActivityAggregated";
  default:
    return "Unknown";
  }
}

inline bool isAggregatedFeedType(FeedType type) {
  return type == FeedType::TimelineAggregated || type == FeedType::TrendingAggregated ||
         type == FeedType::NotificationAggregated || type == FeedType::UserActivityAggregated;
}

//==============================================================================
// Feed State Structures
//==============================================================================

/**
 * SavedPostsState - State for saved/bookmarked posts
 */
struct SavedPostsState {
  juce::Array<FeedPost> posts;
  bool isLoading = false;
  juce::String error;
  int totalCount = 0;
  int offset = 0;
  int limit = 20;
  bool hasMore = true;
  int64_t lastUpdated = 0;
};

/**
 * ArchivedPostsState - State for archived posts
 */
struct ArchivedPostsState {
  juce::Array<FeedPost> posts;
  bool isLoading = false;
  juce::String error;
  int totalCount = 0;
  int offset = 0;
  int limit = 20;
  bool hasMore = true;
  int64_t lastUpdated = 0;
};

/**
 * FeedState - State for a single feed type (flat posts)
 */
struct FeedState {
  juce::Array<FeedPost> posts;
  bool isLoading = false;
  bool isRefreshing = false;
  bool hasMore = true;
  int offset = 0;
  int limit = 20;
  int total = 0;
  juce::String error;
  int64_t lastUpdated = 0;
  bool isSynced = true;
};

/**
 * AggregatedFeedState - State for aggregated feeds (groups)
 */
struct AggregatedFeedState {
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
};

/**
 * PostsState - Immutable state for all post collections in the application
 *
 * Manages:
 * - Feed posts (Timeline, Trending, Global, ForYou, etc.)
 * - Saved posts (bookmarked/liked posts)
 * - Archived posts (hidden/deleted posts)
 *
 * Each collection has independent loading, pagination, and error state.
 * This consolidates both FeedStore and post collection management.
 */
struct PostsState {
  // Feed collections (multiple feed types)
  std::map<FeedType, FeedState> feeds;
  std::map<FeedType, AggregatedFeedState> aggregatedFeeds;
  FeedType currentFeedType = FeedType::Timeline;

  // User post collections
  SavedPostsState savedPosts;
  ArchivedPostsState archivedPosts;

  // Global error tracking
  juce::String errorMessage;
  int64_t lastUpdated = 0;

  // Convenience accessors for current feed
  const FeedState &getCurrentFeed() const {
    static FeedState empty;
    auto it = feeds.find(currentFeedType);
    return it != feeds.end() ? it->second : empty;
  }

  const AggregatedFeedState &getCurrentAggregatedFeed() const {
    static AggregatedFeedState empty;
    auto it = aggregatedFeeds.find(currentFeedType);
    return it != aggregatedFeeds.end() ? it->second : empty;
  }
};

//==============================================================================
// PostsStore - Consolidated Store for All Posts
//==============================================================================

/**
 * PostsStore - Reactive store for managing all post collections
 *
 * Consolidates FeedStore and post collection management into a single
 * generalized store that handles:
 * - Feed posts (Timeline, Trending, Global, ForYou, etc. with multi-tier caching)
 * - Saved posts (bookmarked/liked posts)
 * - Archived posts (hidden/deleted posts)
 *
 * Features:
 * - Multiple feed types with independent loading/pagination state
 * - Optimistic updates for likes, saves, follows, reactions
 * - Multi-tier caching (memory + disk) with TTL
 * - Real-time synchronization via WebSocket
 * - Cache warming for offline support
 * - Aggregated and flat feed support
 *
 * Usage:
 * ```cpp
 * auto postsStore = std::make_shared<PostsStore>(networkClient);
 * postsStore->subscribe([this](const PostsState& state) {
 *   displayFeedPosts(state.getCurrentFeed().posts);
 *   displaySavedPosts(state.savedPosts.posts);
 * });
 * postsStore->loadFeed(FeedType::Timeline);
 * postsStore->loadSavedPosts();
 * ```
 */
class PostsStore : public Store<PostsState>, private juce::Timer {
public:
  explicit PostsStore(NetworkClient *client);
  ~PostsStore() override;

  //==============================================================================
  // Network Client Management

  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  NetworkClient *getNetworkClient() const {
    return networkClient;
  }

  //==============================================================================
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
   */
  void switchFeedType(FeedType feedType);

  /**
   * Get current feed type
   */
  FeedType getCurrentFeedType() const {
    return getState().currentFeedType;
  }

  //==============================================================================
  // Post Interactions (Optimistic Updates)

  void toggleLike(const juce::String &postId);
  void toggleSave(const juce::String &postId);
  void toggleRepost(const juce::String &postId);
  void addReaction(const juce::String &postId, const juce::String &emoji);
  void toggleFollow(const juce::String &postId, bool willFollow);
  void updateFollowStateByUserId(const juce::String &userId, bool willFollow);
  void toggleMute(const juce::String &userId, bool willMute);
  void toggleBlock(const juce::String &userId, bool willBlock);
  void toggleArchive(const juce::String &postId, bool archived);
  void togglePin(const juce::String &postId, bool pinned);
  void updatePlayCount(const juce::String &postId, int newCount);

  //==============================================================================
  // Real-Time Updates

  void handleNewPostNotification(const juce::var &postData);
  void handleLikeCountUpdate(const juce::String &postId, int likeCount);
  void updateUserPresence(const juce::String &userId, bool isOnline, const juce::String &status);

  //==============================================================================
  // Cache Management

  void setCacheTTL(int seconds) {
    cacheTTLSeconds = seconds;
  }

  int getCacheTTL() const {
    return cacheTTLSeconds;
  }

  void clearCache();
  void clearCache(FeedType feedType);

  //==============================================================================
  // Cache Warming & Offline Support

  void startCacheWarming();
  void stopCacheWarming();
  void setOnlineStatus(bool isOnline);
  bool isOnline() const;
  bool isCurrentFeedCached() const;

  //==============================================================================
  // Real-Time Synchronization

  void enableRealtimeSync();
  void disableRealtimeSync();
  bool isRealtimeSyncEnabled() const;
  bool isCurrentFeedSynced() const;

  //==============================================================================
  // Saved Posts Management

  void loadSavedPosts();
  void loadMoreSavedPosts();
  void refreshSavedPosts();
  void unsavePost(const juce::String &postId);
  std::optional<FeedPost> getSavedPostById(const juce::String &postId) const;

  //==============================================================================
  // Archived Posts Management

  void loadArchivedPosts();
  void loadMoreArchivedPosts();
  void refreshArchivedPosts();
  void restorePost(const juce::String &postId);
  std::optional<FeedPost> getArchivedPostById(const juce::String &postId) const;

  //==============================================================================
  // Helpers

  const FeedPost *findPost(const juce::String &postId) const;
  std::optional<std::pair<FeedType, int>> findPostLocation(const juce::String &postId) const;

protected:
  void removePostFromSaved(const juce::String &postId);
  void removePostFromArchived(const juce::String &postId);
  void updatePostInSaved(const FeedPost &post);
  void updatePostInArchived(const FeedPost &post);

private:
  // Timer callback for cache expiration
  void timerCallback() override;

  // Network client
  NetworkClient *networkClient = nullptr;

  // Cache settings
  int cacheTTLSeconds = 3600; // 1 hour

  // Multi-tier cache
  std::unique_ptr<Util::Cache::MultiTierCache<juce::String, juce::Array<FeedPost>>> feedCache;

  // Cache warmer
  std::shared_ptr<CacheWarmer> cacheWarmer;
  bool isOnlineStatus_ = true;
  bool currentFeedIsFromCache_ = false;

  // Real-time sync
  std::shared_ptr<Network::RealtimeSync> realtimeSync;

  // Legacy cache storage
  struct CacheEntry {
    FeedResponse response;
    juce::Time timestamp;
    bool isValid(int ttlSeconds) const {
      auto age = juce::Time::getCurrentTime() - timestamp;
      return age.inSeconds() < ttlSeconds;
    }
  };
  std::map<FeedType, CacheEntry> diskCache;

  // Internal helpers
  void performFetch(FeedType feedType, int limit, int offset);
  void handleFetchSuccess(FeedType feedType, const juce::var &data, int limit, int offset);
  void handleFetchError(FeedType feedType, const juce::String &error);
  FeedResponse parseJsonResponse(const juce::var &json);
  AggregatedFeedResponse parseAggregatedJsonResponse(const juce::var &json);

  // Cache helpers
  juce::String feedTypeToCacheKey(FeedType feedType) const;
  void schedulePopularFeedWarmup();
  void warmTimeline();
  void warmTrending();
  void warmUserPosts();

  // Disk cache helpers
  juce::File getCacheFile(FeedType feedType) const;
  void loadCacheFromDisk(FeedType feedType);
  void saveCacheToDisk(FeedType feedType, const CacheEntry &entry);

  // Update helpers
  void updatePostInAllFeeds(const juce::String &postId, std::function<void(FeedPost &)> updater);
  void updateSavedPosts(const juce::Array<FeedPost> &posts, int totalCount, int offset, bool hasMore);
  void updateArchivedPosts(const juce::Array<FeedPost> &posts, int totalCount, int offset, bool hasMore);

  // Network callbacks
  void handleSavedPostsLoaded(Outcome<juce::var> result);
  void handleArchivedPostsLoaded(Outcome<juce::var> result);
  void handlePostUnsaved(const juce::String &postId, Outcome<juce::var> result);
  void handlePostRestored(const juce::String &postId, Outcome<juce::var> result);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostsStore)
};

} // namespace Stores
} // namespace Sidechain
