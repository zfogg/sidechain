#include "../AppStore.h"
#include "../../util/logging/Logger.h"

namespace Sidechain {
namespace Stores {

void AppStore::loadFeaturedSounds() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load featured sounds - network client not set");
    return;
  }

  updateState([](AppState &state) { state.sounds.isFeaturedLoading = true; });

  networkClient->getTrendingSounds(20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> soundsList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          soundsList.add(data[i]);
        }
      }

      updateState([soundsList](AppState &state) {
        state.sounds.featuredSounds = soundsList;
        state.sounds.isFeaturedLoading = false;
        state.sounds.soundError = "";
        Util::logInfo("AppStore", "Loaded " + juce::String(soundsList.size()) + " featured sounds");
      });
    } else {
      updateState([result](AppState &state) {
        state.sounds.isFeaturedLoading = false;
        state.sounds.soundError = result.getError();
        Util::logError("AppStore", "Failed to load featured sounds: " + result.getError());
      });
    }

    notifyObservers();
  });
}

void AppStore::loadRecentSounds() {
  if (!networkClient) {
    Util::logError("AppStore", "Cannot load recent sounds - network client not set");
    return;
  }

  updateState([](AppState &state) { state.sounds.isLoading = true; });

  // Use searchSounds with empty query to get recent sounds
  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> soundsList;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          soundsList.add(data[i]);
        }
      }

      updateState([soundsList](AppState &state) {
        state.sounds.recentSounds = soundsList;
        state.sounds.isLoading = false;
        state.sounds.soundError = "";
        state.sounds.recentOffset = soundsList.size();
        Util::logInfo("AppStore", "Loaded " + juce::String(soundsList.size()) + " recent sounds");
      });
    } else {
      updateState([result](AppState &state) {
        state.sounds.isLoading = false;
        state.sounds.soundError = result.getError();
      });
    }

    notifyObservers();
  });
}

void AppStore::loadMoreSounds() {
  if (!networkClient) {
    return;
  }

  const auto &currentState = getState();
  if (currentState.sounds.recentSounds.isEmpty()) {
    return;
  }

  networkClient->searchSounds("", 20, [this](Outcome<juce::var> result) {
    if (result.isOk()) {
      const auto data = result.getValue();
      juce::Array<juce::var> newSounds;

      if (data.isArray()) {
        for (int i = 0; i < data.size(); ++i) {
          newSounds.add(data[i]);
        }
      }

      updateState([newSounds](AppState &state) {
        for (const auto &sound : newSounds) {
          state.sounds.recentSounds.add(sound);
        }
        state.sounds.recentOffset += newSounds.size();
      });
    }

    notifyObservers();
  });
}

void AppStore::refreshSounds() {
  Util::logInfo("AppStore", "Refreshing sounds");

  updateState([](AppState &state) {
    state.sounds.isRefreshing = true;
    state.sounds.lastUpdated = juce::Time::getCurrentTime().toMilliseconds();
  });

  // Load both featured and recent sounds
  loadFeaturedSounds();
  loadRecentSounds();
}

} // namespace Stores
} // namespace Sidechain
