#pragma once

#include "../network/NetworkClient.h"
#include "../util/logging/Logger.h"
#include "Store.h"
#include <JuceHeader.h>

namespace Sidechain {
namespace Stores {

/**
 * SoundState - Immutable state for sound pages
 */
struct SoundState {
  // Sound data
  juce::var soundData; // JSON data for sound pages
  bool isLoading = false;
  bool isRefreshing = false;

  // Featured sounds
  juce::Array<juce::var> featuredSounds;
  bool isFeaturedLoading = false;

  // Recent sounds
  juce::Array<juce::var> recentSounds;
  int recentOffset = 0;
  bool hasMoreRecent = true;

  // Pagination
  int offset = 0;
  int limit = 20;
  int totalCount = 0;

  // Error handling
  juce::String error;
  int64_t lastUpdated = 0;

  bool operator==(const SoundState &other) const {
    return isLoading == other.isLoading && offset == other.offset && totalCount == other.totalCount;
  }
};

/**
 * SoundStore - Reactive store for sound pages and featured sounds
 *
 * Handles:
 * - Loading sound pages data
 * - Featured sounds display
 * - Recent sounds feed
 * - Sound pagination
 *
 * Usage:
 *   auto& soundStore = SoundStore::getInstance();
 *   soundStore.setNetworkClient(networkClient);
 *
 *   auto unsubscribe = soundStore.subscribe([](const SoundState& state) {
 *       if (state.isLoading) {
 *           showLoadingSpinner();
 *       } else {
 *           displaySounds(state.featuredSounds, state.recentSounds);
 *       }
 *   });
 *
 *   // Load sounds
 *   soundStore.loadFeaturedSounds();
 *   soundStore.loadRecentSounds();
 *
 *   // Load more
 *   soundStore.loadMoreSounds();
 */
class SoundStore : public Store<SoundState> {
public:
  /**
   * Get singleton instance
   */
  static SoundStore &getInstance() {
    static SoundStore instance;
    return instance;
  }

  /**
   * Set the network client for API calls
   */
  void setNetworkClient(NetworkClient *client) {
    networkClient = client;
  }

  //==============================================================================
  // Sound Loading Methods

  /**
   * Load featured sounds
   */
  void loadFeaturedSounds();

  /**
   * Load recent sounds
   */
  void loadRecentSounds();

  /**
   * Load more sounds (pagination)
   */
  void loadMoreSounds();

  /**
   * Refresh sound pages data
   */
  void refresh();

  /**
   * Clear all sound data
   */
  void clearData();

private:
  SoundStore() : Store<SoundState>() {}

  NetworkClient *networkClient = nullptr;
};

} // namespace Stores
} // namespace Sidechain
