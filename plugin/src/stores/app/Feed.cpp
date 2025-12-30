#include "../AppStore.h"
#include "../../network/NetworkClient.h"
#include "../util/StoreUtils.h"
#include "../util/PostInteractionHelper.h"
#include "../../util/logging/Logger.h"
#include "../../util/rx/JuceScheduler.h"
#include <nlohmann/json.hpp>

namespace Sidechain {
namespace Stores {

using Utils::JsonArrayParser;
using Utils::NetworkClientGuard;
using Utils::PostInteractionHelper;

// ==============================================================================
// Helper Functions

static inline bool isAggregatedFeedType(FeedType feedType) {
  return feedType == FeedType::TimelineAggregated || feedType == FeedType::TrendingAggregated ||
         feedType == FeedType::NotificationAggregated || feedType == FeedType::UserActivityAggregated;
}

static inline juce::String feedTypeToString(FeedType feedType) {
  switch (feedType) {
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

// ==============================================================================
// Feed Loading

void AppStore::loadFeed(FeedType feedType, [[maybe_unused]] bool forceRefresh) {
  if (!networkClient) {
    PostsState newState = stateManager.posts->getState();
    newState.currentFeedType = feedType;
    newState.feedError = "Network client not initialized";
    stateManager.posts->setState(newState);
    return;
  }

  Util::logInfo("AppStore", "Loading feed", "feedType=" + feedTypeToString(feedType));

  // Note: We always fetch fresh data on network, don't use stale cache.
  // Removed cache check - always fetch from network for latest content.

  // Update state to loading
  PostsState loadingState = stateManager.posts->getState();
  loadingState.currentFeedType = feedType;
  if (loadingState.feeds.count(feedType) == 0) {
    loadingState.feeds[feedType] = FeedState();
  }
  loadingState.feeds[feedType].isLoading = true;
  loadingState.feeds[feedType].error = "";
  stateManager.posts->setState(loadingState);

  // Fetch from network
  performFetch(feedType, 20, 0);
}

void AppStore::refreshCurrentFeed() {
  auto currentState = stateManager.posts->getState();
  loadFeed(currentState.currentFeedType, true);
}

void AppStore::loadMore() {
  auto currentState = stateManager.posts->getState();
  auto feedType = currentState.currentFeedType;

  if (currentState.feeds.count(feedType) == 0) {
    Util::logWarning("AppStore", "Cannot load more - feed not initialized");
    return;
  }

  auto &feedState = currentState.feeds.at(feedType);
  if (!feedState.hasMore || feedState.isLoading || !networkClient) {
    return;
  }

  PostsState loadingState = stateManager.posts->getState();
  if (loadingState.feeds.count(feedType) > 0) {
    loadingState.feeds[feedType].isLoading = true;
  }
  stateManager.posts->setState(loadingState);

  performFetch(feedType, feedState.limit, feedState.offset);
}

void AppStore::switchFeedType(FeedType feedType) {
  PostsState newState = stateManager.posts->getState();
  newState.currentFeedType = feedType;
  stateManager.posts->setState(newState);

  // Load the new feed if not already loaded
  // Use .find() instead of [] to avoid implicitly creating empty map entries
  auto currentState = stateManager.posts->getState();
  auto feedIt = currentState.feeds.find(feedType);
  if (feedIt == currentState.feeds.end() || feedIt->second.posts.empty()) {
    loadFeed(feedType, false);
  }
}

// ==============================================================================
// Saved Posts

void AppStore::loadSavedPosts() {
  if (!networkClient) {
    PostsState errorState = stateManager.posts->getState();
    errorState.savedPosts.error = "Network client not initialized";
    stateManager.posts->setState(errorState);
    return;
  }

  Util::logInfo("AppStore", "Loading saved posts");

  PostsState loadingState = stateManager.posts->getState();
  loadingState.savedPosts.isLoading = true;
  loadingState.savedPosts.offset = 0;
  loadingState.savedPosts.posts.clear();
  loadingState.savedPosts.error = "";
  stateManager.posts->setState(loadingState);

  networkClient->getSavedPostsObservable(20, 0).subscribe(
      [this](const std::vector<FeedPost> &posts) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<FeedPost>> postList;
        postList.reserve(posts.size());
        for (const auto &p : posts) {
          postList.push_back(std::make_shared<FeedPost>(p));
        }

        PostsState state = stateManager.posts->getState();
        state.savedPosts.posts = std::move(postList);
        state.savedPosts.isLoading = false;
        state.savedPosts.hasMore = posts.size() >= 20;
        state.savedPosts.offset = static_cast<int>(posts.size());
        state.savedPosts.error = "";
        stateManager.posts->setState(state);
        Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(posts.size())) + " saved posts");
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        PostsState state = stateManager.posts->getState();
        state.savedPosts.isLoading = false;
        state.savedPosts.error = errorMsg;
        stateManager.posts->setState(state);
        Util::logError("AppStore", "Failed to load saved posts: " + errorMsg);
      });
}

void AppStore::loadMoreSavedPosts() {
  auto currentState = stateManager.posts->getState();
  if (!currentState.savedPosts.hasMore || currentState.savedPosts.isLoading || !networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Loading more saved posts");

  PostsState loadingState = stateManager.posts->getState();
  loadingState.savedPosts.isLoading = true;
  stateManager.posts->setState(loadingState);

  networkClient->getSavedPostsObservable(currentState.savedPosts.limit, currentState.savedPosts.offset)
      .subscribe(
          [this](const std::vector<FeedPost> &posts) {
            // Convert to shared_ptr and append to existing
            PostsState state = stateManager.posts->getState();
            for (const auto &p : posts) {
              state.savedPosts.posts.push_back(std::make_shared<FeedPost>(p));
            }
            state.savedPosts.isLoading = false;
            state.savedPosts.hasMore = posts.size() >= static_cast<size_t>(state.savedPosts.limit);
            state.savedPosts.offset += static_cast<int>(posts.size());
            state.savedPosts.error = "";
            stateManager.posts->setState(state);
            Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(posts.size())) + " more saved posts");
          },
          [this](std::exception_ptr ep) {
            juce::String errorMsg;
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              errorMsg = e.what();
            }
            PostsState state = stateManager.posts->getState();
            state.savedPosts.isLoading = false;
            state.savedPosts.error = errorMsg;
            stateManager.posts->setState(state);
            Util::logError("AppStore", "Failed to load more saved posts: " + errorMsg);
          });
}

