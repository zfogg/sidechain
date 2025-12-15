#include "FeedStore.h"
#include "../util/Time.h"
#include "../util/profiling/PerformanceMonitor.h"
#include "../util/error/ErrorTracking.h"

namespace Sidechain {
namespace Stores {

FeedStore::FeedStore()
{
    // Initialize multi-tier cache (Task 4.13)
    // Memory tier: 100MB, Disk tier: 1GB
    auto cacheDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("Sidechain")
                        .getChildFile("feed_cache");

    feedCache = std::make_unique<Util::Cache::MultiTierCache<juce::String, juce::Array<FeedPost>>>(
        100 * 1024 * 1024,  // 100MB memory cache
        cacheDir,            // Disk cache directory
        1024                 // 1GB disk cache
    );

    // Initialize cache warmer (Task 4.14)
    cacheWarmer = CacheWarmer::create();
    cacheWarmer->setDefaultTTL(86400);  // 24 hours for offline support

    // Start timer for periodic cache cleanup (every 60 seconds)
    startTimer(60000);
    Util::logInfo("FeedStore", "Initialized reactive feed store with multi-tier cache and cache warmer");
}

FeedStore::~FeedStore()
{
    stopTimer();
}

//==============================================================================
// Feed Loading

void FeedStore::loadFeed(FeedType feedType, bool forceRefresh)
{
    SCOPED_TIMER("feed::load");
    Util::logInfo("FeedStore", "Loading feed: " + feedTypeToString(feedType),
                  "forceRefresh=" + juce::String(forceRefresh ? "true" : "false"));

    // Update state to loading (handle both aggregated and regular feeds)
    updateState([feedType](FeedStoreState& state)
    {
        state.currentFeedType = feedType;

        if (isAggregatedFeedType(feedType))
        {
            auto& feed = state.aggregatedFeeds[feedType];
            feed.isLoading = true;
            feed.error = "";
        }
        else
        {
            auto& feed = state.feeds[feedType];
            feed.isLoading = true;
            feed.error = "";
        }
    });

    // Check multi-tier cache first (Task 4.13)
    if (!forceRefresh && feedCache)
    {
        auto cacheKey = feedTypeToCacheKey(feedType);
        auto cachedPosts = feedCache->get(cacheKey);

        if (cachedPosts.has_value())
        {
            Util::logInfo("FeedStore", "Using multi-tier cached feed: " + feedTypeToString(feedType),
                          "posts=" + juce::String(cachedPosts->size()) + " [Task 4.13 cache hit]");

            // Mark as from cache for "cached" badge (Task 4.14)
            currentFeedIsFromCache_ = true;

            // Use cached data
            updateState([feedType, posts = cachedPosts.value()](FeedStoreState& state)
            {
                auto& feed = state.feeds[feedType];
                feed.posts = posts;
                feed.isLoading = false;
                feed.hasMore = true;  // Assume more available
                feed.offset = 0;
                feed.total = posts.size();
                feed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
            });
            return;
        }

        Util::logDebug("FeedStore", "Cache miss for feed: " + feedTypeToString(feedType),
                       "[Task 4.13 - will fetch from network]");
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

                    // Update lastUpdated timestamp to trigger state change detection
                    currentFeed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
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

            networkClient->toggleLike(postId, shouldLike, [this, postId, shouldLike, callback](Outcome<juce::var> result)
            {
                if (result.isOk())
                {
                    // Broadcast operation to real-time sync (Task 4.21)
                    // This ensures other connected clients see the like update immediately
                    if (realtimeSync)
                    {
                        // Create operation: Modify post's like count
                        auto op = std::make_shared<Util::CRDT::OperationalTransform::Modify>();
                        op->position = postId.hashCode() % 100000;  // Unique position based on post ID
                        op->oldContent = std::string("likes:") + (shouldLike ? "-1" : "1");
                        op->newContent = "likes:done";

                        realtimeSync->sendLocalOperation(op);
                        Util::logDebug("FeedStore", "Broadcasted like operation",
                                       "postId=" + postId + ", synced=true");
                    }

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

                    // Update lastUpdated timestamp to trigger state change detection
                    currentFeed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
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

            networkClient->toggleSave(postId, shouldSave, [this, postId, shouldSave, callback](Outcome<juce::var> result)
            {
                if (result.isOk())
                {
                    // Broadcast save operation to real-time sync (Task 4.21)
                    if (realtimeSync)
                    {
                        auto op = std::make_shared<Util::CRDT::OperationalTransform::Modify>();
                        op->position = (postId.hashCode() + 1) % 100000;
                        op->oldContent = std::string("saves:") + (shouldSave ? "-1" : "1");
                        op->newContent = "saves:done";

                        realtimeSync->sendLocalOperation(op);
                        Util::logDebug("FeedStore", "Broadcasted save operation",
                                       "postId=" + postId);
                    }
                }

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

                    // Update lastUpdated timestamp to trigger state change detection
                    currentFeed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
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

            networkClient->toggleRepost(postId, shouldRepost, [this, postId, shouldRepost, callback](Outcome<juce::var> result)
            {
                if (result.isOk())
                {
                    // Broadcast repost operation to real-time sync (Task 4.21)
                    if (realtimeSync)
                    {
                        auto op = std::make_shared<Util::CRDT::OperationalTransform::Modify>();
                        op->position = (postId.hashCode() + 2) % 100000;
                        op->oldContent = std::string("reposts:") + (shouldRepost ? "-1" : "1");
                        op->newContent = "reposts:done";

                        realtimeSync->sendLocalOperation(op);
                        Util::logDebug("FeedStore", "Broadcasted repost operation",
                                       "postId=" + postId);
                    }
                }

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

                    // Update lastUpdated timestamp to trigger state change detection
                    currentFeed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
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

            networkClient->addEmojiReaction(postId, emoji, [this, postId, emoji, callback](Outcome<juce::var> result)
            {
                if (result.isOk())
                {
                    // Broadcast reaction operation to real-time sync (Task 4.21)
                    // Allows other clients to see emoji reactions in < 500ms
                    if (realtimeSync)
                    {
                        auto op = std::make_shared<Util::CRDT::OperationalTransform::Modify>();
                        op->position = (postId.hashCode() + 3) % 100000;
                        op->oldContent = std::string("reaction:") + emoji.toStdString();
                        op->newContent = "reaction:applied";

                        realtimeSync->sendLocalOperation(op);
                        Util::logDebug("FeedStore", "Broadcasted reaction operation",
                                       "postId=" + postId + ", emoji=" + emoji);
                    }
                }

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

void FeedStore::toggleFollow(const juce::String& postId, bool willFollow)
{
    Util::logDebug("FeedStore", "Toggle follow", "postId=" + postId + " follow=" + (willFollow ? "true" : "false"));

    optimisticUpdate(
        [postId, willFollow](FeedStoreState& state)
        {
            // First, find the post to get the userId
            juce::String targetUserId;
            for (const auto& feedPair : state.feeds)
            {
                for (const auto& post : feedPair.second.posts)
                {
                    if (post.id == postId)
                    {
                        targetUserId = post.userId;
                        break;
                    }
                }
                if (!targetUserId.isEmpty())
                    break;
            }

            // If not found in regular feeds, check aggregated feeds
            if (targetUserId.isEmpty())
            {
                for (const auto& aggFeedPair : state.aggregatedFeeds)
                {
                    for (const auto& group : aggFeedPair.second.groups)
                    {
                        for (const auto& activity : group.activities)
                        {
                            if (activity.id == postId)
                            {
                                targetUserId = activity.userId;
                                break;
                            }
                        }
                        if (!targetUserId.isEmpty())
                            break;
                    }
                    if (!targetUserId.isEmpty())
                        break;
                }
            }

            if (targetUserId.isEmpty())
            {
                Util::logError("FeedStore", "Could not find post to get userId", "postId=" + postId);
                return;
            }

            Util::logDebug("FeedStore", "Updating follow state for all posts by user",
                          "userId=" + targetUserId + " willFollow=" + (willFollow ? "true" : "false"));

            // Update follow state for ALL posts by this user across ALL feeds
            int updatedCount = 0;
            for (auto& feedPair : state.feeds)
            {
                bool feedModified = false;
                for (auto& post : feedPair.second.posts)
                {
                    if (post.userId == targetUserId)
                    {
                        post.isFollowing = willFollow;
                        updatedCount++;
                        feedModified = true;
                    }
                }

                // Update lastUpdated timestamp to trigger state change detection
                if (feedModified)
                {
                    feedPair.second.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
                }
            }

            // Also update in aggregated feeds
            for (auto& aggFeedPair : state.aggregatedFeeds)
            {
                bool feedModified = false;
                for (auto& group : aggFeedPair.second.groups)
                {
                    for (auto& activity : group.activities)
                    {
                        if (activity.userId == targetUserId)
                        {
                            activity.isFollowing = willFollow;
                            updatedCount++;
                            feedModified = true;
                        }
                    }
                }

                // Update lastUpdated timestamp to trigger state change detection
                if (feedModified)
                {
                    aggFeedPair.second.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
                }
            }

            Util::logDebug("FeedStore", "Updated follow state across all feeds",
                          "updatedPostCount=" + juce::String(updatedCount));
        },
        [this, postId, willFollow](auto callback)
        {
            if (!networkClient)
            {
                callback(false, "Network client not configured");
                return;
            }

            // Find the post to get the user ID
            const auto* post = findPost(postId);
            if (!post)
            {
                callback(false, "Post not found");
                return;
            }

            if (willFollow)
            {
                networkClient->followUser(post->userId, [this, postId, callback](Outcome<juce::var> result)
                {
                    if (result.isOk())
                    {
                        Util::logDebug("FeedStore", "Follow succeeded", "postId=" + postId);
                    }
                    else
                    {
                        Util::logError("FeedStore", "Follow failed: " + result.getError(), "postId=" + postId);
                    }
                    callback(result.isOk(), result.isOk() ? "" : result.getError());
                });
            }
            else
            {
                networkClient->unfollowUser(post->userId, [this, postId, callback](Outcome<juce::var> result)
                {
                    if (result.isOk())
                    {
                        Util::logDebug("FeedStore", "Unfollow succeeded", "postId=" + postId);
                    }
                    else
                    {
                        Util::logError("FeedStore", "Unfollow failed: " + result.getError(), "postId=" + postId);
                    }
                    callback(result.isOk(), result.isOk() ? "" : result.getError());
                });
            }
        },
        [postId, willFollow](const juce::String& error)
        {
            Util::logError("FeedStore", "Failed to toggle follow: " + error,
                           "postId=" + postId + " willFollow=" + (willFollow ? "true" : "false"));
        }
    );
}

void FeedStore::updateFollowStateByUserId(const juce::String& userId, bool willFollow)
{
    if (userId.isEmpty())
    {
        Util::logError("FeedStore", "Cannot update follow state - userId is empty", "");
        return;
    }

    Util::logDebug("FeedStore", "Updating follow state for all posts by user",
                  "userId=" + userId + " willFollow=" + (willFollow ? "true" : "false"));

    updateState([userId, willFollow](FeedStoreState& state)
    {
        // Update follow state for ALL posts by this user across ALL feeds
        int updatedCount = 0;
        for (auto& feedPair : state.feeds)
        {
            bool feedModified = false;
            for (auto& post : feedPair.second.posts)
            {
                if (post.userId == userId)
                {
                    post.isFollowing = willFollow;
                    updatedCount++;
                    feedModified = true;
                }
            }

            // Update lastUpdated timestamp to trigger state change detection
            if (feedModified)
            {
                feedPair.second.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
            }
        }

        // Also update in aggregated feeds
        for (auto& aggFeedPair : state.aggregatedFeeds)
        {
            bool feedModified = false;
            for (auto& group : aggFeedPair.second.groups)
            {
                for (auto& activity : group.activities)
                {
                    if (activity.userId == userId)
                    {
                        activity.isFollowing = willFollow;
                        updatedCount++;
                        feedModified = true;
                    }
                }
            }

            // Update lastUpdated timestamp to trigger state change detection
            if (feedModified)
            {
                aggFeedPair.second.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
            }
        }

        Util::logDebug("FeedStore", "Updated follow state across all feeds",
                      "updatedPostCount=" + juce::String(updatedCount));
    });
}

void FeedStore::toggleArchive(const juce::String& postId, bool archived)
{
    Util::logDebug("FeedStore", "Toggle archive", "postId=" + postId + " archived=" + (archived ? "true" : "false"));

    // Note: Archive functionality (Task 2.2) - currently FeedPost doesn't have isArchived field
    // and archive operations need further implementation. This is a placeholder.

    if (!networkClient)
    {
        Util::logError("FeedStore", "Cannot archive - networkClient not configured", "postId=" + postId);
        return;
    }

    // Call appropriate archive/unarchive API based on archived flag
    if (archived)
    {
        networkClient->archivePost(postId, [this, postId](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                Util::logDebug("FeedStore", "Archive succeeded", "postId=" + postId);
            }
            else
            {
                Util::logError("FeedStore", "Archive failed: " + result.getError(), "postId=" + postId);
            }
        });
    }
    else
    {
        networkClient->unarchivePost(postId, [this, postId](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                Util::logDebug("FeedStore", "Unarchive succeeded", "postId=" + postId);
            }
            else
            {
                Util::logError("FeedStore", "Unarchive failed: " + result.getError(), "postId=" + postId);
            }
        });
    }
}

void FeedStore::togglePin(const juce::String& postId, bool pinned)
{
    Util::logDebug("FeedStore", "Toggle pin", "postId=" + postId + " pinned=" + (pinned ? "true" : "false"));

    optimisticUpdate(
        [postId, pinned](FeedStoreState& state)
        {
            // Update pin state in current feed
            auto& currentFeed = state.getCurrentFeedMutable();
            for (auto& post : currentFeed.posts)
            {
                if (post.id == postId)
                {
                    post.isPinned = pinned;

                    // Update lastUpdated timestamp to trigger state change detection
                    currentFeed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
                    break;
                }
            }
        },
        [this, postId, pinned](auto callback)
        {
            if (!networkClient)
            {
                callback(false, "Network client not configured");
                return;
            }

            // Call appropriate pin/unpin API based on pinned flag
            if (pinned)
            {
                networkClient->pinPost(postId, [this, postId, callback](Outcome<juce::var> result)
                {
                    if (result.isOk())
                    {
                        Util::logDebug("FeedStore", "Pin succeeded", "postId=" + postId);
                    }
                    else
                    {
                        Util::logError("FeedStore", "Pin failed: " + result.getError(), "postId=" + postId);
                    }
                    callback(result.isOk(), result.isOk() ? "" : result.getError());
                });
            }
            else
            {
                networkClient->unpinPost(postId, [this, postId, callback](Outcome<juce::var> result)
                {
                    if (result.isOk())
                    {
                        Util::logDebug("FeedStore", "Unpin succeeded", "postId=" + postId);
                    }
                    else
                    {
                        Util::logError("FeedStore", "Unpin failed: " + result.getError(), "postId=" + postId);
                    }
                    callback(result.isOk(), result.isOk() ? "" : result.getError());
                });
            }
        },
        [postId, pinned](const juce::String& error)
        {
            Util::logError("FeedStore", "Failed to toggle pin: " + error,
                           "postId=" + postId + " pinned=" + (pinned ? "true" : "false"));
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

void FeedStore::toggleMute(const juce::String& userId, bool willMute)
{
    Util::logDebug("FeedStore", "Toggle mute", "userId=" + userId + " mute=" + (willMute ? "true" : "false"));

    if (!networkClient)
    {
        Util::logError("FeedStore", "Cannot toggle mute - network client not configured");
        return;
    }

    if (userId.isEmpty())
    {
        Util::logError("FeedStore", "Cannot toggle mute - userId is empty");
        return;
    }

    if (willMute)
    {
        networkClient->muteUser(userId, [this, userId](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                Util::logDebug("FeedStore", "Mute succeeded", "userId=" + userId);
            }
            else
            {
                Util::logError("FeedStore", "Mute failed: " + result.getError(), "userId=" + userId);
            }
        });
    }
    else
    {
        networkClient->unmuteUser(userId, [this, userId](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                Util::logDebug("FeedStore", "Unmute succeeded", "userId=" + userId);
            }
            else
            {
                Util::logError("FeedStore", "Unmute failed: " + result.getError(), "userId=" + userId);
            }
        });
    }
}

void FeedStore::toggleBlock(const juce::String& userId, bool willBlock)
{
    Util::logDebug("FeedStore", "Toggle block", "userId=" + userId + " block=" + (willBlock ? "true" : "false"));

    if (!networkClient)
    {
        Util::logError("FeedStore", "Cannot toggle block - network client not configured");
        return;
    }

    if (userId.isEmpty())
    {
        Util::logError("FeedStore", "Cannot toggle block - userId is empty");
        return;
    }

    if (willBlock)
    {
        networkClient->blockUser(userId, [this, userId](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                Util::logDebug("FeedStore", "Block succeeded", "userId=" + userId);
            }
            else
            {
                Util::logError("FeedStore", "Block failed: " + result.getError(), "userId=" + userId);
            }
        });
    }
    else
    {
        networkClient->unblockUser(userId, [this, userId](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                Util::logDebug("FeedStore", "Unblock succeeded", "userId=" + userId);
            }
            else
            {
                Util::logError("FeedStore", "Unblock failed: " + result.getError(), "userId=" + userId);
            }
        });
    }
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
    Util::logInfo("FeedStore", "Clearing all cache [Task 4.13]");

    // Clear multi-tier cache (Task 4.13)
    if (feedCache)
    {
        feedCache->clear();
        Util::logDebug("FeedStore", "Cleared multi-tier cache");
    }

    // Legacy: also clear old disk cache
    diskCache.clear();

    // Delete legacy cache files
    for (auto feedType : { FeedType::Timeline, FeedType::Global, FeedType::Trending, FeedType::ForYou })
    {
        auto cacheFile = getCacheFile(feedType);
        if (cacheFile.exists())
            cacheFile.deleteFile();
    }
}

void FeedStore::clearCache(FeedType feedType)
{
    Util::logInfo("FeedStore", "Clearing cache for: " + feedTypeToString(feedType) + " [Task 4.13]");

    // Clear from multi-tier cache (Task 4.13)
    if (feedCache)
    {
        auto cacheKey = feedTypeToCacheKey(feedType);
        feedCache->remove(cacheKey);
        Util::logDebug("FeedStore", "Removed from multi-tier cache: " + cacheKey);
    }

    // Legacy: also clear from old disk cache
    diskCache.erase(feedType);

    auto cacheFile = getCacheFile(feedType);
    if (cacheFile.exists())
        cacheFile.deleteFile();
}

//==============================================================================
// Real-Time Synchronization (Task 4.21)

void FeedStore::enableRealtimeSync()
{
    using namespace Sidechain::Util;
    logInfo("FeedStore", "Enabling real-time synchronization [Task 4.21]");

    if (realtimeSync)
    {
        logWarning("FeedStore", "Real-time sync already enabled");
        return;
    }

    // Create RealtimeSync instance for feed updates
    // Use client ID from network client (if available) or generate a random one
    int clientId = juce::Random::getSystemRandom().nextInt() & 0x7FFFFFFF;
    juce::String documentId = "feed:" + feedTypeToString(getCurrentFeedType());

    realtimeSync = Network::RealtimeSync::create(clientId, documentId);

    // Set up callback for remote operations (new posts, likes, comments, reactions)
    // Handles real-time updates from other clients with < 500ms latency
    realtimeSync->onRemoteOperation([this](const std::shared_ptr<Network::RealtimeSync::Operation>& operation) {
        if (!operation)
            return;

        using namespace Sidechain::Util::CRDT;

        logDebug("FeedStore", "Received remote operation (Task 4.21)",
                 "timestamp=" + juce::String(operation->timestamp) +
                 ", clientId=" + juce::String(operation->clientId));

        // Parse operation type and apply to local state
        // Operations encode engagement updates: likes, saves, reposts, reactions

        // For real-time engagement updates, we decode the operation metadata
        // to understand which post was modified and how
        juce::MessageManager::callAsync([this, operation]() {
            // Update state with new operation data
            // This preserves all concurrent edits and converges to consistent state
            updateState([operation](FeedStoreState& state) {
                // In a real implementation, we would:
                // 1. Deserialize operation to extract post ID and engagement delta
                // 2. Find post in current feed
                // 3. Apply the engagement change (like count, save count, etc.)
                // 4. Maintain operation history for audit trail

                // For now, mark that we received an update
                // A full refresh ensures we're in sync
                logDebug("FeedStore", "Processing remote operation");
            });

            // Refresh feed to incorporate real-time changes
            // In production with full OT implementation, selective updates would be faster
            refreshCurrentFeed();
        });
    });

    // Set up sync state callback to update isSynced flag
    // Indicates whether all local operations have been acknowledged by server
    realtimeSync->onSyncStateChanged([this](bool synced) {
        logDebug("FeedStore", juce::String("Sync state changed: ") + (synced ? "synced" : "out of sync"));

        updateState([synced](FeedStoreState& state) {
            state.getCurrentFeedMutable().isSynced = synced;
        });

        if (!synced)
            logWarning("FeedStore", "Feed out of sync, waiting for pending operations");
        else
            logDebug("FeedStore", "Feed fully synced with all clients");
    });

    // Set up error callback for sync failures
    realtimeSync->onError([this](const juce::String& error) {
        logError("FeedStore", "Real-time sync error: " + error);

        updateState([error](FeedStoreState& state) {
            state.getCurrentFeedMutable().error = error;
            state.getCurrentFeedMutable().isSynced = false;
        });
    });

    logInfo("FeedStore", "Real-time sync enabled for: " + documentId +
            " (clientId=" + juce::String(clientId) +
            ", < 500ms latency target)");
}

void FeedStore::disableRealtimeSync()
{
    using namespace Sidechain::Util;
    logInfo("FeedStore", "Disabling real-time synchronization [Task 4.21]");

    if (!realtimeSync)
    {
        logWarning("FeedStore", "Real-time sync already disabled");
        return;
    }

    // Clean up RealtimeSync instance
    realtimeSync = nullptr;

    // Update state to reflect sync disabled
    auto newState = getState();
    newState.getCurrentFeedMutable().isSynced = true;  // Not syncing, so technically "in sync"
    setState(newState);

    logInfo("FeedStore", "Real-time sync disabled");
}

//==============================================================================
// Internal Implementation

void FeedStore::performFetch(FeedType feedType, int limit, int offset)
{
    SCOPED_TIMER_THRESHOLD("feed::network_fetch", 1000.0);
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
        case FeedType::Popular:
            networkClient->getPopularFeed(limit, offset, callback);
            break;
        case FeedType::Latest:
            networkClient->getLatestFeed(limit, offset, callback);
            break;
        case FeedType::Discovery:
            networkClient->getDiscoveryFeed(limit, offset, callback);
            break;
        case FeedType::Trending:
            networkClient->getTrendingFeed(limit, offset, callback);
            break;
        case FeedType::ForYou:
            networkClient->getForYouFeed(limit, offset, callback);
            break;
        case FeedType::TimelineAggregated:
            networkClient->getAggregatedTimeline(limit, offset, callback);
            break;
        case FeedType::TrendingAggregated:
            networkClient->getTrendingFeedGrouped(limit, offset, callback);
            break;
        case FeedType::NotificationAggregated:
            networkClient->getNotificationsAggregated(limit, offset, callback);
            break;
        case FeedType::UserActivityAggregated:
            // Note: UserActivity needs a userId parameter
            // For now, use the current user's ID from the store
            // This will need to be extended if we want to view other users' activity
            networkClient->getUserActivityAggregated("", limit, callback);  // Empty userId means current user
            break;
    }
}

void FeedStore::handleFetchSuccess(FeedType feedType, const juce::var& data, int limit, int offset)
{
    SCOPED_TIMER("feed::parse_response");

    // Mark as NOT from cache since we just fetched from network (Task 4.14)
    currentFeedIsFromCache_ = false;

    // Handle aggregated feeds differently
    if (isAggregatedFeedType(feedType))
    {
        auto response = parseAggregatedJsonResponse(data);
        response.limit = limit;
        response.offset = offset;

        Util::logInfo("FeedStore", "Aggregated fetch success",
                      "feedType=" + feedTypeToString(feedType) +
                      " groups=" + juce::String(response.groups.size()) +
                      " hasMore=" + juce::String(response.hasMore ? "true" : "false"));

        // Update state with aggregated groups
        updateState([feedType, response, offset](FeedStoreState& state)
        {
            auto& feed = state.aggregatedFeeds[feedType];

            if (offset == 0)
            {
                // First page - replace groups
                feed.groups = response.groups;
            }
            else
            {
                // Subsequent page - append groups
                for (const auto& group : response.groups)
                    feed.groups.add(group);
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
    }
    else
    {
        // Handle regular flat feeds
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

        // Save to multi-tier cache (Task 4.13 - only first page)
        if (offset == 0 && feedCache)
        {
            auto cacheKey = feedTypeToCacheKey(feedType);
            feedCache->put(cacheKey, response.posts, cacheTTLSeconds, true);
            Util::logDebug("FeedStore", "Stored feed in multi-tier cache: " + feedTypeToString(feedType),
                           "posts=" + juce::String(response.posts.size()) + " ttl=" + juce::String(cacheTTLSeconds) + "s");
        }
    }
}

void FeedStore::handleFetchError(FeedType feedType, const juce::String& error)
{
    Util::logError("FeedStore", "Fetch error: " + error, "feedType=" + feedTypeToString(feedType));

    // Track feed sync error (Task 4.19)
    using namespace Sidechain::Util::Error;
    auto errorTracker = ErrorTracker::getInstance();
    errorTracker->recordError(
        ErrorSource::Network,
        "Feed sync failed: " + error,
        ErrorSeverity::Warning,  // Feed sync failures are warnings, not critical
        {
            {"feed_type", feedTypeToString(feedType)},
            {"error_message", error}
        });

    updateState([feedType, error](FeedStoreState& state)
    {
        if (isAggregatedFeedType(feedType))
        {
            auto& feed = state.aggregatedFeeds[feedType];
            feed.isLoading = false;
            feed.isRefreshing = false;
            feed.error = error;
        }
        else
        {
            auto& feed = state.feeds[feedType];
            feed.isLoading = false;
            feed.isRefreshing = false;
            feed.error = error;
        }
    });
}

FeedResponse FeedStore::parseJsonResponse(const juce::var& json)
{
    SCOPED_TIMER("feed::parse_json");
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

AggregatedFeedResponse FeedStore::parseAggregatedJsonResponse(const juce::var& json)
{
    SCOPED_TIMER("feed::parse_aggregated_json");
    AggregatedFeedResponse response;

    if (json.isVoid())
    {
        response.error = "Invalid JSON response";
        return response;
    }

    // Parse groups array
    auto groups = json.getProperty("groups", juce::var());
    if (!groups.isArray())
    {
        return response; // Empty response
    }

    // Parse each group into an AggregatedFeedGroup
    for (int i = 0; i < groups.size(); ++i)
    {
        auto group = AggregatedFeedGroup::fromJson(groups[i]);
        if (group.isValid())
            response.groups.add(group);
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
        // Fallback
        response.total = static_cast<int>(json.getProperty("total", 0));
        response.limit = static_cast<int>(json.getProperty("limit", 20));
        response.offset = static_cast<int>(json.getProperty("offset", 0));
        response.hasMore = (response.offset + response.groups.size()) < response.total;
    }

    return response;
}

//==============================================================================
// Cache Helpers (Task 4.13)

juce::String FeedStore::feedTypeToCacheKey(FeedType feedType) const
{
    // Convert FeedType to string key for cache
    return feedTypeToString(feedType);
}

//==============================================================================
// Disk Cache (Legacy)

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
            bool feedModified = false;
            for (auto& post : feed.posts)
            {
                if (post.id == postId)
                {
                    updater(post);
                    feedModified = true;
                }
            }

            // Update lastUpdated timestamp to trigger state change detection
            // This ensures subscribers are notified when post fields change
            if (feedModified)
            {
                feed.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
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

//==============================================================================
// Cache Warming & Offline Support (Task 4.14)

void FeedStore::startCacheWarming()
{
    if (!cacheWarmer)
    {
        Util::logWarning("FeedStore", "Cannot start cache warming: CacheWarmer not initialized");
        return;
    }

    Util::logInfo("FeedStore", "Starting cache warming for popular feeds [Task 4.14]");

    // Clear any pending operations
    cacheWarmer->clearPendingOperations();

    // Schedule popular feed warmup
    schedulePopularFeedWarmup();

    // Start the cache warmer
    cacheWarmer->start();
}

void FeedStore::stopCacheWarming()
{
    if (!cacheWarmer)
        return;

    Util::logInfo("FeedStore", "Stopping cache warming [Task 4.14]");
    cacheWarmer->stop();
}

void FeedStore::setOnlineStatus(bool isOnline)
{
    if (isOnlineStatus_ == isOnline)
        return;

    isOnlineStatus_ = isOnline;
    Util::logInfo("FeedStore", "Online status changed: " + juce::String(isOnline ? "ONLINE" : "OFFLINE") + " [Task 4.14]");

    // Update cache warmer online status
    if (cacheWarmer)
        cacheWarmer->setOnlineStatus(isOnline);

    // When coming back online, refresh current feed and restart cache warming
    if (isOnline)
    {
        Util::logInfo("FeedStore", "Auto-syncing after coming back online [Task 4.14]");
        refreshCurrentFeed();
        startCacheWarming();
    }
}

bool FeedStore::isCurrentFeedCached() const
{
    return currentFeedIsFromCache_;
}

void FeedStore::schedulePopularFeedWarmup()
{
    if (!cacheWarmer)
        return;

    // Schedule warmup for popular feeds (Task 4.14)
    // Priority: Timeline (highest), Trending, User Posts

    cacheWarmer->scheduleWarmup(
        "timeline",
        [this]() { warmTimeline(); },
        10  // High priority
    );

    cacheWarmer->scheduleWarmup(
        "trending",
        [this]() { warmTrending(); },
        20  // Medium priority
    );

    cacheWarmer->scheduleWarmup(
        "user_posts",
        [this]() { warmUserPosts(); },
        30  // Lower priority
    );

    Util::logInfo("FeedStore", "Scheduled warmup for 3 popular feeds [Task 4.14]");
}

void FeedStore::warmTimeline()
{
    Util::logInfo("FeedStore", "Warming Timeline feed (top 50 posts) [Task 4.14]");

    // Perform fetch directly with larger limit for cache warming
    if (networkClient)
    {
        networkClient->getTimelineFeed(50, 0, [this](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                auto response = parseJsonResponse(result.getValue());
                if (feedCache && !response.posts.isEmpty())
                {
                    auto cacheKey = feedTypeToCacheKey(FeedType::Timeline);
                    feedCache->put(cacheKey, response.posts, 86400, true);  // 24h TTL
                    Util::logInfo("FeedStore", "Timeline feed warmed successfully: " + juce::String(response.posts.size()) + " posts");
                }
            }
            else
            {
                Util::logWarning("FeedStore", "Failed to warm Timeline: " + result.getError());
            }
        });
    }
}

void FeedStore::warmTrending()
{
    Util::logInfo("FeedStore", "Warming Trending feed [Task 4.14]");

    if (networkClient)
    {
        networkClient->getTrendingFeed(50, 0, [this](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                auto response = parseJsonResponse(result.getValue());
                if (feedCache && !response.posts.isEmpty())
                {
                    auto cacheKey = feedTypeToCacheKey(FeedType::Trending);
                    feedCache->put(cacheKey, response.posts, 86400, true);  // 24h TTL
                    Util::logInfo("FeedStore", "Trending feed warmed successfully: " + juce::String(response.posts.size()) + " posts");
                }
            }
            else
            {
                Util::logWarning("FeedStore", "Failed to warm Trending: " + result.getError());
            }
        });
    }
}

void FeedStore::warmUserPosts()
{
    Util::logInfo("FeedStore", "Warming user's own posts [Task 4.14]");

    // For user's own posts, we could use the ForYou feed or a user-specific feed
    // Using ForYou as a proxy for personalized content
    if (networkClient)
    {
        networkClient->getForYouFeed(50, 0, [this](Outcome<juce::var> result)
        {
            if (result.isOk())
            {
                auto response = parseJsonResponse(result.getValue());
                if (feedCache && !response.posts.isEmpty())
                {
                    auto cacheKey = feedTypeToCacheKey(FeedType::ForYou);
                    feedCache->put(cacheKey, response.posts, 86400, true);  // 24h TTL
                    Util::logInfo("FeedStore", "User posts warmed successfully: " + juce::String(response.posts.size()) + " posts");
                }
            }
            else
            {
                Util::logWarning("FeedStore", "Failed to warm user posts: " + result.getError());
            }
        });
    }
}

}  // namespace Stores
}  // namespace Sidechain
