#pragma once

#include "../ui/social/UserCard.h"
#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * UserDiscoveryState - Immutable state for user discovery sections
 */
struct UserDiscoveryState {
  juce::Array<DiscoveredUser> trendingUsers;
  juce::Array<DiscoveredUser> featuredProducers;
  juce::Array<DiscoveredUser> suggestedUsers;
  juce::Array<DiscoveredUser> similarProducers;
  juce::Array<DiscoveredUser> recommendedToFollow;
  juce::StringArray availableGenres;

  bool isTrendingLoading = false;
  bool isFeaturedLoading = false;
  bool isSuggestedLoading = false;
  bool isSimilarLoading = false;
  bool isRecommendedLoading = false;
  bool isGenresLoading = false;

  juce::String error;
  int64_t lastUpdated = 0;
};

/**
 * UserDiscoveryStore - Reactive store for managing user discovery sections
 *
 * Features:
 * - Load trending, featured, suggested, similar, and recommended users
 * - Load available genres for filtering
 * - Error handling and recovery
 *
 * Note: Search and genre filtering are kept as component-local state
 * since they're more transient UI states.
 *
 * Usage:
 * ```cpp
 * auto discoveryStore = std::make_shared<UserDiscoveryStore>(networkClient);
 * discoveryStore->subscribe([this](const UserDiscoveryState& state) {
 *   // Update UI with discovery data
 * });
 * discoveryStore->loadDiscoveryData("userId");
 * ```
 */
class UserDiscoveryStore : public Store<UserDiscoveryState> {
public:
  explicit UserDiscoveryStore(NetworkClient *client);
  ~UserDiscoveryStore() override = default;

  //==============================================================================
  // Data Loading
  void loadDiscoveryData(const juce::String &currentUserId);
  void refreshDiscoveryData(const juce::String &currentUserId);

  //==============================================================================
  // Discovery Sections
  void loadTrendingUsers();
  void loadFeaturedProducers();
  void loadSuggestedUsers();
  void loadSimilarProducers(const juce::String &currentUserId);
  void loadRecommendedToFollow();
  void loadAvailableGenres();

  //==============================================================================
  // Current State Access
  bool isLoading() const {
    const auto &state = getState();
    return state.isTrendingLoading || state.isFeaturedLoading || state.isSuggestedLoading || state.isSimilarLoading ||
           state.isRecommendedLoading || state.isGenresLoading;
  }

  juce::Array<DiscoveredUser> getTrendingUsers() const {
    return getState().trendingUsers;
  }

  juce::Array<DiscoveredUser> getFeaturedProducers() const {
    return getState().featuredProducers;
  }

  juce::Array<DiscoveredUser> getSuggestedUsers() const {
    return getState().suggestedUsers;
  }

  juce::Array<DiscoveredUser> getSimilarProducers() const {
    return getState().similarProducers;
  }

  juce::Array<DiscoveredUser> getRecommendedToFollow() const {
    return getState().recommendedToFollow;
  }

  juce::StringArray getAvailableGenres() const {
    return getState().availableGenres;
  }

protected:
  //==============================================================================
  // Helper methods
  void updateTrendingUsers(const juce::Array<DiscoveredUser> &users);
  void updateFeaturedProducers(const juce::Array<DiscoveredUser> &users);
  void updateSuggestedUsers(const juce::Array<DiscoveredUser> &users);
  void updateSimilarProducers(const juce::Array<DiscoveredUser> &users);
  void updateRecommendedToFollow(const juce::Array<DiscoveredUser> &users);
  void updateAvailableGenres(const juce::StringArray &genres);

private:
  NetworkClient *networkClient;

  //==============================================================================
  // Network callbacks
  void handleTrendingUsersLoaded(Outcome<juce::var> result);
  void handleFeaturedProducersLoaded(Outcome<juce::var> result);
  void handleSuggestedUsersLoaded(Outcome<juce::var> result);
  void handleSimilarProducersLoaded(Outcome<juce::var> result);
  void handleRecommendedUsersLoaded(Outcome<juce::var> result);
  void handleGenresLoaded(Outcome<juce::var> result);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UserDiscoveryStore)
};

} // namespace Stores
} // namespace Sidechain