void AppStore::unsavePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Unsaving post: " + postId);

  // Optimistic removal from saved posts
  PostsState newState = stateManager.posts->getState();
  auto it = std::find_if(newState.savedPosts.posts.begin(), newState.savedPosts.posts.end(),
                         [&postId](const auto &post) { return post->id == postId; });
  if (it != newState.savedPosts.posts.end()) {
    newState.savedPosts.posts.erase(it);
  }
  stateManager.posts->setState(newState);

  // Send to server using observable
  networkClient->unsavePostObservable(postId).subscribe(
      [](int) { Util::logDebug("AppStore", "Post unsaved successfully"); },
      [this, postId](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          // Refresh on error to restore the post
          Util::logError("AppStore", "Failed to unsave post: " + juce::String(e.what()));
          loadSavedPosts();
        }
      });
}

// ==============================================================================
// Archived Posts

void AppStore::loadArchivedPosts() {
  if (!networkClient) {
    PostsState errorState = stateManager.posts->getState();
    errorState.archivedPosts.error = "Network client not initialized";
    stateManager.posts->setState(errorState);
    return;
  }

  Util::logInfo("AppStore", "Loading archived posts");

  PostsState loadingState = stateManager.posts->getState();
  loadingState.archivedPosts.isLoading = true;
  loadingState.archivedPosts.offset = 0;
  loadingState.archivedPosts.posts.clear();
  loadingState.archivedPosts.error = "";
  stateManager.posts->setState(loadingState);

  networkClient->getArchivedPostsObservable(20, 0).subscribe(
      [this](const std::vector<FeedPost> &posts) {
        // Convert to shared_ptr for state
        std::vector<std::shared_ptr<FeedPost>> postList;
        postList.reserve(posts.size());
        for (const auto &p : posts) {
          postList.push_back(std::make_shared<FeedPost>(p));
        }

        PostsState state = stateManager.posts->getState();
        state.archivedPosts.posts = std::move(postList);
        state.archivedPosts.isLoading = false;
        state.archivedPosts.hasMore = posts.size() >= 20;
        state.archivedPosts.offset = static_cast<int>(posts.size());
        state.archivedPosts.error = "";
        stateManager.posts->setState(state);
        Util::logInfo("AppStore", "Loaded " + juce::String(static_cast<int>(posts.size())) + " archived posts");
      },
      [this](std::exception_ptr ep) {
        juce::String errorMsg;
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          errorMsg = e.what();
        }
        PostsState state = stateManager.posts->getState();
        state.archivedPosts.isLoading = false;
        state.archivedPosts.error = errorMsg;
        stateManager.posts->setState(state);
        Util::logError("AppStore", "Failed to load archived posts: " + errorMsg);
      });
}

void AppStore::loadMoreArchivedPosts() {
  auto currentState = stateManager.posts->getState();
  if (!currentState.archivedPosts.hasMore || currentState.archivedPosts.isLoading || !networkClient) {
    return;
  }

  Util::logDebug("AppStore", "Loading more archived posts");

  PostsState loadingState = stateManager.posts->getState();
  loadingState.archivedPosts.isLoading = true;
  stateManager.posts->setState(loadingState);

  // Fetch archived posts from NetworkClient
  int offset = currentState.archivedPosts.offset;
  networkClient->getArchivedPostsObservable(20, offset)
      .subscribe(
          [this](const std::vector<FeedPost> &posts) {
            // Convert to shared_ptr and append to existing
            PostsState state = stateManager.posts->getState();
            for (const auto &p : posts) {
              state.archivedPosts.posts.push_back(std::make_shared<FeedPost>(p));
            }
            state.archivedPosts.isLoading = false;
            state.archivedPosts.hasMore = posts.size() >= 20;
            state.archivedPosts.offset += static_cast<int>(posts.size());
            state.archivedPosts.error = "";
            stateManager.posts->setState(state);
            Util::logInfo("AppStore",
                          "Loaded " + juce::String(static_cast<int>(posts.size())) + " more archived posts");
          },
          [this](std::exception_ptr ep) {
            juce::String errorMsg;
            try {
              std::rethrow_exception(ep);
            } catch (const std::exception &e) {
              errorMsg = e.what();
            }
            PostsState state = stateManager.posts->getState();
            state.archivedPosts.isLoading = false;
            state.archivedPosts.error = errorMsg;
            stateManager.posts->setState(state);
            Util::logError("AppStore", "Failed to load more archived posts: " + errorMsg);
          });
}

void AppStore::restorePost(const juce::String &postId) {
  if (!networkClient) {
    return;
  }

  Util::logInfo("AppStore", "Restoring post: " + postId);

  // Optimistic removal from archived posts
  PostsState newState = stateManager.posts->getState();
  auto it = std::find_if(newState.archivedPosts.posts.begin(), newState.archivedPosts.posts.end(),
                         [&postId](const auto &post) { return post->id == postId; });
  if (it != newState.archivedPosts.posts.end()) {
    newState.archivedPosts.posts.erase(it);
  }
  stateManager.posts->setState(newState);

  // Send to server using observable
  networkClient->unarchivePostObservable(postId).subscribe(
      [](const juce::var &) { Util::logDebug("AppStore", "Post restored successfully"); },
      [this](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          // Refresh on error to restore the post to the list
          Util::logError("AppStore", "Failed to restore post: " + juce::String(e.what()));
          loadArchivedPosts();
        }
      });
}

// ==============================================================================
// Post Interactions

