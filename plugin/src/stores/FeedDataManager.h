#pragma once

#include "../models/FeedResponse.h"
#include "../network/NetworkClient.h"
#include <JuceHeader.h>

//==============================================================================
/**
 * FeedDataManager handles fetching, caching, and pagination of feed data
 *
 * Features:
 * - Async feed fetching with callback-based API
 * - Local JSON cache with configurable TTL (default 5 minutes)
 * - Pagination support for infinite scroll
 * - Multiple feed types (timeline, global)
 * - Error handling and retry logic
 *
 * Thread Safety:
 * - All public methods should be called from MESSAGE THREAD
 * - Network callbacks are dispatched to message thread automatically
 */
class FeedDataManager : private juce::Timer {
public:
  FeedDataManager();
  ~FeedDataManager() override;

  //==============================================================================
  // Feed types supported
  enum class FeedType {
    Timeline, // User's following feed (posts from people they follow)
    Global,   // Global discover feed (all public posts)
    Trending, // Trending feed (posts sorted by engagement score)
    ForYou    // Personalized recommendations based on listening history
  };

  //==============================================================================
  // Callback types
  using FeedCallback = std::function<void(const FeedResponse &)>;
  using RefreshCallback = std::function<void(bool success, const juce::String &error)>;

  //==============================================================================
  // Feed fetching

  /** Fetch a page of feed posts
   *  @param feedType Which feed to fetch
   *  @param limit Number of posts per page (default 20)
   *  @param offset Starting offset for pagination
   *  @param callback Called when fetch completes (on message thread)
   */
  void fetchFeed(FeedType feedType, int limit, int offset, FeedCallback callback);

  /** Convenience method to fetch first page */
  void fetchFeed(FeedType feedType, FeedCallback callback);

  /** Refresh the current feed (fetch fresh data, ignore cache)
   *  @param callback Called when refresh completes
   */
  void refreshFeed(RefreshCallback callback);

  /** Load more posts (pagination) */
  void loadMorePosts(FeedCallback callback);

  //==============================================================================
  // Cache management

  /** Set cache TTL in seconds (default 300 = 5 minutes) */
  void setCacheTTL(int seconds) {
    cacheTTLSeconds = seconds;
  }
  int getCacheTTL() const {
    return cacheTTLSeconds;
  }

  /** Clear all cached data */
  void clearCache();

  /** Clear cache for a specific feed type */
  void clearCache(FeedType feedType);

  /** Check if cache is valid for a feed type */
  bool isCacheValid(FeedType feedType) const;

  /** Get cached posts (returns empty if cache invalid) */
  FeedResponse getCachedFeed(FeedType feedType) const;

  //==============================================================================
  // State queries

  /** Check if currently fetching data */
  bool isFetching() const {
    return fetchingInProgress;
  }

  /** Get the current feed type */
  FeedType getCurrentFeedType() const {
    return currentFeedType;
  }

  /** Set the current feed type (for pagination context) */
  void setCurrentFeedType(FeedType type) {
    currentFeedType = type;
  }

  /** Get current pagination state */
  int getCurrentOffset() const {
    return currentOffset;
  }
  int getCurrentLimit() const {
    return currentLimit;
  }
  bool hasMorePosts() const {
    return hasMore;
  }

  /** Get total loaded posts count */
  int getLoadedPostsCount() const;

  //==============================================================================
  // Network client

  /** Set the network client to use for requests */
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  /** Get the network client */
  NetworkClient *getNetworkClient() const {
    return networkClient;
  }

  //==============================================================================
  // Configuration

  /** Set the base URL for API requests */
  void setBaseURL(const juce::String &url) {
    baseURL = url;
  }

  /** Set authentication token */
  void setAuthToken(const juce::String &token) {
    authToken = token;
  }

private:
  //==============================================================================
  // Timer callback for cache expiration checks
  void timerCallback() override;

  //==============================================================================
  // Internal fetch implementation
  void performFetch(FeedType feedType, int limit, int offset, FeedCallback callback);
  void handleFetchResponse(const juce::var &feedData, FeedType feedType, int limit, int offset, FeedCallback callback);
  void handleFetchError(const juce::String &error, FeedCallback callback);

  //==============================================================================
  // Cache implementation
  struct CacheEntry {
    FeedResponse response;
    juce::Time timestamp;
    FeedType feedType;
    int offset = 0;

    bool isValid(int ttlSeconds) const {
      auto age = juce::Time::getCurrentTime() - timestamp;
      return age.inSeconds() < ttlSeconds;
    }
  };

  juce::File getCacheFile(FeedType feedType) const;
  void loadCacheFromDisk(FeedType feedType);
  void saveCacheToDisk(FeedType feedType, const CacheEntry &entry);
  void updateCache(FeedType feedType, const FeedResponse &response, int offset);

  //==============================================================================
  // JSON parsing helpers
  FeedResponse parseJsonResponse(const juce::var &json);
  juce::String getEndpointForFeedType(FeedType feedType) const;

  //==============================================================================
  // State
  NetworkClient *networkClient = nullptr;
  juce::String baseURL;
  juce::String authToken;

  FeedType currentFeedType = FeedType::Timeline;
  int currentOffset = 0;
  int currentLimit = 20;
  bool hasMore = true;
  bool fetchingInProgress = false;

  // Cache
  std::map<FeedType, CacheEntry> cache;
  std::map<FeedType, juce::Array<FeedPost>> loadedPosts; // Accumulated posts for infinite scroll
  int cacheTTLSeconds = 300;                             // 5 minutes default

  // Callbacks waiting for response
  FeedCallback pendingCallback;
  RefreshCallback pendingRefreshCallback;

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeedDataManager)
};
