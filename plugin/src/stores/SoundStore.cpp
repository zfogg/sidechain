#include "SoundStore.h"

namespace Sidechain {
namespace Stores {

void SoundStore::loadFeaturedSounds() {
  if (!networkClient) {
    updateState([](SoundState &state) { state.error = "Network client not initialized"; });
    return;
  }

  updateState([](SoundState &state) { state.isFeaturedLoading = true; });
  // TODO: Implement featured sounds loading with NetworkClient
  updateState([](SoundState &state) {
    state.isFeaturedLoading = false;
    state.error = "";
  });
}

void SoundStore::loadRecentSounds() {
  if (!networkClient) {
    updateState([](SoundState &state) { state.error = "Network client not initialized"; });
    return;
  }

  updateState([](SoundState &state) {
    state.isLoading = true;
    state.recentOffset = 0;
  });

  // TODO: Implement recent sounds loading with NetworkClient
  updateState([](SoundState &state) {
    state.isLoading = false;
    state.hasMoreRecent = false;
    state.error = "";
  });
}

void SoundStore::loadMoreSounds() {
  if (!networkClient) {
    updateState([](SoundState &state) { state.error = "Network client not initialized"; });
    return;
  }

  SoundState currentState = getState();
  if (!currentState.hasMoreRecent) {
    return;
  }

  updateState([](SoundState &state) { state.isLoading = true; });

  // TODO: Implement load more sounds with NetworkClient
  updateState([](SoundState &state) {
    state.isLoading = false;
    state.error = "";
  });
}

void SoundStore::refresh() {
  if (!networkClient) {
    updateState([](SoundState &state) { state.error = "Network client not initialized"; });
    return;
  }

  updateState([](SoundState &state) {
    state.isRefreshing = true;
    state.recentOffset = 0;
  });

  // TODO: Implement refresh with NetworkClient
  updateState([](SoundState &state) {
    state.isRefreshing = false;
    state.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
    state.error = "";
  });
}

void SoundStore::clearData() {
  updateState([](SoundState &state) {
    state.soundData = juce::var();
    state.featuredSounds.clear();
    state.recentSounds.clear();
    state.offset = 0;
    state.totalCount = 0;
    state.error = "";
  });
}

} // namespace Stores
} // namespace Sidechain