void AppStore::toggleLike(const juce::String &postId) {
  if (!NetworkClientGuard::checkSilent(networkClient)) {
    return;
  }

  auto config = PostInteractionHelper::createLikeConfig(
      [this](const juce::String &id, bool wasLiked, std::function<void(Outcome<nlohmann::json>)> callback) {
        if (wasLiked) {
          networkClient->unlikePostObservable(id).subscribe(
              [callback](int) { callback(Outcome<nlohmann::json>::ok(nlohmann::json())); },
              [callback](std::exception_ptr ep) {
                try {
                  std::rethrow_exception(ep);
                } catch (const std::exception &e) {
                  callback(Outcome<nlohmann::json>::error(juce::String(e.what())));
                }
              });
        } else {
          networkClient->likePostObservable(id, "").subscribe(
              [callback](const NetworkClient::LikeResult &result) {
                // Convert LikeResult to nlohmann::json for callback interface
                nlohmann::json jsonResult;
                jsonResult["like_count"] = result.likeCount;
                jsonResult["is_liked"] = result.isLiked;
                callback(Outcome<nlohmann::json>::ok(jsonResult));
              },
              [callback](std::exception_ptr ep) {
                try {
                  std::rethrow_exception(ep);
                } catch (const std::exception &e) {
                  callback(Outcome<nlohmann::json>::error(juce::String(e.what())));
                }
              });
        }
      });

  PostInteractionHelper::performToggle(stateManager.posts, postId, config);
}

void AppStore::toggleSave(const juce::String &postId) {
  if (!NetworkClientGuard::checkSilent(networkClient)) {
    return;
  }

  auto config = PostInteractionHelper::createSaveConfig(
      [this](const juce::String &id, bool wasSaved, std::function<void(Outcome<nlohmann::json>)> callback) {
        auto observable = wasSaved ? networkClient->unsavePostObservable(id) : networkClient->savePostObservable(id);
        observable.subscribe([callback](int) { callback(Outcome<nlohmann::json>::ok(nlohmann::json())); },
                             [callback](std::exception_ptr ep) {
                               try {
                                 std::rethrow_exception(ep);
                               } catch (const std::exception &e) {
                                 callback(Outcome<nlohmann::json>::error(juce::String(e.what())));
                               }
                             });
      });

  PostInteractionHelper::performToggle(stateManager.posts, postId, config);
}

void AppStore::toggleRepost(const juce::String &postId) {
  if (!NetworkClientGuard::checkSilent(networkClient)) {
    return;
  }

  auto config = PostInteractionHelper::createRepostConfig(
      [this](const juce::String &id, bool wasReposted, std::function<void(Outcome<nlohmann::json>)> callback) {
        auto observable =
            wasReposted ? networkClient->undoRepostObservable(id) : networkClient->repostPostObservable(id, "");
        observable.subscribe([callback](int) { callback(Outcome<nlohmann::json>::ok(nlohmann::json())); },
                             [callback](std::exception_ptr ep) {
                               try {
                                 std::rethrow_exception(ep);
                               } catch (const std::exception &e) {
                                 callback(Outcome<nlohmann::json>::error(juce::String(e.what())));
                               }
                             });
      });

  PostInteractionHelper::performToggle(stateManager.posts, postId, config);
}

void AppStore::addReaction(const juce::String &postId, const juce::String &emoji) {
  if (!networkClient) {
    return;
  }

  // Add a reaction by liking with an emoji using observable
  networkClient->likePostObservable(postId, emoji)
      .subscribe([](const NetworkClient::LikeResult &) {},
                 [](std::exception_ptr ep) {
                   try {
                     std::rethrow_exception(ep);
                   } catch (const std::exception &e) {
                     Util::logError("AppStore", "Failed to add reaction: " + juce::String(e.what()));
                   }
                 });
}

void AppStore::toggleFollow(const juce::String &postId, bool willFollow) {
  if (!NetworkClientGuard::checkSilent(networkClient)) {
    return;
  }

  using Utils::FollowHelper;

  // Find user ID and current follow state
  auto currentState = stateManager.posts->getState();
  auto userInfo = FollowHelper::findUserAndFollowState(currentState, postId);

  if (!userInfo.has_value()) {
    return;
  }

  auto [userId, previousFollowState] = *userInfo;
  if (userId.isEmpty()) {
    return;
  }

  // Apply optimistic update
  PostsState newState = stateManager.posts->getState();
  FollowHelper::updateFollowState(newState, postId, willFollow);
  stateManager.posts->setState(newState);

  Util::logDebug("AppStore", "Follow post optimistic update: " + postId + " - " + (willFollow ? "follow" : "unfollow"));

  // Use observable for network request - separate branches due to different return types
  auto errorHandler = [this, postId, previousFollowState](std::exception_ptr ep) {
    try {
      std::rethrow_exception(ep);
    } catch (const std::exception &e) {
      Util::logError("AppStore", "Failed to " + juce::String(previousFollowState ? "unfollow" : "follow") +
                                     " user: " + juce::String(e.what()));
      PostsState rollbackState = stateManager.posts->getState();
      FollowHelper::updateFollowState(rollbackState, postId, previousFollowState);
      stateManager.posts->setState(rollbackState);
    }
  };

  if (willFollow) {
    networkClient->followUserObservable(userId).subscribe(
        [previousFollowState, postId](const NetworkClient::FollowResult &) {
          Util::logInfo("AppStore", "User followed successfully: " + postId);
        },
        errorHandler);
  } else {
    networkClient->unfollowUserObservable(userId).subscribe(
        [previousFollowState, postId](int) { Util::logInfo("AppStore", "User unfollowed successfully: " + postId); },
        errorHandler);
  }
}

void AppStore::toggleMute(const juce::String &userId, bool willMute) {
  if (!networkClient) {
    return;
  }

  auto observable = willMute ? networkClient->muteUserObservable(userId) : networkClient->unmuteUserObservable(userId);

  observable.subscribe(
      [](int) {
        // Invalidate feed caches on successful mute/unmute (feed may change)
      },
      [willMute](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Util::logError("AppStore", "Failed to " + juce::String(willMute ? "mute" : "unmute") +
                                         " user: " + juce::String(e.what()));
        }
      });
}

void AppStore::togglePin(const juce::String &postId, bool pinned) {
  if (!NetworkClientGuard::checkSilent(networkClient)) {
    return;
  }

  // Update UI optimistically
  PostsState newState = stateManager.posts->getState();
  PostInteractionHelper::updatePostAcrossCollections(
      newState, postId, [pinned](std::shared_ptr<FeedPost> &post) { post->isPinned = pinned; });
  stateManager.posts->setState(newState);

  // Call actual API to persist pin/unpin state using observable
  auto observable = pinned ? networkClient->pinPostObservable(postId) : networkClient->unpinPostObservable(postId);

  observable.subscribe([](const juce::var &) {},
                       [postId, pinned](std::exception_ptr ep) {
                         try {
                           std::rethrow_exception(ep);
                         } catch (const std::exception &e) {
                           Util::logError("AppStore", "Failed to " + juce::String(pinned ? "pin" : "unpin") + " post " +
                                                          postId + ": " + juce::String(e.what()));
                         }
                       });

  Util::logInfo("AppStore", pinned ? "Pin post: " + postId : "Unpin post: " + postId);
}

