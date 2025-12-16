#include "UserDiscoveryStore.h"
#include "../util/Json.h"
#include "../util/Log.h"

namespace Sidechain {
namespace Stores {

//==============================================================================
UserDiscoveryStore::UserDiscoveryStore(NetworkClient *client) : networkClient(client) {
  Log::info("UserDiscoveryStore: Initializing");
  setState(UserDiscoveryState{});
}

//==============================================================================
void UserDiscoveryStore::loadDiscoveryData(const juce::String &currentUserId) {
  Log::info("UserDiscoveryStore: Loading all discovery sections");
  loadTrendingUsers();
  loadFeaturedProducers();
  loadSuggestedUsers();
  loadSimilarProducers(currentUserId);
  loadRecommendedToFollow();
  loadAvailableGenres();
}

void UserDiscoveryStore::refreshDiscoveryData(const juce::String &currentUserId) {
  Log::info("UserDiscoveryStore: Refreshing discovery data");
  // Clear all users
  auto state = getState();
  state.trendingUsers.clear();
  state.featuredProducers.clear();
  state.suggestedUsers.clear();
  state.similarProducers.clear();
  state.recommendedToFollow.clear();
  setState(state);

  loadDiscoveryData(currentUserId);
}

//==============================================================================
void UserDiscoveryStore::loadTrendingUsers() {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.isTrendingLoading = true;
  setState(state);

  networkClient->getTrendingUsers(10, [this](Outcome<juce::var> result) { handleTrendingUsersLoaded(result); });
}

void UserDiscoveryStore::loadFeaturedProducers() {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.isFeaturedLoading = true;
  setState(state);

  networkClient->getFeaturedProducers(10, [this](Outcome<juce::var> result) { handleFeaturedProducersLoaded(result); });
}

void UserDiscoveryStore::loadSuggestedUsers() {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.isSuggestedLoading = true;
  setState(state);

  networkClient->getSuggestedUsers(10, [this](Outcome<juce::var> result) { handleSuggestedUsersLoaded(result); });
}

void UserDiscoveryStore::loadSimilarProducers(const juce::String &currentUserId) {
  if (networkClient == nullptr || currentUserId.isEmpty())
    return;

  auto state = getState();
  state.isSimilarLoading = true;
  setState(state);

  networkClient->getSimilarUsers(currentUserId, 10,
                                 [this](Outcome<juce::var> result) { handleSimilarProducersLoaded(result); });
}

void UserDiscoveryStore::loadRecommendedToFollow() {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.isRecommendedLoading = true;
  setState(state);

  networkClient->getRecommendedUsersToFollow(
      10, 0, [this](Outcome<juce::var> result) { handleRecommendedUsersLoaded(result); });
}

void UserDiscoveryStore::loadAvailableGenres() {
  if (networkClient == nullptr)
    return;

  auto state = getState();
  state.isGenresLoading = true;
  setState(state);

  networkClient->getAvailableGenres([this](Outcome<juce::var> result) { handleGenresLoaded(result); });
}

//==============================================================================
void UserDiscoveryStore::handleTrendingUsersLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isTrendingLoading = false;

  if (result.isError()) {
    Log::error("UserDiscoveryStore: Failed to load trending users - " + result.getError());
    state.error = "Failed to load trending users";
    setState(state);
    return;
  }

  auto response = result.getValue();
  if (Json::isObject(response)) {
    auto users = Json::getArray(response, "users");
    if (Json::isArray(users)) {
      juce::Array<DiscoveredUser> trendingUsers;
      for (int i = 0; i < users.size(); ++i) {
        trendingUsers.add(DiscoveredUser::fromJson(users[i]));
      }
      state.trendingUsers = trendingUsers;
      Log::info("UserDiscoveryStore: Loaded " + juce::String(trendingUsers.size()) + " trending users");
    }
  } else {
    Log::error("UserDiscoveryStore: Invalid trending users response");
    state.error = "Invalid response format";
  }

  setState(state);
}

void UserDiscoveryStore::handleFeaturedProducersLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isFeaturedLoading = false;

  if (result.isError()) {
    Log::error("UserDiscoveryStore: Failed to load featured producers - " + result.getError());
    state.error = "Failed to load featured producers";
    setState(state);
    return;
  }

