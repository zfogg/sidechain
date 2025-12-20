#include "../AppStore.h"
#include "../EntityStore.h"
#include "../../util/Log.h"
#include "../../util/Json.h"

using namespace Sidechain;

// ==============================================================================
// Discovery Redux Actions
// ==============================================================================

void AppStore::loadTrendingUsersAndCache(int limit) {
  if (!networkClient) {
    Log::warn("AppStore", "Cannot load trending users - network client not configured");
    return;
  }

  Log::info("AppStore", "Loading trending users");

  // Redux: Create new DiscoveryState with loading indicator
  auto discoverySlice = sliceManager.getDiscoverySlice();
  Stores::DiscoveryState newState = discoverySlice->getState();
  newState.isTrendingLoading = true;
  newState.discoveryError = "";
  discoverySlice->setState(newState);

  // Network request to fetch trending users
  networkClient->getTrendingUsers(limit, [this](Outcome<juce::var> result) {
    auto discoverySlice = sliceManager.getDiscoverySlice();

    if (result.isError()) {
      // Redux: Create new state with error
      Stores::DiscoveryState errorState = discoverySlice->getState();
      errorState.isTrendingLoading = false;
      errorState.discoveryError = result.getError();
      discoverySlice->setState(errorState);
      return;
    }

    try {
      auto jsonArray = result.getValue().getArray();
      if (!jsonArray) {
        throw std::runtime_error("Response is not an array");
      }

      // Step 1: Normalize and cache users in EntityStore
      std::vector<std::shared_ptr<User>> mutableUsers;
      for (int i = 0; i < jsonArray->size(); ++i) {
        try {
          auto jsonStr = (*jsonArray)[i].toString().toStdString();
          auto json = nlohmann::json::parse(jsonStr);

          auto user = EntityStore::getInstance().normalizeUser(json);
          if (user) {
            mutableUsers.push_back(user);
          }
        } catch (const std::exception &e) {
          Log::warn("AppStore", "Failed to parse trending user JSON: " + juce::String(e.what()));
        }
      }

      // Step 2: Create new immutable state with trending users
      Stores::DiscoveryState newState = discoverySlice->getState();
      newState.isTrendingLoading = false;
      newState.discoveryError = "";
      newState.lastTrendingUpdate = juce::Time::getCurrentTime().toMilliseconds();

      // Convert mutable users to immutable view
      newState.trendingUsers.clear();
      for (const auto &user : mutableUsers) {
        newState.trendingUsers.push_back(std::const_pointer_cast<const User>(user));
      }

      // Redux: Atomically replace state - all subscribers notified
      discoverySlice->setState(newState);

      Log::info("AppStore", "Loaded " + juce::String(mutableUsers.size()) + " trending users");

    } catch (const std::exception &e) {
      // Redux: Create new error state
      Stores::DiscoveryState errorState = discoverySlice->getState();
      errorState.isTrendingLoading = false;
      errorState.discoveryError = juce::String(e.what());
      discoverySlice->setState(errorState);
      Log::error("AppStore", "Failed to load trending users: " + juce::String(e.what()));
    }
  });
}