// ==============================================================================
// Helper Methods

void AppStore::performFetch(FeedType feedType, int limit, int offset) {
  Util::logDebug("AppStore", "performFetch called for feedType=" + feedTypeToString(feedType) +
                                 ", limit=" + juce::String(limit) + ", offset=" + juce::String(offset));

  if (!networkClient) {
    Util::logError("AppStore", "performFetch: networkClient is null!");
    return;
  }

  // Handle aggregated feeds separately (they still use callbacks)
  if (feedType == FeedType::TimelineAggregated) {
    networkClient->getAggregatedTimeline(limit, offset,
                                         [this, feedType, limit, offset](Outcome<nlohmann::json> result) {
                                           if (result.isOk()) {
                                             handleFetchSuccess(feedType, result.getValue(), limit, offset);
                                           } else {
                                             handleFetchError(feedType, result.getError());
                                           }
                                         });
    return;
  } else if (feedType == FeedType::TrendingAggregated) {
    networkClient->getTrendingFeedGrouped(limit, offset,
                                          [this, feedType, limit, offset](Outcome<nlohmann::json> result) {
                                            if (result.isOk()) {
                                              handleFetchSuccess(feedType, result.getValue(), limit, offset);
                                            } else {
                                              handleFetchError(feedType, result.getError());
                                            }
                                          });
    return;
  } else if (feedType == FeedType::NotificationAggregated) {
    networkClient->getNotificationsAggregated(limit, offset,
                                              [this, feedType, limit, offset](Outcome<nlohmann::json> result) {
                                                if (result.isOk()) {
                                                  handleFetchSuccess(feedType, result.getValue(), limit, offset);
                                                } else {
                                                  handleFetchError(feedType, result.getError());
                                                }
                                              });
    return;
  } else if (feedType == FeedType::UserActivityAggregated) {
    Util::logWarning("AppStore", "UserActivityAggregated requires userId - skipping");
    return;
  }

  // Get the appropriate typed observable based on feed type
  rxcpp::observable<NetworkClient::FeedResult> feedObservable;

  if (feedType == FeedType::Timeline) {
    Util::logDebug("AppStore", "performFetch: using getTimelineFeedObservable");
    feedObservable = networkClient->getTimelineFeedObservable(limit, offset);
  } else if (feedType == FeedType::Trending) {
    feedObservable = networkClient->getTrendingFeedObservable(limit, offset);
  } else if (feedType == FeedType::Global) {
    feedObservable = networkClient->getGlobalFeedObservable(limit, offset);
  } else if (feedType == FeedType::ForYou) {
    feedObservable = networkClient->getForYouFeedObservable(limit, offset);
  } else if (feedType == FeedType::Popular) {
    feedObservable = networkClient->getPopularFeedObservable(limit, offset);
  } else if (feedType == FeedType::Latest) {
    feedObservable = networkClient->getLatestFeedObservable(limit, offset);
  } else if (feedType == FeedType::Discovery) {
    feedObservable = networkClient->getDiscoveryFeedObservable(limit, offset);
  } else {
    Util::logError("AppStore", "Unknown feed type");
    return;
  }

  // Subscribe to the typed observable
  feedObservable.subscribe(
      [this, feedType, limit, offset](const NetworkClient::FeedResult &result) {
        Log::debug("AppStore: Feed response received via observable, got " +
                   juce::String(static_cast<int>(result.posts.size())) + " typed posts");
        handleTypedFetchSuccess(feedType, result, limit, offset);
      },
      [this, feedType](std::exception_ptr ep) {
        try {
          std::rethrow_exception(ep);
        } catch (const std::exception &e) {
          Log::debug("AppStore: Feed request failed: " + juce::String(e.what()));
          handleFetchError(feedType, juce::String(e.what()));
        }
      });
}

void AppStore::handleFetchSuccess(FeedType feedType, const nlohmann::json &data, int limit, int offset) {
  // Log pagination info for debugging
  Log::info("========== handleFetchSuccess ENTRY ==========");
  Log::info("handleFetchSuccess called for feedType=" + feedTypeToString(feedType));
  Log::debug("handleFetchSuccess parsing response");

  try {
    // Log pagination info for debugging
    Log::debug("handleFetchSuccess: feedType=" + feedTypeToString(feedType) + ", offset=" + juce::String(offset) +
               ", limit=" + juce::String(limit));

    // Handle aggregated feed types (trends, recommendations, etc.)
    if (isAggregatedFeedType(feedType)) {
      auto response = parseAggregatedJsonResponse(data);

      PostsState newState = stateManager.posts->getState();
      if (newState.aggregatedFeeds.count(feedType) == 0) {
        newState.aggregatedFeeds[feedType] = AggregatedFeedState();
      }

      auto &feedState = newState.aggregatedFeeds[feedType];
      if (offset == 0) {
        feedState.groups = response.groups;
      } else {
        for (const auto &group : response.groups) {
          feedState.groups.add(group);
        }
      }

      feedState.isLoading = false;
      feedState.offset = offset + static_cast<int>(response.groups.size());
      feedState.total = response.total;
      feedState.hasMore = feedState.offset < feedState.total;
      feedState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
      feedState.error = "";
      stateManager.posts->setState(newState);
    } else {

      Log::debug("========== About to call parseJsonResponse ==========");
      auto response = parseJsonResponse(data);
      Log::debug("========== parseJsonResponse COMPLETE, got " + juce::String(response.posts.size()) +
                 " posts ==========");

      // Validate response size against requested limit
      const size_t responseSize = static_cast<size_t>(response.posts.size());
      const size_t expectedLimit = static_cast<size_t>(limit);
      if (responseSize > expectedLimit) {
        Log::warn("AppStore: Response size (" + juce::String(static_cast<int>(responseSize)) +
                  ") exceeds requested limit (" + juce::String(limit) + ")");
      }

      // Cache the feed data in memory cache (5-minute TTL)
      Log::debug("========== About to cache feed data ==========");
      auto cacheKey = "feed:" + feedTypeToString(feedType).toLowerCase();
      Log::debug("========== Feed cached successfully ==========");

      Log::debug("========== About to call updateFeedState ==========");
      PostsState newState = stateManager.posts->getState();
      if (newState.feeds.count(feedType) == 0) {
        newState.feeds[feedType] = FeedState();
      }

      auto &feedState = newState.feeds[feedType];
      if (offset == 0) {
        feedState.posts.clear();
      }
      // Convert juce::Array<FeedPost> to vector<shared_ptr<FeedPost>>
      for (const auto &post : response.posts) {
        feedState.posts.push_back(std::make_shared<FeedPost>(post));
      }

      feedState.isLoading = false;
      feedState.isRefreshing = false;
      feedState.offset = offset + response.posts.size();
      feedState.total = response.total;
      feedState.hasMore = response.hasMore; // Use has_more from API response
      feedState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
      feedState.error = "";
      feedState.isSynced = true;
      stateManager.posts->setState(newState);
      Log::debug("========== updateFeedState COMPLETE ==========");
    }

    Log::info("========== handleFetchSuccess COMPLETE ==========");
    Log::debug("Loaded feed for feedType=" + feedTypeToString(feedType));
  } catch (const std::exception &e) {
    Log::error("========== EXCEPTION in handleFetchSuccess: " + juce::String(e.what()) + " ==========");
    Log::error("Exception in handleFetchSuccess: " + juce::String(e.what()));
  } catch (...) {
    Log::error("========== UNKNOWN EXCEPTION in handleFetchSuccess ==========");
    Log::error("Unknown exception in handleFetchSuccess");
  }
}