  auto response = result.getValue();
  if (Json::isObject(response)) {
    auto users = Json::getArray(response, "users");
    if (Json::isArray(users)) {
      juce::Array<DiscoveredUser> featuredProducers;
      for (int i = 0; i < users.size(); ++i) {
        featuredProducers.add(DiscoveredUser::fromJson(users[i]));
      }
      state.featuredProducers = featuredProducers;
      Log::info("UserDiscoveryStore: Loaded " + juce::String(featuredProducers.size()) + " featured producers");
    }
  } else {
    Log::error("UserDiscoveryStore: Invalid featured producers response");
    state.error = "Invalid response format";
  }

  setState(state);
}

void UserDiscoveryStore::handleSuggestedUsersLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isSuggestedLoading = false;

  if (result.isError()) {
    Log::error("UserDiscoveryStore: Failed to load suggested users - " + result.getError());
    state.error = "Failed to load suggested users";
    setState(state);
    return;
  }

  auto response = result.getValue();
  if (Json::isObject(response)) {
    auto users = Json::getArray(response, "users");
    if (Json::isArray(users)) {
      juce::Array<DiscoveredUser> suggestedUsers;
      for (int i = 0; i < users.size(); ++i) {
        suggestedUsers.add(DiscoveredUser::fromJson(users[i]));
      }
      state.suggestedUsers = suggestedUsers;
      Log::info("UserDiscoveryStore: Loaded " + juce::String(suggestedUsers.size()) + " suggested users");
    }
  } else {
    Log::error("UserDiscoveryStore: Invalid suggested users response");
    state.error = "Invalid response format";
  }

  setState(state);
}

void UserDiscoveryStore::handleSimilarProducersLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isSimilarLoading = false;

  if (result.isError()) {
    Log::error("UserDiscoveryStore: Failed to load similar producers - " + result.getError());
    state.error = "Failed to load similar producers";
    setState(state);
    return;
  }

  auto response = result.getValue();
  if (Json::isObject(response)) {
    auto users = Json::getArray(response, "users");
    if (Json::isArray(users)) {
      juce::Array<DiscoveredUser> similarProducers;
      for (int i = 0; i < users.size(); ++i) {
        similarProducers.add(DiscoveredUser::fromJson(users[i]));
      }
      state.similarProducers = similarProducers;
      Log::info("UserDiscoveryStore: Loaded " + juce::String(similarProducers.size()) + " similar producers");
    }
  } else {
    Log::error("UserDiscoveryStore: Invalid similar producers response");
    state.error = "Invalid response format";
  }

  setState(state);
}

void UserDiscoveryStore::handleRecommendedUsersLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isRecommendedLoading = false;

  if (result.isError()) {
    Log::error("UserDiscoveryStore: Failed to load recommended users - " + result.getError());
    state.error = "Failed to load recommended users";
    setState(state);
    return;
  }

  auto response = result.getValue();
  if (Json::isObject(response)) {
    auto users = Json::getArray(response, "users");
    if (Json::isArray(users)) {
      juce::Array<DiscoveredUser> recommendedUsers;
      for (int i = 0; i < users.size(); ++i) {
        recommendedUsers.add(DiscoveredUser::fromJson(users[i]));
      }
      state.recommendedToFollow = recommendedUsers;
      Log::info("UserDiscoveryStore: Loaded " + juce::String(recommendedUsers.size()) + " recommended users");
    }
  } else {
    Log::error("UserDiscoveryStore: Invalid recommended users response");
    state.error = "Invalid response format";
  }

  setState(state);
}

void UserDiscoveryStore::handleGenresLoaded(Outcome<juce::var> result) {
  auto state = getState();
  state.isGenresLoading = false;

  if (result.isError()) {
    Log::error("UserDiscoveryStore: Failed to load genres - " + result.getError());
    state.error = "Failed to load genres";
    setState(state);
    return;
  }

  auto response = result.getValue();
  if (Json::isObject(response)) {
    auto genres = Json::getArray(response, "genres");
    if (Json::isArray(genres)) {
      juce::StringArray genreList;
      for (int i = 0; i < Json::arraySize(genres); ++i) {
        genreList.add(Json::getStringAt(genres, i));
      }
      state.availableGenres = genreList;
      Log::info("UserDiscoveryStore: Loaded " + juce::String(genreList.size()) + " genres");
    }
  } else {
    Log::error("UserDiscoveryStore: Invalid genres response");
    state.error = "Invalid response format";
  }

  setState(state);
}

} // namespace Stores
} // namespace Sidechain
