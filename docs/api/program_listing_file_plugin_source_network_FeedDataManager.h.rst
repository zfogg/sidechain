
.. _program_listing_file_plugin_source_network_FeedDataManager.h:

Program Listing for File FeedDataManager.h
==========================================

|exhale_lsh| :ref:`Return to documentation for file <file_plugin_source_network_FeedDataManager.h>` (``plugin/source/network/FeedDataManager.h``)

.. |exhale_lsh| unicode:: U+021B0 .. UPWARDS ARROW WITH TIP LEFTWARDS

.. code-block:: cpp

   #pragma once
   
   #include <JuceHeader.h>
   #include "../models/FeedResponse.h"
   #include "NetworkClient.h"
   
   //==============================================================================
   class FeedDataManager : private juce::Timer
   {
   public:
       FeedDataManager();
       ~FeedDataManager() override;
   
       //==============================================================================
       // Feed types supported
       enum class FeedType
       {
           Timeline,   // User's following feed (posts from people they follow)
           Global,     // Global discover feed (all public posts)
           Trending    // Trending feed (posts sorted by engagement score)
       };
   
       //==============================================================================
       // Callback types
       using FeedCallback = std::function<void(const FeedResponse&)>;
       using RefreshCallback = std::function<void(bool success, const juce::String& error)>;
   
       //==============================================================================
       // Feed fetching
   
       void fetchFeed(FeedType feedType, int limit, int offset, FeedCallback callback);
   
       void fetchFeed(FeedType feedType, FeedCallback callback);
   
       void refreshFeed(RefreshCallback callback);
   
       void loadMorePosts(FeedCallback callback);
   
       //==============================================================================
       // Cache management
   
       void setCacheTTL(int seconds) { cacheTTLSeconds = seconds; }
       int getCacheTTL() const { return cacheTTLSeconds; }
   
       void clearCache();
   
       void clearCache(FeedType feedType);
   
       bool isCacheValid(FeedType feedType) const;
   
       FeedResponse getCachedFeed(FeedType feedType) const;
   
       //==============================================================================
       // State queries
   
       bool isFetching() const { return fetchingInProgress; }
   
       FeedType getCurrentFeedType() const { return currentFeedType; }
   
       void setCurrentFeedType(FeedType type) { currentFeedType = type; }
   
       int getCurrentOffset() const { return currentOffset; }
       int getCurrentLimit() const { return currentLimit; }
       bool hasMorePosts() const { return hasMore; }
   
       int getLoadedPostsCount() const;
   
       //==============================================================================
       // Network client
   
       void setNetworkClient(NetworkClient* client) { networkClient = client; }
   
       NetworkClient* getNetworkClient() const { return networkClient; }
   
       //==============================================================================
       // Configuration
   
       void setBaseURL(const juce::String& url) { baseURL = url; }
   
       void setAuthToken(const juce::String& token) { authToken = token; }
   
   private:
       //==============================================================================
       // Timer callback for cache expiration checks
       void timerCallback() override;
   
       //==============================================================================
       // Internal fetch implementation
       void performFetch(FeedType feedType, int limit, int offset, FeedCallback callback);
       void handleFetchResponse(const juce::var& feedData, FeedType feedType,
                               int limit, int offset, FeedCallback callback);
       void handleFetchError(const juce::String& error, FeedCallback callback);
   
       //==============================================================================
       // Cache implementation
       struct CacheEntry
       {
           FeedResponse response;
           juce::Time timestamp;
           FeedType feedType;
           int offset = 0;
   
           bool isValid(int ttlSeconds) const
           {
               auto age = juce::Time::getCurrentTime() - timestamp;
               return age.inSeconds() < ttlSeconds;
           }
       };
   
       juce::File getCacheFile(FeedType feedType) const;
       void loadCacheFromDisk(FeedType feedType);
       void saveCacheToDisk(FeedType feedType, const CacheEntry& entry);
       void updateCache(FeedType feedType, const FeedResponse& response, int offset);
   
       //==============================================================================
       // JSON parsing helpers
       FeedResponse parseJsonResponse(const juce::var& json);
       juce::String getEndpointForFeedType(FeedType feedType) const;
   
       //==============================================================================
       // State
       NetworkClient* networkClient = nullptr;
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
       int cacheTTLSeconds = 300; // 5 minutes default
   
       // Callbacks waiting for response
       FeedCallback pendingCallback;
       RefreshCallback pendingRefreshCallback;
   
       //==============================================================================
       JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeedDataManager)
   };