void AppStore::handleTypedFetchSuccess(FeedType feedType, const NetworkClient::FeedResult &result, int limit,
                                       int offset) {
  Log::info("========== handleTypedFetchSuccess ENTRY ==========");
  Log::info("handleTypedFetchSuccess for feedType=" + feedTypeToString(feedType) +
            ", posts=" + juce::String(static_cast<int>(result.posts.size())));

  try {
    // Validate response size against requested limit
    const size_t responseSize = result.posts.size();
    const size_t expectedLimit = static_cast<size_t>(limit);
    if (responseSize > expectedLimit) {
      Log::warn("AppStore: Response size (" + juce::String(static_cast<int>(responseSize)) +
                ") exceeds requested limit (" + juce::String(limit) + ")");
    }

    PostsState newState = stateManager.posts->getState();
    if (newState.feeds.count(feedType) == 0) {
      newState.feeds[feedType] = FeedState();
    }

    auto &feedState = newState.feeds[feedType];
    if (offset == 0) {
      feedState.posts.clear();
    }

    // Convert typed FeedPost values to shared_ptr<FeedPost>
    for (const auto &post : result.posts) {
      feedState.posts.push_back(std::make_shared<FeedPost>(post));
    }

    feedState.isLoading = false;
    feedState.isRefreshing = false;
    feedState.offset = offset + static_cast<int>(result.posts.size());
    feedState.total = result.total;
    feedState.hasMore = result.hasMore;
    feedState.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
    feedState.error = "";
    feedState.isSynced = true;
    stateManager.posts->setState(newState);

    Log::info("========== handleTypedFetchSuccess COMPLETE ==========");
    Log::debug("Loaded " + juce::String(static_cast<int>(result.posts.size())) +
               " posts for feedType=" + feedTypeToString(feedType));
  } catch (const std::exception &e) {
    Log::error("========== EXCEPTION in handleTypedFetchSuccess: " + juce::String(e.what()) + " ==========");
  } catch (...) {
    Log::error("========== UNKNOWN EXCEPTION in handleTypedFetchSuccess ==========");
  }
}

void AppStore::handleFetchError(FeedType feedType, const juce::String &error) {
  Util::logError("AppStore", "Failed to load feed: " + error);

  PostsState newState = stateManager.posts->getState();

  // Handle both regular and aggregated feed types
  if (isAggregatedFeedType(feedType)) {
    if (newState.aggregatedFeeds.count(feedType) > 0) {
      newState.aggregatedFeeds[feedType].isLoading = false;
      newState.aggregatedFeeds[feedType].error = error;
    }
  } else {
    if (newState.feeds.count(feedType) > 0) {
      newState.feeds[feedType].isLoading = false;
      newState.feeds[feedType].error = error;
    }
  }
  stateManager.posts->setState(newState);
}

void AppStore::handleSavedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    PostsState errorState = stateManager.posts->getState();
    errorState.savedPosts.isLoading = false;
    errorState.savedPosts.error = result.getError();
    stateManager.posts->setState(errorState);
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    PostsState errorState = stateManager.posts->getState();
    errorState.savedPosts.isLoading = false;
    errorState.savedPosts.error = "Invalid saved posts response";
    stateManager.posts->setState(errorState);
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    PostsState errorState = stateManager.posts->getState();
    errorState.savedPosts.isLoading = false;
    errorState.savedPosts.error = "Invalid posts array in response";
    stateManager.posts->setState(errorState);
    return;
  }

  // Use JsonArrayParser with validation
  auto loadedPosts = JsonArrayParser<FeedPost>::parseWithValidation(
      postsArray, [](const FeedPost &post) { return post.isValid(); }, "saved posts");

  PostsState successState = stateManager.posts->getState();
  successState.savedPosts.posts = std::move(loadedPosts);
  successState.savedPosts.isLoading = false;
  successState.savedPosts.totalCount = totalCount;
  successState.savedPosts.offset += static_cast<int>(successState.savedPosts.posts.size());
  successState.savedPosts.hasMore = successState.savedPosts.offset < totalCount;
  successState.savedPosts.error = "";
  successState.savedPosts.lastUpdated = Utils::StateHelpers::now();
  stateManager.posts->setState(successState);

  Util::logDebug("AppStore", "Loaded " + juce::String(successState.savedPosts.posts.size()) + " saved posts");
}

