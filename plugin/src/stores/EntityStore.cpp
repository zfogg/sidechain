#include "EntityStore.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

// ==============================================================================
// Fetch Operations with Cache-First Strategy
//
// These methods implement the pattern:
// 1. Check if entity is in cache and not expired
// 2. If valid cache hit, return immediately
// 3. Otherwise fetch from network (when API methods are available)
// 4. Cache the result for future requests
// 5. Return the fetched entity
//
// Note: Some network fetch methods are stubs pending API implementation

rxcpp::observable<FeedPost> EntityStore::fetchPost(const juce::String &id) {
  return rxcpp::sources::create<FeedPost>([this, id](auto observer) {
           // Check cache first
           if (auto cached = posts_.get(id)) {
             Util::logDebug("EntityStore", "Cache hit for post: " + id);
             observer.on_next(*cached);
             observer.on_completed();
             return;
           }

           // Cache miss - network fetch not yet implemented for single posts
           // The feed loading already caches posts, so this is mainly for direct access
           Util::logWarning("EntityStore", "Post not in cache and single-post fetch not implemented: " + id);
           observer.on_error(
               std::make_exception_ptr(std::runtime_error("Post not found in cache: " + id.toStdString())));
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<Story> EntityStore::fetchStory(const juce::String &id) {
  return rxcpp::sources::create<Story>([this, id](auto observer) {
           // Check cache first
           if (auto cached = stories_.get(id)) {
             Util::logDebug("EntityStore", "Cache hit for story: " + id);
             observer.on_next(*cached);
             observer.on_completed();
             return;
           }

           // Cache miss - network fetch not yet implemented for single stories
           Util::logWarning("EntityStore", "Story not in cache and single-story fetch not implemented: " + id);
           observer.on_error(
               std::make_exception_ptr(std::runtime_error("Story not found in cache: " + id.toStdString())));
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<User> EntityStore::fetchUser(const juce::String &id) {
  return rxcpp::sources::create<User>([this, id](auto observer) {
           // Check cache first
           if (auto cached = users_.get(id)) {
             Util::logDebug("EntityStore", "Cache hit for user: " + id);
             observer.on_next(*cached);
             observer.on_completed();
             return;
           }

           // Cache miss - fetch from network
           if (!networkClient_) {
             observer.on_error(std::make_exception_ptr(std::runtime_error("NetworkClient not set")));
             return;
           }

           Util::logDebug("EntityStore", "Cache miss for user: " + id + ", fetching from network");

           networkClient_->getUser(id, [this, observer, id](Outcome<nlohmann::json> result) {
             if (result.isOk()) {
               try {
                 auto userPtr = normalizeUser(result.getValue());
                 if (userPtr) {
                   observer.on_next(*userPtr);
                   observer.on_completed();
                 } else {
                   observer.on_error(
                       std::make_exception_ptr(std::runtime_error("Failed to parse user: " + id.toStdString())));
                 }
               } catch (const std::exception &e) {
                 observer.on_error(std::make_exception_ptr(
                     std::runtime_error("Failed to parse user JSON: " + std::string(e.what()))));
               }
             } else {
               Util::logError("EntityStore", "Failed to fetch user: " + result.getError());
               observer.on_error(std::make_exception_ptr(
                   std::runtime_error("Failed to fetch user: " + result.getError().toStdString())));
             }
           });
         })
      .observe_on(Rx::observe_on_juce_thread());
}

rxcpp::observable<std::vector<FeedPost>> EntityStore::fetchPosts(const juce::Array<juce::String> &ids) {
  return rxcpp::sources::create<std::vector<FeedPost>>([this, ids](auto observer) {
           if (ids.isEmpty()) {
             observer.on_next(std::vector<FeedPost>{});
             observer.on_completed();
             return;
           }

           // Collect results from cache
           std::vector<FeedPost> results;
           std::vector<juce::String> missingIds;
           results.reserve(static_cast<size_t>(ids.size()));

           // Check cache for each ID
           for (const auto &id : ids) {
             if (auto cached = posts_.get(id)) {
               results.push_back(*cached);
             } else {
               missingIds.push_back(id);
             }
           }

           // Log cache statistics
           if (!missingIds.empty()) {
             Util::logDebug("EntityStore", "fetchPosts: " + juce::String(results.size()) + " cached, " +
                                               juce::String(missingIds.size()) +
                                               " missing (single-post fetch not implemented)");
           } else {
             Util::logDebug("EntityStore", "fetchPosts: All " + juce::String(ids.size()) + " posts found in cache");
           }

           // Return what we have from cache
           // Network fetch for individual posts not yet implemented
           observer.on_next(std::move(results));
           observer.on_completed();
         })
      .observe_on(Rx::observe_on_juce_thread());
}

} // namespace Stores
} // namespace Sidechain
