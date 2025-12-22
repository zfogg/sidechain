#include "../AppStore.h"
#include "../EntityStore.h"
#include "../util/UserLoadingHelper.h"
#include "../util/StoreUtils.h"
#include "../../util/Log.h"
#include "../../util/Json.h"

using namespace Sidechain;
using Stores::Utils::UserLoadingHelper;
using Stores::Utils::NetworkClientGuard;

// ==============================================================================
// Discovery Redux Actions
// ==============================================================================

void AppStore::loadTrendingUsersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient.get(), "load trending users")) {
    return;
  }

  Log::info("AppStore", "Loading trending users");

  UserLoadingHelper<Stores::DiscoveryState>::loadUsers(
      sliceManager.discovery,
      // Set loading state
      [](Stores::DiscoveryState &s) {
        s.isTrendingLoading = true;
        s.discoveryError = "";
      },
      // Network call
      [this, limit](auto callback) { networkClient->getTrendingUsers(limit, callback); },
      // On success
      [](Stores::DiscoveryState &s, auto users) {
        s.trendingUsers = std::move(users);
        s.isTrendingLoading = false;
        s.discoveryError = "";
        s.lastTrendingUpdate = juce::Time::getCurrentTime().toMilliseconds();
      },
      // On error
      [](Stores::DiscoveryState &s, const juce::String &err) {
        s.isTrendingLoading = false;
        s.discoveryError = err;
      },
      "trending users");
}

void AppStore::loadFeaturedProducersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient.get(), "load featured producers")) {
    return;
  }

  Log::info("AppStore", "Loading featured producers");

  UserLoadingHelper<Stores::DiscoveryState>::loadUsers(
      sliceManager.discovery,
      // Set loading state
      [](Stores::DiscoveryState &s) {
        s.isFeaturedLoading = true;
        s.discoveryError = "";
      },
      // Network call
      [this, limit](auto callback) { networkClient->getFeaturedProducers(limit, callback); },
      // On success
      [](Stores::DiscoveryState &s, auto users) {
        s.featuredProducers = std::move(users);
        s.isFeaturedLoading = false;
        s.discoveryError = "";
        s.lastFeaturedUpdate = juce::Time::getCurrentTime().toMilliseconds();
      },
      // On error
      [](Stores::DiscoveryState &s, const juce::String &err) {
        s.isFeaturedLoading = false;
        s.discoveryError = err;
      },
      "featured producers");
}

void AppStore::loadSuggestedUsersAndCache(int limit) {
  if (!NetworkClientGuard::check(networkClient.get(), "load suggested users")) {
    return;
  }

  Log::info("AppStore", "Loading suggested users");

  UserLoadingHelper<Stores::DiscoveryState>::loadUsers(
      sliceManager.discovery,
      // Set loading state
      [](Stores::DiscoveryState &s) {
        s.isSuggestedLoading = true;
        s.discoveryError = "";
      },
      // Network call
      [this, limit](auto callback) { networkClient->getSuggestedUsers(limit, callback); },
      // On success
      [](Stores::DiscoveryState &s, auto users) {
        s.suggestedUsers = std::move(users);
        s.isSuggestedLoading = false;
        s.discoveryError = "";
        s.lastSuggestedUpdate = juce::Time::getCurrentTime().toMilliseconds();
      },
      // On error
      [](Stores::DiscoveryState &s, const juce::String &err) {
        s.isSuggestedLoading = false;
        s.discoveryError = err;
      },
      "suggested users");
}