void AppStore::handleArchivedPostsLoaded(Outcome<juce::var> result) {
  if (!result.isOk()) {
    PostsState errorState = stateManager.posts->getState();
    errorState.archivedPosts.isLoading = false;
    errorState.archivedPosts.error = result.getError();
    stateManager.posts->setState(errorState);
    return;
  }

  auto data = result.getValue();
  if (!data.isObject()) {
    PostsState errorState = stateManager.posts->getState();
    errorState.archivedPosts.isLoading = false;
    errorState.archivedPosts.error = "Invalid archived posts response";
    stateManager.posts->setState(errorState);
    return;
  }

  auto postsArray = data.getProperty("posts", juce::var());
  auto totalCount = static_cast<int>(data.getProperty("total", 0));

  if (!postsArray.isArray()) {
    PostsState errorState = stateManager.posts->getState();
    errorState.archivedPosts.isLoading = false;
    errorState.archivedPosts.error = "Invalid posts array in response";
    stateManager.posts->setState(errorState);
    return;
  }

  // Use JsonArrayParser with validation
  auto loadedPosts = JsonArrayParser<FeedPost>::parseWithValidation(
      postsArray, [](const FeedPost &post) { return post.isValid(); }, "archived posts");

  PostsState successState = stateManager.posts->getState();
  successState.archivedPosts.posts = std::move(loadedPosts);
  successState.archivedPosts.isLoading = false;
  successState.archivedPosts.totalCount = totalCount;
  successState.archivedPosts.offset += static_cast<int>(successState.archivedPosts.posts.size());
  successState.archivedPosts.hasMore = successState.archivedPosts.offset < totalCount;
  successState.archivedPosts.error = "";
  successState.archivedPosts.lastUpdated = Utils::StateHelpers::now();
  stateManager.posts->setState(successState);

  Util::logDebug("AppStore", "Loaded " + juce::String(successState.archivedPosts.posts.size()) + " archived posts");
}

bool AppStore::isCurrentFeedCached() const {
  auto state = stateManager.posts->getState();
  auto feedType = state.currentFeedType;
  auto now = juce::Time::getCurrentTime().toMilliseconds();
  const int cacheTTLSeconds = 300; // 5 minutes

  if (isAggregatedFeedType(feedType)) {
    // Check aggregated feed caching with timestamp tracking
    if (state.aggregatedFeeds.count(feedType) > 0) {
      const auto &feedState = state.aggregatedFeeds.at(feedType);
      if (feedState.groups.size() == 0) {
        return false;
      }
      auto ageMs = now - feedState.lastUpdated;
      auto ageSecs = ageMs / 1000;
      return ageSecs < cacheTTLSeconds;
    }
    return false;
  } else {
    if (state.feeds.count(feedType) > 0) {
      auto &feedState = state.feeds.at(feedType);
      auto ageMs = now - feedState.lastUpdated;
      auto ageSecs = ageMs / 1000;
      return !feedState.posts.empty() && ageSecs < cacheTTLSeconds;
    }
  }

  return false;
}

FeedResponse AppStore::parseJsonResponse(const nlohmann::json &json) {
  FeedResponse response;

  if (!json.is_object()) {
    return response;
  }

  // Try "activities" first (unified feed format), then "posts" (fallback for other endpoints)
  nlohmann::json postsArray;
  if (json.contains("activities") && json["activities"].is_array()) {
    postsArray = json["activities"];
  } else if (json.contains("posts") && json["posts"].is_array()) {
    postsArray = json["posts"];
  } else {
    postsArray = nlohmann::json::array();
  }

  // Extract total from meta.count or total field
  if (json.contains("meta") && json["meta"].is_object()) {
    const auto &metaObj = json["meta"];
    if (metaObj.contains("count")) {
      response.total = metaObj.value("count", 0);
    } else {
      response.total = json.value("total", 0);
    }
    // Extract has_more flag for pagination
    response.hasMore = metaObj.value("has_more", false);
  } else {
    response.total = json.value("total", 0);
  }

  // Parse posts from JSON array
  for (const auto &postJson : postsArray) {
    auto post = FeedPost::fromJson(postJson);
    if (post.isValid()) {
      response.posts.add(post);
    }
  }

  Util::logDebug("AppStore", "Parsed " + juce::String(response.posts.size()) + " posts from feed response");
  return response;
}

AggregatedFeedResponse AppStore::parseAggregatedJsonResponse(const nlohmann::json &json) {
  AggregatedFeedResponse response;

  if (!json.is_object()) {
    return response;
  }

  response.total = json.value("total", 0);

  if (json.contains("groups") && json["groups"].is_array()) {
    for (const auto &groupJson : json["groups"]) {
      auto group = AggregatedFeedGroup::fromJson(groupJson);
      response.groups.add(group);
    }
  }

  return response;
}

// ==============================================================================
// Phase 4: Reactive Feed Service Operations with Memory Caching

// These methods implement loadFeedObservable and likePostObservable
// using RxCpp observables with automatic cache invalidation strategies.