void AppStore::loadFeaturedProducersAndCache(int limit) {
  if (!networkClient) {
    Log::warn("AppStore", "Cannot load featured producers - network client not configured");
    return;
  }

  Log::info("AppStore", "Loading featured producers");

  // Redux: Create new DiscoveryState with loading indicator
  auto discoverySlice = sliceManager.getDiscoverySlice();
  Stores::DiscoveryState newState = discoverySlice->getState();
  newState.isFeaturedLoading = true;
  newState.discoveryError = "";
  discoverySlice->setState(newState);

  // Network request to fetch featured producers
  networkClient->getFeaturedProducers(limit, [this](Outcome<juce::var> result) {
    auto discoverySlice = sliceManager.getDiscoverySlice();

    if (result.isError()) {
      // Redux: Create new state with error
      Stores::DiscoveryState errorState = discoverySlice->getState();
      errorState.isFeaturedLoading = false;
      errorState.discoveryError = result.getError();
      discoverySlice->setState(errorState);
      return;
    }

    try {
      auto jsonArray = result.getValue().getArray();
      if (!jsonArray) {
        throw std::runtime_error("Response is not an array");
      }

      // Step 1: Normalize and cache users in EntityStore
      std::vector<std::shared_ptr<User>> mutableUsers;
      for (int i = 0; i < jsonArray->size(); ++i) {
        try {
          auto jsonStr = (*jsonArray)[i].toString().toStdString();
          auto json = nlohmann::json::parse(jsonStr);

          auto user = EntityStore::getInstance().normalizeUser(json);
          if (user) {
            mutableUsers.push_back(user);
          }
        } catch (const std::exception &e) {
          Log::warn("AppStore", "Failed to parse featured producer JSON: " + juce::String(e.what()));
        }
      }

      // Step 2: Create new immutable state with featured producers
      Stores::DiscoveryState newState = discoverySlice->getState();
      newState.isFeaturedLoading = false;
      newState.discoveryError = "";
      newState.lastFeaturedUpdate = juce::Time::getCurrentTime().toMilliseconds();

      // Convert mutable users to immutable view
      newState.featuredProducers.clear();
      for (const auto &user : mutableUsers) {
        newState.featuredProducers.push_back(std::const_pointer_cast<const User>(user));
      }

      // Redux: Atomically replace state - all subscribers notified
      discoverySlice->setState(newState);

      Log::info("AppStore", "Loaded " + juce::String(mutableUsers.size()) + " featured producers");

    } catch (const std::exception &e) {
      // Redux: Create new error state
      Stores::DiscoveryState errorState = discoverySlice->getState();
      errorState.isFeaturedLoading = false;
      errorState.discoveryError = juce::String(e.what());
      discoverySlice->setState(errorState);
      Log::error("AppStore", "Failed to load featured producers: " + juce::String(e.what()));
    }
  });
}

void AppStore::loadSuggestedUsersAndCache(int limit) {
  if (!networkClient) {
    Log::warn("AppStore", "Cannot load suggested users - network client not configured");
    return;
  }

  Log::info("AppStore", "Loading suggested users");

  // Redux: Create new DiscoveryState with loading indicator
  auto discoverySlice = sliceManager.getDiscoverySlice();
  Stores::DiscoveryState newState = discoverySlice->getState();
  newState.isSuggestedLoading = true;
  newState.discoveryError = "";
  discoverySlice->setState(newState);

  // Network request to fetch suggested users
  networkClient->getSuggestedUsers(limit, [this](Outcome<juce::var> result) {
    auto discoverySlice = sliceManager.getDiscoverySlice();

    if (result.isError()) {
      // Redux: Create new state with error
      Stores::DiscoveryState errorState = discoverySlice->getState();
      errorState.isSuggestedLoading = false;
      errorState.discoveryError = result.getError();
      discoverySlice->setState(errorState);
      return;
    }

    try {
      auto jsonArray = result.getValue().getArray();
      if (!jsonArray) {
        throw std::runtime_error("Response is not an array");
      }

      // Step 1: Normalize and cache users in EntityStore
      std::vector<std::shared_ptr<User>> mutableUsers;
      for (int i = 0; i < jsonArray->size(); ++i) {
        try {
          auto jsonStr = (*jsonArray)[i].toString().toStdString();
          auto json = nlohmann::json::parse(jsonStr);

          auto user = EntityStore::getInstance().normalizeUser(json);
          if (user) {
            mutableUsers.push_back(user);
          }
        } catch (const std::exception &e) {
          Log::warn("AppStore", "Failed to parse suggested user JSON: " + juce::String(e.what()));
        }
      }

      // Step 2: Create new immutable state with suggested users
      Stores::DiscoveryState newState = discoverySlice->getState();
      newState.isSuggestedLoading = false;
      newState.discoveryError = "";
      newState.lastSuggestedUpdate = juce::Time::getCurrentTime().toMilliseconds();

      // Convert mutable users to immutable view
      newState.suggestedUsers.clear();
      for (const auto &user : mutableUsers) {
        newState.suggestedUsers.push_back(std::const_pointer_cast<const User>(user));
      }

      // Redux: Atomically replace state - all subscribers notified
      discoverySlice->setState(newState);

      Log::info("AppStore", "Loaded " + juce::String(mutableUsers.size()) + " suggested users");

    } catch (const std::exception &e) {
      // Redux: Create new error state
      Stores::DiscoveryState errorState = discoverySlice->getState();
      errorState.isSuggestedLoading = false;
      errorState.discoveryError = juce::String(e.what());
      discoverySlice->setState(errorState);
      Log::error("AppStore", "Failed to load suggested users: " + juce::String(e.what()));
    }
  });
}