rxcpp::observable<nlohmann::json> AppStore::loadFeedObservable(FeedType feedType) {
  return rxcpp::sources::create<nlohmann::json>([this, feedType](auto observer) {
           // Fetch from network
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           Util::logDebug("AppStore", "Loading feed from network: " + feedTypeToString(feedType));

           // Create callback to handle network response
           auto callback = [feedType, observer](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               // Cache the response (30 seconds for feeds - they update frequently)
               auto data = result.getValue();
               Util::logInfo("AppStore", "Feed loaded and cached: " + feedTypeToString(feedType));
               observer.on_next(data);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Feed load failed: " + result.getError());
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           };

           // Call the appropriate feed method based on feed type
           if (feedType == FeedType::Timeline) {
             networkClient->getTimelineFeed(20, 0, callback);
           } else if (feedType == FeedType::Trending) {
             networkClient->getTrendingFeed(20, 0, callback);
           } else if (feedType == FeedType::Global) {
             networkClient->getGlobalFeed(20, 0, callback);
           } else if (feedType == FeedType::ForYou) {
             networkClient->getForYouFeed(20, 0, callback);
           } else if (feedType == FeedType::Popular) {
             networkClient->getPopularFeed(20, 0, callback);
           } else if (feedType == FeedType::Latest) {
             networkClient->getLatestFeed(20, 0, callback);
           } else if (feedType == FeedType::Discovery) {
             networkClient->getDiscoveryFeed(20, 0, callback);
           } else if (feedType == FeedType::TimelineAggregated) {
             networkClient->getAggregatedTimeline(20, 0, callback);
           } else if (feedType == FeedType::TrendingAggregated) {
             networkClient->getTrendingFeedGrouped(20, 0, callback);
           } else if (feedType == FeedType::NotificationAggregated) {
             networkClient->getNotificationsAggregated(20, 0, callback);
           } else if (feedType == FeedType::UserActivityAggregated) {
             Util::logWarning("AppStore", "UserActivityAggregated requires userId - skipping");
             observer.on_error(std::make_exception_ptr(std::runtime_error("UserActivityAggregated requires userId")));
           } else {
             Util::logError("AppStore", "Unknown feed type");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Unknown feed type")));
           }
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::likePostObservable(const juce::String &postId) {
  return rxcpp::sources::create<int>([this, postId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           // Check current like state from app state
           auto currentPostsState = stateManager.posts->getState();
           bool isCurrentlyLiked = false;

           for (const auto &[feedType, feedState] : currentPostsState.feeds) {
             for (const auto &post : feedState.posts) {
               if (post->id == postId) {
                 isCurrentlyLiked = post->isLiked;
                 break;
               }
             }
           }

           // Store previous state for rollback on error
           auto previousState = isCurrentlyLiked;

           // Apply optimistic update
           PostsState newState = stateManager.posts->getState();
           for (auto &[feedType, feedState] : newState.feeds) {
             for (auto &post : feedState.posts) {
               if (post->id == postId) {
                 post->isLiked = !post->isLiked;
                 // BUG FIX: Prevent negative like counts
                 post->likeCount = post->isLiked ? post->likeCount + 1 : std::max(0, post->likeCount - 1);
               }
             }
           }
           stateManager.posts->setState(newState);

           Util::logDebug("AppStore", "Like post optimistic update: " + postId);

           // Send to server - like or unlike based on previous state
           if (isCurrentlyLiked) {
             networkClient->unlikePost(postId, [this, postId, previousState, observer](Outcome<nlohmann::json> result) {
               if (result.isOk()) {
                 Util::logInfo("AppStore", "Post unliked successfully: " + postId);
                 // Invalidate feed caches on successful unlike
                 observer.on_next(0);
                 observer.on_completed();
               } else {
                 Util::logError("AppStore", "Failed to unlike post: " + result.getError());
                 // Rollback optimistic update on error
                 PostsState rollbackState = stateManager.posts->getState();
                 for (auto &[feedType, feedState] : rollbackState.feeds) {
                   for (auto &post : feedState.posts) {
                     if (post->id == postId) {
                       post->isLiked = previousState;
                       // BUG FIX: Prevent negative like counts on rollback
                       post->likeCount = previousState ? post->likeCount + 1 : std::max(0, post->likeCount - 1);
                     }
                   }
                 }
                 stateManager.posts->setState(rollbackState);
                 observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
               }
             });
           } else {
             networkClient->likePost(
                 postId, "", [this, postId, previousState, observer](Outcome<nlohmann::json> result) {
                   if (result.isOk()) {
                     Util::logInfo("AppStore", "Post liked successfully: " + postId);
                     // Invalidate feed caches on successful like
                     observer.on_next(0);
                     observer.on_completed();
                   } else {
                     Util::logError("AppStore", "Failed to like post: " + result.getError());
                     // Rollback optimistic update on error
                     PostsState rollbackState = stateManager.posts->getState();
                     for (auto &[feedType, feedState] : rollbackState.feeds) {
                       for (auto &post : feedState.posts) {
                         if (post->id == postId) {
                           post->isLiked = previousState;
                           // BUG FIX: Prevent negative like counts on rollback
                           post->likeCount = previousState ? post->likeCount + 1 : std::max(0, post->likeCount - 1);
                         }
                       }
                     }
                     stateManager.posts->setState(rollbackState);
                     observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
                   }
                 });
           }
         })
      .observe_on(Rx::observe_on_juce_thread());
}

// ==============================================================================
// Additional Reactive Observable Methods

rxcpp::observable<int> AppStore::toggleSaveObservable(const juce::String &postId) {
  return rxcpp::sources::create<int>([this, postId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           // Check current save state
           auto currentPostsState = stateManager.posts->getState();
           bool isCurrentlySaved = false;

           for (const auto &[feedType, feedState] : currentPostsState.feeds) {
             for (const auto &post : feedState.posts) {
               if (post->id == postId) {
                 isCurrentlySaved = post->isSaved;
                 break;
               }
             }
           }

           // Store previous state for rollback on error
           auto previousState = isCurrentlySaved;

           // Apply optimistic update
           PostsState newState = stateManager.posts->getState();
           for (auto &[feedType, feedState] : newState.feeds) {
             for (auto &post : feedState.posts) {
               if (post->id == postId) {
                 post->isSaved = !post->isSaved;
                 // BUG FIX: Prevent negative save counts
                 post->saveCount = post->isSaved ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
               }
             }
           }

           for (auto &post : newState.savedPosts.posts) {
             if (post->id == postId) {
               post->isSaved = !post->isSaved;
               // BUG FIX: Prevent negative save counts
               post->saveCount = post->isSaved ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
             }
           }

           for (auto &post : newState.archivedPosts.posts) {
             if (post->id == postId) {
               post->isSaved = !post->isSaved;
               // BUG FIX: Prevent negative save counts
               post->saveCount = post->isSaved ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
             }
           }
           stateManager.posts->setState(newState);

           Util::logDebug("AppStore", "Toggle save optimistic update: " + postId);

           // Send to server
           if (previousState) {
             networkClient->unsavePost(postId, [this, postId, previousState, observer](Outcome<nlohmann::json> result) {
               if (result.isOk()) {
                 Util::logInfo("AppStore", "Post unsaved successfully: " + postId);
                 observer.on_next(0);
                 observer.on_completed();
               } else {
                 Util::logError("AppStore", "Failed to unsave post: " + result.getError());
                 // Rollback optimistic update on error
                 PostsState rollbackState = stateManager.posts->getState();
                 for (auto &[feedType, feedState] : rollbackState.feeds) {
                   for (auto &post : feedState.posts) {
                     if (post->id == postId) {
                       post->isSaved = previousState;
                       // BUG FIX: Prevent negative save counts on rollback
                       post->saveCount = previousState ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
                     }
                   }
                 }
                 for (auto &post : rollbackState.savedPosts.posts) {
                   if (post->id == postId) {
                     post->isSaved = previousState;
                     // BUG FIX: Prevent negative save counts on rollback
                     post->saveCount = previousState ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
                   }
                 }
                 for (auto &post : rollbackState.archivedPosts.posts) {
                   if (post->id == postId) {
                     post->isSaved = previousState;
                     // BUG FIX: Prevent negative save counts on rollback
                     post->saveCount = previousState ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
                   }
                 }
                 stateManager.posts->setState(rollbackState);
                 observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
               }
             });
           } else {
             networkClient->savePost(postId, [this, postId, previousState, observer](Outcome<nlohmann::json> result) {
               if (result.isOk()) {
                 Util::logInfo("AppStore", "Post saved successfully: " + postId);
                 observer.on_next(0);
                 observer.on_completed();
               } else {
                 Util::logError("AppStore", "Failed to save post: " + result.getError());
                 // Rollback optimistic update on error
                 PostsState rollbackState = stateManager.posts->getState();
                 for (auto &[feedType, feedState] : rollbackState.feeds) {
                   for (auto &post : feedState.posts) {
                     if (post->id == postId) {
                       post->isSaved = previousState;
                       // BUG FIX: Prevent negative save counts on rollback
                       post->saveCount = previousState ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
                     }
                   }
                 }
                 for (auto &post : rollbackState.savedPosts.posts) {
                   if (post->id == postId) {
                     post->isSaved = previousState;
                     // BUG FIX: Prevent negative save counts on rollback
                     post->saveCount = previousState ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
                   }
                 }
                 for (auto &post : rollbackState.archivedPosts.posts) {
                   if (post->id == postId) {
                     post->isSaved = previousState;
                     // BUG FIX: Prevent negative save counts on rollback
                     post->saveCount = previousState ? post->saveCount + 1 : std::max(0, post->saveCount - 1);
                   }
                 }
                 stateManager.posts->setState(rollbackState);
                 observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
               }
             });
           }
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::toggleRepostObservable(const juce::String &postId) {
  return rxcpp::sources::create<int>([this, postId](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           // Find current repost state
           auto currentState = stateManager.posts->getState();
           auto toggleState = PostInteractionHelper::findCurrentState(
               currentState, postId, [](const FeedPost &p) { return p.isReposted; },
               [](const FeedPost &p) { return p.repostCount; });

           if (!toggleState.has_value()) {
             Util::logWarning("AppStore", "Post not found for repost toggle: " + postId);
             observer.on_error(std::make_exception_ptr(std::runtime_error("Post not found")));
             return;
           }

           bool wasReposted = toggleState->isActive;
           auto newToggleState = toggleState->toggle();

           // Apply optimistic update
           PostsState newState = stateManager.posts->getState();
           PostInteractionHelper::updatePostAcrossCollections(newState, postId,
                                                              [&newToggleState](std::shared_ptr<FeedPost> &post) {
                                                                post->isReposted = newToggleState.isActive;
                                                                post->repostCount = newToggleState.count;
                                                              });
           stateManager.posts->setState(newState);

           Util::logDebug("AppStore", "Repost optimistic update: " + postId);

           // Call API and wait for response
           auto callback = [this, postId, wasReposted, toggleState, observer](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               Util::logInfo("AppStore",
                             "Repost " + juce::String(wasReposted ? "undone" : "applied") + " successfully: " + postId);
               observer.on_next(0);
               observer.on_completed();
             } else {
               Util::logError("AppStore", "Failed to toggle repost: " + result.getError());
               // Rollback optimistic update
               PostsState rollbackState = stateManager.posts->getState();
               PostInteractionHelper::updatePostAcrossCollections(rollbackState, postId,
                                                                  [&toggleState](std::shared_ptr<FeedPost> &post) {
                                                                    post->isReposted = toggleState->isActive;
                                                                    post->repostCount = toggleState->count;
                                                                  });
               stateManager.posts->setState(rollbackState);
               observer.on_error(std::make_exception_ptr(std::runtime_error(result.getError().toStdString())));
             }
           };

           if (wasReposted) {
             networkClient->undoRepost(postId, callback);
           } else {
             networkClient->repostPost(postId, "", callback);
           }
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::togglePinObservable(const juce::String &postId, bool pinned) {
  return rxcpp::sources::create<int>([this, postId, pinned](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           // Call the existing callback-based method for now - it handles optimistic updates and cache invalidation
           togglePin(postId, pinned);
           observer.on_next(0);
           observer.on_completed();
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::toggleFollowObservable(const juce::String &postId, bool willFollow) {
  return rxcpp::sources::create<int>([this, postId, willFollow](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           // Call the existing callback-based method for now - it handles optimistic updates and cache invalidation
           toggleFollow(postId, willFollow);
           observer.on_next(0);
           observer.on_completed();
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<int> AppStore::addReactionObservable(const juce::String &postId, const juce::String &emoji) {
  return rxcpp::sources::create<int>([this, postId, emoji](auto observer) {
           if (!networkClient) {
             Util::logError("AppStore", "Network client not initialized");
             observer.on_error(std::make_exception_ptr(std::runtime_error("Network client not initialized")));
             return;
           }

           // Call the existing callback-based method for now - it handles optimistic updates and cache invalidation
           addReaction(postId, emoji);
           observer.on_next(0);
           observer.on_completed();
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<nlohmann::json> AppStore::loadMultipleFeedsObservable(const std::vector<FeedType> &feedTypes) {
  if (feedTypes.empty()) {
    return rxcpp::observable<>::empty<nlohmann::json>();
  }

  // Create observables for each feed type and merge them
  std::vector<rxcpp::observable<nlohmann::json>> feedObservables;
  feedObservables.reserve(feedTypes.size());

  for (const auto &feedType : feedTypes) {
    feedObservables.push_back(loadFeedObservable(feedType));
  }

  // Merge all feed observables into one stream
  // Each feed will emit its results as they arrive
  return rxcpp::observable<>::iterate(feedObservables)
      .flat_map([](auto obs) { return obs; })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
